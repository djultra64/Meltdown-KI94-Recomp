#ifndef KI_ORIGINAL_KI15D_880053F4_H
#define KI_ORIGINAL_KI15D_880053F4_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x880053f4.
 *
 * The original scans the secondary-object records beginning at 0x8808be00.
 * Byte zero is the occupied/type marker. The selected 0x100-byte record is
 * cleared from its last word to its first, and its arcade address is returned
 * in $a2. There is deliberately no "pool full" result: after 29 occupied
 * searchable records, the R4600 code selects the immediately following one.
 *
 * The return value below reports host memory-map errors only. On success,
 * selected_address receives the value that the original leaves in $a2.
 */
KiMemoryResult ki15d_880053f4(KiMemory *memory, uint64_t *selected_address);

#endif
