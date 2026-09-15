#!/usr/bin/env python3
"""Validate curated knowledge and make a narrow, local metadata backup."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
from typing import Iterable
import re


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CATALOG_PATH = Path("provenance/knowledge.json")
MANIFEST_NAME = "manifest.json"
CONFIDENCE = {
    "observed",
    "hypothesis",
    "statically-derived",
    "differentially-verified",
    "refuted",
}
EVIDENCE_CLASSES = {
    "observation",
    "static-analysis",
    "differential-verification",
    "implementation",
    "review",
    "runtime-integration",
}

# This is intentionally a list of files, not directories or globs. In
# particular it excludes ROMs, disk images, captures, dumps, screenshots,
# generated assembly, and the unverified POC-120B1 implementation drafts.
BACKUP_ALLOWLIST = (
    "docs/EXTERNAL_REFERENCES.md",
    "docs/GHIDRA.md",
    "docs/HARDWARE_BASELINE.md",
    "docs/KNOWLEDGE.md",
    "docs/OBJECT_MODEL.md",
    "docs/TYPE1A_EMISSION.md",
    "docs/VERIFICATION.md",
    "provenance/README.md",
    "provenance/knowledge.json",
    "provenance/functions/TEMPLATE.json",
    "provenance/functions/ki15d_88001b90_type1a.json",
    "provenance/functions/ki15d_88003d30.json",
    "provenance/functions/ki15d_88004180.json",
    "provenance/functions/ki15d_88004e54.json",
    "provenance/functions/ki15d_880053f4.json",
    "provenance/functions/ki15d_880054d0.json",
    "provenance/functions/ki15d_880063ac.json",
    "provenance/functions/ki15d_88006670_type1a.json",
    "provenance/functions/ki15d_8800700c.json",
    "provenance/functions/ki15d_8800842c.json",
    "provenance/functions/ki15d_8800b1fc.json",
    "provenance/functions/ki15d_8802aa24.json",
    "provenance/functions/ki15d_8802d5b0.json",
    "provenance/observations/ki15d_8800700c_live.csv",
    "provenance/observations/ki15d_endokuken_lifecycle.csv",
    "provenance/observations/ki15d_endokuken_render_path.csv",
    "provenance/observations/ki15d_type1a_animation.csv",
    "provenance/observations/ki15d_type1a_animation_transforms.csv",
    "provenance/observations/ki15d_type1a_blend_table.csv",
    "provenance/observations/ki15d_type1a_composite_transforms.csv",
    "provenance/observations/ki15d_type1a_composite_verification.csv",
    "provenance/observations/ki15d_type1a_native_lifetime.csv",
    "provenance/observations/ki15d_type1a_native_render_verification.csv",
    "provenance/observations/ki15d_type1a_packed_frames.csv",
    "work/planning/IMPLEMENTATION_PLAN.md",
    "work/planning/KNOWLEDGE_REGISTER.md",
    "work/planning/PROJECT_SPECIFICATION.md",
    "work/planning/UNCERTAINTIES.md",
    "work/reviews/POC-120A.md",
    "work/tasks/POC-120B/DISCOVERY.md",
    "work/tasks/POC-120B1/BRIEF.md",
    "work/tasks/KNOW-090/BASELINE.md",
    "work/tasks/KNOW-090/BRIEF.md",
    "work/tasks/KNOW-090/SELF_VERIFICATION.md",
)


class KnowledgeError(ValueError):
    """A user-readable metadata or recovery error."""


def _object_without_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise KnowledgeError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> object:
    try:
        return json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_object_without_duplicate_keys,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, KnowledgeError) as exc:
        raise KnowledgeError(f"cannot read {path}: {exc}") from exc


def _safe_relative_path(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise KnowledgeError(f"{label} must be a non-empty path string")
    if "\\" in value or "\0" in value:
        raise KnowledgeError(f"{label} is not a portable repository path: {value!r}")
    path = PurePosixPath(value)
    if path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
        raise KnowledgeError(f"{label} escapes or does not normalize within the root: {value!r}")
    return value


def _regular_file_without_symlinks(root: Path, relative: str) -> Path:
    current = root
    for part in PurePosixPath(relative).parts:
        current = current / part
        try:
            if current.is_symlink():
                raise KnowledgeError(f"symlink is not allowed: {relative}")
        except OSError as exc:
            raise KnowledgeError(f"cannot inspect {relative}: {exc}") from exc
    if not current.is_file():
        raise KnowledgeError(f"required regular file is missing: {relative}")
    return current


def _symlink_component(root: Path, relative: str) -> Path | None:
    current = root
    for part in PurePosixPath(relative).parts:
        current = current / part
        if current.is_symlink():
            return current
    return None


def _tracked_paths(root: Path) -> set[str]:
    try:
        result = subprocess.run(
            ["git", "ls-files", "-z"],
            cwd=root,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise KnowledgeError(f"cannot inventory tracked files: {exc}") from exc
    return {item.decode("utf-8") for item in result.stdout.split(b"\0") if item}


def _nonempty_string(value: object, label: str, errors: list[str]) -> None:
    if not isinstance(value, str) or not value.strip():
        errors.append(f"{label} must be a non-empty string")


def _string_list(value: object, label: str, errors: list[str], minimum: int = 1) -> None:
    if not isinstance(value, list) or len(value) < minimum or any(
        not isinstance(item, str) or not item.strip() for item in value
    ):
        errors.append(f"{label} must contain at least {minimum} non-empty string(s)")


def validate_catalog(
    catalog: object,
    root: Path = PROJECT_ROOT,
    tracked_paths: set[str] | None = None,
) -> tuple[list[str], list[str]]:
    """Return (errors, unavailable optional evidence) for a parsed catalog."""
    errors: list[str] = []
    unavailable: list[str] = []
    if not isinstance(catalog, dict):
        return ["catalog root must be an object"], unavailable
    if catalog.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if catalog.get("game") != {"mame_set": "kinst", "revision": "v1.5d"}:
        errors.append("game identity must be exactly mame_set=kinst, revision=v1.5d")
    facts = catalog.get("facts")
    uncertainties = catalog.get("uncertainties")
    if not isinstance(facts, list) or not isinstance(uncertainties, list):
        return errors + ["facts and uncertainties must be arrays"], unavailable
    if tracked_paths is None:
        tracked_paths = _tracked_paths(root)

    fact_id_values = [item.get("id") for item in facts if isinstance(item, dict)]
    uncertainty_id_values = [
        item.get("id") for item in uncertainties if isinstance(item, dict)
    ]
    fact_ids = {item for item in fact_id_values if isinstance(item, str)}
    uncertainty_ids = {item for item in uncertainty_id_values if isinstance(item, str)}
    if len(fact_ids) != len(facts) or any(
        not isinstance(item, str) or re.fullmatch(r"K-[0-9]{3}", item) is None
        for item in fact_id_values
    ):
        errors.append("fact identifiers must be unique strings matching K-NNN")
    if len(uncertainty_ids) != len(uncertainties) or any(
        not isinstance(item, str) or re.fullmatch(r"U-[0-9]{3}", item) is None
        for item in uncertainty_id_values
    ):
        errors.append("uncertainty identifiers must be unique strings matching U-NNN")

    function_dir = root / "provenance" / "functions"
    function_ids: set[str] = set()
    for path in function_dir.glob("*.json"):
        if path.name == "TEMPLATE.json" or path.is_symlink():
            continue
        try:
            record = load_json(path)
        except KnowledgeError as exc:
            errors.append(str(exc))
            continue
        if isinstance(record, dict) and isinstance(record.get("id"), str):
            function_ids.add(record["id"])

    def check_entry(entry: object, expected_prefix: str) -> None:
        if not isinstance(entry, dict):
            errors.append(f"{expected_prefix} entry must be an object")
            return
        entry_id = entry.get("id")
        label = entry_id if isinstance(entry_id, str) else expected_prefix
        _nonempty_string(entry.get("title"), f"{label}.title", errors)
        confidence = entry.get("confidence")
        if not isinstance(confidence, str) or confidence not in CONFIDENCE:
            errors.append(f"{label}.confidence must be one of {sorted(CONFIDENCE)}")
        _string_list(entry.get("scope"), f"{label}.scope", errors)
        _nonempty_string(entry.get("limits"), f"{label}.limits", errors)
        _nonempty_string(entry.get("invalidation"), f"{label}.invalidation", errors)

        links = entry.get("function_records", [])
        if not isinstance(links, list) or any(not isinstance(item, str) for item in links):
            errors.append(f"{label}.function_records must be an array of strings")
        else:
            for function_id in links:
                if function_id not in function_ids:
                    errors.append(f"{label} links unknown function record {function_id!r}")

        evidence = entry.get("evidence")
        if not isinstance(evidence, list) or not evidence:
            errors.append(f"{label}.evidence must be a non-empty array")
            return
        has_tracked = False
        for number, source in enumerate(evidence):
            source_label = f"{label}.evidence[{number}]"
            if not isinstance(source, dict):
                errors.append(f"{source_label} must be an object")
                continue
            evidence_class = source.get("class")
            if not isinstance(evidence_class, str) or evidence_class not in EVIDENCE_CLASSES:
                errors.append(f"{source_label}.class is invalid")
            availability = source.get("availability")
            if not isinstance(availability, str) or availability not in (
                "tracked",
                "local-optional",
            ):
                errors.append(f"{source_label}.availability is invalid")
                continue
            try:
                relative = _safe_relative_path(source.get("path"), f"{source_label}.path")
            except KnowledgeError as exc:
                errors.append(str(exc))
                continue
            locator = source.get("locator")
            _nonempty_string(locator, f"{source_label}.locator", errors)
            source_path = root / relative
            symlink = _symlink_component(root, relative)
            if availability == "tracked":
                has_tracked = True
                if relative not in tracked_paths:
                    errors.append(f"{source_label} is declared tracked but is not Git-tracked: {relative}")
                if not source_path.is_file() or symlink is not None:
                    errors.append(f"{source_label} tracked evidence is missing or unsafe: {relative}")
            elif symlink is not None:
                errors.append(f"{source_label} optional evidence has a symlink component: {symlink}")
                continue
            elif not source_path.is_file():
                unavailable.append(f"{label}: {relative}")
                continue
            if source_path.is_file() and symlink is None and isinstance(locator, str):
                try:
                    if locator not in source_path.read_text(encoding="utf-8"):
                        errors.append(f"{source_label}.locator not found in {relative}: {locator!r}")
                except (OSError, UnicodeError) as exc:
                    errors.append(f"{source_label} is not readable UTF-8 text: {exc}")
        if not has_tracked:
            errors.append(f"{label} requires at least one tracked evidence source")

    for entry in facts:
        check_entry(entry, "fact")
        if isinstance(entry, dict):
            _nonempty_string(entry.get("statement"), f"{entry.get('id')}.statement", errors)
            related = entry.get("related_uncertainties", [])
            if not isinstance(related, list) or any(
                not isinstance(item, str) or item not in uncertainty_ids for item in related
            ):
                errors.append(f"{entry.get('id')}.related_uncertainties contains an unknown ID")

    for entry in uncertainties:
        check_entry(entry, "uncertainty")
        if not isinstance(entry, dict):
            continue
        label = entry.get("id")
        if not isinstance(entry.get("status"), str) or entry.get("status") not in (
            "open",
            "resolved",
            "refuted",
        ):
            errors.append(f"{label}.status is invalid")
        if not isinstance(entry.get("priority"), str) or entry.get("priority") not in (
            "P0",
            "P1",
            "P2",
        ):
            errors.append(f"{label}.priority is invalid")
        _string_list(entry.get("owners"), f"{label}.owners", errors)
        _string_list(entry.get("alternatives"), f"{label}.alternatives", errors, minimum=2)
        _string_list(entry.get("blocked_tasks"), f"{label}.blocked_tasks", errors)
        _nonempty_string(entry.get("impact"), f"{label}.impact", errors)
        _nonempty_string(entry.get("next_experiment"), f"{label}.next_experiment", errors)
    return errors, sorted(set(unavailable))


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _validate_allowlist(paths: Iterable[str]) -> tuple[str, ...]:
    result: list[str] = []
    seen: set[str] = set()
    casefolded: set[str] = set()
    for number, item in enumerate(paths):
        relative = _safe_relative_path(item, f"allowlist[{number}]")
        folded = relative.casefold()
        if relative in seen or folded in casefolded:
            raise KnowledgeError(f"duplicate or case-colliding allowlist path: {relative}")
        seen.add(relative)
        casefolded.add(folded)
        result.append(relative)
    return tuple(result)


def _approved_backup_path(root: Path, path: Path, label: str) -> Path:
    root = root.resolve()
    if path.is_absolute():
        absolute = path
        try:
            relative = _safe_relative_path(
                absolute.relative_to(root).as_posix(), label
            )
        except ValueError as exc:
            raise KnowledgeError(f"{label} must be under work/knowledge-backups") from exc
    else:
        relative = path.as_posix()
        absolute = root / _safe_relative_path(relative, label)
    relative_path = PurePosixPath(relative)
    if relative_path.parts[:2] != ("work", "knowledge-backups") or len(relative_path.parts) < 3:
        raise KnowledgeError(f"{label} must be below work/knowledge-backups")
    current = root
    for part in relative_path.parts:
        current = current / part
        if current.is_symlink():
            raise KnowledgeError(f"{label} has a symlink ancestor: {current}")
    approved = root / "work" / "knowledge-backups"
    try:
        absolute.resolve(strict=False).relative_to(approved)
    except ValueError as exc:
        raise KnowledgeError(f"{label} must be under work/knowledge-backups") from exc
    return absolute


def export_bundle(
    root: Path,
    bundle: Path,
    allowlist: Iterable[str] = BACKUP_ALLOWLIST,
) -> tuple[str, int]:
    paths = _validate_allowlist(allowlist)
    bundle = _approved_backup_path(root, bundle, "bundle")
    if bundle.exists() or bundle.is_symlink():
        raise KnowledgeError(f"refusing to overwrite existing bundle: {bundle}")
    payloads: list[tuple[str, bytes]] = []
    for relative in paths:
        source = _regular_file_without_symlinks(root, relative)
        data = source.read_bytes()
        try:
            data.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise KnowledgeError(f"allowlisted file is not UTF-8 text: {relative}") from exc
        if b"\0" in data:
            raise KnowledgeError(f"allowlisted file contains a NUL byte: {relative}")
        payloads.append((relative, data))
    manifest = {
        "schema_version": 1,
        "format": "meltdown-knowledge-metadata-bundle",
        "limitations": (
            "Same-volume metadata/text copy only; excludes game inputs, captures, assets, "
            "dumps, generated assembly, binaries, and unverified implementation drafts."
        ),
        "files": [
            {"path": relative, "size": len(data), "sha256": _sha256(data)}
            for relative, data in payloads
        ],
    }
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")
    bundle.mkdir(parents=True)
    (bundle / MANIFEST_NAME).write_bytes(manifest_bytes)
    for relative, data in payloads:
        destination = bundle / "files" / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    return _sha256(manifest_bytes), len(payloads)


def _read_verified_bundle(
    root: Path,
    bundle: Path,
    expected_allowlist: Iterable[str] = BACKUP_ALLOWLIST,
    expected_manifest_sha256: str | None = None,
) -> tuple[dict[str, object], list[tuple[str, bytes]], str]:
    bundle = _approved_backup_path(root, bundle, "bundle")
    if not bundle.is_dir() or bundle.is_symlink():
        raise KnowledgeError(f"bundle is missing, not a directory, or a symlink: {bundle}")
    manifest_path = bundle / MANIFEST_NAME
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise KnowledgeError("bundle manifest is missing or is a symlink")
    manifest_bytes = manifest_path.read_bytes()
    manifest = load_json(manifest_path)
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise KnowledgeError("bundle manifest schema_version must be 1")
    if manifest.get("format") != "meltdown-knowledge-metadata-bundle":
        raise KnowledgeError("bundle manifest format is invalid")
    files = manifest.get("files")
    if not isinstance(files, list):
        raise KnowledgeError("bundle manifest files must be an array")
    paths = _validate_allowlist(
        item.get("path") if isinstance(item, dict) else None for item in files
    )
    if paths != _validate_allowlist(expected_allowlist):
        raise KnowledgeError("bundle manifest does not contain the exact approved allowlist")
    payload_root = bundle / "files"
    payloads: list[tuple[str, bytes]] = []
    for relative, item in zip(paths, files):
        assert isinstance(item, dict)
        path = _regular_file_without_symlinks(payload_root, relative)
        data = path.read_bytes()
        if item.get("size") != len(data) or item.get("sha256") != _sha256(data):
            raise KnowledgeError(f"bundle payload is corrupt: {relative}")
        payloads.append((relative, data))
    actual_entries: set[str] = set()
    for directory, directory_names, file_names in os.walk(bundle, followlinks=False):
        base = Path(directory)
        for name in directory_names:
            if (base / name).is_symlink():
                raise KnowledgeError(f"bundle contains a symlink: {base / name}")
        for name in file_names:
            path = base / name
            if path.is_symlink():
                raise KnowledgeError(f"bundle contains a symlink: {path}")
            actual_entries.add(path.relative_to(bundle).as_posix())
    expected_entries = {MANIFEST_NAME, *(f"files/{path}" for path in paths)}
    if actual_entries != expected_entries:
        raise KnowledgeError("bundle has missing or unmanifested files")
    manifest_sha256 = _sha256(manifest_bytes)
    if expected_manifest_sha256 is not None and manifest_sha256 != expected_manifest_sha256:
        raise KnowledgeError(
            "bundle manifest identity differs: "
            f"expected {expected_manifest_sha256}, found {manifest_sha256}"
        )
    return manifest, payloads, manifest_sha256


def restore_check(
    root: Path,
    bundle: Path,
    destination: Path,
    expected_allowlist: Iterable[str] = BACKUP_ALLOWLIST,
    expected_manifest_sha256: str | None = None,
) -> tuple[str, int]:
    _, payloads, manifest_sha256 = _read_verified_bundle(
        root, bundle, expected_allowlist, expected_manifest_sha256
    )
    destination = _approved_backup_path(root, destination, "restore destination")
    if destination.exists() or destination.is_symlink():
        raise KnowledgeError(f"refusing to overwrite restore destination: {destination}")
    destination.mkdir(parents=True)
    for relative, data in payloads:
        output = destination / relative
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    for relative, expected in payloads:
        restored = _regular_file_without_symlinks(destination, relative).read_bytes()
        if restored != expected or _sha256(restored) != _sha256(expected):
            raise KnowledgeError(f"restored byte/hash comparison failed: {relative}")
    return manifest_sha256, len(payloads)


def command_check(args: argparse.Namespace) -> int:
    catalog = load_json(PROJECT_ROOT / args.catalog)
    errors, unavailable = validate_catalog(catalog)
    for item in unavailable:
        print(f"UNAVAILABLE optional local evidence: {item}")
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    assert isinstance(catalog, dict)
    print(
        "knowledge catalog valid: "
        f"{len(catalog['facts'])} facts, {len(catalog['uncertainties'])} uncertainties, "
        f"{len(unavailable)} optional local source(s) unavailable"
    )
    return 0


def command_export(args: argparse.Namespace) -> int:
    manifest_hash, count = export_bundle(PROJECT_ROOT, Path(args.bundle))
    print(f"exported {count} files; manifest sha256 {manifest_hash}")
    return 0


def command_restore_check(args: argparse.Namespace) -> int:
    manifest_hash, count = restore_check(
        PROJECT_ROOT,
        Path(args.bundle),
        Path(args.destination),
        expected_manifest_sha256=args.manifest_sha256,
    )
    print(f"restored and compared {count} files; manifest sha256 {manifest_hash}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    check = subparsers.add_parser("check", help="validate the curated knowledge catalog")
    check.add_argument("--catalog", default=str(CATALOG_PATH))
    check.set_defaults(func=command_check)
    export = subparsers.add_parser("export", help="copy the exact approved metadata allowlist")
    export.add_argument("--bundle", required=True)
    export.set_defaults(func=command_export)
    restore = subparsers.add_parser(
        "restore-check", help="restore a verified bundle into a fresh directory"
    )
    restore.add_argument("--bundle", required=True)
    restore.add_argument("--destination", required=True)
    restore.add_argument(
        "--manifest-sha256",
        help="require the recorded 64-character manifest identity",
    )
    restore.set_defaults(func=command_restore_check)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except KnowledgeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
