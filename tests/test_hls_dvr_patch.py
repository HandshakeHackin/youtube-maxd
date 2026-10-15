"""Check reproducible pinned HLS overlays without compiling/installing an SDK."""
from pathlib import Path
import hashlib
import importlib.util
import os
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('hls_patch', root / 'scripts/patch_ffmpeg_hls_dvr.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
def check_patch(pristine_path):
    pristine = Path(pristine_path).read_bytes()
    assert hashlib.sha256(pristine).hexdigest() == module.PRISTINE_SHA256
    with tempfile.TemporaryDirectory(prefix='tube-hls-patch-') as directory:
        source = Path(directory) / 'hls.c'
        backup = source.with_name('hls.c.prosperotube-original')
        source.write_bytes(pristine)
        module.apply_file(source)
        expected = source.read_bytes()
        assert backup.read_bytes() == pristine
        assert source.with_name('prosperotube_hls_dvr.inc').read_bytes() == (root/'scripts/ffmpeg_hls_dvr.inc').read_bytes()
        module.apply_file(source)
        assert source.read_bytes() == expected
        # A prior layout is replaced from the checked original, never patched twice.
        source.write_bytes(expected.replace(expected.splitlines()[0], b'/* ProsperoTube HLS DVR overlay: older */', 1))
        module.apply_file(source)
        assert source.read_bytes() == expected
        def refused():
            before=source.read_bytes()
            try: module.apply_file(source)
            except RuntimeError: pass
            else: raise AssertionError('unverified HLS source accepted')
            assert source.read_bytes()==before
        source.write_bytes(expected+b'/* unexpected edit */\n')
        refused()
        source.write_bytes(expected)
        backup.write_bytes(pristine+b'/* damaged original */\n')
        refused()
        backup.unlink()
        source.write_bytes(pristine+b'/* unknown source */\n')
        refused()
        assert not backup.exists()
        source.write_bytes(expected.replace(expected.splitlines()[0], b'/* ProsperoTube HLS DVR overlay: older */', 1))
        refused()
    print('PASS: pinned HLS original, idempotent apply, changed-layout regeneration and corrupt/unknown source refusal')


def test_hls_dvr_patch():
    source = os.environ.get('PROSPEROTUBE_DVR_FFMPEG_SOURCE')
    if not source:
        import pytest
        pytest.skip('Set PROSPEROTUBE_DVR_FFMPEG_SOURCE for the pinned HLS patch check')
    check_patch(Path(source) / 'libavformat/hls.c.prosperotube-original')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: test_hls_dvr_patch.py PRISTINE_HLS_C')
    check_patch(sys.argv[1])
