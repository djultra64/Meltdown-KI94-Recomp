from __future__ import annotations

import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile
import unittest
from unittest import mock

from tests.test_type1a_state_to_render import SAMPLE_TRACE


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_evidence", ROOT / "tools" / "verify_evidence.py"
)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)
MANIFEST = VERIFY.load_manifest(ROOT / "provenance/verification.json")


def public_clone(directory: str) -> Path:
    """Synthetic checkout fixture: deliberately excludes ignored work/."""
    root = Path(directory) / "checkout"
    paths = {"provenance/verification.json"}
    for anchor in MANIFEST["anchors"]:
        paths.update(item["path"] for item in anchor["public_dependencies"])
    for relative in paths:
        destination = root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT / relative, destination)
    return root


def emitter_lines(*, mutate_row: int | None = None, truncate: bool = False) -> str:
    """Synthetic parser/replay text derived from the public digest fixture."""
    with (ROOT / "tests/fixtures/ki15d_88003d30.csv").open(
        "r", encoding="utf-8", newline=""
    ) as handle:
        rows = list(csv.DictReader(handle))
    lines = []
    for index, row in enumerate(rows):
        digest = row["digest"]
        if index == mutate_row:
            digest = f"{int(digest, 16) ^ 1:08x}"
        lines.append(
            f"KI_EMITTER_ORACLE countdown={row['countdown'].upper()} "
            f"cases=100 digest={digest.upper()}"
        )
    lines.append("KI_EMITTER_ORACLE complete=10000")
    if truncate:
        lines.pop(-2)
    return "\n".join(lines) + "\n"


def synthetic_emitter_segment(root: Path) -> Path:
    """Build only the public 68-byte block; never substitute for source identity."""
    segment = bytearray(208048)
    words = []
    for line in (ROOT / "provenance/asm/ki15d_88003d30.s").read_text(
        encoding="utf-8"
    ).splitlines():
        match = re.match(r"[0-9a-f]{8}: ([0-9a-f]{8}) ", line)
        if match:
            words.append(int(match.group(1), 16))
    block = b"".join(struct.pack("<I", word) for word in words)
    self_hash = hashlib.sha256(block).hexdigest()
    assert self_hash == MANIFEST["anchors"][0]["source"]["sha256"]
    offset = int(MANIFEST["anchors"][0]["source"]["segment_offset"], 16)
    segment[offset : offset + len(block)] = block
    path = root / MANIFEST["source_segment"]["path"]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(segment)
    return path


