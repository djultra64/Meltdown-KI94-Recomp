#include "ki/bgr555.h"

#include <stdint.h>

uint16_t ki_bgr555_saturating_add(uint16_t destination, uint16_t blend)
{
    const uint32_t left = destination;
    const uint32_t right = blend;
    const uint32_t sum = left + right;

    /*
     * A carry out of any five-bit component appears in mask 0x8420. The MIPS
     * XOR/AND sequence finds those carries without unpacking the components.
     * Subtracting the mask shifted by five then fills each overflowing field
     * with ones before OR combines it with the ordinary sum.
     */
    const uint32_t carries = ((left ^ right) ^ sum) & UINT32_C(0x8420);
    const uint32_t saturation_bits = carries - (carries >> 5);

    return (uint16_t)(sum | saturation_bits);
}
