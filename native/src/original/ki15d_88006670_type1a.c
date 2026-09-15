#include "ki/original/ki15d_88006670_type1a.h"

#include <stdint.h>

static const uint32_t SAVED_RETURN_ADDRESS = UINT32_C(0x8808727c);
static const uint32_t ORDER_BASE_ADDRESS = UINT32_C(0x880872a0);
static const uint32_t ORDER_SCRATCH_ADDRESS = UINT32_C(0x880872c0);
static const uint32_t ORDER_ENTRY_COUNT = 0x20;

static Ki15dAnimationResult animation_memory_result(KiMemoryResult result)
{
    if (result == KI_MEMORY_UNMAPPED) {
        return KI15D_ANIMATION_UNMAPPED;
    }
    if (result == KI_MEMORY_READ_ONLY) {
        return KI15D_ANIMATION_READ_ONLY;
    }
    return KI15D_ANIMATION_OK;
}

static int is_negative_32(uint32_t value)
{
    return (value & UINT32_C(0x80000000)) != 0;
}

static int signed_less_than_two(uint32_t value)
{
    return is_negative_32(value) || value < 2;
}

/*
 * Reconstruct the helper at 0x88006314 used by command opcode 0x10. It builds
 * a 32-byte scratch order with gp moved to a one-based requested position,
 * then copies all eight scratch words back over the active order.
 */
static Ki15dAnimationResult move_order_entry(KiMemory *memory,
                                             uint32_t one_based_position,
                                             uint32_t gp_value)
{
    uint8_t current_entry = 0;
    KiMemoryResult memory_result = ki_memory_read_u8(
        memory, ORDER_BASE_ADDRESS + one_based_position - 1, &current_entry);
    if (memory_result != KI_MEMORY_OK) {
        return animation_memory_result(memory_result);
    }
    if ((uint32_t)current_entry == gp_value) {
        return KI15D_ANIMATION_OK;
    }

    uint32_t source = ORDER_BASE_ADDRESS;
    const uint32_t insertion_address =
        ORDER_SCRATCH_ADDRESS + one_based_position - 1;
    for (uint32_t index = 0; index < ORDER_ENTRY_COUNT; index++) {
        uint8_t value = 0;
        do {
            memory_result = ki_memory_read_u8(memory, source, &value);
            if (memory_result != KI_MEMORY_OK) {
                return animation_memory_result(memory_result);
            }
            source++;
        } while ((uint32_t)value == gp_value);

        const uint32_t destination = ORDER_SCRATCH_ADDRESS + index;
        if (destination == insertion_address) {
            /* Do not consume the displaced source entry at the insertion. */
            source--;
            value = (uint8_t)gp_value;
        }
        memory_result = ki_memory_write_u8(memory, destination, value);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
    }

    /* The R4600 copies the completed scratch order back as eight words. */
    for (uint32_t offset = 0; offset < ORDER_ENTRY_COUNT; offset += 4) {
        uint32_t value = 0;
        memory_result = ki_memory_read_u32_le(
            memory, ORDER_SCRATCH_ADDRESS + offset, &value);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        memory_result =
            ki_memory_write_u32_le(memory, ORDER_BASE_ADDRESS + offset, value);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
    }
    return KI15D_ANIMATION_OK;
}

static Ki15dAnimationResult execute_opcode_10(KiMemory *memory,
                                              uint32_t script_pointer,
                                              uint32_t gp_value)
{
    uint8_t requested_position = 0;
    const KiMemoryResult memory_result = ki_memory_read_u8(
        memory, script_pointer + 2, &requested_position);
    if (memory_result != KI_MEMORY_OK) {
        return animation_memory_result(memory_result);
    }
    if (requested_position == 0) {
        return KI15D_ANIMATION_OK;
    }

    uint32_t effective_gp = gp_value;
    if (signed_less_than_two(gp_value) && requested_position >= 0x0a) {
        effective_gp ^= UINT32_C(1);
        requested_position = 1;
    }
    return move_order_entry(memory, requested_position, effective_gp);
}

