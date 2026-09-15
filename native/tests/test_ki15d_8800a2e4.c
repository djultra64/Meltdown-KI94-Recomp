#include "ki/original/ki15d_88003d30.h"
#include "ki/original/ki15d_8800a2e4.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum {
    RAM_SIZE = 0x100000,
    WINDOW_OFFSET = 0x90000,
    WINDOW_SIZE = 0x4000,
    TABLE_OFFSET = 0x34610,
    TABLE_SIZE = 0x80
};

typedef struct Case {
    unsigned int number;
    char name[48];
    unsigned int receiver, attacker, descriptor;
    unsigned int effect, flags, height;
    unsigned int receiver_z, attacker_z;
    unsigned int variant, countdown, cadence, row_variant;
} Case;

static uint8_t ram[RAM_SIZE];
static uint8_t expected_ram[RAM_SIZE];
static unsigned int constructor_calls;

static size_t offset(uint64_t address)
{
    return (size_t)((uint32_t)address - UINT32_C(0x88000000));
}

static void write_u8(uint64_t address, unsigned int value)
{
    ram[offset(address)] = (uint8_t)value;
}

static void write_u32(uint64_t address, uint32_t value)
{
    const size_t at = offset(address);
    ram[at] = (uint8_t)value;
    ram[at + 1] = (uint8_t)(value >> 8);
    ram[at + 2] = (uint8_t)(value >> 16);
    ram[at + 3] = (uint8_t)(value >> 24);
}

static void seed_case(const Case *test)
{
    memset(ram, 0x5a, sizeof(ram));
    memset(ram + TABLE_OFFSET, 0x66, TABLE_SIZE);
    for (unsigned int index = 0; index < 16; ++index) {
        write_u8(UINT64_C(0x88034647) + index, index * 13u + 7u);
    }
    const uint64_t row = UINT64_C(0x88034610) + (test->effect & 15u) * 8u;
    write_u8(row, test->countdown);
    write_u8(row + 1, test->cadence);
    write_u8(row + 2, test->row_variant);
    const uint64_t receiver = UINT64_C(0x88090000) + test->receiver;
    const uint64_t attacker = UINT64_C(0x88090000) + test->attacker;
    const uint64_t descriptor = UINT64_C(0x88090000) + test->descriptor;
    write_u32(receiver + 0x0c, test->receiver_z);
    write_u32(attacker + 0x0c, test->attacker_z);
    write_u8(attacker + 0x8e, test->variant);
    write_u8(descriptor + 0x1d, test->effect);
    write_u8(descriptor + 0x13, test->flags);
    write_u8(descriptor + 0x1a, test->height);
}

static uint64_t bounded_digest(void)
{
    uint64_t digest = UINT64_C(0xcbf29ce484222325);
    for (size_t index = 0; index < WINDOW_SIZE; ++index) {
        digest = (digest ^ ram[WINDOW_OFFSET + index]) * UINT64_C(0x100000001b3);
    }
    for (size_t index = 0; index < TABLE_SIZE; ++index) {
        digest = (digest ^ ram[TABLE_OFFSET + index]) * UINT64_C(0x100000001b3);
    }
    return digest;
}

