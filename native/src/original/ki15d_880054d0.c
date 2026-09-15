#include "ki/original/ki15d_880054d0.h"

#include <stddef.h>
#include <stdint.h>

enum { OBJECT_RECORD_SIZE = 0x100 };

KiMemoryResult ki15d_880054d0(KiMemory *memory, uint64_t record_address,
                              uint64_t *next_address)
{
    /*
     * The R4600 increments $a2 first, then stores zero to -4($a2) in the
     * branch delay slot. Iterating upward from offset zero has exactly the
     * same externally visible write order.
     */
    for (size_t offset = 0; offset < OBJECT_RECORD_SIZE; offset += 4) {
        const KiMemoryResult result =
            ki_memory_write_u32_le(memory, record_address + offset, 0);
        if (result != KI_MEMORY_OK) {
            return result;
        }
    }

    /* The original leaves both $a2 and $a3 at $fp + 0x100. */
    *next_address = record_address + OBJECT_RECORD_SIZE;
    return KI_MEMORY_OK;
}