Ki15dAnimationResult ki15d_88006670_type1a_step(
    KiMemory *memory, uint64_t record_address, uint32_t gp_value,
    uint32_t return_address)
{
    KiMemoryResult memory_result =
        ki_memory_write_u32_le(memory, SAVED_RETURN_ADDRESS, return_address);
    if (memory_result != KI_MEMORY_OK) {
        return animation_memory_result(memory_result);
    }

    uint32_t script_pointer = 0;
    memory_result = ki_memory_read_u32_le(memory, record_address + 0x20,
                                          &script_pointer);
    if (memory_result != KI_MEMORY_OK) {
        return animation_memory_result(memory_result);
    }
    if (script_pointer == 0) {
        /* The unrelated descriptor-selection path is not reconstructed yet. */
        return KI15D_ANIMATION_UNSUPPORTED_PATH;
    }

    uint32_t timer = 0;
    memory_result =
        ki_memory_read_u32_le(memory, record_address + 0x18, &timer);
    if (memory_result != KI_MEMORY_OK) {
        return animation_memory_result(memory_result);
    }

    /* `slti timer,0x100` is signed; ordinary active timers take this branch. */
    if (!is_negative_32(timer) && timer >= UINT32_C(0x100)) {
        timer -= UINT32_C(0x100);
        memory_result =
            ki_memory_write_u32_le(memory, record_address + 0x18, timer);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        if (!is_negative_32(timer)) {
            return KI15D_ANIMATION_OK;
        }
    }

    for (;;) {
        uint8_t token = 0;
        uint8_t duration_or_opcode = 0;
        memory_result = ki_memory_read_u8(memory, script_pointer, &token);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        memory_result = ki_memory_read_u8(memory, script_pointer + 1,
                                          &duration_or_opcode);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }

        if (token == 0) {
            if (duration_or_opcode == UINT8_C(0x10)) {
                const Ki15dAnimationResult command_result =
                    execute_opcode_10(memory, script_pointer, gp_value);
                if (command_result != KI15D_ANIMATION_OK) {
                    return command_result;
                }
                /* Opcode 0x10 consumes four stream bytes before continuing. */
                script_pointer += 4;
                continue;
            }
            if (duration_or_opcode == UINT8_C(0x14)) {
                memory_result =
                    ki_memory_write_u32_le(memory, record_address + 0x20, 0);
                return animation_memory_result(memory_result);
            }
            return KI15D_ANIMATION_UNSUPPORTED_PATH;
        }

        uint16_t raw_time_scale = 0;
        memory_result = ki_memory_read_u16_le(memory, record_address + 0x42,
                                              &raw_time_scale);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        const int32_t time_scale =
            (raw_time_scale & UINT16_C(0x8000)) != 0
                ? (int32_t)raw_time_scale - INT32_C(0x10000)
                : (int32_t)raw_time_scale;
        if (time_scale == 0) {
            return KI15D_ANIMATION_INVALID_TIME_SCALE;
        }

        const int64_t numerator = (int64_t)duration_or_opcode << 16;
        const uint32_t scaled_duration =
            (uint32_t)(numerator / (int64_t)time_scale);
        uint8_t timer_low = 0;
        memory_result =
            ki_memory_read_u8(memory, record_address + 0x18, &timer_low);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }

        script_pointer += 2;
        memory_result =
            ki_memory_write_u8(memory, record_address + 0x14, token);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        memory_result = ki_memory_write_u32_le(memory, record_address + 0x20,
                                               script_pointer);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }

        timer = (uint32_t)timer_low + scaled_duration - UINT32_C(0x100);
        memory_result =
            ki_memory_write_u32_le(memory, record_address + 0x18, timer);
        if (memory_result != KI_MEMORY_OK) {
            return animation_memory_result(memory_result);
        }
        if (!is_negative_32(timer)) {
            return KI15D_ANIMATION_OK;
        }
        /* A negative remainder consumes another pair during the same tick. */
    }
}