static void original_fixture_cases(void)
{
    FILE *inputs = fopen("tests/fixtures/ki15d_8800a2e4_inputs.csv", "r");
    FILE *outputs = fopen("tests/fixtures/ki15d_8800a2e4.csv", "r");
    assert(inputs != NULL && outputs != NULL);
    char input_line[256], output_line[128], trailing;
    assert(fgets(input_line, sizeof(input_line), inputs) != NULL);
    assert(fgets(output_line, sizeof(output_line), outputs) != NULL);
    assert(strcmp(input_line, "case,name,receiver,attacker,descriptor,effect,flags,height,receiver_z,attacker_z,variant,countdown,cadence,row_variant\n") == 0);
    assert(strcmp(output_line, "case,c4,c5,c6,c7,bounded_fnv64\n") == 0);
    for (unsigned int number = 1; number <= 18; ++number) {
        Case test;
        assert(fgets(input_line, sizeof(input_line), inputs) != NULL);
        assert(sscanf(input_line,
                      "%u,%47[^,],%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x %c",
                      &test.number, test.name, &test.receiver, &test.attacker,
                      &test.descriptor, &test.effect, &test.flags, &test.height,
                      &test.receiver_z, &test.attacker_z, &test.variant,
                      &test.countdown, &test.cadence, &test.row_variant,
                      &trailing) == 14);
        unsigned int output_number, c4, c5, c6, c7;
        uint64_t oracle_digest;
        assert(fgets(output_line, sizeof(output_line), outputs) != NULL);
        assert(sscanf(output_line, "%u,%x,%x,%x,%x,%" SCNx64 " %c",
                      &output_number, &c4, &c5, &c6, &c7, &oracle_digest,
                      &trailing) == 6);
        assert(test.number == number && output_number == number);
        seed_case(&test);
        memcpy(expected_ram, ram, sizeof(ram));
        const size_t receiver = WINDOW_OFFSET + test.receiver;
        expected_ram[receiver + 0xc4] = (uint8_t)c4;
        expected_ram[receiver + 0xc5] = (uint8_t)c5;
        expected_ram[receiver + 0xc6] = (uint8_t)c6;
        expected_ram[receiver + 0xc7] = (uint8_t)c7;
        KiMemory memory = {.main_ram = ram, .main_ram_size = sizeof(ram)};
        const KiMemoryResult result = ki15d_8800a2e4(
            &memory, UINT64_C(0xffffffff88090000) + test.receiver,
            UINT64_C(0xffffffff88090000) + test.attacker,
            UINT64_C(0xffffffff88090000) + test.descriptor);
        if (result != KI_MEMORY_OK || memcmp(ram, expected_ram, sizeof(ram)) != 0 ||
            bounded_digest() != oracle_digest) {
            fprintf(stderr, "contact initializer mismatch case %u (%s)\n", number, test.name);
            assert(0);
        }
    }
    assert(fgets(input_line, sizeof(input_line), inputs) == NULL && !ferror(inputs));
    assert(fgets(output_line, sizeof(output_line), outputs) == NULL && !ferror(outputs));
    fclose(inputs);
    fclose(outputs);
}

