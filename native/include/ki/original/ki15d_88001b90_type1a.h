#ifndef KI_ORIGINAL_KI15D_88001B90_TYPE1A_H
#define KI_ORIGINAL_KI15D_88001B90_TYPE1A_H

#include "ki/memory.h"
#include "ki/packed_renderer.h"

#include <stdint.h>

typedef enum Ki15dHorizontalClip {
    KI15D_HORIZONTAL_CLIP_NONE = 0,
    KI15D_HORIZONTAL_CLIP_LEFT = 1,
    KI15D_HORIZONTAL_CLIP_RIGHT = 2,
    KI15D_HORIZONTAL_CLIP_BOTH = 3
} Ki15dHorizontalClip;

typedef struct Ki15dType1aRenderCall {
    uint32_t frame_address;
    uint32_t palette_source_address;
    uint32_t palette_table_address;
    uint32_t framebuffer_address;
    uint32_t row_callback_address;
    uint16_t frame_width;
    uint16_t frame_height;
    uint32_t scaled_width;
    uint8_t framebuffer_bank;
    uint8_t render_mode;
    Ki15dHorizontalClip horizontal_clip;
    KiPackedRenderTransform transform;
} Ki15dType1aRenderCall;

typedef enum Ki15dType1aRenderResult {
    KI15D_TYPE1A_RENDER_OK = 0,
    KI15D_TYPE1A_RENDER_INVALID_ARGUMENT = 1,
    KI15D_TYPE1A_RENDER_MEMORY_ERROR = 2,
    KI15D_TYPE1A_RENDER_UNSUPPORTED_STATE = 3,
    KI15D_TYPE1A_RENDER_NOT_VISIBLE = 4,
    KI15D_TYPE1A_RENDER_INVALID_FRAME = 5
} Ki15dType1aRenderResult;

/*
 * Reconstruct the verified display-class-0x60, flags-0 path through KI v1.5d
 * routine 0x88001b90 and common renderer setup 0x880107a8.
 *
 * The routine reads the original object record and renderer globals from
 * guest memory. It returns a host-neutral descriptor; copying the selected
 * palette and writing the framebuffer remain separate operations. Unsupported
 * original branches, including nonzero palette-control fields 0x8e/0x97,
 * fail explicitly instead of being approximated.
 */
Ki15dType1aRenderResult ki15d_88001b90_type1a_prepare_render(
    const KiMemory *memory,
    uint32_t record_address,
    Ki15dType1aRenderCall *call);

#endif
