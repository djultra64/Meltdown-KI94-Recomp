#!/usr/bin/env python3
"""Strictly replay native execution against a retained original matrix."""

from __future__ import annotations

import argparse
import ctypes
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
CASE_NAMES = ("empty", "timer", "advance", "command10", "command14",
              "signed", "signed_div", "clear54d0")
REGISTER_NAMES = ("zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
                  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
                  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
                  "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra")


class Memory(ctypes.Structure):
    _fields_ = [("low", ctypes.c_void_p), ("low_size", ctypes.c_size_t),
                ("main", ctypes.c_void_p), ("main_size", ctypes.c_size_t),
                ("rom", ctypes.c_void_p), ("rom_size", ctypes.c_size_t)]


class Cpu(ctypes.Structure):
    _fields_ = [("gpr", ctypes.c_uint64 * 32), ("hi", ctypes.c_uint64),
                ("lo", ctypes.c_uint64), ("pc", ctypes.c_uint64),
                ("delay_target", ctypes.c_uint64),
                ("delay_pending", ctypes.c_bool), ("fpr", ctypes.c_uint64*32),
                ("status_register",ctypes.c_uint32),("fcc",ctypes.c_bool),
                ("fcc_valid",ctypes.c_bool)]

class ServiceInput(ctypes.Structure):
    _fields_=[("kind",ctypes.c_uint32),("pc",ctypes.c_uint32),("address",ctypes.c_uint32),("value",ctypes.c_uint32)]
class ServiceOutput(ctypes.Structure):
    _fields_=[("invocation",ctypes.c_uint64),("pc",ctypes.c_uint32),("address",ctypes.c_uint32),("value",ctypes.c_uint32)]


class Pilot(ctypes.Structure):
    _fields_ = [("memory", Memory), ("cpu", Cpu), ("status", ctypes.c_int),
                ("stop_pc", ctypes.c_uint64), ("stop_detail", ctypes.c_uint64),
                ("instructions", ctypes.c_uint64), ("sequence", ctypes.c_uint64),
                ("transaction_kind", ctypes.c_uint32),
                ("service_inputs",ServiceInput*7),("service_outputs",ServiceOutput*12),
                ("service_input_count",ctypes.c_uint32),("service_input_cursor",ctypes.c_uint32),
                ("service_output_count",ctypes.c_uint32),("service_output_capacity",ctypes.c_uint32)]


def final_contexts(log: str) -> dict[int, dict[str, int]]:
    if re.search(r"(?i)(fatal error|unknown command|invalid debugger|traceback)", log):
        raise ValueError("MAME log contains an error marker")
    result = {}
    for match in re.finditer(r"^KI_NATIVE phase=scope_end case=([0-9A-F]+) (.*)$",
                             log, re.MULTILINE):
        case = int(match.group(1), 16)
        if case in result:
            raise ValueError(f"duplicate scope_end for case {case}")
        result[case] = {key: int(value, 16) for key, value in
                        re.findall(r"([a-z0-9]+)=([0-9A-F]{16})", match.group(2))}
    if set(result) != set(range(1, 9)):
        raise ValueError("matrix must contain exactly one scope_end for cases 1..8")
    required = set(REGISTER_NAMES[1:]) | {"hi", "lo", "pc"}
    if any(set(context) != required for context in result.values()):
        raise ValueError("scope_end context is incomplete or has extra fields")
    return result


def build_library(destination: Path) -> ctypes.CDLL:
    command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
               "-O2", "-fPIC", "-shared", "-Inative/include", "native/src/memory.c",
               "native/src/original/ki15d_880054d0.c", "native/src/native_pilot.c",
               "native/src/native_pilot_snapshot.c", "-lm",
               "-o", str(destination)]
    subprocess.run(command, cwd=ROOT, check=True)
    library = ctypes.CDLL(str(destination))
    library.ki_native_pilot_bind.argtypes = [ctypes.POINTER(Pilot),
                                              ctypes.POINTER(ctypes.c_uint8),
                                              ctypes.c_size_t]
    library.ki_native_pilot_advance.argtypes = [ctypes.POINTER(Pilot), ctypes.c_uint64]
    library.ki_native_pilot_advance.restype = ctypes.c_int
    return library


def compare(directory: Path) -> None:
    contexts = final_contexts((directory / "run.log").read_text(encoding="utf-8"))
    with tempfile.TemporaryDirectory(prefix="ki-native-pilot-") as temporary:
        library = build_library(Path(temporary) / "pilot.so")
        for number, name in enumerate(CASE_NAMES, 1):
            before = (directory / f"{number:02d}-{name}-before.bin").read_bytes()
            expected = (directory / f"{number:02d}-{name}-after.bin").read_bytes()
            if len(before) != 0x100000 or len(expected) != 0x100000:
                raise ValueError(f"case {number}: snapshot is not exactly 1 MiB")
            ram = (ctypes.c_uint8 * len(before)).from_buffer_copy(before)
            pilot = Pilot()
            library.ki_native_pilot_bind(ctypes.byref(pilot), ram, len(before))
            for index in range(1, 32):
                pilot.cpu.gpr[index] = 0x5A5A000000000000 | index
            pilot.cpu.gpr[28] = 1
            pilot.cpu.gpr[29] = 0xFFFFFFFF8809F000
            pilot.cpu.gpr[30] = 0xFFFFFFFF8808C000
            pilot.cpu.hi, pilot.cpu.lo = 0x1111222233334444, 0xAAAABBBBCCCCDDDD
            pilot.cpu.pc = 0xFFFFFFFF88002060
            if name == "clear54d0":
                pilot.cpu.gpr[31] = 0xFFFFFFFF880045B4
                pilot.cpu.pc = 0xFFFFFFFF880054D0
            status = library.ki_native_pilot_advance(ctypes.byref(pilot), 1000)
            expected_status = 3 if name == "empty" else 2 if name in {
                "command14", "signed_div"} else 4 if name == "clear54d0" else 1
            if status != expected_status:
                raise ValueError(f"case {number}: native status {status}, expected {expected_status}")
            actual_ram = bytes(ram)
            if actual_ram != expected:
                offset = next(i for i, pair in enumerate(zip(actual_ram, expected))
                              if pair[0] != pair[1])
                raise ValueError(f"case {number}: first RAM difference at 0x{offset:x}")
            actual = {name: pilot.cpu.gpr[index]
                      for index, name in enumerate(REGISTER_NAMES) if index}
            actual.update(hi=pilot.cpu.hi, lo=pilot.cpu.lo, pc=pilot.cpu.pc)
            differences = [name for name in actual if actual[name] != contexts[number][name]]
            if differences:
                raise ValueError(f"case {number}: register differences: {','.join(differences)}")
    print("native pilot oracle replay passed: 8 cases, full RAM and final context")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    compare(args.directory if args.directory.is_absolute() else ROOT / args.directory)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
