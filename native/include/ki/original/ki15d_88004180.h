#ifndef KI_ORIGINAL_KI15D_88004180_H
#define KI_ORIGINAL_KI15D_88004180_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x88004180.
 *
 * The original receives the current object record in $fp. It applies a signed
 * one-shot movement amount from offset 0x7e, or an unsigned persistent amount
 * from offset 0x84 when the one-shot value is zero. Signed direction values at
 * 0x7c and 0x80 produce deltas for the position words at 0x04 and 0x08.
 *
 * Arithmetic deliberately follows 32-bit R4600 rules. The two most recent
 * deltas are also written to the original globals at 0x88087b20/0x88087b24.
 */
KiMemoryResult ki15d_88004180(KiMemory *memory, uint64_t record_address);

#endif
