from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "knowledge", PROJECT_ROOT / "tools" / "knowledge.py"
)
assert SPEC is not None and SPEC.loader is not None
KNOWLEDGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(KNOWLEDGE)


class CatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.catalog = KNOWLEDGE.load_json(PROJECT_ROOT / "provenance" / "knowledge.json")
        cls.tracked = KNOWLEDGE._tracked_paths(PROJECT_ROOT)

    def validate(self, catalog: object) -> tuple[list[str], list[str]]:
        return KNOWLEDGE.validate_catalog(catalog, PROJECT_ROOT, self.tracked)

    def test_current_catalog_covers_seed_baseline(self) -> None:
        self.assertTrue(
            {f"K-{index:03d}" for index in range(1, 17)}
            <= {item["id"] for item in self.catalog["facts"]}
        )
        self.assertTrue(
            {f"U-{index:03d}" for index in range(1, 15)}
            <= {item["id"] for item in self.catalog["uncertainties"]}
        )
        errors, unavailable = self.validate(self.catalog)
        self.assertEqual(errors, [])
        self.assertIsInstance(unavailable, list)

    def test_minimal_checkout_reports_optional_sources_unavailable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tracked: set[str] = set()
            required = {
                source["path"]
                for entry in self.catalog["facts"] + self.catalog["uncertainties"]
                for source in entry["evidence"]
                if source["availability"] == "tracked"
            }
            required.update(
                path.relative_to(PROJECT_ROOT).as_posix()
                for path in (PROJECT_ROOT / "provenance/functions").glob("*.json")
            )
            for relative in required:
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(PROJECT_ROOT / relative, destination)
                tracked.add(relative)
            errors, unavailable = KNOWLEDGE.validate_catalog(
                self.catalog, root, tracked
            )
            self.assertEqual(errors, [])
            self.assertEqual(len(unavailable), 6)

    def test_invalid_ids_are_diagnostic_not_type_errors(self) -> None:
        for value in ("K-1", {"bad": "shape"}):
            with self.subTest(value=value):
                catalog = copy.deepcopy(self.catalog)
                catalog["facts"][0]["id"] = value
                errors, _ = self.validate(catalog)
                self.assertTrue(any("identifiers" in error for error in errors))

    def test_duplicate_id_is_rejected_without_freezing_future_ids(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        future = copy.deepcopy(catalog["facts"][0])
        future["id"] = "K-017"
        future["related_uncertainties"] = []
        catalog["facts"].append(future)
        errors, _ = self.validate(catalog)
        self.assertFalse(any("identifiers" in error for error in errors))
        catalog["facts"][-1]["id"] = "K-016"
        errors, _ = self.validate(catalog)
        self.assertTrue(any("identifiers" in error for error in errors))

    def test_invalid_confidence_and_evidence_class_are_rejected(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        catalog["facts"][0]["confidence"] = {"not": "a string"}
        catalog["facts"][0]["evidence"][0]["class"] = ["observation"]
        errors, _ = self.validate(catalog)
        self.assertTrue(any("confidence" in error for error in errors))
        self.assertTrue(any(".class is invalid" in error for error in errors))

    def test_wrong_game_identity_and_malformed_locator_are_rejected(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        catalog["game"] = {"mame_set": "kinst2", "revision": "v1.5d"}
        catalog["facts"][0]["evidence"][0]["locator"] = {"bad": "shape"}
        errors, _ = self.validate(catalog)
        self.assertTrue(any("game identity" in error for error in errors))
        self.assertTrue(any("locator" in error for error in errors))

    def test_missing_or_untracked_required_evidence_is_rejected(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        catalog["facts"][0]["evidence"] = [{
            "class": "observation",
            "availability": "tracked",
            "path": "docs/does-not-exist.md",
            "locator": "missing",
        }]
        errors, _ = self.validate(catalog)
        self.assertTrue(any("not Git-tracked" in error for error in errors))
        self.assertTrue(any("missing or unsafe" in error for error in errors))

    def test_missing_optional_evidence_is_reported_unavailable(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        catalog["facts"][0]["evidence"].append({
            "class": "review",
            "availability": "local-optional",
            "path": "work/reviews/not-present.md",
            "locator": "not present",
        })
        errors, unavailable = self.validate(catalog)
        self.assertEqual(errors, [])
        self.assertEqual(unavailable, ["K-001: work/reviews/not-present.md"])

    def test_bad_function_and_uncertainty_links_are_rejected(self) -> None:
        catalog = copy.deepcopy(self.catalog)
        catalog["facts"][0]["function_records"] = ["ki15d_missing"]
        catalog["facts"][0]["related_uncertainties"] = [{"bad": "shape"}]
        errors, _ = self.validate(catalog)
        self.assertTrue(any("unknown function record" in error for error in errors))
        self.assertTrue(any("unknown ID" in error for error in errors))

    def test_malformed_and_duplicate_key_json_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.json"
            for body in ('{"facts":', '{"schema_version": 1, "schema_version": 1}'):
                with self.subTest(body=body):
                    path.write_text(body, encoding="utf-8")
                    with self.assertRaises(KNOWLEDGE.KnowledgeError):
                        KNOWLEDGE.load_json(path)


class RecoveryTests(unittest.TestCase):
    def make_root(self, directory: str) -> tuple[Path, tuple[str, ...]]:
        root = Path(directory) / "project"
        (root / "docs").mkdir(parents=True)
        (root / "work" / "planning").mkdir(parents=True)
        (root / "work" / "knowledge-backups").mkdir(parents=True)
        (root / "docs" / "a.md").write_text("alpha\n", encoding="utf-8")
        (root / "work" / "planning" / "b.md").write_text("beta\n", encoding="utf-8")
        return root, ("docs/a.md", "work/planning/b.md")

    def export(self, root: Path, allowlist: tuple[str, ...]) -> tuple[Path, str]:
        bundle = Path("work/knowledge-backups/bundle")
        manifest_hash, count = KNOWLEDGE.export_bundle(root, bundle, allowlist)
        self.assertEqual(count, len(allowlist))
        return root / bundle, manifest_hash

    def test_export_and_fresh_restore_compare_every_byte(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            bundle, manifest_hash = self.export(root, allowlist)
            restored_hash, count = KNOWLEDGE.restore_check(
                root,
                bundle,
                Path("work/knowledge-backups/restored"),
                allowlist,
                manifest_hash,
            )
            self.assertEqual(restored_hash, manifest_hash)
            self.assertEqual(count, 2)
            self.assertEqual(
                (root / "work/knowledge-backups/restored/docs/a.md").read_bytes(),
                b"alpha\n",
            )

    def test_path_escape_and_case_collision_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.make_root(directory)
            for allowlist in (("../outside.md",), ("docs/a.md", "DOCS/A.MD")):
                with self.subTest(allowlist=allowlist):
                    with self.assertRaises(KNOWLEDGE.KnowledgeError):
                        KNOWLEDGE.export_bundle(
                            root,
                            Path("work/knowledge-backups/bundle"),
                            allowlist,
                        )

    def test_source_and_destination_symlinks_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            outside = Path(directory) / "outside"
            outside.mkdir()
            (root / "docs" / "link.md").symlink_to(outside / "target.md")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.export_bundle(
                    root,
                    Path("work/knowledge-backups/bad"),
                    ("docs/link.md",),
                )
            bundle, _ = self.export(root, allowlist)
            (root / "work/knowledge-backups/link").symlink_to(outside, target_is_directory=True)
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(
                    root,
                    bundle,
                    Path("work/knowledge-backups/link/restored"),
                    allowlist,
                )

    def test_source_parent_and_backup_root_symlinks_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.make_root(directory)
            outside = Path(directory) / "outside"
            outside.mkdir()
            shutil.rmtree(root / "docs")
            (root / "docs").symlink_to(outside, target_is_directory=True)
            (outside / "a.md").write_text("alpha\n", encoding="utf-8")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.export_bundle(
                    root,
                    Path("work/knowledge-backups/bundle"),
                    ("docs/a.md",),
                )
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            shutil.rmtree(root / "work/knowledge-backups")
            (root / "work/knowledge-backups").symlink_to(
                Path(directory) / "missing", target_is_directory=True
            )
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.export_bundle(
                    root, Path("work/knowledge-backups/bundle"), allowlist
                )

    def test_non_text_allowlisted_source_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.make_root(directory)
            (root / "docs" / "a.md").write_bytes(b"text\0payload")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.export_bundle(
                    root,
                    Path("work/knowledge-backups/bundle"),
                    ("docs/a.md",),
                )

    def test_corrupt_or_incomplete_bundle_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            bundle, _ = self.export(root, allowlist)
            (bundle / "files/docs/a.md").write_text("changed\n", encoding="utf-8")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(
                    root,
                    bundle,
                    Path("work/knowledge-backups/restored"),
                    allowlist,
                )
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            bundle, _ = self.export(root, allowlist)
            manifest_path = bundle / KNOWLEDGE.MANIFEST_NAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["files"].pop()
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(
                    root,
                    bundle,
                    Path("work/knowledge-backups/restored"),
                    allowlist,
                )

    def test_unmanifested_file_and_wrong_manifest_identity_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            bundle, manifest_hash = self.export(root, allowlist)
            (bundle / "extra.txt").write_text("extra\n", encoding="utf-8")
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(
                    root,
                    bundle,
                    Path("work/knowledge-backups/restored"),
                    allowlist,
                )
            (bundle / "extra.txt").unlink()
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(
                    root,
                    bundle,
                    Path("work/knowledge-backups/restored"),
                    allowlist,
                    "0" * len(manifest_hash),
                )

    def test_export_and_restore_overwrite_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root, allowlist = self.make_root(directory)
            bundle, _ = self.export(root, allowlist)
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.export_bundle(root, bundle, allowlist)
            destination = root / "work/knowledge-backups/restored"
            destination.mkdir()
            with self.assertRaises(KNOWLEDGE.KnowledgeError):
                KNOWLEDGE.restore_check(root, bundle, destination, allowlist)


if __name__ == "__main__":
    unittest.main()
