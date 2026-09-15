#!/usr/bin/env python3
"""Emit one finite original 099c-to-12b8 source-initialized acquisition."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REGISTERS = ("at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1",
             "t2", "t3", "t4", "t5", "t6", "t7", "s0", "s1", "s2",
             "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1",
             "gp", "sp", "fp", "ra")


def write(address: int, value: int, width: str = "d") -> str:
    return f"do {width}@0x{address:08x}=0x{value:x}"


def context(phase: str) -> str:
    fields = list(REGISTERS) + ["hi", "lo", "sr"] + [f"fpr{i}" for i in range(32)]
    formatting = " ".join(f"{name}=%016X" for name in fields)
    return f'logerror "KI_CONNECTED phase={phase} pc=%016X {formatting}\\n",pc,{",".join(fields)}'


def fighter(base: int, kind: int, x: int, stream: int, receiver: bool) -> list[str]:
    values = ((0, kind, "b"), (0x54, 0x3000, "w"), (0x9e, 0x14, "w"),
              (0x2c, 4, "w"), (0x5f, 0x0a, "b"), (0x1c, 2, "b"),
              (0x18, 0x200, "d"), (0x42, 0x100, "w"),
              (0x20, stream, "d"), (0xa7, 0x15 if receiver else 0, "b"),
              (4, x, "d"))
    return ([write(base + offset, value, width) for offset, value, width in values] +
            [write(stream, 0x14000204)])


def render(directory: Path) -> str:
    lines = [
        "# MAME 0.289 finite source-initialized connected-phase acquisition.",
        "# Synthetic fighter streams are diagnostic inputs, not shipping animation state.",
        "focus maincpu", "step 0x2", "fill 0x88000000,0x100000,0x00",
        "fill 0x80000000,0x80000,0x00",
        "load work/kipack/ki15d/rom-0.bin,0x88000000",
        "load work/kipack/ki15d/rom-1.bin,0x88033900",
        "fill 0x88089a48,0x200,0x00", "fill 0x8808a620,0x236e0,0x00",
        write(0x880884f8, 1), write(0x88086238, 6, "b"),
        write(0x8808623a, 6, "b"), write(0x880861f4, 0x100000),
    ]
    lines += fighter(0x8808bc00, 1, 0x6000, 0x88092200, False)
    lines += fighter(0x8808bd00, 6, 0xffffa000, 0x88092300, True)
    lines += [write(0x8808bd98, 0), write(0x8808bd8b, 0, "b"),
              write(0x8808bdc4, 3, "b"), write(0x8808bdc5, 8, "b")]
    actor = 0x8808be00
    values = ((0, 0x12, "b"), (0x54, 0, "w"), (0x61, 0, "b"),
              (0x2c, 0, "w"), (0x60, 0, "b"), (0x63, 0, "b"),
              (0x7e, 0, "w"), (0x20, 0x8804233c, "d"), (0x18, 0, "d"),
              (0x42, 0x100, "w"), (4, 0x1000, "d"), (0x0c, 0x1000, "d"),
              (0x84, 0x100, "d"))
    lines += [write(actor + offset, value, width) for offset, value, width in values]
    lines += [write(0x88086228, 0), write(0x8808726c, 6),
              write(0x88087298, 0x10000), write(0x8808dfdc, 0x40400000),
              write(0x8808dfe0, 0x30000)]
    camera = (0, 0x35400, 0, 0x8d9a0, 0, 0x7530, 0, 0x1e0000,
              0x80000, 0x80000, 0, 0)
    lines += [write(0x880861c0 + index * 4, value)
              for index, value in enumerate(camera)]
    lines += [write(0x880861f0, 0x1f, "w"), write(0x880861f2, 0, "b")]
    lines += [f"do {name}=0x{0x5a5a000000000000 | index:x}"
              for index, name in enumerate(REGISTERS, 1)]
    lines += ["do sp=0xffffffff88087300", "do hi=0x1111222233334444",
              "do lo=0xaaaabbbbccccdddd"]
    lines += [f"do fpr{index}=0x0" for index in range(32)]
    lines += ["do sr=0x34008001", "do pc=0xffffffff8800099c", context("entry"),
              f"save {directory}/main-before.bin,0x88000000,0x100000",
              f"save {directory}/low-before.bin,0x80000000,0x80000",
              "temp0=0x0", "temp1=0x0", "temp2=0x0",
              'bp 0x8800914c,(t0==0x0)&&(temp2==0x0),{temp0=temp0+0x1 ; temp2=0x1 ; logerror "KI_CONNECTED phase=div_before pc=%016X hi=%016X lo=%016X t0=%016X t1=%016X s4=%016X\\n",pc,hi,lo,t0,t1,s4 ; g}',
              'bp 0x88009150,temp2==0x1,{temp1=temp1+0x1 ; temp2=0x0 ; logerror "KI_CONNECTED phase=div_after pc=%016X hi=%016X lo=%016X t0=%016X t1=%016X s4=%016X\\n",pc,hi,lo,t0,t1,s4 ; g}',
              f"bp 0x880012b8,0x1,{{{context('complete')}}}",
              f"trace {directory}/pc.trace,maincpu,noloop", "gtime 0x10",
              "trace off,maincpu", context("scope_end"),
              'logerror "KI_CONNECTED phase=terminal pc=%016X div_before=%X div_after=%X\\n",pc,temp0,temp1',
              f"save {directory}/main-after.bin,0x88000000,0x100000",
              f"save {directory}/low-after.bin,0x80000000,0x80000", "quit", ""]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    if ROOT not in output.parents or "work" not in output.relative_to(ROOT).parts:
        parser.error("output must be a fresh directory under work/")
    if output.exists():
        parser.error("output already exists")
    output.mkdir(parents=True)
    script = output / "oracle.cmd"
    script.write_text(render(output.relative_to(ROOT)), encoding="utf-8")
    print(script)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
