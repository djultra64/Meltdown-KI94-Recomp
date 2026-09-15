#include "ki/original/ki15d_880063ac.h"

#include <stdint.h>

static const uint32_t ANIMATION_TABLE_ADDRESS = UINT32_C(0x8805e310);

KiMemoryResult ki15d_880063ac(KiMemory *memory, uint32_t animation_index,
                              uint64_t record_address)
{
    /* `sll` and `addu` in the original intentionally have 32-bit wraparound. */
    const uint32_t entry_address =
        ANIMATION_TABLE_ADDRESS + (animation_index << 3);
    uint8_t byte_value = 0;
    uint32_t word_value = 0;
    KiMemoryResult result = KI_MEMORY_OK;

    /* Entry byte 7 becomes the object's halfword at offset 0x8a. */
    result = ki_memory_read_u8(memory, entry_address + 7, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u16_le(memory, record_address + 0x8a,
                                    byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Entry word 0 is the packed animation-command stream pointer. */
    result = ki_memory_read_u32_le(memory, entry_address, &word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x20,
                                    word_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Byte 4 is stored both as a byte-sized state and a zero-extended word. */
    result = ki_memory_read_u8(memory, entry_address + 4, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u8(memory, record_address + 0x1c, byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x34,
                                    byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Reset the two animation counters before applying entry byte 6. */
    result = ki_memory_read_u8(memory, entry_address + 6, &byte_value);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x18, 0);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, record_address + 0x14, 0);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* In the R4600 code this final write occupies the return delay slot. */
    return ki_memory_write_u16_le(memory, record_address + 0x2c, byte_value);
}
