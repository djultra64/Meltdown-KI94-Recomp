#ifndef KI_PACKED_FRAME_H
#define KI_PACKED_FRAME_H

#include <stddef.h>
#include <stdint.h>

/* 0xff is outside the frame format's five-bit color-index range. */
enum { KI_PACKED_FRAME_TRANSPARENT = 0xff };

typedef enum KiPackedFrameResult {
    KI_PACKED_FRAME_OK = 0,
    KI_PACKED_FRAME_INVALID_ARGUMENT = 1,
    KI_PACKED_FRAME_TRUNCATED = 2,
    KI_PACKED_FRAME_INVALID_DIMENSIONS = 3,
    KI_PACKED_FRAME_OUTPUT_TOO_SMALL = 4,
    KI_PACKED_FRAME_INVALID_ROW = 5
} KiPackedFrameResult;

/*
 * Header shared by the packed frames selected by KI v1.5d at 0x880016c4.
 * The first two signed values are the frame's origin relative to its object.
 * Width and height describe the logical, unscaled index image that follows.
 */
typedef struct KiPackedFrameInfo {
    int16_t origin_x;
    int16_t origin_y;
    uint16_t width;
    uint16_t height;
    size_t bytes_consumed;
} KiPackedFrameInfo;

/*
 * Decode one frame to row-major five-bit indices. Transparent pixels become
 * KI_PACKED_FRAME_TRANSPARENT. This stage intentionally does not apply the
 * renderer's palette, scaling, clipping, orientation, or blend operation.
 *
 * The packet grammar was recovered from the KI v1.5d renderer beginning at
 * 0x880107a8. Each row starts with its byte size; size one is an empty row and
 * every nonempty row then has an initial transparent skip. A nonzero packet
 * byte contains a five-bit color index in bits 7..3 and a run length minus one
 * in bits 2..0. Zero introduces another skip byte.
 */
KiPackedFrameResult ki_decode_packed_frame(const uint8_t *data,
                                           size_t data_size,
                                           uint8_t *output_indices,
                                           size_t output_capacity,
                                           KiPackedFrameInfo *info);

#endif
