#!/usr/bin/env python3
"""Small, dependency-free project tools for the KI native port."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
from typing import Any, Iterable
import zipfile


PROJECT_ROOT = Path(__file__).resolve().parents[1]
KNOWN_INPUTS_PATH = PROJECT_ROOT / "config" / "known_inputs.json"
VALID_KINDS = {"decompilation", "static-recompilation", "hardware-shim", "placeholder"}
VALID_RECONSTRUCTION_STATES = {"stub", "in-progress", "implemented"}
VALID_VERIFICATION_STATES = {"unverified", "partial", "trace-matched", "accepted"}
ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9_-]*$")
HEX_ADDRESS_PATTERN = re.compile(r"^0x[0-9a-fA-F]{1,16}$")


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    temporary.replace(path)


def digest_stream(handle: Any) -> tuple[str, str, int]:
    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    size = 0
    while chunk := handle.read(1024 * 1024):
        sha1.update(chunk)
        sha256.update(chunk)
        size += len(chunk)
    return sha1.hexdigest(), sha256.hexdigest(), size


def known_indexes() -> tuple[dict[str, dict[str, Any]], dict[str, dict[str, Any]]]:
    manifest = load_json(KNOWN_INPUTS_PATH)
    by_sha1: dict[str, dict[str, Any]] = {}
    by_crc_size: dict[str, dict[str, Any]] = {}
    for item in manifest["files"]:
        if item.get("sha1"):
            by_sha1[item["sha1"].lower()] = item
        if item.get("crc32") and item.get("size") is not None:
            key = f'{item["crc32"].lower()}:{item["size"]}'
            by_crc_size[key] = item
    return by_sha1, by_crc_size


def identify(sha1: str, size: int, crc32: str | None = None) -> dict[str, Any] | None:
    by_sha1, by_crc_size = known_indexes()
    match = by_sha1.get(sha1.lower())
    if match is None and crc32 is not None:
        match = by_crc_size.get(f"{crc32.lower()}:{size}")
    if match is None:
        return None
    return {"role": match["role"], "expected_name": match["name"]}


def inspect_regular_file(path: Path) -> dict[str, Any]:
    with path.open("rb") as handle:
        sha1, sha256, size = digest_stream(handle)
    item: dict[str, Any] = {
        "path": str(path.resolve()),
        "kind": "file",
        "size": size,
        "sha1": sha1,
        "sha256": sha256,
    }
    match = identify(sha1, size)
    if match:
        item["known_input"] = match
    return item


def inspect_zip(path: Path) -> dict[str, Any]:
    result = inspect_regular_file(path)
    result["kind"] = "zip"
    result["entries"] = []
    with zipfile.ZipFile(path, "r") as archive:
        for info in sorted(archive.infolist(), key=lambda candidate: candidate.filename):
            if info.is_dir():
                continue
            with archive.open(info, "r") as handle:
                sha1, sha256, size = digest_stream(handle)
            crc32 = f"{info.CRC:08x}"
            entry: dict[str, Any] = {
                "name": info.filename,
                "size": size,
                "crc32": crc32,
                "sha1": sha1,
                "sha256": sha256,
            }
            match = identify(sha1, size, crc32)
            if match:
                entry["known_input"] = match
            result["entries"].append(entry)
    return result


def chdman_metadata(path: Path) -> dict[str, Any] | None:
    executable = shutil.which("chdman")
    if executable is None:
        return None
    process = subprocess.run(
        [executable, "info", "-i", str(path)],
        check=False,
        capture_output=True,
        text=True,
    )
    output = process.stdout + process.stderr
    metadata: dict[str, Any] = {"exit_code": process.returncode}
    for label, key in (("SHA1", "sha1"), ("Raw SHA1", "raw_sha1"), ("Data SHA1", "data_sha1")):
        match = re.search(rf"^\s*{re.escape(label)}:\s*([0-9a-fA-F]{{40}})\s*$", output, re.MULTILINE)
        if match:
            metadata[key] = match.group(1).lower()
    if metadata.get("sha1"):
        match = identify(metadata["sha1"], 0)
        if match:
            metadata["known_input"] = match
    return metadata


def inspect_chd_header(path: Path) -> dict[str, Any]:
    """Read identity fields from a CHD header without decompressing game data."""
    with path.open("rb") as handle:
        header = handle.read(124)
    if len(header) < 16 or header[:8] != b"MComprHD":
        return {"valid": False, "error": "cabecera CHD no reconocida"}

    header_size = int.from_bytes(header[8:12], "big")
    version = int.from_bytes(header[12:16], "big")
    result: dict[str, Any] = {
        "valid": True,
        "version": version,
        "header_size": header_size,
    }
    if version == 5 and header_size >= 124 and len(header) >= 124:
        result.update(
            {
                "compressors": [
                    header[offset : offset + 4].decode("ascii", errors="replace")
                    for offset in range(16, 32, 4)
                ],
                "logical_bytes": int.from_bytes(header[32:40], "big"),
                "hunk_bytes": int.from_bytes(header[56:60], "big"),
                "unit_bytes": int.from_bytes(header[60:64], "big"),
                "raw_sha1": header[64:84].hex(),
                "sha1": header[84:104].hex(),
                "parent_sha1": header[104:124].hex(),
            }
        )
    elif version == 4 and header_size >= 108 and len(header) >= 108:
        result.update(
            {
                "logical_bytes": int.from_bytes(header[28:36], "big"),
                "hunk_bytes": int.from_bytes(header[44:48], "big"),
                "sha1": header[48:68].hex(),
                "parent_sha1": header[68:88].hex(),
                "raw_sha1": header[88:108].hex(),
            }
        )
    elif version == 3 and header_size >= 120 and len(header) >= 120:
        result.update(
            {
                "logical_bytes": int.from_bytes(header[28:36], "big"),
                "hunk_bytes": int.from_bytes(header[76:80], "big"),
                "sha1": header[80:100].hex(),
                "parent_sha1": header[100:120].hex(),
            }
        )
    else:
        result.update({"valid": False, "error": f"unsupported CHD version {version}"})
    if result.get("sha1"):
        match = identify(result["sha1"], 0)
        if match:
            result["known_input"] = match
    return result


def iter_input_files(source: Path) -> Iterable[Path]:
    if source.is_file():
        yield source
        return
    for root, directories, filenames in os.walk(source):
        directories[:] = sorted(directory for directory in directories if not Path(root, directory).is_symlink())
        for filename in sorted(filenames):
            path = Path(root, filename)
            if path.is_file() and not path.is_symlink():
                yield path


def command_inventory(args: argparse.Namespace) -> int:
    source = Path(args.source).expanduser()
    if not source.exists():
        print(f"Path does not exist: {source}", file=sys.stderr)
        return 2

    files: list[dict[str, Any]] = []
    for path in iter_input_files(source):
        try:
            if zipfile.is_zipfile(path):
                item = inspect_zip(path)
            else:
                item = inspect_regular_file(path)
            if path.suffix.lower() == ".chd":
                item["kind"] = "chd"
                item["chd_header"] = inspect_chd_header(path)
                metadata = chdman_metadata(path)
                if metadata is not None:
                    item["chdman"] = metadata
                else:
                    item["chdman"] = {"available": False}
            files.append(item)
            print(f"inventoried: {path}")
        except (OSError, zipfile.BadZipFile) as error:
            files.append({"path": str(path.resolve()), "error": str(error)})
            print(f"could not read: {path}: {error}", file=sys.stderr)

    report = {
        "schema_version": 1,
        "source": str(source.resolve()),
        "known_manifest": str(KNOWN_INPUTS_PATH.relative_to(PROJECT_ROOT)),
        "files": files,
    }
    output = Path(args.output)
    if not output.is_absolute():
        output = PROJECT_ROOT / output
    save_json(output, report)

    matches = 0
    for item in files:
        matches += int("known_input" in item)
        matches += sum(int("known_input" in entry) for entry in item.get("entries", []))
        matches += int("known_input" in item.get("chd_header", {}))
        matches += int("known_input" in item.get("chdman", {}))
    print(f"report: {output}")
    print(f"known matches: {matches}")
    return 0


def command_doctor(_: argparse.Namespace) -> int:
    flatpak = shutil.which("flatpak")
    flatpak_mame = False
    flatpak_ghidra = False
    if flatpak:
        probe = subprocess.run(
            [flatpak, "info", "org.mamedev.MAME"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        flatpak_mame = probe.returncode == 0
        probe = subprocess.run(
            [flatpak, "info", "org.ghidra_sre.Ghidra"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        flatpak_ghidra = probe.returncode == 0
    checks = [
        ("python3", True, "project tooling"),
        ("gcc", True, "native PC runtime"),
        ("make", True, "repeatable tests"),
        ("mame", False, "reference system and debugger"),
        ("chdman", False, "logical CHD verification"),
        ("ghidraRun", False, "interactive Ghidra analysis"),
        ("analyzeHeadless", False, "Ghidra automation"),
        ("clang", False, "alternative compiler and sanitizers"),
    ]
    missing_required = False
    print("Detected tools:")
    for command, required_now, purpose in checks:
        location = shutil.which(command)
        if location is None and flatpak_mame and command == "mame":
            location = "Flatpak org.mamedev.MAME"
        if location is None and flatpak_mame and command == "chdman":
            location = "included in Flatpak org.mamedev.MAME"
        if location is None and flatpak_ghidra and command in {"ghidraRun", "analyzeHeadless"}:
            location = "Flatpak org.ghidra_sre.Ghidra"
        state = location or "NOT FOUND"
        marker = "ok" if location else ("missing" if required_now else "optional")
        print(f"  [{marker:9}] {command:16} {state} — {purpose}")
        if required_now and location is None:
            missing_required = True
    return 1 if missing_required else 0


def compare_loaded_segments(
    segment_dir: Path, memory_path: Path, memory_base: int
) -> dict[str, Any]:
    """Compare sequential rom-N segment pairs with a captured memory image."""
    memory = memory_path.read_bytes()
    segments: list[dict[str, Any]] = []
    index = 0
    while True:
        binary_path = segment_dir / f"rom-{index}.bin"
        address_path = segment_dir / f"rom-{index}.addr"
        if not binary_path.exists() and not address_path.exists():
            break
        if not binary_path.is_file() or not address_path.is_file():
            raise ValueError(f"missing rom-{index}.bin/rom-{index}.addr pair")

        raw_address = address_path.read_bytes()
        if len(raw_address) != 4:
            raise ValueError(f"rom-{index}.addr must be exactly 4 bytes")
        load_address = struct.unpack("=I", raw_address)[0]
        data = binary_path.read_bytes()
        offset = load_address - memory_base
        if offset < 0 or offset + len(data) > len(memory):
            raise ValueError(
                f"rom-{index} (0x{load_address:08x}+0x{len(data):x}) "
                "falls outside the memory capture"
            )

        live = memory[offset : offset + len(data)]
        differing_offsets = [
            position for position, (expected, actual) in enumerate(zip(data, live))
            if expected != actual
        ]
        ranges: list[tuple[int, int]] = []
        if differing_offsets:
            start = previous = differing_offsets[0]
            for position in differing_offsets[1:]:
                if position != previous + 1:
                    ranges.append((start, previous))
                    start = position
                previous = position
            ranges.append((start, previous))

        matching = len(data) - len(differing_offsets)
        segments.append(
            {
                "index": index,
                "load_address": f"0x{load_address:08x}",
                "size": len(data),
                "matching_bytes": matching,
                "differing_bytes": len(differing_offsets),
                "matching_ratio": matching / len(data) if data else 1.0,
                "segment_sha256": hashlib.sha256(data).hexdigest(),
                "memory_slice_sha256": hashlib.sha256(live).hexdigest(),
                "first_differing_ranges": [
                    {
                        "start": f"0x{load_address + start:08x}",
                        "end": f"0x{load_address + end:08x}",
                    }
                    for start, end in ranges[:20]
                ],
            }
        )
        index += 1

    if not segments:
        raise ValueError("no rom-N.bin/rom-N.addr pairs found")
    return {
        "schema_version": 1,
        "segment_directory": str(segment_dir.resolve()),
        "memory_image": str(memory_path.resolve()),
        "memory_base": f"0x{memory_base:08x}",
        "segments": segments,
    }


def command_compare_segments(args: argparse.Namespace) -> int:
    segment_dir = Path(args.segment_dir).expanduser()
    memory_path = Path(args.memory).expanduser()
    try:
        report = compare_loaded_segments(segment_dir, memory_path, int(args.memory_base, 0))
    except (OSError, ValueError) as error:
        print(f"Could not compare segments: {error}", file=sys.stderr)
        return 2

    for item in report["segments"]:
        print(
            f"rom-{item['index']}: {item['matching_bytes']}/{item['size']} bytes "
            f"matching; differences={item['differing_bytes']}"
        )
    if args.output:
        output = Path(args.output)
        if not output.is_absolute():
            output = PROJECT_ROOT / output
        save_json(output, report)
        print(f"report: {output}")
    if args.require_exact and any(item["differing_bytes"] for item in report["segments"]):
        return 1
    return 0


def validate_repository_file(
    reference: str,
    field_name: str,
    project_root: Path,
) -> list[str]:
    """Validate a project-code reference without allowing paths outside the project.

    Provenance records may also reference ignored local evidence such as traces
    and assembly listings. Those files are intentionally not checked here.
    Reconstruction source is different: declaring it asserts that an actual
    implementation file exists in the project, so a stale path is an error.
    """

    candidate = Path(reference)
    if candidate.is_absolute():
        return [f"`{field_name}` must be relative to the project root"]

    root = project_root.resolve()
    resolved = (root / candidate).resolve()
    try:
        resolved.relative_to(root)
    except ValueError:
        return [f"`{field_name}` must stay within the project root"]
    if not resolved.is_file():
        return [f"`{field_name}` does not reference an existing file: {reference}"]
    return []


def validate_record(path: Path, project_root: Path | None = None) -> list[str]:
    errors: list[str] = []
    validation_root = PROJECT_ROOT if project_root is None else project_root
    try:
        record = load_json(path)
    except (OSError, json.JSONDecodeError) as error:
        return [f"invalid JSON: {error}"]
    if record.get("_template") is True:
        return []

    for key in ("schema_version", "id", "name", "game", "original", "reconstruction", "verification"):
        if key not in record:
            errors.append(f"missing `{key}`")
    if errors:
        return errors
    if record["schema_version"] != 1:
        errors.append("`schema_version` must be 1")
    if not isinstance(record["id"], str) or not ID_PATTERN.fullmatch(record["id"]):
        errors.append("`id` may only contain lowercase letters, numbers, hyphens, and underscores")
    if path.stem != record["id"]:
        errors.append("the file name must match `id`")

    game = record["game"]
    for key in ("mame_set", "revision", "input_manifest"):
        if not isinstance(game, dict) or not game.get(key):
            errors.append(f"missing `game.{key}`")

    original = record["original"]
    for key in ("cpu", "virtual_address", "assembly_path"):
        if not isinstance(original, dict) or not original.get(key):
            errors.append(f"missing `original.{key}`")
    address = original.get("virtual_address") if isinstance(original, dict) else None
    if not isinstance(address, str) or not HEX_ADDRESS_PATTERN.fullmatch(address):
        errors.append("`original.virtual_address` must be hexadecimal with a 0x prefix")

    reconstruction = record["reconstruction"]
    if not isinstance(reconstruction, dict):
        errors.append("`reconstruction` must be an object")
    else:
        if reconstruction.get("kind") not in VALID_KINDS:
            errors.append(f"`reconstruction.kind` must be one of {sorted(VALID_KINDS)}")
        if reconstruction.get("status") not in VALID_RECONSTRUCTION_STATES:
            errors.append(
                f"`reconstruction.status` must be one of {sorted(VALID_RECONSTRUCTION_STATES)}"
            )
        status = reconstruction.get("status")
        if "source_path" not in reconstruction:
            errors.append("missing `reconstruction.source_path`")
        else:
            source_path = reconstruction["source_path"]
            if source_path is None:
                if status != "stub":
                    errors.append(
                        "`reconstruction.source_path` may be null only when "
                        "`reconstruction.status` is `stub`"
                    )
            elif not isinstance(source_path, str) or not source_path.strip():
                errors.append(
                    "`reconstruction.source_path` must be null or a non-empty string"
                )
            else:
                errors.extend(
                    validate_repository_file(
                        source_path,
                        "reconstruction.source_path",
                        validation_root,
                    )
                )
        if not isinstance(reconstruction.get("hardware_dependencies"), list):
            errors.append("`reconstruction.hardware_dependencies` must be a list")

    verification = record["verification"]
    if not isinstance(verification, dict):
        errors.append("`verification` must be an object")
    else:
        if verification.get("status") not in VALID_VERIFICATION_STATES:
            errors.append(
                f"`verification.status` must be one of {sorted(VALID_VERIFICATION_STATES)}"
            )
        for key in ("fixtures", "commands"):
            if not isinstance(verification.get(key), list):
                errors.append(f"`verification.{key}` must be a list")
    return errors


def command_provenance_check(args: argparse.Namespace) -> int:
    target = Path(args.path)
    if not target.is_absolute():
        target = PROJECT_ROOT / target
    paths = [target] if target.is_file() else sorted(target.glob("*.json"))
    failures = 0
    checked = 0
    for path in paths:
        if path.name == "TEMPLATE.json":
            continue
        checked += 1
        errors = validate_record(path)
        if errors:
            failures += 1
            print(f"ERROR {path.relative_to(PROJECT_ROOT)}")
            for error in errors:
                print(f"  - {error}")
        else:
            print(f"OK {path.relative_to(PROJECT_ROOT)}")
    print(f"records checked: {checked}; errors: {failures}")
    return 1 if failures else 0


def command_new_function(args: argparse.Namespace) -> int:
    if not ID_PATTERN.fullmatch(args.id):
        print("The id may only contain lowercase letters, numbers, hyphens, and underscores.", file=sys.stderr)
        return 2
    if not HEX_ADDRESS_PATTERN.fullmatch(args.address):
        print("The address must be hexadecimal and start with 0x.", file=sys.stderr)
        return 2
    address = f"0x{int(args.address, 16):08x}"
    target = PROJECT_ROOT / "provenance" / "functions" / f"{args.id}.json"
    if target.exists():
        print(f"Already exists: {target}", file=sys.stderr)
        return 2
    record = {
        "schema_version": 1,
        "id": args.id,
        "name": args.name,
        "game": {
            "mame_set": args.mame_set,
            "revision": args.revision,
            "input_manifest": "work/input-inventory.json",
        },
        "original": {
            "cpu": "R4600LE",
            "virtual_address": address,
            "physical_address": None,
            "file_offset": None,
            "assembly_path": f"work/asm/{args.id}.s",
            "discovery_notes": "",
        },
        "reconstruction": {
            "kind": args.kind,
            "status": "stub",
            # A new record describes identified original code, not an
            # implementation that has already been created. The source path is
            # added when reconstruction work actually begins.
            "source_path": None,
            "hardware_dependencies": [],
        },
        "verification": {
            "status": "unverified",
            "fixtures": [],
            "mame_version": "",
            "commands": [],
            "notes": "",
        },
    }
    save_json(target, record)
    print(target)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Meltdown project tools")
    subparsers = parser.add_subparsers(dest="command", required=True)

    inventory = subparsers.add_parser("inventory", help="inventory local dumps without modifying them")
    inventory.add_argument("source")
    inventory.add_argument("--output", default="work/input-inventory.json")
    inventory.set_defaults(handler=command_inventory)

    doctor = subparsers.add_parser("doctor", help="show available tools")
    doctor.set_defaults(handler=command_doctor)

    compare_segments = subparsers.add_parser(
        "compare-segments", help="compare extracted segments with a RAM capture"
    )
    compare_segments.add_argument("segment_dir")
    compare_segments.add_argument("memory")
    compare_segments.add_argument("--memory-base", default="0x08000000")
    compare_segments.add_argument("--output")
    compare_segments.add_argument("--require-exact", action="store_true")
    compare_segments.set_defaults(handler=command_compare_segments)

    provenance = subparsers.add_parser("provenance-check", help="validate function records")
    provenance.add_argument("path", nargs="?", default="provenance/functions")
    provenance.set_defaults(handler=command_provenance_check)

    new_function = subparsers.add_parser("new-function", help="create a function record")
    new_function.add_argument("--id", required=True)
    new_function.add_argument("--address", required=True)
    new_function.add_argument("--name", required=True)
    new_function.add_argument("--mame-set", default="kinst")
    new_function.add_argument("--revision", default="v1.5d")
    new_function.add_argument("--kind", choices=sorted(VALID_KINDS), default="static-recompilation")
    new_function.set_defaults(handler=command_new_function)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.handler(args)


if __name__ == "__main__":
    raise SystemExit(main())
