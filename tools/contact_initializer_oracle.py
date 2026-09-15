#!/usr/bin/env python3
"""Generate controlled MAME cases and reduce ignored snapshots to scalar evidence."""

from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import io
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ROOT / "tests/fixtures/ki15d_8800a2e4_inputs.csv"
FIELDS = "case,name,receiver,attacker,descriptor,effect,flags,height,receiver_z,attacker_z,variant,countdown,cadence,row_variant".split(",")
WINDOW_BASE = 0x88090000
WINDOW_SIZE = 0x4000
TABLE_BASE = 0x88034610
TABLE_SIZE = 0x80
SEGMENT_SHA256 = "5dd923067b1a3398d4d26b6558e46cf396ab69b66afd5f8b183885b820e79b79"
BLOCK_OFFSET = 0xA2E4
BLOCK_SIZE = 152
BLOCK_SHA256 = "8d10efb5394ef73b8ad2e8e909097d8162086196e2ac520a9e25638c321a1555"
MAME_VERSION = "0.289"
MAME_FLATPAK_COMMIT = "41ebdb5c266627f4d1f02ba11f8109e793d2381081b68afc48714dd48a703788"


class OracleError(ValueError):
    pass


def load_cases(path: Path = INPUTS) -> list[dict]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != FIELDS:
            raise OracleError("invalid input columns")
        cases = []
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise OracleError("invalid input column count")
            if not re.fullmatch(r"[a-z][a-z0-9_]{0,47}", row["name"]):
                raise OracleError("invalid case name")
            case = {"name": row["name"]}
            try:
                case["case"] = int(row["case"], 10)
                for field in FIELDS[2:]:
                    if not re.fullmatch(r"[0-9a-fA-F]+", row[field]):
                        raise ValueError(field)
                    case[field] = int(row[field], 16)
            except ValueError as error:
                raise OracleError("invalid input number") from error
            if case["case"] != len(cases) + 1:
                raise OracleError("missing/reordered input case")
            for field in FIELDS[2:]:
                limit = (WINDOW_SIZE - 0x100 if field in ("receiver", "attacker", "descriptor")
                         else 0xffffffff if field.endswith("_z") else 0xff)
                if case[field] > limit:
                    raise OracleError(f"out-of-range {field}")
            if case["receiver"] % 4 or case["attacker"] % 4:
                raise OracleError("unaligned position record")
            cases.append(case)
    if len(cases) != 18:
        raise OracleError("expected the complete 18-case suite")
    return cases


def writes(case: dict) -> list[tuple[int, int, int]]:
    """Ordered synthetic input stores shared by the script and snapshot check.

    Mapping bytes are seeded before the selected row: overlapping addresses
    intentionally obey last-write semantics. Aliased records do too.
    """
    stores = [(0x88034647 + index, 1, (index * 13 + 7) & 0xff)
              for index in range(16)]
    row = TABLE_BASE + (case["effect"] & 15) * 8
    stores += [(row + offset, 1, case[field]) for offset, field in
               enumerate(("countdown", "cadence", "row_variant"))]
    receiver = WINDOW_BASE + case["receiver"]
    attacker = WINDOW_BASE + case["attacker"]
    descriptor = WINDOW_BASE + case["descriptor"]
    stores += [(receiver + 0x0c, 4, case["receiver_z"]),
               (attacker + 0x0c, 4, case["attacker_z"]),
               (attacker + 0x8e, 1, case["variant"]),
               (descriptor + 0x1d, 1, case["effect"]),
               (descriptor + 0x13, 1, case["flags"]),
               (descriptor + 0x1a, 1, case["height"])]
    return stores


def initial_regions(case: dict) -> tuple[bytearray, bytearray]:
    window = bytearray([0x5a]) * WINDOW_SIZE
    table = bytearray([0x66]) * TABLE_SIZE
    for address, size, value in writes(case):
        region, base = (window, WINDOW_BASE) if address >= WINDOW_BASE else (table, TABLE_BASE)
        region[address - base:address - base + size] = value.to_bytes(size, "little")
    return window, table


