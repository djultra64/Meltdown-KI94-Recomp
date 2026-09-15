#include "ki/original/ki15d_8800842c.h"

#include <stdint.h>

static uint32_t sign_extend_u16(uint16_t value)
{
    if ((value & UINT16_C(0x8000)) != 0) {
        return UINT32_C(0xffff0000) | value;
    }
    return value;
}

static uint32_t multiply_low_32(uint32_t left, uint32_t right)
{
    return (uint32_t)((uint64_t)left * (uint64_t)right);
}

static uint32_t arithmetic_shift_right_8(uint32_t value)
{
    const uint32_t sign_fill =
        (value & UINT32_C(0x80000000)) != 0 ? UINT32_C(0xff000000) : 0;
    return (value >> 8) | sign_fill;
}

KiMemoryResult ki15d_8800842c(KiMemory *memory, uint64_t record_address)
{
    uint16_t raw_time_scale = 0;
    uint16_t raw_acceleration = 0;
    uint32_t velocity = 0;
    uint32_t position = 0;

    /* Preserve the original load order before either state word is changed. */
    KiMemoryResult result = ki_memory_read_u16_le(
        memory, record_address + 0x40, &raw_time_scale);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u16_le(memory, record_address + 0x3c,
                                   &raw_acceleration);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u32_le(memory, record_address + 0x10, &velocity);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    const uint32_t time_scale = sign_extend_u16(raw_time_scale);
    const uint32_t acceleration = sign_extend_u16(raw_acceleration);

    /*
     * The R4600 first shifts velocity left in a 32-bit register. High bits are
     * intentionally discarded before acceleration is subtracted.
     */
    const uint32_t integrated_velocity =
        (velocity << 8) - multiply_low_32(acceleration, time_scale);
    const uint32_t new_velocity =
        arithmetic_shift_right_8(integrated_velocity);

    result = ki_memory_read_u32_le(memory, record_address + 0x0c, &position);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t displacement = arithmetic_shift_right_8(
        multiply_low_32(integrated_velocity, time_scale));
    const uint32_t new_position =
        arithmetic_shift_right_8((position << 8) + displacement);

    result = ki_memory_write_u32_le(memory, record_address + 0x10,
                                    new_velocity);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* The position store is the original routine's return delay-slot write. */
    return ki_memory_write_u32_le(memory, record_address + 0x0c,
                                  new_position);
}
