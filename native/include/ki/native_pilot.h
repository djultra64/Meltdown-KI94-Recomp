#ifndef KI_NATIVE_PILOT_H
#define KI_NATIVE_PILOT_H

#include "ki/memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { KI_PILOT_RAM_SIZE = 0x100000, KI_PILOT_GPR_COUNT = 32,
       KI_PILOT_LOW_RAM_SIZE = 0x80000,
       KI_PILOT_FPR_COUNT = 32, KI_PILOT_SERVICE_INPUT_MAX = 7,
       KI_PILOT_SERVICE_OUTPUT_MAX = 12 };

typedef enum KiPilotStatus {
    KI_PILOT_READY = 0,
    KI_PILOT_COMPLETE_ACTIVE,
    KI_PILOT_COMPLETE_RELEASED,
    KI_PILOT_COMPLETE_EMPTY,
    KI_PILOT_COMPLETE_MANUAL,
    KI_PILOT_COMPLETE_DISPATCH,
    KI_PILOT_STOP_BUDGET,
    KI_PILOT_STOP_MEMORY,
    KI_PILOT_STOP_UNALIGNED,
    KI_PILOT_STOP_UNKNOWN_PC,
    KI_PILOT_STOP_UNKNOWN_TARGET,
    KI_PILOT_STOP_NULL_SCRIPT,
    KI_PILOT_STOP_DIVISION,
    KI_PILOT_STOP_PRECONDITION,
    KI_PILOT_STOP_CODE_IDENTITY,
    KI_PILOT_COMPLETE_FRAME,
    KI_PILOT_STOP_SERVICE_INPUT,
    KI_PILOT_STOP_SERVICE_OUTPUT,
    KI_PILOT_STOP_FP_DOMAIN,
    KI_PILOT_COMPLETE_CONTACT,
    KI_PILOT_STOP_ARITHMETIC,
    KI_PILOT_COMPLETE_RENDER,
    KI_PILOT_COMPLETE_PROJECTION,
    KI_PILOT_COMPLETE_CONNECTED
} KiPilotStatus;

typedef struct KiPilotCpu {
    uint64_t gpr[KI_PILOT_GPR_COUNT];
    uint64_t hi;
    uint64_t lo;
    uint64_t pc;
    uint64_t delay_target;
    bool delay_pending;
    uint64_t fpr[KI_PILOT_FPR_COUNT];
    uint32_t status_register;
    bool fcc;
    bool fcc_valid;
} KiPilotCpu;

typedef enum KiPilotServiceKind { KI_PILOT_SERVICE_COUNT=1, KI_PILOT_SERVICE_MMIO_BYTE=2 } KiPilotServiceKind;
typedef struct KiPilotServiceInput { uint32_t kind, pc, address, value; } KiPilotServiceInput;
typedef struct KiPilotServiceOutput { uint64_t invocation; uint32_t pc, address, value; } KiPilotServiceOutput;

typedef struct KiNativePilot {
    KiMemory memory;
    KiPilotCpu cpu;
    KiPilotStatus status;
    uint64_t stop_pc;
    uint64_t stop_detail;
    uint64_t instruction_count;
    uint64_t invocation_sequence;
    uint32_t transaction_kind;
    KiPilotServiceInput service_inputs[KI_PILOT_SERVICE_INPUT_MAX];
    KiPilotServiceOutput service_outputs[KI_PILOT_SERVICE_OUTPUT_MAX];
    uint32_t service_input_count, service_input_cursor;
    uint32_t service_output_count, service_output_capacity;
} KiNativePilot;

enum {
    KI_PILOT_TRANSACTION_RECORD = 0,
    KI_PILOT_TRANSACTION_DISPATCH = 1,
    KI_PILOT_TRANSACTION_FRAME = 2,
    KI_PILOT_TRANSACTION_CONTACT = 3,
    KI_PILOT_TRANSACTION_RENDER = 4,
    KI_PILOT_TRANSACTION_PROJECTION = 5,
    KI_PILOT_TRANSACTION_CONNECTED = 6
};

