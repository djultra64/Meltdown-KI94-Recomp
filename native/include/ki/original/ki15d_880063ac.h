#ifndef KI_ORIGINAL_KI15D_880063AC_H
#define KI_ORIGINAL_KI15D_880063AC_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x880063ac.
 *
 * The original receives an animation-table index in $s0 and an object-record
 * address in $t2. It applies the corresponding 8-byte entry from 0x8805e310,
 * replacing the temporary index/state in the object with a script pointer and
 * the parameters needed by the common animation interpreter.
 */
KiMemoryResult ki15d_880063ac(KiMemory *memory, uint32_t animation_index,
                              uint64_t record_address);

#endif
