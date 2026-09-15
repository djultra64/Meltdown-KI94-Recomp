#ifndef KI_ORIGINAL_KI15D_8802D5B0_H
#define KI_ORIGINAL_KI15D_8802D5B0_H

#include <stdint.h>

/*
 * Native reconstruction of the Killer Instinct v1.5d routine at 0x8802d5b0.
 *
 * The full 64-bit result is intentional. The original three-call wrapper
 * forwards bit 32 from one call to the next, so a uint32_t return type would
 * silently change the arcade behavior.
 */
uint64_t ki15d_8802d5b0(uint64_t input);

#endif
