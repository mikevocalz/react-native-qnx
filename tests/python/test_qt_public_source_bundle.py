#!/usr/bin/env python3
"""Host-only source-bundle metadata fixtures, never Qt provenance evidence.

The copied recipe uses a tiny synthetic archive and synthetic CVE diffs. Only
the temporary recipe's pinned archive hash changes; production pins stay intact.
"""

import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
PATCHES = [
    "qtbase-qnx-offscreen-surface.patch",
    "qtbase-qnx-resize-swap.patch",
    "qtbase-qnx-software-grab.patch",
]
DIFFS = [
    "CVE-2026-116337-qtbase-6.12-0001.diff",
    "CVE-2026-116337-qtbase-6.12-0002.diff",
    "CVE-2026-105924-qtbase-6.12.diff",
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class SourceBundleMetadataTest(unittest.TestCase):
    def setUp(self):
        self.scratch = tempfile.TemporaryDirectory(prefix="qt-source-metadata-")
        self.addCleanup(self.scratch.cleanup)
        self.root = Path(self.scratch.name)
        self.repo = self.root / "fixture-repo"
        self.scripts = self.repo / "scripts/qt-public"
        self.scripts.mkdir(parents=True)
        for name in ("make-source-bundle.sh", "qnx-patch-set.sh"):
            shutil.copyfile(REPO / "scripts/qt-public" / name, self.scripts / name)
        (self.scripts / "patches").mkdir()
        for name in PATCHES:
            shutil.copyfile(
                REPO / "scripts/qt-public/patches" / name,
                self.scripts / "patches" / name,
            )
        offer = self.repo / "docs/distribution/source-offer.md"
        offer.parent.mkdir(parents=True)
        offer.write_text("SYNTHETIC TEST OFFER. Not a Qt distribution.\n")

        self.public = self.root / "public"
        download = self.public / "download"
        download.mkdir(parents=True)
        archive = download / "qt-everywhere-src-6.12.0.tar.xz"
        payload = b"synthetic metadata fixture, not Qt source\n"
        with tarfile.open(archive, "w:xz") as bundle:
            entry = tarfile.TarInfo("synthetic/README")
            entry.size = len(payload)
            bundle.addfile(entry, io.BytesIO(payload))
        self.archive_hash = sha(archive)
        script = self.scripts / "make-source-bundle.sh"
        text = script.read_text()
        old_hash = "98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165"
        self.assertEqual(text.count("archive_sha=" + old_hash), 1)
        script.write_text(text.replace("archive_sha=" + old_hash,
                                       "archive_sha=" + self.archive_hash))

        (download / "cve-patches").mkdir()
        hashes = []
        for name in DIFFS:
            path = download / "cve-patches" / name
            path.write_text("Synthetic CVE diff fixture: " + name + "\n")
            hashes.append(sha(path) + "  " + name)
        (self.scripts / "apply-cve-patches.sh").write_text("\n".join(hashes) + "\n")

        subprocess.run(["git", "init", "-q", str(self.repo)], check=True)
        subprocess.run(["git", "-C", str(self.repo), "add", "."], check=True)
        subprocess.run([
            "git", "-C", str(self.repo), "-c", "user.name=Fixture",
            "-c", "user.email=fixture@example.invalid", "-c", "commit.gpgsign=false",
            "commit", "-qm", "synthetic source-bundle fixture",
        ], check=True)
        self.stamp = self.public / "src/qt-everywhere-src-6.12.0/qtbase/.qnx-patches-applied"
        self.stamp.parent.mkdir(parents=True)

    def run_bundle(self, selection, name):
        env = os.environ.copy()
        env.update(QT_PUBLIC_ROOT=str(self.public), WITH_ARCHIVE="1")
        env.pop("QNX_SOFTWARE_GRAB", None)
        if selection is not None:
            env["QNX_SOFTWARE_GRAB"] = selection
        output = self.root / name
        result = subprocess.run(
            ["bash", str(self.scripts / "make-source-bundle.sh"), str(output)],
            env=env, text=True, capture_output=True,
        )
        return result, output

    def test_default_and_opt_in_manifest_match_selected_recipe(self):
        for selection in (None, "0", "1"):
            with self.subTest(selection=selection):
                selected = PATCHES if selection == "1" else PATCHES[:2]
                self.stamp.write_text("\n".join(selected) + "\n")
                result, output = self.run_bundle(selection, "out-" + str(selection))
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                root = output / "rnqnx-qt612-source-bundle"
                manifest = json.loads((root / "SOURCE-MANIFEST.json").read_text())
                self.assertEqual(manifest["rnQnxPatches"], {
                    "softwareGrab": selection == "1",
                    "appliedByScript": selected,
                    "notAppliedByScript": [] if selection == "1" else PATCHES[2:],
                })
                self.assertEqual(manifest["qt"]["sha256"], self.archive_hash)
                self.assertTrue(manifest["qt"]["includedInBundle"])
                self.assertEqual(sha(root / manifest["qt"]["archive"]), self.archive_hash)
                for patch in selected:
                    relative = "qt-public/patches/" + patch
                    self.assertEqual(manifest["recipe"]["files"][relative],
                                     sha(self.scripts / "patches" / patch))
                    self.assertEqual(sha(root / relative),
                                     manifest["recipe"]["files"][relative])
                self.assertIn("qt-public/qnx-patch-set.sh", manifest["recipe"]["files"])
                for name in DIFFS:
                    relative = "cve-patches/" + name
                    self.assertEqual(manifest["appliedQtbaseDiffs"]["files"][relative],
                                     sha(self.public / "download" / relative))
                archive = output / "rnqnx-qt612-source-bundle.tar.gz"
                expected = sha(archive) + "  " + archive.name
                self.assertEqual(Path(str(archive) + ".sha256").read_text().strip(), expected)

    def test_mismatched_stamp_refuses_both_variants_before_output(self):
        for selection in ("0", "1"):
            with self.subTest(selection=selection):
                other = PATCHES if selection == "0" else PATCHES[:2]
                self.stamp.write_text("\n".join(other) + "\n")
                before = self.stamp.read_bytes()
                result, output = self.run_bundle(selection, "mismatch-" + selection)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("does not match the applied QNX patch stamp", result.stderr)
                self.assertFalse(output.exists())
                self.assertEqual(self.stamp.read_bytes(), before)


if __name__ == "__main__":
    unittest.main(verbosity=2)
