#ifndef KI_ORIGINAL_KI15D_8800A2E4_H
#define KI_ORIGINAL_KI15D_8800A2E4_H

#include "ki/memory.h"

/*
 * KI v1.5d contact-effect initializer, 0x8800a2e4..0x8800a378.
 * receiver=$fp, attacker=$t4, descriptor=$t5. Original lookup data must be
 * mapped at 0x88034610 (8-byte rows) and 0x88034647 (variant mapping).
 * These ranges overlap in the original address space; do not split/copy them.
 *
 * Preserves guest memory effects and ordered accesses, including aliased input
 * records. A zero descriptor byte 0x1d leaves all receiver fields untouched.
 * On mapping failure, earlier writes remain. The API does not return temporary
 * CPU registers or execute the surrounding hit processing at 0x8800a37c.
 * Collision, damage, pause control and scheduling are not recovered here.
 */
KiMemoryResult ki15d_8800a2e4(KiMemory *memory, uint64_t receiver,
                             uint64_t attacker, uint64_t descriptor);

#endif
