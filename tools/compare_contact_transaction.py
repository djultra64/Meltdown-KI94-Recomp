#!/usr/bin/env python3
"""Replay the retained contact-D transaction through the finite native pilot."""
from __future__ import annotations
import ctypes,importlib.util,re,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];RUN=ROOT/"work/tasks/POC-120B2/contact-d"
spec=importlib.util.spec_from_file_location("base",ROOT/"tools/compare_native_pilot_oracle.py");BASE=importlib.util.module_from_spec(spec);spec.loader.exec_module(BASE)
COUNTS=(0x4e041b1e,0x4e041d42,0x4e04319b)
OUTPUTS=((0xb0000098,0x08),(0xb0000090,1),(0xb0000090,3),(0xb0000098,0xd2),(0xb0000090,1),(0xb0000090,3),(0xb0000098,1),(0xb0000090,1),(0xb0000090,3),(0xb0000098,0xfa),(0xb0000090,1),(0xb0000090,3))
OUTPUT_PCS=(0x88003e94,0x88003ea0,0x88003ebc,0x88003f10,0x88003f1c,0x88003f38)*2
def context(log:str,phase:str):
    line=re.search(rf"^KI_CONTACT_D phase={phase} .*?$",log,re.MULTILINE)
    if not line:raise ValueError(f"missing {phase}")
    return {k:int(v,16) for k,v in re.findall(r"([a-z0-9]+)=([0-9A-Fa-f]{8,16})",line.group())}
