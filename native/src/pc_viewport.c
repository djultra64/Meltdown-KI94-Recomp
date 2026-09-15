#include "ki/pc_viewport.h"

#include <stdint.h>

int ki_pc_viewport_layout(int source_width,
                          int source_height,
                          int drawable_width,
                          int drawable_height,
                          KiPcFitMode mode,
                          KiPcViewport *result)
{
    if (source_width <= 0 || source_height <= 0 || drawable_width <= 0 ||
        drawable_height <= 0 || result == 0 ||
        (mode != KI_PC_FIT_INTEGER && mode != KI_PC_FIT_ASPECT)) return 0;

    int width;
    int height;
    int scale = 0;
    const int fit_x = drawable_width / source_width;
    const int fit_y = drawable_height / source_height;
    if (mode == KI_PC_FIT_INTEGER && fit_x > 0 && fit_y > 0) {
        scale = fit_x < fit_y ? fit_x : fit_y;
        width = source_width * scale;
        height = source_height * scale;
    } else if ((int64_t)drawable_width*source_height <=
               (int64_t)drawable_height*source_width) {
        /* A drawable smaller than one source pixel scale falls back to the
         * same floor-rounded aspect fit used by the explicit aspect mode. */
        width = drawable_width;
        height = (int)((int64_t)drawable_width * source_height / source_width);
    } else {
        height = drawable_height;
        width = (int)((int64_t)drawable_height * source_width / source_height);
    }
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    *result = (KiPcViewport){(drawable_width - width) / 2,
                             (drawable_height - height) / 2,
                             width, height, scale};
    return 1;
}
