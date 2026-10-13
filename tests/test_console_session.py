"""Offline checks of tools/console: parsers, postconditions, the lock, the
candidate check and the session's control flow against the in-memory console.
Nothing here may open a socket: the fixture below makes that an error."""
import hashlib
import json
import socket
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/console"))
import observe as ob  # noqa: E402
import session as se  # noqa: E402
import sim  # noqa: E402
from transport import ConsoleDown  # noqa: E402

BUILD = "prosperotube-0123abcd_1011-2201"
STATUS = (f"build={BUILD} t=12 scr=2 be=1 pos=3.50 dur=19.0 fps=15.0 fatal=0 eof=0 complete=0 "
          "active=1 parked=0\n")


@pytest.fixture(autouse=True)
def no_network(monkeypatch):
    def refuse(*_args, **_kwargs):
        raise AssertionError("a test tried to open a socket")
    monkeypatch.setattr(socket, "socket", refuse)
    monkeypatch.setattr(socket, "create_connection", refuse)


def status(**changes):
    return {**ob.parse_status(STATUS), **changes}


def ui(doc="hui_provider", rows=("Home", "History"), at=0, screen=23, paused=None, seq=1):
    focus = {"id": f"row-{at}", "text": rows[at], "index": at + 1, "total": len(rows),
             "focusable": [{"id": f"row-{i}", "text": text} for i, text in enumerate(rows)]}
    player = None if paused is None else {"osd_visible": True, "paused": paused, "active_overlay": None}
    return {"seq": seq, "t_ms": 1, "ui": {"screen": {"id": screen, "name": "x"}, "docs": [doc],
                                         "focused": focus, "modal": None, "toast": None, "native_player": player}}


# --- parsers ---------------------------------------------------------------------

def test_status_line_is_parsed_with_types():
    parsed = ob.parse_status(STATUS.encode())
    assert parsed["build"] == BUILD and parsed["t"] == 12 and parsed["pos"] == 3.5
    assert parsed["complete"] == 0 and parsed["parked"] == 0 and parsed["active"] == 1


@pytest.mark.parametrize("bad", [b"", None, STATUS.rstrip("\n"), STATUS.replace(" complete=0", ""),
                                 STATUS.replace("t=12", "t=x"), STATUS + STATUS, "garbage\n"])
def test_incomplete_or_foreign_status_is_rejected(bad):
    assert ob.parse_status(bad) is None


def test_ui_snapshot_helpers():
    snapshot = ob.parse_ui(json.dumps(ui(rows=("Home", "Me at the zoo"), at=1)).encode())
    assert ob.ui_doc(snapshot) == "hui_provider" and ob.ui_screen(snapshot) == 23
    assert ob.ui_rows(snapshot) == ["Home", "Me at the zoo"] and ob.find_row(snapshot, "me AT the") == 2
    assert ob.find_row(snapshot, "absent") == 0
    assert ob.parse_ui(b"{\"seq\":1") is None and ob.parse_ui(b"{\"ui\":{}}") is None and ob.parse_ui(None) is None
    assert ob.ui_doc(None) == "" and ob.ui_rows(None) == [] and "docs=-" in ob.ui_line(None)


def test_log_statistics_and_storage_verdict():
    log = ("x stats frames_in=10 converted=10 published=9 late_drop=1 seek_disc=0\n"
           "x stats frames_in=284 converted=284 published=284 late_drop=3 seek_disc=2\n"
           "x stats seek_req=1 ok=1 fail=0 clock_resets=0 seek_to_first_ms=338\n")
    stats = ob.parse_stats(log)
    assert stats["published"] == 284 and stats["late_drop"] == 3 and stats["seek_ok"] == 1
    assert ob.pc_stats(stats)[0] and "3 late drops" in ob.pc_stats(stats)[1]
    assert not ob.pc_stats(None)[0] and not ob.pc_stats({**stats, "published": 0})[0]
    assert "ABOVE 5%" in ob.pc_stats({**stats, "late_drop": 30})[1]
    assert ob.parse_stats("nothing") is None
    assert ob.storage_verdict("Lapy: path=/x status=0 storage=ready\n") == "ready"
    assert ob.storage_verdict("Lapy: path=/x status=0 storage=failed\n") == "failed"
    assert ob.storage_verdict("Lapy: path=/x status=3; durable storage unavailable\n") == "failed"
    assert ob.storage_verdict("frame 1\n") is None


# --- postconditions -----------------------------------------------------------------

