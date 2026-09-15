#include "ki/pc_window.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint32_t pixels[16 * 12];
    uint32_t unchanged[16 * 12];
    for (size_t index = 0; index < sizeof(pixels) / sizeof(pixels[0]); ++index)
        pixels[index] = UINT32_C(0xff000000) | (uint32_t)(index * 7919u);
    memcpy(unchanged, pixels, sizeof(pixels));

    const KiPcWindowOptions options = {
        16, 12, 2, KI_PC_FIT_INTEGER, UINT32_C(0xff224466), 0
    };
    char error[256] = {0};
    KiPcWindow *window = ki_pc_window_create_with_options(
        "VID-365 SDL smoke", &options, error, sizeof(error));
    if (window == NULL ||
        !ki_pc_window_present(window, pixels, 16, error, sizeof(error)) ||
        memcmp(pixels, unchanged, sizeof(pixels)) != 0) {
        fprintf(stderr, "SDL presentation smoke failed: %s\n", error);
        ki_pc_window_destroy(window);
        return 1;
    }
    ki_pc_window_destroy(window);
    puts("SDL presentation source-buffer invariance passed");
    return 0;
}
