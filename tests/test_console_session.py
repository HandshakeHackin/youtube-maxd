"""Offline checks of tools/console: parsers, postconditions, the lock, the
candidate check and the session's control flow against the in-memory console.
Nothing here may open a socket: the fixture below makes that an error."""
import hashlib
import json
import re
import socket
import subprocess
import sys
from types import SimpleNamespace
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/console"))
import observe as ob  # noqa: E402
import session as se  # noqa: E402
import sim  # noqa: E402
from afterglow_stage import AfterglowStage  # noqa: E402
from transport import ConsoleDown  # noqa: E402

BUILD = "prosperotube-0123abcd_1011-2201"
STATUS = (f"build={BUILD} t=12 scr=2 be=1 pos=3.50 dur=19.0 fps=15.0 fatal=0 eof=0 complete=0 "
          "active=1 parked=0\n")


def test_afterglow_cleanup_removes_partial_override_uploads():
    path = ob.AFTERGLOW_SOUND_OFF
    files = {f"{ob.DATA}/{name}" for name in ob.override_names(path)}
    console = SimpleNamespace(delete=files.discard,
                              listdir=lambda _: {name.rsplit("/", 1)[-1]: {} for name in files})
    stage = AfterglowStage.__new__(AfterglowStage)
    stage.s, stage.written = SimpleNamespace(console=console), [path]
    assert stage.remove([path]) == [] and path in stage.gone and not files


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
    # The lines as native/elevation/tube_jailbreak.cpp writes them now: request, then outcome with ids.
    assert ob.storage_verdict("Lapy: requesting filesystem access\n") is None
    assert ob.storage_verdict("Lapy: requesting filesystem access\n"
                              "Lapy: path=helper status=0 storage=ready error=0 uid 0/0 gid 0/1\n") == "ready"
    assert ob.storage_verdict("Lapy: path=existing status=0 storage=failed error=0xd uid 1000/1000 gid 1000/1000\n") == "failed"
    assert ob.storage_verdict("Lapy: path=resident status=11; durable storage unavailable; uid 1000/1000 gid 1000/1000\n") == "failed"


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

def make_candidate(root, layout="protocol", instrumented=True, afterglow=True, capture=True):
    folder = root / ob.TITLE
    (folder / "sce_sys").mkdir(parents=True)
    marks = b" dev_command dev_status " if instrumented else b""
    marks += (ob.AG_MOUNT_MARK + b" " if afterglow else b"") + (ob.AG_CAPTURE_MARK + b" " if capture else b"")
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


def run(tmp_path, monkeypatch, extra=(), instrumented=True, lock_text=None, candidate_marks=None, **scenario):
    candidate = make_candidate(tmp_path / "candidate", instrumented=instrumented, **(candidate_marks or {}))
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
    for helper in ("observe.py", "sim.py", "afterglow_stage.py"):
        text = (ROOT / "tools/console" / helper).read_text()
        assert "ftplib" not in text and "import socket" not in text and "import subprocess" not in text


# --- the Afterglow interface: what its frame reports ---------------------------------------------

# dev_ui.json as the instrumented build writes it while Afterglow is shown: Frame::snapshot_json()
# (native/afterglow/frame_draw.cpp) where the classic layouts report the focused element.
AG_HOME = (
    '{"seq":412,"t_ms":190233412,"ui":{"screen":{"id":23,"name":"EmbyBrowse"},"docs":["afterglow","youtube"],'
    '"focused":{"interface":"afterglow","look":"afterglow","screen":"home","stack":[{"screen":"home","argument":""}],'
    '"moving":0,"rail_open":0,"attract":0,"dialog":0,'
    '"focus":{"id":"topics/1","text":"Technology","index":1,"total":2,"rect":[392.0,508.0,248.0,136.0]},'
    '"focusable":[{"id":"topics/0","text":"Science"},{"id":"topics/1","text":"Technology"},'
    '{"id":"trending/0","text":"How tides work"},{"id":"trending/1","text":"Как работает прилив"}],"toasts":[],'
    '"state":{"load":"ready","profile":"guest","shelves":"2","row":"topics","hero":"How tides work","invite":"1",'
    '"lists":{"shelf:topics":["Science","Technology"],"shelf:trending":["How tides work","Как работает прилив"]}},'
    '"stats":{"frame_ms":16.6667,"worst_frame_ms":33.4,"update_ms":0.412,"draw_ms":1.87,"instances":231,"runs":24,'
    '"fill":2.91,"pictures_ready":9,"pictures_pending":0,"picture_bytes":1.17965e+06,"data_pending":0}},'
    '"also_highlighted":[],"modal":null,"toast":null,"native_player":null}}')


def ag(**changes):
    """The frame's snapshot of AG_HOME with some of its values replaced."""
    frame = json.loads(AG_HOME)["ui"]["focused"]
    frame.update(changes)
    return frame


def test_afterglow_snapshot_is_found_where_the_build_puts_it():
    snapshot = ob.parse_ui(AG_HOME.encode())
    frame = ob.ag_frame(snapshot)
    assert ob.ag_shown(snapshot) and frame["interface"] == "afterglow" and ob.ag_screen(frame) == "home"
    assert ob.ag_stack(frame) == ["home"] and ob.ag_place(frame) == ("topics", 1) and ob.ag_state(frame, "row") == "topics"
    assert ob.ag_lists(frame) == [("topics", ["Science", "Technology"]), ("trending", ["How tides work", "Как работает прилив"])]
    assert not ob.ag_flag(frame, "rail_open") and ob.ag_flag(ag(rail_open=1), "rail_open")
    assert "focus=topics/1" in ob.ag_line(frame) and ob.ag_line(None) == "no Afterglow snapshot"
    assert "rail_open" in ob.ag_line(ag(rail_open=1)) and ob.ag_place(ag(focus={"id": "rail/home"})) is None
    assert not ob.pc_browser(snapshot)[0]  # the classic gate does not take the interface for its browser
    # the classic browser, a kit screen and nothing at all are not the interface
    assert ob.ag_frame(ob.parse_ui(REAL_BROWSER.encode())) is None and not ob.ag_shown(ob.parse_ui(REAL_BROWSER.encode()))
    assert ob.ag_frame(ui()) is None and ob.ag_frame(None) is None and ob.ag_lists(None) == [] and ob.ag_stack(None) == []
    # an RmlUi keyboard over the interface: the document is listed, the report names the keyboard's element
    covered = json.loads(AG_HOME)
    covered["ui"]["focused"] = {"id": "k0", "tag": "div", "class": "kb-key focused", "text": "q", "doc": "keyboard"}
    assert ob.ag_shown(covered) and ob.ag_frame(covered) is None


def test_afterglow_stats_are_read_summarised_and_judged():
    values, missing = ob.ag_stats(ag())
    assert missing == [] and values["frame_ms"] == 16.6667 and values["picture_bytes"] == 1179650.0 and len(values) == len(ob.AG_STATS)
    partial = ag(stats={"frame_ms": 16.7, "runs": True, "instances": "many", "fill": None})
    assert ob.ag_stats(partial) == ({"frame_ms": 16.7}, [name for name in ob.AG_STATS if name != "frame_ms"])
    assert ob.ag_stats(ag(stats="none")) == ({}, list(ob.AG_STATS)) and ob.ag_stats(None)[0] == {}
    summary = ob.ag_summary([{"frame_ms": 16.0, "instances": 200, "runs": 20}, {"frame_ms": 18.0, "instances": 220, "runs": 22},
                             {"frame_ms": 33.0, "instances": 210, "runs": 21, "fill": 2.5}])
    assert summary["frame_ms"] == {"min": 16.0, "median": 18.0, "max": 33.0} and summary["fill"]["median"] == 2.5
    assert ob.ag_summary([{"frame_ms": 16.0}, {"frame_ms": 18.0}])["frame_ms"]["median"] == 17.0
    ok, detail = ob.pc_ag_draws(summary)
    assert ok and "frame 18.0 ms (max 33.0" in detail and "210 instances in 21 runs" in detail
    assert not ob.pc_ag_draws(ob.ag_summary([{"frame_ms": 16.0, "instances": 0, "runs": 0}]))[0]
    with pytest.raises(ob.Unjudged, match="instances"):
        ob.pc_ag_draws(ob.ag_summary([{"frame_ms": 16.0}]))


