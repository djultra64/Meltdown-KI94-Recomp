#include "ki/original/ki15d_8800700c.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 512,
    MAIN_RAM_SIZE = 1024 * 1024,
    SOURCE_OFFSET = 0x00090000,
    RECORD_SIZE = 16
};

static const uint64_t SOURCE_ADDRESS = UINT64_C(0xffffffff88090000);
static const uint64_t DESTINATION_ADDRESS = UINT64_C(0xffffffff88090100);

typedef struct FixtureCase {
    uint8_t first_byte;
    uint32_t source[4];
    uint32_t destination_before[4];
    uint32_t destination_after[4];
} FixtureCase;

static int parse_fixture_case(const char *line, FixtureCase *test_case)
{
    uint32_t first_byte = 0;
    const int fields = sscanf(
        line,
        "%" SCNx32 ",%" SCNx32 ",%" SCNx32 ",%" SCNx32 ",%" SCNx32
        ",%" SCNx32 ",%" SCNx32 ",%" SCNx32 ",%" SCNx32 ",%" SCNx32
        ",%" SCNx32 ",%" SCNx32 ",%" SCNx32,
        &first_byte, &test_case->source[0], &test_case->source[1],
        &test_case->source[2], &test_case->source[3],
        &test_case->destination_before[0], &test_case->destination_before[1],
        &test_case->destination_before[2], &test_case->destination_before[3],
        &test_case->destination_after[0], &test_case->destination_after[1],
        &test_case->destination_after[2], &test_case->destination_after[3]);

    if (fields != 13 || first_byte > UINT8_MAX) {
        return 0;
    }
    test_case->first_byte = (uint8_t)first_byte;
    return 1;
}

static int write_record(KiMemory *memory, uint64_t address,
                        const uint32_t words[4])
{
    for (size_t index = 0; index < 4; index++) {
        const KiMemoryResult result =
            ki_memory_write_u32_le(memory, address + index * 4, words[index]);
        if (result != KI_MEMORY_OK) {
            return 0;
        }
    }
    return 1;
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

static int run_fixture_case(const FixtureCase *test_case,
                            unsigned int line_number, uint8_t *actual_ram,
                            uint8_t *expected_ram)
{
    /* A non-zero background exposes unintended writes more clearly than zero. */
    memset(actual_ram, 0x5a, MAIN_RAM_SIZE);
    memset(expected_ram, 0x5a, MAIN_RAM_SIZE);

    KiMemory actual_memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    KiMemory expected_memory = actual_memory;
    expected_memory.main_ram = expected_ram;

    if (!write_record(&actual_memory, SOURCE_ADDRESS, test_case->source) ||
        !write_record(&actual_memory, DESTINATION_ADDRESS,
                      test_case->destination_before) ||
        !write_record(&expected_memory, SOURCE_ADDRESS, test_case->source) ||
        !write_record(&expected_memory, DESTINATION_ADDRESS,
                      test_case->destination_after)) {
        fprintf(stderr, "fixture line %u: test address is not mapped\n",
                line_number);
        return 0;
    }

    uint8_t source_before[RECORD_SIZE];
    memcpy(source_before, actual_ram + SOURCE_OFFSET, sizeof(source_before));

    const KiMemoryResult result =
        ki15d_8800700c(&actual_memory, DESTINATION_ADDRESS, SOURCE_ADDRESS,
                       test_case->first_byte);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture line %u: routine returned memory error %d\n",
                line_number, (int)result);
        return 0;
    }

    if (memcmp(source_before, actual_ram + SOURCE_OFFSET,
               sizeof(source_before)) != 0) {
        fprintf(stderr, "fixture line %u: source record was modified\n",
                line_number);
        return 0;
    }

    /*
     * Comparing all mapped RAM proves both the expected destination values and
     * the absence of any write outside the routine's 13 output bytes.
     */
    return report_first_memory_difference(actual_ram, expected_ram, line_number);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_8800700c.csv";
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
            strncmp(line, "first_byte,", 11) == 0) {
            continue;
        }

        FixtureCase test_case;
        if (!parse_fixture_case(line, &test_case)) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }
        if (!run_fixture_case(&test_case, line_number, actual_ram,
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
    printf("native routine 0x8800700c: %u MAME memory cases matched\n",
           cases_checked);
    return 0;
}
