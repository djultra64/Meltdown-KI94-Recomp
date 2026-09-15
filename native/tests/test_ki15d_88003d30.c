#include "ki/original/ki15d_88003d30.h"
#include "ki/original/ki15d_8800b1fc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RAM_SIZE = 1024 * 1024, FIGHTER = 0x91000, POOL = 0x8be00 };
static const uint64_t FIGHTER_ADDRESS = UINT64_C(0xffffffff88091000);

static void write32(uint8_t *ram, size_t offset, uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i) {
        ram[offset + i] = (uint8_t)(value >> (8 * i));
    }
}

static void initialize(uint8_t *ram)
{
    memset(ram, 0xa5, RAM_SIZE);
    memset(ram + POOL, 0, 0x1e00);
    memset(ram + FIGHTER, 0x5a, 0x100);
    write32(ram, FIGHTER + 0x04, 0xfffffff0);
    write32(ram, FIGHTER + 0x08, 0x12345678);
    write32(ram, FIGHTER + 0x0c, 0xffffff80);
    write32(ram, FIGHTER + 0x74, 0x00000100);
    write32(ram, FIGHTER + 0x78, 0xfffffe00);
    ram[FIGHTER + 0xc4] = 0x31;
    ram[FIGHTER + 0xc5] = 0x78;
    ram[FIGHTER + 0xc6] = 0x55;
    ram[FIGHTER + 0xc7] = 0xe5;
    /* Minimal scalar constructor dependencies, not an opaque RAM capture. */
    ram[0xb2d8] = 0x0c;
    write32(ram, 0x5e370, 0x8805e6f2);
    ram[0x5e374] = 0x0a;
    ram[0x5e375] = 0x0a;
    ram[0x5e376] = 0x04;
    ram[0x5e377] = 0;
}

static void compare_ram(const uint8_t *actual, const uint8_t *expected,
                        unsigned int gp, unsigned int step)
{
    for (size_t offset = 0; offset < RAM_SIZE; ++offset) {
        if (actual[offset] != expected[offset]) {
            fprintf(stderr, "gp=%u step=%u RAM+%zx: expected=%02x actual=%02x\n",
                    gp, step, offset, expected[offset], actual[offset]);
            exit(1);
        }
    }
}

static void check_sequence(uint8_t *actual, uint8_t *expected, uint32_t gp)
{
    initialize(actual);
    /* Exercise the allocator's skip path without using the organic schedule.
     * There is no lifetime update here: seven consecutive free slots remain. */
    memset(actual + POOL, 0x77, 0x100);
    memcpy(expected, actual, RAM_SIZE);
    KiMemory actual_memory = {.main_ram = actual, .main_ram_size = RAM_SIZE};
    KiMemory expected_memory = {.main_ram = expected, .main_ram_size = RAM_SIZE};
    unsigned int emitted = 0;
    for (unsigned int step = 0; step < 60; ++step) {
        uint64_t expected_particle = 0;
        if (step < 49) {
            expected[FIGHTER + 0xc4] = (uint8_t)(48 - step);
            expected[FIGHTER + 0xc5] = (uint8_t)(8 + (step % 8) * 16);
            if (step % 8 == 0) {
                /* Composition oracle: use the separately MAME-verified
                 * constructor on explicit post-delay-slot emitter state. */
                assert(ki15d_8800b1fc(&expected_memory, FIGHTER_ADDRESS, gp,
                         0x88003d6c, &expected_particle) == KI_MEMORY_OK);
                ++emitted;
                assert(expected_particle ==
                       UINT64_C(0xffffffff8808be00) + emitted * 0x100);
            }
        }
        uint64_t particle = UINT64_MAX;
        assert(ki15d_88003d30(&actual_memory, FIGHTER_ADDRESS, gp, &particle)
               == KI_MEMORY_OK);
        assert(particle == expected_particle);
        compare_ram(actual, expected, gp, step);
    }
    assert(emitted == 7);
    /* Explicit orientation expectation in addition to dependency composition. */
    const size_t first = POOL + 0x100;
    assert(actual[first + 0x7c] == 0);
    assert(actual[first + 0x7d] == (gp ? 0x01 : 0xff));
    assert(actual[first + 0x80] == 0);
    assert(actual[first + 0x81] == (gp ? 0xfe : 0x02));
}