def test_afterglow_home_shelves_focus_ids_and_texts():
    frame = ag()
    assert ob.pc_ag_home(frame) == (True, "load=ready, profile=guest, topics (2), trending (2)")
    assert ob.pc_ag_focus_ids(frame)[0] and "4 focusable ids" in ob.pc_ag_focus_ids(frame)[1]
    ok, detail = ob.pc_ag_texts(frame, "[1.0] INFO  afterglow: interface up, look afterglow, 3840x2160\n")
    assert ok and "4 titles, none empty (1 with letters beyond Latin)" in detail
    # what is wrong is said, not guessed
    offline = ag(state={"load": "offline", "lists": {}}, focus={"id": "offline", "text": ""})
    assert ob.pc_ag_home(offline) == (False, "no shelf is listed (load=offline, focus offline)")
    assert not ob.pc_ag_home(ag(screen="opening"))[0]
    assert not ob.pc_ag_focus_ids(ag(focus={"id": "topics/7", "text": "x"}))[0]
    assert not ob.pc_ag_focus_ids(ag(focus={"id": "elsewhere/0", "text": "Science"}))[0]
    assert not ob.pc_ag_focus_ids(ag(focus={"id": "topics/0", "text": "Technology"}))[0]  # the id names another card
    assert not ob.pc_ag_focus_ids(ag(focusable=[{"id": "card-3", "text": "x"}]))[0]
    blank = ag()
    blank["state"]["lists"]["shelf:trending"][1] = " "
    assert ob.pc_ag_texts(blank, "")[0] is False and "1 of 4 cards have no title" in ob.pc_ag_texts(blank, "")[1]
    down = "[2.0] INFO  afterglow: the text engine did not start (no font file could be opened); the kit's baked fonts draw\n"
    assert ob.pc_ag_texts(frame, down) == (False, "the text engine did not start (no font file could be opened): the kit's baked fonts draw")
    # what the snapshot does not say cannot be judged
    for check, broken in ((ob.pc_ag_home, None), (ob.pc_ag_home, ag(state="x")), (ob.pc_ag_focus_ids, ag(focus=None)),
                          (lambda frame: ob.pc_ag_texts(frame, ""), ag(state={"lists": {}}))):
        with pytest.raises(ob.Unjudged):
            check(broken)


def test_afterglow_log_lines_say_look_output_size_and_capture_verdict():
    log = ("[1.0] INFO  afterglow: interface up, look afterglow, 3840x2160\n[1.1] INFO  EVO boot: Application: x\n"
           "[9.0] INFO  afterglow: the screen stack is full; the new screen was not opened\n")
    assert ob.ag_interface_up(log) == {"look": "afterglow", "width": 3840, "height": 2160} and ob.ag_interface_up("x") is None
    assert len(ob.ag_log_lines(log)) == 2 and ob.ag_text_engine_failed(log) is None
    assert ob.capture_verdict("[1] INFO  bc REMOTE_CMD shot\n") is None
    assert ob.capture_verdict("[1] INFO  bc REMOTE_CMD shot\n[1] INFO  bc DEV_CAPTURE wrote /data/prosperotube/evo_shot_001.bmp 3840x2160\n") == \
        ("wrote", "/data/prosperotube/evo_shot_001.bmp 3840x2160")
    assert ob.capture_verdict("[1] INFO  bc DEV_CAPTURE failed cannot write /x.part\n") == ("failed", "cannot write /x.part")
    assert ob.capture_verdict("[1] INFO  bc REMOTE_CMD shot\n[1] INFO  bc REMOTE_CMD unknown\n")[0] == "unknown"
    assert ob.capture_verdict("[1] INFO  bc REMOTE_CMD shot\n[1] INFO  bc REMOTE_CMD key l1\n[1] INFO  bc REMOTE_CMD unknown\n")[0] == "unknown"
    assert ob.capture_verdict("[1] INFO  bc REMOTE_CMD key x\n[1] INFO  bc REMOTE_CMD unknown\n") is None


def test_a_snapshot_the_c_writer_got_wrong_is_read_after_repair_or_not_at_all():
    assert ob.parse_ui(AG_HOME.replace('"fill":2.91', '"fill":nan').encode()) is None
    snapshot, repairs = ob.parse_ui_repaired(AG_HOME.replace('"fill":2.91', '"fill":nan').replace('"draw_ms":1.87', '"draw_ms":-inf').encode())
    assert repairs == ["nan or inf written as a number"] and ob.ag_stats(ob.ag_frame(snapshot))[1] == ["draw_ms", "fill"]
    cut = AG_HOME.encode().replace("Как".encode(), "Как".encode()[:5])  # a title cut inside a character
    assert ob.parse_ui(cut) is None
    snapshot, repairs = ob.parse_ui_repaired(cut)
    assert repairs == ["bytes that are not UTF-8"] and ob.ag_place(ob.ag_frame(snapshot)) == ("topics", 1)
    assert ob.parse_ui_repaired(AG_HOME.encode()) == (ob.parse_ui(AG_HOME.encode()), [])
    assert ob.parse_ui_repaired(AG_HOME[:300].encode()) == (None, []) and ob.parse_ui_repaired(b"") == (None, [])
    assert ob.parse_ui_repaired(b'{"nan": "a title: nan"}')[0] is None  # not a snapshot, and no text was touched to find out
    assert ob.ag_raw_shown(AG_HOME[:300].encode()) and not ob.ag_raw_shown(REAL_BROWSER.encode()) and not ob.ag_raw_shown(None)


def test_capture_is_read_as_pixels_and_written_again_as_png():
    import zlib
    data = sim.sim_bmp(96, 54)
    facts, problem = ob.bmp_facts(data)
    assert problem is None and (facts["width"], facts["height"], facts["bits"], facts["bottom_up"]) == (96, 54, 24, True)
    assert ob.pc_capture(facts)[0] and "96x54" in ob.pc_capture(facts)[1]
    flat, _ = ob.bmp_facts(sim.sim_bmp(96, 54, flat=True))
    assert flat["colours"] == 1 and ob.pc_capture(flat) == (False, f"the capture is flat: 1 colours among {flat['samples']} sampled points")
    assert not ob.pc_capture({**facts, "lit": 0.001})[0]
    for bad in (b"", b"BM", b"PNG" + bytes(60), data[:400], data[:28] + b"\x08\x00" + data[30:]):
        assert ob.bmp_facts(bad)[0] is None
    # an odd width has padding at the end of every row: the PNG must hold the same pixels, top row first
    odd = sim.sim_bmp(5, 3)
    png = ob.bmp_to_png(odd)
    assert png[:8] == b"\x89PNG\r\n\x1a\n" and int.from_bytes(png[16:20], "big") == 5 and int.from_bytes(png[20:24], "big") == 3
    length = int.from_bytes(png[33:37], "big")
    rows = zlib.decompress(png[41:41 + length])
    assert len(rows) == 3 * (1 + 5 * 3)
    stride = (5 * 3 + 3) & ~3
    for row in range(3):  # PNG row 0 is the BMP's last stored row; pixels go from B,G,R to R,G,B
        stored = odd[54 + (2 - row) * stride:54 + (2 - row) * stride + 15]
        line = rows[row * 16 + 1:row * 16 + 16]
        assert rows[row * 16] == 0 and line[0::3] == stored[2::3] and line[1::3] == stored[1::3] and line[2::3] == stored[0::3]
    with pytest.raises(ValueError):
        ob.bmp_to_png(b"not a picture")


# --- the Afterglow stage against the in-memory console ------------------------------------------

ORIGINAL = json.dumps({"source": "http://old.example:8765", "token": "o" * 43, "records": []}).encode() + b"\n"
# What today's build cannot be judged on: the frame's snapshot carries no numbers of its text engine yet
# (docs/afterglow/requests/drv.md). With them, a stage without failures is a "pass"; until then it is "partial".
ASKED = ["(c) glyphs and fonts of the text engine"]
# The sound step comes last and only moves the focus: along the shelf and back, to the other shelf and back.
SOUND_KEYS = ["key right", "key left", "key up", "key down"]
AG_KEYS = ["shot", "key right", "key down", "key up", "key left", "key left", "key right", "key square", "key circle",
           "key down", "shot", "key cross", "key circle", "key cross", *SOUND_KEYS]
SOUND_STEP, REMOVED_STEP = "(s) sound: the port opens at the first cue", "(g) every file the stage wrote is removed"
# Every file the stage writes, in the order it writes them: the sound module's two silencers before the switch.
OVERRIDES = [ob.AFTERGLOW_SOUND_OFF, ob.AFTERGLOW_VIBRATION_OFF, ob.AFTERGLOW_ON]


def ag_run(tmp_path, monkeypatch, extra=(), **scenario):
    scenario.setdefault("state", ORIGINAL)
    return run(tmp_path, monkeypatch, extra=["--afterglow", *extra], **scenario)


def ag_steps(current):
    return {step["step"]: step["result"] for step in current.steps if step.get("stage") == "afterglow"}


def ag_sent(ops):
    """What the stage sent to the app, without the presses that only keep Home awake."""
    sent = commands(ops)
    return [line for line in sent[sent.index("shot"):] if line not in ("key l1", "quit")] if "shot" in sent else []


