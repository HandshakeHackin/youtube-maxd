"""Explicit public TV script/evaluator probe; no account, media or console traffic."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
from urllib.parse import urlsplit

import httpx
from yt_dlp import YoutubeDL
from yt_dlp.extractor.youtube.jsc.provider import (
    JsChallengeRequest, JsChallengeType, NChallengeInput, SigChallengeInput,
)

from prosperotube.oauth_video import TVOAuthYoutubeIE
from prosperotube.worker import Quiet


def public_text(url, user_agent):
    parsed = urlsplit(url)
    if (parsed.scheme != "https" or parsed.netloc != "www.youtube.com"
            or parsed.query or parsed.fragment or not (parsed.path == "/tv" or
                re.fullmatch(r"/s/player/[0-9a-fA-F]{8,}/tv-player[^?]+\.js", parsed.path))):
        raise ValueError("invalid public TV endpoint")
    with httpx.Client(timeout=12, follow_redirects=False, trust_env=False) as client:
        with client.stream("GET", url, headers={"User-Agent": user_agent}) as response:
            if response.status_code != 200:
                raise ValueError("public TV request failed")
            chunks, size = [], 0
            for chunk in response.iter_bytes():
                size += len(chunk)
                if size > 8 * 1024 * 1024:
                    raise ValueError("public TV response too large")
                chunks.append(chunk)
    return b"".join(chunks).decode("utf-8")


def characterize(node):
    report = {"result": "failed", "stage": "configuration", "account_authorized": False,
              "token_poll_requested": False, "bearer_used": False,
              "media_downloaded": False, "console_traffic": False,
              "scope": "public TV script with synthetic signature/n inputs"}
    try:
        with YoutubeDL({"quiet": True, "no_warnings": True, "logger": Quiet(),
                        "cachedir": False, "skip_download": True,
                        "js_runtimes": {"node": {"path": node}}}) as ydl:
            # The adapter's synthetic token is never used: only two public GETs
            # are permitted and the evaluator receives the downloaded script.
            ie = TVOAuthYoutubeIE("unused-public-probe-token")
            ydl.add_info_extractor(ie)
            agent = ie._get_default_ytcfg("tv")["INNERTUBE_CONTEXT"]["client"]["userAgent"]
            config = ie.extract_ytcfg("public-probe", public_text("https://www.youtube.com/tv", agent))
            player = ie._extract_player_url(config)
            if not player:
                report["code"] = "tv_script_missing"
                return report
            report["stage"] = "script"
            code = public_text(player, agent)
            report.update(script_bytes=len(code.encode("utf-8")),
                          script_sha256=hashlib.sha256(code.encode("utf-8")).hexdigest(),
                          script_variant=Path(urlsplit(player).path).name)
            ie._load_player = lambda *args, **kwargs: code
            ie.initialize()
            report["timestamp_found"] = ie._extract_signature_timestamp("public-probe", player) is not None
            report["stage"] = "evaluator"
            signature = "".join(chr(33 + i) for i in range(86))
            nvalue = "synthetic_n_probe_value"
            requests = [JsChallengeRequest(JsChallengeType.SIG, SigChallengeInput(player, [signature])),
                        JsChallengeRequest(JsChallengeType.N, NChallengeInput(player, [nvalue]))]
            responses = ie._jsc_director.bulk_solve(requests)
            solved = {request.type: response.output.results for request, response in responses}
            sig = solved.get(JsChallengeType.SIG, {}).get(signature)
            n = solved.get(JsChallengeType.N, {}).get(nvalue)
            report.update(signature_solved=isinstance(sig, str) and 0 < len(sig) <= len(signature)
                          and set(sig).issubset(set(signature)),
                          n_solved=isinstance(n, str) and 0 < len(n) <= 8192 and n != nvalue,
                          solved_requests=len(responses))
            if report["timestamp_found"] and report["signature_solved"] and report["n_solved"]:
                report.update(result="pass", stage="complete")
            else:
                report["code"] = "tv_evaluator_incomplete"
    except Exception:
        report["code"] = "tv_signature_probe_failed"
    return report


def main():
    if sys.argv[1:] == ["--worker"]:
        request = json.load(sys.stdin)
        with contextlib.redirect_stdout(sys.stderr):
            result = characterize(request["node"])
        json.dump(result, sys.stdout)
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.name != "posix":
        parser.error("Run this probe in WSL/Linux for bounded process-group cleanup.")
    if not args.node.is_file():
        parser.error("Choose an installed Node.js runtime.")
    process = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), "--worker"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, start_new_session=True)
    try:
        output, _ = process.communicate(json.dumps({"node": str(args.node.resolve())}).encode(), timeout=90)
        result = json.loads(output) if process.returncode == 0 and len(output) <= 16384 else None
        if not isinstance(result, dict):
            raise ValueError()
    except (subprocess.TimeoutExpired, ValueError, UnicodeError):
        result = {"result": "failed", "code": "tv_signature_worker_failed",
                  "account_authorized": False, "console_traffic": False}
    finally:
        # Retire every evaluator descendant even if the worker timed out or
        # exited before its Node process. Never signal the caller's group.
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        process.communicate()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))
    return 0 if result["result"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