def test_heartbeat_needs_the_candidate_build_and_a_moving_clock():
    assert ob.pc_heartbeat(status(t=1), status(t=2), BUILD)[0]
    assert not ob.pc_heartbeat(status(t=2), status(t=2), BUILD)[0]
    assert not ob.pc_heartbeat(None, status(t=2), BUILD)[0]
    assert not ob.pc_heartbeat(status(t=1), status(t=2), "prosperotube-ffffffff_0101-0000")[0]
    assert not ob.pc_heartbeat(status(t=1), status(t=2, parked=1), BUILD)[0]
    assert not ob.pc_heartbeat(status(t=1), None, BUILD)[0]


def test_playing_needs_progress_and_no_fatal():
    assert ob.pc_playing([status(t=1, pos=1.0), status(t=2, pos=2.0)])[0]
    assert not ob.pc_playing([status(t=1, pos=1.0)])[0]
    assert not ob.pc_playing([status(t=1, pos=1.0), status(t=2, pos=1.0)])[0]
    assert not ob.pc_playing([status(t=1, pos=1.0), status(t=2, pos=2.0, fatal=1)])[0]
    assert not ob.pc_playing([status(t=1, pos=1.0, active=0), status(t=2, pos=2.0, scr=23)])[0]
    assert not ob.pc_playing([None, None])[0]


def test_paused_needs_a_still_position_and_the_flag_or_a_third_sample():
    held = [status(t=5, pos=4.0), status(t=6, pos=4.0)]
    assert ob.pc_paused(held + [status(t=7, pos=4.0)], None)[0]  # snapshot silent: three still samples
    assert not ob.pc_paused(held + [status(t=7, pos=4.0, active=0)], None)[0]
    assert ob.pc_paused(held, ui("hui_osd", ("clip",), screen=2, paused=True))[0]
    assert not ob.pc_paused(held, ui("hui_osd", ("clip",), screen=2, paused=False))[0]
    assert not ob.pc_paused([status(t=5, pos=4.0), status(t=6, pos=5.0)], ui("hui_osd", ("c",), paused=True))[0]
    assert not ob.pc_paused(held[:1], ui("hui_osd", ("clip",), paused=True))[0]
    assert not ob.pc_paused([held[0], held[0]], ui("hui_osd", ("clip",), paused=True))[0]


def test_seek_lands_near_the_target_and_respects_the_duration():
    assert ob.pc_seek(4.0, status(pos=14.2), 10)[0]
    assert not ob.pc_seek(4.0, status(pos=5.0), 10)[0]
    assert not ob.pc_seek(4.0, status(pos=14.2, fatal=1), 10)[0]
    assert ob.pc_seek(12.0, status(pos=18.9, dur=19.0), 10)[0]
    assert not ob.pc_seek(4.0, status(pos=30.0, dur=600.0), 10)[0]
    assert not ob.pc_seek(4.0, None, 10)[0]


def test_completion_dialog_rows_quality_and_captions():
    assert ob.pc_complete(status(eof=1, complete=1))[0]
    assert not ob.pc_complete(status(eof=1, complete=0))[0]  # demux exhausted is not the end
    assert not ob.pc_complete(status(complete=1, fatal=1))[0]
    dialog = ui("hui_dialog", ("KEEP WATCHING", "STOP"), at=1, screen=20)
    assert ob.pc_dialog(dialog, "stop")[0] and not ob.pc_dialog(dialog, "KEEP WATCHING")[0]
    assert not ob.pc_dialog(ui("hui_osd", ("STOP",)), "STOP")[0]
    assert ob.pc_rows(ui(rows=("a", "Me at the zoo")), "zoo")[0]
    assert not ob.pc_rows(ui("hui_keyboard", ("Me at the zoo",)), "zoo")[0]
    assert ob.pc_browser(ui())[0] and not ob.pc_browser(ui("hui_osd", screen=2))[0]
    assert ob.pc_doc(ui("hui_keyboard"), "hui_keyboard")[0] and not ob.pc_doc(ui(seq=3), "hui_provider", after_seq=3)[0]
    assert ob.quality_rows(["Video presets: Custom limits", "English", "360p", "Seek interval: 10 seconds"]) == \
        ["Video presets: Custom limits", "360p"]
    offered, current = ob.caption_tracks(["Off", "English (current) [PRIMARY]", "French"])
    assert offered == ["English (current) [PRIMARY]", "French"] and current == ["English (current) [PRIMARY]"]


def test_service_line_is_read_from_the_log_and_never_with_a_token():
    assert ob.pc_service("boot\nembedded service: listening on 127.0.0.1:23456\nframe loop") == (True, "listening on 127.0.0.1:23456")
    assert not ob.pc_service("boot\nframe loop")[0]
    assert not ob.pc_service("embedded service: listening on 127.0.0.1:8765")[0]      # outside the console's range
    assert not ob.pc_service("embedded service: listening on 127.0.0.1:49152")[0]
    refused = ob.pc_service("embedded service: did not start: no free port")
    assert not refused[0] and "no free port" in refused[1] and ob.service_failed("x\nembedded service: did not start: y")
    assert not ob.service_failed("embedded service: listening on 127.0.0.1:23456")
    leaked = ob.pc_service("embedded service: listening on 127.0.0.1:23456 token " + "ab" * 32)
    assert not leaked[0] and "token" in leaked[1]
    # a later start replaces an earlier failure
    assert ob.pc_service("embedded service: did not start: x\nembedded service: listening on 127.0.0.1:20000")[0]