def left_as_found(current, console, ops, lock):
    """Whatever the stage met: one launch, one close, the override file gone, the state put back, the lock released."""
    assert payloads(ops) == ["launch", "close"] and console.running == [] and current.teardown_clean is True
    assert not lock.exists() and console.exit_asked == 0
    assert console.files[ob.STATE_FILE] == ORIGINAL and current.obs["restore"]["failed"] == []
    names = {Path(path).name for path in console.files if path.startswith(ob.DATA + "/")}
    assert not names & (set(ob.DEV_FILES) | set(ob.AFTERGLOW_FILES)) and not [name for name in names if ob.SHOT.match(name)]
    for override in OVERRIDES:
        switch = [op for op in ops if override in op["detail"] and op["writes"]]
        assert [op["kind"] for op in switch] in (["ftp-put", "ftp-delete"], [])  # written once, removed once, by the stage itself
    written = {op["detail"].split()[0] for op in ops if op["kind"] == "ftp-put"}
    assert written <= {ob.CMD_FILE, ob.STATE_FILE, *OVERRIDES}  # nothing else is ever written to the console


def test_afterglow_stage_follows_the_core_path_in_the_same_launch(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch)
    log = (out / "session.log").read_text()
    assert verdict == "pass", log
    assert [stage["stage"] for stage in current.stages] == [0, 1, 2, 3, 4, 5, "5a", 6]
    assert [stage["result"] for stage in current.stages] == ["PASS"] * 8
    left_as_found(current, console, ops, lock)
    report = current.obs["afterglow"]
    steps = ag_steps(current)
    assert report["result"] == "partial" and report["failed"] == [] and report["notes"] == ASKED, steps
    assert list(steps) == [
        "(pre) the app is on its browser with nothing playing", "(a) switched on: the frame draws",
        "(c) Home lists its shelves", "(c) pictures become ready", "(b) frame statistics at rest", "(b) output resolution",
        "(c) focus ids are <shelf>/<index>", "(c) texts are present", "(c) glyphs and fonts of the text engine",
        "(e) scanout capture of Home",
        "(d) focus moves along a shelf", "(d) focus moves between shelves", "(d) the rail opens and closes",
        "(d) another screen opens and Circle returns", "(b) frame statistics while moving",
        "(e) scanout capture of Home, a video in focus",
        "(f) Cross on a video starts playback", "(f) position advancing", "(f) stopped through the confirmation",
        "(f) engine statistics for the playback", "(f) the interface shown again is Afterglow", SOUND_STEP,
        "(g) afterglow.on removed, nothing else written", "(g) the classic browser draws again", REMOVED_STEP]
    assert [label for label, result in steps.items() if result != "PASS"] == ASKED
    # the classic core path came first and is what it was
    sent = commands(ops)
    assert sent[:6] == ["key cross", "key right", "key right", "key cross", "key cross", "seek +10"]
    assert current.obs["video"]["id"] == "simVideo0001" and current.obs["stopped_by"] == "confirmation dialog"
    kinds = [(op["kind"], op["detail"].split()[0]) for op in ops]
    assert kinds.index(("ftp-put", ob.AFTERGLOW_ON)) > max(i for i, op in enumerate(ops) if "command: seek" in op["detail"])
    assert ag_sent(ops) == AG_KEYS
    # the numbers and the picture
    assert set(report["stats_rest"]["summary"]) == set(ob.AG_STATS) and report["stats_rest"]["samples"] >= 6
    assert report["stats_moving"]["samples"] >= 6
    assert report["output"] == {"look": "afterglow", "width": 3840, "height": 2160, "source": "the mount's line in the app's log"}
    assert report["home"]["shelves"]["topics"][0] == "Science" and report["home"]["profile"] == "guest"
    assert report["rail"] == {"focus": "rail/home", "text": "Home", "destinations": 7}
    assert report["other_screen"]["screen"] == "search" and report["played"] == {"focus": "trending/0", "title": "How tides work"}
    capture = report["captures"]["afterglow-home"]
    assert (capture["width"], capture["height"]) == (96, 54) and "screen=home" in capture["shows"] and "focus=topics/0" in capture["shows"]
    assert not [name for name, shot in report["captures"].items() if "moving" in shot["shows"]]  # each of a page at rest
    assert (out / "afterglow-home.bmp").read_bytes() == sim.sim_bmp() and (out / "afterglow-home.png").read_bytes()[:4] == b"\x89PNG"
    assert "focus=trending/0" in report["captures"]["afterglow-home-video"]["shows"] and (out / "afterglow-home-video.png").is_file()
    assert (out / "playback-scanout.bmp").read_bytes()[:2] == b"BM"  # the classic capture of the player is still taken
    for name in ("first-snapshot.json", "home.json", "stats-rest.json", "stats-moving.json", "rail.json", "other-screen.json",
                 "stop-dialog-ui.json", "after-playback.json"):
        assert (out / "afterglow" / name).is_file(), name
    result = json.loads((out / "result.json").read_text())
    assert result["launches"] == 1 and result["observations"]["afterglow"]["result"] == "partial"
    on = next(step for step in result["steps"] if step["step"] == "(a) switched on: the frame draws")
    assert on["gate"] == "hard" and 0 < on["numbers"]["seconds"] <= 5 and on["numbers"]["first_worst_frame_ms"] > 0
    rest = next(step for step in result["steps"] if step["step"] == "(b) frame statistics at rest")
    assert rest["gate"] == "soft" and rest["numbers"]["instances"] > 0 and "frame 16." in rest["detail"]
    assert "Afterglow PARTIAL" in log.splitlines()[-1]
    # the faces the interface wrote for its text engine are the app's own cache: left, and listed
    assert set(sim.AG_FONTS) <= set(current.obs["restore"]["left"])


def test_build_that_reports_what_was_asked_of_the_frame_owner_can_pass_outright(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_asked=True)
    report = current.obs["afterglow"]
    assert verdict == "pass" and report["result"] == "pass" and report["notes"] == [] and report["failed"] == []
    assert report["output"] == {"width": 3840, "height": 2160, "source": "the snapshot"}
    glyphs = next(step for step in current.steps if step["step"] == ASKED[0])
    assert glyphs["result"] == "PASS" and glyphs["numbers"]["glyphs"] == 412 and "6 font files" in glyphs["detail"]
    left_as_found(current, console, ops, lock)
    (tmp_path / "baked").mkdir()
    _, current, _, _, _, _ = ag_run(tmp_path / "baked", monkeypatch, ag_asked=True, ag_text_engine=False)
    assert ag_steps(current)[ASKED[0]] == "FAIL" and ag_steps(current)["(c) texts are present"] == "FAIL"


def test_without_the_flag_nothing_of_the_stage_happens(tmp_path, monkeypatch):
    verdict, current, console, ops, _, _ = run(tmp_path, monkeypatch)
    assert verdict == "pass" and "afterglow" not in current.obs and len(current.stages) == 7
    assert not [op for op in ops if "afterglow" in op["detail"]] and "shot" not in commands(ops)


@pytest.mark.parametrize("profile, played", [("guest", "trending/0"), ("account", "recommended/0")])
def test_afterglow_stage_plays_from_the_first_shelf_of_videos_of_either_profile(tmp_path, monkeypatch, profile, played):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_profile=profile)
    assert verdict == "pass" and current.obs["afterglow"]["notes"] == ASKED and current.obs["afterglow"]["played"]["focus"] == played
    left_as_found(current, console, ops, lock)


def test_interface_that_never_appears_fails_the_session_and_leaves_the_console_as_found(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, afterglow="never")
    steps = ag_steps(current)
    assert verdict == "fail" and current.stages[6]["result"] == "FAIL" and "(a) switched on" in current.stages[6]["detail"]
    assert steps["(a) switched on: the frame draws"] == "FAIL" and "(e) scanout capture of Home" not in steps
    said = next(step["detail"] for step in current.steps if step["step"] == "(a) switched on: the frame draws")
    assert "the classic browser still draws" in said and "last line in the app's log: none at all" in said
    assert steps["(g) afterglow.on removed, nothing else written"] == "PASS" and steps["(g) the classic browser draws again"] == "PASS"
    assert current.obs["afterglow"]["result"] == "fail" and ag_sent(ops) == []  # no key is sent to an interface that is not there
    assert current.obs["stats"]["published"] > 0  # the core path's result stands
    left_as_found(current, console, ops, lock)


def test_switch_that_cannot_be_written_fails_the_stage_before_it_sends_anything(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, afterglow="unwritable")
    steps = ag_steps(current)
    # the two files written before the switch are the stage's to take away again: its third step
    assert verdict == "fail" and list(steps.values()) == ["PASS", "FAIL", "PASS"] and steps["(a) switched on: the frame draws"] == "FAIL"
    assert list(steps)[2] == REMOVED_STEP and not [path for path in OVERRIDES if path in console.files]
    assert current.obs["afterglow"]["result"] == "fail" and ag_sent(ops) == [] and ob.AFTERGLOW_ON not in console.files
    assert payloads(ops) == ["launch", "close"] and current.teardown_clean is True and not lock.exists()
    assert console.files[ob.STATE_FILE] == ORIGINAL


