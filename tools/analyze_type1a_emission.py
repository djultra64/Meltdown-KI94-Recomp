#!/usr/bin/env python3
"""Validate/reduce original-CPU emitter oracles; never a production scheduler."""

from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re
from typing import Iterable


class TraceError(ValueError):
    pass


def oracle_rows(lines: Iterable[str]) -> list[tuple[int, int, int]]:
    rows: list[tuple[int, int, int]] = []
    complete = False
    pattern = re.compile(
        r"KI_EMITTER_ORACLE countdown=([0-9A-F]{2}) cases=100 "
        r"digest=([0-9A-F]{8})"
    )
    for line in lines:
        line = line.strip()
        if "KI_EMITTER_ORACLE" not in line:
            continue
        if complete:
            raise TraceError("oracle data after completion")
        if line == "KI_EMITTER_ORACLE complete=10000":
            complete = True
            continue
        match = pattern.fullmatch(line)
        if not match or int(match[1], 16) != len(rows):
            raise TraceError(f"invalid/missing/reordered oracle row {len(rows)}")
        rows.append((int(match[1], 16), 256, int(match[2], 16)))
    if not complete or len(rows) != 256:
        raise TraceError("incomplete 65,536-case oracle")
    return rows


def organic_events(lines: Iterable[str]) -> list[dict[str, str]]:
    events = []
    for line in lines:
        if not line.startswith("KI_EMIT "):
            continue
        event: dict[str, str] = {}
        for token in line.split()[1:]:
            pair = token.split("=")
            if len(pair) != 2 or pair[0] in event or not pair[1]:
                raise TraceError("malformed/duplicate organic field")
            event[pair[0]] = pair[1]
        if "phase" not in event:
            raise TraceError("missing organic phase")
        events.append(event)
    return events


