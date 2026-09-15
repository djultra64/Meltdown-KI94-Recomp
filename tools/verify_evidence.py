#!/usr/bin/env python3
"""Check and replay two fixed reviewed KI v1.5d verification anchors."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import sys
from typing import Callable


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = Path("provenance/verification.json")
EXPECTED_IDENTITY = {"mame_set": "kinst", "revision": "v1.5d", "cpu": "R4600LE"}
EXPECTED_IMPLEMENTATION_BASE = "289f2dc064715ef33781254dc1dcb3d621f1022e"
EXPECTED_SEGMENT = {
    "index": 0,
    "load_address": "0x08000000",
    "size": 208048,
    "sha256": "5dd923067b1a3398d4d26b6558e46cf396ab69b66afd5f8b183885b820e79b79",
}
EXPECTED_ADAPTERS = {
    "emitter-decisions": "emitter-decisions-v1",
    "type1a-bridge": "type1a-bridge-v1",
}
EXPECTED_PRIVATE_PATHS = {
    "emitter-decisions": "work/mame/traces/review-poc120a-oracle.log",
    "type1a-bridge": "work/mame/traces/ki15d-type1a-state-to-render-a.log",
}
EXPECTED_PUBLIC_PATHS = {
    "emitter-decisions": {
        "provenance/functions/ki15d_88003d30.json",
        "provenance/asm/ki15d_88003d30.s",
        "mame/oracle_ki15d_88003d30.cmd",
        "tools/analyze_type1a_emission.py",
        "tests/fixtures/ki15d_88003d30.csv",
        "native/src/original/ki15d_88003d30.c",
        "native/tests/test_ki15d_88003d30_decision.c",
        "native/tests/test_ki15d_88003d30.c",
    },
    "type1a-bridge": {
        "provenance/functions/ki15d_88001b90_type1a.json",
        "provenance/asm/ki15d_88001b90_type1a.s",
        "mame/trace_ki15d_type1a_state_to_render.cmd",
        "mame/autoplay_jago_vs_idle_fulgore.lua",
        "tools/analyze_type1a_state_to_render.py",
        "tests/fixtures/ki15d_88001b90_type1a_bridge.csv",
        "tests/fixtures/ki15d_type1a_state_to_render.csv",
        "native/src/original/ki15d_88001b90_type1a.c",
        "native/tests/test_ki15d_88001b90_type1a.c",
    },
}
# The manifest describes two already-reviewed legacy anchors, rather than an
# extensible evidence format.  These versioned digests pin every descriptive
# field whose wording defines what was run, observed, excluded, or still
# unknown.  They deliberately exclude identities already checked structurally
# below (paths, hashes, counts and source bounds).  Changing a claim therefore
# requires code review and cannot silently self-approve by editing the JSON.
SEMANTIC_CONTRACT_VERSION = "verify-095-v1"
EXPECTED_SEMANTIC_SHA256 = {
    "emitter-decisions": "33f3cd71d635d26ef7c694fd926312bcc04605331e9ba2982ebd60136a6dac24",
    "type1a-bridge": "b6eae7941aa5e777d917be3cd2c8237731c3653245639b120df4b206211eb57f",
}
EXPECTED_SEGMENT_LIMITS_SHA256 = "e17df10cbc4165f34725feb8c6399e5e8a9bf969a46bfe7f73714e2ef9feda62"
SEMANTIC_FIELDS = (
    "evidence_type",
    "source",
    "abi",
    "initialization",
    "bounded_memory",
    "cache_observation",
    "interceptions",
    "acquisition",
    "admission",
    "completion_rule",
    "native_verification",
    "known_limits",
)
SHA256_RE = re.compile(r"[0-9a-f]{64}")


class VerificationError(ValueError):
    """Invalid, stale, corrupt, or divergent verification evidence."""


def _no_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_manifest(path: Path) -> object:
    try:
        return json.loads(
            path.read_text(encoding="utf-8"), object_pairs_hook=_no_duplicate_keys
        )
    except (OSError, UnicodeError, json.JSONDecodeError, VerificationError) as exc:
        raise VerificationError(f"cannot read manifest {path}: {exc}") from exc


def _safe_path(value: object, label: str) -> str:
    if not isinstance(value, str) or not value or "\\" in value or "\0" in value:
        raise VerificationError(f"{label} must be a portable repository-relative path")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
        raise VerificationError(f"{label} escapes or does not normalize within the repository")
    return value


def _safe_file(root: Path, relative: str, *, required: bool) -> Path | None:
    current = root
    for part in PurePosixPath(relative).parts:
        current = current / part
        if current.is_symlink():
            raise VerificationError(f"symlink evidence path is not allowed: {relative}")
    if not current.is_file():
        if required:
            raise VerificationError(f"required evidence is missing: {relative}")
        return None
    return current


def _hash_file(path: Path) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    try:
        with path.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                size += len(block)
                digest.update(block)
    except OSError as exc:
        raise VerificationError(f"cannot read evidence {path}: {exc}") from exc
    return size, digest.hexdigest()


def _require_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise VerificationError(f"{label} must be a non-empty string")
    return value


def _require_strings(value: object, label: str) -> list[str]:
    if not isinstance(value, list) or not value or any(
        not isinstance(item, str) or not item.strip() for item in value
    ):
        raise VerificationError(f"{label} must be a non-empty string array")
    return value


def _hex_int(value: object, label: str) -> int:
    if not isinstance(value, str) or re.fullmatch(r"0x[0-9a-fA-F]+", value) is None:
        raise VerificationError(f"{label} must be hexadecimal")
    return int(value, 16)


def _sha(value: object, label: str) -> str:
    if not isinstance(value, str) or SHA256_RE.fullmatch(value) is None:
        raise VerificationError(f"{label} must be a lowercase SHA-256")
    return value


def _require_object(value: object, label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise VerificationError(f"{label} must be an object")
    return value


def _require_exact_keys(value: dict[str, object], expected: set[str], label: str) -> None:
    missing = sorted(expected - set(value))
    extra = sorted(set(value) - expected)
    if missing or extra:
        raise VerificationError(f"{label} has missing keys {missing} and extra keys {extra}")


def _semantic_digest(anchor: dict[str, object]) -> str:
    projection = {field: anchor[field] for field in SEMANTIC_FIELDS}
    # Source bytes/bounds are validated independently; only the human-meaningful
    # exit boundary belongs in this descriptive contract.
    projection["source"] = {"exit": _require_object(anchor["source"], "source")["exit"]}
    encoded = json.dumps(projection, sort_keys=True, separators=(",", ":")).encode()
    return hashlib.sha256(encoded).hexdigest()


def _load_function_record(root: Path, relative: str) -> dict[str, object]:
    path = _safe_file(root, relative, required=True)
    assert path is not None
    record = load_manifest(path)
    return _require_object(record, f"function record {relative}")


def validate_manifest(manifest: object, root: Path = PROJECT_ROOT) -> dict[str, object]:
    """Validate schema and all public identities/hashes; do not replay captures."""
    data = _require_object(manifest, "manifest")
    _require_exact_keys(
        data,
        {"schema_version", "id", "identity", "implementation_base", "source_segment", "anchors"},
        "manifest",
    )
    if data.get("schema_version") != 1 or data.get("id") != "ki15d-verify-095":
        raise VerificationError("unsupported verification manifest schema or id")
    if data.get("identity") != EXPECTED_IDENTITY:
        raise VerificationError("wrong game/revision/CPU identity")
    base = data.get("implementation_base")
    if base != EXPECTED_IMPLEMENTATION_BASE:
        raise VerificationError("implementation_base is not the fixed reviewed commit")
    segment = _require_object(data.get("source_segment"), "source_segment")
    _require_exact_keys(
        segment, {"path", "index", "load_address", "size", "sha256", "limits"}, "source_segment"
    )
    segment_relative = _safe_path(segment.get("path"), "source_segment.path")
    if segment_relative != "work/kipack/ki15d/rom-0.bin":
        raise VerificationError("source_segment.path is not the fixed reviewed input")
    for field, expected in EXPECTED_SEGMENT.items():
        if segment.get(field) != expected:
            raise VerificationError(f"source_segment.{field} has the wrong fixed identity")
    segment_limits = _require_string(segment.get("limits"), "source_segment.limits")
    if hashlib.sha256(segment_limits.encode()).hexdigest() != EXPECTED_SEGMENT_LIMITS_SHA256:
        raise VerificationError("source_segment.limits differs from the reviewed semantic contract")

    anchors = data.get("anchors")
    if not isinstance(anchors, list):
        raise VerificationError("anchors must be an array")
    ids = [item.get("id") for item in anchors if isinstance(item, dict)]
    string_ids = [item for item in ids if isinstance(item, str)]
    if len(ids) != len(anchors) or len(string_ids) != len(ids) or len(set(string_ids)) != len(ids):
        raise VerificationError("anchor ids must be unique strings")
    if set(string_ids) != set(EXPECTED_ADAPTERS):
        raise VerificationError("manifest must contain exactly the two supported anchors")

    public_checked = 0
    private_availability: list[dict[str, str]] = [{
        "anchor": "source-segment",
        "path": segment_relative,
        "status": (
            "AVAILABLE"
            if _safe_file(root, segment_relative, required=False) is not None
            else "UNAVAILABLE"
        ),
    }]
    for raw_anchor in anchors:
        anchor = _require_object(raw_anchor, "anchor")
        _require_exact_keys(
            anchor,
            {
                "id", "adapter", "evidence_type", "source", "abi", "initialization",
                "bounded_memory", "cache_observation", "interceptions", "acquisition",
                "admission", "public_dependencies", "private_capture", "expected",
                "completion_rule", "native_verification", "known_limits",
            },
            "anchor",
        )
        anchor_id = _require_string(anchor.get("id"), "anchor.id")
        if anchor.get("adapter") != EXPECTED_ADAPTERS[anchor_id]:
            raise VerificationError(f"{anchor_id}: unsupported fixed adapter")
        for field in ("evidence_type", "bounded_memory", "cache_observation", "completion_rule"):
            _require_string(anchor.get(field), f"{anchor_id}.{field}")
        for field in ("initialization", "interceptions", "known_limits"):
            _require_strings(anchor.get(field), f"{anchor_id}.{field}")
        abi = _require_object(anchor.get("abi"), f"{anchor_id}.abi")
        _require_exact_keys(abi, {"inputs", "outputs", "excluded"}, f"{anchor_id}.abi")
        for field in ("inputs", "outputs", "excluded"):
            _require_strings(abi.get(field), f"{anchor_id}.abi.{field}")
        acquisition = _require_object(anchor.get("acquisition"), f"{anchor_id}.acquisition")
        _require_exact_keys(
            acquisition, {"emulator", "version", "cpu_mode", "command_description"},
            f"{anchor_id}.acquisition",
        )
        for field in ("emulator", "version", "cpu_mode", "command_description"):
            _require_string(acquisition.get(field), f"{anchor_id}.acquisition.{field}")
        if acquisition.get("emulator") != "MAME" or acquisition.get("version") != "0.289":
            raise VerificationError(f"{anchor_id}: wrong emulator identity")
        admission = _require_object(anchor.get("admission"), f"{anchor_id}.admission")
        _require_exact_keys(admission, {"kind", "review_path", "limits"}, f"{anchor_id}.admission")
        if admission.get("kind") != "reviewed-legacy":
            raise VerificationError(f"{anchor_id}: evidence must be labeled reviewed-legacy")
        _safe_path(admission.get("review_path"), f"{anchor_id}.admission.review_path")
        _require_string(admission.get("limits"), f"{anchor_id}.admission.limits")

        source = _require_object(anchor.get("source"), f"{anchor_id}.source")
        _require_exact_keys(
            source, {"provenance_path", "entry", "exit", "segment_offset", "size", "sha256"},
            f"{anchor_id}.source",
        )
        provenance_path = _safe_path(
            source.get("provenance_path"), f"{anchor_id}.source.provenance_path"
        )
        entry = _hex_int(source.get("entry"), f"{anchor_id}.source.entry")
        offset = _hex_int(source.get("segment_offset"), f"{anchor_id}.source.segment_offset")
        size = source.get("size")
        if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
            raise VerificationError(f"{anchor_id}.source.size must be positive")
        source_sha = _sha(source.get("sha256"), f"{anchor_id}.source.sha256")
        _require_string(source.get("exit"), f"{anchor_id}.source.exit")
        if (entry & 0x1FFFFFFF) - int(EXPECTED_SEGMENT["load_address"], 16) != offset:
            raise VerificationError(f"{anchor_id}: entry/load/segment offset disagree")
        if offset < 0 or offset + size > int(EXPECTED_SEGMENT["size"]):
            raise VerificationError(f"{anchor_id}: source range exceeds segment 0")

        record = _load_function_record(root, provenance_path)
        record_game = _require_object(record.get("game"), f"{anchor_id} game")
        if (
            record_game.get("mame_set") != EXPECTED_IDENTITY["mame_set"]
            or record_game.get("revision") != EXPECTED_IDENTITY["revision"]
        ):
            raise VerificationError(f"{anchor_id}: function provenance game identity differs")
        original = _require_object(record.get("original"), f"{anchor_id} original")
        if original.get("cpu") != EXPECTED_IDENTITY["cpu"]:
            raise VerificationError(f"{anchor_id}: function provenance CPU identity differs")
        extracted = _require_object(original.get("extracted_segment"), f"{anchor_id} segment")
        if original.get("virtual_address") != source.get("entry"):
            raise VerificationError(f"{anchor_id}: function provenance entry differs")
        comparisons = {
            "index": EXPECTED_SEGMENT["index"],
            "load_address": EXPECTED_SEGMENT["load_address"],
            "segment_offset": source.get("segment_offset"),
            "segment_size": EXPECTED_SEGMENT["size"],
            "segment_sha256": EXPECTED_SEGMENT["sha256"],
            "function_size": size,
            "function_sha256": source_sha,
        }
        for field, expected in comparisons.items():
            if extracted.get(field) != expected:
                raise VerificationError(f"{anchor_id}: stale source identity at {field}")

        dependencies = anchor.get("public_dependencies")
        if not isinstance(dependencies, list) or not dependencies:
            raise VerificationError(f"{anchor_id}.public_dependencies must be non-empty")
        seen_paths: set[str] = set()
        for number, raw_dependency in enumerate(dependencies):
            dependency = _require_object(raw_dependency, f"{anchor_id}.dependency[{number}]")
            _require_exact_keys(
                dependency, {"role", "path", "sha256"}, f"{anchor_id}.dependency[{number}]"
            )
            _require_string(dependency.get("role"), f"{anchor_id}.dependency[{number}].role")
            relative = _safe_path(
                dependency.get("path"), f"{anchor_id}.dependency[{number}].path"
            )
            if relative in seen_paths:
                raise VerificationError(f"{anchor_id}: duplicate public dependency {relative}")
            seen_paths.add(relative)
            expected_sha = _sha(
                dependency.get("sha256"), f"{anchor_id}.dependency[{number}].sha256"
            )
            path = _safe_file(root, relative, required=True)
            assert path is not None
            _, actual_sha = _hash_file(path)
            if actual_sha != expected_sha:
                raise VerificationError(
                    f"{anchor_id}: stale public dependency {relative}: "
                    f"expected {expected_sha}, found {actual_sha}"
                )
            public_checked += 1
        if provenance_path not in seen_paths:
            raise VerificationError(f"{anchor_id}: source provenance is not hash-pinned")
        if seen_paths != EXPECTED_PUBLIC_PATHS[anchor_id]:
            raise VerificationError(f"{anchor_id}: public dependency set is incomplete or unexpected")

        private = _require_object(anchor.get("private_capture"), f"{anchor_id}.private_capture")
        _require_exact_keys(private, {"path", "size", "sha256"}, f"{anchor_id}.private_capture")
        private_path = _safe_path(private.get("path"), f"{anchor_id}.private_capture.path")
        if private_path != EXPECTED_PRIVATE_PATHS[anchor_id]:
            raise VerificationError(f"{anchor_id}: private capture path is not the fixed reviewed input")
        if (
            not isinstance(private.get("size"), int)
            or isinstance(private.get("size"), bool)
            or private.get("size") <= 0
        ):
            raise VerificationError(f"{anchor_id}.private_capture.size must be positive")
        _sha(private.get("sha256"), f"{anchor_id}.private_capture.sha256")
        available = _safe_file(root, private_path, required=False) is not None
        private_availability.append(
            {"anchor": anchor_id, "path": private_path, "status": "AVAILABLE" if available else "UNAVAILABLE"}
        )
        expected = _require_object(anchor.get("expected"), f"{anchor_id}.expected")
        native = _require_object(anchor.get("native_verification"), f"{anchor_id}.native_verification")
        _require_exact_keys(native, {"make_target", "limits"}, f"{anchor_id}.native_verification")
        if native.get("make_target") != "test-native":
            raise VerificationError(f"{anchor_id}: native verification target must be test-native")
        _require_string(native.get("limits"), f"{anchor_id}.native_verification.limits")
        if anchor_id == "emitter-decisions" and expected != {
            "rows": 256, "decisions": 65536, "completion_marker": "KI_EMITTER_ORACLE complete=10000"
        }:
            raise VerificationError("emitter-decisions: wrong expected completion contract")
        if anchor_id == "type1a-bridge" and expected != {
            "calls": 315, "visible_groups": 96, "required_phases_per_call": 9
        }:
            raise VerificationError("type1a-bridge: wrong expected completion contract")
        if _semantic_digest(anchor) != EXPECTED_SEMANTIC_SHA256[anchor_id]:
            raise VerificationError(
                f"{anchor_id}: descriptive metadata differs from the reviewed "
                f"{SEMANTIC_CONTRACT_VERSION} semantic contract"
            )

    return {
        "action": "check",
        "category": "PASS",
        "anchors": sorted(EXPECTED_ADAPTERS),
        "public_dependencies_checked": public_checked,
        "private_evidence": private_availability,
        "limits": "Metadata consistency only; private replay and freshly rebuilt native tests are separate gates.",
        "statement": "Public identities are valid; no original replay result is claimed.",
    }


def _import_module(path: Path, name: str) -> object:
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise VerificationError(f"cannot load fixed analyzer {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def _emitter_adapter(root: Path, anchor: dict[str, object]) -> dict[str, object]:
    analyzer = _import_module(root / "tools/analyze_type1a_emission.py", "verify_emission_adapter")
    private = _require_object(anchor["private_capture"], "private_capture")
    capture_path = root / str(private["path"])
    try:
        with capture_path.open("r", encoding="utf-8") as handle:
            actual = analyzer.oracle_rows(handle)
        fixture_path = root / "tests/fixtures/ki15d_88003d30.csv"
        with fixture_path.open("r", encoding="utf-8", newline="") as handle:
            reader = csv.DictReader(handle)
            if reader.fieldnames != ["countdown", "cases", "digest"]:
                raise VerificationError("emitter fixture has an unexpected header")
            expected = [
                (int(row["countdown"], 16), int(row["cases"]), int(row["digest"], 16))
                for row in reader
            ]
    except (OSError, ValueError, KeyError, csv.Error) as exc:
        raise VerificationError(f"emitter-decisions parse failed: {exc}") from exc
    if len(actual) != len(expected):
        raise VerificationError(
            f"emitter-decisions first difference: row count {len(actual)} != {len(expected)}"
        )
    for index, (actual_row, expected_row) in enumerate(zip(actual, expected)):
        if actual_row != expected_row:
            raise VerificationError(
                f"emitter-decisions first difference at row {index}: "
                f"got {actual_row}, expected {expected_row}"
            )
    decisions = sum(row[1] for row in actual)
    if len(actual) != 256 or decisions != 65536:
        raise VerificationError("emitter-decisions completion/count contract failed")
    return {"rows": len(actual), "decisions": decisions, "completion": "validated"}


def _bridge_adapter(root: Path, anchor: dict[str, object]) -> dict[str, object]:
    analyzer = _import_module(
        root / "tools/analyze_type1a_state_to_render.py", "verify_bridge_adapter"
    )
    private = _require_object(anchor["private_capture"], "private_capture")
    capture_path = root / str(private["path"])
    try:
        with capture_path.open("r", encoding="utf-8-sig") as handle:
            calls = analyzer.parse_trace(handle)
        rows = [analyzer.derive_row(call) for call in calls]
        analyzer.assign_timeline(rows)
        bridge_rows = analyzer.bridge_rows(calls, rows)
        analyzer.compare_bridge_fixture(
            bridge_rows, root / "tests/fixtures/ki15d_88001b90_type1a_bridge.csv"
        )
    except (OSError, ValueError) as exc:
        raise VerificationError(f"type1a-bridge first difference: {exc}") from exc
    groups = int(rows[-1]["tick"]) + 1 if rows else 0
    if len(rows) != 315 or groups != 96:
        raise VerificationError(
            f"type1a-bridge completion/count contract failed: {len(rows)} calls, {groups} groups"
        )
    return {"calls": len(rows), "visible_groups": groups, "completion": "validated"}


ADAPTERS: dict[str, Callable[[Path, dict[str, object]], dict[str, object]]] = {
    "emitter-decisions-v1": _emitter_adapter,
    "type1a-bridge-v1": _bridge_adapter,
}


def replay(
    manifest: object,
    root: Path = PROJECT_ROOT,
    selected: str = "all",
    adapters: dict[str, Callable[[Path, dict[str, object]], dict[str, object]]] | None = None,
) -> tuple[dict[str, object], int]:
    public_result = validate_manifest(manifest, root)
    data = _require_object(manifest, "manifest")
    anchors = [_require_object(item, "anchor") for item in data["anchors"]]
    if selected != "all":
        if selected not in EXPECTED_ADAPTERS:
            raise VerificationError(f"unknown anchor {selected!r}")
        anchors = [anchor for anchor in anchors if anchor["id"] == selected]
    segment = _require_object(data["source_segment"], "source_segment")
    private_inputs = [("source segment", segment)] + [
        (str(anchor["id"]), _require_object(anchor["private_capture"], "private_capture"))
        for anchor in anchors
    ]
    missing: list[str] = []
    checked: list[dict[str, object]] = []
    for label, descriptor in private_inputs:
        relative = _safe_path(descriptor.get("path"), f"{label}.path")
        path = _safe_file(root, relative, required=False)
        if path is None:
            missing.append(relative)
            continue
        expected_size = descriptor.get("size")
        expected_sha = _sha(descriptor.get("sha256"), f"{label}.sha256")
        actual_size, actual_sha = _hash_file(path)
        if actual_size != expected_size or actual_sha != expected_sha:
            raise VerificationError(
                f"{label} identity differs for {relative}: expected size/hash "
                f"{expected_size}/{expected_sha}, found {actual_size}/{actual_sha}"
            )
        checked.append({"role": label, "path": relative, "size": actual_size, "sha256": actual_sha})
    if missing:
        return ({
            "action": "replay",
            "category": "NOT_RUN",
            "anchors": [anchor["id"] for anchor in anchors],
            "missing": missing,
            "limits": {str(anchor["id"]): anchor["known_limits"] for anchor in anchors},
            "statement": "Required private evidence is unavailable; no replay PASS is claimed.",
        }, 2)

    segment_path = root / str(segment["path"])
    try:
        segment_bytes = segment_path.read_bytes()
    except OSError as exc:
        raise VerificationError(f"cannot read source segment {segment_path}: {exc}") from exc
    for anchor in anchors:
        source = _require_object(anchor["source"], f"{anchor['id']}.source")
        offset = _hex_int(source["segment_offset"], "segment_offset")
        size = int(source["size"])
        actual_sha = hashlib.sha256(segment_bytes[offset : offset + size]).hexdigest()
        if offset + size > len(segment_bytes) or actual_sha != source["sha256"]:
            raise VerificationError(
                f"{anchor['id']}: source segment block identity differs at offset 0x{offset:x}"
            )

    dispatch = ADAPTERS if adapters is None else adapters
    results: list[dict[str, object]] = []
    for anchor in anchors:
        adapter_name = str(anchor["adapter"])
        if adapter_name not in dispatch:
            raise VerificationError(f"fixed adapter unavailable: {adapter_name}")
        results.append({
            "anchor": anchor["id"],
            **dispatch[adapter_name](root, anchor),
            "known_limits": anchor["known_limits"],
        })
    return ({
        "action": "replay",
        "category": "PASS",
        "anchors": results,
        "private_identities_checked": checked,
        "public_dependencies_checked": public_result["public_dependencies_checked"],
        "statement": "Reviewed retained captures replayed; this is not fresh original execution.",
    }, 0)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", default=str(DEFAULT_MANIFEST))
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("check", help="validate public metadata and dependencies")
    replay_parser = subparsers.add_parser("replay", help="replay fixed retained evidence")
    replay_parser.add_argument("--anchor", choices=("all", *EXPECTED_ADAPTERS), default="all")
    return parser


def _report_identity(manifest_path: Path) -> dict[str, object]:
    manifest_size, manifest_sha = _hash_file(manifest_path)
    runner_path = Path(__file__).resolve()
    runner_size, runner_sha = _hash_file(runner_path)
    return {
        "manifest_path": str(manifest_path),
        "manifest_size": manifest_size,
        "manifest_sha256": manifest_sha,
        "runner_path": str(runner_path),
        "runner_size": runner_size,
        "runner_sha256": runner_sha,
    }


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    manifest_path = Path(args.manifest)
    if not manifest_path.is_absolute():
        manifest_path = PROJECT_ROOT / manifest_path
    try:
        manifest = load_manifest(manifest_path)
        if args.command == "check":
            result = validate_manifest(manifest)
            status = 0
        else:
            result, status = replay(manifest, selected=args.anchor)
        result.update(_report_identity(manifest_path))
        print(json.dumps(result, indent=2, sort_keys=True))
        return status
    except VerificationError as exc:
        result = {
            "action": getattr(args, "command", None),
            "anchor": getattr(args, "anchor", None),
            "category": "FAIL",
            "first_difference": str(exc),
        }
        try:
            result.update(_report_identity(manifest_path))
        except VerificationError:
            pass
        print(json.dumps(result, indent=2, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
