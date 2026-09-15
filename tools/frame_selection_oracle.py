#!/usr/bin/env python3
"""Emit the finite original frame-selection boundary matrix."""
from __future__ import annotations
import argparse
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
REGS=("at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3","t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6","s7","t8","t9","k0","k1","gp","sp","fp","ra")
CASES=(("authentic",0x0a,0x04,False),("normalize",0x0a,0x00,False),
       ("final",0x0a,0x15,False),("clamp",0x0a,0xff,False),
       ("first_row",0x01,0x01,False),("later",0x0a,0x0b,False),
       ("fallback",0x0a,0x03,True))

def put(address:int,value:int,width:str="d")->str:return f"do {width}@0x{address:08x}=0x{value:x}"
def context(phase:str)->str:
    fmt=" ".join(f"{r}=%016X" for r in REGS+("hi","lo","pc"));args=",".join(REGS+("hi","lo","pc"))
    return f'logerror "KI_FRAME phase={phase} case=%X {fmt}\\n",temp9,{args}'
def render(directory:Path)->str:
    lines=["# Generated MAME 0.289 frame selector matrix; no code patches.","focus maincpu","step 0x2","temp9=0x0",f"bp 0x88001898,0x1,{{{context('complete')}}}"]
    for number,(name,key,token,synthetic) in enumerate(CASES,1):
        lines += [f"# case {number}: {name}",f"temp9=0x{number:x}","fill 0x88000000,0x100000,0x00",
                  "load work/kipack/ki15d/rom-0.bin,0x88000000","load work/kipack/ki15d/rom-1.bin,0x88033900",
                  "load work/assets/ki15d-boot-graphics/sector-0aaa-count-010a.bin,0x88090100",
                  put(0x8808c000,0x1a,"b"),put(0x8808c014,token,"b"),put(0x8808c034,key,"b")]
        if synthetic:
            lines += [put(0x88090140,0x880f0000),put(0x880f0000,1,"b"),put(0x880f0002,1,"b"),put(0x880f0004,0x40),put(0x880f0008,0x20),
                      put(0x880f0040,0x0a,"b"),put(0x880f0042,3,"b"),put(0x880f0044,0x80),put(0x880f0048,0x20),put(0x880f004c,0x80),put(0x880f0050,0x80)]
        for index,reg in enumerate(REGS,1):lines.append(f"do {reg}=0x{0x5a5a000000000000|index:x}")
        lines += ["do v1=0x11","do a1=0x1a","do gp=0x4","do sp=0xffffffff88087300","do fp=0xffffffff8808c000","do ra=0xffffffff88001898",
                  "do hi=0x1111222233334444","do lo=0xaaaabbbbccccdddd",
                  f"save {directory.as_posix()}/{number:02d}-{name}-before.bin,0x88000000,0x100000",context("entry"),
                  "do pc=0xffffffff880016c4","gtime 0x2",
                  f"save {directory.as_posix()}/{number:02d}-{name}-after.bin,0x88000000,0x100000",context("scope_end")]
    return "\n".join(lines+["quit",""])
def main()->int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("output",type=Path);args=parser.parse_args()
    output=args.output if args.output.is_absolute() else ROOT/args.output
    if ROOT not in output.parents or "work" not in output.relative_to(ROOT).parts:parser.error("output must be under work/")
    if output.exists():parser.error("output already exists")
    output.mkdir(parents=True);(output/"oracle.cmd").write_text(render(output.relative_to(ROOT)),encoding="utf-8")
    print(output/"oracle.cmd");return 0
if __name__=="__main__":raise SystemExit(main())
