#include "ki/native_pilot.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RECORD_OFFSET = 0x8c000 };

static int load_at(uint8_t *ram, size_t offset, const char *path)
{
    FILE *input = fopen(path, "rb");
    if (input == NULL) return 0;
    const size_t capacity = KI_PILOT_RAM_SIZE - offset;
    const size_t count = fread(ram + offset, 1, capacity, input);
    const int ok = !ferror(input) && feof(input) && count != 0;
    fclose(input);
    return ok;
}

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static void initialize_cpu(KiNativePilot *pilot)
{
    for (unsigned int index = 1; index < KI_PILOT_GPR_COUNT; ++index)
        pilot->cpu.gpr[index] = UINT64_C(0x5a5a000000000000) | index;
    pilot->cpu.gpr[28] = 1;
    pilot->cpu.gpr[29] = UINT64_C(0xffffffff8809f000);
    pilot->cpu.gpr[30] = UINT64_C(0xffffffff8808c000);
    pilot->cpu.hi = UINT64_C(0x1111222233334444);
    pilot->cpu.lo = UINT64_C(0xaaaabbbbccccdddd);
    pilot->cpu.pc = UINT64_C(0xffffffff88002060);
}

static void initialize_dispatch_ram(uint8_t *ram, const uint8_t *source)
{
    memcpy(ram, source, KI_PILOT_RAM_SIZE);
    memset(ram + 0x8a620, 0, 0x236e0);
    write_u32_le(ram + 0x884f8, 1);
    ram[0x86238] = 6; ram[0x8623a] = 6;
    write_u32_le(ram + 0x87280, UINT32_C(0x1234));
    write_u32_le(ram + 0x87284, UINT32_C(0x5678));
}

static void initialize_dispatch_cpu(KiNativePilot *pilot)
{
    initialize_cpu(pilot);
    pilot->cpu.gpr[3] = UINT64_C(0x1122334487654321);
    pilot->cpu.gpr[29] = UINT64_C(0xffffffff88087300);
    pilot->cpu.gpr[31] = UINT64_C(0xffffffff880009a4);
}