def test_saved_state_history_and_redaction():
    state = json.dumps({"source": "http://pc.example:8765", "token": "s" * 43, "records": [
        {"id": "simVideo0001", "played": True, "position": 0, "duration": 19, "title": "Me at the zoo"},
        {"id": "other", "played": False, "position": 7}]}).encode()
    assert ob.pc_state_local(state)[0] and ob.pc_state_local(b"{}")[0]      # an old companion address may stay stored
    assert not ob.pc_state_local(json.dumps({"source": "http://127.0.0.1:23456", "token": "t" * 64}).encode())[0]
    assert not ob.pc_state_local(b"not json")[0]
    assert ob.pc_history(state, "simVideo0001", True)[0] and ob.pc_history(state, "other", False)[0]
    assert not ob.pc_history(state, "other", True)[0] and not ob.pc_history(state, "missing", False)[0]
    shown = json.dumps(ob.redact_state(json.dumps({"source": "http://pc.example:8765#123456", "token": "s" * 43}).encode()))
    assert "123456" not in shown and "sss" not in shown and "<present>" in shown


# --- lock ---------------------------------------------------------------------------

def test_lock_is_create_new_and_only_our_own_is_released(tmp_path):
    lock = tmp_path / "lock-xx.txt"
    ob.lock_acquire(lock, "ProsperoTube")
    assert lock.read_bytes() == b"ProsperoTube"
    with pytest.raises(ob.LockBusy) as busy:
        ob.lock_acquire(lock, "ProsperoTube")
    assert "ProsperoTube" in str(busy.value)
    assert ob.lock_release(lock, "ProsperoTube") and not lock.exists()
    assert not ob.lock_release(lock, "ProsperoTube")  # already gone
    lock.write_text("another agent\n")
    assert not ob.lock_release(lock, "ProsperoTube") and lock.read_text() == "another agent\n"
    with pytest.raises(ob.LockBusy) as busy:
        ob.lock_acquire(lock, "ProsperoTube")
    assert str(busy.value) == "another agent" and ob.lock_holder(lock) == "another agent"
    lock.write_text("ProsperoTube\r\n")  # an editor's line ending does not make it foreign
    assert ob.lock_release(lock, "ProsperoTube")


# --- candidate ------------------------------------------------------------------------

def make_candidate(root, layout="protocol", instrumented=True):
    folder = root / ob.TITLE
    (folder / "sce_sys").mkdir(parents=True)
    marks = b" dev_command dev_status " if instrumented else b""
    (folder / "eboot.bin").write_bytes(b"\x4f\x15\x3d\x1d" + BUILD.encode() + marks)
    (folder / "sce_sys/param.json").write_text("{}")
    (folder / "assets").mkdir()
    (folder / "assets/a.txt").write_text("a")
    lines = []
    for item in sorted(p for p in folder.rglob("*") if p.is_file()):
        name = item.relative_to(folder).as_posix()
        name = f"./{name}" if layout == "protocol" else f"{ob.TITLE}/{name}"
        lines.append(f"{hashlib.sha256(item.read_bytes()).hexdigest()}  {name}")
    if layout != "protocol":
        (root / f"{ob.TITLE}.zip").write_bytes(b"zip")
        lines.append(f"{hashlib.sha256(b'zip').hexdigest()}  {ob.TITLE}.zip")
    (root / "SHA256SUMS").write_text("\n".join(lines) + "\n")
    return root


@pytest.mark.parametrize("layout", ["protocol", "release"])
def test_candidate_matches_its_manifest_in_both_layouts(tmp_path, layout):
    info = ob.verify_candidate(make_candidate(tmp_path, layout))
    assert info["files"] == 3 and info["build"] == BUILD and info["instrumented"]


def test_candidate_changes_are_refused(tmp_path):
    root = make_candidate(tmp_path)
    (root / ob.TITLE / "assets/a.txt").write_text("b")
    with pytest.raises(ob.CandidateError, match="differs"):
        ob.verify_candidate(root)
    (root / ob.TITLE / "assets/a.txt").write_text("a")
    (root / ob.TITLE / "extra.bin").write_text("x")
    with pytest.raises(ob.CandidateError, match="not in SHA256SUMS"):
        ob.verify_candidate(root)
    (root / ob.TITLE / "extra.bin").unlink()
    (root / ob.TITLE / "assets/a.txt").unlink()
    with pytest.raises(ob.CandidateError, match="absent"):
        ob.verify_candidate(root)
    (root / "SHA256SUMS").unlink()
    with pytest.raises(ob.CandidateError, match="missing"):
        ob.verify_candidate(root)


