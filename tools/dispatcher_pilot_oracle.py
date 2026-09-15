#!/usr/bin/env python3
"""Emit the six-case original whole-dispatch comparison matrix."""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGS = ("at","v0","v1","a0","a1","a2","a3","t0","t1","t2","t3",
        "t4","t5","t6","t7","s0","s1","s2","s3","s4","s5","s6",
        "s7","t8","t9","k0","k1","gp","sp","fp","ra")
CASES = (("empty",0,0,False,False,False),
         ("paused_fighters",3,0,True,False,False),
         ("mixed_replace",1,4,True,True,False),
         ("smaller_replace",9,1,True,True,False),
         ("signed_small",0xff,0,True,True,False),
         ("terminal",0,0xff,False,True,True))

def write(address: int, value: int, width: str = "d") -> str:
    return f"do {width}@0x{address:08x}=0x{value:x}"

def context(phase: str) -> str:
    fmt = " ".join(f"{name}=%016X" for name in REGS + ("hi","lo","pc"))
    args = ",".join(REGS + ("hi","lo","pc"))
    return f'logerror "KI_DISPATCH phase={phase} case=%X {fmt}\\n",temp9,{args}'

def record_type15(terminal: bool) -> list[str]:
    r, stream = 0x8808bf00, 0x88092000
    data = (0,0x14) if terminal else (4,2,0,0x14)
    return ([write(r,0x15,"b"),write(r+0x20,stream),write(r+0x18,0 if terminal else 0x200),
             write(r+0x42,0x100,"w"),write(r+0x84,0x11223344),write(r+0x98,0)] +
            [write(stream+i,v,"b") for i,v in enumerate(data)])

def record_type1a(terminal: bool) -> list[str]:
    r, stream = 0x8808c000, 0x88092100
    data = (0,0x14) if terminal else (4,2,0,0x14)
    return ([write(r,0x1a,"b"),write(r+0x20,stream),write(r+0x18,0 if terminal else 0x200),
             write(r+0x40,0x100,"w"),write(r+0x42,0x100,"w"),
             write(r+0x5a,0x1000,"w"),write(r+0x0c,0x5500)] +
            [write(stream+i,v,"b") for i,v in enumerate(data)])

def small(address: int, signed_case: bool, opposite: bool = False) -> list[str]:
    countdown = 1 if signed_case else 3
    velocity = 0xff if signed_case else 0xfe
    h14, h16 = ((0x80,0xffc0) if opposite else (0xff80,0x40))
    return [write(address,1,"b"),write(address+1,countdown,"b"),write(address+0x18,velocity,"b"),
            write(address+4,0x100),write(address+8,0xffffff00),write(address+0xc,0x1000),
            write(address+0x10,0x100,"w"),write(address+0x12,0x40,"w"),
            write(address+0x14,h14,"w"),write(address+0x16,h16,"w")]

def render(directory: Path) -> str:
    lines=["# Generated original MAME 0.289 whole-dispatch matrix; no code patches.",
           "focus maincpu","step 0x2","temp9=0x0"]
    for pc,phase in ((0x88002060,"large_dispatch"),(0x880021c8,"small_pool"),
                     (0x88002410,"epilogue"),(0x880009a4,"pause_call")):
        lines.append(f"bp 0x{pc:08x},0x1,{{{context(phase)} ; g}}")
    lines.append(f"bp 0x880009ac,0x1,{{{context('complete')}}}")
    for number,(name,active,pending,fighters,mixed,terminal) in enumerate(CASES,1):
        lines += [f"# case {number}: {name}",f"temp9=0x{number:x}",
                  "fill 0x88000000,0x100000,0x00",
                  "load work/kipack/ki15d/rom-0.bin,0x88000000",
                  "load work/kipack/ki15d/rom-1.bin,0x88033900",
                  write(0x880884f8,1),write(0x88086238,6,"b"),write(0x8808623a,6,"b"),
                  write(0x88087280,0x1234),write(0x88087284,0x5678),
                  write(0x88087aec,active,"b"),write(0x88087aed,pending,"b")]
        if fighters:
            lines += [write(0x8808bc00,1,"b"),write(0x8808bc54,0x3000,"w"),
                      write(0x8808bd00,6,"b"),write(0x8808bd54,0x3000,"w")]
        if mixed:
            lines += record_type15(terminal)+record_type1a(terminal)+small(0x8808a620,name=="signed_small")
            if name=="signed_small": lines += small(0x8808a63c,True,True)
        for index,reg in enumerate(REGS,1):
            lines.append(f"do {reg}=0x{0x5a5a000000000000|index:x}")
        lines += ["do v1=0x1122334487654321","do sp=0xffffffff88087300",
                  "do ra=0xffffffff880009a4","do hi=0x1111222233334444",
                  "do lo=0xaaaabbbbccccdddd",
                  f"save {directory.as_posix()}/{number:02d}-{name}-before.bin,0x88000000,0x100000",
                  context("entry"),"do pc=0xffffffff88002038","gtime 0x2",
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
if __name__=="__main__": raise SystemExit(main())
