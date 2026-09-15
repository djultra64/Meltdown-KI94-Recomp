#ifndef KI_MEMORY_H
#define KI_MEMORY_H

#include <stddef.h>
#include <stdint.h>

typedef enum KiMemoryResult {
    KI_MEMORY_OK = 0,
    KI_MEMORY_UNMAPPED = 1,
    KI_MEMORY_READ_ONLY = 2
} KiMemoryResult;

/*
 * Host pointers backing the memory regions currently understood by Meltdown.
 * All public accessors accept original R4600 virtual addresses and translate
 * their direct-mapped KSEG aliases before selecting one of these regions.
 */
typedef struct KiMemory {
    uint8_t *low_ram;
    size_t low_ram_size;
    uint8_t *main_ram;
    size_t main_ram_size;
    const uint8_t *boot_rom;
    size_t boot_rom_size;
} KiMemory;

/* Convert 32-bit KSEG0/KSEG1 aliases to the arcade's physical address. */
uint32_t ki_physical_address(uint64_t virtual_address);

KiMemoryResult ki_memory_read_u8(const KiMemory *memory, uint64_t address,
                                 uint8_t *value);
KiMemoryResult ki_memory_read_u16_le(const KiMemory *memory, uint64_t address,
                                     uint16_t *value);
KiMemoryResult ki_memory_read_u32_le(const KiMemory *memory, uint64_t address,
                                     uint32_t *value);
KiMemoryResult ki_memory_write_u8(KiMemory *memory, uint64_t address,
                                  uint8_t value);
KiMemoryResult ki_memory_write_u16_le(KiMemory *memory, uint64_t address,
                                      uint16_t value);
KiMemoryResult ki_memory_write_u32_le(KiMemory *memory, uint64_t address,
                                      uint32_t value);

#endif
