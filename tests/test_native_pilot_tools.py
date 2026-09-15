import importlib.util
import gc
from pathlib import Path
import tempfile
import unittest
import weakref
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_native_pilot", ROOT / "tools/generate_native_pilot.py")
GEN = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(GEN)


class NativePilotGeneratorTests(unittest.TestCase):
    def test_exact_finite_enrollment(self):
        words = GEN.load_words()
        self.assertEqual(len(words), 3187)
        self.assertEqual(sum(end - start for start, end, *_ in GEN.RANGES), 668)
        self.assertEqual(sum(end - start for start, end, _ in GEN.BINARY_RANGES), 880)
        self.assertEqual(sum(end - start for start, end, _ in GEN.RESUMED_RANGES), 1984)
        self.assertEqual(sum(end - start for start, end, _ in GEN.ALLOCATION_RANGES), 388)
        self.assertEqual(sum(end - start for start, end, _ in GEN.FRAME_RANGES), 208)
        self.assertEqual(sum(end - start for start, end, _ in GEN.PROJECTION_RANGES), 1528)
        self.assertEqual(sum(end - start for start, end, _ in GEN.TYPE12_RANGES), 556)
        self.assertEqual(sum(end - start for start, end, _ in GEN.CONNECTED_SPINE_RANGES), 1216)
        self.assertEqual(sum(end - start for start, end, *_ in GEN.PUBLISHER_RANGES), 144)
        self.assertEqual(sum(len(data) for _, data, _ in GEN.SOURCE_DATA), 39)
        self.assertEqual(len(GEN.load_manual_words()), 7)
        self.assertIn("snapshot_identity[32]", GEN.render_identity())

    def test_unknown_instruction_rejected(self):
        with self.assertRaisesRegex(GEN.GenerationError, "unsupported word"):
            GEN.emit_instruction(0x88002060, 0x4C000000)

    def test_resumed_instruction_semantics_are_explicit(self):
        self.assertEqual(GEN.emit_instruction(0x88002060, 0x3062ffff),
                         "ANDI(2, 3, 0xffffu);")
        self.assertEqual(GEN.emit_instruction(0x88002060, 0x34628000),
                         "ORI(2, 3, 0x8000u);")
        self.assertEqual(GEN.emit_instruction(0x88002060, 0x00641024),
                         "AND(2, 3, 4);")
        self.assertEqual(GEN.emit_instruction(0x88002060, 0x00031002),
                         "SRL(2, 3, 0);")

    def test_projection_fp_instruction_semantics_are_explicit(self):
        self.assertEqual(GEN.emit_instruction(0x88001038, 0xc4820004),
                         "LWC1(2, 4, 4);")
        self.assertEqual(GEN.emit_instruction(0x88001048, 0x460b1082),
                         "MUL_S(2, 2, 11);")
        self.assertEqual(GEN.emit_instruction(0x88019d60, 0x460c2034),
                         "C_OLT_S(4, 12);")

    def test_generated_file_is_current(self):
        self.assertEqual(
            (ROOT / "native/src/generated/ki15d_native_pilot.inc").read_text(),
            GEN.render(GEN.load_words()),
        )

    def test_oracle_uses_explicit_numeric_literals_and_fresh_output(self):
        oracle_spec = importlib.util.spec_from_file_location(
            "native_pilot_oracle", ROOT / "tools/native_pilot_oracle.py")
        oracle = importlib.util.module_from_spec(oracle_spec)
        assert oracle_spec.loader is not None
        oracle_spec.loader.exec_module(oracle)
        text = oracle.render(Path("work/test-native-pilot"))
        self.assertIn("do b@0x8808c000=0x1a", text)
        self.assertNotIn("=a3", text)
        self.assertEqual(text.count("phase=scope_end"), 8)

    def test_comparison_parser_rejects_extra_or_incomplete_boundaries(self):
        compare_spec = importlib.util.spec_from_file_location(
            "compare_native_pilot_oracle",
            ROOT / "tools/compare_native_pilot_oracle.py",
        )
        compare = importlib.util.module_from_spec(compare_spec)
        assert compare_spec.loader is not None
        compare_spec.loader.exec_module(compare)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            compare.final_contexts("KI_NATIVE phase=scope_end case=1 pc=0000000088002088\n")
        with self.assertRaisesRegex(ValueError, "error marker"):
            compare.final_contexts("fatal error: malformed debugger command")

    def test_connected_comparator_keeps_intermediate_ram_alive_for_advance(self):
        spec = importlib.util.spec_from_file_location(
            "compare_connected_phase_oracle",
            ROOT / "tools/compare_connected_phase_oracle.py")
        compare = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(compare)

        class RetentionObserved(Exception):
            pass

        class Call:
            def __init__(self, callback):
                self.callback = callback

            def __call__(self, *args):
                return self.callback(*args)

        ram_refs = {}
        pilots = []

        def bind(pilot_pointer, ram, _size):
            pilot = pilot_pointer._obj
            if pilot not in pilots:
                pilots.append(pilot)
                ram_refs[id(pilot)] = []
            ram_refs[id(pilot)].append(weakref.ref(ram))

        def bind_low(pilot_pointer, ram, _size):
            ram_refs[id(pilot_pointer._obj)].append(weakref.ref(ram))
            return True

        def advance(pilot_pointer, _steps):
            pilot = pilot_pointer._obj
            if pilot is pilots[1]:
                gc.collect()
                self.assertTrue(all(reference() is not None
                                    for reference in ram_refs[id(pilot)]))
                raise RetentionObserved
            return 0

        class Library:
            ki_native_pilot_bind = Call(bind)
            ki_native_pilot_bind_low_ram = Call(bind_low)
            ki_native_pilot_configure_services = Call(lambda *_: True)
            ki_native_pilot_begin_connected = Call(lambda *_: True)
            ki_native_pilot_advance = Call(advance)

        with mock.patch.object(compare.TYPES, "build_library", return_value=Library()):
            with self.assertRaises(RetentionObserved):
                compare.main()

    def test_dispatcher_oracle_has_six_explicit_cases(self):
        spec = importlib.util.spec_from_file_location(
            "dispatcher_pilot_oracle", ROOT / "tools/dispatcher_pilot_oracle.py")
        oracle = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(oracle)
        text = oracle.render(Path("work/test-dispatcher-pilot"))
        self.assertEqual(text.count("phase=scope_end"), 6)
        self.assertIn("do b@0x88087aed=0xff", text)
        self.assertNotIn("=a3", text)

    def test_resumed_oracle_has_six_source_informed_cases(self):
        spec = importlib.util.spec_from_file_location(
            "resumed_dispatcher_pilot_oracle",
            ROOT / "tools/resumed_dispatcher_pilot_oracle.py")
        oracle = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(oracle)
        text = oracle.render(Path("work/test-resumed-dispatcher-pilot"))
        self.assertEqual(text.count("phase=scope_end"), 6)
        self.assertIn("do d@0x880861f4=0x100000", text)
        self.assertIn("do b@0x8808bda7=0x15", text)
        self.assertLess(text.index("do pc=0xffffffff88002038"),
                        text.index("phase=entry"))
        self.assertNotIn("=a3", text)

    def test_resumed_context_parser_rejects_incomplete_capture(self):
        spec = importlib.util.spec_from_file_location(
            "compare_resumed_dispatcher_pilot_oracle",
            ROOT / "tools/compare_resumed_dispatcher_pilot_oracle.py")
        compare = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(compare)
        with self.assertRaisesRegex(ValueError, "incomplete matrix"):
            compare.contexts("KI_RESUMED phase=entry case=1 pc=ffffffff88002038\n", "entry")
        rendered = compare.ORACLE.render(Path("work/test-resumed-dispatcher-pilot"))
        compare.validate_script(rendered, ROOT / "work/test-resumed-dispatcher-pilot")
        with self.assertRaisesRegex(ValueError, "exact supported rendering"):
            compare.validate_script(rendered.replace("gtime 0x2", "gtime 0x1\ngtime 0x2", 1),
                                    ROOT / "work/test-resumed-dispatcher-pilot")
        with self.assertRaisesRegex(ValueError, "exact supported rendering"):
            compare.validate_script(rendered.replace("do v1=0x1122334487654321",
                                                     "do v1=0x1122334487654320", 1),
                                    ROOT / "work/test-resumed-dispatcher-pilot")
        expected = {}
        for case in range(1, 7):
            expected[case] = {name: 0x5a5a000000000000 | index
                              for index, name in enumerate(compare.ORACLE.REGS, 1)}
            expected[case].update(v1=0x1122334487654321,
                                  sp=0xffffffff88087300,
                                  ra=0xffffffff880009a4,
                                  hi=0x1111222233334444,
                                  lo=0xaaaabbbbccccdddd,
                                  pc=0x0000000088002038)
        compare.validate_entry_context(expected, False)
        expected[3]["v1"] ^= 1
        with self.assertRaisesRegex(ValueError, "full pre-entry CPU context"):
            compare.validate_entry_context(expected, False)

    def test_allocation_oracle_covers_free_skip_fallback_and_regression(self):
        spec = importlib.util.spec_from_file_location(
            "allocation_dispatcher_pilot_oracle",
            ROOT / "tools/allocation_dispatcher_pilot_oracle.py")
        oracle = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(oracle)
        text = oracle.render(Path("work/test-allocation-dispatcher-pilot"))
        self.assertEqual(text.count("phase=scope_end"), 6)
        self.assertIn("do b@0x8808bdc4=0x31", text)
        self.assertIn("do b@0x8808da00=0x1a", text)
        self.assertIn("do b@0x8808bdc4=0x3", text)
        compare_spec = importlib.util.spec_from_file_location(
            "compare_allocation_dispatcher_pilot_oracle",
            ROOT / "tools/compare_allocation_dispatcher_pilot_oracle.py")
        compare = importlib.util.module_from_spec(compare_spec)
        assert compare_spec.loader is not None
        compare_spec.loader.exec_module(compare)
        directory = ROOT / "work/test-allocation-dispatcher-pilot"
        compare.validate_script(text, directory)
        with self.assertRaisesRegex(ValueError, "script identity"):
            compare.validate_script(text.replace("gtime 0x2", "gtime 0x1", 1),
                                    directory)
        with self.assertRaisesRegex(ValueError, "incomplete allocation"):
            compare.contexts("KI_ALLOC phase=entry case=1 pc=0000000088002038\n",
                             "entry")


if __name__ == "__main__":
    unittest.main()