static void check_exhaustion(uint8_t *actual, uint8_t *expected)
{
    initialize(actual);
    memset(actual + POOL, 0x77, 0x1e00); /* Includes the occupied fallback. */
    memcpy(expected, actual, RAM_SIZE);
    KiMemory a = {.main_ram = actual, .main_ram_size = RAM_SIZE};
    KiMemory e = {.main_ram = expected, .main_ram_size = RAM_SIZE};
    expected[FIGHTER + 0xc4] = 0x30;
    expected[FIGHTER + 0xc5] = 0x08;
    uint64_t wanted = 0, particle = 0;
    assert(ki15d_8800b1fc(&e, FIGHTER_ADDRESS, 1, 0x88003d6c, &wanted)
           == KI_MEMORY_OK);
    assert(ki15d_88003d30(&a, FIGHTER_ADDRESS, 1, &particle) == KI_MEMORY_OK);
    assert(wanted == UINT64_C(0xffffffff8808db00) && particle == wanted);
    compare_ram(actual, expected, 1, 0);
    for (size_t offset = POOL; offset < POOL + 0x1d00; ++offset) {
        assert(actual[offset] == 0x77);
    }
    assert(actual[POOL + 0x1d00] == 0x1a);
    assert(actual[POOL + 0x1dff] == 0); /* Occupied fallback really was cleared. */
}

static void check_failures(void)
{
    uint8_t ram[0x100] = {0};
    uint8_t expected[sizeof(ram)];
    KiMemory memory = {.main_ram = ram, .main_ram_size = 0xc4};
    uint64_t particle = UINT64_MAX;
    assert(ki15d_88003d30(&memory, 0x88000000, 1, &particle)
           == KI_MEMORY_UNMAPPED && particle == 0);
    memory.main_ram_size = 0xc5;
    /* Zero countdown must not read unmapped c5. Nonzero must read it BEFORE
     * decrementing c4, and a read failure must leave the input unchanged. */
    assert(ki15d_88003d30(&memory, 0x88000000, 1, &particle) == KI_MEMORY_OK);
    ram[0xc4] = 1;
    memcpy(expected, ram, sizeof(ram));
    assert(ki15d_88003d30(&memory, 0x88000000, 1, &particle)
           == KI_MEMORY_UNMAPPED && particle == 0);
    assert(memcmp(ram, expected, sizeof(ram)) == 0);
    memory.main_ram_size = sizeof(ram);
    ram[0xc5] = 0xff;
    expected[0xc4] = 0;
    expected[0xc5] = 0x0f;
    /* Constructor cannot save ra in this tiny RAM; earlier emitter writes
     * must survive, including the overflowing cadence's delay-slot reset. */
    assert(ki15d_88003d30(&memory, 0x88000000, 1, &particle)
           == KI_MEMORY_UNMAPPED && particle == 0);
    assert(memcmp(ram, expected, sizeof(ram)) == 0);
    ram[0xc4] = 1;
    memcpy(expected, ram, sizeof(ram));
    KiMemory rom = {.boot_rom = ram, .boot_rom_size = sizeof(ram)};
    assert(ki15d_88003d30(&rom, 0xbfc00000, 1, &particle)
           == KI_MEMORY_READ_ONLY && particle == 0);
    assert(memcmp(ram, expected, sizeof(ram)) == 0);
}

int main(void)
{
    uint8_t *actual = malloc(RAM_SIZE), *expected = malloc(RAM_SIZE);
    assert(actual != NULL && expected != NULL);
    check_sequence(actual, expected, 0);
    check_sequence(actual, expected, 1);
    check_sequence(actual, expected, UINT32_MAX);
    check_exhaustion(actual, expected);
    check_failures();
    free(actual);
    free(expected);
    puts("ki15d_88003d30: constructor sequences, orientation, fallback and errors passed");
    return 0;
}
