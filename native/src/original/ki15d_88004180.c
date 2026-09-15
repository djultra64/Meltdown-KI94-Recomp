#include "ki/original/ki15d_88004180.h"

#include <stdint.h>

static const uint32_t LAST_PLANAR_DELTA_X = UINT32_C(0x88087b20);
static const uint32_t LAST_PLANAR_DELTA_Y = UINT32_C(0x88087b24);

/* Convert a little-endian object halfword to its R4600 signed value. */
static uint32_t sign_extend_u16(uint16_t value)
{
    if ((value & UINT16_C(0x8000)) != 0) {
        return UINT32_C(0xffff0000) | value;
    }
    return value;
}

/*
 * MIPS `mult` exposes the low 32 product bits through LO. Unsigned arithmetic
 * has the same low bits as a signed two's-complement multiplication and avoids
 * undefined signed overflow in C.
 */
static uint32_t multiply_low_32(uint32_t left, uint32_t right)
{
    return (uint32_t)((uint64_t)left * (uint64_t)right);
}

/* Reproduce `sra value,8` without relying on the host's signed-shift rules. */
static uint32_t arithmetic_shift_right_8(uint32_t value)
{
    const uint32_t sign_fill =
        (value & UINT32_C(0x80000000)) != 0 ? UINT32_C(0xff000000) : 0;
    return (value >> 8) | sign_fill;
}

KiMemoryResult ki15d_88004180(KiMemory *memory, uint64_t record_address)
{
    uint16_t halfword = 0;
    KiMemoryResult result =
        ki_memory_read_u16_le(memory, record_address + 0x7e, &halfword);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Entry 0x88004180 treats the one-shot amount as a signed halfword. */
    uint32_t movement_amount = sign_extend_u16(halfword);
    if (movement_amount == 0) {
        result = ki_memory_read_u16_le(memory, record_address + 0x84,
                                       &halfword);
        if (result != KI_MEMORY_OK) {
            return result;
        }
        movement_amount = halfword;
        if (movement_amount == 0) {
            return KI_MEMORY_OK;
        }
    }

    result = ki_memory_read_u16_le(memory, record_address + 0x7c, &halfword);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t direction_x = sign_extend_u16(halfword);

    /* The one-shot amount is consumed even when persistent movement was used. */
    result = ki_memory_write_u16_le(memory, record_address + 0x7e, 0);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint32_t position = 0;
    result = ki_memory_read_u32_le(memory, record_address + 0x04, &position);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t delta_x = arithmetic_shift_right_8(
        multiply_low_32(movement_amount, direction_x));
    result = ki_memory_write_u32_le(memory, LAST_PLANAR_DELTA_X, delta_x);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x04,
                                    position + delta_x);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    result = ki_memory_read_u16_le(memory, record_address + 0x80, &halfword);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t direction_y = sign_extend_u16(halfword);
    result = ki_memory_read_u32_le(memory, record_address + 0x08, &position);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t delta_y = arithmetic_shift_right_8(
        multiply_low_32(movement_amount, direction_y));
    result = ki_memory_write_u32_le(memory, LAST_PLANAR_DELTA_Y, delta_y);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x08,
                                    position + delta_y);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint16_t decay = 0;
    uint16_t persistent_amount = 0;
    result = ki_memory_read_u16_le(memory, record_address + 0x86, &decay);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u16_le(memory, record_address + 0x84,
                                   &persistent_amount);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* `subu` followed by `bgtz` clamps zero and unsigned underflow to zero. */
    uint32_t remaining = (uint32_t)persistent_amount - (uint32_t)decay;
    if (remaining == 0 || (remaining & UINT32_C(0x80000000)) != 0) {
        remaining = 0;
    }
    return ki_memory_write_u16_le(memory, record_address + 0x84,
                                  (uint16_t)remaining);
}