int main(void)
{
    uint8_t *ram = calloc(KI_PILOT_RAM_SIZE, 1);
    if (ram == NULL ||
        !load_at(ram, 0, "work/kipack/ki15d/rom-0.bin") ||
        !load_at(ram, 0x33900, "work/kipack/ki15d/rom-1.bin")) {
        fprintf(stderr, "could not load admitted pilot RAM\n");
        free(ram);
        return 1;
    }

    uint8_t *record = ram + RECORD_OFFSET;
    record[0] = UINT8_C(0x1a);
    write_u32_le(record + 0x04, UINT32_C(0xffff9a00));
    write_u32_le(record + 0x0c, UINT32_C(0x00005500));
    write_u32_le(record + 0x10, UINT32_C(0x000000a0));
    write_u32_le(record + 0x20, UINT32_C(0x8805e6f2));
    record[0x42] = 0x00; record[0x43] = 0x10;
    record[0x58] = 0x00; record[0x59] = 0x10;
    record[0x5a] = 0x00; record[0x5b] = 0x10;
    ram[0x872a0] = 1;

    uint8_t *initial_ram = malloc(KI_PILOT_RAM_SIZE);
    if (initial_ram == NULL) { free(ram); return 1; }
    memcpy(initial_ram, ram, KI_PILOT_RAM_SIZE);
    KiNativePilot pilot;
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE);
    initialize_cpu(&pilot);

    const KiPilotStatus status = ki_native_pilot_advance(&pilot, 512);
    if (status != KI_PILOT_COMPLETE_ACTIVE) {
        fprintf(stderr, "connected pilot stopped: %s pc=%016" PRIx64
                        " detail=%016" PRIx64 " instructions=%" PRIu64 "\n",
                ki_native_pilot_status_name(status), pilot.stop_pc,
                pilot.stop_detail, pilot.instruction_count);
        free(ram);
        return 1;
    }
    printf("native pilot connected path passed (%" PRIu64 " instructions)\n",
           pilot.instruction_count);
    const size_t snapshot_size = ki_native_pilot_snapshot_size();
    uint8_t *snapshot = malloc(snapshot_size);
    uint8_t *rebound_ram = malloc(KI_PILOT_RAM_SIZE);
    if (snapshot == NULL || rebound_ram == NULL ||
        ki_native_pilot_snapshot_save(&pilot, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_OK) {
        fprintf(stderr, "could not save complete pilot snapshot\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }
    const uint64_t expected_hash = ki_native_pilot_state_hash(&pilot);

    KiNativePilot rebound;
    memset(rebound_ram, 0x3c, KI_PILOT_RAM_SIZE);
    ki_native_pilot_bind(&rebound, rebound_ram, KI_PILOT_RAM_SIZE);
    if (ki_native_pilot_snapshot_restore(&rebound, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_OK ||
        ki_native_pilot_state_hash(&rebound) != expected_hash ||
        memcmp(ram, rebound_ram, KI_PILOT_RAM_SIZE) != 0) {
        fprintf(stderr, "snapshot did not restore after host-buffer rebind\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }

    /* Restore the same checkpoint into distinct host buffers, then replay
     * three explicitly scheduled transactions at increasing history depth. */
    for (unsigned int depth = 0; depth < 1; ++depth) {
        const bool began_pilot = ki_native_pilot_begin(
            &pilot, UINT64_C(0xffffffff8808c000), 1);
        const bool began_rebound = ki_native_pilot_begin(
            &rebound, UINT64_C(0xffffffff8808c000), 1);
        const KiPilotStatus pilot_status = began_pilot ?
            ki_native_pilot_advance(&pilot, 1000) : pilot.status;
        const KiPilotStatus rebound_status = began_rebound ?
            ki_native_pilot_advance(&rebound, 1000) : rebound.status;
        if (!began_pilot || !began_rebound ||
            pilot_status != rebound_status ||
            (pilot_status < KI_PILOT_COMPLETE_ACTIVE ||
             pilot_status > KI_PILOT_COMPLETE_MANUAL) ||
            ki_native_pilot_state_hash(&pilot) != ki_native_pilot_state_hash(&rebound)) {
            fprintf(stderr, "snapshot replay diverged at depth %u (%s/%s)\n",
                    depth + 1, ki_native_pilot_status_name(pilot_status),
                    ki_native_pilot_status_name(rebound_status));
            fprintf(stderr, "counts=%" PRIu64 "/%" PRIu64 " seq=%" PRIu64
                            "/%" PRIu64 " ram_equal=%d hash=%016" PRIx64
                            "/%016" PRIx64 "\n",
                    pilot.instruction_count, rebound.instruction_count,
                    pilot.invocation_sequence, rebound.invocation_sequence,
                    memcmp(ram, rebound_ram, KI_PILOT_RAM_SIZE) == 0,
                    ki_native_pilot_state_hash(&pilot),
                    ki_native_pilot_state_hash(&rebound));
            for (unsigned int index = 0; index < KI_PILOT_GPR_COUNT; ++index)
                if (pilot.cpu.gpr[index] != rebound.cpu.gpr[index])
                    fprintf(stderr, "gpr%u=%016" PRIx64 "/%016" PRIx64 "\n",
                            index, pilot.cpu.gpr[index], rebound.cpu.gpr[index]);
            fprintf(stderr, "hi=%016" PRIx64 "/%016" PRIx64
                            " lo=%016" PRIx64 "/%016" PRIx64
                            " pc=%016" PRIx64 "/%016" PRIx64 "\n",
                    pilot.cpu.hi, rebound.cpu.hi, pilot.cpu.lo, rebound.cpu.lo,
                    pilot.cpu.pc, rebound.cpu.pc);
            free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
            return 1;
        }
    }

    /* A release checkpoint must restore and deterministically produce the
     * subsequent empty-dispatch boundary, not merely deserialize its hash. */
    for (unsigned int copy = 0; copy < 2; ++copy) {
        uint8_t *image = copy == 0 ? ram : rebound_ram;
        write_u32_le(image + RECORD_OFFSET + 0x18, 0);
        write_u32_le(image + RECORD_OFFSET + 0x20, UINT32_C(0x88092000));
        image[0x92000] = 0; image[0x92001] = UINT8_C(0x14);
    }
    const bool release_began_a = ki_native_pilot_begin(
        &pilot, UINT64_C(0xffffffff8808c000), 1);
    const bool release_began_b = ki_native_pilot_begin(
        &rebound, UINT64_C(0xffffffff8808c000), 1);
    const KiPilotStatus release_a = release_began_a ?
        ki_native_pilot_advance(&pilot, 1000) : pilot.status;
    const KiPilotStatus release_b = release_began_b ?
        ki_native_pilot_advance(&rebound, 1000) : rebound.status;
    if (!release_began_a || !release_began_b ||
        release_a != KI_PILOT_COMPLETE_RELEASED ||
        release_b != KI_PILOT_COMPLETE_RELEASED ||
        ki_native_pilot_state_hash(&pilot) != ki_native_pilot_state_hash(&rebound) ||
        ki_native_pilot_snapshot_save(&pilot, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_OK ||
        ki_native_pilot_snapshot_restore(&rebound, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_OK ||
        !ki_native_pilot_begin(&pilot, UINT64_C(0xffffffff8808c000), 1) ||
        !ki_native_pilot_begin(&rebound, UINT64_C(0xffffffff8808c000), 1) ||
        ki_native_pilot_advance(&pilot, 32) != KI_PILOT_COMPLETE_EMPTY ||
        ki_native_pilot_advance(&rebound, 32) != KI_PILOT_COMPLETE_EMPTY ||
        ki_native_pilot_state_hash(&pilot) != ki_native_pilot_state_hash(&rebound)) {
        fprintf(stderr, "release/empty snapshot replay diverged (%s/%s)\n",
                ki_native_pilot_status_name(release_a),
                ki_native_pilot_status_name(release_b));
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }

    const uint64_t unchanged_hash = ki_native_pilot_state_hash(&rebound);
    snapshot[snapshot_size - 1] ^= 1;
    if (ki_native_pilot_snapshot_restore(&rebound, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_INTEGRITY ||
        ki_native_pilot_state_hash(&rebound) != unchanged_hash) {
        fprintf(stderr, "corrupt snapshot changed live state\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }
    snapshot[snapshot_size - 1] ^= 1;
    if (ki_native_pilot_snapshot_restore(&rebound, snapshot, snapshot_size - 1) !=
            KI_PILOT_SNAPSHOT_SIZE) {
        fprintf(stderr, "truncated snapshot was accepted\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }
    snapshot[32] ^= 1;
    if (ki_native_pilot_snapshot_restore(&rebound, snapshot, snapshot_size) !=
            KI_PILOT_SNAPSHOT_IDENTITY ||
        ki_native_pilot_state_hash(&rebound) != unchanged_hash) {
        fprintf(stderr, "wrong-identity snapshot changed live state\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram);
        return 1;
    }
    printf("native pilot snapshot/rebind gates passed (%zu bytes)\n",
           snapshot_size);
    free(snapshot);
    free(rebound_ram);

    memcpy(ram, initial_ram, KI_PILOT_RAM_SIZE);
    ram[0x341d4] = 0; ram[0x341d5] = 0; ram[0x341d6] = 0; ram[0x341d7] = 0;
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_cpu(&pilot);
    if (ki_native_pilot_advance(&pilot, 32) != KI_PILOT_STOP_UNKNOWN_TARGET) {
        fprintf(stderr, "unknown dispatch target was not trapped\n");
        free(initial_ram); free(ram); return 1;
    }
    memcpy(ram, initial_ram, KI_PILOT_RAM_SIZE);
    ram[0x2060] ^= 1;
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_cpu(&pilot);
    if (ki_native_pilot_advance(&pilot, 32) != KI_PILOT_STOP_CODE_IDENTITY) {
        fprintf(stderr, "changed source word was not trapped\n");
        free(initial_ram); free(ram); return 1;
    }
    memcpy(ram, initial_ram, KI_PILOT_RAM_SIZE);
    ram[0x54d0] ^= 1;
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_cpu(&pilot);
    pilot.cpu.gpr[31] = UINT64_C(0xffffffff880045b4);
    pilot.cpu.pc = UINT64_C(0xffffffff880054d0);
    if (ki_native_pilot_advance(&pilot, 32) != KI_PILOT_STOP_CODE_IDENTITY) {
        fprintf(stderr, "changed manual-adapter source word was not trapped\n");
        free(initial_ram); free(ram); return 1;
    }
    memcpy(ram, initial_ram, KI_PILOT_RAM_SIZE);
    write_u32_le(ram + RECORD_OFFSET + 0x20, 0);
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_cpu(&pilot);
    if (ki_native_pilot_advance(&pilot, 256) != KI_PILOT_STOP_NULL_SCRIPT) {
        fprintf(stderr, "null installed script was not trapped\n");
        free(initial_ram); free(ram); return 1;
    }
    memcpy(ram, initial_ram, KI_PILOT_RAM_SIZE);
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_cpu(&pilot);
    if (ki_native_pilot_advance(&pilot, 1) != KI_PILOT_STOP_BUDGET) {
        fprintf(stderr, "instruction budget was not enforced\n");
        free(initial_ram); free(ram); return 1;
    }
    initialize_dispatch_ram(ram, initial_ram);
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE); initialize_dispatch_cpu(&pilot);
    if (!ki_native_pilot_begin_dispatch(&pilot, UINT64_C(0xffffffff880009a4)) ||
        ki_native_pilot_advance(&pilot, 20000) != KI_PILOT_COMPLETE_DISPATCH) {
        fprintf(stderr, "empty whole dispatcher stopped: %s pc=%016" PRIx64
                        " detail=%016" PRIx64 "\n",
                ki_native_pilot_status_name(pilot.status), pilot.stop_pc,
                pilot.stop_detail);
        free(initial_ram); free(ram); return 1;
    }
    printf("native pilot empty whole-dispatch path passed (%" PRIu64
           " cumulative instructions)\n", pilot.instruction_count);

    snapshot = malloc(ki_native_pilot_snapshot_size());
    rebound_ram = malloc(KI_PILOT_RAM_SIZE);
    if (snapshot == NULL || rebound_ram == NULL ||
        ki_native_pilot_snapshot_save(&pilot, snapshot,
            ki_native_pilot_snapshot_size()) != KI_PILOT_SNAPSHOT_OK) {
        fprintf(stderr, "whole-dispatch snapshot save failed\n");
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram); return 1;
    }
    KiNativePilot dispatch_rebound;
    ki_native_pilot_bind(&dispatch_rebound, rebound_ram, KI_PILOT_RAM_SIZE);
    const KiPilotSnapshotResult dispatch_restore =
        ki_native_pilot_snapshot_restore(&dispatch_rebound, snapshot,
            ki_native_pilot_snapshot_size());
    const bool dispatch_begin_a = ki_native_pilot_begin_dispatch(
        &pilot, UINT64_C(0xffffffff880009a4));
    const bool dispatch_begin_b = ki_native_pilot_begin_dispatch(
        &dispatch_rebound, UINT64_C(0xffffffff880009a4));
    const KiPilotStatus dispatch_status_a = dispatch_begin_a ?
        ki_native_pilot_advance(&pilot, 20000) : pilot.status;
    const KiPilotStatus dispatch_status_b = dispatch_begin_b ?
        ki_native_pilot_advance(&dispatch_rebound, 20000) : dispatch_rebound.status;
    if (dispatch_restore != KI_PILOT_SNAPSHOT_OK ||
        !dispatch_begin_a || !dispatch_begin_b ||
        dispatch_status_a != KI_PILOT_COMPLETE_DISPATCH ||
        dispatch_status_b != KI_PILOT_COMPLETE_DISPATCH ||
        ki_native_pilot_state_hash(&pilot) !=
            ki_native_pilot_state_hash(&dispatch_rebound)) {
        fprintf(stderr, "whole-dispatch snapshot replay diverged (%d,%d,%d,%s,%s)\n",
                (int)dispatch_restore, dispatch_begin_a, dispatch_begin_b,
                ki_native_pilot_status_name(dispatch_status_a),
                ki_native_pilot_status_name(dispatch_status_b));
        fprintf(stderr, "stops=%016" PRIx64 "/%016" PRIx64
                        " details=%016" PRIx64 "/%016" PRIx64 "\n",
                pilot.stop_pc, dispatch_rebound.stop_pc,
                pilot.stop_detail, dispatch_rebound.stop_detail);
        free(snapshot); free(rebound_ram); free(initial_ram); free(ram); return 1;
    }
    free(snapshot); free(rebound_ram);

#define EXPECT_DISPATCH_STOP(expected, label) do {                           \
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE);                    \
    initialize_dispatch_cpu(&pilot);                                         \
    if (!ki_native_pilot_begin_dispatch(&pilot,                              \
            UINT64_C(0xffffffff880009a4)) ||                                 \
        ki_native_pilot_advance(&pilot, 20000) != (expected)) {               \
        fprintf(stderr, label " frontier was not trapped (%s at %016" PRIx64 ")\n", \
                ki_native_pilot_status_name(pilot.status), pilot.cpu.pc);     \
        free(initial_ram); free(ram); return 1;                               \
    }                                                                         \
} while (0)
    initialize_dispatch_ram(ram, initial_ram);
    ram[0x8bc00] = 1; ram[0x8bd00] = 6;
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_UNKNOWN_PC, "unpaused fighter");
    if (ram[0x8851c] == 0 && ram[0x8851d] == 0) {
        fprintf(stderr, "unsupported fighter stop lost earlier input store\n");
        free(initial_ram); free(ram); return 1;
    }
    initialize_dispatch_ram(ram, initial_ram);
    ram[0x8a620] = 3;
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_UNKNOWN_PC, "small type3");
    initialize_dispatch_ram(ram, initial_ram);
    ram[0x8a620] = 1;
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_UNKNOWN_PC, "expired small record");
    if (ram[0x8a621] != UINT8_C(0xff)) {
        fprintf(stderr, "unsupported expiry stop lost delay-ordered decrement\n");
        free(initial_ram); free(ram); return 1;
    }
    initialize_dispatch_ram(ram, initial_ram);
    ram[0x8a620] = 1; ram[0x8a621] = 3; ram[0x8a638] = UINT8_C(0xfe);
    write_u32_le(ram + 0x8a624, UINT32_C(0x7fffffff));
    write_u32_le(ram + 0x8a62c, UINT32_C(0x1000));
    ram[0x8a630] = 0x00; ram[0x8a631] = 0x01;
    ram[0x8a632] = 0x40;
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_UNKNOWN_PC, "out-of-bounds small record");
    initialize_dispatch_ram(ram, initial_ram);
    ram[0x8bf00] = UINT8_C(0x15);
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_NULL_SCRIPT, "null animation");
    initialize_dispatch_ram(ram, initial_ram);
    write_u32_le(ram + 0x884f8, 0);
    EXPECT_DISPATCH_STOP(KI_PILOT_STOP_UNKNOWN_PC, "post-pool zero global");
    initialize_dispatch_ram(ram, initial_ram);
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE);
    initialize_dispatch_cpu(&pilot); pilot.cpu.gpr[29]++;
    if (!ki_native_pilot_begin_dispatch(&pilot, UINT64_C(0xffffffff880009a4)) ||
        ki_native_pilot_advance(&pilot, 1) != KI_PILOT_STOP_PRECONDITION) {
        fprintf(stderr, "unaligned dispatcher stack was accepted\n");
        free(initial_ram); free(ram); return 1;
    }
    initialize_dispatch_ram(ram, initial_ram);
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE);
    initialize_dispatch_cpu(&pilot);
    pilot.cpu.gpr[29] = UINT64_C(0xffffffff88100008);
    if (!ki_native_pilot_begin_dispatch(&pilot, UINT64_C(0xffffffff880009a4)) ||
        ki_native_pilot_advance(&pilot, 4) != KI_PILOT_STOP_PRECONDITION) {
        fprintf(stderr, "unmapped dispatcher stack was accepted\n");
        free(initial_ram); free(ram); return 1;
    }
    initialize_dispatch_ram(ram, initial_ram);
    uint8_t code_before[8];
    memcpy(code_before, ram + 0x2060, sizeof(code_before));
    ki_native_pilot_bind(&pilot, ram, KI_PILOT_RAM_SIZE);
    initialize_dispatch_cpu(&pilot);
    pilot.cpu.gpr[29] = UINT64_C(0xffffffff88002068);
    if (!ki_native_pilot_begin_dispatch(&pilot, UINT64_C(0xffffffff880009a4)) ||
        ki_native_pilot_advance(&pilot, 4) != KI_PILOT_STOP_PRECONDITION ||
        memcmp(code_before, ram + 0x2060, sizeof(code_before)) != 0) {
        fprintf(stderr, "code-overlapping dispatcher stack changed RAM\n");
        free(initial_ram); free(ram); return 1;
    }
#undef EXPECT_DISPATCH_STOP
    printf("native pilot explicit frontier gates passed\n");
    free(initial_ram);
    free(ram);
    return 0;
}
