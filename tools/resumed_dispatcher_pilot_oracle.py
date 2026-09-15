#!/usr/bin/env python3
"""Emit the six-case original resumed/no-allocation dispatcher matrix."""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGS = ("at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3",
        "t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6",
        "s7","t8","t9","k0","k1","gp","sp","fp","ra")
NAMES = ("resumed", "mixed_effect", "countdown_zero", "positive_height",
         "pending_replace", "pending_clear")

def write(address: int, value: int, width: str = "d") -> str:
    return f"do {width}@0x{address:08x}=0x{value:x}"

def context(phase: str) -> str:
    fmt = " ".join(f"{name}=%016X" for name in REGS + ("hi","lo","pc"))
    args = ",".join(REGS + ("hi","lo","pc"))
    return f'logerror "KI_RESUMED phase={phase} case=%X {fmt}\\n",temp9,{args}'

def fighter(base: int, kind: int, x: int, stream: int, receiver: bool,
            positive: bool) -> list[str]:
    values = [(0,kind,"b"),(0x54,0x3000,"w"),(0x9e,0x14,"w"),
              (0x2c,4,"w"),(0x5f,0x0a,"b"),(0x1c,2,"b"),
              (0x18,0x200,"d"),(0x42,0x100,"w"),(0x20,stream,"d"),
              (0xa7,0x15 if receiver else 0,"b"),(4,x,"d"),
              (0x0c,0x1000 if positive else 0,"d")]
    return ([write(base+offset,value,width) for offset,value,width in values] +
            [write(stream+i,value,"b") for i,value in enumerate((4,2,0,0x14))])

def effect(base: int, kind: int, stream: int, countdown: int, cadence: int,
           timer: int) -> list[str]:
    values=[(0,kind,"b"),(0x20,stream,"d"),(0x18,timer,"d"),(0x42,0x100,"w")]
    if kind == 0x15: values += [(0x84,0x11223344,"d"),(0xc4,countdown,"b"),(0xc5,cadence,"b")]
    else: values += [(0x40,0x100,"w"),(0x5a,0x1000,"w"),(0x0c,0x5500,"d")]
    return ([write(base+o,v,w) for o,v,w in values] +
            [write(stream+i,value,"b") for i,value in enumerate((4,2,0,0x14))])

def small() -> list[str]:
    base=0x8808a620
    return [write(base,1,"b"),write(base+1,3,"b"),write(base+0x18,0xfe,"b"),
            write(base+4,0x100),write(base+8,0xffffff00),write(base+0xc,0x1000),
            write(base+0x10,0x100,"w"),write(base+0x12,0x40,"w"),
            write(base+0x14,0xff80,"w"),write(base+0x16,0x40,"w")]

def render(directory: Path) -> str:
    lines=["# Generated MAME 0.289 resumed dispatcher matrix; source executes unchanged.",
           "focus maincpu","step 0x2","temp9=0x0",
           f"bp 0x880009ac,0x1,{{{context('complete')}}}"]
    for number,name in enumerate(NAMES,1):
        mixed=number in (2,3,5,6); positive=number == 4
        active,pending=((0,4) if number == 5 else (0,1) if number == 6 else (0,0))
        lines += [f"# case {number}: {name}",f"temp9=0x{number:x}",
                  "fill 0x88000000,0x100000,0x00",
                  "load work/kipack/ki15d/rom-0.bin,0x88000000",
                  "load work/kipack/ki15d/rom-1.bin,0x88033900",
                  write(0x880884f8,1),write(0x88086238,6,"b"),write(0x8808623a,6,"b"),
                  write(0x880861f4,0x100000),write(0x88087aec,active,"b"),
                  write(0x88087aed,pending,"b")]
        lines += fighter(0x8808bc00,1,0x6000,0x88092200,False,positive)
        lines += fighter(0x8808bd00,6,0xffffa000,0x88092300,True,positive)
        if mixed:
            lines += effect(0x8808bf00,0x15,0x88092000,1 if number == 3 else (0 if number == 6 else 3),8,0 if number == 6 else 0x200)
            lines += effect(0x8808c000,0x1a,0x88092100,0,0,0 if number == 6 else 0x200)
            lines += small()
        for index,reg in enumerate(REGS,1): lines.append(f"do {reg}=0x{0x5a5a000000000000|index:x}")
        lines += ["do v1=0x1122334487654321","do sp=0xffffffff88087300",
                  "do ra=0xffffffff880009a4","do hi=0x1111222233334444",
                  "do lo=0xaaaabbbbccccdddd",
                  "do pc=0xffffffff88002038",context("entry"),
                  f"save {directory.as_posix()}/{number:02d}-{name}-before.bin,0x88000000,0x100000",
                  "gtime 0x2",
                  f"save {directory.as_posix()}/{number:02d}-{name}-after.bin,0x88000000,0x100000",
                  context("scope_end")]
    return "\n".join(lines+["quit",""])

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("output",type=Path);args=parser.parse_args()
    output=args.output if args.output.is_absolute() else ROOT/args.output
    if ROOT not in output.parents or "work" not in output.relative_to(ROOT).parts: parser.error("output must be under work/")
    if output.exists(): parser.error("output already exists")
    output.mkdir(parents=True);(output/"oracle.cmd").write_text(render(output.relative_to(ROOT)),encoding="utf-8")
    print(output/"oracle.cmd");return 0

if __name__ == "__main__": raise SystemExit(main())
