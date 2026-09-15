#include "ki/native_pilot.h"

#include <string.h>

enum { HEADER_SIZE = 1152, SNAPSHOT_VERSION = 3 };
static const uint8_t magic[8] = {'K','I','N','P','S','0','0','3'};
/* SHA-256 of the documented v1 build/load/source identity manifest. */
#include "generated/ki15d_native_pilot_identity.inc"

static void put32(uint8_t *out, uint32_t value)
{
    for (unsigned int i = 0; i < 4; ++i) out[i] = (uint8_t)(value >> (i * 8));
}

static void put64(uint8_t *out, uint64_t value)
{
    for (unsigned int i = 0; i < 8; ++i) out[i] = (uint8_t)(value >> (i * 8));
}

static uint32_t get32(const uint8_t *in)
{
    uint32_t value = 0;
    for (unsigned int i = 0; i < 4; ++i) value |= (uint32_t)in[i] << (i * 8);
    return value;
}

static uint64_t get64(const uint8_t *in)
{
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; ++i) value |= (uint64_t)in[i] << (i * 8);
    return value;
}

static uint64_t hash_bytes(const uint8_t *bytes, size_t size)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool successful_boundary(KiPilotStatus status)
{
    return (status >= KI_PILOT_COMPLETE_ACTIVE && status <= KI_PILOT_COMPLETE_DISPATCH) ||
           status == KI_PILOT_COMPLETE_FRAME || status == KI_PILOT_COMPLETE_CONTACT ||
           status == KI_PILOT_COMPLETE_RENDER || status == KI_PILOT_COMPLETE_PROJECTION ||
           status == KI_PILOT_COMPLETE_CONNECTED;
}

static bool snapshot_boundary(const KiNativePilot *pilot)
{
    /* Region exits are deliberately typed stops; they are not effect-complete
     * snapshots, even if a caller forges an old completion status. */
    if (pilot->transaction_kind > KI_PILOT_TRANSACTION_CONNECTED) return false;
    return successful_boundary(pilot->status) ||
           (pilot->status==KI_PILOT_READY &&
            ((pilot->transaction_kind==KI_PILOT_TRANSACTION_CONTACT &&
              (uint32_t)pilot->cpu.pc==UINT32_C(0x88008cdc)) ||
             (pilot->transaction_kind==KI_PILOT_TRANSACTION_CONNECTED &&
              (uint32_t)pilot->cpu.pc==UINT32_C(0x8800099c))) &&
            pilot->service_input_cursor==0 && pilot->service_output_count==0);
}

size_t ki_native_pilot_snapshot_size(void)
{
    return HEADER_SIZE + KI_PILOT_RAM_SIZE + KI_PILOT_LOW_RAM_SIZE;
}

KiPilotSnapshotResult ki_native_pilot_snapshot_save(
    const KiNativePilot *pilot, uint8_t *output, size_t output_size)
{
    const size_t required = ki_native_pilot_snapshot_size();
    if (pilot == NULL || output == NULL || pilot->memory.main_ram == NULL)
        return KI_PILOT_SNAPSHOT_ARGUMENT;
    if (output_size != required || pilot->memory.main_ram_size != KI_PILOT_RAM_SIZE ||
        (pilot->memory.low_ram && pilot->memory.low_ram_size!=KI_PILOT_LOW_RAM_SIZE) ||
        (!pilot->memory.low_ram && pilot->memory.low_ram_size!=0))
        return KI_PILOT_SNAPSHOT_SIZE;
    if (!snapshot_boundary(pilot) || pilot->cpu.delay_pending)
        return KI_PILOT_SNAPSHOT_BOUNDARY;
    memset(output, 0, HEADER_SIZE);
    memcpy(output, magic, sizeof(magic));
    put32(output + 8, SNAPSHOT_VERSION); put32(output + 12, (uint32_t)required);
    put32(output + 16, KI_PILOT_RAM_SIZE); put32(output + 20, (uint32_t)pilot->status);
    memcpy(output + 32, snapshot_identity, sizeof(snapshot_identity));
    size_t at = 64;
    for (unsigned int i = 0; i < KI_PILOT_GPR_COUNT; ++i, at += 8)
        put64(output + at, pilot->cpu.gpr[i]);
    put64(output + 320, pilot->cpu.hi); put64(output + 328, pilot->cpu.lo);
    put64(output + 336, pilot->cpu.pc); put64(output + 344, pilot->cpu.delay_target);
    put32(output + 352, pilot->cpu.delay_pending); put32(output + 356, pilot->status);
    put64(output + 360, pilot->stop_pc); put64(output + 368, pilot->stop_detail);
    put64(output + 376, pilot->instruction_count); put64(output + 384, pilot->invocation_sequence);
    put32(output + 392, pilot->transaction_kind);
    for (unsigned int i=0;i<KI_PILOT_FPR_COUNT;++i) put64(output+400+i*8,pilot->cpu.fpr[i]);
    put32(output+656,pilot->cpu.status_register);
    put32(output+660,pilot->service_input_count);put32(output+664,pilot->service_input_cursor);
    put32(output+668,pilot->service_output_count);put32(output+672,pilot->service_output_capacity);
    put32(output+1032,pilot->memory.low_ram!=NULL);
    put32(output+1036,pilot->cpu.fcc);
    put32(output+1040,pilot->cpu.fcc_valid);
    for(unsigned i=0;i<KI_PILOT_SERVICE_INPUT_MAX;++i){size_t p=676+i*16;put32(output+p,pilot->service_inputs[i].kind);put32(output+p+4,pilot->service_inputs[i].pc);put32(output+p+8,pilot->service_inputs[i].address);put32(output+p+12,pilot->service_inputs[i].value);}
    for(unsigned i=0;i<KI_PILOT_SERVICE_OUTPUT_MAX;++i){size_t p=788+i*20;put64(output+p,pilot->service_outputs[i].invocation);put32(output+p+8,pilot->service_outputs[i].pc);put32(output+p+12,pilot->service_outputs[i].address);put32(output+p+16,pilot->service_outputs[i].value);}
    memcpy(output + HEADER_SIZE, pilot->memory.main_ram, KI_PILOT_RAM_SIZE);
    if (pilot->memory.low_ram && pilot->memory.low_ram_size==KI_PILOT_LOW_RAM_SIZE)
        memcpy(output+HEADER_SIZE+KI_PILOT_RAM_SIZE,pilot->memory.low_ram,KI_PILOT_LOW_RAM_SIZE);
    else
        memset(output+HEADER_SIZE+KI_PILOT_RAM_SIZE,0,KI_PILOT_LOW_RAM_SIZE);
    put64(output + 24, hash_bytes(output + 32, required - 32));
    return KI_PILOT_SNAPSHOT_OK;
}