def test_snapshot_that_is_not_json_costs_the_judgements_not_the_picture(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, ag_snapshot="cut")
    steps = ag_steps(current)
    assert verdict == "pass" and current.obs["afterglow"]["result"] == "partial", steps
    assert steps["(a) switched on: the frame draws"] == "PASS" and steps["(e) scanout capture of Home"] == "PASS"
    assert steps.pop("(b) output resolution") == "PASS"  # read from the app's log, not from the snapshot
    judged = [label for label in steps if label.startswith(("(b)", "(c)", "(d)", "(f)", "(s)"))]
    assert len(judged) == 13 and all(steps[label] == "NOTE" for label in judged) and "FAIL" not in steps.values()
    assert ag_sent(ops) == ["shot"]  # no key goes to a screen that cannot be read
    saved = sorted((out / "afterglow").glob("unjudged-*.json"))
    assert len(saved) == len(judged) and all(ob.ag_raw_shown(path.read_bytes()) for path in saved)
    assert ob.ag_raw_shown((out / "afterglow/unreadable-snapshot.json").read_bytes())
    assert (out / "afterglow-home.bmp").is_file()
    left_as_found(current, console, ops, lock)


def test_snapshot_with_a_number_that_is_none_is_repaired_and_said(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, ag_snapshot="nan")
    assert verdict == "pass" and current.obs["afterglow"]["notes"] == ASKED and ag_sent(ops) == AG_KEYS
    assert current.obs["afterglow"]["snapshot_repairs"] == ["nan or inf written as a number"]
    assert current.obs["afterglow"]["stats_rest"]["missing"] == ["fill"] and b'"fill":nan' in (out / "afterglow/repaired-snapshot.json").read_bytes()
    assert any("not valid JSON" in note for note in current.notes)
    left_as_found(current, console, ops, lock)


BOTH_CAPTURES = ["(e) scanout capture of Home", "(e) scanout capture of Home, a video in focus"]


@pytest.mark.parametrize("scenario, failed, also", [
    ({"ag_pictures": False}, ["(c) pictures become ready"], {"(f) position advancing": "PASS"}),
    ({"ag_text_engine": False}, ["(c) texts are present"], {"(e) scanout capture of Home": "PASS"}),
    ({"capture": "refused"}, BOTH_CAPTURES, {"(f) position advancing": "PASS"}),
    ({"capture": "unknown"}, BOTH_CAPTURES, {"(f) position advancing": "PASS"}),
    ({"capture": "silent"}, BOTH_CAPTURES, {"(d) the rail opens and closes": "PASS", "(f) position advancing": "PASS"}),
    ({"capture": "flat"}, BOTH_CAPTURES, {"(f) position advancing": "PASS"}),
    ({"ag_rail": False}, ["(d) the rail opens and closes"], {"(f) position advancing": "PASS"}),
    ({"ag_other_opens": False}, ["(d) another screen opens and Circle returns"], {"(f) position advancing": "PASS"}),
    ({"ag_back": False}, ["(f) the interface shown again is Afterglow"], {"(g) the classic browser draws again": "PASS"}),
    ({"exit_dialog": False}, ["(f) stopped through the confirmation"], {"(f) player retired after the stop command": "PASS"}),
])
def test_soft_failures_are_recorded_and_the_stage_goes_on(tmp_path, monkeypatch, scenario, failed, also):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, **scenario)
    steps = ag_steps(current)
    assert verdict == "pass", (out / "session.log").read_text()  # nothing an interface says fails the session
    assert [steps[label] for label in failed] == ["FAIL"] * len(failed)
    assert current.obs["afterglow"]["failed"] == failed and current.obs["afterglow"]["result"] == "fail"
    assert current.obs["afterglow"]["hard_failed"] == [] and current.stages[6]["result"] == "PASS"
    for label, result in also.items():
        assert steps[label] == result
    assert steps["(g) the classic browser draws again"] == "PASS"
    left_as_found(current, console, ops, lock)


def test_capture_refusals_say_why(tmp_path, monkeypatch):
    for index, (mode, reason) in enumerate((("refused", "the build refused the capture: cannot write"), ("unknown", "has no capture command"),
                                            ("flat", "the capture is flat"), ("silent", "no new evo_shot file"))):
        folder = tmp_path / str(index)
        folder.mkdir()
        _, current, _, _, _, out = ag_run(folder, monkeypatch, capture=mode)
        detail = next(step["detail"] for step in current.steps if step["step"] == "(e) scanout capture of Home")
        assert reason in detail
        assert (out / "afterglow-home.png").exists() == (mode == "flat")  # only a picture that was read is kept


@pytest.mark.parametrize("scenario, reason, crosses", [
    ({"ag_play": "ignored"}, "not observed within 90 s", 1),
    ({"ag_play": "refused"}, "This video could not be opened.", 1),
    ({"ag_unresolved": 2}, "the stream did not resolve", 2),
])
def test_playback_that_does_not_start_is_a_hard_failure_with_a_clean_teardown(tmp_path, monkeypatch, scenario, reason, crosses):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, **scenario)
    steps = ag_steps(current)
    assert verdict == "fail" and steps["(f) Cross on a video starts playback"] == "FAIL"
    assert current.obs["afterglow"]["hard_failed"] == ["(f) Cross on a video starts playback"]
    assert reason in next(step["detail"] for step in current.steps if step["step"] == "(f) Cross on a video starts playback")
    assert current.stages[6]["result"] == "FAIL" and "(f) position advancing" not in steps
    sent = ag_sent(ops)
    assert sent.count("key cross") == crosses and "key circle" not in sent[sent.index("key cross"):]  # nothing to stop
    assert steps["(e) scanout capture of Home"] == "PASS" and steps["(g) the classic browser draws again"] == "PASS"
    left_as_found(current, console, ops, lock)


def test_one_card_that_does_not_resolve_is_passed_over_once(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_unresolved=1)
    assert verdict == "pass" and current.obs["afterglow"]["failed"] == [] and current.obs["afterglow"]["notes"] == ASKED
    assert current.obs["afterglow"]["played"] == {"focus": "trending/1", "title": "Inside a volcano"}
    assert any("did not resolve" in note and "tried once" in note for note in current.notes)
    left_as_found(current, console, ops, lock)


@pytest.mark.parametrize("home", ["offline", "empty", "loading"])
def test_home_without_shelves_is_said_and_still_captured(tmp_path, monkeypatch, home):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_home=home)
    steps = ag_steps(current)
    assert verdict == "pass" and steps["(c) Home lists its shelves"] == "FAIL" and steps["(e) scanout capture of Home"] == "PASS"
    assert steps["(b) frame statistics at rest"] == "PASS" and steps["(f) Cross on a video starts playback"] == "NOTE"
    assert ag_sent(ops) == ["shot"]  # with nothing to focus, no key is pressed
    left_as_found(current, console, ops, lock)


@pytest.mark.parametrize("scenario, step, presses", [
    ({"ag_rail_closes": False}, "(d) the rail opens and closes", ["key left", "key left", "key right"]),
    ({"ag_other_returns": False}, "(d) another screen opens and Circle returns", ["key square", "key circle", "key circle"]),
])
def test_interface_that_does_not_come_back_to_home_gets_no_more_keys(tmp_path, monkeypatch, scenario, step, presses):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, **scenario)
    steps, sent = ag_steps(current), ag_sent(ops)
    assert verdict == "pass" and steps[step] == "FAIL" and steps["(f) Cross on a video starts playback"] == "NOTE"
    assert sent[-len(presses):] == presses and "key cross" not in sent  # Cross is never pressed where the focus is not known
    assert steps["(g) the classic browser draws again"] == "PASS"
    left_as_found(current, console, ops, lock)


def test_attract_mode_is_ended_with_a_press_home_ignores_before_the_picture(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_profile="account", ag_attract_at_start=True)
    sent = commands(ops)
    assert verdict == "pass" and current.obs["afterglow"]["notes"] == ASKED and current.obs["afterglow"]["attract_ended"] >= 1
    assert sent[sent.index("shot") - 1] == "key l1" and "attract" not in current.obs["afterglow"]["captures"]["afterglow-home"]["shows"]
    left_as_found(current, console, ops, lock)


@pytest.mark.parametrize("other, screen", [("triangle", "video"), ("none", None)])
def test_the_other_screen_can_be_a_video_page_or_left_out(tmp_path, monkeypatch, other, screen):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, extra=["--afterglow-other", other])
    steps = ag_steps(current)
    assert verdict == "pass" and steps["(f) position advancing"] == "PASS"
    if screen:
        assert current.obs["afterglow"]["other_screen"]["screen"] == screen and current.obs["afterglow"]["other_screen"]["stack"] == ["home", screen]
    else:
        assert steps["(d) another screen opens and Circle returns"] == "NOTE" and "key square" not in ag_sent(ops)
    left_as_found(current, console, ops, lock)


def test_app_that_stops_taking_commands_in_the_stage_is_left_running_with_the_switch_removed(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_hang_on="square")
    steps = ag_steps(current)
    assert verdict == "unresolved" and payloads(ops) == ["launch"] and lock.read_text() == "ProsperoTube"
    assert steps["(d) another screen opens and Circle returns"] == "FAIL" and steps["(f) Cross on a video starts playback"] == "NOTE"
    assert ob.AFTERGLOW_ON not in console.files  # the stage's own file is gone although nothing could be restored
    assert steps["(g) afterglow.on removed, nothing else written"] == "PASS" and console.exit_asked == 0


