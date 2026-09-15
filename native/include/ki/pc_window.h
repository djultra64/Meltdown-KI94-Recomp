#ifndef KI_PC_WINDOW_H
#define KI_PC_WINDOW_H

#include <stddef.h>
#include <stdint.h>
#include "ki/pc_viewport.h"

typedef struct KiPcWindow KiPcWindow;

typedef struct KiPcWindowOptions {
    int logical_width, logical_height, display_scale;
    KiPcFitMode fit_mode;
    uint32_t decoration_argb;
    int fullscreen_desktop;
} KiPcWindowOptions;

/*
 * Open a nearest-neighbor PC window without exposing a particular host API to
 * the game renderer. SDL2 is loaded at runtime, so analysis and headless tests
 * continue to build on systems that do not have SDL development headers.
 */
KiPcWindow *ki_pc_window_create(const char *title,
                                int logical_width,
                                int logical_height,
                                int display_scale,
                                char *error,
                                size_t error_capacity);

KiPcWindow *ki_pc_window_create_with_options(const char *title,
                                              const KiPcWindowOptions *options,
                                              char *error,
                                              size_t error_capacity);

/* Return zero after the user closes the window, otherwise keep running. */
int ki_pc_window_process_events(KiPcWindow *window);

/* Present one logical ARGB8888 frame without modifying its source pixels. */
int ki_pc_window_present(KiPcWindow *window,
                         const uint32_t *argb_pixels,
                         size_t stride_pixels,
                         char *error,
                         size_t error_capacity);

void ki_pc_window_delay(KiPcWindow *window, uint32_t milliseconds);
void ki_pc_window_destroy(KiPcWindow *window);

#endif
