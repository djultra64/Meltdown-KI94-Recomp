from __future__ import annotations

import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "analyze_type1a_state_to_render",
    PROJECT_ROOT / "tools" / "analyze_type1a_state_to_render.py",
)
assert SPEC is not None and SPEC.loader is not None
ANALYZER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ANALYZER
SPEC.loader.exec_module(ANALYZER)


SAMPLE_TRACE = """\
KI_S2R phase=input call=0 object=FFFFFFFF8808C000 type=1A token=04 pos04=0001B380:00010C00:000055AA vel10=000000AA byte24=02 frame30=88097670 state34=0A scale58=1000:1080 word68=010B3A48 word6c=0081D1A7 word70=000010E9 facing8c=00 palette8e=00 display94=60 flags95=00 aux97=00 clipc8=0000 widthca=0000 fbselect=00 viewport=000000F0 palette_source=FFFFFFFF88090100
KI_S2R phase=renderer call=0 object=FFFFFFFF8808C000 frame=FFFFFFFF88097670 frame_header=000A:0009:0017:0012 a0=0000000000000000 gp=0000000000000000 t1=00000000000010E9 t3=0000000000001170 t4=0000000000000002 s5=0000000000000000 s6=0000000000000060 s7=0000000000010B3A t8=00000000000081D1 k0=0000000000000000 k1=0000000000000140 fp=00000000000000F0 s2=0000000000000280
KI_S2R phase=vertical_fixed call=0 frame_origin_y_product=0000000000009CF0 y_scale=0000000000001170 y_base_fixed=0000000000082510 y_with_origin=000000000008C200
KI_S2R phase=surface call=0 fbselect_register=0 framebuffer=FFFFFFFF80030000 row_y=000000000000008C row_byte_offset=0000000000015E00 frame_origin_x_product=000000000000A91A scaled_width=0000000000000018 scaled_height=0000000000000013 x_base_fixed=000000000010BBA0 row_pointer=FFFFFFFF80045E00
KI_S2R phase=clip call=0 render_mode=70 next_row_left_boundary=FFFFFFFF80046080 next_row_right_boundary=FFFFFFFF80046300 available_rows=000000000000008D y_integer=000000000000008C vertical_clip_origin=0000000000000000 vertical_fraction_prebias=0000000000000DFF horizontal_remainder=0000000000000286 row_stride=0000000000000280
KI_S2R phase=dispatch call=0 render_mode=70 row_callback=FFFFFFFF88011918 palette_table=FFFFFFFF8803480A
KI_S2R phase=first_row call=0 object=FFFFFFFF8808C000 frame=0000000088097670 source=FFFFFFFF88097679 skipped_source_rows=0 skipped_source_bytes=0 destination=FFFFFFFF80046002 x_scale=00000000000010E9 x_remainder=0000000000000286 y_accumulator=0000000000001F6F y_scale=0000000000001170 rows_minus_one=0000000000000012 row_stride=0000000000000280 pixel_step=0000000000000002 palette=FFFFFFFF8803480A
KI_S2R phase=palette_lookup call=0 table=FFFFFFFF8803480A address=FFFFFFFF8803480E
KI_S2R phase=done call=0 object=FFFFFFFF8808C000
"""

