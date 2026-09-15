#include "ki/original/ki15d_8802d5b0.h"

uint64_t ki15d_8802d5b0(uint64_t input)
{
    /*
     * These unsigned shifts mirror the R4600 DSLL/DSRL pairs exactly. Using
     * uint64_t also gives the required modulo-2^64 behavior without relying on
     * implementation-defined signed shifts.
     */
    const uint64_t bit_0_promoted_to_bit_32 = (input << 63) >> 31;
    const uint64_t low_33_shifted_right = (input << 31) >> 32;
    const uint64_t low_20_shifted_left = (input << 44) >> 32;

    const uint64_t mixed =
        (bit_0_promoted_to_bit_32 | low_33_shifted_right) ^
        low_20_shifted_left;
    const uint64_t feedback = (mixed >> 20) & UINT64_C(0x0fff);

    return mixed ^ feedback;
}
