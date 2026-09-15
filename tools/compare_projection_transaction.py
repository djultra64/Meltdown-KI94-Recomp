#!/usr/bin/env python3
"""Replay the retained projection-A state through the finite native transaction."""
from __future__ import annotations
import ctypes,hashlib,importlib.util,re,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];RUN=ROOT/"work/tasks/POC-120C/projection-a"
spec=importlib.util.spec_from_file_location("base",ROOT/"tools/compare_native_pilot_oracle.py");BASE=importlib.util.module_from_spec(spec);spec.loader.exec_module(BASE)
HASHES={"run.log":"a402220b699ee16f99d87d8525d61685848f3112e6257d9fd8c60637cb84c9e4","main-before.bin":"8f38b11a96d22d3b4d9e661bae9d0fa468af3b7a8fb56db0d8e989d9d2fd71cf","projection.trace":"26e4f5073363e56dd472003bce719ba7223ba0ade52fcb451d22139a9a73419c","main-after.bin":"1c1968892c1c149703cd2314202afcd7fb7ef9bc94443848f2793dda2c263ea0"}
def context(log,phase):
    line=re.search(rf"^KI_PROJECTION_FULL phase={phase} .*?$",log,re.MULTILINE)
    if not line:raise ValueError(f"missing {phase}")
    return {k:int(v,16) for k,v in re.findall(r"([a-z][a-z0-9]*)=([0-9A-Fa-f]{8,16})",line.group())}
