#!/usr/bin/env python3
"""Run the resumed dispatcher natively and, when captured, compare the original."""

from __future__ import annotations

import argparse
import ctypes
import importlib.util
from pathlib import Path
import re
import tempfile

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("pilot_compare",ROOT/"tools/compare_native_pilot_oracle.py")
CMP=importlib.util.module_from_spec(SPEC);assert SPEC.loader is not None;SPEC.loader.exec_module(CMP)
ORACLE_SPEC=importlib.util.spec_from_file_location("resumed_oracle",ROOT/"tools/resumed_dispatcher_pilot_oracle.py")
ORACLE=importlib.util.module_from_spec(ORACLE_SPEC);assert ORACLE_SPEC.loader is not None;ORACLE_SPEC.loader.exec_module(ORACLE)

def seed_from_block(block: str) -> bytes:
    data=bytearray(0x100000)
    for line in block.splitlines():
        if line.startswith("load "):
            name,address=line[5:].split(",");payload=(ROOT/name).read_bytes();offset=int(address,0)&0xfffff
            data[offset:offset+len(payload)]=payload
        match=re.match(r"do ([bwd])@0x88([0-9a-f]{6})=0x([0-9a-f]+)$",line)
        if match:
            width={"b":1,"w":2,"d":4}[match.group(1)];offset=int(match.group(2),16)
            data[offset:offset+width]=int(match.group(3),16).to_bytes(width,"little")
    return bytes(data)

def contexts(log: str, phase: str) -> dict[int,dict[str,int]]:
    if re.search(r"(?i)(fatal error|unknown command|invalid debugger|traceback)",log): raise ValueError("MAME log error")
    result={}
    for match in re.finditer(rf"^KI_RESUMED phase={phase} case=([0-9A-F]+) (.*)$",log,re.MULTILINE):
        case=int(match.group(1),16)
        if case in result: raise ValueError("duplicate scope_end")
        result[case]={k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-F]{16})",match.group(2))}
    required=set(CMP.REGISTER_NAMES[1:])|{"hi","lo","pc"}
    if set(result)!=set(range(1,7)) or any(set(v)!=required for v in result.values()): raise ValueError("incomplete matrix")
    return result

def validate_script(script: str, directory: Path) -> bool:
    """Accept only the exact current or retained-A pre-redirection rendering."""
    relative=directory.relative_to(ROOT)
    current=ORACLE.render(relative)
    historical=current
    entry=ORACLE.context("entry")
    for number,name in enumerate(ORACLE.NAMES,1):
        save=f"save {relative.as_posix()}/{number:02d}-{name}-before.bin,0x88000000,0x100000"
        historical=historical.replace(
            f"do pc=0xffffffff88002038\n{entry}\n{save}",
            f"{save}\n{entry}\ndo pc=0xffffffff88002038")
    if script not in (current,historical):
        raise ValueError("oracle script differs from an exact supported rendering")
    return script == historical

def validate_entry_context(entry_context: dict[int,dict[str,int]],
                           pre_redirect: bool) -> None:
    expected_pcs=([0x00000000bfc00388]+[0x00000000880009ac]*5) if pre_redirect else [0x0000000088002038]*6
    for case in range(1,7):
        expected={name:0x5a5a000000000000|index for index,name in enumerate(ORACLE.REGS,1)}
        expected.update(v1=0x1122334487654321,sp=0xffffffff88087300,
                        ra=0xffffffff880009a4,hi=0x1111222233334444,
                        lo=0xaaaabbbbccccdddd,pc=expected_pcs[case-1])
        if entry_context[case] != expected:
            raise ValueError(f"case {case}: full pre-entry CPU context differs from declaration")

