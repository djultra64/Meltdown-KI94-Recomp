#include "ki/original/ki15d_8800b1fc.h"

#include "ki/original/ki15d_880053f4.h"
#include "ki/original/ki15d_880063ac.h"

#include <stdint.h>

static const uint32_t SAVED_RETURN_ADDRESS = UINT32_C(0x88087274);
static const uint32_t PARTICLE_ANIMATION_INDEX = UINT32_C(0x8800b2d8);

KiMemoryResult ki15d_8800b1fc(KiMemory *memory, uint64_t fighter_address,
                              uint32_t gp_value, uint32_t return_address,
                              uint64_t *particle_address)
{
    /* The constructor borrows a shared tail and therefore saves $ra first. */
    KiMemoryResult result =
        ki_memory_write_u32_le(memory, SAVED_RETURN_ADDRESS, return_address);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint64_t record = 0;
    result = ki15d_880053f4(memory, &record);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    *particle_address = record;

    result = ki_memory_write_u8(memory, record + 0x00, UINT8_C(0x1a));
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint8_t byte_value = 0;
    result = ki_memory_read_u8(memory, fighter_address + 0xc7, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, record + 0x8e,
                                (uint8_t)(byte_value & UINT8_C(0x1f)));
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Position words 0x04 and 0x08 are copied in the original load order. */
    uint32_t word_value = 0;
    result = ki_memory_read_u32_le(memory, fighter_address + 0x08,
                                   &word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record + 0x08, word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u32_le(memory, fighter_address + 0x04,
                                   &word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record + 0x04, word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint32_t direction_x = 0;
    uint32_t direction_y = 0;
    result = ki_memory_read_u32_le(memory, fighter_address + 0x74,
                                   &direction_x);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u32_le(memory, fighter_address + 0x78,
                                   &direction_y);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    if (gp_value == 0) {
        /* `negu` wraps at 32 bits before the low halfword is stored. */
        direction_x = 0 - direction_x;
        direction_y = 0 - direction_y;
    }
    result = ki_memory_write_u16_le(memory, record + 0x7c,
                                    (uint16_t)direction_x);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u16_le(memory, record + 0x80,
                                    (uint16_t)direction_y);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    uint8_t emission_countdown = 0;
    result = ki_memory_read_u8(memory, fighter_address + 0xc4,
                               &emission_countdown);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t persistent_movement =
        (uint32_t)emission_countdown << 3;
    result = ki_memory_write_u16_le(memory, record + 0x84,
                                    (uint16_t)persistent_movement);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    const uint32_t one_shot_movement =
        (persistent_movement << 3) - UINT32_C(0x00000c00);
    result = ki_memory_write_u16_le(memory, record + 0x7e,
                                    (uint16_t)one_shot_movement);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    result = ki_memory_read_u8(memory, fighter_address + 0xc6, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_read_u32_le(memory, fighter_address + 0x0c,
                                   &word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    word_value += (uint32_t)byte_value << 8;
    result = ki_memory_write_u32_le(memory, record + 0x0c, word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, record + 0x94, UINT8_C(0x60));
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Values below 0x1a grow from 0x0800; later emissions use full scale. */
    result = ki_memory_read_u8(memory, fighter_address + 0xc4,
                               &emission_countdown);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    uint16_t initial_scale = UINT16_C(0x1000);
    if (emission_countdown < UINT8_C(0x1a)) {
        initial_scale =
            (uint16_t)(((uint32_t)emission_countdown << 6) + 0x0800u);
    }
    result = ki_memory_write_u16_le(memory, record + 0x58, initial_scale);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u16_le(memory, record + 0x5a, initial_scale);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    result = ki_memory_write_u8(memory, record + 0x24, UINT8_C(0x02));
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record + 0x3c,
                                    UINT32_C(0xfffffff6));
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record + 0x10,
                                    UINT32_C(0x000000a0));
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* The second c7 read and right shift have no memory output but are real. */
    result = ki_memory_read_u8(memory, fighter_address + 0xc7, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    (void)(byte_value >> 5);

    uint8_t animation_index = 0;
    result =
        ki_memory_read_u8(memory, PARTICLE_ANIMATION_INDEX, &animation_index);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    const uint32_t shared_flags = UINT32_C(0x01000100);
    result = ki_memory_write_u32_le(memory, record + 0x40, shared_flags);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record + 0x4c, shared_flags);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    return ki15d_880063ac(memory, animation_index, record);
}