def test_console_that_stops_answering_in_the_stage_ends_everything_at_once(tmp_path, monkeypatch):
    (tmp_path / "a").mkdir()
    (tmp_path / "b").mkdir()
    _, _, _, whole, _, _ = ag_run(tmp_path / "a", monkeypatch)
    switched = next(index for index, op in enumerate(whole) if op["kind"] == "ftp-put" and ob.AFTERGLOW_ON in op["detail"])
    verdict, current, console, ops, lock, out = ag_run(tmp_path / "b", monkeypatch, down_after=switched + 40)
    assert verdict == "unresolved" and len(ops) == switched + 40 and lock.read_text() == "ProsperoTube"
    assert payloads(ops) == ["launch"] and current.stages[6]["result"] == "CONSOLE-DOWN"
    assert ob.AFTERGLOW_ON in console.files  # nothing more was sent, not even the removal: the owner is told
    assert json.loads((out / "result.json").read_text())["observations"]["afterglow"]["result"] in ("fail", "partial", "pass")


def test_override_that_keeps_the_classic_browser_stops_the_session_before_anything_changes(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, files={ob.AFTERGLOW_OFF: b""})
    assert verdict == "no-run" and payloads(ops) == [] and not [op for op in ops if op["writes"]] and not lock.exists()
    assert "afterglow.off" in current.stages[2]["detail"] and console.files[ob.AFTERGLOW_OFF] == b""


def test_switch_left_by_an_earlier_run_is_set_aside_for_the_launch_and_put_back(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, files={ob.AFTERGLOW_ON: b"left by hand\n"})
    assert verdict == "pass" and current.obs["afterglow"]["failed"] == [] and not lock.exists()
    assert console.files[ob.AFTERGLOW_ON] == b"left by hand\n" and "afterglow.on" in current.obs["restore"]["restored"]
    assert any("was already in the data folder" in note for note in current.notes)
    launch = [op["kind"] for op in ops].index("payload")
    assert [op for op in ops[:launch] if op["kind"] == "ftp-delete" and ob.AFTERGLOW_ON in op["detail"]]


@pytest.mark.parametrize("marks, reason", [({"afterglow": False}, "no Afterglow mount"), ({"capture": False}, "no capture command")])
def test_candidate_without_the_mount_or_the_capture_command_is_refused_before_any_console_operation(tmp_path, monkeypatch, marks, reason):
    verdict, current, _, ops, lock, _ = ag_run(tmp_path, monkeypatch, candidate_marks=marks)
    assert verdict == "no-run" and ops == [] and not lock.exists() and reason in current.stages[0]["detail"]
    (tmp_path / "classic").mkdir()
    assert run(tmp_path / "classic", monkeypatch, candidate_marks=marks)[0] == "pass"  # the classic session does not need them


def test_core_failure_ends_the_session_before_the_stage_unless_asked(tmp_path, monkeypatch):
    (tmp_path / "a").mkdir()
    (tmp_path / "b").mkdir()
    verdict, current, console, ops, lock, _ = ag_run(tmp_path / "a", monkeypatch, topic_rows=0)
    assert verdict == "fail" and "afterglow" not in current.obs and not [op for op in ops if "afterglow" in op["detail"]]
    verdict, current, console, ops, lock, out = ag_run(tmp_path / "b", monkeypatch, extra=["--afterglow-after-core-fail"], topic_rows=0)
    assert verdict == "fail" and current.stages[5]["result"] == "FAIL" and current.stages[6]["result"] == "PASS"  # the core path's verdict stands
    assert current.obs["afterglow"]["failed"] == [] and (out / "afterglow-home.bmp").is_file()
    left_as_found(current, console, ops, lock)


def test_stage_is_not_run_when_the_app_is_not_on_its_browser(tmp_path, monkeypatch):
    original = se.Session.functional

    def left_playing(self):  # a core path that ends with the player open again
        original(self)
        self.command("key cross")
        self.step("player open again", lambda: self.on_screen(ob.SCREEN_PLAYER), 20)
    monkeypatch.setattr(se.Session, "functional", left_playing)
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch)
    steps = ag_steps(current)
    assert verdict == "pass" and current.obs["afterglow"]["result"] == "not-run" and list(steps.values()) == ["NOTE"]
    assert "scr=2" in next(step["detail"] for step in current.steps if step.get("stage") == "afterglow")
    assert not [op for op in ops if "afterglow" in op["detail"]] and not lock.exists()


def dry_run(tmp_path, monkeypatch, capsys):
    """session.py --dry-run --afterglow as a person starts it: (exit status, what it printed)."""
    candidate = make_candidate(tmp_path / "candidate")
    protocol = tmp_path / "protocol/scripts"
    protocol.mkdir(parents=True)
    for tool in ("ps5-console.py", "send-controller.sh"):
        (protocol / tool).write_text("")
    monkeypatch.setattr(se.shutil, "which", lambda name: f"/usr/bin/{name}")
    monkeypatch.setattr(se.subprocess, "run", lambda *a, **k: subprocess.CompletedProcess(a, 0, "built.elf\n", ""))
    code = se.main(["--i-have-owner-approval", "--dry-run", "--afterglow", "--lock-file", str(tmp_path / "lock.txt"),
                    "--candidate", str(candidate), "--protocol", str(tmp_path / "protocol"), "--out", str(tmp_path / "out")])
    return code, capsys.readouterr().out


def test_dry_run_walks_the_stage_and_prints_its_console_operations(tmp_path, monkeypatch, capsys):
    code, printed = dry_run(tmp_path, monkeypatch, capsys)
    assert code == 0 and "RESULT: PASS (DRY RUN, simulated console)" in printed and "Afterglow PARTIAL" in printed
    assert f"ftp-put {ob.AFTERGLOW_ON} (Afterglow override: on" in printed and f"ftp-delete {ob.AFTERGLOW_ON}" in printed
    assert "command: shot" in printed and not (tmp_path / "lock.txt").exists()


# --- the sound module's own audio port: silenced before the switch, let go as the last step ------

def sound_step(current):
    return next(step for step in current.steps if step["step"] == SOUND_STEP)


def file_moves(ops):
    """Every write and removal of the stage's override files, in the order the console saw them."""
    return [(op["kind"], Path(op["detail"].split()[0]).name) for op in ops
            if op["kind"] in ("ftp-put", "ftp-delete") and op["detail"].split()[0] in OVERRIDES]


def test_sound_module_lines_and_the_frame_counter_are_read_as_the_app_writes_them():
    up = ("[1.0] INFO  afterglow: sound: 47 recordings, 28 cues served, 0 silent, 3184 KiB\n"
          "[1.1] INFO  afterglow: interface up, look afterglow, 3840x2160\n")
    said = ob.ag_sound(up)
    assert said["recordings"] == {"files": 47, "served": 28, "silent": 0} and said["port"] is None and said["line"] is None
    opened = ob.ag_sound(up + "[9.0] INFO  afterglow: sound: port open, handle 3\n")
    assert (opened["port"], opened["handle"], opened["line"]) == ("open", 3, "afterglow: sound: port open, handle 3")
    failed = ob.ag_sound(up + "[9.0] INFO  afterglow: sound: the port failed (0x80260005); the interface stays silent\n")
    assert failed["port"] == "failed" and failed["handle"] is None and "0x80260005" in failed["line"]
    assert ob.ag_sound("[9.0] INFO  afterglow: sound: no thread for the port; the interface stays silent\n")["port"] == "failed"
    # a port that opened and refused its grains later is a failed one
    later = ob.ag_sound("[9.0] INFO  afterglow: sound: port open, handle 3\n"
                        "[20.0] INFO  afterglow: sound: the port failed (0x80260001); the interface stays silent\n")
    assert later["port"] == "failed" and later["handle"] == 3 and "0x80260001" in later["line"]
    rest = ob.ag_sound("[1] INFO  afterglow: sound: glass/focus_01.wav: not a recording\n[2] INFO  afterglow: sound: parked\n"
                       "[3] INFO  afterglow: sound: closed, 412 grains written, 0 refused\n"
                       "[4] INFO  afterglow: vibration: mode 0x00000000, first pulse 0x00000000\n")
    assert rest["recordings"] is None and rest["port"] is None and len(rest["other"]) == 3
    assert rest["vibration"] == ["afterglow: vibration: mode 0x00000000, first pulse 0x00000000"]
    assert ob.ag_sound("") == {"recordings": None, "port": None, "handle": None, "line": None, "vibration": [], "other": []}
    # the frame's own counter: alive is a counter that rises between two reads
    counted = [ag(stats={"frame_ms": 16.7, "frames": frames}) for frames in (4520.0, 4581.0)]
    assert ob.ag_frames(counted[0]) == 4520.0 and ob.ag_frames(ag()) is None and ob.ag_frames(ag(stats={"frames": True})) is None
    assert ob.pc_ag_beat(*counted) == (True, "frames 4520 -> 4581")
    assert ob.pc_ag_beat(counted[1], counted[1]) == (False, "the frame's counter stands at 4581")
    assert ob.pc_ag_beat(counted[0], None)[0] is False and ob.pc_ag_beat(None, None)[0] is False
    assert ob.pc_ag_beat(ag(), counted[1])[0] is None  # a build that does not count: not known, and said so
    # the files, and what a put that failed can leave of each
    assert ob.override_names(ob.AFTERGLOW_SOUND_OFF) == ("afterglow-sound.off", ".afterglow-sound.off.upload")
    assert set(ob.AFTERGLOW_FILES) == {name for path in OVERRIDES for name in ob.override_names(path)} and len(ob.AFTERGLOW_FILES) == 6
    assert "afterglow.off" not in ob.AFTERGLOW_FILES  # the file that keeps the classic browser is never the session's


