from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "contact_initializer_oracle", ROOT / "tools/contact_initializer_oracle.py"
)
assert SPEC is not None and SPEC.loader is not None
ORACLE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ORACLE)


def synthetic_pilot(directory: Path) -> tuple[list[dict], str]:
    cases = ORACLE.pilot_cases(ORACLE.load_cases())
    lines = []
    for case in cases:
        number = case["case"]
        window, table = ORACLE.initial_regions(case)
        (directory / f"{number:02d}-window-before.bin").write_bytes(window)
        (directory / f"{number:02d}-table-before.bin").write_bytes(table)
        effect = case["effect"]
        lines.append(
            f"KI_CONTACT phase=effect case={number:X} pc=8800A2E8 "
            f"cpu={effect:02X} debugger={effect:02X}"
        )
        if case["effect"]:
            values = [(0x20 * number + index) & 0xFF for index in range(4)]
            row_offset = (case["effect"] & 15) * 8
            values[0:2] = table[row_offset:row_offset + 2]
            receiver = case["receiver"]
            window[receiver + 0xC4:receiver + 0xC8] = bytes(values)
            for pc, offset, value in zip(
                (0x8800A308, 0x8800A310, 0x8800A344), (0xC4, 0xC5, 0xC6), values
            ):
                lines.append(
                    f"KI_CONTACT phase=store case={number:X} pc={pc:X} "
                    f"address={ORACLE.WINDOW_BASE + receiver + offset:016X} value={value:02X}"
                )
            reread = values[0] if case["source_case"] == 15 else case["effect"]
            lines.append(
                f"KI_CONTACT phase=reread case={number:X} pc=8800A34C "
                f"cpu={reread:02X} debugger={reread:02X}"
            )
            lines.append(
                f"KI_CONTACT phase=store case={number:X} pc=8800A378 "
                f"address={ORACLE.WINDOW_BASE + receiver + 0xC7:016X} value={values[3]:02X}"
            )
        lines.append(f"KI_CONTACT phase=terminal case={number:X} pc=000000008800A37C")
        (directory / f"{number:02d}-window-after.bin").write_bytes(window)
        (directory / f"{number:02d}-table-after.bin").write_bytes(table)
        lines.append(f"KI_CONTACT case={number} done")
    lines.append(f"KI_CONTACT complete={len(cases)}")
    return cases, "\n".join(lines) + "\n"


class ContactInitializerPilotTests(unittest.TestCase):
    def test_pilot_is_cases_1_2_15_plus_changed_seed_repeat(self) -> None:
        full = ORACLE.load_cases()
        pilot = ORACLE.pilot_cases(full)
        self.assertEqual([case["source_case"] for case in pilot], [1, 2, 15, 1])
        self.assertEqual([case["case"] for case in pilot], [1, 2, 3, 4])
        self.assertNotEqual(pilot[0]["receiver_z"], pilot[3]["receiver_z"])
        self.assertNotEqual(pilot[0]["countdown"], pilot[3]["countdown"])
        self.assertEqual(full[0]["name"], "organic_seed")

    def test_script_observes_original_operands_reread_and_terminal(self) -> None:
        directory = ROOT / "work" / "mame" / "contact-init-pilot-unit"
        script = ORACLE.generate_script(
            ORACLE.pilot_cases(ORACLE.load_cases()), directory, pilot=True
        )
        for pc in ("8800a2e8", "8800a308", "8800a310", "8800a344", "8800a34c", "8800a378"):
            self.assertIn(f"bp {pc}", script)
        self.assertIn("cpu=%02X debugger=%02X", script)
        self.assertIn('phase=terminal case=1 pc=%016X', script)
        self.assertIn("do pc=0xffffffff8800a2e4\ng", script)
        self.assertIn("do b@0x88034653=0xa3", script)
        self.assertNotIn("do b@88034653=a3", script)
        self.assertNotIn("fill 8800a2e4", script.lower())

    def test_complete_synthetic_pilot_validates(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            cases, log = synthetic_pilot(directory)
            ORACLE.validate_pilot_observation(cases, directory, log)
            csv_result = ORACLE.reduce_snapshots(cases, directory, log)
            self.assertEqual(len(csv_result.splitlines()), 5)

    def test_wrong_terminal_reordered_or_extra_event_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            cases, log = synthetic_pilot(directory)
            variants = (
                log.replace("pc=000000008800A37C", "pc=000000008800A378", 1),
                log.replace("phase=store", "phase=reread", 1),
                log + "KI_CONTACT phase=terminal case=4 pc=8800A37C\n",
            )
            for changed in variants:
                with self.subTest(changed=changed[-80:]), self.assertRaises(ORACLE.OracleError):
                    ORACLE.validate_pilot_observation(cases, directory, changed)

    def test_unknown_malformed_and_trailing_oracle_lines_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            cases, log = synthetic_pilot(directory)
            variants = (
                log + "KI_CONTACT phase=surprise case=4 pc=8800A37C\n",
                log.replace("value=31", "value=31 trailing", 1),
                log.replace(" address=", " ", 1),
                log.replace("KI_CONTACT case=1 done", "KI_CONTACT case=1 done extra", 1),
            )
            for changed in variants:
                with self.subTest(changed=changed[-100:]), self.assertRaises(ORACLE.OracleError):
                    ORACLE.validate_pilot_observation(cases, directory, changed)

    def test_store_reread_and_bounded_snapshot_disagreement_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            cases, log = synthetic_pilot(directory)
            bad_store = log.replace("value=31", "value=30", 1)
            bad_reread = log.replace(
                "phase=reread case=1 pc=8800A34C cpu=01 debugger=01",
                "phase=reread case=1 pc=8800A34C cpu=01 debugger=02",
                1,
            )
            for changed in (bad_store, bad_reread):
                with self.assertRaises(ORACLE.OracleError):
                    ORACLE.validate_pilot_observation(cases, directory, changed)
            path = directory / "01-window-after.bin"
            window = bytearray(path.read_bytes())
            window[0] ^= 1
            path.write_bytes(window)
            with self.assertRaisesRegex(ORACLE.OracleError, "outside receiver"):
                ORACLE.reduce_snapshots(cases, directory, log)


if __name__ == "__main__":
    unittest.main()