def main():
    for name,want in HASHES.items():
        if hashlib.sha256((RUN/name).read_bytes()).hexdigest()!=want:raise ValueError(f"retained {name} changed")
    log=(RUN/"run.log").read_text();before=context(log,"before");after=context(log,"after")
    with tempfile.TemporaryDirectory(prefix="ki-projection-") as temp:
        lib=BASE.build_library(Path(temp)/"pilot.so");lib.ki_native_pilot_begin_projection.argtypes=[ctypes.POINTER(BASE.Pilot)];lib.ki_native_pilot_begin_projection.restype=ctypes.c_bool
        expected=(RUN/"main-after.bin").read_bytes();ram=(ctypes.c_uint8*0x100000).from_buffer_copy((RUN/"main-before.bin").read_bytes());pilot=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(ram))
        for i,name in enumerate(BASE.REGISTER_NAMES):
            if i:pilot.cpu.gpr[i]=before[name]
        pilot.cpu.hi=before["hi"];pilot.cpu.lo=before["lo"];pilot.cpu.status_register=before["sr"]
        for i in range(32):pilot.cpu.fpr[i]=before[f"fpr{i}"]
        if not lib.ki_native_pilot_begin_projection(ctypes.byref(pilot)):raise ValueError("projection begin rejected")
        host=ctypes.CDLL(None);host.fegetround.restype=ctypes.c_int;host.fesetround.argtypes=[ctypes.c_int];host.feclearexcept.argtypes=[ctypes.c_int];host.feraiseexcept.argtypes=[ctypes.c_int];host.fetestexcept.argtypes=[ctypes.c_int]
        original_round=host.fegetround()
        if host.fesetround(0x800)!=0:raise ValueError("host upward-rounding test unavailable")
        host.feclearexcept(0x3d);host.feraiseexcept(0x01)
        start_instructions=pilot.instructions
        status=lib.ki_native_pilot_advance(ctypes.byref(pilot),10000)
        first_run_instructions=pilot.instructions-start_instructions
        if host.fegetround()!=0x800:raise ValueError("projection changed host rounding mode")
        if host.fetestexcept(0x3d)!=0x01:raise ValueError("projection changed host FP flags")
        host.feclearexcept(0x3d)
        host.fesetround(original_round)
        if status!=22:raise ValueError(f"status {status} pc={pilot.cpu.pc:08x} detail={pilot.stop_detail:08x}")
        if bytes(ram)!=expected:
            at=next(i for i,(a,b) in enumerate(zip(ram,expected)) if a!=b);raise ValueError(f"RAM differs at {at:05x}: {ram[at]:02x}!={expected[at]:02x}")
        values={name:pilot.cpu.gpr[i] for i,name in enumerate(BASE.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,sr=pilot.cpu.status_register,pc=pilot.cpu.pc);values.update({f"fpr{i}":pilot.cpu.fpr[i] for i in range(32)})
        diff=[key for key,value in values.items() if value!=after[key]]
        if diff:raise ValueError("context differs: "+",".join(diff))
        lib.ki_native_pilot_snapshot_size.restype=ctypes.c_size_t;size=lib.ki_native_pilot_snapshot_size();snapshot=(ctypes.c_uint8*size)();lib.ki_native_pilot_snapshot_save.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t];lib.ki_native_pilot_snapshot_restore.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(pilot),snapshot,size)!=0:raise ValueError("projection snapshot rejected")
        replay_ram=(ctypes.c_uint8*0x100000)();replay=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(replay),replay_ram,len(replay_ram))
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(replay),snapshot,size)!=0 or bytes(replay_ram)!=expected:raise ValueError("projection snapshot restore failed")
        for item,item_ram in ((pilot,ram),(replay,replay_ram)):
            item_ram[:]=(RUN/"main-before.bin").read_bytes()
            for i,name in enumerate(BASE.REGISTER_NAMES):
                if i:item.cpu.gpr[i]=before[name]
            item.cpu.hi=before["hi"];item.cpu.lo=before["lo"];item.cpu.status_register=before["sr"]
            for i in range(32):item.cpu.fpr[i]=before[f"fpr{i}"]
            if not lib.ki_native_pilot_begin_projection(ctypes.byref(item)) or lib.ki_native_pilot_advance(ctypes.byref(item),10000)!=22:raise ValueError("projection resimulation failed")
        if bytes(replay_ram)!=bytes(ram) or bytes(replay.cpu)!=bytes(pilot.cpu):raise ValueError("projection restore/resimulation diverged")
        def rejected(change_ram=lambda value:None,change_cpu=lambda value:None):
            data=bytearray((RUN/"main-before.bin").read_bytes());change_ram(data);buf=(ctypes.c_uint8*len(data)).from_buffer_copy(data);item=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(item),buf,len(buf))
            for i,name in enumerate(BASE.REGISTER_NAMES):
                if i:item.cpu.gpr[i]=before[name]
            item.cpu.hi=before["hi"];item.cpu.lo=before["lo"];item.cpu.status_register=before["sr"]
            for i in range(32):item.cpu.fpr[i]=before[f"fpr{i}"]
            change_cpu(item)
            if not lib.ki_native_pilot_begin_projection(ctypes.byref(item)):raise ValueError("negative begin rejected")
            return lib.ki_native_pilot_advance(ctypes.byref(item),10000)
        if rejected(change_ram=lambda data:data.__setitem__(0x1038,data[0x1038]^1))!=14:raise ValueError("changed projection source accepted")
        if rejected(change_cpu=lambda item:item.cpu.fpr.__setitem__(11,0x00000001))!=18:raise ValueError("subnormal operand accepted")
        if rejected(change_cpu=lambda item:item.cpu.fpr.__setitem__(11,0x7f800000))!=18:raise ValueError("non-finite operand accepted")
        if rejected(change_cpu=lambda item:item.cpu.fpr.__setitem__(11,0x100000000))!=13:raise ValueError("upper FPR lane accepted")
        if rejected(change_cpu=lambda item:item.cpu.gpr.__setitem__(29,0xffffffff88001050))!=13:raise ValueError("source-overlapping stack accepted")
        print(f"projection-A original/native replay passed ({first_run_instructions} instructions)")
    return 0
if __name__=="__main__":raise SystemExit(main())
