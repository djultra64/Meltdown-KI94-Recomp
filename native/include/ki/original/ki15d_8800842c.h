#ifndef KI_ORIGINAL_KI15D_8800842C_H
#define KI_ORIGINAL_KI15D_8800842C_H

#include "ki/memory.h"

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x8800842c.
 *
 * The current object record arrives in $fp. The routine integrates the signed
 * acceleration at offset 0x3c into velocity 0x10, then integrates that velocity
 * into position 0x0c. Offset 0x40 scales both operations as an 8-bit fixed-point
 * time factor. All shifts, products, and overflows retain original R4600
 * behavior rather than being promoted to host floating point.
 */
KiMemoryResult ki15d_8800842c(KiMemory *memory, uint64_t record_address);

#endif