class ManifestTests(unittest.TestCase):
    def test_current_public_manifest_passes_without_replay_claim(self) -> None:
        result = VERIFY.validate_manifest(MANIFEST, ROOT)
        self.assertEqual(result["category"], "PASS")
        self.assertIn("no original replay", result["statement"])
        self.assertEqual(result["public_dependencies_checked"], 17)

    def test_minimal_public_checkout_marks_private_evidence_unavailable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = public_clone(directory)
            result = VERIFY.validate_manifest(MANIFEST, root)
            self.assertEqual(
                [item["status"] for item in result["private_evidence"]],
                ["UNAVAILABLE", "UNAVAILABLE", "UNAVAILABLE"],
            )
            replay, status = VERIFY.replay(MANIFEST, root)
            self.assertEqual(status, 2)
            self.assertEqual(replay["category"], "NOT_RUN")
            self.assertEqual(len(replay["missing"]), 3)

    def test_wrong_revision_duplicate_ids_and_unknown_adapter_fail(self) -> None:
        variants = []
        wrong_revision = copy.deepcopy(MANIFEST)
        wrong_revision["identity"]["revision"] = "v1.4"
        variants.append(wrong_revision)
        duplicate = copy.deepcopy(MANIFEST)
        duplicate["anchors"][1]["id"] = duplicate["anchors"][0]["id"]
        variants.append(duplicate)
        adapter = copy.deepcopy(MANIFEST)
        adapter["anchors"][0]["adapter"] = "run-command-from-json"
        variants.append(adapter)
        for manifest in variants:
            with self.subTest(manifest=manifest), self.assertRaises(
                VERIFY.VerificationError
            ):
                VERIFY.validate_manifest(manifest, ROOT)

    def test_false_type_correct_semantic_claims_fail_individually(self) -> None:
        mutations = {
            "implementation base": lambda m: m.__setitem__("implementation_base", "0" * 40),
            "source segment limits": lambda m: m["source_segment"].__setitem__(
                "limits", "Complete boot and runtime identity."
            ),
            "evidence type": lambda m: m["anchors"][0].__setitem__(
                "evidence_type", "fresh complete original execution"
            ),
            "source exit": lambda m: m["anchors"][1]["source"].__setitem__(
                "exit", "full function and scheduler completion"
            ),
            "full ABI claim": lambda m: m["anchors"][0]["abi"].__setitem__(
                "excluded", ["none; full ABI proven"]
            ),
            "initialization": lambda m: m["anchors"][0].__setitem__(
                "initialization", ["uninitialized production gameplay state"]
            ),
            "bounded memory": lambda m: m["anchors"][0].__setitem__(
                "bounded_memory", "All machine memory is equivalent."
            ),
            "coherence claim": lambda m: m["anchors"][1].__setitem__(
                "cache_observation", "All debugger memory is universally cache-coherent."
            ),
            "interception": lambda m: m["anchors"][0].__setitem__(
                "interceptions", ["Constructor body executes before interception."]
            ),
            "CPU mode": lambda m: m["anchors"][0]["acquisition"].__setitem__(
                "cpu_mode", "unknown"
            ),
            "acquisition description": lambda m: m["anchors"][0]["acquisition"].__setitem__(
                "command_description", "Run an arbitrary command from this value."
            ),
            "new capture claim": lambda m: m["anchors"][0]["admission"].__setitem__(
                "limits", "Fresh self-attesting capture created by VERIFY-095."
            ),
            "completion": lambda m: m["anchors"][0].__setitem__(
                "completion_rule", "Accept any number of rows."
            ),
            "native limits": lambda m: m["anchors"][0]["native_verification"].__setitem__(
                "limits", "An old binary proves native PASS."
            ),
            "known limits": lambda m: m["anchors"][1].__setitem__(
                "known_limits", ["No remaining limits; gameplay closure proven."]
            ),
        }
        for label, mutate in mutations.items():
            with self.subTest(label=label):
                manifest = copy.deepcopy(MANIFEST)
                mutate(manifest)
                with self.assertRaises(VERIFY.VerificationError):
                    VERIFY.validate_manifest(manifest, ROOT)

    def test_missing_and_extra_schema_fields_fail(self) -> None:
        variants = []
        missing = copy.deepcopy(MANIFEST)
        del missing["anchors"][0]["cache_observation"]
        variants.append(missing)
        extra = copy.deepcopy(MANIFEST)
        extra["anchors"][0]["fresh_original_execution"] = True
        variants.append(extra)
        nested_extra = copy.deepcopy(MANIFEST)
        nested_extra["anchors"][1]["abi"]["complete"] = True
        variants.append(nested_extra)
        for manifest in variants:
            with self.subTest(manifest=manifest), self.assertRaisesRegex(
                VERIFY.VerificationError, "missing keys|extra keys"
            ):
                VERIFY.validate_manifest(manifest, ROOT)

    def test_malformed_and_duplicate_key_json_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"
            for text in ('{"schema_version":', '{"id":"a","id":"b"}'):
                with self.subTest(text=text):
                    path.write_text(text, encoding="utf-8")
                    with self.assertRaises(VERIFY.VerificationError):
                        VERIFY.load_manifest(path)

    def test_public_fixture_and_harness_staleness_fail(self) -> None:
        for target in (
            "tests/fixtures/ki15d_88003d30.csv",
            "tools/analyze_type1a_state_to_render.py",
        ):
            with self.subTest(target=target), tempfile.TemporaryDirectory() as directory:
                root = public_clone(directory)
                with (root / target).open("a", encoding="utf-8") as handle:
                    handle.write("# synthetic staleness\n")
                with self.assertRaisesRegex(VERIFY.VerificationError, "stale public"):
                    VERIFY.validate_manifest(MANIFEST, root)

    def test_source_provenance_must_match_fixed_load_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = public_clone(directory)
            manifest = copy.deepcopy(MANIFEST)
            anchor = manifest["anchors"][0]
            relative = anchor["source"]["provenance_path"]
            path = root / relative
            record = json.loads(path.read_text(encoding="utf-8"))
            record["original"]["extracted_segment"]["function_size"] = 64
            path.write_text(json.dumps(record), encoding="utf-8")
            actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
            next(
                item for item in anchor["public_dependencies"] if item["path"] == relative
            )["sha256"] = actual_hash
            with self.assertRaisesRegex(VERIFY.VerificationError, "stale source identity"):
                VERIFY.validate_manifest(manifest, root)

    def test_function_record_game_cpu_and_source_bounds_are_checked(self) -> None:
        for field in ("game", "cpu", "bounds"):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = public_clone(directory)
                manifest = copy.deepcopy(MANIFEST)
                anchor = manifest["anchors"][0]
                relative = anchor["source"]["provenance_path"]
                path = root / relative
                record = json.loads(path.read_text(encoding="utf-8"))
                if field == "game":
                    record["game"]["revision"] = "v1.4"
                elif field == "cpu":
                    record["original"]["cpu"] = "R4300LE"
                else:
                    anchor["source"]["segment_offset"] = "0x00032ca0"
                    record["original"]["virtual_address"] = "0x88032ca0"
                    record["original"]["extracted_segment"]["segment_offset"] = "0x00032ca0"
                path.write_text(json.dumps(record), encoding="utf-8")
                actual_hash = hashlib.sha256(path.read_bytes()).hexdigest()
                next(
                    item for item in anchor["public_dependencies"] if item["path"] == relative
                )["sha256"] = actual_hash
                with self.assertRaises(VERIFY.VerificationError):
                    VERIFY.validate_manifest(manifest, root)

    def test_dropped_fixed_dependency_is_rejected(self) -> None:
        manifest = copy.deepcopy(MANIFEST)
        manifest["anchors"][0]["public_dependencies"].pop()
        with self.assertRaisesRegex(VERIFY.VerificationError, "dependency set"):
            VERIFY.validate_manifest(manifest, ROOT)

    def test_path_traversal_and_public_symlink_fail(self) -> None:
        traversal = copy.deepcopy(MANIFEST)
        traversal["anchors"][0]["public_dependencies"][0]["path"] = "../escape"
        with self.assertRaisesRegex(VERIFY.VerificationError, "escapes"):
            VERIFY.validate_manifest(traversal, ROOT)
        with tempfile.TemporaryDirectory() as directory:
            root = public_clone(directory)
            relative = "tests/fixtures/ki15d_88003d30.csv"
            target = root / relative
            target.unlink()
            target.symlink_to(ROOT / relative)
            with self.assertRaisesRegex(VERIFY.VerificationError, "symlink"):
                VERIFY.validate_manifest(MANIFEST, root)

    def test_wrong_private_segment_identity_fails_before_adapter(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = public_clone(directory)
            path = root / MANIFEST["source_segment"]["path"]
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic wrong segment identity\n")
            with self.assertRaisesRegex(VERIFY.VerificationError, "identity differs"):
                VERIFY.replay(MANIFEST, root)

    def test_wrong_private_capture_identity_fails_before_adapter(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = public_clone(directory)
            segment_descriptor = MANIFEST["source_segment"]
            segment_path = root / segment_descriptor["path"]
            segment_path.parent.mkdir(parents=True, exist_ok=True)
            segment_path.write_bytes(b"synthetic segment placeholder\n")
            capture_descriptor = MANIFEST["anchors"][0]["private_capture"]
            capture_path = root / capture_descriptor["path"]
            capture_path.parent.mkdir(parents=True, exist_ok=True)
            capture_path.write_bytes(b"synthetic wrong capture identity\n")
            original_hash_file = VERIFY._hash_file

            def hash_with_admitted_segment(path: Path) -> tuple[int, str]:
                if path == segment_path:
                    return segment_descriptor["size"], segment_descriptor["sha256"]
                return original_hash_file(path)

            with mock.patch.object(VERIFY, "_hash_file", side_effect=hash_with_admitted_segment):
                with self.assertRaisesRegex(VERIFY.VerificationError, "identity differs"):
                    VERIFY.replay(MANIFEST, root, selected="emitter-decisions")


class AdapterTests(unittest.TestCase):
    def make_emitter_root(self, directory: str, capture: str) -> tuple[Path, dict[str, object]]:
        root = Path(directory)
        for relative in (
            "tools/analyze_type1a_emission.py",
            "tests/fixtures/ki15d_88003d30.csv",
        ):
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        capture_path = root / "capture.log"
        capture_path.write_text(capture, encoding="utf-8")
        anchor = {"private_capture": {"path": "capture.log"}}
        return root, anchor

    def test_emitter_adapter_accepts_all_65536_synthetic_decisions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, anchor = self.make_emitter_root(directory, emitter_lines())
            result = VERIFY._emitter_adapter(root, anchor)
            self.assertEqual(result, {"rows": 256, "decisions": 65536, "completion": "validated"})

    def test_matching_synthetic_hash_cannot_approve_truncated_capture(self) -> None:
        capture = emitter_lines(truncate=True)
        synthetic_hash = hashlib.sha256(capture.encode()).hexdigest()
        self.assertRegex(synthetic_hash, r"^[0-9a-f]{64}$")
        with tempfile.TemporaryDirectory() as directory:
            root, anchor = self.make_emitter_root(directory, capture)
            with self.assertRaisesRegex(VERIFY.VerificationError, "incomplete"):
                VERIFY._emitter_adapter(root, anchor)

    def test_synthetic_end_to_end_replay_reaches_real_emitter_adapter(self) -> None:
        for truncate, expected_category in ((False, "PASS"), (True, "error")):
            with self.subTest(truncate=truncate), tempfile.TemporaryDirectory() as directory:
                root = public_clone(directory)
                manifest = copy.deepcopy(MANIFEST)
                segment_path = synthetic_emitter_segment(root)
                capture = emitter_lines(truncate=truncate)
                descriptor = manifest["anchors"][0]["private_capture"]
                capture_path = root / descriptor["path"]
                capture_path.parent.mkdir(parents=True, exist_ok=True)
                capture_path.write_text(capture, encoding="utf-8")
                descriptor["size"] = len(capture.encode())
                descriptor["sha256"] = hashlib.sha256(capture.encode()).hexdigest()
                original_hash_file = VERIFY._hash_file

                def admit_only_synthetic_full_segment(path: Path) -> tuple[int, str]:
                    if path == segment_path:
                        source = manifest["source_segment"]
                        return source["size"], source["sha256"]
                    return original_hash_file(path)

                with mock.patch.object(
                    VERIFY, "_hash_file", side_effect=admit_only_synthetic_full_segment
                ):
                    if truncate:
                        with self.assertRaisesRegex(VERIFY.VerificationError, "incomplete"):
                            VERIFY.replay(manifest, root, selected="emitter-decisions")
                    else:
                        result, status = VERIFY.replay(
                            manifest, root, selected="emitter-decisions"
                        )
                        self.assertEqual(status, 0)
                        self.assertEqual(result["category"], expected_category)

    def test_emitter_adapter_reports_first_output_difference(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, anchor = self.make_emitter_root(directory, emitter_lines(mutate_row=7))
            with self.assertRaisesRegex(VERIFY.VerificationError, "first difference at row 7"):
                VERIFY._emitter_adapter(root, anchor)

    def test_bridge_adapter_rejects_incomplete_public_synthetic_trace(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in (
                "tools/analyze_type1a_state_to_render.py",
                "tests/fixtures/ki15d_88001b90_type1a_bridge.csv",
            ):
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, destination)
            (root / "capture.log").write_text(SAMPLE_TRACE, encoding="utf-8")
            anchor = {"private_capture": {"path": "capture.log"}}
            with self.assertRaisesRegex(VERIFY.VerificationError, "first difference"):
                VERIFY._bridge_adapter(root, anchor)

    def test_bridge_dependency_reports_first_field_difference(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            analyzer = VERIFY._import_module(
                ROOT / "tools/analyze_type1a_state_to_render.py",
                "verify_bridge_public_synthetic",
            )
            calls = analyzer.parse_trace(SAMPLE_TRACE.splitlines())
            row = analyzer.derive_row(calls[0])
            row.update({"tick": 0, "pass_index": 0, "milestones": "first_visible"})
            bridge = analyzer.bridge_rows(calls, [row])
            fixture = root / "bridge.csv"
            with fixture.open("w", encoding="utf-8", newline="") as handle:
                analyzer.write_bridge_csv(bridge, handle)
            fixture.write_text(
                fixture.read_text(encoding="utf-8").replace(",257,140,", ",258,140,"),
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValueError, "destination_x"):
                analyzer.compare_bridge_fixture(bridge, fixture)


if __name__ == "__main__":
    unittest.main()