# --- the session against the in-memory console ---------------------------------------------

BASE = "http://pc.example:8765"


def run(tmp_path, monkeypatch, extra=(), instrumented=True, lock_text=None, **scenario):
    candidate = make_candidate(tmp_path / "candidate", instrumented=instrumented)
    protocol = tmp_path / "protocol/scripts"
    protocol.mkdir(parents=True)
    for tool in ("ps5-console.py", "send-controller.sh"):
        (protocol / tool).write_text("")
    lock = tmp_path / "lock-xx.txt"
    if lock_text is not None:
        lock.write_text(lock_text)
    out = tmp_path / "out"
    out.mkdir()
    monkeypatch.setattr(se.shutil, "which", lambda name: f"/usr/bin/{name}")
    monkeypatch.setattr(se.subprocess, "run", lambda *a, **k: subprocess.CompletedProcess(a, 0, "built.elf\n", ""))
    args = se.parse(["--i-have-owner-approval", "--host", "console.invalid", "--lock-file", str(lock),
                     "--candidate", str(candidate),
                     "--protocol", str(tmp_path / "protocol"), "--out", str(out), *extra])
    clock = sim.SimClock()
    console = sim.SimConsole(clock, echo=lambda line: None, evidence=out, **scenario)
    current = se.Session(args, console, clock, ConsoleDown)
    verdict = current.run()
    ops = [json.loads(line) for line in (out / "ops.jsonl").read_text().splitlines()] if (out / "ops.jsonl").exists() else []
    return verdict, current, console, ops, lock, out


def payloads(ops):
    return [op["detail"].split()[0] for op in ops if op["kind"] == "payload"]


def test_refuses_without_the_owner_approval_flag(capsys):
    with pytest.raises(SystemExit) as refused:
        se.parse(["--host", "console.invalid", "--lock-file", "x", "--candidate", "c"])
    assert refused.value.code == 2 and "owner's approval" in capsys.readouterr().err


