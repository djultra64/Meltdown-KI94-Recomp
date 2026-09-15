#include "ki/original/ki15d_88004180.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 512,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    GLOBAL_DELTA_X_OFFSET = 0x00087b20,
    GLOBAL_DELTA_Y_OFFSET = 0x00087b24,
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

static int run_fixture_case(
    unsigned int fixture_case, uint32_t record_address, uint8_t fill_byte,
    uint32_t position_x_before, uint32_t position_y_before, uint16_t axis_x,
    uint16_t one_shot, uint16_t axis_y, uint16_t persistent,
    uint16_t decay, uint32_t global_x_before, uint32_t global_y_before,
    uint32_t position_x_after, uint32_t position_y_after,
    uint16_t one_shot_after, uint16_t persistent_after,
    uint32_t global_x_after, uint32_t global_y_after, uint8_t *actual_ram,
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

    /*
     * Start both images identically, then modify only the documented expected
     * outputs. A whole-RAM comparison detects accidental writes to any other
     * object field or global.
     */
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

    WRITE_BOTH_32(0x04, position_x_before);
    WRITE_BOTH_32(0x08, position_y_before);
    WRITE_BOTH_16(0x7c, axis_x);
    WRITE_BOTH_16(0x7e, one_shot);
    WRITE_BOTH_16(0x80, axis_y);
    WRITE_BOTH_16(0x84, persistent);
    WRITE_BOTH_16(0x86, decay);

#undef WRITE_BOTH_16
#undef WRITE_BOTH_32

    write_u32_le(actual_ram + GLOBAL_DELTA_X_OFFSET, global_x_before);
    write_u32_le(expected_ram + GLOBAL_DELTA_X_OFFSET, global_x_before);
    write_u32_le(actual_ram + GLOBAL_DELTA_Y_OFFSET, global_y_before);
    write_u32_le(expected_ram + GLOBAL_DELTA_Y_OFFSET, global_y_before);

    write_u32_le(expected_ram + record_offset + 0x04, position_x_after);
    write_u32_le(expected_ram + record_offset + 0x08, position_y_after);
    write_u16_le(expected_ram + record_offset + 0x7e, one_shot_after);
    write_u16_le(expected_ram + record_offset + 0x84, persistent_after);
    write_u32_le(expected_ram + GLOBAL_DELTA_X_OFFSET, global_x_after);
    write_u32_le(expected_ram + GLOBAL_DELTA_Y_OFFSET, global_y_after);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    const KiMemoryResult result =
        ki15d_88004180(&memory, record_address);
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
    const char *fixture_path = "tests/fixtures/ki15d_88004180.csv";
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
        uint32_t position_x_before = 0;
        uint32_t position_y_before = 0;
        unsigned int axis_x = 0;
        unsigned int one_shot = 0;
        unsigned int axis_y = 0;
        unsigned int persistent = 0;
        unsigned int decay = 0;
        uint32_t global_x_before = 0;
        uint32_t global_y_before = 0;
        uint32_t position_x_after = 0;
        uint32_t position_y_after = 0;
        unsigned int one_shot_after = 0;
        unsigned int persistent_after = 0;
        uint32_t global_x_after = 0;
        uint32_t global_y_after = 0;
        const int parsed = sscanf(
            line,
            "%u,%" SCNx32 ",%x,%" SCNx32 ",%" SCNx32
            ",%x,%x,%x,%x,%x,%" SCNx32 ",%" SCNx32 ",%" SCNx32
            ",%" SCNx32 ",%x,%x,%" SCNx32 ",%" SCNx32,
            &fixture_case, &record_address, &fill_byte, &position_x_before,
            &position_y_before, &axis_x, &one_shot, &axis_y, &persistent,
            &decay, &global_x_before, &global_y_before, &position_x_after,
            &position_y_after, &one_shot_after, &persistent_after,
            &global_x_after, &global_y_after);
        if (parsed != 18 || fill_byte > UINT8_MAX || axis_x > UINT16_MAX ||
            one_shot > UINT16_MAX || axis_y > UINT16_MAX ||
            persistent > UINT16_MAX || decay > UINT16_MAX ||
            one_shot_after > UINT16_MAX || persistent_after > UINT16_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }

        if (!run_fixture_case(
                fixture_case, record_address, (uint8_t)fill_byte,
                position_x_before, position_y_before, (uint16_t)axis_x,
                (uint16_t)one_shot, (uint16_t)axis_y, (uint16_t)persistent,
                (uint16_t)decay, global_x_before, global_y_before,
                position_x_after, position_y_after, (uint16_t)one_shot_after,
                (uint16_t)persistent_after, global_x_after, global_y_after,
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
    printf("native routine 0x88004180: %u MAME motion cases matched\n",
           cases_checked);
    return 0;
}
