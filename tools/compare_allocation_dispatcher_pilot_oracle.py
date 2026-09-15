#!/usr/bin/env python3
"""Run and compare the six allocation-inclusive dispatcher cases."""

from __future__ import annotations

import argparse,ctypes,importlib.util,re,tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def module(name: str, path: str):
    spec=importlib.util.spec_from_file_location(name,ROOT/path);value=importlib.util.module_from_spec(spec);assert spec.loader is not None;spec.loader.exec_module(value);return value
CMP=module("pilot_compare","tools/compare_native_pilot_oracle.py")
BASE=module("resumed_compare","tools/compare_resumed_dispatcher_pilot_oracle.py")
ORACLE=module("allocation_oracle","tools/allocation_dispatcher_pilot_oracle.py")

def contexts(log: str, phase: str) -> dict[int,dict[str,int]]:
    result={}
    for match in re.finditer(rf"^KI_ALLOC phase={phase} case=([0-9A-F]+) (.*)$",log,re.MULTILINE):
        case=int(match.group(1),16)
        if case in result: raise ValueError("duplicate marker")
        result[case]={k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-F]{16})",match.group(2))}
    required=set(CMP.REGISTER_NAMES[1:])|{"hi","lo","pc"}
    if set(result)!=set(range(1,7)) or any(set(v)!=required for v in result.values()): raise ValueError("incomplete allocation matrix")
    return result

def validate_script(script: str, directory: Path) -> None:
    if script != ORACLE.render(directory.relative_to(ROOT)):
        raise ValueError("allocation oracle script identity changed")

def compare(directory: Path) -> None:
    script=(directory/"oracle.cmd").read_text()
    validate_script(script,directory)
    blocks=[script.split(f"# case {n}: {name}",1)[1].split("# case ",1)[0] for n,name in enumerate(ORACLE.NAMES,1)]
    captured=(directory/"run.log").exists()
    if captured:
        log=(directory/"run.log").read_text();markers=re.findall(r"^KI_ALLOC (.*)$",log,re.MULTILINE)
        if len(markers)!=18 or any(not re.match(r"phase=(entry|complete|scope_end) case=[1-6] ",line) for line in markers):raise ValueError("malformed or extra allocation marker")
        expected_context=contexts(log,"scope_end");BASE.validate_entry_context(contexts(log,"entry"),False);contexts(log,"complete")
    else:expected_context={}
    with tempfile.TemporaryDirectory(prefix="ki-allocation-") as temporary:
        library=CMP.build_library(Path(temporary)/"pilot.so");library.ki_native_pilot_begin_dispatch.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_uint64];library.ki_native_pilot_begin_dispatch.restype=ctypes.c_bool
        def execute(seed: bytes):
            ram=(ctypes.c_uint8*len(seed)).from_buffer_copy(seed);pilot=CMP.Pilot();library.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(seed))
            for index in range(1,32):pilot.cpu.gpr[index]=0x5a5a000000000000|index
            pilot.cpu.gpr[3]=0x1122334487654321;pilot.cpu.gpr[29]=0xffffffff88087300;pilot.cpu.gpr[31]=0xffffffff880009a4;pilot.cpu.hi=0x1111222233334444;pilot.cpu.lo=0xaaaabbbbccccdddd
            if not library.ki_native_pilot_begin_dispatch(ctypes.byref(pilot),0xffffffff880009a4):raise ValueError("begin rejected")
            return library.ki_native_pilot_advance(ctypes.byref(pilot),100000),pilot,ram
        replay_boundaries=[];seeds=[]
        for number,(name,block) in enumerate(zip(ORACLE.NAMES,blocks),1):
            seed=BASE.seed_from_block(block)
            seeds.append(seed)
            if captured and seed!=(directory/f"{number:02d}-{name}-before.bin").read_bytes(): raise ValueError(f"case {number}: before-image differs")
            status,pilot,ram=execute(seed)
            if status!=5:raise ValueError(f"case {number}: status {status} pc=0x{pilot.cpu.pc:x} detail=0x{pilot.stop_detail:x}")
            if captured:
                actual=bytes(ram);expected=(directory/f"{number:02d}-{name}-after.bin").read_bytes()
                if actual!=expected:
                    offset=next(i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b);raise ValueError(f"case {number}: RAM 0x{offset:x} {actual[offset]:02x}!={expected[offset]:02x}")
                values={key:pilot.cpu.gpr[i] for i,key in enumerate(CMP.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,pc=pilot.cpu.pc)
                diff=[key for key in values if values[key]!=expected_context[number][key]]
                if diff:raise ValueError(f"case {number}: register differences {','.join(diff)}")
            if number in (1,4): replay_boundaries.append((pilot,ram))
        for offset in (0xb2d8,0x5e370,0x5e6f2):
            changed=bytearray(seeds[0]);changed[offset]^=1
            status,pilot,_=execute(bytes(changed))
            if status!=14 or (pilot.stop_pc&0xffffffff)!=0x88000000+offset:
                raise ValueError(f"changed allocation source data accepted at 0x{offset:x}")
        library.ki_native_pilot_snapshot_size.restype=ctypes.c_size_t;size=library.ki_native_pilot_snapshot_size()
        library.ki_native_pilot_snapshot_save.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        library.ki_native_pilot_snapshot_restore.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        for history,(left,left_ram) in enumerate(replay_boundaries,1):
            snapshot=(ctypes.c_uint8*size)();right_ram=(ctypes.c_uint8*0x100000)();right=CMP.Pilot();library.ki_native_pilot_bind(ctypes.byref(right),right_ram,len(right_ram))
            if library.ki_native_pilot_snapshot_save(ctypes.byref(left),snapshot,size)!=0 or library.ki_native_pilot_snapshot_restore(ctypes.byref(right),snapshot,size)!=0:raise ValueError(f"replay {history}: snapshot failure")
            for pilot in (left,right):
                if not library.ki_native_pilot_begin_dispatch(ctypes.byref(pilot),0xffffffff880009a4) or library.ki_native_pilot_advance(ctypes.byref(pilot),100000)!=5:raise ValueError(f"replay {history}: next update failed")
            if bytes(left_ram)!=bytes(right_ram) or bytes(left.cpu)!=bytes(right.cpu):raise ValueError(f"replay {history}: restored update diverged")
    print(f"allocation dispatcher {'oracle replay' if captured else 'native preflight'} passed: 6 cases")

def main()->int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path);args=parser.parse_args();compare(args.directory if args.directory.is_absolute() else ROOT/args.directory);return 0
if __name__=="__main__":raise SystemExit(main())
