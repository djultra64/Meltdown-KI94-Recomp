#include "ki/packed_renderer.h"

#include "ki/bgr555.h"

#include <limits.h>
#include <string.h>

enum {
    FRAME_HEADER_SIZE = 8,
    FIXED_MASK = KI_RENDERER_FIXED_ONE - 1
};

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

/*
 * Advance one horizontal segment through the original 12-bit accumulator.
 * Transparent gaps and colored runs share this state. That detail is why a
 * conventional nearest-neighbor resize differs at some packet boundaries.
 */
static uint32_t scale_segment(uint32_t logical_length,
                              uint32_t scale,
                              uint32_t *remainder)
{
    const uint64_t accumulated =
        (uint64_t)*remainder + ((uint64_t)logical_length * scale);
    *remainder = (uint32_t)(accumulated & FIXED_MASK);
    return (uint32_t)(accumulated >> 12);
}

static void advance_cursor(int *x, int direction, uint32_t distance)
{
    if (distance > (uint32_t)INT_MAX) {
        *x = direction > 0 ? INT_MAX : INT_MIN;
        return;
    }

    const int signed_distance = (int)distance * direction;
    if ((signed_distance > 0 && *x > INT_MAX - signed_distance) ||
        (signed_distance < 0 && *x < INT_MIN - signed_distance)) {
        *x = direction > 0 ? INT_MAX : INT_MIN;
        return;
    }
    *x += signed_distance;
}

static int draw_blended_pixel(KiBgr555Surface *destination,
                              int x,
                              int y,
                              uint16_t blend)
{
    if (x < 0 || y < 0 || x >= destination->width ||
        y >= destination->height) {
        return 0;
    }

    uint16_t *pixel =
        destination->pixels + ((size_t)y * destination->stride_pixels) + x;
    *pixel = ki_bgr555_saturating_add(*pixel, blend);
    return 1;
}

static KiPackedRenderResult render_row(const uint8_t *row,
                                       size_t row_size,
                                       uint16_t logical_width,
                                       const uint16_t palette[32],
                                       KiBgr555Surface *destination,
                                       int destination_x,
                                       int destination_y,
                                       int x_direction,
                                       uint32_t x_scale,
                                       uint32_t initial_remainder,
                                       KiPackedRenderStats *stats)
{
    if (row_size == 1) {
        return KI_PACKED_RENDER_OK;
    }
    if (row_size < 3) {
        return KI_PACKED_RENDER_INVALID_FRAME;
    }

    size_t cursor = 1;
    uint32_t logical_x = row[cursor++];
    if (logical_x > logical_width) {
        return KI_PACKED_RENDER_INVALID_FRAME;
    }

    uint32_t remainder = initial_remainder & FIXED_MASK;
    int output_x = destination_x;
    advance_cursor(&output_x, x_direction,
                   scale_segment(logical_x, x_scale, &remainder));

    while (cursor < row_size) {
        const uint8_t packet = row[cursor++];

        if (packet == 0) {
            if (cursor >= row_size) {
                return KI_PACKED_RENDER_INVALID_FRAME;
            }

            const uint32_t skip = row[cursor++];
            if (skip > (uint32_t)logical_width - logical_x) {
                return KI_PACKED_RENDER_INVALID_FRAME;
            }
            logical_x += skip;
            advance_cursor(&output_x, x_direction,
                           scale_segment(skip, x_scale, &remainder));
            continue;
        }

        const uint32_t run_length = (packet & 0x07u) + 1u;
        if (run_length > (uint32_t)logical_width - logical_x) {
            return KI_PACKED_RENDER_INVALID_FRAME;
        }
        logical_x += run_length;

        const uint32_t output_count =
            scale_segment(run_length, x_scale, &remainder);
        const uint16_t blend = palette[packet >> 3];
        for (uint32_t pixel = 0; pixel < output_count; pixel++) {
            if (draw_blended_pixel(destination, output_x, destination_y,
                                   blend)) {
                stats->blended_pixels++;
            } else {
                stats->clipped_pixels++;
            }
            advance_cursor(&output_x, x_direction, 1);
        }
    }

    return KI_PACKED_RENDER_OK;
}

