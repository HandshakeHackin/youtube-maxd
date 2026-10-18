"""The Lapy helper build follows ProsperoEden's method and refuses a helper that differs.

Runs offline with fake build outputs; scripts/build_lapy_helper.py is not asked to
fetch or build anything.
Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("build_lapy_helper", ROOT / "scripts/build_lapy_helper.py")
helper = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(helper)

TITLE = "PPSA99991"
ELF = b"\x7fELF\x02\x01" + bytes(64)


def _manifest(elf=ELF, **changes):
    manifest = {
        "build_id": "test",
        "schema": "lapy-owned-build/1",
        "target_title": TITLE,
        "mode": "elf-helper",
        "max_requests": 1,
        "service": False,
        "require_client_result": False,
        "console_validated": False,
        "features": ["root_layout_probe_retry"],
        "protocol_sha256": helper.PROTOCOL_SHA256,
        "elf_sha256": hashlib.sha256(elf).hexdigest(),
    }
    manifest.update(changes)
    return manifest


def _expect_refusal(action, text):
    try:
        action()
    except RuntimeError as error:
        assert text in str(error), error
        return
    raise AssertionError(f"accepted a helper that should fail with {text!r}")


def test_pins_match_prosperoeden():
    # ProsperoEden tools/deps.json and tools/build-lapy-helper.py at 971ad0a (2026-10-13).
    assert helper.LAPY_URL == "https://github.com/blackbearreloaded/PS5-Lapy-JB-Daemon.git"
    assert helper.LAPY_COMMIT == "57fcc19b7f57e46c823d6f2a6c6233ebf3288079"
    assert helper.SDK_URL.endswith("/sdk/releases/download/v0.42/ps5-payload-sdk.zip")
    assert helper.SDK_SHA256 == "8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da"
    assert "/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log/ps5log.h" in helper.PS5LOG_URL
    assert helper.PS5LOG_SHA256 == "394af67d0f8b60b3335deb53396e52855ea2daa50ca914a456ea7663f48900c6"
    assert helper.PROTOCOL_SHA256 == "bb02c4aa814eaba7a7a423a31b29ff41f786212c2953678cf29434e85fa0f869"


def test_build_uses_upstream_target_without_changes():
    source = (ROOT / "scripts/build_lapy_helper.py").read_text()
    assert '["make", "owned-helper", f"TARGET_TITLE={title}"]' in source
    # No copy or overlay of Lapy's privileged source, no diagnostic build options.
    for retired in ("diagnostic_source", "STOP_DIAGNOSTICS", "--result-file", "build_owned_daemon.py"):
        assert retired not in source, retired
    build = (ROOT / "scripts/build_ps5.sh").read_text()
    assert 'build_lapy_helper.py" PPSA99991 "${work}/prosperotube-lapy"' in build
    assert 'build_lapy_helper.py" PPSA99991 --check-app "${work}/output/app/PPSA99991"' in build
    assert "PROSPEROTUBE_LAPY_" not in build
    prepare = (ROOT / "scripts/prepare_native.py").read_text()
    for name in ("lapy.elf", "lapy-manifest.json", "Lapy-MIT.txt"):
        assert f'prosperotube-lapy/{name}' in prepare


def test_good_build_is_accepted():
    with tempfile.TemporaryDirectory() as directory:
        lapy = Path(directory) / "lapy"
        (lapy / "source").mkdir(parents=True)
        protocol = lapy / "source/lapy_elevation_protocol.h"
        protocol.write_bytes(b"protocol")
        build = Path(directory) / "build"
        build.mkdir()
        (build / "lapy.elf").write_bytes(ELF)
        (build / "lapy-manifest.json").write_text(json.dumps(_manifest()))
        (build / "manifest.json").write_text(json.dumps({"result_file": None}))
        # The checkout's protocol must be the pinned one.
        _expect_refusal(lambda: helper.verify_build(build, lapy, TITLE), "checkout's protocol")
        original = helper.PROTOCOL_SHA256
        try:
            helper.PROTOCOL_SHA256 = hashlib.sha256(b"protocol").hexdigest()
            (build / "lapy-manifest.json").write_text(json.dumps(_manifest(protocol_sha256=helper.PROTOCOL_SHA256)))
            assert helper.verify_build(build, lapy, TITLE)["target_title"] == TITLE
            (build / "manifest.json").write_text(json.dumps({"result_file": "/data/x.log"}))
            _expect_refusal(lambda: helper.verify_build(build, lapy, TITLE), "result file")
            (build / "manifest.json").write_text(json.dumps({"target_snapshot_delay_us": 5}))
            _expect_refusal(lambda: helper.verify_build(build, lapy, TITLE), "snapshot delay")
        finally:
            helper.PROTOCOL_SHA256 = original


def test_manifest_refusals():
    with tempfile.TemporaryDirectory() as directory:
        elf = Path(directory) / "lapy.elf"
        elf.write_bytes(ELF)
        helper.check_manifest(_manifest(), elf, TITLE)
        cases = (
            (_manifest(target_title="PPSA99008"), "target_title"),
            (_manifest(service=True), "service"),
            (_manifest(max_requests=2), "max_requests"),
            (_manifest(mode="service"), "mode"),
            (_manifest(console_validated=True), "console_validated"),
            (_manifest(features=[]), "root-layout retry"),
            (_manifest(protocol_sha256="0" * 64), "protocol"),
            (_manifest(elf_sha256="0" * 64), "differs from its upstream build manifest"),
        )
        for manifest, text in cases:
            _expect_refusal(lambda manifest=manifest: helper.check_manifest(manifest, elf, TITLE), text)
        thirty_two_bit = b"\x7fELF\x01\x01" + bytes(64)
        elf.write_bytes(thirty_two_bit)
        _expect_refusal(lambda: helper.check_manifest(_manifest(thirty_two_bit), elf, TITLE), "ELF64")


def test_packaged_app_check():
    with tempfile.TemporaryDirectory() as directory:
        app = Path(directory) / TITLE
        (app / "assets/prosperotube-notices").mkdir(parents=True)
        (app / "lapy.elf").write_bytes(ELF)
        (app / "lapy-manifest.json").write_text(json.dumps(_manifest()))
        _expect_refusal(lambda: helper.check_app(app, TITLE), "Lapy-MIT.txt")
        (app / "assets/prosperotube-notices/Lapy-MIT.txt").write_text("MIT License\n")
        assert helper.check_app(app, TITLE)["elf_sha256"] == hashlib.sha256(ELF).hexdigest()
        (app / "lapy.elf").write_bytes(ELF + b"changed")
        _expect_refusal(lambda: helper.check_app(app, TITLE), "differs")


if __name__ == "__main__":
    for name, test in sorted(globals().items()):
        if name.startswith("test_") and callable(test):
            test()
    print("PASS: Lapy helper build follows ProsperoEden's pins and checks")
