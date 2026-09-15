#include "ki/original/ki15d_88003d30.h"

#include "ki/original/ki15d_8800b1fc.h"

KiMemoryResult ki15d_88003d30(KiMemory *memory, uint64_t fighter_address,
                             uint32_t gp_value, uint64_t *particle_address)
{
    *particle_address = 0;
    uint8_t countdown = 0;
    KiMemoryResult result =
        ki_memory_read_u8(memory, fighter_address + 0xc4, &countdown);
    if (result != KI_MEMORY_OK || countdown == 0) {
        /* The zero branch does not even read the cadence byte. */
        return result;
    }

    uint8_t cadence = 0;
    result = ki_memory_read_u8(memory, fighter_address + 0xc5, &cadence);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, fighter_address + 0xc4,
                                (uint8_t)(countdown - 1u));
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /*
     * sb truncates the stored accumulator, NOT the register used by srl/slt.
     * For example cadence 0xff becomes byte 0x0f but compares 16 against 15,
     * and therefore emits. Computing the comparison from the stored byte
     * would silently suppress this original overflow path.
     */
    const uint32_t accumulated = (uint32_t)cadence + 0x10u;
    result = ki_memory_write_u8(memory, fighter_address + 0xc5,
                                (uint8_t)accumulated);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint8_t threshold = (uint8_t)(accumulated & 0x0fu);
    if ((accumulated >> 4) < threshold) {
        return KI_MEMORY_OK;
    }

    /* jal's delay slot resets c5 BEFORE the constructor reads fighter state. */
    result = ki_memory_write_u8(memory, fighter_address + 0xc5, threshold);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    return ki15d_8800b1fc(memory, fighter_address, gp_value,
                          UINT32_C(0x88003d6c), particle_address);
}
