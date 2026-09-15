#ifndef KI_ORIGINAL_KI15D_8800B1FC_H
#define KI_ORIGINAL_KI15D_8800B1FC_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x8800b1fc.
 *
 * The current fighter arrives in $fp. The constructor allocates a secondary
 * record, creates a type-0x1a Endokuken-impact particle, derives its motion and
 * scale from fighter fields, and installs the animation selected by original
 * data byte 0x8800b2d8.
 *
 * gp_value reproduces the original zero/nonzero orientation branch. The R4600
 * stores $ra at 0x88087274 while using a shared setup tail, so return_address is
 * explicit rather than silently dropping that observable write.
 */
KiMemoryResult ki15d_8800b1fc(KiMemory *memory, uint64_t fighter_address,
                              uint32_t gp_value, uint32_t return_address,
                              uint64_t *particle_address);

#endif