def test_full_session_passes_with_one_launch_and_one_close(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = run(tmp_path, monkeypatch)
    assert verdict == "pass", (out / "session.log").read_text()
    assert [stage["result"] for stage in current.stages] == ["PASS"] * 7
    assert payloads(ops) == ["launch", "close"]
    assert ops[0]["detail"].startswith("/mnt/sandbox") and not ops[0]["writes"]  # the idle check comes first
    kinds = [op["kind"] for op in ops]
    assert kinds.index("ftp-install") < kinds.index("ftp-settle") < kinds.index("payload")
    launch = kinds.index("payload")
    assert "/mnt/sandbox" in [op["detail"] for op in ops[:launch] if op["kind"] == "ftp-list"][-2]  # rechecked after the install
    assert not lock.exists() and current.teardown_clean is True
    result = json.loads((out / "result.json").read_text())
    assert result["launches"] == 1 and result["observations"]["history"]["position"] > 0
    assert result["observations"]["stats"]["published"] > 0 and result["observations"]["options"]["quality"]
    assert result["observations"]["video"]["id"] == "simVideo0001" and result["observations"]["soft_misses"] == []
    assert "search:science" in result["observations"]["service_answer"] and current.obs["stopped_by"] == "confirmation dialog"
    sent = [op["detail"] for op in ops if "command:" in op["detail"]]
    assert not [line for line in sent if "command: type" in line or "square" in line]  # no keyboard on the first pass
    assert b"activate 'app:exit'" not in console.files[ob.LOG_FILE]
    assert (out / "playback-scanout.bmp").read_bytes()[:2] == b"BM"
    assert not any(name.startswith(("dev_", "evo_shot")) for name in (console.listdir(ob.DATA) or {}))
    assert ob.STATE_FILE not in console.files  # the console had none: the session's own is removed
    assert result["observations"]["service"] == "listening on 127.0.0.1:26757"
    assert not [op for op in ops if "options" in op["detail"]]  # nothing is typed into Options: there is no pairing
    with pytest.raises(SystemExit):  # the companion's arguments are gone
        se.parse(["--i-have-owner-approval", "--host", "h", "--lock-file", "x", "--candidate", "c", "--companion", "http://pc:8765"])


def test_service_that_did_not_start_fails_before_any_search_and_closes_cleanly(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, service="no free port between 20000 and 40000")
    assert verdict == "fail" and payloads(ops) == ["launch", "close"] and not lock.exists()
    assert current.teardown_clean is True and console.running == []
    assert not [op for op in ops if "command: type" in op["detail"]]
    assert any("did not start" in step["detail"] for step in current.steps if step["result"] == "FAIL")


def test_busy_console_stops_after_the_first_read_only_operation(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = run(tmp_path, monkeypatch, busy=["PPSA01234"])
    assert verdict == "no-run" and len(ops) == 1 and ops[0]["kind"] == "ftp-list" and not ops[0]["writes"]
    assert not lock.exists() and payloads(ops) == []


def test_our_own_title_already_running_also_blocks(tmp_path, monkeypatch):
    verdict, _, _, ops, lock, _ = run(tmp_path, monkeypatch, busy=[ob.TITLE])
    assert verdict == "no-run" and len(ops) == 1 and not lock.exists()


def test_held_lock_means_no_console_operation_and_the_lock_is_untouched(tmp_path, monkeypatch):
    verdict, _, _, ops, lock, _ = run(tmp_path, monkeypatch, lock_text="another agent")
    assert verdict == "no-run" and ops == [] and lock.read_text() == "another agent"


def test_title_started_by_someone_before_the_launch_means_no_launch(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, busy_before_launch=["PPSA01234"])
    assert verdict == "no-run" and payloads(ops) == [] and current.launched is False
    assert ops[-1]["detail"].startswith("/mnt/sandbox")  # the recheck is the last console operation
    assert not lock.exists()  # nothing of ours is running: the console is theirs, the lock is given back


def test_uninstrumented_candidate_is_refused_before_any_console_operation(tmp_path, monkeypatch):
    verdict, _, _, ops, lock, _ = run(tmp_path, monkeypatch, instrumented=False)
    assert verdict == "no-run" and ops == [] and not lock.exists()


def test_app_that_does_not_park_is_not_force_closed_and_the_lock_stays(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, parks=False)
    assert verdict == "unresolved" and payloads(ops) == ["launch"]
    assert console.running == [ob.TITLE] and lock.read_text() == "ProsperoTube"
    assert not any(op["kind"] in ("ftp-delete", "ftp-put") and "dev_command" not in op["detail"]
                   for op in ops[[o["kind"] for o in ops].index("payload"):])  # nothing restored under a running app


def test_forced_close_only_with_the_explicit_flag(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, extra=["--allow-unparked-close"], parks=False)
    assert payloads(ops) == ["launch", "close"] and current.obs["forced_close"] is True
    assert verdict == "pass" and not lock.exists()


def test_close_that_does_not_release_is_never_repeated(tmp_path, monkeypatch):
    verdict, _, console, ops, lock, _ = run(tmp_path, monkeypatch, closes=False)
    assert verdict == "unresolved" and payloads(ops) == ["launch", "close"] and lock.exists()


def test_failed_storage_stops_without_input_and_without_a_close(tmp_path, monkeypatch):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, storage="failed")
    assert verdict == "fail" and payloads(ops) == ["launch"]
    assert not any(op["kind"] == "ftp-put" for op in ops) and not lock.exists()
    assert current.stages[4]["result"] == "FAIL"


@pytest.mark.parametrize("after", [3, 30, 80, 110])
def test_console_that_stops_answering_ends_everything_and_keeps_the_lock(tmp_path, monkeypatch, after):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, down_after=after)
    assert verdict == "unresolved" and len(ops) == after and lock.read_text() == "ProsperoTube"
    assert payloads(ops).count("launch") <= 1 and payloads(ops).count("close") <= 1


def test_console_silent_at_the_first_operation_is_a_no_run_with_the_lock_given_back(tmp_path, monkeypatch):
    verdict, _, _, ops, lock, _ = run(tmp_path, monkeypatch, down_after=0)
    assert verdict == "no-run" and ops == [] and not lock.exists()


def test_original_provider_state_is_restored_byte_for_byte(tmp_path, monkeypatch):
    original = json.dumps({"source": "http://old.example:8765", "token": "o" * 43, "records": []}).encode() + b"\n"
    verdict, current, console, _, _, out = run(tmp_path, monkeypatch, state=original)
    assert verdict == "pass" and console.files[ob.STATE_FILE] == original
    assert current.obs["restore"]["restored"] == ["youtube.json"] and current.obs["restore"]["failed"] == []
    assert (out / "state-before/youtube.json").read_bytes() == original
    assert json.loads((out / "state-after/youtube.json").read_text())["records"][0]["id"] == "simVideo0001"


def test_keep_state_leaves_the_session_history_and_still_removes_development_files(tmp_path, monkeypatch):
    original = json.dumps({"source": "http://old.example:8765", "token": "o" * 43}).encode()
    verdict, _, console, _, _, _ = run(tmp_path, monkeypatch, extra=["--keep-state"], state=original)
    kept = json.loads(console.files[ob.STATE_FILE])
    assert verdict == "pass" and kept["source"] == "http://old.example:8765" and kept["records"][0]["id"] == "simVideo0001"
    assert not any(Path(path).name in ob.DEV_FILES or ob.SHOT.match(Path(path).name) for path in console.files)