static void ordered_mapping_failures(void)
{
    KiMemory memory = {.main_ram = ram};
    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + 8;
    write_u8(UINT64_C(0x8800011d), 1);
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, UINT64_C(0x88000200), UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + 0x79;
    write_u8(UINT64_C(0x8800011d), 0x0f);
    write_u8(UINT64_C(0x88034688), 0x44);
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, UINT64_C(0x88000200), UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[0x2c4] = 0x44;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + TABLE_SIZE;
    write_u8(UINT64_C(0x8800011d), 1);
    write_u8(UINT64_C(0x88034618), 0x31);
    write_u8(UINT64_C(0x88034619), 0x78);
    const uint64_t short_receiver = UINT64_C(0x88000000) + memory.main_ram_size - 0xc5;
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, short_receiver, UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[memory.main_ram_size - 1] = 0x31;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + TABLE_SIZE;
    write_u8(UINT64_C(0x8800011d), 1);
    write_u8(UINT64_C(0x88000113), 0x40);
    write_u8(UINT64_C(0x8800011a), 0x55);
    write_u8(UINT64_C(0x88034618), 0x31);
    write_u8(UINT64_C(0x88034619), 0x78);
    const uint64_t c6_receiver = UINT64_C(0x88000000) + memory.main_ram_size - 0xc6;
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, c6_receiver, UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[memory.main_ram_size - 2] = 0x31;
    expected_ram[memory.main_ram_size - 1] = 0x78;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    /* A high-nibble mapping failure retains c4/c5/c6 in original order. */
    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + 0x38;
    write_u8(UINT64_C(0x8800011d), 0x11);
    write_u8(UINT64_C(0x88000113), 0x40);
    write_u8(UINT64_C(0x8800011a), 0x55);
    write_u8(UINT64_C(0x8800038e), 0x22);
    write_u8(UINT64_C(0x88034618), 0x31);
    write_u8(UINT64_C(0x88034619), 0x78);
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, UINT64_C(0x88000200), UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[0x2c4] = 0x31;
    expected_ram[0x2c5] = 0x78;
    expected_ram[0x2c6] = 0x55;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    /* A missing late row byte likewise retains the first three stores. */
    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + 0x0a;
    write_u8(UINT64_C(0x8800011d), 1);
    write_u8(UINT64_C(0x88000113), 0x40);
    write_u8(UINT64_C(0x8800011a), 0x55);
    write_u8(UINT64_C(0x8800038e), 0x22);
    write_u8(UINT64_C(0x88034618), 0x31);
    write_u8(UINT64_C(0x88034619), 0x78);
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, UINT64_C(0x88000200), UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[0x2c4] = 0x31;
    expected_ram[0x2c5] = 0x78;
    expected_ram[0x2c6] = 0x55;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);

    /* The final receiver byte can fail after all original reads complete. */
    memset(ram, 0x5a, sizeof(ram));
    memory.main_ram_size = TABLE_OFFSET + TABLE_SIZE;
    write_u8(UINT64_C(0x8800011d), 1);
    write_u8(UINT64_C(0x88000113), 0x40);
    write_u8(UINT64_C(0x8800011a), 0x55);
    write_u8(UINT64_C(0x8800038e), 0x22);
    write_u8(UINT64_C(0x88034618), 0x31);
    write_u8(UINT64_C(0x88034619), 0x78);
    write_u8(UINT64_C(0x8803461a), 0x03);
    const uint64_t c7_receiver = UINT64_C(0x88000000) + memory.main_ram_size - 0xc7;
    memcpy(expected_ram, ram, sizeof(ram));
    assert(ki15d_8800a2e4(&memory, c7_receiver, UINT64_C(0x88000300),
                          UINT64_C(0x88000100)) == KI_MEMORY_UNMAPPED);
    expected_ram[memory.main_ram_size - 3] = 0x31;
    expected_ram[memory.main_ram_size - 2] = 0x78;
    expected_ram[memory.main_ram_size - 1] = 0x55;
    assert(memcmp(ram, expected_ram, sizeof(ram)) == 0);
}

/* Test-only constructor boundary for initializer -> emitter composition. */
KiMemoryResult ki15d_8800b1fc(KiMemory *memory, uint64_t fighter_address,
                              uint32_t gp_value, uint32_t return_address,
                              uint64_t *particle_address)
{
    (void)memory;
    assert(fighter_address == UINT64_C(0xffffffff88090100));
    assert(gp_value == 1 && return_address == UINT32_C(0x88003d6c));
    ++constructor_calls;
    *particle_address = UINT64_C(0xffffffff8808be00);
    return KI_MEMORY_OK;
}

static void initializer_emitter_composition(void)
{
    Case test = {1, "organic_seed", 0x100, 0x200, 0x300, 1, 0x80, 1,
                 0, 0x55aa, 0, 0x31, 0x78, 0};
    seed_case(&test);
    KiMemory memory = {.main_ram = ram, .main_ram_size = sizeof(ram)};
    assert(ki15d_8800a2e4(&memory, UINT64_C(0xffffffff88090100),
                          UINT64_C(0xffffffff88090200),
                          UINT64_C(0xffffffff88090300)) == KI_MEMORY_OK);
    constructor_calls = 0;
    for (unsigned int invocation = 0; invocation < 49; ++invocation) {
        uint64_t particle = 0;
        assert(ki15d_88003d30(&memory, UINT64_C(0xffffffff88090100), 1,
                               &particle) == KI_MEMORY_OK);
    }
    assert(constructor_calls == 7);
    assert(ram[WINDOW_OFFSET + 0x100 + 0xc4] == 0);
}

int main(void)
{
    original_fixture_cases();
    ordered_mapping_failures();
    initializer_emitter_composition();
    puts("native contact initializer: 18 original cases, full RAM, failures and 7 emissions passed");
    return 0;
}
