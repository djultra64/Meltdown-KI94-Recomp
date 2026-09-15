#include "ki/original/ki15d_88003d30.h"
#include "ki/original/ki15d_8800b1fc.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

enum { RAM_SIZE = 1024, FIGHTER_OFFSET = 0x100 };
static uint8_t expected_ram[RAM_SIZE];
static unsigned int calls;

/* Test-only link substitution: observe the exact constructor call boundary.
 * Production builds link the real, separately verified constructor instead. */
KiMemoryResult ki15d_8800b1fc(KiMemory *memory, uint64_t fighter_address,
                              uint32_t gp_value, uint32_t return_address,
                              uint64_t *particle_address)
{
    assert(fighter_address == UINT64_C(0xffffffff88000100));
    assert(gp_value == 1);
    assert(return_address == UINT32_C(0x88003d6c));
    assert(memcmp(memory->main_ram, expected_ram, RAM_SIZE) == 0);
    ++calls;
    *particle_address = UINT64_C(0xffffffff8808be00);
    return KI_MEMORY_OK;
}

int main(void)
{
    FILE *fixture = fopen("tests/fixtures/ki15d_88003d30.csv", "r");
    assert(fixture != NULL);
    char line[128];
    assert(fgets(line, sizeof(line), fixture) != NULL);
    assert(strcmp(line, "countdown,cases,digest\n") == 0);
    uint8_t ram[RAM_SIZE];
    KiMemory memory = {.main_ram = ram, .main_ram_size = sizeof(ram)};
    for (unsigned int countdown = 0; countdown < 256; ++countdown) {
        uint32_t digest = UINT32_C(0x811c9dc5);
        for (unsigned int cadence = 0; cadence < 256; ++cadence) {
            memset(ram, 0x5a, sizeof(ram));
            ram[FIGHTER_OFFSET + 0xc4] = (uint8_t)countdown;
            ram[FIGHTER_OFFSET + 0xc5] = (uint8_t)cadence;
            memcpy(expected_ram, ram, sizeof(ram));
            /* Independent nibble formulation, plus the original-CPU digest
             * below: every case checks the whole mapped RAM, not just c4/c5. */
            const unsigned int next_high = (cadence / 16) + 1;
            const unsigned int threshold = cadence % 16;
            const unsigned int emit = countdown != 0 && next_high >= threshold;
            if (countdown != 0) {
                expected_ram[FIGHTER_OFFSET + 0xc4] = (uint8_t)(countdown - 1);
                expected_ram[FIGHTER_OFFSET + 0xc5] =
                    (uint8_t)(emit ? threshold : cadence + 16);
            }
            calls = 0;
            uint64_t particle = UINT64_MAX;
            assert(ki15d_88003d30(&memory, UINT64_C(0xffffffff88000100),
                                   1, &particle) == KI_MEMORY_OK);
            if (memcmp(ram, expected_ram, sizeof(ram)) != 0 || calls != emit ||
                particle != (emit ? UINT64_C(0xffffffff8808be00) : 0)) {
                fprintf(stderr, "decision mismatch c4=%02x c5=%02x\n",
                        countdown, cadence);
                return 1;
            }
            const uint32_t packed =
                ((uint32_t)ram[FIGHTER_OFFSET + 0xc4] << 16) |
                ((uint32_t)ram[FIGHTER_OFFSET + 0xc5] << 8) | calls;
            digest = (digest ^ packed) * UINT32_C(0x01000193);
        }
        unsigned int fixture_countdown, cases;
        uint32_t oracle_digest;
        char trailing;
        assert(fgets(line, sizeof(line), fixture) != NULL);
        assert(sscanf(line, "%x,%u,%" SCNx32 " %c", &fixture_countdown,
                      &cases, &oracle_digest, &trailing) == 3);
        assert(fixture_countdown == countdown && cases == 256);
        if (digest != oracle_digest) {
            fprintf(stderr, "MAME digest mismatch c4=%02x: expected=%08" PRIx32
                    " actual=%08" PRIx32 "\n", countdown, oracle_digest, digest);
            return 1;
        }
    }
    assert(fgets(line, sizeof(line), fixture) == NULL && !ferror(fixture));
    fclose(fixture);
    puts("ki15d_88003d30: 65,536 original-CPU decision cases and full-RAM checks passed");
    return 0;
}
