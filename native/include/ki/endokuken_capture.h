#ifndef KI_ENDOKUKEN_CAPTURE_H
#define KI_ENDOKUKEN_CAPTURE_H

#include "ki/packed_renderer.h"

#include <stddef.h>
#include <stdint.h>

enum {
    KI_ENDOKUKEN_CAPTURE_FRAME_COUNT = 96,
    KI_ENDOKUKEN_CAPTURE_SAMPLE_COUNT = 315
};

/*
 * One call made by KI v1.5d's type-0x1a secondary-object pass. The transform
 * is the state observed at the first packed row, where all placement and
 * fixed-point calculations are complete. object_address is retained for
 * provenance and for checking the original object-pass order.
 */
typedef struct KiEndokukenCaptureSample {
    uint32_t object_address;
    uint8_t token;
    size_t frame_offset;
    KiPackedRenderTransform transform;
} KiEndokukenCaptureSample;

/* A half-open span into KI_ENDOKUKEN_CAPTURE_SAMPLES for one displayed tick. */
typedef struct KiEndokukenCaptureFrame {
    uint16_t first_sample;
    uint8_t sample_count;
} KiEndokukenCaptureFrame;

extern const KiEndokukenCaptureSample
    KI_ENDOKUKEN_CAPTURE_SAMPLES[KI_ENDOKUKEN_CAPTURE_SAMPLE_COUNT];
extern const KiEndokukenCaptureFrame
    KI_ENDOKUKEN_CAPTURE_FRAMES[KI_ENDOKUKEN_CAPTURE_FRAME_COUNT];

#endif
