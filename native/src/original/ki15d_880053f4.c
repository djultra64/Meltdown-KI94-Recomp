#include "ki/original/ki15d_880053f4.h"

#include <stddef.h>
#include <stdint.h>

enum {
    OBJECT_RECORD_SIZE = 0x100,
    SEARCHABLE_OBJECT_RECORDS = 0x1d
};

static const uint64_t OBJECT_POOL_ADDRESS = UINT64_C(0xffffffff8808be00);

KiMemoryResult ki15d_880053f4(KiMemory *memory, uint64_t *selected_address)
{
    uint64_t candidate = OBJECT_POOL_ADDRESS;

    /*
     * Mirror the branch-delay-slot advance in the original scan. If all 29
     * tested records are occupied, candidate advances once more to 0x8808db00.
     */
    for (unsigned int index = 0; index < SEARCHABLE_OBJECT_RECORDS; index++) {
        uint8_t marker = 0;
        const KiMemoryResult result =
            ki_memory_read_u8(memory, candidate, &marker);
        if (result != KI_MEMORY_OK) {
            return result;
        }
        if (marker == 0) {
            break;
        }
        candidate += OBJECT_RECORD_SIZE;
    }

    *selected_address = candidate;

    /* The R4600 loop clears words in descending address order. */
    for (size_t offset = OBJECT_RECORD_SIZE; offset != 0; offset -= 4) {
        const KiMemoryResult result =
            ki_memory_write_u32_le(memory, candidate + offset - 4, 0);
        if (result != KI_MEMORY_OK) {
            return result;
        }
    }

    return KI_MEMORY_OK;
}
