#include "ki/original/ki15d_880054d0.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 160,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    OBJECT_RECORD_SIZE = 0x100
};

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

static int run_fixture_case(uint32_t record_address, uint8_t fill_byte,
                            uint32_t guard_before, uint32_t guard_after,
                            uint32_t expected_next, unsigned int line_number,
                            uint8_t *actual_ram, uint8_t *expected_ram)
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

    /*
     * A complete RAM comparison catches writes beyond the selected record;
     * explicit guard words mirror the values recorded by the MAME oracle.
     */
    memset(actual_ram, 0xa5, MAIN_RAM_SIZE);
    memset(expected_ram, 0xa5, MAIN_RAM_SIZE);
    memset(actual_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    memset(expected_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    write_u32_le(actual_ram + record_offset - 4, guard_before);
    write_u32_le(expected_ram + record_offset - 4, guard_before);
    write_u32_le(actual_ram + record_offset + OBJECT_RECORD_SIZE, guard_after);
    write_u32_le(expected_ram + record_offset + OBJECT_RECORD_SIZE, guard_after);
    memset(expected_ram + record_offset, 0, OBJECT_RECORD_SIZE);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    uint64_t next_address = 0;
    const KiMemoryResult result =
        ki15d_880054d0(&memory, record_address, &next_address);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture line %u: memory error %d\n", line_number,
                (int)result);
        return 0;
    }
    if ((uint32_t)next_address != expected_next) {
        fprintf(stderr,
                "fixture line %u: expected next address %08" PRIx32
                ", got %08" PRIx32 "\n",
                line_number, expected_next, (uint32_t)next_address);
        return 0;
    }

    return report_first_memory_difference(actual_ram, expected_ram, line_number);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_880054d0.csv";
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
            strncmp(line, "record_address,", 15) == 0) {
            continue;
        }

        uint32_t record_address = 0;
        unsigned int fill_byte = 0;
        uint32_t guard_before = 0;
        uint32_t guard_after = 0;
        uint32_t expected_next = 0;
        if (sscanf(line, "%" SCNx32 ",%x,%" SCNx32 ",%" SCNx32
                         ",%" SCNx32,
                   &record_address, &fill_byte, &guard_before, &guard_after,
                   &expected_next) != 5 ||
            fill_byte > UINT8_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }
        if (!run_fixture_case(record_address, (uint8_t)fill_byte, guard_before,
                              guard_after, expected_next, line_number,
                              actual_ram, expected_ram)) {
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
    printf("native routine 0x880054d0: %u MAME clear cases matched\n",
           cases_checked);
    return 0;
}
