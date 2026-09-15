#!/usr/bin/env python3
"""Emit the finite original-CPU comparison matrix for the connected pilot."""

from __future__ import annotations

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CASES = (
    ("empty", 0, 0, 0, ()),
    ("timer", 0x1A, 0x04, 0x200, (0x04, 0x02, 0x00, 0x14)),
    ("advance", 0x1A, 0x04, 0xFF, (0x04, 0x02, 0x00, 0x14)),
    ("command10", 0x1A, 0, 0, (0x00, 0x10, 0x05, 0x00, 0x04, 0x02, 0x00, 0x14)),
    ("command14", 0x1A, 0, 0, (0x00, 0x14)),
    ("signed", 0x1A, 0x04, 0, (0x04, 0x02, 0x00, 0x14)),
    ("signed_div", 0x1A, 0, 0, (0x04, 0x02, 0x00, 0x14)),
    ("clear54d0", -1, 0, 0, ()),
)
GPRS = ("zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
        "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
        "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
        "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra")


def event(phase: str) -> str:
    names = GPRS[1:] + ("hi", "lo", "pc")
    fmt = " ".join(f"{name}=%016X" for name in names)
    args = ",".join(names)
    return f'logerror "KI_NATIVE phase={phase} case=%X {fmt}\\n",temp9,{args}'


def store(address: int, value: int, width: str = "d") -> str:
    return f"do {width}@0x{address:08x}=0x{value:x}"


def render(output: Path) -> str:
    lines = [
        "# Generated finite matrix; original R4600/MAME 0.289, no code patches.",
        "focus maincpu", "step 0x2", "temp9=0x0",
    ]
    for address, phase in ((0x88002064, "loaded_type"),
                           (0x88002080, "dispatch_target"),
                           (0x88004e54, "update_entry"),
                           (0x88004e5c, "planar_done"),
                           (0x88004e64, "vertical_done"),
                           (0x88004e6c, "animation_done"),
                           (0x88004180, "planar_entry"),
                           (0x88004208, "planar_return"),
                           (0x8800842c, "vertical_entry"),
                           (0x88008470, "vertical_return"),
                           (0x88006670, "animation_entry"),
                           (0x880067f8, "animation_return"),
                           (0x88006314, "order_entry"),
                           (0x88006cb4, "order_return"),
                           (0x880067d4, "division_result"),
                           (0x880067e4, "timer_result"),
                           (0x880054d0, "clear_entry")):
        lines.append(f"bp 0x{address:08x},0x1,{{{event(phase)} ; g}}")
    lines += [
        f"bp 0x88002088,(temp9!=0x1)&&(temp9!=0x8),{{{event('complete2088')}}}",
        f"bp 0x880021c8,temp9==0x1,{{{event('complete21c8')}}}",
        f"bp 0x880045b4,temp9==0x8,{{{event('clear_return')}}}",
    ]
    for number, (name, obj_type, token, timer, stream) in enumerate(CASES, 1):
        record = 0x8808C000
        lines += [
            f"# case {number}: {name}", f"temp9=0x{number:x}",
            "fill 0x88000000,0x100000,0x00",
            "load work/kipack/ki15d/rom-0.bin,0x88000000",
            "load work/kipack/ki15d/rom-1.bin,0x88033900",
            f"fill 0x{record:08x},0x100,0x{'a5' if name == 'clear54d0' else '00'}",
        ]
        if obj_type >= 0:
            lines += [store(record, obj_type, "b"), store(record + 0x04, 0xFFFF9A00),
                      store(record + 0x0C, 0x00005500), store(record + 0x10, 0xA0),
                      store(record + 0x14, token), store(record + 0x18, timer),
                      store(record + 0x20, 0x88092000 if stream else 0),
                      store(record + 0x40, 0x0100, "w"), store(record + 0x42, 0x0100, "w"),
                      store(record + 0x58, 0x1000, "w"), store(record + 0x5A, 0x1000, "w"),
                      store(0x88087B20, 0), store(0x88087B24, 0)]
            lines += [store(0x88092000 + index, byte, "b") for index, byte in enumerate(stream)]
            lines += [store(0x880872A0 + index, index, "b") for index in range(0x20)]
            lines.append("fill 0x880872c0,0x20,0x00")
            if name == "signed":
                lines += [store(record + 0x7C, 0xFF80, "w"), store(record + 0x7E, 0xFF00, "w"),
                          store(record + 0x80, 0x0180, "w"), store(record + 0x84, 0x0200, "w"),
                          store(record + 0x86, 0x0100, "w"), store(record + 0x3C, 0xFFF6, "w"),
                          store(record + 0x10, 0xFFFFFF60), store(record + 0x0C, 0x00005500),
                          store(record + 0x40, 0x0080, "w")]
            if name == "signed_div":
                lines.append(store(record + 0x42, 0xFF00, "w"))
        for index, register in enumerate(GPRS[1:], 1):
            lines.append(f"do {register}=0x{(0x5a5a000000000000 | index):x}")
        lines += ["do gp=0x1", "do sp=0xffffffff8809f000", "do fp=0xffffffff8808c000",
                  "do hi=0x1111222233334444", "do lo=0xaaaabbbbccccdddd",
                  f"save {output.as_posix()}/{number:02d}-{name}-before.bin,0x88000000,0x100000"]
        if name == "clear54d0":
            lines += ["do ra=0xffffffff880045b4", event("entry"),
                      "do pc=0xffffffff880054d0"]
        else:
            lines += [event("entry"), "do pc=0xffffffff88002060"]
        lines += ["gtime 0x1",
                  f"save {output.as_posix()}/{number:02d}-{name}-after.bin,0x88000000,0x100000",
                  event("scope_end")]
    lines.append("quit")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output if args.output.is_absolute() else ROOT / args.output
    if ROOT not in output.parents or "work" not in output.relative_to(ROOT).parts:
        parser.error("output must be a fresh ignored work/ directory")
    if output.exists():
        parser.error("output already exists; evidence must be fresh")
    output.mkdir(parents=True)
    (output / "oracle.cmd").write_text(render(output.relative_to(ROOT)), encoding="utf-8")
    print(output / "oracle.cmd")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