KiPilotSnapshotResult ki_native_pilot_snapshot_restore(
    KiNativePilot *pilot, const uint8_t *input, size_t input_size)
{
    const size_t required = ki_native_pilot_snapshot_size();
    if (pilot == NULL || input == NULL || pilot->memory.main_ram == NULL)
        return KI_PILOT_SNAPSHOT_ARGUMENT;
    if (input_size != required || pilot->memory.main_ram_size != KI_PILOT_RAM_SIZE ||
        get32(input + 12) != required || get32(input + 16) != KI_PILOT_RAM_SIZE)
        return KI_PILOT_SNAPSHOT_SIZE;
    if (memcmp(input, magic, sizeof(magic)) != 0 || get32(input + 8) != SNAPSHOT_VERSION)
        return KI_PILOT_SNAPSHOT_FORMAT;
    if (memcmp(input + 32, snapshot_identity, sizeof(snapshot_identity)) != 0)
        return KI_PILOT_SNAPSHOT_IDENTITY;
    if (get64(input + 24) != hash_bytes(input + 32, required - 32))
        return KI_PILOT_SNAPSHOT_INTEGRITY;
    const uint32_t status = get32(input + 356);
    const bool saved_low_present=get32(input+1032)!=0;
    const bool live_low_present=pilot->memory.low_ram!=NULL;
    const uint32_t transaction=get32(input+392);
    const uint32_t pc=(uint32_t)get64(input+336);
    const bool ready_transaction=status==KI_PILOT_READY &&
        ((transaction==KI_PILOT_TRANSACTION_CONTACT && pc==UINT32_C(0x88008cdc)) ||
         (transaction==KI_PILOT_TRANSACTION_CONNECTED && pc==UINT32_C(0x8800099c))) &&
        get32(input+664)==0 && get32(input+668)==0;
    if (status != get32(input + 20) ||
        (!successful_boundary((KiPilotStatus)status) && !ready_transaction) ||
        get32(input + 352) != 0 || get32(input + 392) > KI_PILOT_TRANSACTION_CONNECTED ||
        get32(input+1036)>1 || get32(input+1040)>1 ||
        get32(input+660)>KI_PILOT_SERVICE_INPUT_MAX || get32(input+664)>get32(input+660) ||
        get32(input+672)>KI_PILOT_SERVICE_OUTPUT_MAX || get32(input+668)>get32(input+672))
        return KI_PILOT_SNAPSHOT_BOUNDARY;
    if (get32(input+1032)>1 || saved_low_present!=live_low_present ||
        (live_low_present && pilot->memory.low_ram_size!=KI_PILOT_LOW_RAM_SIZE))
        return KI_PILOT_SNAPSHOT_SIZE;

    /* Validation above is complete; no live byte changes before this point. */
    KiPilotCpu cpu = {0};
    size_t at = 64;
    for (unsigned int i = 0; i < KI_PILOT_GPR_COUNT; ++i, at += 8)
        cpu.gpr[i] = get64(input + at);
    cpu.hi = get64(input + 320); cpu.lo = get64(input + 328);
    cpu.pc = get64(input + 336); cpu.delay_target = get64(input + 344);
    for(unsigned i=0;i<KI_PILOT_FPR_COUNT;++i)cpu.fpr[i]=get64(input+400+i*8);
    cpu.status_register=get32(input+656);
    cpu.fcc=get32(input+1036)!=0;
    cpu.fcc_valid=get32(input+1040)!=0;
    KiPilotServiceInput service_inputs[KI_PILOT_SERVICE_INPUT_MAX]={0};
    KiPilotServiceOutput service_outputs[KI_PILOT_SERVICE_OUTPUT_MAX]={0};
    for(unsigned i=0;i<KI_PILOT_SERVICE_INPUT_MAX;++i){size_t p=676+i*16;service_inputs[i].kind=get32(input+p);service_inputs[i].pc=get32(input+p+4);service_inputs[i].address=get32(input+p+8);service_inputs[i].value=get32(input+p+12);}
    for(unsigned i=0;i<KI_PILOT_SERVICE_OUTPUT_MAX;++i){size_t p=788+i*20;service_outputs[i].invocation=get64(input+p);service_outputs[i].pc=get32(input+p+8);service_outputs[i].address=get32(input+p+12);service_outputs[i].value=get32(input+p+16);}
    const uint8_t *saved_low=input+HEADER_SIZE+KI_PILOT_RAM_SIZE;
    memcpy(pilot->memory.main_ram, input + HEADER_SIZE, KI_PILOT_RAM_SIZE);
    if(pilot->memory.low_ram)memcpy(pilot->memory.low_ram,saved_low,KI_PILOT_LOW_RAM_SIZE);
    pilot->cpu = cpu; pilot->status = (KiPilotStatus)status;
    pilot->stop_pc = get64(input + 360); pilot->stop_detail = get64(input + 368);
    pilot->instruction_count = get64(input + 376);
    pilot->invocation_sequence = get64(input + 384);
    pilot->transaction_kind = get32(input + 392);
    memcpy(pilot->service_inputs,service_inputs,sizeof(service_inputs));
    memcpy(pilot->service_outputs,service_outputs,sizeof(service_outputs));
    pilot->service_input_count=get32(input+660);pilot->service_input_cursor=get32(input+664);
    pilot->service_output_count=get32(input+668);pilot->service_output_capacity=get32(input+672);
    return KI_PILOT_SNAPSHOT_OK;
}

