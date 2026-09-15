#include "ki/original/ki15d_8800b1fc.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    LINE_CAPACITY = 640,
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    OBJECT_POOL_OFFSET = 0x0008be00,
    OBJECT_POOL_SIZE = 0x1e00,
    OBJECT_RECORD_SIZE = 0x100,
    SAVED_RETURN_OFFSET = 0x00087274,
    ANIMATION_INDEX_OFFSET = 0x0000b2d8,
    ANIMATION_ENTRY_OFFSET = 0x0005e370
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

static void initialize_original_data(uint8_t *ram)
{
    /* 0x8800b2d8 selects table index 12 for this particle constructor. */
    ram[ANIMATION_INDEX_OFFSET] = UINT8_C(0x0c);

    /* Original eight-byte animation-table entry at 0x8805e370. */
    write_u32_le(ram + ANIMATION_ENTRY_OFFSET, UINT32_C(0x8805e6f2));
    ram[ANIMATION_ENTRY_OFFSET + 4] = UINT8_C(0x0a);
    ram[ANIMATION_ENTRY_OFFSET + 5] = UINT8_C(0x0a);
    ram[ANIMATION_ENTRY_OFFSET + 6] = UINT8_C(0x04);
    ram[ANIMATION_ENTRY_OFFSET + 7] = UINT8_C(0x00);
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
    unsigned int fixture_case, uint32_t fighter_address,
    uint8_t fighter_fill, uint32_t gp_value, uint32_t return_address,
    uint8_t first_record_fill, uint32_t position_x, uint32_t position_y,
    uint32_t position_z, uint32_t direction_x, uint32_t direction_y,
    uint8_t countdown, uint8_t height, uint8_t variant,
    uint32_t expected_selected, uint8_t expected_variant,
    uint32_t expected_position_z, uint16_t expected_direction_x,
    uint16_t expected_direction_y, uint16_t expected_one_shot,
    uint16_t expected_persistent, uint16_t expected_scale,
    uint8_t *actual_ram, uint8_t *expected_ram)
{
    const uint32_t fighter_physical = ki_physical_address(fighter_address);
    const uint32_t selected_physical = ki_physical_address(expected_selected);
    if (fighter_physical < MAIN_RAM_PHYSICAL_BASE ||
        fighter_physical + OBJECT_RECORD_SIZE >
            MAIN_RAM_PHYSICAL_BASE + MAIN_RAM_SIZE ||
        selected_physical < MAIN_RAM_PHYSICAL_BASE ||
        selected_physical + OBJECT_RECORD_SIZE >
            MAIN_RAM_PHYSICAL_BASE + MAIN_RAM_SIZE) {
        fprintf(stderr, "fixture case %u: address is outside test RAM\n",
                fixture_case);
        return 0;
    }
    const size_t fighter_offset = fighter_physical - MAIN_RAM_PHYSICAL_BASE;
    const size_t selected_offset = selected_physical - MAIN_RAM_PHYSICAL_BASE;

    memset(actual_ram, 0xa5, MAIN_RAM_SIZE);
    memset(expected_ram, 0xa5, MAIN_RAM_SIZE);
    memset(actual_ram + OBJECT_POOL_OFFSET, 0, OBJECT_POOL_SIZE);
    memset(expected_ram + OBJECT_POOL_OFFSET, 0, OBJECT_POOL_SIZE);
    if (first_record_fill != 0) {
        memset(actual_ram + OBJECT_POOL_OFFSET, first_record_fill,
               OBJECT_RECORD_SIZE);
        memset(expected_ram + OBJECT_POOL_OFFSET, first_record_fill,
               OBJECT_RECORD_SIZE);
    }

    memset(actual_ram + fighter_offset, fighter_fill, OBJECT_RECORD_SIZE);
    memset(expected_ram + fighter_offset, fighter_fill, OBJECT_RECORD_SIZE);

#define WRITE_FIGHTER_BOTH_8(offset, value)                                    \
    do {                                                                        \
        actual_ram[fighter_offset + (offset)] = (value);                         \
        expected_ram[fighter_offset + (offset)] = (value);                       \
    } while (0)
#define WRITE_FIGHTER_BOTH_32(offset, value)                                   \
    do {                                                                        \
        write_u32_le(actual_ram + fighter_offset + (offset), (value));           \
        write_u32_le(expected_ram + fighter_offset + (offset), (value));         \
    } while (0)

    WRITE_FIGHTER_BOTH_32(0x04, position_x);
    WRITE_FIGHTER_BOTH_32(0x08, position_y);
    WRITE_FIGHTER_BOTH_32(0x0c, position_z);
    WRITE_FIGHTER_BOTH_32(0x74, direction_x);
    WRITE_FIGHTER_BOTH_32(0x78, direction_y);
    WRITE_FIGHTER_BOTH_8(0xc4, countdown);
    WRITE_FIGHTER_BOTH_8(0xc6, height);
    WRITE_FIGHTER_BOTH_8(0xc7, variant);

#undef WRITE_FIGHTER_BOTH_8
#undef WRITE_FIGHTER_BOTH_32

    initialize_original_data(actual_ram);
    initialize_original_data(expected_ram);
    const uint32_t initial_saved_return =
        UINT32_C(0xd0c00000) | fixture_case;
    write_u32_le(actual_ram + SAVED_RETURN_OFFSET, initial_saved_return);
    write_u32_le(expected_ram + SAVED_RETURN_OFFSET, return_address);

    /*
     * The allocator clears the selected record. Build the exact MAME result by
     * starting at zero and applying only writes observed in the constructor and
     * its verified animation-setup dependency.
     */
    memset(expected_ram + selected_offset, 0, OBJECT_RECORD_SIZE);
    expected_ram[selected_offset + 0x00] = UINT8_C(0x1a);
    write_u32_le(expected_ram + selected_offset + 0x04, position_x);
    write_u32_le(expected_ram + selected_offset + 0x08, position_y);
    write_u32_le(expected_ram + selected_offset + 0x0c,
                 expected_position_z);
    write_u32_le(expected_ram + selected_offset + 0x10, UINT32_C(0x000000a0));
    write_u32_le(expected_ram + selected_offset + 0x14, 0);
    write_u32_le(expected_ram + selected_offset + 0x18, 0);
    expected_ram[selected_offset + 0x1c] = UINT8_C(0x0a);
    write_u32_le(expected_ram + selected_offset + 0x20,
                 UINT32_C(0x8805e6f2));
    expected_ram[selected_offset + 0x24] = UINT8_C(0x02);
    write_u16_le(expected_ram + selected_offset + 0x2c, UINT16_C(0x0004));
    write_u32_le(expected_ram + selected_offset + 0x34, UINT32_C(0x0000000a));
    write_u32_le(expected_ram + selected_offset + 0x3c, UINT32_C(0xfffffff6));
    write_u32_le(expected_ram + selected_offset + 0x40, UINT32_C(0x01000100));
    write_u32_le(expected_ram + selected_offset + 0x4c, UINT32_C(0x01000100));
    write_u16_le(expected_ram + selected_offset + 0x58, expected_scale);
    write_u16_le(expected_ram + selected_offset + 0x5a, expected_scale);
    write_u16_le(expected_ram + selected_offset + 0x7c,
                 expected_direction_x);
    write_u16_le(expected_ram + selected_offset + 0x7e, expected_one_shot);
    write_u16_le(expected_ram + selected_offset + 0x80,
                 expected_direction_y);
    write_u16_le(expected_ram + selected_offset + 0x84,
                 expected_persistent);
    write_u16_le(expected_ram + selected_offset + 0x8a, 0);
    expected_ram[selected_offset + 0x8e] = expected_variant;
    expected_ram[selected_offset + 0x94] = UINT8_C(0x60);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = actual_ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    uint64_t actual_selected = 0;
    const KiMemoryResult result =
        ki15d_8800b1fc(&memory, fighter_address, gp_value, return_address,
                       &actual_selected);
    if (result != KI_MEMORY_OK) {
        fprintf(stderr, "fixture case %u: memory error %d\n", fixture_case,
                (int)result);
        return 0;
    }
    if ((uint32_t)actual_selected != expected_selected) {
        fprintf(stderr,
                "fixture case %u: expected record %08" PRIx32
                ", got %08" PRIx32 "\n",
                fixture_case, expected_selected, (uint32_t)actual_selected);
        return 0;
    }
    return report_first_memory_difference(actual_ram, expected_ram,
                                          fixture_case);
}