def bounded_digest(window: bytes, table: bytes) -> int:
    digest = 0xcbf29ce484222325
    for byte in window + table:
        digest = ((digest ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return digest


def local_run_directory(path: Path) -> Path:
    resolved = path.resolve()
    if not resolved.is_relative_to(ROOT / "work"):
        raise OracleError("oracle scripts/snapshots must remain under ignored work/")
    if not re.fullmatch(r"[a-zA-Z0-9_./-]+", resolved.relative_to(ROOT).as_posix()):
        raise OracleError("use a simple work/ path for MAME script filenames")
    return resolved


def fresh_output_path(path: Path, label: str) -> Path:
    resolved = path.resolve()
    if not resolved.is_relative_to(ROOT / "work") or resolved.exists():
        raise OracleError(f"{label} must be a fresh path under ignored work/")
    resolved.parent.mkdir(parents=True, exist_ok=True)
    return resolved


def pilot_cases(cases: list[dict]) -> list[dict]:
    """Return the finite cache/completion pilot without changing the full fixture."""
    selected = [copy.deepcopy(cases[index - 1]) for index in (1, 2, 15)]
    repeated = copy.deepcopy(cases[0])
    repeated.update({
        "name": "organic_seed_changed",
        "receiver_z": 0x00001200,
        "attacker_z": 0x0000A900,
        "variant": 0x5D,
        "countdown": 0xA6,
        "cadence": 0x3C,
        "row_variant": 0x07,
    })
    selected.append(repeated)
    for sequence, case in enumerate(selected, 1):
        case["source_case"] = case["case"]
        case["case"] = sequence
    return selected


def generate_script(cases: list[dict], directory: Path, *, pilot: bool = False) -> str:
    prefix = directory.relative_to(ROOT).as_posix()
    lines = ["# Generated synthetic contact initializer oracle; original block unpatched.",
             "focus maincpu", "step 2", "load work/kipack/ki15d/rom-0.bin,88000000",
             "bp 8800a37c"]
    if pilot:
        # Breakpoints observe operands before each original store. At 0xa34c,
        # $t6 is the value loaded by the descriptor reread at 0xa348.
        lines += [
            'bp 8800a2e8,1,{logerror "KI_CONTACT phase=effect case=%X pc=8800A2E8 cpu=%02X debugger=%02X\\n",temp9,t6,b@(t5+0x1d) ; g}',
            'bp 8800a308,1,{logerror "KI_CONTACT phase=store case=%X pc=8800A308 address=%016X value=%02X\\n",temp9,fp+0xc4,t6 ; g}',
            'bp 8800a310,1,{logerror "KI_CONTACT phase=store case=%X pc=8800A310 address=%016X value=%02X\\n",temp9,fp+0xc5,t6 ; g}',
            'bp 8800a344,1,{logerror "KI_CONTACT phase=store case=%X pc=8800A344 address=%016X value=%02X\\n",temp9,fp+0xc6,a2 ; g}',
            'bp 8800a34c,1,{logerror "KI_CONTACT phase=reread case=%X pc=8800A34C cpu=%02X debugger=%02X\\n",temp9,t6,b@(t5+0x1d) ; g}',
            'bp 8800a378,1,{logerror "KI_CONTACT phase=store case=%X pc=8800A378 address=%016X value=%02X\\n",temp9,fp+0xc7,a2 ; g}',
        ]
    for case in cases:
        number = case["case"]
        lines += [f"# Case {number}: {case['name']}", "fill 88090000,4000,5a",
                  "fill 88034610,80,66"]
        for address, size, value in writes(case):
            lines.append(f"do {'b' if size == 1 else 'd'}@0x{address:x}=0x{value:x}")
        for region, base, size in (("window", WINDOW_BASE, WINDOW_SIZE),
                                   ("table", TABLE_BASE, TABLE_SIZE)):
            lines.append(f"save {prefix}/{number:02d}-{region}-before.bin,{base:x},{size:x}")
        lines += [f"do temp9=0x{number:x}",
                  f"do fp=0xffffffff{WINDOW_BASE + case['receiver']:08x}",
                  f"do t4=0xffffffff{WINDOW_BASE + case['attacker']:08x}",
                  f"do t5=0xffffffff{WINDOW_BASE + case['descriptor']:08x}",
                  "do pc=0xffffffff8800a2e4", "g"]
        if pilot:
            lines.append(f'logerror "KI_CONTACT phase=terminal case={number:X} pc=%016X\\n",pc')
        for region, base, size in (("window", WINDOW_BASE, WINDOW_SIZE),
                                   ("table", TABLE_BASE, TABLE_SIZE)):
            lines.append(f"save {prefix}/{number:02d}-{region}-after.bin,{base:x},{size:x}")
        lines.append(f'logerror "KI_CONTACT case={number} done\\n"')
    lines += [f'logerror "KI_CONTACT complete={len(cases)}\\n"', "quit"]
    return "\n".join(lines) + "\n"


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_run_metadata(directory: Path, script: str, cases: list[dict], suite: str) -> None:
    segment = (ROOT / "work/kipack/ki15d/rom-0.bin").read_bytes()
    metadata = {
        "schema": f"contact-init-{suite}-v1",
        "source_segment": {"size": len(segment), "sha256": sha256_bytes(segment)},
        "original_block": {
            "offset": BLOCK_OFFSET,
            "size": BLOCK_SIZE,
            "sha256": sha256_bytes(segment[BLOCK_OFFSET:BLOCK_OFFSET + BLOCK_SIZE]),
            "entry": "0x8800a2e4",
            "terminal_pc": "0x8800a37c",
        },
        "script_sha256": sha256_bytes(script.encode()),
        "inputs_sha256": sha256_bytes(INPUTS.read_bytes()),
        "tool_sha256": sha256_bytes(Path(__file__).read_bytes()),
        "emulator": {"name": "MAME Flatpak", "version": MAME_VERSION,
                     "flatpak_commit": MAME_FLATPAK_COMMIT, "cpu_mode": "-nodrc"},
        "external_timeout_seconds": 20 if suite == "cache-pilot" else 30,
        "cases": [{"sequence": case["case"],
                   "fixture_case": case.get("source_case", case["case"]),
                   "name": case["name"]} for case in cases],
    }
    (directory / "pilot-metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")


PILOT_EVENT_RE = re.compile(
    r"KI_CONTACT phase=(effect|store|reread|terminal) case=([0-9A-F]+) "
    r"pc=([0-9A-F]+)(?: address=([0-9A-F]+) value=([0-9A-F]+)| "
    r"cpu=([0-9A-F]+) debugger=([0-9A-F]+))?", re.IGNORECASE
)
MARKER_RE = re.compile(r"KI_CONTACT (?:case=\d+ done|complete=\d+)")


def parse_contact_log(log: str) -> tuple[list[tuple], list[str]]:
    """Reject every unknown or malformed harness line, while allowing prefixes."""
    events = []
    markers = []
    for line in log.splitlines():
        position = line.find("KI_CONTACT")
        if position < 0:
            continue
        payload = line[position:].strip()
        event = PILOT_EVENT_RE.fullmatch(payload)
        marker = MARKER_RE.fullmatch(payload)
        if event is not None:
            groups = event.groups()
            if ((groups[0].lower() == "store" and (groups[3] is None or groups[4] is None))
                    or (groups[0].lower() in ("effect", "reread")
                        and (groups[5] is None or groups[6] is None))
                    or (groups[0].lower() == "terminal"
                        and any(value is not None for value in groups[3:]))):
                raise OracleError("malformed CPU observation event")
            events.append(groups)
        elif marker is not None:
            markers.append(marker.group(0))
        else:
            raise OracleError(f"unknown/malformed oracle line: {payload}")
    return events, markers


def validate_pilot_observation(cases: list[dict], directory: Path, log: str) -> None:
    events, _ = parse_contact_log(log)
    position = 0
    for case in cases:
        number = case["case"]
        enabled = case["effect"] != 0
        expected_phases = ["effect"] + (
            ["store", "store", "store", "reread", "store"] if enabled else []
        ) + ["terminal"]
        current = events[position:position + len(expected_phases)]
        if [event[0] for event in current] != expected_phases or any(
            int(event[1], 16) != number for event in current
        ):
            raise OracleError(f"case {number}: missing/reordered CPU observation events")
        position += len(current)
        terminal = current[-1]
        if int(terminal[2], 16) & 0xFFFFFFFF != 0x8800A37C:
            raise OracleError(f"case {number}: unexpected terminal PC")
        effect = current[0]
        if (int(effect[2], 16) != 0x8800A2E8
                or int(effect[5], 16) != int(effect[6], 16)
                or int(effect[5], 16) != case["effect"]):
            raise OracleError(f"case {number}: initial effect CPU/debugger read mismatch")
        if not enabled:
            continue
        window = (directory / f"{number:02d}-window-after.bin").read_bytes()
        receiver = WINDOW_BASE + case["receiver"]
        stores = [event for event in current if event[0] == "store"]
        table = (directory / f"{number:02d}-table-before.bin").read_bytes()
        row_offset = (case["effect"] & 15) * 8
        for event, offset, expected_pc in zip(
            stores, (0xC4, 0xC5, 0xC6, 0xC7),
            (0x8800A308, 0x8800A310, 0x8800A344, 0x8800A378)
        ):
            if int(event[2], 16) != expected_pc:
                raise OracleError(f"case {number}: unexpected store PC")
            if int(event[3], 16) & 0xFFFFFFFF != receiver + offset:
                raise OracleError(f"case {number}: unexpected store address")
            if int(event[4], 16) & 0xFF != window[case["receiver"] + offset]:
                raise OracleError(f"case {number}: CPU store differs from saved memory")
        if any((int(stores[index][4], 16) & 0xFF) != table[row_offset + index]
               for index in (0, 1)):
            raise OracleError(f"case {number}: CPU table read differs from before-image")
        reread = current[4]
        if (int(reread[2], 16) != 0x8800A34C
                or int(reread[5], 16) != int(reread[6], 16)):
            raise OracleError(f"case {number}: descriptor CPU/debugger reread mismatch")
    if position != len(events):
        raise OracleError("unexpected extra CPU observation event")


def pilot_report(directory: Path, csv_result: bytes) -> dict:
    metadata_path = directory / "pilot-metadata.json"
    metadata = json.loads(metadata_path.read_text())
    script = (directory / "oracle.cmd").read_bytes()
    segment = (ROOT / "work/kipack/ki15d/rom-0.bin").read_bytes()
    checks = {
        "source_segment": (len(segment), sha256_bytes(segment)),
        "original_block": (BLOCK_SIZE, sha256_bytes(segment[BLOCK_OFFSET:BLOCK_OFFSET + BLOCK_SIZE])),
        "script": (len(script), sha256_bytes(script)),
        "inputs": (INPUTS.stat().st_size, sha256_bytes(INPUTS.read_bytes())),
        "tool": (Path(__file__).stat().st_size, sha256_bytes(Path(__file__).read_bytes())),
    }
    expected = {
        "source_segment": (metadata["source_segment"]["size"], metadata["source_segment"]["sha256"]),
        "original_block": (metadata["original_block"]["size"], metadata["original_block"]["sha256"]),
        "script": (len(script), metadata["script_sha256"]),
        "inputs": (INPUTS.stat().st_size, metadata["inputs_sha256"]),
        "tool": (Path(__file__).stat().st_size, metadata["tool_sha256"]),
    }
    if checks != expected or metadata["emulator"] != {
        "name": "MAME Flatpak", "version": MAME_VERSION,
        "flatpak_commit": MAME_FLATPAK_COMMIT, "cpu_mode": "-nodrc"
    }:
        raise OracleError("pilot source/script/input/emulator identity mismatch")
    artifacts = {}
    for path in sorted(directory.glob("*.bin")) + [directory / "run.log"]:
        data = path.read_bytes()
        artifacts[path.name] = {"size": len(data), "sha256": sha256_bytes(data)}
    return {"schema": f"{metadata['schema']}-result", "category": "PASS",
            "identities": metadata, "artifacts": artifacts,
            "scalar_csv_sha256": sha256_bytes(csv_result)}


def reduce_snapshots(cases: list[dict], directory: Path, log: str) -> str:
    _, markers = parse_contact_log(log)
    expected_markers = [f"KI_CONTACT case={case['case']} done" for case in cases]
    expected_markers.append(f"KI_CONTACT complete={len(cases)}")
    if markers != expected_markers:
        raise OracleError("incomplete/malformed/reordered oracle run")
    output = io.StringIO()
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(("case", "c4", "c5", "c6", "c7", "bounded_fnv64"))
    for case in cases:
        number = case["case"]
        expected_window, expected_table = initial_regions(case)
        for region, expected in (("window", expected_window), ("table", expected_table)):
            before = (directory / f"{number:02d}-{region}-before.bin").read_bytes()
            if before != expected:
                raise OracleError(f"case {number}: {region} initial state mismatch")
        window = (directory / f"{number:02d}-window-after.bin").read_bytes()
        table = (directory / f"{number:02d}-table-after.bin").read_bytes()
        if len(window) != WINDOW_SIZE or table != expected_table:
            raise OracleError(f"case {number}: bounded size/table mutation")
        offset = case["receiver"] + 0xc4
        expected_window[offset:offset + 4] = window[offset:offset + 4]
        if window != expected_window:
            raise OracleError(f"case {number}: write outside receiver c4..c7")
        writer.writerow((number, *(f"{byte:02x}" for byte in window[offset:offset + 4]),
                         f"{bounded_digest(window, table):016x}"))
    return output.getvalue()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "mode", choices=("generate", "reduce", "pilot-generate", "pilot-reduce",
                         "full-generate", "full-reduce")
    )
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        cases = load_cases()
        directory = local_run_directory(args.directory)
        if args.mode in ("generate", "pilot-generate", "full-generate"):
            segment = (ROOT / "work/kipack/ki15d/rom-0.bin").read_bytes()
            if (hashlib.sha256(segment).hexdigest() != SEGMENT_SHA256
                    or hashlib.sha256(segment[BLOCK_OFFSET:BLOCK_OFFSET + BLOCK_SIZE]).hexdigest() != BLOCK_SHA256):
                raise OracleError("wrong original segment fingerprint")
            # A fresh directory prevents stale snapshots surviving a failed run.
            directory.mkdir(parents=True, exist_ok=False)
            run_cases = pilot_cases(cases) if args.mode == "pilot-generate" else cases
            hardened = args.mode in ("pilot-generate", "full-generate")
            script = generate_script(run_cases, directory, pilot=hardened)
            (directory / "oracle.cmd").write_text(script)
            if hardened:
                write_run_metadata(
                    directory, script, run_cases,
                    "cache-pilot" if args.mode == "pilot-generate" else "full-18"
                )
            print(directory / "oracle.cmd")
        else:
            if args.output is None:
                parser.error("reduce requires --output for the scalar CSV")
            output_path = fresh_output_path(args.output, "scalar output")
            run_cases = pilot_cases(cases) if args.mode == "pilot-reduce" else cases
            log = (directory / "run.log").read_text()
            if args.mode in ("pilot-reduce", "full-reduce"):
                if args.report is None:
                    parser.error(f"{args.mode} requires --report")
                report_path = fresh_output_path(args.report, "evidence report")
                if report_path == output_path:
                    raise OracleError("pilot report and scalar output must differ")
                validate_pilot_observation(run_cases, directory, log)
            result = reduce_snapshots(run_cases, directory, log)
            if args.mode in ("pilot-reduce", "full-reduce"):
                report = json.dumps(
                    pilot_report(directory, result.encode()), indent=2, sort_keys=True
                ) + "\n"
                output_path.write_text(result)
                report_path.write_text(report)
            else:
                output_path.write_text(result)
            print(f"Validated {len(run_cases)} original-CPU bounded-memory cases")
    except (OSError, OracleError) as error:
        parser.exit(1, f"contact oracle error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
