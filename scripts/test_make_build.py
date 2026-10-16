"""Offline checks for public build orchestration and release ZIP validation."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import unittest
import warnings
import zipfile

from release_version import identity
from verify_app_zip import verify

ROOT = Path(__file__).resolve().parents[1]


class BuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="pt-make-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.log = self.root / "calls"
        self.env = dict(os.environ, CALLS=str(self.log))
        for key in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL", "PROSPEROTUBE_PS5_DIST"):
            self.env.pop(key, None)

    def test_make_order_and_failure(self):
        shutil.copyfile(ROOT / "Makefile", self.root / "Makefile")
        scripts = self.root / "scripts"
        scripts.mkdir()
        (scripts / "bootstrap_build_deps.sh").write_text(
            'printf "deps\n" >> "$CALLS"\nexit "${DEPS_RC:-0}"\n')
        (scripts / "build_ps5.sh").write_text('printf "build\n" >> "$CALLS"\n')
        (scripts / "install_host_deps.sh").write_text('printf "host-deps\n" >> "$CALLS"\nexit 99\n')
        (scripts / "verify_app_zip.py").write_text(
            'import os, sys\nwith open(os.environ["CALLS"], "a") as f: f.write("verify " + sys.argv[1] + "\\n")\n')
        for target in ([], ["zip"], ["all"]):
            with self.subTest(target=target):
                self.log.unlink(missing_ok=True)
                result = subprocess.run(["make", "--no-print-directory", *target], cwd=self.root,
                                        env=self.env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.log.read_text().splitlines(),
                                 ["deps", "build", f"verify {self.root}/dist/PPSA99142.zip"])
        self.log.unlink()
        destination = self.root / "custom output"
        result = subprocess.run(["make", "--no-print-directory", "zip"], cwd=self.root,
                                env=dict(self.env, PROSPEROTUBE_PS5_DIST=str(destination)),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.log.read_text().splitlines(),
                         ["deps", "build", f"verify {destination}/PPSA99142.zip"])
        self.log.unlink()
        result = subprocess.run(["make", "--no-print-directory", "zip"], cwd=self.root,
                                env=dict(self.env, DEPS_RC="17"), capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.log.read_text().splitlines(), ["deps"])

    def archive(self, fault=None):
        title, version, _ = identity()
        archive = self.root / f"{title}.zip"
        meta = {"titleId": title, "contentVersion": "99.999.999" if fault == "version" else version}
        entries = [(f"{title}/eboot.bin", b"synthetic executable"),
                   (f"{title}/sce_sys/param.json", json.dumps(meta).encode())]
        if fault == "traversal": entries.append((f"{title}/../outside", b"bad"))
        if fault == "duplicate": entries.append(entries[0])
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(archive, "w") as output:
                for name, data in entries:
                    item = zipfile.ZipInfo(name)
                    item.create_system = 3
                    item.external_attr = (stat.S_IFREG | (0o644 if fault == "permissions" else 0o777)) << 16
                    output.writestr(item, data)
        return archive

    def test_zip_checksum_and_rejections(self):
        archive = self.archive()
        verify(archive)
        manifest = self.root / "SHA256SUMS"
        self.assertEqual(manifest.read_text(), f"{hashlib.sha256(archive.read_bytes()).hexdigest()}  {archive.name}\n")
        for fault in ("version", "traversal", "permissions", "duplicate"):
            with self.subTest(fault=fault):
                manifest.unlink(missing_ok=True)
                with self.assertRaises(ValueError): verify(self.archive(fault))
                self.assertFalse(manifest.exists())

    def test_host_installer_is_explicit_and_rejects_other_distros(self):
        commands = self.root / "bin"
        commands.mkdir()
        for name, body in (("sudo", 'exec "$@"'),
                           ("apt-get", 'printf "%s\n" "$*" >> "$CALLS"')):
            command = commands / name
            command.write_text("#!/bin/bash\n" + body + "\n")
            command.chmod(0o755)
        source = (ROOT / "scripts/install_host_deps.sh").read_text()
        self.assertIn("source /etc/os-release", source)
        script = self.root / "host-deps.sh"
        env = dict(self.env, PATH=str(commands) + ":/usr/bin:/bin")
        for system, expected in (("ID=ubuntu; VERSION_ID=24.04", 0), ("ID=debian; VERSION_ID=13", 2)):
            with self.subTest(system=system):
                self.log.unlink(missing_ok=True)
                script.write_text(source.replace("source /etc/os-release", system))
                result = subprocess.run(["bash", str(script)], env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, expected, result.stderr)
                if expected:
                    self.assertFalse(self.log.exists())
                else:
                    calls = self.log.read_text().splitlines()
                    self.assertEqual(len(calls), 2)
                    self.assertEqual(calls[0], "update")
                    self.assertTrue(calls[1].startswith("install --yes --no-install-recommends "))
                    for package in ("meson", "libavfilter-dev", "libegl1-mesa-dev", "libgl1-mesa-dri"):
                        self.assertIn(package, calls[1].split())


if __name__ == "__main__":
    unittest.main()
