"""Offline checks for public build orchestration and release ZIP validation."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tarfile
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
                                 ["deps", "build", f"verify {self.root}/dist/PPSA99012.zip"])
        self.log.unlink()
        destination = self.root / "custom output"
        result = subprocess.run(["make", "--no-print-directory", "zip"], cwd=self.root,
                                env=dict(self.env, PROSPEROTUBE_PS5_DIST=str(destination)),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.log.read_text().splitlines(),
                         ["deps", "build", f"verify {destination}/PPSA99012.zip"])
        self.log.unlink()
        result = subprocess.run(["make", "--no-print-directory", "zip"], cwd=self.root,
                                env=dict(self.env, DEPS_RC="17"), capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.log.read_text().splitlines(), ["deps"])

    def archive(self, fault=None):
        title, version, _ = identity()
        archive = self.root / f"{title}.zip"
        meta = {"titleId": title, "contentVersion": "99.999.999" if fault == "version" else version,
                "applicationCategoryType": 0 if fault == "category" else 65536, "contentBadgeType": 2}
        if fault == "intent": meta["gameIntent"] = {}
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
        for fault in ("version", "category", "intent", "traversal", "permissions", "duplicate"):
            with self.subTest(fault=fault):
                manifest.unlink(missing_ok=True)
                with self.assertRaises(ValueError): verify(self.archive(fault))
                self.assertFalse(manifest.exists())

    def test_bootstrap_fetch_cache_and_recipe(self):
        scripts = self.root / "scripts"
        scripts.mkdir()
        (self.root / "standalone").mkdir()
        (self.root / "companion/innertube").mkdir(parents=True)
        commands = self.root / "bin"
        commands.mkdir()
        # Toolchain commands are only probed; the SDK and compiler build scripts are stubs.
        for name in ("git", "clang", "llvm-config", "ld.lld", "llvm-readelf-18", "llvm-objcopy-18",
                     "wget", "unzip", "cmake", "ninja", "make", "meson", "nasm", "pkg-config"):
            command = commands / name
            command.write_text("#!/bin/bash\nexit 0\n")
            command.chmod(0o755)
        node_tree = self.root / "fixture/node-v24.10.0-linux-x64"
        (node_tree / "bin").mkdir(parents=True)
        for name, body in (("node", 'echo v24.10.0'),
                           ("npm", 'printf "npm %s\\n" "$*" >> "$CALLS"')):
            executable = node_tree / "bin" / name
            executable.write_text("#!/bin/bash\n" + body + "\n")
            executable.chmod(0o755)
        archive = self.root / "node.tar.xz"
        with tarfile.open(archive, "w:xz") as output:
            output.add(node_tree, arcname=node_tree.name)
        curl = commands / "curl"
        curl.write_text('#!/bin/bash\nprintf "fetch\\n" >> "$CALLS"\ncp "$FIXTURE_ARCHIVE" "${@: -1}"\n')
        curl.chmod(0o755)
        (scripts / "bootstrap_sdk.sh").write_text('printf "sdk\\n" >> "$CALLS"\n')
        (self.root / "standalone/build_bundle.sh").write_text('printf "bundle\\n" >> "$CALLS"\n')
        shader = scripts / "build_ps5_shader_compiler.sh"
        shader.write_text('''printf "shader\n" >> "$CALLS"
mkdir -p "$PROSPEROTUBE_SHADER_TOOLS/bin" "$PROSPEROTUBE_SHADER_TOOLS/lib"
printf '#!/bin/bash\nexit 0\n' > "$PROSPEROTUBE_SHADER_TOOLS/bin/amdllpc"
chmod +x "$PROSPEROTUBE_SHADER_TOOLS/bin/amdllpc"
touch "$PROSPEROTUBE_SHADER_TOOLS/lib/libspvgen.so"
sha256sum "$0" | cut -d ' ' -f 1 > "$PROSPEROTUBE_SHADER_TOOLS/.prosperotube-recipe"
''')
        source = (ROOT / "scripts/bootstrap_build_deps.sh").read_text()
        pinned = "2642f4428869aca32443660fd71b3918e2be1277a899bdcaeb64c93b54b5af17"
        self.assertIn(pinned, source)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        bootstrap = scripts / "bootstrap_build_deps.sh"
        bootstrap.write_text(source.replace(pinned, digest))
        node_home = self.root / "node"
        env = dict(self.env, PATH=str(commands) + ":/usr/bin:/bin", FIXTURE_ARCHIVE=str(archive),
                   PROSPEROTUBE_NODE_HOME=str(node_home), PROSPEROTUBE_SHADER_TOOLS=str(self.root / "shader"))
        def run(expected):
            self.log.unlink(missing_ok=True)
            result = subprocess.run(["bash", str(bootstrap)], env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(self.log.read_text().splitlines(), expected)
        run(["fetch", "sdk", "npm ci --ignore-scripts", "bundle", "shader"])
        self.assertTrue((node_home / "bin/node").is_file())
        run(["sdk", "npm ci --ignore-scripts", "bundle"])
        shader.write_text(shader.read_text() + "# Changed compiler recipe\n")
        run(["sdk", "npm ci --ignore-scripts", "bundle", "shader"])
        # An invalid download must fail before extraction, SDK work, or npm execution.
        shutil.rmtree(node_home)
        bootstrap.write_text(source.replace(pinned, "0" * 64))
        self.log.unlink()
        result = subprocess.run(["bash", str(bootstrap)], env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.log.read_text().splitlines(), ["fetch"])
        self.assertFalse(node_home.exists())
        self.assertEqual(list(self.root.glob(".node-*")), [])

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