def summarize_organic(events: list[dict[str, str]], reference: Path) -> dict:
    """Validate the one controlled impact, not arbitrary KI gameplay.

    Framebuffer-bank changes group contiguous rendered passes only. This is
    observational time indexing, not recovery of a video interrupt or caller.
    Events before the next render group belong to that group's update phase;
    the terminal release is one update after the final visible group.
    """
    def require(condition: bool, message: str) -> None:
        if not condition:
            raise TraceError(message)

    def number(event: dict[str, str], field: str) -> int:
        try:
            return int(event[field], 16)
        except (KeyError, ValueError) as error:
            raise TraceError(f"invalid {event.get('phase')} field {field}") from error

    counts = Counter(e["phase"] for e in events)
    expected = dict(init_entry=1, init_table=1, init_c4=1, init_c5=1,
                    init_c6=1, init_c7=1, step=49, decision=49, spawn=7,
                    allocated=7, update=322, render=315, release=7)
    require(counts == expected, f"incomplete/unknown organic phases: {dict(counts)}")
    require([e["phase"] for e in events[:6]] ==
            ["init_entry", "init_table", "init_c4", "init_c5", "init_c6", "init_c7"],
            "initializer phases reordered")
    for index, event in enumerate(events):
        successor = {"step": "decision", "spawn": "allocated"}.get(event["phase"])
        if successor is not None:
            require(index + 1 < len(events) and events[index + 1]["phase"] == successor,
                    f"missing/reordered {successor}")
    for field, value in (("c4", 0x31), ("c5", 0x78), ("c6", 0x55), ("c7", 0)):
        entry = next(e for e in events if e["phase"] == f"init_{field}")
        require(number(entry, "value") == value, f"unexpected initial {field}")

    frames: list[list[tuple[int, int]]] = []
    pending: list[dict[str, str]] = []
    timed: list[tuple[int, dict[str, str]]] = []
    bank = None
    for event in events:
        if event["phase"] != "render":
            pending.append(event)
            continue
        next_bank = number(event, "bank")
        require(next_bank in (0, 1), "invalid bank")
        if next_bank != bank:
            frames.append([])
            bank = next_bank
            timed.extend((len(frames) - 1, e) for e in pending)
            pending.clear()
        require(not pending, "update interleaved inside a render group")
        frames[-1].append((number(event, "object") & 0xffffffff,
                           number(event, "token")))
    timed.extend((len(frames), e) for e in pending)
    require(len(frames) == 96, "expected 96 visible groups")
    with reference.open(newline="") as handle:
        reference_rows = list(csv.DictReader(line for line in handle
                                            if not line.startswith("#")))
    reference_events = [(int(r["tick"]), int(r["object"], 16) & 0xffffffff,
                         int(r["token"], 16)) for r in reference_rows]
    rendered = [(tick, obj, token) for tick, frame in enumerate(frames)
                for obj, token in frame]
    require(rendered == reference_events, "render order/token differs from POC-110 oracle")

    steps = [(t, e) for t, e in timed if e["phase"] == "step"]
    decisions = [e for _, e in timed if e["phase"] == "decision"]
    for index, ((tick, step), decision) in enumerate(zip(steps, decisions)):
        cadence = 0x78 if index == 0 else 8 + ((index - 1) % 8) * 16
        require(number(step, "index") == index and
                number(step, "countdown") == 49 - index and
                number(step, "cadence") == cadence and number(step, "gp") == 1,
                f"emitter input mismatch at invocation {index}")
        require(number(decision, "index") == index and
                number(decision, "countdown_after") == 48 - index and
                number(decision, "cadence_high") == (cadence + 16) >> 4 and
                number(decision, "cadence_low") == 8 and
                number(decision, "spawn") == (index % 8 == 0),
                f"emitter decision mismatch at invocation {index}")
        require(tick == (index if index == 0 else index + 3),
                f"unexpected receiver update group at invocation {index}")

    spawn_events = [(t, e) for t, e in timed if e["phase"] == "spawn"]
    allocations = [(t, e) for t, e in timed if e["phase"] == "allocated"]
    objects = [0x8808c000, 0x8808be00, 0x8808bf00, 0x8808c100,
               0x8808c200, 0x8808c300, 0x8808c000]
    spawn_ticks = [0, 11, 19, 27, 35, 43, 51]
    for ordinal, ((tick, spawn), (allocated_tick, allocation)) in enumerate(
            zip(spawn_events, allocations)):
        require(tick == allocated_tick == spawn_ticks[ordinal] and
                number(spawn, "index") == ordinal * 8 and
                number(spawn, "ordinal") == number(allocation, "ordinal") == ordinal and
                number(spawn, "countdown_after") == 48 - ordinal * 8 and
                number(spawn, "cadence_reset") == 8 and
                number(spawn, "gp") == number(allocation, "gp") == 1 and
                number(allocation, "object") & 0xffffffff == objects[ordinal],
                f"spawn/allocation mismatch at ordinal {ordinal}")
    updates = [e for _, e in timed if e["phase"] == "update"]
    require([number(e, "sequence") for e in updates] == list(range(322)),
            "update sequence lost or reordered")
    releases = [(t, number(e, "object") & 0xffffffff) for t, e in timed
                if e["phase"] == "release"]
    require(releases == [(tick + 45, obj) for tick, obj in zip(spawn_ticks, objects)],
            "release timing/order mismatch")
    actual_updates = [(t, number(e, "object") & 0xffffffff) for t, e in timed
                      if e["phase"] == "update"]
    expected_updates = [(tick, obj) for tick in range(97)
                        for obj in sorted(obj for start, obj in zip(spawn_ticks, objects)
                                          if start <= tick <= start + 45)]
    require(actual_updates == expected_updates, "update object/traversal order mismatch")
    require(all(number(e, "script") == 0 for _, e in timed
                if e["phase"] == "release"), "release before animation termination")
    # Digest all parsed events including the observed traversal order. Legacy
    # exploratory logs mislabeled allocator a2 as 'countdown'; it is not state
    # evidence and is deliberately excluded when comparing those local runs.
    normalized = [dict(e) for e in events]
    for e in normalized:
        if e["phase"] == "allocated":
            e.pop("countdown", None)
    digest = hashlib.sha256(json.dumps(normalized, sort_keys=True).encode()).hexdigest()
    return dict(counts=dict(counts), visible_ticks=len(frames), spawn_ticks=spawn_ticks,
                peak_particles=max(map(len, frames)), release_ticks=[t for t, _ in releases],
                event_sha256=digest)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("oracle", "organic"))
    parser.add_argument("trace", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--reference", type=Path,
                        default=Path("tests/fixtures/ki15d_type1a_state_to_render.csv"))
    args = parser.parse_args()
    try:
        with args.trace.open() as handle:
            if args.mode == "oracle":
                rows = oracle_rows(handle)
                if args.output is None:
                    parser.error("oracle mode requires --output")
                with args.output.open("w", newline="") as output:
                    writer = csv.writer(output, lineterminator="\n")
                    writer.writerow(("countdown", "cases", "digest"))
                    writer.writerows((f"{c:02x}", n, f"{d:08x}") for c, n, d in rows)
                print(f"Validated 65,536 original-instruction cases: {args.output}")
            else:
                summary = summarize_organic(organic_events(handle), args.reference)
                print(json.dumps(summary, indent=2))
    except (OSError, TraceError) as error:
        parser.exit(1, f"emission trace error: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
