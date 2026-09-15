#include "ki/original/ki15d_88004e54.h"

#include "ki/original/ki15d_88004180.h"
#include "ki/original/ki15d_880054d0.h"
#include "ki/original/ki15d_8800842c.h"

#include <stdint.h>

static Ki15dAnimationResult update_memory_result(KiMemoryResult result)
{
    if (result == KI_MEMORY_UNMAPPED) {
        return KI15D_ANIMATION_UNMAPPED;
    }
    if (result == KI_MEMORY_READ_ONLY) {
        return KI15D_ANIMATION_READ_ONLY;
    }
    return KI15D_ANIMATION_OK;
}

Ki15dAnimationResult ki15d_88004e54_type1a_update(
    KiMemory *memory, uint64_t record_address, uint32_t gp_value, int *active)
{
    KiMemoryResult memory_result =
        ki15d_88004180(memory, record_address);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }

    memory_result = ki15d_8800842c(memory, record_address);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }

    /* `jal 0x88006670` leaves this exact return address in the saved global. */
    const Ki15dAnimationResult animation_result =
        ki15d_88006670_type1a_step(memory, record_address, gp_value,
                                   UINT32_C(0x88004e6c));
    if (animation_result != KI15D_ANIMATION_OK) {
        return animation_result;
    }

    uint16_t vertical_scale = 0;
    memory_result = ki_memory_read_u16_le(memory, record_address + 0x5a,
                                          &vertical_scale);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }
    vertical_scale = (uint16_t)(vertical_scale + UINT16_C(0x0080));
    memory_result = ki_memory_write_u16_le(memory, record_address + 0x5a,
                                           vertical_scale);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }

    uint32_t script_pointer = 0;
    memory_result = ki_memory_read_u32_le(memory, record_address + 0x20,
                                          &script_pointer);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }
    if (script_pointer != 0) {
        *active = 1;
        return KI15D_ANIMATION_OK;
    }

    /* Original branch target 0x880045ac releases the entire object record. */
    uint64_t next_address = 0;
    memory_result =
        ki15d_880054d0(memory, record_address, &next_address);
    if (memory_result != KI_MEMORY_OK) {
        return update_memory_result(memory_result);
    }
    (void)next_address;
    *active = 0;
    return KI15D_ANIMATION_OK;
}