def test_stop_with_confirmation_path(tmp_path, monkeypatch):
    verdict, current, _, ops, _, _ = run(tmp_path, monkeypatch, extra=["--end", "stop"], duration=600.0)
    assert verdict == "pass" and current.obs["end_mode"] == "stop" and payloads(ops) == ["launch", "close"]
    assert any(step["step"].startswith("(g) stop confirmation open") and step["result"] == "PASS" for step in current.steps)


def test_topic_missing_from_the_list_fails_cleanly_without_pressing_cross_on_another_row(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, extra=["--topic-row", "No such topic"])
    assert verdict == "fail" and payloads(ops) == ["launch", "close"] and not lock.exists()
    assert current.teardown_clean is True and console.running == []


# --- what the first console session captured (2026-10-13): fixtures --------------------------

REAL_BROWSER = (
    '{"seq": 2, "t_ms": 168464144, "ui": {"screen": {"id": 23, "name": "EmbyBrowse"}, "docs": ["youtube"], '
    '"focused": {"id": "", "tag": "div", "class": "tube-card folder", "text": "D Discover Search public videos by topic", '
    '"doc": "youtube", "index": 1, "total": 8, "rect": {"x": 128, "y": 288, "w": 864, "h": 664}, "focusable": ['
    '{"id": "", "text": "D Discover Search public videos by topic"}, '
    '{"id": "", "text": "C Continue watching & history Stored on this PS5"}, '
    '{"id": "", "text": "F Favorites Your local saved videos"}, '
    '{"id": "", "text": "P Play queue R2 on a video to add or remove"}, '
    '{"id": "", "text": "F Followed channels Local follows, independent o..."}, '
    '{"id": "", "text": "Y YouTube preferences Quality, codec, captions a..."}, '
    '{"id": "", "text": "P Player settings Display, audio and controller ..."}, '
    '{"id": "", "text": "C Close ProsperoTube Release playback, then clos..."}]}, '
    '"also_highlighted": [], "modal": null, "toast": null, "native_player": null}}')
REAL_STATUS = ("build=prosperotube-6b3b1c41_1013-1226 t=168465 scr=23 be=0 pos=0.00 dur=0.0 fps=60.0 "
               "fatal=0 eof=0 complete=0 active=0 parked=0\n")
REAL_LOG = """[168463.593] INFO  EVO boot: Lapy: path=helper status=0 storage=ready
[168463.993] INFO  embedded service: listening on 127.0.0.1:26757
[168464.013] INFO  EVO boot: provider: RequestPage parent='' page=0
[168464.018] INFO  EVO boot: provider: 'youtube' catalog parent='' -> 8 rows (folder_level=1 has_more=0 total=8)
[168464.032] INFO  EVO boot: Application: entering frame loop
[168555.236] INFO  bc REMOTE_CMD quit
[168555.240] INFO  stats frames_in=0 converted=0 published=0 late_drop=0 seek_disc=0
"""


def test_the_browser_the_console_showed_is_recognised():
    snapshot = ob.parse_ui(REAL_BROWSER.encode())
    ok, detail = ob.pc_browser(snapshot)
    assert ok and detail.startswith("row 1 of 8")  # this exact snapshot failed the first session's gate
    assert ob.ui_docs(snapshot) == ["youtube"] and ob.ui_focus(snapshot)["id"] == ""
    assert ob.find_row(snapshot, r"\bDiscover\b") == 1 and ob.find_row(snapshot, "Close ProsperoTube") == 8
    assert ob.find_row(snapshot, r"\bScience\b") == 0 and ob.focus_kind(snapshot) == "folder"
    assert not ob.keyboard_open(snapshot) and "tube-card folder" in ob.ui_line(snapshot)
    previous, current = ob.parse_status(REAL_STATUS.replace("t=168465", "t=168464")), ob.parse_status(REAL_STATUS)
    assert current["scr"] == ob.SCREEN_BROWSER and ob.pc_heartbeat(previous, current, "prosperotube-6b3b1c41_1013-1226")[0]


def test_browser_needs_a_cursor_on_a_row_in_either_layout():
    snapshot = json.loads(REAL_BROWSER)
    kit = ui("hui_provider", ("Discover", "Favorites"))
    assert ob.pc_browser(kit)[0] and ob.focus_kind(kit) is None
    snapshot["ui"]["focused"] = None
    assert not ob.pc_browser(snapshot)[0]  # the list is loading: no cursor yet
    assert not ob.pc_browser(None)[0] and not ob.pc_browser(ui("hui_osd", ("clip",), screen=2))[0]
    playable = json.loads(REAL_BROWSER)
    playable["ui"]["focused"]["class"] = "tube-card"
    assert ob.focus_kind(playable) == "playable"