def test_simulated_sound_lines_are_lines_the_app_can_write():
    source = ROOT / "native/afterglow/sound/app/sound_app.cpp"
    if not source.is_file():
        pytest.skip("the sound module is not in this tree")
    formats = re.findall(r'evo_boot_log\("((?:[^"\\]|\\.)*)"', source.read_text())

    def shape(text):
        """A printf format of the module as a pattern for the line it writes."""
        pattern = re.escape(text)
        for conversion, written in (("0x%08x", "0x[0-9a-f]{8}"), ("%zu", r"\d+"), ("%llu", r"\d+"), ("%d", r"-?\d+"), ("%s", ".*")):
            pattern = pattern.replace(re.escape(conversion), written)
        return pattern
    for line in (sim.AG_SOUND_BANK, sim.AG_SOUND_NO_BANK, sim.AG_SOUND_OPEN, sim.AG_SOUND_FAILED, sim.AG_PULSE):
        assert [text for text in formats if re.fullmatch(shape(text), line)], line
    # and each is read back as what it says
    assert ob.ag_sound(sim.AG_SOUND_BANK)["recordings"] == {"files": 47, "served": 28, "silent": 0}
    assert ob.ag_sound(sim.AG_SOUND_OPEN)["port"] == "open" and ob.ag_sound(sim.AG_SOUND_FAILED)["port"] == "failed"
    assert ob.ag_sound(sim.AG_PULSE)["vibration"] == [sim.AG_PULSE]


def test_simulator_models_the_sound_files_and_the_two_second_look():
    def started(files=(), **scenario):
        """A launched app with the interface on Home; the given files were there before the switch."""
        clock = sim.SimClock()
        console = sim.SimConsole(clock, echo=lambda line: None, **scenario)
        console.controller("launch", ob.TITLE)
        for path in (*files, ob.AFTERGLOW_ON):
            console.put(path, b"x")
        wait(console, clock, 8)
        return console, clock

    def wait(console, clock, seconds):
        for _ in range(int(seconds * 2)):  # the app takes a step with every console operation
            clock.sleep(0.5)
            console.size(ob.STATUS_FILE)

    def key(console, clock, name):
        console.put(ob.CMD_FILE, f"key {name}\n".encode())
        wait(console, clock, 0.5)
        return console.files[ob.LOG_FILE].decode()

    silencers = (ob.AFTERGLOW_SOUND_OFF, ob.AFTERGLOW_VIBRATION_OFF)
    console, clock = started(silencers)
    log = key(console, clock, "right")
    assert sim.AG_SOUND_BANK in log and log.index(sim.AG_SOUND_BANK) < log.index("afterglow: interface up")  # read whatever the files say
    assert "afterglow: sound: port" not in log and "afterglow: vibration" not in key(console, clock, "up")  # silent; a refused move, no pulse
    console.delete(ob.AFTERGLOW_SOUND_OFF)
    console.size(ob.STATUS_FILE)
    assert "afterglow: sound: port" not in key(console, clock, "left")  # the app has not looked yet
    wait(console, clock, sim.AG_SOUND_LOOK_SECONDS)
    assert key(console, clock, "l1").count(sim.AG_SOUND_OPEN) == 0  # the button Home ignores has no cue
    assert key(console, clock, "right").count(sim.AG_SOUND_OPEN) == 1  # the first cue after the look opens the port
    assert key(console, clock, "left").count(sim.AG_SOUND_OPEN) == 1 and sim.AG_PULSE not in key(console, clock, "up")
    console.delete(ob.AFTERGLOW_VIBRATION_OFF)
    wait(console, clock, sim.AG_SOUND_LOOK_SECONDS + 1)
    assert key(console, clock, "up").count(sim.AG_PULSE) == 1 and key(console, clock, "up").count(sim.AG_PULSE) == 1
    # with no file before the switch the welcome under the opening is the first voice: what the stage's order prevents
    console, clock = started()
    assert sim.AG_SOUND_OPEN in console.files[ob.LOG_FILE].decode()
    console, clock = started(ag_sound="fails")
    assert sim.AG_SOUND_FAILED in console.files[ob.LOG_FILE].decode() and sim.AG_SOUND_OPEN not in console.files[ob.LOG_FILE].decode()
    assert ob.ag_place(ob.ag_frame(ob.parse_ui(console.files[ob.UI_FILE]))) == ("topics", 0)  # silent, and it works on
    # the app that is gone with the port: no title, a status and a snapshot that stand still, a command nobody takes
    console, clock = started(silencers, ag_sound="dies")
    before = ob.ag_frames(ob.ag_frame(ob.parse_ui(console.files[ob.UI_FILE])))
    console.delete(ob.AFTERGLOW_SOUND_OFF)
    wait(console, clock, sim.AG_SOUND_LOOK_SECONDS + 1)
    rising = ob.ag_frames(ob.ag_frame(ob.parse_ui(console.files[ob.UI_FILE])))
    assert rising > before and console.running == [ob.TITLE]
    key(console, clock, "right")
    status_then = console.files[ob.STATUS_FILE]
    wait(console, clock, 3)
    assert console.running == [] and console.files[ob.STATUS_FILE] == status_then
    assert ob.ag_frames(ob.ag_frame(ob.parse_ui(console.files[ob.UI_FILE]))) == rising and "afterglow: sound: port" not in console.files[ob.LOG_FILE].decode()
    console.put(ob.CMD_FILE, b"key left\n")
    assert console.size(ob.CMD_FILE) is not None


def test_sound_is_silenced_before_the_switch_and_let_go_only_as_the_last_step(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch)
    assert verdict == "pass", (out / "session.log").read_text()
    # both files of the sound module before the switch, with nothing in between; the sound file is the first to go
    # again, by the sound step; the switch and the last file by the switching off
    assert file_moves(ops) == [("ftp-put", "afterglow-sound.off"), ("ftp-put", "afterglow-vibration.off"), ("ftp-put", "afterglow.on"),
                               ("ftp-delete", "afterglow-sound.off"), ("ftp-delete", "afterglow.on"),
                               ("ftp-delete", "afterglow-vibration.off")]
    first = next(index for index, op in enumerate(ops) if op["kind"] == "ftp-put" and ob.AFTERGLOW_SOUND_OFF in op["detail"])
    assert [op["kind"] for op in ops[first:first + 3]] == ["ftp-put"] * 3
    # every other step of the stage has sent what it sends before the sound is let go; after it only the focus moves
    released = next(index for index, op in enumerate(ops) if op["kind"] == "ftp-delete" and ob.AFTERGLOW_SOUND_OFF in op["detail"])
    assert ag_sent(ops[:released]) == AG_KEYS[:-len(SOUND_KEYS)]
    assert [line for line in commands(ops[released:]) if line not in ("key l1", "quit")] == SOUND_KEYS
    steps = list(ag_steps(current))
    assert steps[-4:] == [SOUND_STEP, "(g) afterglow.on removed, nothing else written", "(g) the classic browser draws again", REMOVED_STEP]
    assert steps.index("(f) the interface shown again is Afterglow") == len(steps) - 5
    # the app's log: the recordings with the interface, the port only after the last key of the playback
    log = console.files[ob.LOG_FILE].decode()
    assert log.count(sim.AG_SOUND_BANK) == 1 and log.count(sim.AG_SOUND_OPEN) == 1 and "afterglow: vibration" not in log
    assert log.index(sim.AG_SOUND_BANK) < log.index("afterglow: interface up") < log.rindex("bc REMOTE_CMD key cross") < log.index(sim.AG_SOUND_OPEN)
    # the record
    step, report = sound_step(current), current.obs["afterglow"]["sound"]
    assert step["result"] == "PASS" and step["gate"] == "soft" and "the port opened" in step["detail"] and sim.AG_SOUND_OPEN in step["detail"]
    assert "47 recordings, 28 cues served, 0 silent" in step["detail"] and "the frame goes on: frames" in step["detail"]
    assert "no line of the port while the file was in place" in step["detail"]
    numbers = step["numbers"]
    assert (numbers["port"], numbers["presses"], numbers["recordings"], numbers["cues_served"], numbers["cues_silent"]) == ("open", 4, 47, 28, 0)
    assert numbers["frames_after"] > numbers["frames_before"] > 0
    assert report["port"] == "open" and report["handle"] == 3 and report["silent_while_off"] and report["advancing"] is True
    assert report["keys"] == ["right", "left", "up", "down"] and not report["lost"] and report["vibration"] == [] and report["other"] == []
    assert sim.AG_SOUND_OPEN in " ".join(current.obs["afterglow"]["log"]) and (out / "afterglow/after-sound.json").is_file()
    assert not [path for path in OVERRIDES if path in console.files]
    left_as_found(current, console, ops, lock)