uint64_t ki_native_pilot_state_hash(const KiNativePilot *pilot)
{
    if (pilot == NULL || pilot->memory.main_ram == NULL ||
        pilot->memory.main_ram_size != KI_PILOT_RAM_SIZE) return 0;
    uint64_t hash = hash_bytes(pilot->memory.main_ram, KI_PILOT_RAM_SIZE);
    if(pilot->memory.low_ram&&pilot->memory.low_ram_size==KI_PILOT_LOW_RAM_SIZE){hash^=hash_bytes(pilot->memory.low_ram,KI_PILOT_LOW_RAM_SIZE);hash*=UINT64_C(1099511628211);}
    for (unsigned int i = 0; i < KI_PILOT_GPR_COUNT; ++i) {
        uint8_t word[8]; put64(word, pilot->cpu.gpr[i]);
        hash ^= hash_bytes(word, sizeof(word)); hash *= UINT64_C(1099511628211);
    }
    uint8_t tail[76];
    put64(tail, pilot->cpu.hi); put64(tail + 8, pilot->cpu.lo);
    put64(tail + 16, pilot->cpu.pc); put64(tail + 24, pilot->instruction_count);
    put64(tail + 32, pilot->invocation_sequence);
    put64(tail + 40, (uint64_t)pilot->status | ((uint64_t)pilot->transaction_kind << 32));
    put32(tail+48,pilot->cpu.status_register);
    put32(tail+52,pilot->service_input_count);
    put32(tail+56,pilot->service_input_cursor);
    put32(tail+60,pilot->service_output_count |
                  (pilot->service_output_capacity << 16));
    put32(tail+64,pilot->memory.low_ram!=NULL);
    put32(tail+68,pilot->cpu.fcc);
    put32(tail+72,pilot->cpu.fcc_valid);
    hash ^= hash_bytes(tail, sizeof(tail));
    for (unsigned i=0;i<KI_PILOT_FPR_COUNT;++i) {
        uint8_t encoded[8];
        put64(encoded,pilot->cpu.fpr[i]);
        hash ^= hash_bytes(encoded,sizeof(encoded));
        hash *= UINT64_C(1099511628211);
    }
    for (unsigned i=0;i<KI_PILOT_SERVICE_INPUT_MAX;++i) {
        uint8_t encoded[16];
        put32(encoded,pilot->service_inputs[i].kind);
        put32(encoded+4,pilot->service_inputs[i].pc);
        put32(encoded+8,pilot->service_inputs[i].address);
        put32(encoded+12,pilot->service_inputs[i].value);
        hash ^= hash_bytes(encoded,sizeof(encoded));
        hash *= UINT64_C(1099511628211);
    }
    for (unsigned i=0;i<KI_PILOT_SERVICE_OUTPUT_MAX;++i) {
        uint8_t encoded[20];
        put64(encoded,pilot->service_outputs[i].invocation);
        put32(encoded+8,pilot->service_outputs[i].pc);
        put32(encoded+12,pilot->service_outputs[i].address);
        put32(encoded+16,pilot->service_outputs[i].value);
        hash ^= hash_bytes(encoded,sizeof(encoded));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