def test_keyboard_is_seen_as_a_modal_or_as_a_kit_document():
    rml = json.loads(REAL_BROWSER)
    rml["ui"]["docs"] = ["youtube", "keyboard"]
    rml["ui"]["modal"] = {"kind": "keyboard", "text": "Search YouTube videos... tides 5 / 800"}
    assert ob.keyboard_open(rml) and "tides" in ob.ui_modal(rml)["text"]
    assert not ob.pc_rows(rml, "Discover")[0]  # rows under a keyboard are not a result list
    assert ob.keyboard_open(ui("hui_keyboard", ("q",))) and not ob.keyboard_open(ui())
    rml["ui"]["modal"] = {"kind": "dialog", "text": "x"}
    rml["ui"]["docs"] = ["youtube"]
    assert not ob.keyboard_open(rml)


def test_provider_log_lines_prove_lists_activations_and_searches():
    assert ob.catalog_lines(REAL_LOG) == [{"parent": "", "rows": 8, "folders": True, "more": False}]
    assert ob.pc_service(REAL_LOG) == (True, "listening on 127.0.0.1:26757") and ob.storage_verdict(REAL_LOG) == "ready"
    assert not ob.pc_catalog(REAL_LOG, 1)[0] and not ob.pc_catalog(REAL_LOG, 0, network=True)[0]
    more = REAL_LOG + ("[1.0] INFO  EVO boot: provider: 'youtube' activate 'search:science' (folder) title='Science'\n"
                       "[1.1] INFO  EVO boot: provider: 'youtube' catalog parent='search:science' -> 20 rows (folder_level=0 has_more=1 total=20)\n"
                       "[1.2] INFO  EVO boot: provider: 'youtube' activate 'abc_DEF-123' (playable) title='It's a test'\n"
                       "[1.3] INFO  EVO boot: provider: Search query='me at the zoo'\n")
    ok, detail = ob.pc_catalog(more, 1, network=True)
    assert ok and "search:science" in detail and "20 rows (videos)" in detail
    assert ob.activations(more)[-1] == {"id": "abc_DEF-123", "kind": "playable", "title": "It's a test"}
    assert ob.searches(more) == ["me at the zoo"] and ob.resolve_failed(more) is None
    empty = more + "[2.0] INFO  EVO boot: provider: 'youtube' catalog parent='search:x' -> 0 rows (folder_level=0 has_more=0 total=0)\n"
    assert not ob.pc_catalog(empty, 1, network=True)[0]
    assert "youtube/abc" in ob.resolve_failed(more + "[3.0] INFO  EVO boot: provider: resolve failed for youtube/abc\n")


def test_cursor_moves_sideways_first_and_by_rows_once_the_grid_is_known():
    assert ob.next_key(1, 3, None, set()) == "right" and ob.next_key(3, 1, None, set()) == "left"
    assert ob.next_key(4, 6, None, {"right"}) == "down" and ob.next_key(4, 6, None, {"right", "down"}) is None
    assert ob.next_key(8, 6, 4, set()) == "left" and ob.next_key(2, 7, 4, set()) == "down"
    assert ob.next_key(7, 2, 4, set()) == "up" and ob.next_key(5, 6, 4, {"right"}) is None


def test_history_is_matched_by_id_with_or_without_a_prefix_or_by_title():
    state = json.dumps({"records": [{"id": "video:abc", "title": "Tides", "played": False, "position": 12}]}).encode()
    assert ob.pc_history(state, "abc", False)[0] and ob.pc_history(state, "video:abc", False)[0]
    assert ob.pc_history(state, "zzz", False, title="Tides")[0] and not ob.pc_history(state, "zzz", False, title="Other")[0]
    assert not ob.pc_history(state, "", False)[0] and not ob.pc_history(state, "abc", True)[0]


# --- the route and the soft steps against the in-memory console ----------------------------------

def commands(ops):
    return [op["detail"].split("command: ", 1)[1].split(";", 1)[0] for op in ops if "command: " in op["detail"]]


def test_route_is_discover_then_a_topic_then_the_first_video(tmp_path, monkeypatch):
    verdict, current, console, ops, _, out = run(tmp_path, monkeypatch)
    assert verdict == "pass"
    assert commands(ops)[:6] == ["key cross", "key right", "key right", "key cross", "key cross", "seek +10"]
    first = json.loads((out / "ui.jsonl").read_text().splitlines()[0])
    real = json.loads(REAL_BROWSER)
    assert first["ui"]["docs"] == real["ui"]["docs"] and first["ui"]["screen"] == real["ui"]["screen"]
    assert first["ui"]["focused"] == real["ui"]["focused"]  # the simulator draws what the console drew
    hard = [step["step"] for step in current.steps if step["result"] == "PASS"]
    for needed in ("(c) topic: the provider listed what the row opens", "(d) the provider activated a playable row",
                   "(d) player open, decoder active", "(d) position advancing", "(e) +10 s seek landed",
                   "(d,g) engine statistics for the playback", "(g) history record for simVideo0001"):
        assert needed in hard


