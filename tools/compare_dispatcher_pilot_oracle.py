#!/usr/bin/env python3
"""Replay native whole-dispatch execution against the six-case original matrix."""

from __future__ import annotations

import argparse
import ctypes
import importlib.util
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("pilot_compare", ROOT / "tools/compare_native_pilot_oracle.py")
CMP = importlib.util.module_from_spec(SPEC); assert SPEC.loader is not None; SPEC.loader.exec_module(CMP)
NAMES = ("empty","paused_fighters","mixed_replace","smaller_replace","signed_small","terminal")

def contexts(log: str) -> dict[int, dict[str,int]]:
    if re.search(r"(?i)(fatal error|unknown command|invalid debugger|traceback)",log): raise ValueError("MAME log error")
    result={}
    for match in re.finditer(r"^KI_DISPATCH phase=scope_end case=([0-9A-F]+) (.*)$",log,re.MULTILINE):
        case=int(match.group(1),16)
        if case in result: raise ValueError("duplicate scope_end")
        result[case]={k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-F]{16})",match.group(2))}
    required=set(CMP.REGISTER_NAMES[1:])|{"hi","lo","pc"}
    if set(result)!=set(range(1,7)) or any(set(v)!=required for v in result.values()): raise ValueError("incomplete matrix")
    return result

def compare(directory: Path) -> None:
    expected_context=contexts((directory/"run.log").read_text())
    with tempfile.TemporaryDirectory(prefix="ki-dispatch-") as temporary:
        library=CMP.build_library(Path(temporary)/"pilot.so")
        library.ki_native_pilot_begin_dispatch.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_uint64]
        library.ki_native_pilot_begin_dispatch.restype=ctypes.c_bool
        for number,name in enumerate(NAMES,1):
            before=(directory/f"{number:02d}-{name}-before.bin").read_bytes(); expected=(directory/f"{number:02d}-{name}-after.bin").read_bytes()
            if len(before)!=0x100000 or len(expected)!=0x100000: raise ValueError(f"case {number}: bad snapshot size")
            ram=(ctypes.c_uint8*len(before)).from_buffer_copy(before); pilot=CMP.Pilot(); library.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(before))
            for index in range(1,32): pilot.cpu.gpr[index]=0x5A5A000000000000|index
            pilot.cpu.gpr[3]=0x1122334487654321;pilot.cpu.gpr[29]=0xFFFFFFFF88087300;pilot.cpu.gpr[31]=0xFFFFFFFF880009A4
            pilot.cpu.hi=0x1111222233334444;pilot.cpu.lo=0xAAAABBBBCCCCDDDD
            if not library.ki_native_pilot_begin_dispatch(ctypes.byref(pilot),0xFFFFFFFF880009A4): raise ValueError(f"case {number}: begin rejected")
            status=library.ki_native_pilot_advance(ctypes.byref(pilot),20000)
            if status!=5: raise ValueError(f"case {number}: status {status}, pc=0x{pilot.cpu.pc:x}, detail=0x{pilot.stop_detail:x}")
            actual=bytes(ram)
            if actual!=expected:
                offset=next(i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
                raise ValueError(f"case {number}: first RAM difference 0x{offset:x}: {actual[offset]:02x}!={expected[offset]:02x}")
            values={name:pilot.cpu.gpr[i] for i,name in enumerate(CMP.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,pc=pilot.cpu.pc)
            diff=[key for key in values if values[key]!=expected_context[number][key]]
            if diff: raise ValueError(f"case {number}: register differences {','.join(diff)}")
    print("dispatcher pilot oracle replay passed: 6 cases, full RAM and final context")

def main()->int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path);args=parser.parse_args();compare(args.directory if args.directory.is_absolute() else ROOT/args.directory);return 0
if __name__=="__main__":raise SystemExit(main())
