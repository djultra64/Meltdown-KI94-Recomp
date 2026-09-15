#!/usr/bin/env python3
"""Compare the retained original frame matrix with the bounded native slice."""
from __future__ import annotations
import argparse,ctypes,importlib.util,re,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def load(name:str,path:str):
    spec=importlib.util.spec_from_file_location(name,ROOT/path);m=importlib.util.module_from_spec(spec);assert spec.loader;spec.loader.exec_module(m);return m
CMP=load("pilot_compare","tools/compare_native_pilot_oracle.py")
ORACLE=load("frame_oracle","tools/frame_selection_oracle.py")

def contexts(log:str,phase:str)->dict[int,dict[str,int]]:
    found={}
    for match in re.finditer(rf"^KI_FRAME phase={phase} case=([0-9A-F]+) (.*)$",log,re.MULTILINE):
        case=int(match.group(1),16)
        if case in found:raise ValueError("duplicate frame marker")
        found[case]={k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-F]{16})",match.group(2))}
    required=set(CMP.REGISTER_NAMES[1:])|{"hi","lo","pc"}
    if set(found)!=set(range(1,8)) or any(set(v)!=required for v in found.values()):raise ValueError(f"incomplete {phase} contexts")
    return found

def validate_entry_context(entry:dict[int,dict[str,int]])->None:
    for number in range(1,8):
        for reg,value in entry[number].items():
            expected=(0x5a5a000000000000|CMP.REGISTER_NAMES.index(reg)) if reg in CMP.REGISTER_NAMES[1:] else None
            if reg=="v1":expected=0x11
            elif reg=="a1":expected=0x1a
            elif reg=="gp":expected=4
            elif reg=="sp":expected=0xffffffff88087300
            elif reg=="fp":expected=0xffffffff8808c000
            elif reg=="ra":expected=0xffffffff88001898
            elif reg=="hi":expected=0x1111222233334444
            elif reg=="lo":expected=0xaaaabbbbccccdddd
            elif reg=="pc":continue # Exact script binding controls the immediate redirect.
            if value!=expected:raise ValueError(f"case {number}: changed entry {reg}")

def compare(directory:Path)->None:
    script=(directory/"oracle.cmd").read_text()
    if script!=ORACLE.render(directory.relative_to(ROOT)):raise ValueError("frame oracle script identity changed")
    log=(directory/"run.log").read_text();markers=re.findall(r"^KI_FRAME (.*)$",log,re.MULTILINE)
    if len(markers)!=21 or any(not re.match(r"phase=(entry|complete|scope_end) case=[1-7] ",x) for x in markers):raise ValueError("malformed or extra frame marker")
    entry,complete,scope=contexts(log,"entry"),contexts(log,"complete"),contexts(log,"scope_end")
    validate_entry_context(entry)
    if complete!=scope:raise ValueError("caller continuation executed after frame stop")
    with tempfile.TemporaryDirectory(prefix="ki-frame-") as temp:
        lib=CMP.build_library(Path(temp)/"pilot.so")
        lib.ki_native_pilot_begin_frame.argtypes=[ctypes.POINTER(CMP.Pilot)];lib.ki_native_pilot_begin_frame.restype=ctypes.c_bool
        def execute(seed:bytes):
            ram=(ctypes.c_uint8*len(seed)).from_buffer_copy(seed);pilot=CMP.Pilot();lib.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(seed))
            initialize_cpu(pilot)
            if not lib.ki_native_pilot_begin_frame(ctypes.byref(pilot)):raise ValueError("begin frame rejected")
            return lib.ki_native_pilot_advance(ctypes.byref(pilot),2000),pilot,ram
        def initialize_cpu(pilot:CMP.Pilot)->None:
            for i in range(1,32):pilot.cpu.gpr[i]=0x5a5a000000000000|i
            pilot.cpu.gpr[3]=0x11;pilot.cpu.gpr[5]=0x1a;pilot.cpu.gpr[28]=4;pilot.cpu.gpr[29]=0xffffffff88087300;pilot.cpu.gpr[30]=0xffffffff8808c000;pilot.cpu.gpr[31]=0xffffffff88001898
            pilot.cpu.hi=0x1111222233334444;pilot.cpu.lo=0xaaaabbbbccccdddd
        fallback=None
        for number,(name,*_) in enumerate(ORACLE.CASES,1):
            seed=(directory/f"{number:02d}-{name}-before.bin").read_bytes()
            status,pilot,ram=execute(seed)
            if status!=15:raise ValueError(f"case {number}: status {status} pc=0x{pilot.cpu.pc:x}")
            expected_ram=(directory/f"{number:02d}-{name}-after.bin").read_bytes()
            if bytes(ram)!=expected_ram:
                at=next(i for i,(a,b) in enumerate(zip(ram,expected_ram)) if a!=b);raise ValueError(f"case {number}: RAM differs at 0x{at:x}")
            values={key:pilot.cpu.gpr[i] for i,key in enumerate(CMP.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,pc=pilot.cpu.pc)
            diff=[key for key in values if values[key]!=scope[number][key]]
            if diff:raise ValueError(f"case {number}: context differs: {','.join(diff)}")
            if name=="fallback":fallback=(pilot,ram)
        assert fallback
        left,left_ram=fallback;size=lib.ki_native_pilot_snapshot_size();snapshot=(ctypes.c_uint8*size)();right_ram=(ctypes.c_uint8*0x100000)();right=CMP.Pilot();lib.ki_native_pilot_bind(ctypes.byref(right),right_ram,len(right_ram))
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(left),snapshot,size)!=0 or lib.ki_native_pilot_snapshot_restore(ctypes.byref(right),snapshot,size)!=0:raise ValueError("frame snapshot failed")
        for pilot in (left,right):
            initialize_cpu(pilot)
            if not lib.ki_native_pilot_begin_frame(ctypes.byref(pilot)) or lib.ki_native_pilot_advance(ctypes.byref(pilot),2000)!=15:raise ValueError("restored frame replay failed")
        if bytes(left_ram)!=bytes(right_ram) or bytes(left.cpu)!=bytes(right.cpu):raise ValueError("mutable-root replay diverged")
    print("frame selector original/native replay passed: 7 executions")
def main()->int:
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("directory",type=Path);a=p.parse_args();compare(a.directory if a.directory.is_absolute() else ROOT/a.directory);return 0
if __name__=="__main__":raise SystemExit(main())