KiPackedRenderResult ki_render_packed_frame_bgr555(
    const uint8_t *frame_data,
    size_t frame_data_size,
    const uint16_t palette[32],
    KiBgr555Surface *destination,
    const KiPackedRenderTransform *transform,
    KiPackedRenderStats *stats)
{
    if (frame_data == NULL || palette == NULL || destination == NULL ||
        destination->pixels == NULL || transform == NULL || stats == NULL ||
        destination->width == 0 || destination->height == 0 ||
        destination->stride_pixels < destination->width ||
        (transform->x_direction != -1 && transform->x_direction != 1) ||
        (transform->y_direction != -1 && transform->y_direction != 1) ||
        transform->x_scale == 0 || transform->y_scale == 0) {
        return KI_PACKED_RENDER_INVALID_ARGUMENT;
    }
    if (frame_data_size < FRAME_HEADER_SIZE) {
        return KI_PACKED_RENDER_TRUNCATED;
    }

    const uint16_t width = read_u16_le(frame_data + 4);
    const uint16_t height = read_u16_le(frame_data + 6);
    if (width == 0 || height == 0) {
        return KI_PACKED_RENDER_INVALID_FRAME;
    }

    memset(stats, 0, sizeof(*stats));
    uint32_t output_rows = transform->output_rows;
    if (output_rows == 0) {
        output_rows =
            (uint32_t)(((uint64_t)height * transform->y_scale) >> 12);
    }
    if (output_rows == 0) {
        return KI_PACKED_RENDER_OK;
    }

    size_t row_offset = FRAME_HEADER_SIZE;
    uint16_t source_y = 0;
    const size_t first_row_offset =
        transform->source_offset == 0 ? FRAME_HEADER_SIZE
                                      : transform->source_offset;
    if (first_row_offset < FRAME_HEADER_SIZE) {
        return KI_PACKED_RENDER_INVALID_FRAME;
    }

    /*
     * At 0x88011840-0x8801185c the original vertical scaler can discard
     * complete packed rows before producing a destination row. Locate the
     * first row captured at 0x88011874 without decoding its pixel packets.
     */
    while (row_offset < first_row_offset) {
        if (source_y >= height || row_offset >= frame_data_size) {
            return KI_PACKED_RENDER_TRUNCATED;
        }
        const size_t skipped_size = frame_data[row_offset];
        if (skipped_size == 0) {
            return KI_PACKED_RENDER_INVALID_FRAME;
        }
        if (skipped_size > frame_data_size - row_offset) {
            return KI_PACKED_RENDER_TRUNCATED;
        }
        row_offset += skipped_size;
        source_y++;
    }
    if (row_offset != first_row_offset) {
        return KI_PACKED_RENDER_INVALID_FRAME;
    }

    uint32_t vertical_accumulator = transform->y_accumulator;
    int output_y = transform->destination_y;
    uint16_t last_drawn_source_y = UINT16_MAX;

    for (uint32_t output_row = 0; output_row < output_rows; output_row++) {
        if (source_y >= height || row_offset >= frame_data_size) {
            return KI_PACKED_RENDER_TRUNCATED;
        }

        size_t row_size = frame_data[row_offset];
        if (row_size == 0) {
            return KI_PACKED_RENDER_INVALID_FRAME;
        }
        if (row_size > frame_data_size - row_offset) {
            return KI_PACKED_RENDER_TRUNCATED;
        }

        const KiPackedRenderResult row_result = render_row(
            frame_data + row_offset, row_size, width, palette, destination,
            transform->destination_x, output_y, transform->x_direction,
            transform->x_scale, transform->x_remainder, stats);
        if (row_result != KI_PACKED_RENDER_OK) {
            return row_result;
        }

        stats->output_rows++;
        if (source_y != last_drawn_source_y) {
            stats->source_rows++;
            last_drawn_source_y = source_y;
        }
        output_y += transform->y_direction;

        /* 0x88011810 exits before advancing vertical state after the final
         * output row. This matters when a downscaled frame ends near its last
         * source row. */
        if (output_row + 1u == output_rows) {
            break;
        }

        /*
         * 0x88011824-0x8801185c repeats a source row while the accumulator is
         * above 0x0fff. Otherwise it advances and adds y_scale, discarding as
         * many packed source rows as necessary before another output row.
         */
        if (vertical_accumulator < KI_RENDERER_FIXED_ONE) {
            return KI_PACKED_RENDER_INVALID_FRAME;
        }
        vertical_accumulator -= KI_RENDERER_FIXED_ONE;
        if (vertical_accumulator > FIXED_MASK) {
            continue;
        }

        do {
            vertical_accumulator += transform->y_scale;
            row_offset += row_size;
            source_y++;

            if (vertical_accumulator > FIXED_MASK) {
                break;
            }
            if (source_y >= height || row_offset >= frame_data_size) {
                return KI_PACKED_RENDER_TRUNCATED;
            }

            /* The skipped row's first byte points directly to the next row. */
            row_size = frame_data[row_offset];
            if (row_size == 0) {
                return KI_PACKED_RENDER_INVALID_FRAME;
            }
            if (row_size > frame_data_size - row_offset) {
                return KI_PACKED_RENDER_TRUNCATED;
            }
        } while (vertical_accumulator <= FIXED_MASK);
    }

    return KI_PACKED_RENDER_OK;
}
