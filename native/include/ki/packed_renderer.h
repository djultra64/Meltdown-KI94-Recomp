#ifndef KI_PACKED_RENDERER_H
#define KI_PACKED_RENDERER_H

#include <stddef.h>
#include <stdint.h>

/* KI's software renderer uses 12 fractional bits for sprite scaling. */
enum { KI_RENDERER_FIXED_ONE = 0x1000 };

typedef struct KiBgr555Surface {
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    size_t stride_pixels;
} KiBgr555Surface;

/*
 * State observed immediately before KI v1.5d renders the first packed row.
 * Keeping the two accumulators explicit is important: ordinary bitmap scaling
 * does not reproduce the arcade renderer's rounding decisions.
 */
typedef struct KiPackedRenderTransform {
    int destination_x;
    int destination_y;
    int x_direction;
    int y_direction;
    uint32_t x_scale;
    uint32_t x_remainder;
    uint32_t y_scale;
    uint32_t y_accumulator;

    /*
     * Byte offset of the first source row selected by the arcade scaler.
     * Zero selects the first packed row immediately after the 8-byte header.
     * Downscaled sprites may skip one or more rows before their first output.
     */
    uint32_t source_offset;

    /* Zero derives the count from frame height and y_scale. */
    uint32_t output_rows;
} KiPackedRenderTransform;

typedef struct KiPackedRenderStats {
    uint32_t output_rows;
    uint32_t source_rows;
    uint32_t blended_pixels;
    uint32_t clipped_pixels;
} KiPackedRenderStats;

typedef enum KiPackedRenderResult {
    KI_PACKED_RENDER_OK = 0,
    KI_PACKED_RENDER_INVALID_ARGUMENT = 1,
    KI_PACKED_RENDER_TRUNCATED = 2,
    KI_PACKED_RENDER_INVALID_FRAME = 3
} KiPackedRenderResult;

/*
 * Render one original packed frame over an existing BGR555 surface.
 *
 * Color indices select 32 BGR555 blend operands. Every visible destination
 * pixel is combined with ki_bgr555_saturating_add(), matching the instruction
 * sequence at 0x8801193c-0x8801195c. Rows and runs use the recovered fixed-
 * point accumulators rather than a host graphics library's scaler.
 */
KiPackedRenderResult ki_render_packed_frame_bgr555(
    const uint8_t *frame_data,
    size_t frame_data_size,
    const uint16_t palette[32],
    KiBgr555Surface *destination,
    const KiPackedRenderTransform *transform,
    KiPackedRenderStats *stats);

#endif
