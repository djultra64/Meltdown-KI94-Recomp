#include "ki/packed_frame.h"

#include <string.h>

enum { FRAME_HEADER_SIZE = 8 };

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

KiPackedFrameResult ki_decode_packed_frame(const uint8_t *data,
                                           size_t data_size,
                                           uint8_t *output_indices,
                                           size_t output_capacity,
                                           KiPackedFrameInfo *info)
{
    if (data == NULL || output_indices == NULL || info == NULL) {
        return KI_PACKED_FRAME_INVALID_ARGUMENT;
    }
    if (data_size < FRAME_HEADER_SIZE) {
        return KI_PACKED_FRAME_TRUNCATED;
    }

    const uint16_t width = read_u16_le(data + 4);
    const uint16_t height = read_u16_le(data + 6);
    if (width == 0 || height == 0 || width > SIZE_MAX / height) {
        return KI_PACKED_FRAME_INVALID_DIMENSIONS;
    }

    const size_t pixel_count = (size_t)width * height;
    if (output_capacity < pixel_count) {
        return KI_PACKED_FRAME_OUTPUT_TOO_SMALL;
    }

    memset(output_indices, KI_PACKED_FRAME_TRANSPARENT, pixel_count);

    size_t row_start = FRAME_HEADER_SIZE;
    for (uint16_t y = 0; y < height; y++) {
        if (row_start >= data_size) {
            return KI_PACKED_FRAME_TRUNCATED;
        }

        /* A size of one represents a completely transparent row. */
        const size_t row_size = data[row_start];
        if (row_size == 0) {
            return KI_PACKED_FRAME_INVALID_ROW;
        }
        if (row_size > data_size - row_start) {
            return KI_PACKED_FRAME_TRUNCATED;
        }

        const size_t row_end = row_start + row_size;
        if (row_size == 1) {
            row_start = row_end;
            continue;
        }

        /* Every nonempty row begins with an initial transparent skip. */
        size_t cursor = row_start + 1;
        size_t x = data[cursor++];
        if (x > width) {
            return KI_PACKED_FRAME_INVALID_ROW;
        }

        while (cursor < row_end) {
            const uint8_t packet = data[cursor++];

            if (packet == 0) {
                /* Zero switches from adjacent color runs to a transparent gap. */
                if (cursor >= row_end) {
                    return KI_PACKED_FRAME_INVALID_ROW;
                }
                const size_t skip = data[cursor++];
                if (skip > (size_t)width - x) {
                    return KI_PACKED_FRAME_INVALID_ROW;
                }
                x += skip;
                continue;
            }

            const size_t run_length = (packet & 0x07u) + 1u;
            const uint8_t color_index = packet >> 3;
            if (run_length > (size_t)width - x) {
                return KI_PACKED_FRAME_INVALID_ROW;
            }

            memset(output_indices + ((size_t)y * width) + x,
                   color_index, run_length);
            x += run_length;
        }

        row_start = row_end;
    }

    info->origin_x = (int16_t)read_u16_le(data);
    info->origin_y = (int16_t)read_u16_le(data + 2);
    info->width = width;
    info->height = height;
    info->bytes_consumed = row_start;
    return KI_PACKED_FRAME_OK;
}