RIGHT_CLIPPED_TRACE = """\
KI_S2R phase=input call=0 object=FFFFFFFF8808C000 type=1A token=11 pos04=0001DC00:00010C00:0000765C vel10=000001B8 byte24=02 frame30=88098AB4 state34=0A scale58=1000:1E00 word68=012CD678 word6c=005BF78C word70=0000110F facing8c=00 palette8e=00 display94=60 flags95=00 aux97=00 clipc8=0000 widthca=0000 fbselect=01 viewport=000000F0 palette_source=FFFFFFFF88090100
KI_S2R phase=renderer call=0 object=FFFFFFFF8808C000 frame=FFFFFFFF88098AB4 frame_header=0013:0014:0028:0029 a0=0000000000000000 gp=0000000000000000 t1=000000000000110F t3=0000000000001FFC t4=0000000000000002 s5=FFFFFFFF88332641 s6=0000000000000060 s7=0000000000012CD6 t8=0000000000005BF7 k0=0000000000000000 k1=0000000000000140 fp=00000000000000F0 s2=0000000000000280
KI_S2R phase=vertical_fixed call=0 frame_origin_y_product=0000000000027FB0 y_scale=0000000000001FFC y_base_fixed=000000000005C770 y_with_origin=0000000000084720
KI_S2R phase=surface call=0 fbselect_register=1 framebuffer=FFFFFFFF80058000 row_y=0000000000000084 row_byte_offset=0000000000014A00 frame_origin_x_product=000000000001441D scaled_width=000000000000002A scaled_height=0000000000000051 x_base_fixed=000000000012D560 row_pointer=FFFFFFFF8006CA00
KI_S2R phase=clip call=0 render_mode=78 next_row_left_boundary=FFFFFFFF8006CC80 next_row_right_boundary=FFFFFFFF8006CF00 available_rows=0000000000000085 y_integer=0000000000000084 vertical_clip_origin=FFFFFFFFFFF88332 vertical_fraction_prebias=00000000000008DF horizontal_remainder=0000000000000143 row_stride=0000000000000280
KI_S2R phase=dispatch call=0 render_mode=78 row_callback=FFFFFFFF880118C0 palette_table=FFFFFFFF8803480A
KI_S2R phase=first_row call=0 object=FFFFFFFF8808C000 frame=0000000088098AB4 source=FFFFFFFF88098ABD skipped_source_rows=0 skipped_source_bytes=0 destination=FFFFFFFF8006CC32 x_scale=000000000000110F x_remainder=0000000000000143 y_accumulator=00000000000028DB y_scale=0000000000001FFC rows_minus_one=0000000000000050 row_stride=0000000000000280 pixel_step=0000000000000002 palette=FFFFFFFF8803480A
KI_S2R phase=palette_lookup call=0 table=FFFFFFFF8803480A address=FFFFFFFF8803480E
KI_S2R phase=done call=0 object=FFFFFFFF8808C000
"""


