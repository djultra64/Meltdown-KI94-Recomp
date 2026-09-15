#include "ki/original/ki15d_8800700c.h"

KiMemoryResult ki15d_8800700c(KiMemory *memory, uint64_t destination_address,
                              uint64_t source_address, uint8_t first_byte)
{
    uint32_t field = 0;
    KiMemoryResult result = KI_MEMORY_OK;

    /*
     * The opening SB changes only destination byte 0. Bytes 1 through 3 stay
     * intact, and source offset 0 is deliberately not copied.
     */
    result = ki_memory_write_u8(memory, destination_address, first_byte);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* Keep the original LW/SW ordering so partial effects remain auditable. */
    result = ki_memory_read_u32_le(memory, source_address + 4, &field);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, destination_address + 4, field);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    result = ki_memory_read_u32_le(memory, source_address + 8, &field);
    if (result != KI_MEMORY_OK) {
        return result;
    }
    result = ki_memory_write_u32_le(memory, destination_address + 8, field);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    result = ki_memory_read_u32_le(memory, source_address + 12, &field);
    if (result != KI_MEMORY_OK) {
        return result;
    }

    /* The R4600 performs this final store in the JR return delay slot. */
    return ki_memory_write_u32_le(memory, destination_address + 12, field);
}
