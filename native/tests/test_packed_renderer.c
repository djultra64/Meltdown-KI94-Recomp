#include "ki/packed_renderer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_fixed_point_scaling_and_upward_rows(void)
{
    /*
     * Two 8-pixel rows. Each starts with one transparent pixel, contains a
     * two-pixel color-5 run, skips one pixel, then has one color-6 pixel.
     */
    static const uint8_t frame[] = {
        0, 0, 0, 0, 8, 0, 2, 0,
        6, 1, 0x29, 0, 1, 0x30,
        6, 1, 0x29, 0, 1, 0x30,
    };
    uint16_t palette[32] = {0};
    palette[5] = 0x0461;
    palette[6] = 0x0862;

    uint16_t pixels[12 * 6] = {0};
    KiBgr555Surface surface = {pixels, 12, 6, 12};
    const KiPackedRenderTransform transform = {
        .destination_x = 2,
        .destination_y = 4,
        .x_direction = 1,
        .y_direction = -1,
        .x_scale = 0x1800,
        .x_remainder = 0x0800,
        .y_scale = 0x1800,
        .y_accumulator = 0x2000,
        .output_rows = 3,
    };
    KiPackedRenderStats stats = {0};

    assert(ki_render_packed_frame_bgr555(
               frame, sizeof(frame), palette, &surface, &transform, &stats) ==
           KI_PACKED_RENDER_OK);

    /* Row zero is repeated at y=4 and y=3; row one lands at y=2. */
    for (int y = 2; y <= 4; y++) {
        assert(pixels[y * 12 + 4] == 0x0461);
        assert(pixels[y * 12 + 5] == 0x0461);
        assert(pixels[y * 12 + 6] == 0x0461);
        assert(pixels[y * 12 + 7] == 0);
        assert(pixels[y * 12 + 8] == 0x0862);
        assert(pixels[y * 12 + 9] == 0x0862);
    }
    assert(stats.output_rows == 3);
    assert(stats.source_rows == 2);
    assert(stats.blended_pixels == 15);
    assert(stats.clipped_pixels == 0);
}

static void test_reverse_orientation_and_clipping(void)
{
    static const uint8_t frame[] = {
        0, 0, 0, 0, 4, 0, 1, 0,
        3, 1, 0x29,
    };
    uint16_t palette[32] = {0};
    palette[5] = 0x0020;

    uint16_t pixels[4] = {0};
    KiBgr555Surface surface = {pixels, 4, 1, 4};
    const KiPackedRenderTransform transform = {
        .destination_x = 2,
        .destination_y = 0,
        .x_direction = -1,
        .y_direction = 1,
        .x_scale = KI_RENDERER_FIXED_ONE,
        .x_remainder = 0,
        .y_scale = KI_RENDERER_FIXED_ONE,
        .y_accumulator = KI_RENDERER_FIXED_ONE,
        .output_rows = 1,
    };
    KiPackedRenderStats stats = {0};

    assert(ki_render_packed_frame_bgr555(
               frame, sizeof(frame), palette, &surface, &transform, &stats) ==
           KI_PACKED_RENDER_OK);
    assert(pixels[0] == 0x0020);
    assert(pixels[1] == 0x0020);
    assert(pixels[2] == 0);
    assert(pixels[3] == 0);

    /* Moving the same two output pixels left clips exactly one of them. */
    memset(pixels, 0, sizeof(pixels));
    KiPackedRenderTransform clipped = transform;
    clipped.destination_x = 0;
    memset(&stats, 0, sizeof(stats));
    assert(ki_render_packed_frame_bgr555(
               frame, sizeof(frame), palette, &surface, &clipped, &stats) ==
           KI_PACKED_RENDER_OK);
    assert(stats.blended_pixels == 0);
    assert(stats.clipped_pixels == 2);
}

static void test_vertical_downscaling_skips_packed_rows(void)
{
    /* Four one-pixel rows, each using a different palette index. */
    static const uint8_t frame[] = {
        0, 0, 0, 0, 1, 0, 4, 0,
        3, 0, 0x28,
        3, 0, 0x30,
        3, 0, 0x38,
        3, 0, 0x40,
    };
    uint16_t palette[32] = {0};
    palette[5] = 0x0001;
    palette[6] = 0x0002;
    palette[7] = 0x0003;
    palette[8] = 0x0004;

    uint16_t pixels[2] = {0};
    KiBgr555Surface surface = {pixels, 1, 2, 1};
    const KiPackedRenderTransform transform = {
        .destination_x = 0,
        .destination_y = 1,
        .x_direction = 1,
        .y_direction = -1,
        .x_scale = KI_RENDERER_FIXED_ONE,
        .x_remainder = 0,
        .y_scale = 0x0800,
        .y_accumulator = KI_RENDERER_FIXED_ONE,
        .source_offset = 11,
        .output_rows = 2,
    };
    KiPackedRenderStats stats = {0};

    assert(ki_render_packed_frame_bgr555(
               frame, sizeof(frame), palette, &surface, &transform, &stats) ==
           KI_PACKED_RENDER_OK);

    /* Row one is selected initially; the scaler then discards row two. */
    assert(pixels[1] == 0x0002);
    assert(pixels[0] == 0x0004);
    assert(stats.output_rows == 2);
    assert(stats.source_rows == 2);
}

int main(void)
{
    test_fixed_point_scaling_and_upward_rows();
    test_reverse_orientation_and_clipping();
    test_vertical_downscaling_skips_packed_rows();
    puts("native packed renderer tests: ok");
    return 0;
}
