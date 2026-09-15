#include "ki/original/ki15d_88006670_type1a.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 384,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    OBJECT_RECORD_SIZE = 0x100,
    SAVED_RETURN_OFFSET = 0x0008727c,
    ORDER_BASE_OFFSET = 0x000872a0,
    ORDER_SCRATCH_OFFSET = 0x000872c0
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

static void initialize_stream_setup(uint8_t *ram, unsigned int setup)
{
    switch (setup) {
    case 1:
        /* Original 0x10 command followed by the first 0x04 frame. */
        ram[0x0005e6f2] = UINT8_C(0x00);
        ram[0x0005e6f3] = UINT8_C(0x10);
        ram[0x0005e6f4] = UINT8_C(0x01);
        ram[0x0005e6f5] = UINT8_C(0x00);
        ram[0x0005e6f6] = UINT8_C(0x04);
        ram[0x0005e6f7] = UINT8_C(0x02);
        memset(ram + ORDER_BASE_OFFSET, 0xc3, 0x40);
        ram[ORDER_BASE_OFFSET] = UINT8_C(0x01);
        break;
    case 2:
        /* Original next pair: token 0x05 for duration 2. */
        ram[0x0005e6f8] = UINT8_C(0x05);
        ram[0x0005e6f9] = UINT8_C(0x02);
        break;
    case 3:
        ram[0x00090000] = UINT8_C(0x33);
        ram[0x00090001] = UINT8_C(0x03);
        break;
    case 4:
        ram[0x00090020] = UINT8_C(0x44);
        ram[0x00090021] = UINT8_C(0x01);
        break;
    case 5:
        /* Original stream terminator. */
        ram[0x0005e71a] = UINT8_C(0x00);
        ram[0x0005e71b] = UINT8_C(0x14);
        break;
    case 6:
        /* Synthetic opcode 0x10 move followed by a normal frame pair. */
        ram[0x00090040] = UINT8_C(0x00);
        ram[0x00090041] = UINT8_C(0x10);
        ram[0x00090042] = UINT8_C(0x03);
        ram[0x00090043] = UINT8_C(0x00);
        ram[0x00090044] = UINT8_C(0x22);
        ram[0x00090045] = UINT8_C(0x02);
        for (unsigned int index = 0; index < 0x20; index++) {
            ram[ORDER_BASE_OFFSET + index] = (uint8_t)index;
        }
        memset(ram + ORDER_SCRATCH_OFFSET, 0xa5, 0x20);
        break;
    default:
        break;
    }
}

static void apply_expected_order_move(uint8_t *ram)
{
    /* gp=5 moves to one-based slot 3; entries 2..4 shift right. */
    const uint8_t first_eight[8] = {0, 1, 5, 2, 3, 4, 6, 7};
    memcpy(ram + ORDER_SCRATCH_OFFSET, first_eight, sizeof(first_eight));
    for (unsigned int index = 8; index < 0x20; index++) {
        ram[ORDER_SCRATCH_OFFSET + index] = (uint8_t)index;
    }
    memcpy(ram + ORDER_BASE_OFFSET, ram + ORDER_SCRATCH_OFFSET, 0x20);
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
    unsigned int fixture_case, unsigned int setup, uint32_t record_address,
    uint8_t fill_byte, uint32_t gp_value, uint32_t return_address,
    uint8_t token_before, uint32_t timer_before, uint32_t script_before,
    uint16_t time_scale, uint8_t token_after, uint32_t timer_after,
    uint32_t script_after, uint8_t *actual_ram, uint8_t *expected_ram)
{
    const uint32_t physical = ki_physical_address(record_address);
    if (physical < MAIN_RAM_PHYSICAL_BASE ||
        physical + OBJECT_RECORD_SIZE >
            MAIN_RAM_PHYSICAL_BASE + MAIN_RAM_SIZE) {
        fprintf(stderr, "fixture case %u: record is outside test RAM\n",
                fixture_case);
        return 0;
    }
    const size_t record_offset = physical - MAIN_RAM_PHYSICAL_BASE;

    memset(actual_ram, 0xa5, MAIN_RAM_SIZE);
    memset(expected_ram, 0xa5, MAIN_RAM_SIZE);
    memset(actual_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    memset(expected_ram + record_offset, fill_byte, OBJECT_RECORD_SIZE);
    initialize_stream_setup(actual_ram, setup);
    initialize_stream_setup(expected_ram, setup);

    write_u32_le(actual_ram + record_offset + 0x14, token_before);
    write_u32_le(expected_ram + record_offset + 0x14, token_before);
    write_u32_le(actual_ram + record_offset + 0x18, timer_before);
    write_u32_le(expected_ram + record_offset + 0x18, timer_before);
    write_u32_le(actual_ram + record_offset + 0x20, script_before);
    write_u32_le(expected_ram + record_offset + 0x20, script_before);
    write_u16_le(actual_ram + record_offset + 0x42, time_scale);
    write_u16_le(expected_ram + record_offset + 0x42, time_scale);

    const uint32_t initial_saved_return =
        UINT32_C(0xa1100000) | fixture_case;
    write_u32_le(actual_ram + SAVED_RETURN_OFFSET, initial_saved_return);
    write_u32_le(expected_ram + SAVED_RETURN_OFFSET, return_address);

    expected_ram[record_offset + 0x14] = token_after;
    write_u32_le(expected_ram + record_offset + 0x18, timer_after);
    write_u32_le(expected_ram + record_offset + 0x20, script_after);
    if (setup == 6) {
        apply_expected_order_move(expected_ram);
    }

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    const Ki15dAnimationResult result =
        ki15d_88006670_type1a_step(&memory, record_address, gp_value,
                                   return_address);
    if (result != KI15D_ANIMATION_OK) {
        fprintf(stderr, "fixture case %u: animation error %d\n", fixture_case,
                (int)result);
        return 0;
    }
    return report_first_memory_difference(actual_ram, expected_ram,
                                          fixture_case);
}

int main(void)
{
    const char *fixture_path =
        "tests/fixtures/ki15d_88006670_type1a.csv";
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
        unsigned int setup = 0;
        uint32_t record_address = 0;
        unsigned int fill_byte = 0;
        uint32_t gp_value = 0;
        uint32_t return_address = 0;
        unsigned int token_before = 0;
        uint32_t timer_before = 0;
        uint32_t script_before = 0;
        unsigned int time_scale = 0;
        unsigned int token_after = 0;
        uint32_t timer_after = 0;
        uint32_t script_after = 0;
        const int parsed = sscanf(
            line,
            "%u,%u,%" SCNx32 ",%x,%" SCNx32 ",%" SCNx32
            ",%x,%" SCNx32 ",%" SCNx32 ",%x,%x,%" SCNx32
            ",%" SCNx32,
            &fixture_case, &setup, &record_address, &fill_byte, &gp_value,
            &return_address, &token_before, &timer_before, &script_before,
            &time_scale, &token_after, &timer_after, &script_after);
        if (parsed != 13 || setup < 1 || setup > 6 ||
            fill_byte > UINT8_MAX || token_before > UINT8_MAX ||
            time_scale > UINT16_MAX || token_after > UINT8_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }

        if (!run_fixture_case(
                fixture_case, setup, record_address, (uint8_t)fill_byte,
                gp_value, return_address, (uint8_t)token_before, timer_before,
                script_before, (uint16_t)time_scale, (uint8_t)token_after,
                timer_after, script_after, actual_ram, expected_ram)) {
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
    printf("native type-0x1a animation path: %u MAME cases matched\n",
           cases_checked);
    return 0;
}
