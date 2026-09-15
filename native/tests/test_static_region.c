#include "ki/native_pilot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}

static void seed(KiNativePilot *p, uint8_t *ram)
{
    static const uint32_t words[10]={0x3c038808,0x3c018808,0x34634c00,0x3404b400,
        0xac207260,0x24020001,0x00642021,0x24630008,0x1464fffe,0xfc60fff8};
    for(size_t i=0;i<KI_PILOT_RAM_SIZE;++i)ram[i]=(uint8_t)(i*37u+11u);
    for(unsigned i=0;i<10;++i)put32(ram+0x1c4+i*4,words[i]);
    ki_native_pilot_bind(p,ram,KI_PILOT_RAM_SIZE);
    for(unsigned i=0;i<KI_PILOT_GPR_COUNT;++i)p->cpu.gpr[i]=UINT64_C(0x1111000000000000)+i;
    for(unsigned i=0;i<KI_PILOT_FPR_COUNT;++i)p->cpu.fpr[i]=UINT64_C(0x2222000000000000)+i;
    p->cpu.gpr[0]=0;p->cpu.hi=UINT64_C(0x3333000000000001);
    p->cpu.lo=UINT64_C(0x4444000000000002);p->cpu.status_register=0x55667788;
    p->cpu.fcc=true;p->cpu.fcc_valid=true;p->cpu.pc=0x880001c4;
}

int main(void)
{
    uint8_t *ram=malloc(KI_PILOT_RAM_SIZE),*before=malloc(KI_PILOT_RAM_SIZE);
    KiNativePilot p;
    const KiPilotRegionDescriptor *d=
        ki_native_pilot_region_descriptor(KI_PILOT_REGION_STARTUP_CLEAR);
    if(!ram||!before||!d||d->entry_pc!=0x880001c4||d->exit_pc!=0x880001ec||
       d->source_word_count!=10||ki_native_pilot_region_descriptor((KiPilotRegionId)99))return 1;
    seed(&p,ram);memcpy(before,ram,KI_PILOT_RAM_SIZE);
    if(!ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR)||
       ki_native_pilot_advance(&p,17287)!=KI_PILOT_STOP_UNKNOWN_PC||
       p.instruction_count!=17287||p.cpu.pc!=0x880001ec||p.cpu.delay_pending||
       p.service_input_cursor||p.service_output_count||p.invocation_sequence)return 2;
    for(size_t i=0x84c00;i<0x90000;++i)if(ram[i])return 3;
    for(size_t i=0;i<KI_PILOT_RAM_SIZE;++i)
        if((i<0x84c00||i>=0x90000)&&!(i>=0x87260&&i<0x87264)&&ram[i]!=before[i])return 4;
    for(unsigned i=5;i<32;++i)if(p.cpu.gpr[i]!=UINT64_C(0x1111000000000000)+i)return 5;
    for(unsigned i=0;i<32;++i)if(p.cpu.fpr[i]!=UINT64_C(0x2222000000000000)+i)return 6;
    if(p.cpu.hi!=UINT64_C(0x3333000000000001)||p.cpu.lo!=UINT64_C(0x4444000000000002)||
       p.cpu.status_register!=0x55667788||!p.cpu.fcc||!p.cpu.fcc_valid)return 7;

    seed(&p,ram);if(!ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR)||
       ki_native_pilot_advance(&p,17286)!=KI_PILOT_STOP_BUDGET||p.instruction_count!=17286)return 8;
    seed(&p,ram);p.cpu.pc+=4;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 9;
    seed(&p,ram);p.cpu.delay_pending=true;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 10;
    seed(&p,ram);if(ki_native_pilot_begin_region(&p,(KiPilotRegionId)99))return 11;
    seed(&p,ram);ram[0x1c4]^=1;if(!ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR)||
       ki_native_pilot_advance(&p,17287)!=KI_PILOT_STOP_CODE_IDENTITY||p.instruction_count)return 12;
    seed(&p,ram);p.service_input_count=1;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 13;
    seed(&p,ram);p.service_input_cursor=1;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 14;
    seed(&p,ram);p.service_output_count=1;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 15;
    seed(&p,ram);p.service_output_capacity=1;if(ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 16;
    seed(&p,ram);ram[0x2060]^=1;
    if(!ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR)||
       ki_native_pilot_advance(&p,17287)!=KI_PILOT_STOP_UNKNOWN_PC)return 17;
    seed(&p,ram);if(!ki_native_pilot_begin_region(&p,KI_PILOT_REGION_STARTUP_CLEAR))return 18;
    size_t n=ki_native_pilot_snapshot_size();uint8_t *snapshot=malloc(n);
    if(!snapshot||ki_native_pilot_snapshot_save(&p,snapshot,n)!=KI_PILOT_SNAPSHOT_BOUNDARY)return 19;
    p.status=KI_PILOT_COMPLETE_ACTIVE;
    if(ki_native_pilot_snapshot_save(&p,snapshot,n)!=KI_PILOT_SNAPSHOT_BOUNDARY)return 20;
    free(snapshot);
    puts("static region PASS");free(before);free(ram);return 0;
}
