"""Check the DASH patch against a pinned FFmpeg dashdec.c source tree."""
from pathlib import Path
import argparse
import os

from patch_ffmpeg_dash_seek import patch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, nargs="?")
    args = parser.parse_args()
    if args.source is None:
        source = os.environ.get("PROSPEROTUBE_DASH_SOURCE")
        if not source:
            parser.error("provide dashdec.c or set PROSPEROTUBE_DASH_SOURCE")
        args.source = Path(source)
    original = args.source.read_text()
    updated = patch(original)
    assert patch(updated) == updated, "DASH patch is not idempotent"
    assert updated.count("ProsperoTube DASH HTTP 403 track=") == 1
    assert updated.count("prosperotube_forbidden_logged") == 3
    warning = updated.split("ProsperoTube DASH HTTP 403 track=", 1)[1].split(
        "v->prosperotube_forbidden_logged = 1;", 1
    )[0]
    assert "v->cur_seg->url," not in warning, "signed URL could reach diagnostic"
    assert "expiry_delta_s=" in warning and "v->cur_seg->url_offset" in warning
    print("DASH 403 diagnostic patch: PASS")


if __name__ == "__main__":
    main()