def main()->int:
    log=(RUN/"run.log").read_text();before=context(log,"before");after=context(log,"after")
    sequence=[(1,0x88009744,0,COUNTS[0]),(1,0x88005bf0,0,COUNTS[1]),(2,0x88003e6c,0xb0000090,0xff),(2,0x88003edc,0xb0000090,0xff),(1,0x88005bf0,0,COUNTS[2]),(2,0x88003e6c,0xb0000090,0xff),(2,0x88003edc,0xb0000090,0xff)]
    with tempfile.TemporaryDirectory(prefix="ki-contact-") as temp:
        lib=BASE.build_library(Path(temp)/"pilot.so");lib.ki_native_pilot_configure_services.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.POINTER(BASE.ServiceInput),ctypes.c_size_t,ctypes.c_size_t];lib.ki_native_pilot_begin_contact.argtypes=[ctypes.POINTER(BASE.Pilot)];lib.ki_native_pilot_begin_contact.restype=ctypes.c_bool
        inputs=(BASE.ServiceInput*7)(*(BASE.ServiceInput(*row) for row in sequence))
        def initialized():
            ram=(ctypes.c_uint8*0x100000).from_buffer_copy((RUN/"ram-before.bin").read_bytes());pilot=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(pilot),ram,len(ram))
            for i,name in enumerate(BASE.REGISTER_NAMES):
                if i:pilot.cpu.gpr[i]=before[name]
            pilot.cpu.hi=before["hi"];pilot.cpu.lo=before["lo"];pilot.cpu.status_register=before["sr"]
            for i in range(32):pilot.cpu.fpr[i]=before[f"fpr{i}"]
            return pilot,ram
        pilot,ram=initialized()
        if not lib.ki_native_pilot_configure_services(ctypes.byref(pilot),inputs,7,12) or not lib.ki_native_pilot_begin_contact(ctypes.byref(pilot)):raise ValueError("contact begin rejected")
        lib.ki_native_pilot_snapshot_size.restype=ctypes.c_size_t;size=lib.ki_native_pilot_snapshot_size();snapshot=(ctypes.c_uint8*size)()
        lib.ki_native_pilot_snapshot_save.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t];lib.ki_native_pilot_snapshot_restore.argtypes=[ctypes.POINTER(BASE.Pilot),ctypes.c_void_p,ctypes.c_size_t]
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(pilot),snapshot,size)!=0:raise ValueError("ready contact snapshot rejected")
        status=lib.ki_native_pilot_advance(ctypes.byref(pilot),100000)
        if status!=19:raise ValueError(f"status {status} pc={pilot.cpu.pc:08x} detail={pilot.stop_detail:08x} inputs={pilot.service_input_cursor} outputs={pilot.service_output_count}")
        if bytes(ram)!=(RUN/"ram-after.bin").read_bytes():
            expected=(RUN/"ram-after.bin").read_bytes();at=next(i for i,(a,b) in enumerate(zip(ram,expected)) if a!=b);raise ValueError(f"RAM differs at {at:05x}: {ram[at]:02x}!={expected[at]:02x}")
        actual=[(e.pc,e.address,e.value,e.invocation) for e in pilot.service_outputs[:pilot.service_output_count]]
        expected_events=[(pc,address,value,0) for pc,(address,value) in zip(OUTPUT_PCS,OUTPUTS)]
        if actual!=expected_events:raise ValueError(f"output journal differs: {actual}")
        values={name:pilot.cpu.gpr[i] for i,name in enumerate(BASE.REGISTER_NAMES) if i};values.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,sr=pilot.cpu.status_register,pc=pilot.cpu.pc)
        values.update({f"fpr{i}":pilot.cpu.fpr[i] for i in range(32)})
        diff=[key for key,value in values.items() if value!=after[key]]
        if diff:raise ValueError(f"context differs: {','.join(diff)}")
        completed_snapshot=(ctypes.c_uint8*size)();completed_ram=(ctypes.c_uint8*0x100000)();completed=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(completed),completed_ram,len(completed_ram))
        lib.ki_native_pilot_state_hash.argtypes=[ctypes.POINTER(BASE.Pilot)];lib.ki_native_pilot_state_hash.restype=ctypes.c_uint64
        if lib.ki_native_pilot_snapshot_save(ctypes.byref(pilot),completed_snapshot,size)!=0 or lib.ki_native_pilot_snapshot_restore(ctypes.byref(completed),completed_snapshot,size)!=0 or completed.service_output_count!=12 or lib.ki_native_pilot_state_hash(ctypes.byref(completed))!=lib.ki_native_pilot_state_hash(ctypes.byref(pilot)):raise ValueError("completed journal snapshot/hash did not survive rebind")
        semantic_hash=lib.ki_native_pilot_state_hash(ctypes.byref(completed));padding=BASE.Pilot.service_outputs.offset+20
        ctypes.cast(ctypes.byref(completed),ctypes.POINTER(ctypes.c_uint8))[padding]=0x7b
        if lib.ki_native_pilot_state_hash(ctypes.byref(completed))!=semantic_hash:raise ValueError("host padding changed semantic state hash")
        replay_ram=(ctypes.c_uint8*0x100000)();replay=BASE.Pilot();lib.ki_native_pilot_bind(ctypes.byref(replay),replay_ram,len(replay_ram))
        old_version=(ctypes.c_uint8*size).from_buffer_copy(bytes(snapshot));old_version[7]=ord("1")
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(replay),old_version,size)!=4 or any(replay_ram):raise ValueError("old snapshot was accepted or changed live RAM")
        if lib.ki_native_pilot_snapshot_restore(ctypes.byref(replay),snapshot,size)!=0 or lib.ki_native_pilot_advance(ctypes.byref(replay),100000)!=19:raise ValueError("restored contact replay failed")
        if bytes(replay_ram)!=bytes(ram) or bytes(replay.cpu)!=bytes(pilot.cpu) or replay.service_output_count!=12:raise ValueError("restored contact replay diverged")
        mismatch,discard=initialized();bad=list(sequence);bad[0]=(1,0x88009748,0,COUNTS[0]);bad_inputs=(BASE.ServiceInput*7)(*(BASE.ServiceInput(*row) for row in bad))
        if not lib.ki_native_pilot_configure_services(ctypes.byref(mismatch),bad_inputs,7,12) or not lib.ki_native_pilot_begin_contact(ctypes.byref(mismatch)) or lib.ki_native_pilot_advance(ctypes.byref(mismatch),100000)!=16:raise ValueError("mismatched service PC accepted")
        missing,discard=initialized()
        if not lib.ki_native_pilot_configure_services(ctypes.byref(missing),inputs,6,12) or lib.ki_native_pilot_begin_contact(ctypes.byref(missing)):raise ValueError("missing service input accepted")
        capacity,discard=initialized()
        if not lib.ki_native_pilot_configure_services(ctypes.byref(capacity),inputs,7,11) or lib.ki_native_pilot_begin_contact(ctypes.byref(capacity)):raise ValueError("undersized journal accepted")
    print("contact-D original/native replay passed")
    return 0
if __name__=="__main__":raise SystemExit(main())
