#!/usr/bin/env python3
"""Compare retained DRC 099c-to-12b8 evidence with the native core."""
from __future__ import annotations
import ctypes,hashlib,importlib.util,re,tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
RUN=ROOT/"work/mame/connected-projection-drc-20260914-d"
SCRIPT_SHA256={"setup.cmd":"4c4be9727531ed6de937d15d8920dd3122de2a47b9c3fafcd9909029634a8358","run.cmd":"638fe5e9f8f9e0cb00f95c373fc2ea41ffc7bb39057beff25c61903524e2b14b"}
LOG_SHA256={"setup.log":"aeeebf1decc87b18312b701273b4bc1a7110f081ad4fd81f94c2b95b59a525b6","run.log":"d090efe9050866bdc3680780bda56fd42721b09bd4af1e94c4d7c835eaefc6e4"}
FILE_SHA256={"main-before.bin":"e634e741497b399b66f1ca060de47c621abe7aa8d023ab9fe54bf9cce9d66162","main-after.bin":"b71ec4b0fc493d945bd7db0cd933f88a0e471e10e8ae17427bf54e473efbf80e","low-before.bin":"07854d2fef297a06ba81685e660c332de36d5d18d546927d30daad6d7fda1541","low-after.bin":"07854d2fef297a06ba81685e660c332de36d5d18d546927d30daad6d7fda1541","pc.trace":"9ddde545d5e8449656e1657800517f612c0081ba8c72b8cac3ab7937cec6d0b6"}
SPEC=importlib.util.spec_from_file_location("pilot_types",ROOT/"tools/compare_native_pilot_oracle.py")
TYPES=importlib.util.module_from_spec(SPEC);assert SPEC.loader is not None;SPEC.loader.exec_module(TYPES)

