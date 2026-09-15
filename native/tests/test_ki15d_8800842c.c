#include "ki/original/ki15d_8800842c.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 256,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
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
                                          unsigned int fixture_case)
{
    for (size_t offset = 0; offset < MAIN_RAM_SIZE; offset++) {
        if (actual[offset] != expected[offset]) {
            fprintf(stderr,
                    "fixture case %u: RAM offset 0x%zx expected=%02x "
                    "actual=%02x\n",
                    fixture_case, offset, expected[offset], actual[offset]);
            return 0;
        }
    }
    return 1;
}

static int run_fixture_case(unsigned int fixture_case,
                            uint32_t record_address, uint8_t fill_byte,
                            uint32_t position_before,
                            uint32_t velocity_before, uint16_t acceleration,
                            uint16_t time_scale, uint32_t position_after,
                            uint32_t velocity_after, uint8_t *actual_ram,
                            uint8_t *expected_ram)
{
    const uint32_t physical = ki_physical_address(record_address);
    if (physical < MAIN_RAM_PHYSICAL_BASE ||
        physical + OBJECT_RECORD_SIZE >
            MAIN_RAM_PHYSICAL_BASE + MAIN_RAM_SIZE) {
        fprintf(stderr, "fixture case %u: target is outside test RAM\n",
                fixture_case);
        return 0;
    }
    const size_t record_offset = physical - MAIN_RAM_PHYSICAL_BASE;

    /* Full-RAM comparison makes the two original state writes an exact set. */
    memset(actual_ram, 0xa5, MAIN_RAM_SIZE);
    memset(expected_ram, 0xa5, MAIN_RAM_SIZE);
    memset(actual_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    memset(expected_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);

#define WRITE_BOTH_16(offset, value)                                           \
    do {                                                                        \
        write_u16_le(actual_ram + record_offset + (offset), (value));            \
        write_u16_le(expected_ram + record_offset + (offset), (value));          \
    } while (0)
#define WRITE_BOTH_32(offset, value)                                           \
    do {                                                                        \
        write_u32_le(actual_ram + record_offset + (offset), (value));            \
        write_u32_le(expected_ram + record_offset + (offset), (value));          \
    } while (0)

    WRITE_BOTH_32(0x0c, position_before);
    WRITE_BOTH_32(0x10, velocity_before);
    WRITE_BOTH_16(0x3c, acceleration);
    WRITE_BOTH_16(0x40, time_scale);

#undef WRITE_BOTH_16
#undef WRITE_BOTH_32

    write_u32_le(expected_ram + record_offset + 0x0c, position_after);
    write_u32_le(expected_ram + record_offset + 0x10, velocity_after);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    const KiMemoryResult result =
        ki15d_8800842c(&memory, record_address);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture case %u: memory error %d\n", fixture_case,
                (int)result);
        return 0;
    }
    return report_first_memory_difference(actual_ram, expected_ram,
                                          fixture_case);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_8800842c.csv";
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
            strncmp(line, "case,", 5) == 0) {
            continue;
        }

        unsigned int fixture_case = 0;
        uint32_t record_address = 0;
        unsigned int fill_byte = 0;
        uint32_t position_before = 0;
        uint32_t velocity_before = 0;
        unsigned int acceleration = 0;
        unsigned int time_scale = 0;
        uint32_t position_after = 0;
        uint32_t velocity_after = 0;
        const int parsed = sscanf(
            line,
            "%u,%" SCNx32 ",%x,%" SCNx32 ",%" SCNx32
            ",%x,%x,%" SCNx32 ",%" SCNx32,
            &fixture_case, &record_address, &fill_byte, &position_before,
            &velocity_before, &acceleration, &time_scale, &position_after,
            &velocity_after);
        if (parsed != 9 || fill_byte > UINT8_MAX ||
            acceleration > UINT16_MAX || time_scale > UINT16_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }

        if (!run_fixture_case(
                fixture_case, record_address, (uint8_t)fill_byte,
                position_before, velocity_before, (uint16_t)acceleration,
                (uint16_t)time_scale, position_after, velocity_after,
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
    printf("native routine 0x8800842c: %u MAME gravity cases matched\n",
           cases_checked);
    return 0;
}