typedef enum KiPilotSnapshotResult {
    KI_PILOT_SNAPSHOT_OK = 0,
    KI_PILOT_SNAPSHOT_ARGUMENT,
    KI_PILOT_SNAPSHOT_BOUNDARY,
    KI_PILOT_SNAPSHOT_SIZE,
    KI_PILOT_SNAPSHOT_FORMAT,
    KI_PILOT_SNAPSHOT_IDENTITY,
    KI_PILOT_SNAPSHOT_INTEGRITY
} KiPilotSnapshotResult;

typedef enum KiPilotRegionId {
    KI_PILOT_REGION_STARTUP_CLEAR = 1
} KiPilotRegionId;

typedef struct KiPilotRegionDescriptor {
    KiPilotRegionId id;
    uint32_t entry_pc;
    uint32_t exit_pc;
    uint32_t source_word_count;
    const char *source_sha256;
    const char *state_scope;
} KiPilotRegionDescriptor;

/* Bind caller-owned RAM. Fill the complete CPU context and declarative RAM
 * seed before advance; the pilot samples no host input or hidden static state. */
void ki_native_pilot_bind(KiNativePilot *pilot, uint8_t *ram, size_t ram_size);
bool ki_native_pilot_bind_low_ram(KiNativePilot *pilot, uint8_t *low_ram,
                                  size_t low_ram_size);
/* Start the next explicitly scheduled transaction from a successful boundary.
 * No fp/gp value is inferred from prior execution or hidden host state. */
bool ki_native_pilot_begin(KiNativePilot *pilot, uint64_t fp, uint64_t gp);
bool ki_native_pilot_begin_dispatch(KiNativePilot *pilot,
                                    uint64_t return_address);
/* Begin the bounded type-1a frame selector with a caller-supplied full CPU
 * context.  Admission verifies its known entry registers and mutable table. */
bool ki_native_pilot_begin_frame(KiNativePilot *pilot);
bool ki_native_pilot_configure_services(KiNativePilot *pilot,
                                        const KiPilotServiceInput *inputs,
                                        size_t input_count,
                                        size_t output_capacity);
bool ki_native_pilot_begin_contact(KiNativePilot *pilot);
bool ki_native_pilot_begin_render(KiNativePilot *pilot);
bool ki_native_pilot_begin_projection(KiNativePilot *pilot);
/* Enter the source-derived connected phase at the original 099c boundary.
 * This is an experimental whole-context transaction, not a production loop. */
bool ki_native_pilot_begin_connected(KiNativePilot *pilot);
/* Static regions are authenticated AOT entries, not caller-described code.
 * The caller supplies the complete guest state and must already be at entry. */
const KiPilotRegionDescriptor *ki_native_pilot_region_descriptor(
    KiPilotRegionId id);
bool ki_native_pilot_begin_region(KiNativePilot *pilot, KiPilotRegionId id);
KiPilotStatus ki_native_pilot_advance(KiNativePilot *pilot, uint64_t budget);
const char *ki_native_pilot_status_name(KiPilotStatus status);
size_t ki_native_pilot_snapshot_size(void);
KiPilotSnapshotResult ki_native_pilot_snapshot_save(
    const KiNativePilot *pilot, uint8_t *output, size_t output_size);
KiPilotSnapshotResult ki_native_pilot_snapshot_restore(
    KiNativePilot *pilot, const uint8_t *input, size_t input_size);
uint64_t ki_native_pilot_state_hash(const KiNativePilot *pilot);
bool ki_native_pilot_exact_i32_to_f32(int32_t input, uint32_t *output);
bool ki_native_pilot_exact_f32_sqrt(uint32_t input, uint32_t *output);
bool ki_native_pilot_exact_f32_to_i32(uint32_t input, int32_t *output);
bool ki_native_pilot_sub_word(int32_t left, int32_t right, int32_t *output);

#endif
