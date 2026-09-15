#include "ki/original/ki15d_880053f4.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 128,
    MAIN_RAM_SIZE = 1024 * 1024,
    OBJECT_POOL_OFFSET = 0x0008be00,
    OBJECT_RECORD_SIZE = 0x100,
    SEARCHABLE_OBJECT_RECORDS = 0x1d
};

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

static int run_fixture_case(unsigned int occupied_records,
                            uint32_t expected_address,
                            unsigned int line_number, uint8_t *actual_ram,
                            uint8_t *expected_ram)
{
    /* MAME's oracle fills this whole area with 0x5a before each call. */
    memset(actual_ram, 0x5a, MAIN_RAM_SIZE);
    memset(expected_ram, 0x5a, MAIN_RAM_SIZE);

    if (occupied_records < SEARCHABLE_OBJECT_RECORDS) {
        const size_t free_record =
            OBJECT_POOL_OFFSET + occupied_records * OBJECT_RECORD_SIZE;
        actual_ram[free_record] = 0;
        expected_ram[free_record] = 0;
    }

    const size_t selected_offset =
        OBJECT_POOL_OFFSET + occupied_records * OBJECT_RECORD_SIZE;
    memset(expected_ram + selected_offset, 0, OBJECT_RECORD_SIZE);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    uint64_t selected_address = 0;
    const KiMemoryResult result =
        ki15d_880053f4(&memory, &selected_address);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture line %u: memory error %d\n", line_number,
                (int)result);
        return 0;
    }
    if ((uint32_t)selected_address != expected_address) {
        fprintf(stderr,
                "fixture line %u: expected address %08" PRIx32
                ", got %08" PRIx32 "\n",
                line_number, expected_address, (uint32_t)selected_address);
        return 0;
    }

    /* This proves both the complete clear and the absence of stray writes. */
    return report_first_memory_difference(actual_ram, expected_ram, line_number);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_880053f4.csv";
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
            strncmp(line, "occupied_records,", 17) == 0) {
            continue;
        }

        unsigned int occupied_records = 0;
        uint32_t expected_address = 0;
        if (sscanf(line, "%u,%" SCNx32, &occupied_records,
                   &expected_address) != 2 ||
            occupied_records > SEARCHABLE_OBJECT_RECORDS) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }
        if (!run_fixture_case(occupied_records, expected_address, line_number,
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
    printf("native routine 0x880053f4: %u MAME allocation cases matched\n",
           cases_checked);
    return 0;
}