def compare(directory: Path) -> None:
    script=(directory/"oracle.cmd").read_text();pre_redirect=validate_script(script,directory)
    blocks=[script.split(f"# case {n}: {name}",1)[1].split("# case ",1)[0]
            for n,name in enumerate(ORACLE.NAMES,1)]
    captured=(directory/"run.log").exists()
    if captured:
        log=(directory/"run.log").read_text()
        markers=re.findall(r"^KI_RESUMED (.*)$",log,re.MULTILINE)
        if len(markers) != 18 or any(not re.match(r"phase=(entry|complete|scope_end) case=[1-6] ",line) for line in markers):
            raise ValueError("malformed or extra KI_RESUMED marker")
        expected_context=contexts(log,"scope_end");entry_context=contexts(log,"entry");contexts(log,"complete")
        validate_entry_context(entry_context,pre_redirect)
    else: expected_context={}
    with tempfile.TemporaryDirectory(prefix="ki-resumed-") as temporary:
        library=CMP.build_library(Path(temporary)/"pilot.so")
        library.ki_native_pilot_begin_dispatch.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_uint64]
        library.ki_native_pilot_begin_dispatch.restype=ctypes.c_bool
        def execute(seed: bytes):
            ram=(ctypes.c_uint8*len(seed)).from_buffer_copy(seed);pilot=CMP.Pilot();library.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(seed))
            for index in range(1,32): pilot.cpu.gpr[index]=0x5a5a000000000000|index
            pilot.cpu.gpr[3]=0x1122334487654321;pilot.cpu.gpr[29]=0xffffffff88087300;pilot.cpu.gpr[31]=0xffffffff880009a4
            pilot.cpu.hi=0x1111222233334444;pilot.cpu.lo=0xaaaabbbbccccdddd
            if not library.ki_native_pilot_begin_dispatch(ctypes.byref(pilot),0xffffffff880009a4): raise ValueError("begin rejected")
            return library.ki_native_pilot_advance(ctypes.byref(pilot),50000),pilot,ram

        seeds=[]
        for number,(name,block) in enumerate(zip(ORACLE.NAMES,blocks),1):
            seed=seed_from_block(block)
            seeds.append(seed)
            if captured:
                original_before=(directory/f"{number:02d}-{name}-before.bin").read_bytes()
                if seed != original_before: raise ValueError(f"case {number}: original before-image differs from declaration")
            status,pilot,ram=execute(seed);actual=bytes(ram)
            if status != 5: raise ValueError(f"case {number}: status {status}, pc=0x{pilot.cpu.pc:x}, detail=0x{pilot.stop_detail:x}, instructions={pilot.instructions}")
            if captured:
                expected=(directory/f"{number:02d}-{name}-after.bin").read_bytes()
                if actual != expected:
                    offset=next(i for i,(a,b) in enumerate(zip(actual,expected)) if a!=b)
                    raise ValueError(f"case {number}: first RAM difference 0x{offset:x}: {actual[offset]:02x}!={expected[offset]:02x}")
                values={key:pilot.cpu.gpr[i] for i,key in enumerate(CMP.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,pc=pilot.cpu.pc)
                differences=[key for key in values if values[key]!=expected_context[number][key]]
                if differences: raise ValueError(f"case {number}: register differences {','.join(differences)}")
        negatives=((0,{0x86238:5},0x88008680,"stage"),
                   (0,{0x8bc1c:1},0x880058a0,"height"),
                   (0,{0x8bc20:0,0x8bc21:0,0x8bc22:0,0x8bc23:0},0x8800667c,"null script"))
        for seed_index,changes,expected_pc,label in negatives:
            changed=bytearray(seeds[seed_index])
            for offset,value in changes.items(): changed[offset]=value
            status,pilot,_=execute(bytes(changed))
            if status == 5 or (pilot.stop_pc & 0xffffffff) != expected_pc:
                raise ValueError(f"{label} frontier not trapped: status={status} pc=0x{pilot.stop_pc:x}")

        # Case 5 is the declared pending-pause replacement boundary.  Preserve
        # it, rebind to an unrelated host buffer, and execute the next complete
        # transaction on both histories rather than only comparing save hashes.
        status,left,left_ram=execute(seeds[4])
        library.ki_native_pilot_snapshot_size.restype=ctypes.c_size_t
        size=library.ki_native_pilot_snapshot_size();snapshot=(ctypes.c_uint8*size)()
        library.ki_native_pilot_snapshot_save.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        library.ki_native_pilot_snapshot_restore.argtypes=[ctypes.POINTER(CMP.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        right_ram=(ctypes.c_uint8*0x100000)();right=CMP.Pilot();library.ki_native_pilot_bind(ctypes.byref(right),right_ram,len(right_ram))
        if status != 5 or library.ki_native_pilot_snapshot_save(ctypes.byref(left),snapshot,size) != 0 or library.ki_native_pilot_snapshot_restore(ctypes.byref(right),snapshot,size) != 0:
            raise ValueError("case 5 snapshot boundary could not be restored")
        for pilot in (left,right):
            if not library.ki_native_pilot_begin_dispatch(ctypes.byref(pilot),0xffffffff880009a4): raise ValueError("case 5 replay begin rejected")
            if library.ki_native_pilot_advance(ctypes.byref(pilot),50000) != 5: raise ValueError("case 5 replay did not complete")
        if bytes(left_ram) != bytes(right_ram) or bytes(left.cpu) != bytes(right.cpu):
            raise ValueError("case 5 restored replay diverged")
    print(f"resumed dispatcher {'oracle replay' if captured else 'native preflight'} passed: 6 cases")

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path);args=parser.parse_args()
    compare(args.directory if args.directory.is_absolute() else ROOT/args.directory);return 0

if __name__ == "__main__": raise SystemExit(main())
