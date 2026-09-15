#include "ki/original/ki15d_880063ac.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 192,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    ANIMATION_TABLE_OFFSET = 0x0005e310,
    OBJECT_RECORD_SIZE = 0x100
};

static void write_u16_le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static int report_first_memory_difference(const uint8_t *actual,
                                          const uint8_t *expected,
                                          unsigned int line_number)
{
    for (size_t offset = 0; offset < MAIN_RAM_SIZE; offset++) {
        if (actual[offset] != expected[offset]) {
            fprintf(stderr,
                    "fixture line %u: RAM offset 0x%zx expected=%02x actual=%02x\n",
                    line_number, offset, expected[offset], actual[offset]);
            return 0;
        }
    }
    return 1;
}

static int run_fixture_case(unsigned int table_index, uint32_t record_address,
                            uint8_t fill_byte, uint32_t script_pointer,
                            uint8_t entry_byte4, uint8_t entry_byte5,
                            uint8_t entry_byte6, uint8_t entry_byte7,
                            uint32_t guard_before, uint32_t guard_after,
                            unsigned int line_number, uint8_t *actual_ram,
                            uint8_t *expected_ram)
{
    const uint32_t physical = ki_physical_address(record_address);
    if (physical < MAIN_RAM_PHYSICAL_BASE + 4 ||
        physical + OBJECT_RECORD_SIZE + 4 >
            MAIN_RAM_PHYSICAL_BASE + MAIN_RAM_SIZE) {
        fprintf(stderr, "fixture line %u: target is outside test RAM\n",
                line_number);
        return 0;
    }
    const size_t record_offset = physical - MAIN_RAM_PHYSICAL_BASE;
    const size_t entry_offset = ANIMATION_TABLE_OFFSET + table_index * 8;
    if (entry_offset + 8 > MAIN_RAM_SIZE) {
        fprintf(stderr, "fixture line %u: table entry is outside test RAM\n",
                line_number);
        return 0;
    }

    memset(actual_ram, 0xa5, MAIN_RAM_SIZE);
    memset(expected_ram, 0xa5, MAIN_RAM_SIZE);
    memset(actual_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    memset(expected_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);

    /* Recreate the exact 8-byte table row used by the MAME oracle. */
    write_u32_le(actual_ram + entry_offset, script_pointer);
    write_u32_le(expected_ram + entry_offset, script_pointer);
    actual_ram[entry_offset + 4] = entry_byte4;
    expected_ram[entry_offset + 4] = entry_byte4;
    actual_ram[entry_offset + 5] = entry_byte5;
    expected_ram[entry_offset + 5] = entry_byte5;
    actual_ram[entry_offset + 6] = entry_byte6;
    expected_ram[entry_offset + 6] = entry_byte6;
    actual_ram[entry_offset + 7] = entry_byte7;
    expected_ram[entry_offset + 7] = entry_byte7;

    write_u32_le(actual_ram + record_offset - 4, guard_before);
    write_u32_le(expected_ram + record_offset - 4, guard_before);
    write_u32_le(actual_ram + record_offset + OBJECT_RECORD_SIZE, guard_after);
    write_u32_le(expected_ram + record_offset + OBJECT_RECORD_SIZE, guard_after);

    /* Build the complete post-call object image while preserving other bytes. */
    write_u16_le(expected_ram + record_offset + 0x8a, entry_byte7);
    write_u32_le(expected_ram + record_offset + 0x20, script_pointer);
    expected_ram[record_offset + 0x1c] = entry_byte4;
    write_u32_le(expected_ram + record_offset + 0x34, entry_byte4);
    write_u32_le(expected_ram + record_offset + 0x18, 0);
    write_u32_le(expected_ram + record_offset + 0x14, 0);
    write_u16_le(expected_ram + record_offset + 0x2c, entry_byte6);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    const KiMemoryResult result =
        ki15d_880063ac(&memory, table_index, record_address);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture line %u: memory error %d\n", line_number,
                (int)result);
        return 0;
    }

    return report_first_memory_difference(actual_ram, expected_ram, line_number);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_880063ac.csv";
    FILE *fixture = fopen(fixture_path, "r");
    if (fixture == NULL) {
        perror(fixture_path);
        return 1;
    }

    uint8_t *actual_ram = malloc(MAIN_RAM_SIZE);
    uint8_t *expected_ram = malloc(MAIN_RAM_SIZE);
    if (actual_ram == NULL || expected_ram == NULL) {
        fprintf(stderr, "could not allocate native test RAM\n");
        free(actual_ram);
        free(expected_ram);
        fclose(fixture);
        return 1;
    }

    char line[LINE_CAPACITY];
    unsigned int line_number = 0;
    unsigned int cases_checked = 0;
    int success = 1;
    while (fgets(line, sizeof(line), fixture) != NULL) {
        line_number++;
        if (line[0] == '#' || line[0] == '\n' ||
            strncmp(line, "table_index,", 12) == 0) {
            continue;
        }

        unsigned int table_index = 0;
        uint32_t record_address = 0;
        unsigned int fill_byte = 0;
        uint32_t script_pointer = 0;
        unsigned int byte4 = 0;
        unsigned int byte5 = 0;
        unsigned int byte6 = 0;
        unsigned int byte7 = 0;
        uint32_t guard_before = 0;
        uint32_t guard_after = 0;
        if (sscanf(line,
                   "%u,%" SCNx32 ",%x,%" SCNx32
                   ",%x,%x,%x,%x,%" SCNx32 ",%" SCNx32,
                   &table_index, &record_address, &fill_byte, &script_pointer,
                   &byte4, &byte5, &byte6, &byte7, &guard_before,
                   &guard_after) != 10 ||
            fill_byte > UINT8_MAX || byte4 > UINT8_MAX ||
            byte5 > UINT8_MAX || byte6 > UINT8_MAX || byte7 > UINT8_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }
        if (!run_fixture_case(table_index, record_address, (uint8_t)fill_byte,
                              script_pointer, (uint8_t)byte4, (uint8_t)byte5,
                              (uint8_t)byte6, (uint8_t)byte7, guard_before,
                              guard_after, line_number, actual_ram,
                              expected_ram)) {
            success = 0;
            break;
        }
        cases_checked++;
    }

    if (ferror(fixture)) {
        perror(fixture_path);
        success = 0;
    }
    if (cases_checked == 0) {
        fprintf(stderr, "%s: no fixture cases found\n", fixture_path);
        success = 0;
    }

    free(actual_ram);
    free(expected_ram);
    fclose(fixture);

    if (!success) {
        return 1;
    }
    printf("native routine 0x880063ac: %u MAME animation cases matched\n",
           cases_checked);
    return 0;
}
