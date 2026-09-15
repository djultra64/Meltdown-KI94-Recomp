#include "ki/original/ki15d_88004e54.h"
#include "ki/original/ki15d_8800b1fc.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAIN_RAM_SIZE = 1024 * 1024,
    MAIN_RAM_PHYSICAL_BASE = 0x08000000,
    FIGHTER_OFFSET = 0x0008bd00,
    FIRST_POOL_OFFSET = 0x0008be00,
    SECOND_POOL_OFFSET = 0x0008bf00,
    EXPECTED_PARTICLE_OFFSET = 0x0008c000,
    ANIMATION_INDEX_OFFSET = 0x0000b2d8,
    ANIMATION_ENTRY_OFFSET = 0x0005e370,
    ORDER_BASE_OFFSET = 0x000872a0,
    OBJECT_RECORD_SIZE = 0x100
};

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static uint16_t read_u16_le(const uint8_t *source)
{
    return (uint16_t)source[0] | ((uint16_t)source[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8) |
           ((uint32_t)source[2] << 16) | ((uint32_t)source[3] << 24);
}

static void install_type1a_original_data(uint8_t *ram)
{
    ram[ANIMATION_INDEX_OFFSET] = UINT8_C(0x0c);
    write_u32_le(ram + ANIMATION_ENTRY_OFFSET, UINT32_C(0x8805e6f2));
    ram[ANIMATION_ENTRY_OFFSET + 4] = UINT8_C(0x0a);
    ram[ANIMATION_ENTRY_OFFSET + 5] = UINT8_C(0x0a);
    ram[ANIMATION_ENTRY_OFFSET + 6] = UINT8_C(0x04);
    ram[ANIMATION_ENTRY_OFFSET + 7] = UINT8_C(0x00);

    /* Exact 42-byte stream at 0x8805e6f2, including setup and terminator. */
    static const uint8_t stream[] = {
        0x00, 0x10, 0x01, 0x00, 0x04, 0x02, 0x05, 0x02,
        0x06, 0x02, 0x07, 0x02, 0x08, 0x02, 0x09, 0x02,
        0x0a, 0x02, 0x0b, 0x02, 0x0c, 0x02, 0x0d, 0x02,
        0x0e, 0x02, 0x0f, 0x02, 0x10, 0x03, 0x11, 0x03,
        0x12, 0x03, 0x13, 0x04, 0x14, 0x04, 0x15, 0x04,
        0x00, 0x14,
    };
    memcpy(ram + 0x0005e6f2, stream, sizeof(stream));

    /* Opcode 0x10 exits early when the requested first slot already holds gp. */
    ram[ORDER_BASE_OFFSET] = UINT8_C(0x01);
}

static int expect_u32(const uint8_t *record, size_t offset, uint32_t expected,
                      const char *name)
{
    const uint32_t actual = read_u32_le(record + offset);
    if (actual != expected) {
        fprintf(stderr, "%s expected %08" PRIx32 ", got %08" PRIx32 "\n",
                name, expected, actual);
        return 0;
    }
    return 1;
}

static int expect_u16(const uint8_t *record, size_t offset, uint16_t expected,
                      const char *name)
{
    const uint16_t actual = read_u16_le(record + offset);
    if (actual != expected) {
        fprintf(stderr, "%s expected %04x, got %04x\n", name, expected,
                actual);
        return 0;
    }
    return 1;
}

int main(void)
{
    uint8_t *ram = calloc(MAIN_RAM_SIZE, 1);
    if (ram == NULL) {
        fprintf(stderr, "could not allocate native test RAM\n");
        return 1;
    }
    install_type1a_original_data(ram);

    /* Projectile and contact records occupy the first two live pool slots. */
    ram[FIRST_POOL_OFFSET] = UINT8_C(0x12);
    ram[SECOND_POOL_OFFSET] = UINT8_C(0x15);

    write_u32_le(ram + FIGHTER_OFFSET + 0x04, UINT32_C(0xffff9a00));
    write_u32_le(ram + FIGHTER_OFFSET + 0x08, UINT32_C(0x00000000));
    write_u32_le(ram + FIGHTER_OFFSET + 0x0c, UINT32_C(0x00000000));
    write_u32_le(ram + FIGHTER_OFFSET + 0x74, UINT32_C(0xffffff00));
    write_u32_le(ram + FIGHTER_OFFSET + 0x78, UINT32_C(0x00000000));
    ram[FIGHTER_OFFSET + 0xc4] = UINT8_C(0x30);
    ram[FIGHTER_OFFSET + 0xc6] = UINT8_C(0x55);
    ram[FIGHTER_OFFSET + 0xc7] = UINT8_C(0x00);

    KiMemory memory = {
        .low_ram = NULL,
        .low_ram_size = 0,
        .main_ram = ram,
        .main_ram_size = MAIN_RAM_SIZE,
        .boot_rom = NULL,
        .boot_rom_size = 0,
    };
    uint64_t particle_address = 0;
    KiMemoryResult constructor_result = ki15d_8800b1fc(
        &memory, UINT32_C(0x8808bd00), 1, UINT32_C(0x88003d6c),
        &particle_address);
    if (constructor_result != KI_MEMORY_OK ||
        (uint32_t)particle_address != UINT32_C(0x8808c000)) {
        fprintf(stderr, "native constructor did not create the live slot\n");
        free(ram);
        return 1;
    }

    uint8_t *record = ram + EXPECTED_PARTICLE_OFFSET;
    int success =
        record[0] == UINT8_C(0x1a) &&
        expect_u32(record, 0x04, UINT32_C(0xffff9a00), "ready position x") &&
        expect_u32(record, 0x0c, UINT32_C(0x00005500), "ready position z") &&
        expect_u32(record, 0x10, UINT32_C(0x000000a0), "ready velocity") &&
        expect_u32(record, 0x20, UINT32_C(0x8805e6f2), "ready script") &&
        expect_u16(record, 0x58, UINT16_C(0x1000), "ready x scale") &&
        expect_u16(record, 0x5a, UINT16_C(0x1000), "ready y scale");

    /* The live trace has 45 visible active updates before its release tick. */
    for (unsigned int tick = 0; success && tick < 45; tick++) {
        int active = 0;
        const Ki15dAnimationResult update_result =
            ki15d_88004e54_type1a_update(&memory, particle_address, 1,
                                         &active);
        if (update_result != KI15D_ANIMATION_OK || !active) {
            fprintf(stderr, "particle stopped early at tick %u (result %d)\n",
                    tick + 1, (int)update_result);
            success = 0;
        }
    }

    /* Checkpoint captured immediately before the original 46th update. */
    if (success) {
        success =
            expect_u32(record, 0x04, UINT32_C(0xffff5680),
                       "pre-release position x") &&
            expect_u32(record, 0x08, UINT32_C(0x00000000),
                       "pre-release position y") &&
            expect_u32(record, 0x0c, UINT32_C(0x0000998e),
                       "pre-release position z") &&
            expect_u32(record, 0x10, UINT32_C(0x00000262),
                       "pre-release velocity") &&
            record[0x14] == UINT8_C(0x15) &&
            expect_u32(record, 0x18, UINT32_C(0x00000000),
                       "pre-release timer") &&
            expect_u32(record, 0x20, UINT32_C(0x8805e71a),
                       "pre-release script") &&
            expect_u16(record, 0x58, UINT16_C(0x1000),
                       "pre-release x scale") &&
            expect_u16(record, 0x5a, UINT16_C(0x2680),
                       "pre-release y scale");
    }

    int active = 1;
    const Ki15dAnimationResult final_result =
        ki15d_88004e54_type1a_update(&memory, particle_address, 1, &active);
    if (final_result != KI15D_ANIMATION_OK || active) {
        fprintf(stderr, "particle did not release on tick 46 (result %d)\n",
                (int)final_result);
        success = 0;
    }
    for (size_t offset = 0; success && offset < OBJECT_RECORD_SIZE; offset++) {
        if (record[offset] != 0) {
            fprintf(stderr, "released record retained byte at 0x%zx\n", offset);
            success = 0;
        }
    }

    free(ram);
    if (!success) {
        return 1;
    }
    printf("native type-0x1a lifetime: 45 active ticks plus release matched\n");
    return 0;
}