def test_port_that_does_not_open_is_a_note_and_the_interface_works_on(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, ag_sound="fails")
    steps, step, report = ag_steps(current), sound_step(current), current.obs["afterglow"]
    assert verdict == "pass", (out / "session.log").read_text()  # the sound step is never a gate
    assert step["result"] == "NOTE" and step["gate"] == "soft" and step["numbers"]["port"] == "failed"
    assert "THE PORT DID NOT OPEN" in step["detail"] and sim.AG_SOUND_FAILED in step["detail"] and "the frame goes on: frames" in step["detail"]
    assert report["result"] == "partial" and report["failed"] == [] and report["hard_failed"] == [] and report["notes"] == ASKED + [SOUND_STEP]
    assert report["sound"]["port"] == "failed" and report["sound"]["advancing"] is True and not report["sound"]["lost"]
    assert ag_sent(ops) == AG_KEYS  # every press of the step was sent and followed: silent, and it works
    assert steps["(g) the classic browser draws again"] == "PASS" and steps[REMOVED_STEP] == "PASS"
    assert current.stages[6]["result"] == "PASS" and "Afterglow PARTIAL" in (out / "session.log").read_text().splitlines()[-1]
    left_as_found(current, console, ops, lock)


def test_stalled_frame_before_sound_keeps_it_silent_until_switch_off(tmp_path, monkeypatch):
    original = AfterglowStage.sound

    def stalled(self, label):
        with monkeypatch.context() as patch:
            patch.setattr(ob, "pc_ag_beat", lambda before, after: (False, "the frame's counter stands at 855"))
            return original(self, label)

    monkeypatch.setattr(AfterglowStage, "sound", stalled)
    _, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch)
    step = sound_step(current)
    assert step["result"] == "NOTE" and "before enabling sound" in step["detail"]
    assert "stays in place until the interface is switched off" in step["detail"]
    assert "THE INTERFACE IS LOST after the sound was let go" not in step["detail"]
    assert ag_sent(ops) == AG_KEYS[:-len(SOUND_KEYS)]
    assert sim.AG_SOUND_OPEN not in console.files[ob.LOG_FILE].decode()
    moves = file_moves(ops)
    off = moves.index(("ftp-delete", "afterglow.on"))
    assert moves.index(("ftp-delete", "afterglow-sound.off")) > off
    left_as_found(current, console, ops, lock)


def test_package_without_recordings_never_asks_for_the_port_and_the_note_says_why(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_sound="mute")
    step, report = sound_step(current), current.obs["afterglow"]
    assert verdict == "pass" and step["result"] == "NOTE" and report["notes"] == ASKED + [SOUND_STEP] and report["failed"] == []
    assert "no line of the port in the app's log" in step["detail"] and "0 recordings, 0 cues served, 28 silent: no cue has a recording" in step["detail"]
    assert report["sound"]["port"] is None and report["sound"]["advancing"] is True and not report["sound"]["lost"]
    assert report["sound"]["recordings"] == {"files": 0, "served": 0, "silent": 28} and ag_sent(ops) == AG_KEYS
    left_as_found(current, console, ops, lock)


def test_app_that_dies_with_the_port_is_said_lost_and_every_file_is_still_removed(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, ag_sound="dies")
    steps, step, report = ag_steps(current), sound_step(current), current.obs["afterglow"]
    assert step["result"] == "NOTE" and step["gate"] == "soft" and "THE INTERFACE IS LOST after the sound was let go" in step["detail"]
    assert "the focus did not follow right" in step["detail"] and "the frame's counter stands at" in step["detail"]
    assert "no line of the port in the app's log" in step["detail"] and "47 recordings" in step["detail"]
    assert report["sound"]["lost"] and report["sound"]["advancing"] is False and report["sound"]["port"] is None
    assert report["sound"]["frames"][0] == report["sound"]["frames"][1] and not (out / "afterglow/after-sound.json").exists()
    # one press and no more: nothing is sent to an app that is gone, by the stage or by the teardown
    assert ag_sent(ops) == AG_KEYS[:-len(SOUND_KEYS)] + ["key right"] and payloads(ops) == ["launch"] and "quit" not in commands(ops)
    # the sound step is a NOTE; what fails the session is the gate that was always there: the classic browser is not back
    assert SOUND_STEP in report["notes"] and SOUND_STEP not in report["failed"]
    assert report["hard_failed"] == ["(g) the classic browser draws again"] and verdict == "fail"
    assert steps["(g) afterglow.on removed, nothing else written"] == "PASS" and steps[REMOVED_STEP] == "PASS"
    assert file_moves(ops)[3:] == [("ftp-delete", "afterglow-sound.off"), ("ftp-delete", "afterglow.on"), ("ftp-delete", "afterglow-vibration.off")]
    assert not [path for path in OVERRIDES if path in console.files]
    assert current.teardown_clean is True and not lock.exists() and console.files[ob.STATE_FILE] == ORIGINAL and console.exit_asked == 0
    assert not {Path(path).name for path in console.files} & set(ob.DEV_FILES)


@pytest.mark.parametrize("scenario, verdict, sound", [
    ({"afterglow": "never"}, "fail", None),             # the interface never appears: there is no sound step
    ({"afterglow": "unwritable"}, "fail", None),        # the switch is refused after both of the module's files were taken
    ({"ag_rail_closes": False}, "pass", "NOTE"),        # the interface is not known to be on Home any more
    ({"ag_back": False}, "pass", "NOTE"),               # the interface does not come back after the player
    ({"ag_home": "offline"}, "pass", "NOTE"),           # Home has nothing to move the focus on
    ({"ag_hang_on": "square"}, "unresolved", "NOTE"),   # the app takes no more commands and is left running
])
def test_every_file_the_stage_wrote_is_removed_on_each_way_out_and_the_sound_stays_off(tmp_path, monkeypatch, scenario, verdict, sound):
    said, current, console, ops, lock, out = ag_run(tmp_path, monkeypatch, **scenario)
    steps, moves = ag_steps(current), file_moves(ops)
    assert said == verdict and steps.get(SOUND_STEP) == sound and steps[REMOVED_STEP] == "PASS", (out / "session.log").read_text()
    assert not [path for path in OVERRIDES if path in console.files]
    assert moves[:3] == [("ftp-put", "afterglow-sound.off"), ("ftp-put", "afterglow-vibration.off"), ("ftp-put", "afterglow.on")]
    # a sound its own step did not let go stays off as long as the interface may draw: the switch is removed first
    assert moves[3:] == [("ftp-delete", "afterglow.on"), ("ftp-delete", "afterglow-sound.off"), ("ftp-delete", "afterglow-vibration.off")]
    log = console.files[ob.LOG_FILE].decode()
    assert "afterglow: sound: port" not in log and "afterglow: vibration" not in log
    assert "sound" not in current.obs["afterglow"] and SOUND_STEP not in current.obs["afterglow"]["failed"]
    if sound:  # said in the record, with the bytes it was looking at
        detail = sound_step(current)["detail"]
        assert "afterglow-sound.off stays in place until the interface is switched off" in detail and "unjudged-" in detail
    if verdict != "unresolved":
        left_as_found(current, console, ops, lock)


def test_hard_failure_before_it_does_not_cost_the_sound_step_while_home_is_still_there(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, ag_play="refused")
    steps, report = ag_steps(current), current.obs["afterglow"]
    assert verdict == "fail" and report["hard_failed"] == ["(f) Cross on a video starts playback"]
    assert steps[SOUND_STEP] == "PASS" and report["sound"]["port"] == "open" and steps[REMOVED_STEP] == "PASS"
    assert ag_sent(ops)[-len(SOUND_KEYS):] == SOUND_KEYS and file_moves(ops)[3][1] == "afterglow-sound.off"
    left_as_found(current, console, ops, lock)


