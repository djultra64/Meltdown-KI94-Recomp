#include "ki/memory.h"

#include <stdbool.h>

enum {
    /* Physical regions exposed by the current Killer Instinct memory map. */
    KI_LOW_RAM_BASE = 0x00000000u,
    KI_MAIN_RAM_BASE = 0x08000000u,
    KI_BOOT_ROM_BASE = 0x1fc00000u
};

static bool range_fits(uint32_t address, uint32_t base, size_t region_size,
                       size_t access_size, size_t *offset)
{
    if (address < base) {
        return false;
    }

    /* Subtract only after the lower-bound check and avoid addition overflow. */
    const uint64_t candidate = (uint64_t)address - base;
    if (candidate > region_size || access_size > region_size - (size_t)candidate) {
        return false;
    }

    *offset = (size_t)candidate;
    return true;
}

uint32_t ki_physical_address(uint64_t virtual_address)
{
    const uint32_t address = (uint32_t)virtual_address;

    /* KSEG0 and KSEG1 are cached/uncached aliases of the same physical range. */
    if (address >= 0x80000000u && address <= 0xbfffffffu) {
        return address & 0x1fffffffu;
    }
    return address;
}

static KiMemoryResult read_pointer(const KiMemory *memory, uint64_t address,
                                   size_t size, const uint8_t **pointer)
{
    const uint32_t physical = ki_physical_address(address);
    size_t offset = 0;

    /* Test regions in map order; range_fits also validates the full access. */
    if (range_fits(physical, KI_LOW_RAM_BASE, memory->low_ram_size, size, &offset)) {
        *pointer = memory->low_ram + offset;
        return KI_MEMORY_OK;
    }
    if (range_fits(physical, KI_MAIN_RAM_BASE, memory->main_ram_size, size, &offset)) {
        *pointer = memory->main_ram + offset;
        return KI_MEMORY_OK;
    }
    if (range_fits(physical, KI_BOOT_ROM_BASE, memory->boot_rom_size, size, &offset)) {
        *pointer = memory->boot_rom + offset;
        return KI_MEMORY_OK;
    }
    return KI_MEMORY_UNMAPPED;
}

static KiMemoryResult write_pointer(KiMemory *memory, uint64_t address,
                                    size_t size, uint8_t **pointer)
{
    const uint32_t physical = ki_physical_address(address);
    size_t offset = 0;

    if (range_fits(physical, KI_LOW_RAM_BASE, memory->low_ram_size, size, &offset)) {
        *pointer = memory->low_ram + offset;
        return KI_MEMORY_OK;
    }
    if (range_fits(physical, KI_MAIN_RAM_BASE, memory->main_ram_size, size, &offset)) {
        *pointer = memory->main_ram + offset;
        return KI_MEMORY_OK;
    }
    if (range_fits(physical, KI_BOOT_ROM_BASE, memory->boot_rom_size, size, &offset)) {
        /* A valid ROM address is distinct from an unmapped write. */
        return KI_MEMORY_READ_ONLY;
    }
    return KI_MEMORY_UNMAPPED;
}

KiMemoryResult ki_memory_read_u8(const KiMemory *memory, uint64_t address,
                                 uint8_t *value)
{
    const uint8_t *source = NULL;
    const KiMemoryResult result = read_pointer(memory, address, 1, &source);
    if (result == KI_MEMORY_OK) {
        *value = source[0];
    }
    return result;
}

KiMemoryResult ki_memory_read_u16_le(const KiMemory *memory, uint64_t address,
                                     uint16_t *value)
{
    const uint8_t *source = NULL;
    const KiMemoryResult result = read_pointer(memory, address, 2, &source);
    if (result == KI_MEMORY_OK) {
        *value = (uint16_t)source[0] | ((uint16_t)source[1] << 8);
    }
    return result;
}

KiMemoryResult ki_memory_read_u32_le(const KiMemory *memory, uint64_t address,
                                     uint32_t *value)
{
    const uint8_t *source = NULL;
    const KiMemoryResult result = read_pointer(memory, address, 4, &source);
    if (result == KI_MEMORY_OK) {
        *value = (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
                 ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
    }
    return result;
}

KiMemoryResult ki_memory_write_u8(KiMemory *memory, uint64_t address,
                                  uint8_t value)
{
    uint8_t *destination = NULL;
    const KiMemoryResult result = write_pointer(memory, address, 1, &destination);
    if (result == KI_MEMORY_OK) {
        destination[0] = value;
    }
    return result;
}

KiMemoryResult ki_memory_write_u16_le(KiMemory *memory, uint64_t address,
                                      uint16_t value)
{
    uint8_t *destination = NULL;
    const KiMemoryResult result = write_pointer(memory, address, 2, &destination);
    if (result == KI_MEMORY_OK) {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> 8);
    }
    return result;
}

KiMemoryResult ki_memory_write_u32_le(KiMemory *memory, uint64_t address,
                                      uint32_t value)
{
    uint8_t *destination = NULL;
    const KiMemoryResult result = write_pointer(memory, address, 4, &destination);
    if (result == KI_MEMORY_OK) {
        destination[0] = (uint8_t)value;
        destination[1] = (uint8_t)(value >> 8);
        destination[2] = (uint8_t)(value >> 16);
        destination[3] = (uint8_t)(value >> 24);
    }
    return result;
}
