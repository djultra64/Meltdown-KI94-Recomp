#ifndef KI_ORIGINAL_KI15D_88004E54_H
#define KI_ORIGINAL_KI15D_88004E54_H

#include "ki/original/ki15d_88006670_type1a.h"

#include <stdint.h>

/*
 * Native reconstruction of the type-0x1a handler at 0x88004e54.
 *
 * One call applies planar motion, scaled vertical motion, animation, and the
 * original 0x0080 vertical-scale growth in that exact order. If animation
 * opcode 0x14 clears the script pointer, the handler immediately invokes the
 * general record-release routine just as the branch to 0x880045ac does.
 *
 * active receives one when the record survived the tick and zero after it was
 * released. Animation errors retain the explicitly partial status of the
 * shared 0x88006670 reconstruction.
 */
Ki15dAnimationResult ki15d_88004e54_type1a_update(
    KiMemory *memory, uint64_t record_address, uint32_t gp_value, int *active);

#endif