def test_kit_layout_of_the_browser_is_driven_the_same_way(tmp_path, monkeypatch):
    verdict, current, _, ops, _, _ = run(tmp_path, monkeypatch, layout="kit")
    assert verdict == "pass" and current.obs["video"]["id"] == "simVideo0001" and payloads(ops) == ["launch", "close"]


def test_topic_that_lists_nothing_is_a_hard_failure_with_a_clean_close(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = run(tmp_path, monkeypatch, topic_rows=0)
    assert verdict == "fail" and payloads(ops) == ["launch", "close"] and not lock.exists()
    assert current.teardown_clean is True and "returned no rows" in current.stages[5]["detail"]
    assert commands(ops).count("key cross") == 2  # nothing is activated on an empty list


def test_stream_that_does_not_resolve_fails_at_once(tmp_path, monkeypatch):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, resolves=False)
    assert verdict == "fail" and "did not resolve" in current.stages[5]["detail"] and not lock.exists()
    assert "seek +10" not in commands(ops)


@pytest.mark.parametrize("scenario, missed", [
    ({"captions_open": False}, "(e) caption picker"),
    ({"exit_dialog": False}, "(g) stop with confirmation"),
    ({"first_cross_shows_controls": False}, None),
])
def test_interface_surprises_are_recorded_and_do_not_stop_the_session(tmp_path, monkeypatch, scenario, missed):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, **scenario)
    assert verdict == "pass" and not lock.exists() and payloads(ops) == ["launch", "close"]
    assert current.obs["soft_misses"] == ([missed] if missed else [])
    assert current.obs["stats"]["published"] > 0 and current.obs["history"]["id"] == "simVideo0001"
    if missed:
        assert any(step["result"] == "SOFT-MISS" for step in current.steps)
        assert not any(step["result"] == "FAIL" for step in current.steps)
    if "exit_dialog" in scenario:
        assert current.obs["stopped_by"] == "development stop command" and commands(ops).count("stop") == 1


def test_picker_that_does_not_close_ends_key_presses_and_the_session_still_proves_playback(tmp_path, monkeypatch):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, picker_closes=False)
    sent = commands(ops)
    assert verdict == "pass" and current.ui_lost and not lock.exists()
    after = sent[sent.index("key down") + 1:]
    assert after[:2] == ["key circle", "key circle"]
    assert [line for line in after[2:] if line.startswith("key ")] == []  # no key goes to a screen it cannot name
    assert "stop" in after and current.obs["stats"]["published"] > 0 and "(h) capture" not in current.obs


def test_short_video_is_played_to_its_end(tmp_path, monkeypatch):
    verdict, current, _, _, _, _ = run(tmp_path, monkeypatch, duration=40.0)
    assert verdict == "pass" and current.obs["end_mode"] == "eof" and current.obs["history"]["played"] is True


def test_search_by_keyboard_is_optional_and_never_a_gate(tmp_path, monkeypatch):
    verdict, current, _, ops, _, _ = run(tmp_path, monkeypatch, extra=["--with-search", "--search-query", "tides"])
    sent = commands(ops)
    assert verdict == "pass" and sent.index("key square") > sent.index("seek +10")
    assert sent[sent.index("key square"):][:3] == ["key square", "type tides", "kbdone"]
    assert current.obs["soft_misses"] == [] and "rows" in current.obs["search"]


def test_keyboard_the_snapshot_cannot_see_is_a_soft_miss_and_is_closed(tmp_path, monkeypatch):
    verdict, current, _, ops, lock, _ = run(tmp_path, monkeypatch, extra=["--with-search"], keyboard_visible=False)
    sent = commands(ops)
    assert verdict == "pass" and current.obs["soft_misses"] == ["(c) search by keyboard"] and not lock.exists()
    assert "type Me at the zoo" not in sent and "kbdone" not in sent  # nothing is typed into a keyboard it cannot see


def test_session_source_has_no_console_transport_of_its_own():
    source = (ROOT / "tools/console/session.py").read_text()
    assert "ftplib" not in source and "import socket" not in source and "create_connection" not in source
    for helper in ("observe.py", "sim.py"):
        text = (ROOT / "tools/console" / helper).read_text()
        assert "ftplib" not in text and "import socket" not in text and "import subprocess" not in text
