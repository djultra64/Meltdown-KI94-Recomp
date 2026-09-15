#!/usr/bin/env python3
"""Emit six original whole-dispatch allocation cases from the resumed baseline."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("resumed",ROOT/"tools/resumed_dispatcher_pilot_oracle.py")
RES=importlib.util.module_from_spec(SPEC);assert SPEC.loader is not None;SPEC.loader.exec_module(RES)
NAMES=("receiver_first_free","first_orientation","skip_occupied",
       "occupied_fallback","final_countdown","no_allocation")

def occupied_record(address: int, stream: int) -> list[str]:
    return RES.effect(address,0x1a,stream,0,0,0x200)

def additions(case: int) -> list[str]:
    lines=[]
    if case == 2:
        lines += [RES.write(0x8808bcc4,0x31,"b"),RES.write(0x8808bcc5,0,"b")]
    elif case == 5:
        lines += [RES.write(0x8808bdc4,1,"b"),RES.write(0x8808bdc5,0,"b")]
    elif case == 6:
        lines += [RES.write(0x8808bdc4,3,"b"),RES.write(0x8808bdc5,8,"b")]
    else:
        lines += [RES.write(0x8808bdc4,0x31,"b"),RES.write(0x8808bdc5,0,"b")]
    if case == 3:
        lines += occupied_record(0x8808be00,0x88092400)
        lines += occupied_record(0x8808bf00,0x88092500)
    elif case == 4:
        for index,address in enumerate(range(0x8808be00,0x8808db00,0x100)):
            lines += occupied_record(address,0x88092400+index*0x10)
    return lines

def render(directory: Path) -> str:
    resumed=RES.render(directory)
    header=resumed.split("# case 1:",1)[0].replace("KI_RESUMED","KI_ALLOC")
    template=resumed.split("# case 1: resumed\n",1)[1].split("# case 2:",1)[0]
    blocks=[]
    for case,name in enumerate(NAMES,1):
        block=template.replace("temp9=0x1",f"temp9=0x{case:x}")
        block=block.replace("01-resumed",f"{case:02d}-{name}").replace("KI_RESUMED","KI_ALLOC")
        block=block.replace("do at=0x5a5a000000000001",
                            "\n".join(additions(case)+["do at=0x5a5a000000000001"]))
        blocks.append(f"# case {case}: {name}\n{block.rstrip()}")
    return header+"\n".join(blocks)+"\nquit\n"

def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("output",type=Path);args=parser.parse_args()
    output=args.output if args.output.is_absolute() else ROOT/args.output
    if ROOT not in output.parents or "work" not in output.relative_to(ROOT).parts: parser.error("output must be under work/")
    if output.exists(): parser.error("output already exists")
    output.mkdir(parents=True);(output/"oracle.cmd").write_text(render(output.relative_to(ROOT)),encoding="utf-8")
    print(output/"oracle.cmd");return 0

if __name__ == "__main__": raise SystemExit(main())
