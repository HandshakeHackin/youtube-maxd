#!/usr/bin/env python3
"""Publish the checked 1.4.1 Actions build and matching source, without replacement.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

REPO = "HandshakeHackin/youtube-maxd"
PR = 4
TAG = "v1.4.1"


def command(*args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def api(endpoint):
    return json.loads(command("gh", "api", endpoint))


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def select():
    pr = api(f"repos/{REPO}/pulls/{PR}")
    if not pr["merged"] or pr["base"]["ref"] != "main" or pr["head"]["ref"] != "build/1.4.1-fixes":
        raise RuntimeError("Expected the merged 1.4.1 candidate PR")
    candidate = pr["head"]["sha"]
    subprocess.run(["git", "merge-base", "--is-ancestor", candidate, "HEAD"], check=True)
    runs = api(f"repos/{REPO}/actions/runs?head_sha={candidate}&event=pull_request&per_page=100")["workflow_runs"]
    builds = sorted((r for r in runs if r["name"] == "Build"), key=lambda r: r["id"], reverse=True)
    for run in builds:
        jobs = api(f"repos/{REPO}/actions/runs/{run['id']}/jobs?per_page=100")["jobs"]
        if any(j["name"] == "Build the release ZIP" and j["conclusion"] == "success" for j in jobs):
            break
    else:
        raise RuntimeError("No successful PS5 package build for the exact candidate commit")
    feeds = [r for r in runs if r["name"] == "Account feed checks"]
    if not any(r["conclusion"] == "success" for r in feeds):
        raise RuntimeError("Account feed checks have not passed for the exact candidate")
    artifact = f"youtube-maxd-PR{PR}-{candidate[:7]}"
    artifacts = api(f"repos/{REPO}/actions/runs/{run['id']}/artifacts")["artifacts"]
    if not any(a["name"] == artifact and not a["expired"] for a in artifacts):
        raise RuntimeError("Verified build artifact is absent or expired")
    output = Path(os.environ["GITHUB_OUTPUT"])
    with output.open("a") as stream:
        stream.write(f"sha={candidate}\nrun={run['id']}\nartifact={artifact}\n")
    print(f"Selected candidate {candidate}, successful package run {run['id']}")


def publish(source, downloads, candidate, run):
    source, downloads = source.resolve(), downloads.resolve()
    if command("git", "rev-parse", "HEAD", cwd=source) != candidate:
        raise RuntimeError("Source checkout differs from the selected candidate")
    sys.path.insert(0, str(source / "scripts"))
    from bootstrap_local_voice import MODEL_SHA, MODEL_SIZE
    from release_version import identity
    from verify_app_zip import verify
    if identity() != ("PPSA99991", "01.004.001", "1.004.001"):
        raise RuntimeError("Source identity is not 1.4.1")
    original = downloads / "PPSA99991.zip"
    recorded = next(row.split()[0] for row in (downloads / "SHA256SUMS").read_text().splitlines()
                    if row.split()[-1] == "PPSA99991.zip")
    if sha(original) != recorded:
        raise RuntimeError("Downloaded package differs from Actions checksum")
    with zipfile.ZipFile(original) as archive:
        model = archive.read("PPSA99991/assets/voice/ggml-tiny.bin")
        if len(model) != MODEL_SIZE or hashlib.sha256(model).hexdigest() != MODEL_SHA:
            raise RuntimeError("Packaged voice model differs from the pinned complete model")
    dist = source / "dist"
    dist.mkdir(exist_ok=True)
    candidate_zip = dist / "PPSA99991.zip"
    shutil.copyfile(original, candidate_zip)
    # Only replace the text build label; retain the built executable and assets.
    subprocess.run([sys.executable, str(source / "scripts/zip_open_modes.py"), str(candidate_zip),
                    "--label", f"YouTube Max’d 1.4.1, {candidate}"], check=True)
    verify(candidate_zip)
    with zipfile.ZipFile(original) as before, zipfile.ZipFile(candidate_zip) as after:
        if set(before.namelist()) != set(after.namelist()):
            raise RuntimeError("Release relabel changed package inventory")
        for name in before.namelist():
            if name != "PPSA99991/build-label.txt" and before.read(name) != after.read(name):
                raise RuntimeError("Release relabel changed built bytes: " + name)
    subprocess.run([sys.executable, str(source / "scripts/package_maxd_release.py")], cwd=source, check=True)
    release = source.parent / "release"
    release.mkdir(exist_ok=True)
    for src, dest in ((candidate_zip, "PPSA99991.zip"),
                      (dist / "YouTube-Maxd-source.zip", "YouTube-Maxd-source.zip"),
                      (dist / "YouTube-Maxd-icon.png", "YouTube-Maxd-icon.png")):
        shutil.copyfile(src, release / dest)
    jobs = api(f"repos/{REPO}/actions/runs/{run}/jobs?per_page=100")["jobs"]
    checks = next((j for j in jobs if j["name"] == "Offline checks"), {})
    validation = {"version": "1.4.1", "content_version": "01.004.001", "title_id": "PPSA99991",
                  "source_commit": candidate, "build_run": f"https://github.com/{REPO}/actions/runs/{run}",
                  "ci_package_sha256": recorded, "release_package_sha256": sha(release / "PPSA99991.zip"),
                  "release_change": "build-label.txt only; all executable and asset bytes preserved",
                  "ps5_cross_build": "passed", "package_checks": "passed",
                  "voice_model_sha256": MODEL_SHA, "voice_model_bytes": MODEL_SIZE,
                  "full_offline_checks_status": checks.get("status"),
                  "full_offline_checks_conclusion": checks.get("conclusion"),
                  "ps5_hardware_tested": False}
    (release / "native-validation.json").write_text(json.dumps(validation, indent=2) + "\n")
    logs = source / "build/test-logs"
    focused = [(p.name, p.read_text(errors="replace")) for p in sorted(logs.glob("*.log"))] if logs.exists() else []
    report = ("# 1.4.1 validation\n\nPS5 cross-build and package verification pass. Targeted title/voice checks and account worker tests pass.\n\n"
              f"Exact build run: {validation['build_run']}\n\n"
              f"Full Offline checks: {checks.get('status')} / {checks.get('conclusion')}. This release does not claim an all-green inherited suite.\n\n"
              "The earlier candidate run 39892316978 reported 79 passed and 49 failed. Inspected failures include obsolete upstream wording/branding, old version literals, optional transforms masked by the existing fork, host checks requiring PS5 voice archives, and stale UI board expectations. Release identity/version literals and the existing playback voice-dispatch expectation were corrected before this build. Remaining suite failures are disclosed rather than suppressed.\n\n"
              "No PS5 hardware testing has been performed. Reporters should confirm Cyrillic playback titles, L3 capture/transcription, and account Home grouping.\n\n")
    for name, body in focused:
        report += f"## {name}\n\n```text\n{body}\n```\n\n"
    (release / "TEST_RESULTS.md").write_text(report)
    notes = (source / "docs/releases/01.004.001.md").read_text()
    (release / "RELEASE_NOTES.md").write_text(notes)
    assets = sorted(p for p in release.iterdir() if p.is_file() and p.name != "SHA256SUMS")
    (release / "SHA256SUMS").write_text("".join(f"{sha(p)}  {p.name}\n" for p in assets))
    existing = subprocess.run(["gh", "release", "view", TAG, "--repo", REPO, "--json", "isDraft,assets"],
                              capture_output=True, text=True)
    if existing.returncode == 0:
        current = json.loads(existing.stdout)
        if not current["isDraft"] or current["assets"]:
            raise RuntimeError("Refusing to replace an existing release or asset")
        raise RuntimeError("An empty draft already exists; inspect it before retrying")
    all_assets = sorted(str(p) for p in release.iterdir() if p.is_file())
    subprocess.run(["gh", "release", "create", TAG, "--repo", REPO, "--target", candidate,
                    "--draft", "--title", "YouTube Max’d 1.4.1", "--notes-file", str(release / "RELEASE_NOTES.md"),
                    *all_assets], check=True)
    remote = api(f"repos/{REPO}/releases/tags/{TAG}")
    uploaded = {a["name"]: a for a in remote["assets"]}
    for path in release.iterdir():
        if path.is_file() and uploaded.get(path.name, {}).get("digest") != "sha256:" + sha(path):
            raise RuntimeError("Uploaded asset digest mismatch: " + path.name)
    subprocess.run(["gh", "release", "edit", TAG, "--repo", REPO, "--draft=false", "--prerelease=false", "--latest"], check=True)
    print(f"Published https://github.com/{REPO}/releases/tag/{TAG}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("select", "publish"))
    parser.add_argument("--source", type=Path)
    parser.add_argument("--downloads", type=Path)
    parser.add_argument("--candidate")
    parser.add_argument("--run", type=int)
    args = parser.parse_args()
    if args.mode == "select":
        select()
    else:
        if not all((args.source, args.downloads, args.candidate, args.run)):
            parser.error("publish requires source, downloads, candidate and run")
        publish(args.source, args.downloads, args.candidate, args.run)
