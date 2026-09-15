#include "ki/frame_asset.h"
#include "ki/native_pilot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_at(uint8_t *ram, size_t offset, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    size_t got = fread(ram + offset, 1, KI_PILOT_RAM_SIZE - offset, file);
    int ok = !ferror(file) && feof(file) && got != 0;
    fclose(file); return ok;
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}

static void write32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}

static void cpu(KiNativePilot *pilot)
{
    for (unsigned i=1;i<32;++i) pilot->cpu.gpr[i]=UINT64_C(0x5a5a000000000000)|i;
    pilot->cpu.gpr[3]=0x11; pilot->cpu.gpr[5]=0x1a;
    pilot->cpu.gpr[28]=4; pilot->cpu.gpr[29]=UINT64_C(0xffffffff88087300);
    pilot->cpu.gpr[30]=UINT64_C(0xffffffff8808c000);
    pilot->cpu.gpr[31]=UINT64_C(0xffffffff88001898);
    pilot->cpu.hi=UINT64_C(0x1111222233334444);
    pilot->cpu.lo=UINT64_C(0xaaaabbbbccccdddd);
}

int main(void)
{
    uint8_t *ram=calloc(KI_PILOT_RAM_SIZE,1), *original=malloc(KI_PILOT_RAM_SIZE);
    if (!ram || !original || !load_at(ram,0,"work/kipack/ki15d/rom-0.bin") ||
        !load_at(ram,0x33900,"work/kipack/ki15d/rom-1.bin")) return 1;
    KiMemory memory={.main_ram=ram,.main_ram_size=KI_PILOT_RAM_SIZE};
    KiFrameAssetResult asset_result=ki_frame_asset_load(&memory,"work/assets/ki15d-boot-graphics/sector-0aaa-count-010a.bin");
    if (asset_result != KI_FRAME_ASSET_OK)
        return fprintf(stderr,"asset load failed: %d\n",asset_result),1;
    ram[0x8c000]=0x1a; ram[0x8c014]=4; ram[0x8c034]=0x0a;
    memcpy(original,ram,KI_PILOT_RAM_SIZE);
    KiNativePilot pilot; ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot);
    if (!ki_native_pilot_begin_frame(&pilot) || ki_native_pilot_advance(&pilot,2000)!=KI_PILOT_COMPLETE_FRAME ||
        read32(ram+0x861f8)!=0x88097773 || read32(ram+0x8c030)!=0x88097670 ||
        read32(ram+0x8c0d0)!=0x88097773)
        return fprintf(stderr,"authentic frame failed status=%s pc=%08x detail=%08x\n",
                       ki_native_pilot_status_name(pilot.status),(unsigned)pilot.cpu.pc,(unsigned)pilot.stop_detail),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); write32(ram+0x90140,0x880f0000);
    ram[0x8c014]=3; ram[0xf0000]=1; ram[0xf0002]=1; write32(ram+0xf0004,0x40); write32(ram+0xf0008,0x20);
    ram[0xf0040]=0x0a; ram[0xf0042]=3; write32(ram+0xf0044,0x80);
    write32(ram+0xf0048,0x20); write32(ram+0xf004c,0x80); write32(ram+0xf0050,0x80);
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,2000)!=KI_PILOT_COMPLETE_FRAME || read32(ram+0x8c030)!=0x880f0060)
        return fprintf(stderr,"synthetic fallback failed status=%s pc=%08x detail=%08x frame=%08x\n",
                       ki_native_pilot_status_name(pilot.status),(unsigned)pilot.cpu.pc,
                       (unsigned)pilot.stop_detail,read32(ram+0x8c030)),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); write32(ram+0x90140,0x880f0002);
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,2000)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"unaligned root accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); ram[0x3420a]=0x70;
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"unsupported class accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); ram[0x8c000]=0x15;
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"wrong record type accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); write32(ram+0x94e04,0);
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"zero-length row accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); ram[0x97616]=0;
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"zero frame count accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE); write32(ram+0x97628,0x2108);
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"out-of-row frame offset accepted\n"),1;
    memcpy(ram,original,KI_PILOT_RAM_SIZE);
    ki_native_pilot_bind(&pilot,ram,KI_PILOT_RAM_SIZE); cpu(&pilot); pilot.cpu.gpr[31]=0xffffffff8800189c; ki_native_pilot_begin_frame(&pilot);
    if (ki_native_pilot_advance(&pilot,8)!=KI_PILOT_STOP_PRECONDITION)
        return fprintf(stderr,"undeclared return accepted\n"),1;
    uint8_t *bad=malloc(KI_FRAME_ASSET_SIZE); if(!bad)return 1;
    FILE *file=fopen("work/assets/ki15d-boot-graphics/sector-0aaa-count-010a.bin","rb");
    if(!file||fread(bad,1,KI_FRAME_ASSET_SIZE,file)!=KI_FRAME_ASSET_SIZE)return 1;
    fclose(file); bad[1]^=1; memset(ram+0x90100,0x6d,KI_FRAME_ASSET_SIZE);
    if(ki_frame_asset_load_bytes(&memory,bad,KI_FRAME_ASSET_SIZE)!=KI_FRAME_ASSET_IDENTITY || ram[0x90100]!=0x6d)
        return fprintf(stderr,"asset identity was not atomic\n"),1;
    free(bad); free(original); free(ram);
    puts("frame selection pilot focused gates passed"); return 0;
}
