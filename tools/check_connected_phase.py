#!/usr/bin/env python3
"""Exercise the bounded 099c-to-12b8 transaction without a captured outcome."""

from __future__ import annotations

import ctypes
import importlib.util
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "pilot_types", ROOT / "tools/compare_native_pilot_oracle.py")
TYPES = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(TYPES)


def put(ram: bytearray, offset: int, value: int, width: int = 4) -> None:
    ram[offset:offset + width] = value.to_bytes(width, "little")


def fighter(ram: bytearray, offset: int, kind: int, x: int,
            stream: int, receiver: bool) -> None:
    # The four-byte stream is a diagnostic branch input, not an authentic
    # installed animation or a production initializer claim.
    for field, value, width in (
        (0, kind, 1), (0x54, 0x3000, 2), (0x9e, 0x14, 2),
        (0x2c, 4, 2), (0x5f, 0x0a, 1), (0x1c, 2, 1),
        (0x18, 0x200, 4), (0x42, 0x100, 2), (0x20, stream, 4),
        (0xa7, 0x15 if receiver else 0, 1), (4, x, 4),
    ):
        put(ram, offset + field, value, width)
    put(ram, stream - 0x88000000, 0x14000204)


def fixture() -> bytearray:
    # Projection-A supplies an accepted numerical backing state.  All live
    # objects/pools below are rebuilt as declared synthetic scenario inputs;
    # this retained snapshot is test evidence, never a production seed.
    ram = bytearray((ROOT / "work/tasks/POC-120C/projection-a/main-before.bin").read_bytes())
    rom0 = (ROOT / "work/kipack/ki15d/rom-0.bin").read_bytes()
    rom1 = (ROOT / "work/kipack/ki15d/rom-1.bin").read_bytes()
    ram[:len(rom0)] = rom0
    ram[0x33900:0x33900 + len(rom1)] = rom1
    ram[0x89a48:0x89c48] = bytes(0x200)
    ram[0x8a620:] = bytes(len(ram) - 0x8a620)
    put(ram, 0x884f8, 1)
    put(ram, 0x86238, 6, 1)
    put(ram, 0x8623a, 6, 1)
    put(ram, 0x861f4, 0x100000)
    fighter(ram, 0x8bc00, 1, 0x6000, 0x88092200, False)
    fighter(ram, 0x8bd00, 6, 0x100000, 0x88092300, True)
    put(ram, 0x8bd2c, 0, 2)
    put(ram, 0x8bd24, 0xfe, 1)
    put(ram, 0x8bdc4, 3, 1)
    put(ram, 0x8bdc5, 8, 1)
    actor = 0x8be00
    for field, value, width in (
        (0, 0x12, 1), (0x54, 0, 2), (0x61, 0, 1), (0x2c, 0, 2),
        (0x60, 0, 1), (0x63, 0, 1), (0x7e, 0, 2),
        (0x20, 0x8804233c, 4), (0x18, 0, 4), (0x42, 0x100, 2),
        (4, 0x1000, 4), (0x0c, 0x1000, 4),
    ):
        put(ram, actor + field, value, width)
    put(ram, 0x86228, 0)
    put(ram, 0x8726c, 6)
    put(ram, 0x87298, 0x10000)
    return ram


def initialize(library: ctypes.CDLL, data: bytearray):
    ram = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
    pilot = TYPES.Pilot()
    library.ki_native_pilot_bind(ctypes.byref(pilot), ram, len(ram))
    for index in range(1, 32):
        pilot.cpu.gpr[index] = 0x5A5A000000000000 | index
    pilot.cpu.gpr[29] = 0xFFFFFFFF88087300
    pilot.cpu.status_register = 0x34008001
    return pilot, ram


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="ki-connected-") as temporary:
        library = TYPES.build_library(Path(temporary) / "pilot.so")
        library.ki_native_pilot_configure_services.argtypes = [
            ctypes.POINTER(TYPES.Pilot), ctypes.POINTER(TYPES.ServiceInput),
            ctypes.c_size_t, ctypes.c_size_t]
        library.ki_native_pilot_begin_connected.argtypes = [ctypes.POINTER(TYPES.Pilot)]
        library.ki_native_pilot_begin_connected.restype = ctypes.c_bool
        library.ki_native_pilot_snapshot_size.restype = ctypes.c_size_t
        library.ki_native_pilot_snapshot_save.argtypes = [
            ctypes.POINTER(TYPES.Pilot), ctypes.c_void_p, ctypes.c_size_t]
        library.ki_native_pilot_snapshot_restore.argtypes = [
            ctypes.POINTER(TYPES.Pilot), ctypes.c_void_p, ctypes.c_size_t]

        pilot, ram = initialize(library, fixture())
        if not library.ki_native_pilot_configure_services(
                ctypes.byref(pilot), None, 0, 0) or not library.ki_native_pilot_begin_connected(ctypes.byref(pilot)):
            raise ValueError("connected begin rejected")
        size = library.ki_native_pilot_snapshot_size()
        snapshot = (ctypes.c_uint8 * size)()
        if library.ki_native_pilot_snapshot_save(
                ctypes.byref(pilot), snapshot, size) != 0:
            raise ValueError("connected entry snapshot rejected")
        status = library.ki_native_pilot_advance(ctypes.byref(pilot), 100000)
        if status != 23 or pilot.cpu.pc != 0x880012b8:
            raise ValueError(f"connected stop {status} at {pilot.cpu.pc:08x}")
        if bytes(ram[0x89a48:0x89a54]) != bytes.fromhex("0200f3411544f848f730163b"):
            raise ValueError("source publisher result changed")
        expected_ram = bytes(ram)
        expected_cpu = bytes(pilot.cpu)

        replay, replay_ram = initialize(library, bytearray(len(ram)))
        if library.ki_native_pilot_snapshot_restore(
                ctypes.byref(replay), snapshot, size) != 0 or library.ki_native_pilot_advance(
                    ctypes.byref(replay), 100000) != 23:
            raise ValueError("restored connected replay failed")
        if bytes(replay_ram) != expected_ram or bytes(replay.cpu) != expected_cpu:
            raise ValueError("restored connected replay diverged")

        rejected, rejected_ram = initialize(library, fixture())
        rejected.cpu.gpr[29] = 0xFFFFFFFF88002068
        before = bytes(rejected_ram)
        if not library.ki_native_pilot_configure_services(
                ctypes.byref(rejected), None, 0, 0) or not library.ki_native_pilot_begin_connected(
                    ctypes.byref(rejected)) or library.ki_native_pilot_advance(
                        ctypes.byref(rejected), 1) != 13 or bytes(rejected_ram) != before:
            raise ValueError("unsafe connected stack was accepted or changed RAM")
    print("connected 099c-to-12b8 native/replay/frontier gates passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
