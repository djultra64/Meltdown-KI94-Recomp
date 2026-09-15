#include "ki/original/ki15d_8800a2e4.h"

KiMemoryResult ki15d_8800a2e4(KiMemory *memory, uint64_t receiver,
                             uint64_t attacker, uint64_t descriptor)
{
    uint8_t effect = 0;
    KiMemoryResult result = ki_memory_read_u8(memory, descriptor + 0x1d, &effect);
    if (result != KI_MEMORY_OK || effect == 0) {
        return result;
    }
    const uint64_t row = UINT64_C(0x88034610) + (effect & 0x0fu) * 8u;
    uint8_t value = 0;
    result = ki_memory_read_u8(memory, row, &value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, receiver + 0xc4, value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u8(memory, row + 1, &value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, receiver + 0xc5, value);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint8_t flags = 0;
    result = ki_memory_read_u8(memory, descriptor + 0x13, &flags);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    /* The original reads descriptor height even when the flag is clear. */
    uint8_t height = 0;
    result = ki_memory_read_u8(memory, descriptor + 0x1a, &height);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    if ((flags & 0x40u) == 0) {
        uint32_t attacker_z = 0, receiver_z = 0;
        result = ki_memory_read_u32_le(memory, attacker + 0x0c, &attacker_z);
        if (result != KI_MEMORY_OK) {
            return result;
        }
        result = ki_memory_read_u32_le(memory, receiver + 0x0c, &receiver_z);
        if (result != KI_MEMORY_OK) {
            return result;
        }
        /* subu wraps to 32 bits, then bgez tests the wrapped sign. A host
         * signed subtraction could overflow and is not equivalent. srl is
         * logical; sb later retains only bits 8..15 of a nonnegative delta. */
        const uint32_t delta = attacker_z - receiver_z;
        height = (delta & UINT32_C(0x80000000)) != 0
                     ? 0 : (uint8_t)(delta >> 8);
    }
    result = ki_memory_write_u8(memory, receiver + 0xc6, height);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Do not cache the first descriptor read: receiver stores may alias it.
     * The attacker variant is also read even if a mapping overrides it. */
    result = ki_memory_read_u8(memory, descriptor + 0x1d, &effect);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    uint8_t variant = 0;
    result = ki_memory_read_u8(memory, attacker + 0x8e, &variant);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const unsigned int mapping_index = effect >> 4;
    if (mapping_index != 0) {
        result = ki_memory_read_u8(memory, UINT64_C(0x88034647) + mapping_index,
                                   &variant);
        if (result != KI_MEMORY_OK) {
            return result;
        }
    }
    result = ki_memory_read_u8(memory, row + 2, &value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    /* Original addu followed by sb, not saturation or a five-bit mask. */
    return ki_memory_write_u8(memory, receiver + 0xc7,
                              (uint8_t)(variant + ((uint32_t)value << 5)));
}