def sha256(path:Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()
def context(log:str,phase:str)->dict[str,int]:
    match=re.search(rf"^KI_CONNECTED phase={phase} .*?$",log,re.MULTILINE)
    if not match:raise ValueError(f"missing {phase} context")
    return {key:int(value,16) for key,value in re.findall(r"([a-z0-9]+)=([0-9A-Fa-f]{1,16})",match.group())}

def main()->int:
    if any(sha256(RUN/name)!=expected for name,expected in SCRIPT_SHA256.items()) or any(sha256(RUN/name)!=expected for name,expected in LOG_SHA256.items()):raise ValueError("retained connected acquisition identity changed")
    if any(sha256(RUN/name)!=expected for name,expected in FILE_SHA256.items()):raise ValueError("retained connected artifact identity changed")
    log=(RUN/"run.log").read_text();loaded=context(log,"loaded");entry=context(log,"entry");complete=context(log,"complete")
    if loaded["pc"]!=0x880ff000 or entry["pc"]!=0x8800099c or any(loaded[key]!=entry[key] for key in loaded if key!="pc"):raise ValueError("DRC bootstrap changed declared entry context")
    trace=(RUN/"pc.trace").read_text().splitlines()
    if [line.split(":",1)[0] for line in trace[:4]]!=["880FF000","880FF004","880FF008","8800099C"]:raise ValueError("DRC bootstrap trace changed")
    before_div=context(log,"div_before");after_div=context(log,"div_after")
    if before_div["t0"]!=0 or after_div["hi"]!=before_div["hi"] or after_div["lo"]!=0:raise ValueError("retained DRC zero-divisor observation changed")
    if len(re.findall(r"^KI_CONNECTED phase=div_before ",log,re.MULTILINE))!=1 or len(re.findall(r"^KI_CONNECTED phase=div_after ",log,re.MULTILINE))!=1:raise ValueError("zero-divisor observation is not one paired event")
    main_before=(RUN/"main-before.bin").read_bytes();low_before=(RUN/"low-before.bin").read_bytes()
    if len(main_before)!=0x100000 or len(low_before)!=0x80000:raise ValueError("retained RAM extent changed")
    with tempfile.TemporaryDirectory(prefix="ki-connected-oracle-") as temporary:
        library=TYPES.build_library(Path(temporary)/"pilot.so")
        library.ki_native_pilot_bind_low_ram.argtypes=[ctypes.POINTER(TYPES.Pilot),ctypes.POINTER(ctypes.c_uint8),ctypes.c_size_t];library.ki_native_pilot_bind_low_ram.restype=ctypes.c_bool
        library.ki_native_pilot_configure_services.argtypes=[ctypes.POINTER(TYPES.Pilot),ctypes.POINTER(TYPES.ServiceInput),ctypes.c_size_t,ctypes.c_size_t]
        library.ki_native_pilot_begin_connected.argtypes=[ctypes.POINTER(TYPES.Pilot)];library.ki_native_pilot_begin_connected.restype=ctypes.c_bool
        def initialized():
            main=(ctypes.c_uint8*len(main_before)).from_buffer_copy(main_before);low=(ctypes.c_uint8*len(low_before)).from_buffer_copy(low_before);item=TYPES.Pilot()
            library.ki_native_pilot_bind(ctypes.byref(item),main,len(main))
            if not library.ki_native_pilot_bind_low_ram(ctypes.byref(item),low,len(low)):raise ValueError("low RAM bind rejected")
            for index,name in enumerate(TYPES.REGISTER_NAMES):
                if index:item.cpu.gpr[index]=entry[name]
            item.cpu.hi=entry["hi"];item.cpu.lo=entry["lo"];item.cpu.status_register=entry["sr"]
            for index in range(32):item.cpu.fpr[index]=entry[f"fpr{index}"]
            if not library.ki_native_pilot_configure_services(ctypes.byref(item),None,0,0) or not library.ki_native_pilot_begin_connected(ctypes.byref(item)):raise ValueError("connected begin rejected")
            return item,main,low
        pilot,main_ram,low_ram=initialized()
        # Native bindings retain raw addresses only, so keep both intermediate
        # RAM arrays alive until its advance has finished.
        intermediate,intermediate_main_ram,intermediate_low_ram=initialized()
        if library.ki_native_pilot_advance(ctypes.byref(intermediate),0x2127)!=6 or intermediate.cpu.pc!=0x88009150 or intermediate.cpu.hi!=before_div["hi"] or intermediate.cpu.lo!=0:raise ValueError("native DRC zero-divisor compatibility changed")
        status=library.ki_native_pilot_advance(ctypes.byref(pilot),20000)
        if status!=23 or pilot.cpu.pc!=0x880012b8:raise ValueError(f"connected stop {status} at {pilot.cpu.pc:08x} detail={pilot.stop_detail:08x}")
        expected_main=(RUN/"main-after.bin").read_bytes();expected_low=(RUN/"low-after.bin").read_bytes()
        actual={name:pilot.cpu.gpr[index] for index,name in enumerate(TYPES.REGISTER_NAMES) if index};actual.update(hi=pilot.cpu.hi,lo=pilot.cpu.lo,sr=pilot.cpu.status_register,pc=pilot.cpu.pc);actual.update({f"fpr{index}":pilot.cpu.fpr[index] for index in range(32)})
        different=[key for key,value in actual.items() if value!=complete[key]]
        if bytes(main_ram)!=expected_main:
            differences=[i for i,(actual,expected) in enumerate(zip(main_ram,expected_main)) if actual!=expected]
            raise ValueError(f"main RAM differs at {differences[0]:05x}")
        if bytes(low_ram)!=expected_low:
            at=next(i for i,(actual,expected) in enumerate(zip(low_ram,expected_low)) if actual!=expected);raise ValueError(f"low RAM differs at {at:05x}")
        if different:raise ValueError(f"endpoint context differs: {','.join(different)}")
    print("connected DRC original/native full-state comparison passed")
    return 0
if __name__=="__main__":raise SystemExit(main())
