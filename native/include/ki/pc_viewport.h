#ifndef KI_PC_VIEWPORT_H
#define KI_PC_VIEWPORT_H

typedef enum KiPcFitMode {
    KI_PC_FIT_INTEGER,
    KI_PC_FIT_ASPECT
} KiPcFitMode;

typedef struct KiPcViewport {
    int x, y, width, height;
    int integer_scale; /* Zero when fractional downscale/aspect fit is used. */
} KiPcViewport;

int ki_pc_viewport_layout(int source_width,
                          int source_height,
                          int drawable_width,
                          int drawable_height,
                          KiPcFitMode mode,
                          KiPcViewport *result);

#endif
