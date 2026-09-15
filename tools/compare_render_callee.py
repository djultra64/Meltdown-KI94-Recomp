#!/usr/bin/env python3
"""Compare retained callee-A with the bounded original renderer execution."""
from __future__ import annotations
import ctypes,importlib.util,re,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];RUN=ROOT/"work/tasks/POC-120B3/callee-a"
spec=importlib.util.spec_from_file_location("base",ROOT/"tools/compare_native_pilot_oracle.py");BASE=importlib.util.module_from_spec(spec);spec.loader.exec_module(BASE)
def context(log,phase):
    line=re.search(rf"^KI_RENDER_FULL phase={phase} .*?$",log,re.MULTILINE)
    if not line:raise ValueError(f"missing {phase}")
    return {k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-Fa-f]{8,16})",line.group())}
def main()->int:
    log=(RUN/"run.log").read_text();before=context(log,"before");after=context(log,"after")
    with tempfile.TemporaryDirectory(prefix="ki-render-") as temp:
        lib=BASE.build_library(Path(temp)/"pilot.so");lib.ki_native_pilot_bind_low_ram.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.POINTER(ctypes.c_uint8),ctypes.c_size_t];lib.ki_native_pilot_bind_low_ram.restype=ctypes.c_bool;lib.ki_native_pilot_begin_render.argtypes=[ctypes.POINTER(BASE.Pilot)];lib.ki_native_pilot_begin_render.restype=ctypes.c_bool
        main=(ctypes.c_uint8*0x100000).from_buffer_copy((RUN/"main-before.bin").read_bytes());low=(ctypes.c_uint8*0x80000).from_buffer_copy((RUN/"low-before.bin").read_bytes());pilot=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(pilot),main,len(main));
        if not lib.ki_native_pilot_bind_low_ram(ctypes.byref(pilot),low,len(low)):raise ValueError("low RAM bind rejected")
        for i,name in enumerate(BASE.REGISTER_NAMES):
            if i:pilot.cpu.gpr[i]=before[name]
        pilot.cpu.hi=before["hi"];pilot.cpu.lo=before["lo"];pilot.cpu.status_register=before["sr"]
        if not lib.ki_native_pilot_begin_render(ctypes.byref(pilot)):raise ValueError("render begin rejected")
        lib.ki_native_pilot_snapshot_size.restype=ctypes.c_size_t;size=lib.ki_native_pilot_snapshot_size();snapshot=(ctypes.c_uint8*size)();lib.ki_native_pilot_snapshot_save.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t];lib.ki_native_pilot_snapshot_restore.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        # Render READY is not generally snapshottable, so retain the full input
        # banks here and verify a completed-boundary rebind below.
        status=lib.ki_native_pilot_advance(ctypes.byref(pilot),20000)
        if status!=21:raise ValueError(f"status {status} pc={pilot.cpu.pc:08x} detail={pilot.stop_detail:08x}")
        if bytes(main)!=(RUN/"main-after.bin").read_bytes():
            expected=(RUN/"main-after.bin").read_bytes();at=next(i for i,(a,b) in enumerate(zip(main,expected)) if a!=b);raise ValueError(f"main differs at {at:05x}")
        if bytes(low)!=(RUN/"low-after.bin").read_bytes():
            expected=(RUN/"low-after.bin").read_bytes();at=next(i for i,(a,b) in enumerate(zip(low,expected)) if a!=b);raise ValueError(f"low differs at {at:05x}: {low[at]:02x}!={expected[at]:02x}")
        values={name:pilot.cpu.gpr[i] for i,name in enumerate(BASE.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,sr=pilot.cpu.status_register,pc=pilot.cpu.pc)
        diff=[key for key,value in values.items() if value!=after[key]]
        if diff:raise ValueError(f"context differs: {','.join(diff)}")
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(pilot),snapshot,size)!=0:raise ValueError("completed render snapshot rejected")
        absent_main=(ctypes.c_uint8*0x100000)();absent=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(absent),absent_main,len(absent_main))
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(absent),snapshot,size)!=3 or any(absent_main):raise ValueError("present-low snapshot crossed into absent domain")
        rebound_main=(ctypes.c_uint8*0x100000)();rebound_low=(ctypes.c_uint8*0x80000)();rebound=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(rebound),rebound_main,len(rebound_main));lib.ki_native_pilot_bind_low_ram(ctypes.byref(rebound),rebound_low,len(rebound_low))
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(rebound),snapshot,size)!=0 or bytes(rebound_main)!=bytes(main) or bytes(rebound_low)!=bytes(low) or bytes(rebound.cpu)!=bytes(pilot.cpu):raise ValueError("two-bank render snapshot rebind failed")
        for item in (pilot,rebound):
            # The original caller supplies the same entry context for a repeat.
            for i,name in enumerate(BASE.REGISTER_NAMES):
                if i:item.cpu.gpr[i]=before[name]
            item.cpu.hi=before["hi"];item.cpu.lo=before["lo"]
            if not lib.ki_native_pilot_begin_render(ctypes.byref(item)) or lib.ki_native_pilot_advance(ctypes.byref(item),20000)!=21:raise ValueError("restored render repeat failed")
        if bytes(rebound_main)!=bytes(main) or bytes(rebound_low)!=bytes(low) or bytes(rebound.cpu)!=bytes(pilot.cpu):raise ValueError("two-bank render resimulation diverged")
        absent.status=1;absent_snapshot=(ctypes.c_uint8*size)()
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(absent),absent_snapshot,size)!=0:raise ValueError("absent-low domain snapshot failed")
        cross_main=(ctypes.c_uint8*0x100000)();cross_low=(ctypes.c_uint8*0x80000)();cross=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(cross),cross_main,len(cross_main));lib.ki_native_pilot_bind_low_ram(ctypes.byref(cross),cross_low,len(cross_low))
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(cross),absent_snapshot,size)!=3 or any(cross_main) or any(cross_low):raise ValueError("absent-low snapshot crossed into present domain")
        def rejected(change,bind_low=True,cpu_change=None):
            ram=bytearray((RUN/"main-before.bin").read_bytes());change(ram)
            test_main=(ctypes.c_uint8*len(ram)).from_buffer_copy(ram);test_low=(ctypes.c_uint8*0x80000).from_buffer_copy((RUN/"low-before.bin").read_bytes());test=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(test),test_main,len(test_main))
            if bind_low:lib.ki_native_pilot_bind_low_ram(ctypes.byref(test),test_low,len(test_low))
            for i,name in enumerate(BASE.REGISTER_NAMES):
                if i:test.cpu.gpr[i]=before[name]
            test.cpu.hi=before["hi"];test.cpu.lo=before["lo"]
            if cpu_change:cpu_change(test)
            if not lib.ki_native_pilot_begin_render(ctypes.byref(test)):raise ValueError("negative begin failed")
            before_code=bytes(test_main[0x1b90:0x1ba0]);status=lib.ki_native_pilot_advance(ctypes.byref(test),20000)
            if bytes(test_main[0x1b90:0x1ba0])!=before_code:raise ValueError("rejected render changed enrolled source")
            return status
        if rejected(lambda ram:None,False)!=13:raise ValueError("missing low RAM accepted")
        if rejected(lambda ram:ram.__setitem__(0x8c024,0x82))!=13:raise ValueError("unsupported facing accepted")
        if rejected(lambda ram:ram.__setitem__(slice(0x8c030,0x8c034),(0x88097671).to_bytes(4,"little")))!=13:raise ValueError("unaligned frame accepted")
        if rejected(lambda ram:ram.__setitem__(slice(0x35630,0x35634),(0).to_bytes(4,"little"))) not in (9,10):raise ValueError("unknown render table target accepted")
        if rejected(lambda ram:ram.__setitem__(slice(0x35630,0x35634),(0x88001ae4).to_bytes(4,"little")))!=10:raise ValueError("terminal render table bypass accepted")
        if rejected(lambda ram:ram.__setitem__(slice(0x35630,0x35634),(0x8801187c).to_bytes(4,"little")))!=10:raise ValueError("wrong enrolled render target accepted")
        if rejected(lambda ram:None,True,
                    lambda p:p.cpu.gpr.__setitem__(29,0xffffffff88001b98))!=13:
            raise ValueError("source-overlapping render stack accepted")
    print("renderer callee-A original/native replay passed")
    return 0
if __name__=="__main__":raise SystemExit(main())