int main(void)
{
    const char *fixture_path = "tests/fixtures/ki15d_8800b1fc.csv";
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
        uint32_t fighter_address = 0;
        unsigned int fighter_fill = 0;
        uint32_t gp_value = 0;
        uint32_t return_address = 0;
        unsigned int first_record_fill = 0;
        uint32_t position_x = 0;
        uint32_t position_y = 0;
        uint32_t position_z = 0;
        uint32_t direction_x = 0;
        uint32_t direction_y = 0;
        unsigned int countdown = 0;
        unsigned int height = 0;
        unsigned int variant = 0;
        uint32_t expected_selected = 0;
        unsigned int expected_variant = 0;
        uint32_t expected_position_z = 0;
        unsigned int expected_direction_x = 0;
        unsigned int expected_direction_y = 0;
        unsigned int expected_one_shot = 0;
        unsigned int expected_persistent = 0;
        unsigned int expected_scale = 0;

        const int parsed = sscanf(
            line,
            "%u,%" SCNx32 ",%x,%" SCNx32 ",%" SCNx32
            ",%x,%" SCNx32 ",%" SCNx32 ",%" SCNx32 ",%" SCNx32
            ",%" SCNx32 ",%x,%x,%x,%" SCNx32 ",%x,%" SCNx32
            ",%x,%x,%x,%x,%x",
            &fixture_case, &fighter_address, &fighter_fill, &gp_value,
            &return_address, &first_record_fill, &position_x, &position_y,
            &position_z, &direction_x, &direction_y, &countdown, &height,
            &variant, &expected_selected, &expected_variant,
            &expected_position_z, &expected_direction_x,
            &expected_direction_y, &expected_one_shot, &expected_persistent,
            &expected_scale);
        if (parsed != 22 || fighter_fill > UINT8_MAX ||
            first_record_fill > UINT8_MAX || countdown > UINT8_MAX ||
            height > UINT8_MAX || variant > UINT8_MAX ||
            expected_variant > UINT8_MAX ||
            expected_direction_x > UINT16_MAX ||
            expected_direction_y > UINT16_MAX ||
            expected_one_shot > UINT16_MAX ||
            expected_persistent > UINT16_MAX || expected_scale > UINT16_MAX) {
            fprintf(stderr, "%s:%u: invalid fixture row\n", fixture_path,
                    line_number);
            success = 0;
            break;
        }

        if (!run_fixture_case(
                fixture_case, fighter_address, (uint8_t)fighter_fill,
                gp_value, return_address, (uint8_t)first_record_fill,
                position_x, position_y, position_z, direction_x, direction_y,
                (uint8_t)countdown, (uint8_t)height, (uint8_t)variant,
                expected_selected, (uint8_t)expected_variant,
                expected_position_z, (uint16_t)expected_direction_x,
                (uint16_t)expected_direction_y, (uint16_t)expected_one_shot,
                (uint16_t)expected_persistent, (uint16_t)expected_scale,
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
    printf("native routine 0x8800b1fc: %u MAME particle cases matched\n",
           cases_checked);
    return 0;
}
