#!/usr/bin/env python3
"""Derive two-process DRC input/replay scripts from connected case A."""
from __future__ import annotations
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/"work/mame/connected-source-20260914-a/oracle.cmd"
PCS=(0x1a0e4,0x1a0e8,0x1a0ec,0x1a0f0,0x1a0f8,0x1a0fc,0x1a100,
     0x1a104,0x1a108,0x1a10c,0x1a110,0x1a114,0x1a118,0x1a11c,
     0x1a120,0x1a124,0x1a128,0x1a12c,0x1a130,0x1a134,0x1a138,
     0x1a13c,0x1a144,0x1a148,0x1a14c,0x1a150,0x1a154,0x1a158,
     0x1a15c,0x1a160,0x1a164,0x1a168,0x1a16c,0x1a174,0x1a178,
     0x1a17c,0x1a184,0x1a188,0x1a190,0x1a194,0x1a19c,0x1a1a0,
     0x1a1a4,0x1a1a8,0x1a1ac,0x1a1b4)

def main()->int:
    if len(sys.argv)!=2:raise SystemExit("usage: connected_projection_probe.py OUTPUT_DIRECTORY")
    destination=Path(sys.argv[1]);destination.mkdir(parents=True,exist_ok=False)
    lines=SOURCE.read_text().replace("connected-source-20260914-a",destination.name).splitlines()
    pc_assignment=next(i for i,line in enumerate(lines) if line=="do pc=0xffffffff8800099c")
    # The saved state crosses from the interpreter to the DRC.  Execute a
    # three-word harness island so MTC0 rebuilds MAME's internal FR mode before
    # entering the unchanged guest phase.  This is diagnostic setup, not ROM.
    lines[pc_assignment:pc_assignment+1]=[
        "do d@0x880ff000=0x409a6000",
        "do d@0x880ff004=0x0a000267",
        "do d@0x880ff008=0x00000000",
        "do k0=0x34008001",
        "do pc=0xffffffff880ff000"]
    checkpoints=[]
    for pc in PCS:
        checkpoints.append(
            f'bp 0x880{pc:05x},a1==0xffffffff880856f8,{{logerror "KI_CONNECTED_FP pc=%016X a0=%016X a1=%016X fpr0=%016X fpr1=%016X fpr2=%016X fpr3=%016X fpr4=%016X fpr5=%016X fpr6=%016X fpr8=%016X fpr10=%016X fpr12=%016X fpr14=%016X fpr16=%016X fpr18=%016X fpr20=%016X fpr22=%016X fpr24=%016X fpr26=%016X fpr28=%016X fpr29=%016X fpr30=%016X\\n",pc,a0,a1,fpr0,fpr1,fpr2,fpr3,fpr4,fpr5,fpr6,fpr8,fpr10,fpr12,fpr14,fpr16,fpr18,fpr20,fpr22,fpr24,fpr26,fpr28,fpr29,fpr30 ; g}}')
    entry=next(i for i,line in enumerate(lines) if line.startswith('logerror "KI_CONNECTED phase=entry '))
    trace=next(i for i,line in enumerate(lines) if line.startswith("trace work/mame/"))
    trace_off=next(i for i,line in enumerate(lines) if line=="trace off,maincpu")
    setup=lines[:entry]+[lines[entry].replace("phase=entry","phase=prepared")]+[
        f"save work/mame/{destination.name}/main-before.bin,0x88000000,0x100000",
        f"save work/mame/{destination.name}/low-before.bin,0x80000000,0x80000",
        "statesave connected","quit",""]
    # The second process performs no debugger writes.  Its first log attests
    # that the state loaded by -state contains the exact declared CPU context.
    runtime=[lines[entry].replace("phase=entry","phase=loaded")]
    runtime.append("bp 0x8800099c,0x1,{"+lines[entry]+" ; g}")
    runtime+=lines[entry+3:trace]
    runtime+=checkpoints+[lines[trace]]+lines[trace+1:trace_off+1]
    runtime+=lines[trace_off+1:]
    (destination/"setup.cmd").write_text("\n".join(setup))
    (destination/"run.cmd").write_text("\n".join(runtime)+"\n")
    print(destination/"setup.cmd");print(destination/"run.cmd")
    return 0
if __name__=="__main__":raise SystemExit(main())
