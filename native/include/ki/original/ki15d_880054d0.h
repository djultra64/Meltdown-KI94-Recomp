#ifndef KI_ORIGINAL_KI15D_880054D0_H
#define KI_ORIGINAL_KI15D_880054D0_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x880054d0.
 *
 * The original receives the current 0x100-byte object record in $fp and
 * clears it in ascending 32-bit word order. Both $a2 and $a3 finish one byte
 * past the record. Returning that address here makes the observable R4600
 * result available to callers and to differential tests.
 *
 * KI_MEMORY_* failures describe the host-side memory map. A successful call
 * reproduces the original routine's RAM writes.
 */
KiMemoryResult ki15d_880054d0(KiMemory *memory, uint64_t record_address,
                              uint64_t *next_address);

#endif
