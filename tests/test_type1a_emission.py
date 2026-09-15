from __future__ import annotations

import copy
import csv
import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "analyze_type1a_emission", ROOT / "tools/analyze_type1a_emission.py")
assert SPEC is not None and SPEC.loader is not None
ANALYZER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ANALYZER)
REFERENCE = ROOT / "tests/fixtures/ki15d_type1a_state_to_render.csv"


def oracle_lines() -> list[str]:
    with (ROOT / "tests/fixtures/ki15d_88003d30.csv").open() as handle:
        rows = list(csv.DictReader(handle))
    return [f"KI_EMITTER_ORACLE countdown={r['countdown'].upper()} cases=100 "
            f"digest={r['digest'].upper()}" for r in rows] + [
                "KI_EMITTER_ORACLE complete=10000"]


def synthetic_organic() -> list[dict[str, str]]:
    """Parser test data only; NEVER an emulator-derived production schedule."""
    events = [{"phase": "init_entry"}, {"phase": "init_table"}]
    for field, value in (("c4", "31"), ("c5", "78"), ("c6", "55"), ("c7", "00")):
        events.append(dict(phase=f"init_{field}", value=value))
    with REFERENCE.open() as handle:
        renders = list(csv.DictReader(line for line in handle if not line.startswith("#")))
    spawns = [(0, 0x8808c000), (11, 0x8808be00), (19, 0x8808bf00),
              (27, 0x8808c100), (35, 0x8808c200), (43, 0x8808c300), (51, 0x8808c000)]
    sequence = 0
    for tick in range(97):
        if tick == 0 or 4 <= tick <= 51:
            index = 0 if tick == 0 else tick - 3
            cadence = 0x78 if index == 0 else 8 + ((index - 1) % 8) * 16
            events.append(dict(phase="step", index=f"{index:X}",
                               countdown=f"{49-index:X}", cadence=f"{cadence:X}", gp="1"))
            events.append(dict(phase="decision", index=f"{index:X}",
                               countdown_after=f"{48-index:X}",
                               cadence_high=f"{(cadence+16)>>4:X}", cadence_low="8",
                               spawn=str(int(index % 8 == 0))))
            if index % 8 == 0:
                ordinal = index // 8
                events.append(dict(phase="spawn", index=f"{index:X}", ordinal=f"{ordinal:X}",
                                   countdown_after=f"{48-index:X}", cadence_reset="8", gp="1"))
                events.append(dict(phase="allocated", ordinal=f"{ordinal:X}",
                                   object=f"{spawns[ordinal][1]:X}", gp="1"))
        for obj, start in sorted((obj, start) for start, obj in spawns
                                 if start <= tick <= start + 45):
            events.append(dict(phase="update", object=f"{obj:X}", sequence=f"{sequence:X}"))
            sequence += 1
            if tick == start + 45:
                events.append(dict(phase="release", object=f"{obj:X}", script="0"))
        events.extend(dict(phase="render", object=r["object"], token=r["token"],
                           bank=str(tick % 2)) for r in renders if int(r["tick"]) == tick)
    return events


class EmissionAnalyzerTests(unittest.TestCase):
    def test_complete_original_digest_fixture(self) -> None:
        self.assertEqual(len(ANALYZER.oracle_rows(oracle_lines())), 256)

    def test_oracle_requires_order_count_and_completion(self) -> None:
        original = oracle_lines()
        variants = [original[:-1], original[1:], original + [original[0]],
                    [original[0].replace("cases=100", "cases=FF")] + original[1:],
                    [original[0].replace("digest=", "digest=Z")] + original[1:]]
        for lines in variants:
            with self.subTest(lines=lines[:1]), self.assertRaises(ANALYZER.TraceError):
                ANALYZER.oracle_rows(lines)

    def test_complete_synthetic_event_validation(self) -> None:
        result = ANALYZER.summarize_organic(synthetic_organic(), REFERENCE)
        self.assertEqual(result["visible_ticks"], 96)
        self.assertEqual(result["peak_particles"], 6)
        self.assertEqual(result["release_ticks"], [45, 56, 64, 72, 80, 88, 96])

    def test_rejects_incomplete_event_run(self) -> None:
        with self.assertRaisesRegex(ANALYZER.TraceError, "incomplete"):
            ANALYZER.summarize_organic(synthetic_organic()[:-1], REFERENCE)

    def test_changed_state_order_and_identity_are_rejected(self) -> None:
        events = synthetic_organic()
        mutations = [("step", "countdown", "0"), ("decision", "spawn", "0"),
                     ("allocated", "object", "8808be00"), ("render", "token", "ff"),
                     ("update", "object", "8808be00"), ("release", "script", "1")]
        for phase, field, value in mutations:
            changed = copy.deepcopy(events)
            next(e for e in changed if e["phase"] == phase)[field] = value
            with self.subTest(phase=phase), self.assertRaises(ANALYZER.TraceError):
                ANALYZER.summarize_organic(changed, REFERENCE)
        events[6], events[7] = events[7], events[6]
        with self.assertRaisesRegex(ANALYZER.TraceError, "reordered"):
            ANALYZER.summarize_organic(events, REFERENCE)

    def test_parse_rejects_duplicate_and_malformed_fields(self) -> None:
        for line in ("KI_EMIT phase=step phase=step", "KI_EMIT index=0",
                     "KI_EMIT phase=step broken", "KI_EMIT phase="):
            with self.subTest(line=line), self.assertRaises(ANALYZER.TraceError):
                ANALYZER.organic_events([line])


if __name__ == "__main__":
    unittest.main()
