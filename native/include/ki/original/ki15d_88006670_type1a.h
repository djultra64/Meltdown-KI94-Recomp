#ifndef KI_ORIGINAL_KI15D_88006670_TYPE1A_H
#define KI_ORIGINAL_KI15D_88006670_TYPE1A_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Result for the verified type-0x1a path through the common animation routine
 * at 0x88006670. Values 1 and 2 intentionally mirror KiMemoryResult so memory
 * failures remain distinguishable from a deliberately unsupported path.
 */
typedef enum Ki15dAnimationResult {
    KI15D_ANIMATION_OK = 0,
    KI15D_ANIMATION_UNMAPPED = 1,
    KI15D_ANIMATION_READ_ONLY = 2,
    KI15D_ANIMATION_UNSUPPORTED_PATH = 3,
    KI15D_ANIMATION_INVALID_TIME_SCALE = 4
} Ki15dAnimationResult;

/*
 * Advance an already installed type-0x1a particle stream by one game tick.
 *
 * This is intentionally narrower than the complete common interpreter. It
 * implements normal token-duration pairs plus opcodes 0x10 and 0x14, the only
 * command opcodes in the verified Endokuken-impact stream. A zero script
 * pointer or another opcode reports UNSUPPORTED_PATH rather than inventing
 * behavior for unrelated object types.
 *
 * The R4600 saves $ra at 0x8808727c. gp_value participates in opcode 0x10;
 * return_address makes that original global write visible to native tests.
 */
Ki15dAnimationResult ki15d_88006670_type1a_step(
    KiMemory *memory, uint64_t record_address, uint32_t gp_value,
    uint32_t return_address);

#endif
