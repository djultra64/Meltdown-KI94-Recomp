import ctypes
import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


GEN = load_module("generate_native_pilot_health_guard",
                  ROOT / "tools/generate_native_pilot.py")
BASE = load_module("native_pilot_types_health_guard",
                   ROOT / "tools/compare_native_pilot_oracle.py")


class StandingHealthGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="ki-health-guard-")
        temporary = Path(cls.temporary.name)
        shim = temporary / "health_guard_shim.c"
        shim.write_text(
            '#include "native/src/native_pilot.c"\n'
            'bool health_guard_step(KiNativePilot *pilot) {'
            ' return generated_execute(pilot); }\n', encoding="utf-8")
        library_path = temporary / "pilot.so"
        subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
            "-O2", "-fPIC", "-shared", "-I.", "-Inative/include",
            "native/src/memory.c", "native/src/original/ki15d_880054d0.c",
            str(shim), "-lm", "-o", str(library_path),
        ], cwd=ROOT, check=True)
        cls.library = ctypes.CDLL(str(library_path))
        cls.library.ki_native_pilot_bind.argtypes = [
            ctypes.POINTER(BASE.Pilot), ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t]
        cls.library.ki_native_pilot_bind_low_ram.argtypes = [
            ctypes.POINTER(BASE.Pilot), ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t]
        cls.library.ki_native_pilot_bind_low_ram.restype = ctypes.c_bool
        cls.library.health_guard_step.argtypes = [ctypes.POINTER(BASE.Pilot)]
        cls.library.health_guard_step.restype = ctypes.c_bool

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def make_case(self, health):
        ram = (ctypes.c_uint8 * 0x100000)()
        low_ram = (ctypes.c_uint8 * 0x80000)()
        for address, word in GEN.load_words():
            offset = address - 0x88000000
            ram[offset:offset + 4] = struct.pack("<I", word)
        ram[0x8BD54:0x8BD56] = struct.pack("<H", health)
        pilot = BASE.Pilot()
        self.library.ki_native_pilot_bind(ctypes.byref(pilot), ram, len(ram))
        self.assertTrue(self.library.ki_native_pilot_bind_low_ram(
            ctypes.byref(pilot), low_ram, len(low_ram)))
        for index in range(1, 32):
            pilot.cpu.gpr[index] = 0x5A5A000000000000 | index
        pilot.cpu.gpr[30] = 0xFFFFFFFF8808BD00
        pilot.cpu.hi = 0x1111222233334444
        pilot.cpu.lo = 0xAAAABBBBCCCCDDDD
        pilot.cpu.pc = 0xFFFFFFFF88003120
        pilot.cpu.status_register = 0x34008001
        for index in range(32):
            pilot.cpu.fpr[index] = 0x1020304000000000 | index
        return pilot, ram, low_ram

    def assert_state_unchanged(self, pilot, original_gpr, original_fpr,
                               original_hi_lo_status, ram, low_ram,
                               original_ram, original_low):
        for index in range(32):
            if index != 4:
                self.assertEqual(pilot.cpu.gpr[index], original_gpr[index])
        self.assertEqual(list(pilot.cpu.fpr), original_fpr)
        self.assertEqual((pilot.cpu.hi, pilot.cpu.lo,
                          pilot.cpu.status_register), original_hi_lo_status)
        self.assertEqual(bytes(ram), original_ram)
        self.assertEqual(bytes(low_ram), original_low)

    def test_health_guard_each_instruction_and_frontier(self):
        for health in (0, 0x3000, 0x8000, 0xFFFF):
            with self.subTest(health=health):
                pilot, ram, low_ram = self.make_case(health)
                original_gpr = list(pilot.cpu.gpr)
                original_fpr = list(pilot.cpu.fpr)
                original_hi_lo_status = (pilot.cpu.hi, pilot.cpu.lo,
                                         pilot.cpu.status_register)
                original_ram, original_low = bytes(ram), bytes(low_ram)
                self.assertTrue(self.library.health_guard_step(ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.gpr[4],
                                  pilot.cpu.delay_pending, pilot.instructions),
                                 (0x88003124, health, False, 1))
                self.assert_state_unchanged(
                    pilot, original_gpr, original_fpr, original_hi_lo_status,
                    ram, low_ram, original_ram, original_low)
                self.assertTrue(self.library.health_guard_step(ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.delay_pending,
                                  pilot.cpu.delay_target, pilot.instructions),
                                 (0x88003128, True,
                                  0x88003294 if health else 0x8800312C, 2))
                self.assertTrue(self.library.health_guard_step(ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.delay_pending,
                                  pilot.instructions),
                                 (0x88003294 if health else 0x8800312C,
                                  False, 3))
                self.assert_state_unchanged(
                    pilot, original_gpr, original_fpr, original_hi_lo_status,
                    ram, low_ram, original_ram, original_low)
                if health == 0:
                    self.assertFalse(self.library.health_guard_step(
                        ctypes.byref(pilot)))
                    self.assertEqual((pilot.status, pilot.stop_pc,
                                      pilot.instructions),
                                     (9, 0x8800312C, 3))

    def test_positive_contact_class_guard_routes_and_preserves_state(self):
        for receiver_class, target in ((0, 0x880036C8),
                                       (0x80, 0x880035E0),
                                       (0xFF, 0x880035E0)):
            with self.subTest(receiver_class=receiver_class):
                pilot, ram, low_ram = self.make_case(0x3000)
                ram[0x8BD5C] = receiver_class
                pilot.cpu.pc = 0xFFFFFFFF880035D4
                original_gpr = list(pilot.cpu.gpr)
                original_fpr = list(pilot.cpu.fpr)
                original_hi_lo_status = (pilot.cpu.hi, pilot.cpu.lo,
                                         pilot.cpu.status_register)
                original_ram, original_low = bytes(ram), bytes(low_ram)

                self.assertTrue(self.library.health_guard_step(
                    ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.gpr[4],
                                  pilot.cpu.delay_pending, pilot.instructions),
                                 (0x880035D8, receiver_class, False, 1))
                self.assertTrue(self.library.health_guard_step(
                    ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.delay_pending,
                                  pilot.cpu.delay_target, pilot.instructions),
                                 (0x880035DC, True, target, 2))
                self.assertTrue(self.library.health_guard_step(
                    ctypes.byref(pilot)))
                self.assertEqual((pilot.cpu.pc, pilot.cpu.delay_pending,
                                  pilot.instructions), (target, False, 3))
                self.assert_state_unchanged(
                    pilot, original_gpr, original_fpr, original_hi_lo_status,
                    ram, low_ram, original_ram, original_low)

                if receiver_class:
                    self.assertFalse(self.library.health_guard_step(
                        ctypes.byref(pilot)))
                    self.assertEqual((pilot.status, pilot.stop_pc,
                                      pilot.instructions),
                                     (9, 0x880035E0, 3))

    def test_positive_contact_state_guards_both_routes(self):
        cases = (
            (0x88003920, 6, 0, 0x88003930, 2, 3),
            (0x88003920, 6, 1, 0x8800392C, 2, 3),
            (0x88003930, 28, 0, 0x880039A8, None, 2),
            (0x88003930, 28, 1, 0x88003938, None, 2),
        )
        for entry, register, value, target, changed, count in cases:
            with self.subTest(entry=hex(entry), value=value):
                pilot, ram, low_ram = self.make_case(0x3000)
                pilot.cpu.pc = entry
                pilot.cpu.gpr[register] = value
                original_gpr = list(pilot.cpu.gpr)
                original_fpr = list(pilot.cpu.fpr)
                original_hi_lo_status = (pilot.cpu.hi, pilot.cpu.lo,
                                         pilot.cpu.status_register)
                original_ram, original_low = bytes(ram), bytes(low_ram)

                for instruction in range(count):
                    self.assertTrue(self.library.health_guard_step(
                        ctypes.byref(pilot)))
                    if instruction == count - 2:
                        self.assertTrue(pilot.cpu.delay_pending)
                        self.assertEqual(pilot.cpu.delay_target, target)
                self.assertEqual((pilot.cpu.pc, pilot.cpu.delay_pending,
                                  pilot.instructions), (target, False, count))
                for index in range(32):
                    if index != changed:
                        self.assertEqual(pilot.cpu.gpr[index],
                                         original_gpr[index])
                if changed is not None:
                    self.assertEqual(pilot.cpu.gpr[changed], value & 1)
                self.assertEqual(list(pilot.cpu.fpr), original_fpr)
                self.assertEqual((pilot.cpu.hi, pilot.cpu.lo,
                                  pilot.cpu.status_register),
                                 original_hi_lo_status)
                self.assertEqual(bytes(ram), original_ram)
                self.assertEqual(bytes(low_ram), original_low)
                if target == 0x8800392C:
                    self.assertFalse(self.library.health_guard_step(
                        ctypes.byref(pilot)))
                    self.assertEqual((pilot.status, pilot.stop_pc,
                                      pilot.instructions),
                                     (9, 0x8800392C, count))


if __name__ == "__main__":
    unittest.main()
