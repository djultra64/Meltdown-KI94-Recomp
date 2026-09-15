#ifndef KI_FRAME_ASSET_H
#define KI_FRAME_ASSET_H

#include "ki/memory.h"

#include <stddef.h>
#include <stdint.h>

enum { KI_FRAME_ASSET_SIZE = 136192 };

typedef enum KiFrameAssetResult {
    KI_FRAME_ASSET_OK = 0,
    KI_FRAME_ASSET_ARGUMENT,
    KI_FRAME_ASSET_IO,
    KI_FRAME_ASSET_SIZE_ERROR,
    KI_FRAME_ASSET_IDENTITY,
    KI_FRAME_ASSET_MEMORY
} KiFrameAssetResult;

/* Authenticate the exact extracted boot request before changing mutable RAM. */
KiFrameAssetResult ki_frame_asset_load(KiMemory *memory, const char *path);
KiFrameAssetResult ki_frame_asset_load_bytes(KiMemory *memory,
                                             const uint8_t *bytes,
                                             size_t size);

#endif
