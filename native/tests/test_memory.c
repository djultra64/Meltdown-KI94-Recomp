#include "ki/memory.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint8_t low_ram[32] = {0};
    uint8_t main_ram[32] = {0};
    const uint8_t boot_rom[8] = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    KiMemory memory = {
        .low_ram = low_ram,
        .low_ram_size = sizeof(low_ram),
        .main_ram = main_ram,
        .main_ram_size = sizeof(main_ram),
        .boot_rom = boot_rom,
        .boot_rom_size = sizeof(boot_rom),
    };

    assert(ki_physical_address(0x0000000008000010ull) == 0x08000010u);
    assert(ki_physical_address(0xffffffff88000010ull) == 0x08000010u);
    assert(ki_physical_address(0xffffffffa8000010ull) == 0x08000010u);

    assert(ki_memory_write_u32_le(&memory, 0xffffffff88000004ull, 0x89abcdefu) ==
           KI_MEMORY_OK);
    assert(main_ram[4] == 0xef && main_ram[5] == 0xcd &&
           main_ram[6] == 0xab && main_ram[7] == 0x89);

    uint32_t word = 0;
    assert(ki_memory_read_u32_le(&memory, 0xffffffff88000004ull, &word) ==
           KI_MEMORY_OK);
    assert(word == 0x89abcdefu);

    assert(ki_memory_read_u32_le(&memory, 0xffffffffbfc00000ull, &word) ==
           KI_MEMORY_OK);
    assert(word == 0x12345678u);
    assert(ki_memory_write_u8(&memory, 0xffffffffbfc00000ull, 0) ==
           KI_MEMORY_READ_ONLY);
    assert(ki_memory_read_u32_le(&memory, 0x10000080u, &word) ==
           KI_MEMORY_UNMAPPED);

    puts("native memory tests: ok");
    return 0;
}