class StateToRenderAnalyzerTests(unittest.TestCase):
    def test_sample_recomputes_original_transform(self) -> None:
        calls = ANALYZER.parse_trace(io.StringIO(SAMPLE_TRACE))
        self.assertEqual(len(calls), 1)
        row = ANALYZER.derive_row(calls[0])
        self.assertEqual(row["destination_x"], 257)
        self.assertEqual(row["destination_y"], 140)
        self.assertEqual(row["x_remainder"], "0286")
        self.assertEqual(row["y_accumulator"], "1f6f")
        self.assertEqual(row["output_rows"], 19)
        self.assertEqual(row["source_cursor_offset"], "0009")
        self.assertEqual(row["horizontal_clip"], "none")
        self.assertEqual(row["row_callback"], "88011918")
        self.assertEqual(row["first_palette_index"], "2")

    def test_horizontal_mode_distinguishes_unclipped_and_right_clipped(self) -> None:
        self.assertEqual(
            ANALYZER.resolve_horizontal_mode(0x60, 0x10E9, 2, 257, 24, 0, 320),
            (0x70, "none"),
        )
        self.assertEqual(
            ANALYZER.resolve_horizontal_mode(0x60, 0x110F, 2, 281, 42, 0, 320),
            (0x78, "right"),
        )

    def test_horizontal_mode_includes_reverse_direction_selector(self) -> None:
        self.assertEqual(
            ANALYZER.resolve_horizontal_mode(0x60, 0x1000, -2, 50, 20, 10, 100),
            (0x64, "none"),
        )
        self.assertEqual(
            ANALYZER.resolve_horizontal_mode(0x60, 0x1000, -2, 105, 20, 10, 100),
            (0x6C, "right"),
        )

    def test_right_clipped_trace_uses_clipped_callback(self) -> None:
        call = ANALYZER.parse_trace(io.StringIO(RIGHT_CLIPPED_TRACE))[0]
        row = ANALYZER.derive_row(call)
        self.assertEqual(row["destination_x"], 281)
        self.assertEqual(row["scaled_width"], 42)
        self.assertEqual(row["horizontal_clip"], "right")
        self.assertEqual(row["render_mode"], "78")
        self.assertEqual(row["row_callback"], "880118c0")

    def test_missing_phase_is_rejected(self) -> None:
        damaged = "\n".join(
            line for line in SAMPLE_TRACE.splitlines() if "phase=clip" not in line
        )
        with self.assertRaisesRegex(ANALYZER.TraceError, "missing phase"):
            ANALYZER.parse_trace(io.StringIO(damaged))

    def test_reordered_phase_is_rejected(self) -> None:
        lines = SAMPLE_TRACE.splitlines()
        lines[2], lines[3] = lines[3], lines[2]
        with self.assertRaisesRegex(ANALYZER.TraceError, "reordered"):
            ANALYZER.parse_trace(io.StringIO("\n".join(lines)))

    def test_changed_register_is_rejected_at_first_mismatch(self) -> None:
        damaged = SAMPLE_TRACE.replace(
            "destination=FFFFFFFF80046002", "destination=FFFFFFFF80046004"
        )
        call = ANALYZER.parse_trace(io.StringIO(damaged))[0]
        with self.assertRaisesRegex(
            ANALYZER.TraceError, "phase first_row field destination"
        ):
            ANALYZER.derive_row(call)

    def test_malformed_token_is_rejected(self) -> None:
        damaged = SAMPLE_TRACE.replace(" token=04", " token")
        with self.assertRaisesRegex(ANALYZER.TraceError, "malformed token"):
            ANALYZER.parse_trace(io.StringIO(damaged))

    def test_fixture_has_complete_milestone_coverage(self) -> None:
        rows = ANALYZER.load_csv(
            PROJECT_ROOT / "tests" / "fixtures" / "ki15d_type1a_state_to_render.csv"
        )
        self.assertEqual(len(rows), 315)
        self.assertEqual(max(int(row["tick"]) for row in rows) + 1, 96)
        milestones = ";".join(row["milestones"] for row in rows)
        self.assertIn("first_visible", milestones)
        self.assertEqual(milestones.count("six_particle_peak"), 6)
        self.assertIn("first_horizontal_downscale", milestones)
        self.assertIn("first_right_clip", milestones)
        self.assertEqual(sum(row["horizontal_clip"] == "right" for row in rows), 54)
        for lifetime in range(7):
            self.assertIn(f"last_visible_lifetime_{lifetime}", milestones)

    def test_fixture_comparison_rejects_changed_expected_value(self) -> None:
        row = ANALYZER.derive_row(ANALYZER.parse_trace(io.StringIO(SAMPLE_TRACE))[0])
        row["tick"] = 0
        row["pass_index"] = 0
        row["milestones"] = "first_visible"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "fixture.csv"
            with path.open("w", encoding="utf-8", newline="") as handle:
                ANALYZER.write_csv([row], handle)
            contents = path.read_text(encoding="utf-8").replace(
                ",257,140,", ",258,140,"
            )
            path.write_text(contents, encoding="utf-8")
            with self.assertRaisesRegex(ANALYZER.TraceError, "destination_x"):
                ANALYZER.compare_fixture([row], path)

    def test_bridge_row_preserves_inputs_and_renderer_row_start(self) -> None:
        call = ANALYZER.parse_trace(io.StringIO(SAMPLE_TRACE))[0]
        row = ANALYZER.derive_row(call)
        row["tick"] = 0
        row["pass_index"] = 0
        row["milestones"] = "first_visible"
        bridge = ANALYZER.derive_bridge_row(call, row)
        self.assertEqual(bridge["record_address"], "8808c000")
        self.assertEqual(bridge["pixel_step"], 2)
        self.assertEqual(bridge["record_scale_y"], "1080")
        self.assertEqual(bridge["word68"], "010b3a48")
        self.assertEqual(bridge["framebuffer_address"], "80030000")
        self.assertEqual(bridge["source_offset"], "0008")
        self.assertEqual(bridge["field8e"], "00")
        self.assertEqual(bridge["field97"], "00")

    def test_bridge_fixture_has_all_trace_cases(self) -> None:
        rows = ANALYZER.load_bridge_csv(
            PROJECT_ROOT
            / "tests"
            / "fixtures"
            / "ki15d_88001b90_type1a_bridge.csv"
        )
        self.assertEqual(len(rows), 315)
        self.assertEqual(sum(row["horizontal_clip"] == "right" for row in rows), 54)
        self.assertEqual(sum(int(row["skipped_source_rows"]) for row in rows), 5)
        self.assertTrue(all(row["field8e"] == "00" for row in rows))
        self.assertTrue(all(row["field97"] == "00" for row in rows))
        self.assertEqual(rows[0]["milestones"], "first_visible")
        self.assertIn("last_visible_lifetime_6", rows[-1]["milestones"])

    def test_bridge_fixture_comparison_reports_first_changed_field(self) -> None:
        call = ANALYZER.parse_trace(io.StringIO(SAMPLE_TRACE))[0]
        row = ANALYZER.derive_row(call)
        row["tick"] = 0
        row["pass_index"] = 0
        row["milestones"] = "first_visible"
        bridge = ANALYZER.derive_bridge_row(call, row)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bridge.csv"
            with path.open("w", encoding="utf-8", newline="") as handle:
                ANALYZER.write_bridge_csv([bridge], handle)
            contents = path.read_text(encoding="utf-8").replace(
                ",257,140,", ",258,140,"
            )
            path.write_text(contents, encoding="utf-8")
            with self.assertRaisesRegex(ANALYZER.TraceError, "destination_x"):
                ANALYZER.compare_bridge_fixture([bridge], path)


if __name__ == "__main__":
    unittest.main()