def test_switch_is_not_written_when_the_sound_cannot_be_silenced(tmp_path, monkeypatch):
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, unwritable=(ob.AFTERGLOW_SOUND_OFF,))
    steps = ag_steps(current)
    assert verdict == "fail" and list(steps.values()) == ["PASS", "FAIL", "PASS"] and list(steps)[2] == REMOVED_STEP
    said = next(step["detail"] for step in current.steps if step["step"] == "(a) switched on: the frame draws")
    assert "afterglow-sound.off was not written" in said and "the interface is not switched on without it" in said
    # neither the other file nor the switch follows a silencer the console did not take
    assert file_moves(ops) == [("ftp-put", "afterglow-sound.off"), ("ftp-delete", "afterglow-sound.off")]
    assert b"afterglow: " not in console.files[ob.LOG_FILE] and ag_sent(ops) == []  # the interface was never built
    assert current.obs["afterglow"]["hard_failed"] == ["(a) switched on: the frame draws"]
    assert payloads(ops) == ["launch", "close"] and current.teardown_clean is True and not lock.exists()
    assert console.files[ob.STATE_FILE] == ORIGINAL and not [path for path in OVERRIDES if path in console.files]


def test_sound_files_left_by_an_earlier_run_are_set_aside_and_put_back(tmp_path, monkeypatch):
    mine = {ob.AFTERGLOW_SOUND_OFF: b"mine\n", ob.AFTERGLOW_VIBRATION_OFF: b"mine too\n"}
    verdict, current, console, ops, lock, _ = ag_run(tmp_path, monkeypatch, files=dict(mine))
    assert verdict == "pass" and sound_step(current)["result"] == "PASS" and not lock.exists()
    assert {path: console.files[path] for path in mine} == mine and ob.AFTERGLOW_ON not in console.files
    assert {"afterglow-sound.off", "afterglow-vibration.off"} <= set(current.obs["restore"]["restored"])
    assert len([note for note in current.notes if "was already in the data folder" in note]) == 2


def test_dry_run_shows_the_sound_files_in_the_order_a_real_run_uses(tmp_path, monkeypatch, capsys):
    code, printed = dry_run(tmp_path, monkeypatch, capsys)
    assert code == 0 and f"PASS {SOUND_STEP}" in printed and f"PASS {REMOVED_STEP}" in printed
    plan = (tmp_path / "out/console-operations.txt").read_text().splitlines()
    wanted = [f"ftp-put {ob.AFTERGLOW_SOUND_OFF} (Afterglow override: no sound", f"ftp-put {ob.AFTERGLOW_VIBRATION_OFF} (Afterglow override: no vibration",
              f"ftp-put {ob.AFTERGLOW_ON} (Afterglow override: on", f"ftp-delete {ob.AFTERGLOW_SOUND_OFF}", f"ftp-delete {ob.AFTERGLOW_ON}",
              f"ftp-delete {ob.AFTERGLOW_VIBRATION_OFF}"]
    places = [[index for index, line in enumerate(plan) if line.startswith(text)] for text in wanted]
    assert all(len(found) == 1 for found in places) and places == sorted(places)  # each once, in this order
    assert places[1][0] == places[0][0] + 1 and places[2][0] == places[1][0] + 1  # nothing between the silencers and the switch
    last = max(index for index, line in enumerate(plan) if "command: key cross" in line or "command: shot" in line)
    assert last < places[3][0]  # the sound is let go after the last capture and the last press of the playback
    between = [line for line in plan[places[3][0]:places[4][0]] if "command: " in line]
    assert between and all(line.split("command: ")[1].split(";")[0] in SOUND_KEYS + ["key l1"] for line in between)
    listed = printed[printed.index("DRY RUN - console operations a real run performs"):]
    assert [listed.index(text) for text in wanted] == sorted(listed.index(text) for text in wanted)  # and that is what is printed


# --- the development capture command of the instrumented build (scripts/prepare_native.py) ------

def dev_capture_block():
    """The delimited block of prepare_native.py, run with a recorder in place of the overlay's replace()."""
    source = (ROOT / "scripts/prepare_native.py").read_text()
    begin, end = "    # --- ProsperoTube dev capture: begin", "    # --- ProsperoTube dev capture: end"
    assert source.count(begin) == 1 and source.count(end) == 1
    block = source[source.index(begin):source.index(end)]
    applied = []
    exec("if True:\n" + block, {"replace": lambda *arguments: applied.append(arguments), "remote": "src/evo_usb_remote.c"})  # noqa: S102
    return applied


def test_dev_capture_block_only_touches_the_file_a_release_build_compiles_to_nothing():
    applied = dev_capture_block()
    assert [arguments[0] for arguments in applied] == ["src/evo_usb_remote.c"] * 2
    function, command = applied  # two insertions, each before its anchor
    assert function[2].endswith(function[1]) and "static void tube_dev_capture(void)" in function[2]
    assert ob.AG_CAPTURE_MARK.decode() in function[2]  # what the session looks for in the candidate's eboot
    assert command[2].endswith(command[1]) and 'strcmp(buf, "shot") == 0' in command[2]
    upstream = ROOT / "third_party/evo/projects/evoplayer/src/evo_usb_remote.c"
    if not upstream.is_file():
        pytest.skip("the EVO submodule is not checked out here")
    text = upstream.read_text()
    guard = "#if defined(EVO_USB_REMOTE) && defined(EVO_APP_MODULE)"
    assert text.count(guard) == 1 and text.rstrip().endswith("#endif /* EVO_USB_REMOTE && EVO_APP_MODULE */")
    body = text[text.index(guard):]
    assert body.count("#if") == body.count("#endif")  # one region to the end of the file
    for _, old, _ in applied:
        assert body.count(old) == 1  # both anchors lie inside it: without --usb-remote none of the block is compiled


def test_dev_capture_command_writes_the_picture_the_session_reads(tmp_path):
    compiler = se.shutil.which("gcc") or se.shutil.which("cc")
    if not compiler:
        pytest.skip("no C compiler")
    source = """
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static const char *g_dir;
static int g_width = 37, g_height = 5;
const char *evo_data_dir(void) { return g_dir; }
void pp_stage_bc(const char *stage, const char *detail) { printf("bc %s %s\\n", stage, detail); }
void evo_agc_runtime_get_size(int *width, int *height) { *width = g_width; *height = g_height; }
void evo_agc_runtime_read_scanout(uint32_t *bgra, int width, int height)
{
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)   /* 0xAARRGGBB: red = x, green = y, blue = x + y */
            bgra[y * width + x] = 0xff000000u | ((uint32_t)(x * 5) << 16) | ((uint32_t)(y * 40) << 8) | (uint32_t)(x + y);
}
static void run_command(const char *line)
{
    char buf[600];
    snprintf(buf, sizeof buf, "%s", line);
    pp_stage_bc("REMOTE_CMD", buf);
    if (strcmp(buf, "stop") == 0) {
        return;
    }
    pp_stage_bc("REMOTE_CMD", "unknown");
}
int main(int argc, char **argv)
{
    g_dir = argv[1];
    run_command("shot");
    run_command("shot");
    g_width = 0;
    run_command("shot");
    g_width = 37;
    g_dir = "/nonexistent-folder";
    run_command("shot");
    return argc == 2 ? 0 : 1;
}
"""
    for path, old, new in dev_capture_block():
        assert source.count(old) == 1
        source = source.replace(old, new)
    (tmp_path / "capture.c").write_text(source)
    built = subprocess.run([compiler, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2", "-o", str(tmp_path / "capture"),
                            str(tmp_path / "capture.c")], capture_output=True, text=True)
    assert built.returncode == 0, built.stderr
    data = tmp_path / "data"
    data.mkdir()
    ran = subprocess.run([str(tmp_path / "capture"), str(data)], capture_output=True, text=True)
    assert ran.returncode == 0, ran.stderr
    assert sorted(item.name for item in data.iterdir()) == ["evo_shot_000.bmp", "evo_shot_001.bmp"]  # the next free name, no part file left
    assert all(ob.SHOT.match(item.name) for item in data.iterdir())
    picture = (data / "evo_shot_000.bmp").read_bytes()
    facts, problem = ob.bmp_facts(picture)
    assert problem is None and (facts["width"], facts["height"], facts["bits"], facts["bottom_up"]) == (37, 5, 24, True)
    assert len(picture) == 54 + facts["stride"] * 5 == int.from_bytes(picture[2:6], "little") and facts["stride"] == 112
    assert ob.pc_capture(facts)[0]
    x, y = 30, 1  # stored bottom-up as blue, green, red
    at = 54 + (5 - 1 - y) * facts["stride"] + x * 3
    assert tuple(picture[at:at + 3]) == (x + y, y * 40, x * 5)
    assert picture == (data / "evo_shot_001.bmp").read_bytes()
    said = ran.stdout
    assert ob.capture_verdict(said.split("bc REMOTE_CMD shot\n")[1]) == ("wrote", f"{data}/evo_shot_000.bmp 37x5")
    assert ob.capture_verdict(said.split("bc REMOTE_CMD shot\n")[3])[0] == "failed" and "no render size" in said
    assert ob.capture_verdict(said.split("bc REMOTE_CMD shot\n")[4]) == ("failed", "cannot write /nonexistent-folder/evo_shot_000.bmp.part")
    assert "REMOTE_CMD unknown" not in said  # the command is known
