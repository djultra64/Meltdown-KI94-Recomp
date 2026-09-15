#ifndef KI_ORIGINAL_KI15D_8800700C_H
#define KI_ORIGINAL_KI15D_8800700C_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x8800700c.
 *
 * This leaf uses an unusual implicit register interface in the original code:
 * $a2 is the destination, $s5 is the source, and the low byte of $a3 is the
 * first output byte. All known callers first obtain a cleared secondary-object
 * record from 0x880053f4, then use this routine to assign its type and copy
 * three source words. Their exact field meanings are still under analysis, so
 * the address-based API deliberately avoids premature coordinate names.
 *
 * The return value reports host memory-map errors. The R4600 routine itself
 * returns no semantic value; invalid arcade addresses would raise an exception.
 */
KiMemoryResult ki15d_8800700c(KiMemory *memory, uint64_t destination_address,
                              uint64_t source_address, uint8_t first_byte);

#endif
