#!/usr/bin/env python3
"""One bounded ProsperoTube hardware acceptance session: one install, one
launch, the core path, one clean close. See README.md beside this file.

Run from WSL. The console address comes from --host or PS5_HOST only. Every
console operation goes through the transport object (transport.Console, or the
simulator for --dry-run); this file never opens a connection to the console.
Stops at the first anomaly, never retries a launch or a close, and always runs
the teardown and lock-release path.
"""
import argparse
import contextlib
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import afterglow_stage  # noqa: E402
import observe as ob  # noqa: E402

SESSION_NAME = "ProsperoTube"
INSTALL = f"/data/homebrew/{ob.TITLE}"
SHADOWMOUNT_LOG = "/data/shadowmount/debug.log"
SNAPSHOT_LIMIT = 16 << 20

# Seconds. Every wait in the session is one of these: none is open-ended.
T = {"launch": 45, "entry": 90, "storage_line": 15, "ready": 90, "register": 120, "keyboard": 10,
     "submit": 15, "search": 60, "service": 20, "focus": 5, "rows": 20, "catalog": 30, "activate": 15, "open": 90, "playing": 30,
     "toggle": 6, "picker": 8, "capture": 25, "seek": 20, "eof_margin": 45, "dialog": 8, "stopped": 20,
     "stats": 20, "history": 30, "consume": 10, "park": 40, "release": 60,
     "functional": 20 * 60}


class Stop(Exception):
    """An anomaly, or a reason not to go on. verdict: STOP (no run) or FAIL."""

    def __init__(self, reason, verdict="FAIL"):
        super().__init__(reason)
        self.reason, self.verdict = reason, verdict


class WallClock:
    now = staticmethod(time.monotonic)
    sleep = staticmethod(time.sleep)


def sha(data):
    return hashlib.sha256(data).hexdigest()


class Session:
    def __init__(self, args, console, clock, down_error):
        self.args, self.console, self.clock, self.Down = args, console, clock, down_error
        self.out = Path(args.out)
        self.stages, self.steps, self.notes, self.obs = [], [], [], {}
        self.lock_path = None
        self.idle = self.launched = self.console_down = self.busy = False
        self.teardown_clean = None
        self.snapshot, self.listing_before, self.shots_before = {}, None, set()
        self.errors_before = None
        self.log_pos, self.log_text = 0, ""
        self.last_ui_seq = None
        self.functional_deadline = None
        self.afterglow_touched = False  # the Afterglow stage wrote its override file: the teardown removes it

    # --- logging -----------------------------------------------------------------
    def log(self, message):
        line = f"{time.strftime('%H:%M:%S', time.gmtime())}Z {message}"
        print(line, flush=True)
        with open(self.out / "session.log", "a") as handle:
            handle.write(line + "\n")

    def note(self, message):
        self.notes.append(message)
        self.log(f"   NOTE {message}")

    def save(self, name, data):
        path = self.out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(data, (dict, list)):
            data = json.dumps(data, indent=1, sort_keys=True).encode()
        path.write_bytes(data if isinstance(data, bytes) else str(data).encode())
        return path

    @contextlib.contextmanager
    def stage(self, number, name):
        self.log(f"== stage {number}: {name}")
        entry = {"stage": number, "name": name, "result": "PASS", "detail": ""}
        self.stages.append(entry)
        try:
            yield
        except Stop as stop:
            entry.update(result=stop.verdict, detail=stop.reason)
            self.log(f"== stage {number}: {stop.verdict} - {stop.reason}")
            raise
        except self.Down as down:
            entry.update(result="CONSOLE-DOWN", detail=str(down))
            self.log(f"== stage {number}: CONSOLE NOT ANSWERING - {down}")
            raise
        else:
            self.log(f"== stage {number}: PASS")

    def step(self, label, probe, timeout, interval=1.0, record=True):
        """Wait, bounded, for an observed postcondition. Never a sleep as proof."""
        start = self.clock.now()
        while True:
            ok, detail = probe()
            waited = self.clock.now() - start
            if ok:
                self.log(f"   PASS {label}: {detail} ({waited:.0f} s)")
                self.steps.append({"step": label, "result": "PASS", "detail": str(detail), "seconds": round(waited, 1)})
                return detail
            over = self.functional_deadline is not None and self.clock.now() > self.functional_deadline
            if waited >= timeout or over:
                reason = f"{label}: not observed within {timeout} s (last: {detail})"
                if record:
                    self.steps.append({"step": label, "result": "FAIL", "detail": str(detail), "seconds": round(waited, 1)})
                raise Stop(reason + (" [functional time budget used up]" if over else ""))
            self.clock.sleep(interval)

    # --- observation ---------------------------------------------------------------
    def status(self):
        status = ob.parse_status(self.console.read(ob.STATUS_FILE))
        if status:
            with open(self.out / "status.jsonl", "a") as handle:
                handle.write(json.dumps(status) + "\n")
        return status

    def ui(self):
        snapshot = ob.parse_ui(self.console.read(ob.UI_FILE))
        if snapshot and snapshot["seq"] != self.last_ui_seq:
            self.last_ui_seq = snapshot["seq"]
            with open(self.out / "ui.jsonl", "a") as handle:
                handle.write(json.dumps(snapshot) + "\n")
        return snapshot

    def read_log(self):
        """Append what the app wrote to its log since the last read (this launch only)."""
        size = self.console.size(ob.LOG_FILE)
        if size is None:
            return self.log_text
        if size < self.log_pos:  # rotated: a new file
            self.log_pos = 0
        if size > self.log_pos:
            chunk = self.console.read(ob.LOG_FILE, self.log_pos) or b""
            self.log_pos += len(chunk)
            self.log_text += chunk.decode("utf-8", "replace")
            with open(self.out / "console.log", "ab") as handle:
                handle.write(chunk)
        return self.log_text

    def command(self, line):
        """One development command: written verified under a temporary name, renamed
        into place, and taken (deleted) by the app before the session goes on."""
        shown = line
        if self.console.size(ob.CMD_FILE) is not None:
            raise Stop(f"an earlier command is still unread; not sending \"{shown}\"")
        code, output = self.console.put(ob.CMD_FILE, (line + "\n").encode(), note=f"command: {shown}")
        if code != 0:
            raise Stop(f"command \"{shown}\" was not written: {output[-200:]}")
        start = self.clock.now()
        while self.console.size(ob.CMD_FILE) is not None:
            if self.clock.now() - start >= T["consume"]:
                raise Stop(f"the app did not take the command \"{shown}\" within {T['consume']} s")
            self.clock.sleep(0.5)
        self.log(f"   sent: {shown}")

    def key(self, name, label, probe, timeout):
        self.command(f"key {name}")
        return self.step(label, probe, timeout)

    # --- stage 0 -------------------------------------------------------------------
    def local_problem(self, reason, verdict="STOP"):
        if self.args.dry_run:
            self.note(f"DRY RUN: a real run would stop here - {reason}")
        else:
            raise Stop(reason, verdict)

    def preconditions(self):
        args = self.args
        try:
            self.candidate = ob.verify_candidate(args.candidate)
        except ob.CandidateError as error:
            raise Stop(f"candidate: {error}", "STOP") from None
        info = self.candidate
        self.log(f"   candidate {info['folder']}: {info['files']} files match SHA256SUMS, eboot {info['eboot'][:16]}, "
                 f"build {info['build']}, development interface {'present' if info['instrumented'] else 'ABSENT'}")
        if not info["build"]:
            raise Stop("no single build identifier in eboot.bin", "STOP")
        if not info["instrumented"]:
            self.local_problem("the candidate was not built with --usb-remote: it cannot be driven or observed")
        if args.afterglow and not info["afterglow"]:
            self.local_problem("--afterglow: the candidate has no Afterglow mount (eboot.bin does not name afterglow.on)")
        elif args.afterglow and not info["capture_command"]:
            self.local_problem("--afterglow: the candidate has no capture command (it was built without the development "
                               "capture block of scripts/prepare_native.py): the picture of Home could not be taken")
        elif args.afterglow:
            self.log("   Afterglow: the mount and the capture command are in eboot.bin")
        protocol = Path(args.protocol)
        for tool in ("scripts/ps5-console.py", "scripts/send-controller.sh"):
            if not (protocol / tool).is_file():
                raise Stop(f"missing {protocol / tool}", "STOP")
        if not shutil.which("nc"):
            self.local_problem("nc (netcat) is not installed: no payload sender, no kernel log")
        for mode in ("launch", "close"):  # both payloads must build before anything is sent
            built = subprocess.run(["bash", str(protocol / "scripts/send-controller.sh"), mode, ob.TITLE, "-", "-"],
                                   env=dict(os.environ, PS5_CONTROLLER_COMPILE_ONLY="1"),
                                   capture_output=True, text=True, timeout=120)
            if built.returncode != 0:
                self.local_problem(f"the {mode} controller does not build: {built.stderr.strip()[-200:]}")
            else:
                self.log(f"   {mode} controller builds for {ob.TITLE}: {built.stdout.strip()}")
        lock = Path(args.lock_file)
        if args.dry_run:
            holder = ob.lock_holder(lock)
            self.log(f"   DRY RUN: the real lock {lock} is {'free' if holder is None else 'HELD by ' + repr(holder)}; "
                     "not touching it, exercising the same code on a scratch file")
            lock = self.out / "dry-run-lock.txt"
        try:
            ob.lock_acquire(lock, SESSION_NAME)
        except ob.LockBusy as busy:
            raise Stop(f"the console lock {lock} is held by \"{busy}\"", "STOP") from None
        except OSError as error:
            raise Stop(f"cannot create the lock {lock}: {error}", "STOP") from None
        self.lock_path = lock
        self.log(f"   lock acquired: {lock} (create-new, content \"{SESSION_NAME}\")")

    # --- stage 1 -------------------------------------------------------------------
    def idle_check(self, when):
        titles = self.console.titles()
        if titles:
            self.busy = True  # someone's session, or our title left open: the console is not ours
            raise Stop(f"{when}: a title is running ({' '.join(titles)}); nothing else is sent", "STOP")
        self.log(f"   {when}: no title is running")

    def title_running(self, wanted):
        titles = self.console.titles()
        return (ob.TITLE in titles) == wanted, f"running: {' '.join(titles) or 'none'}"

    # --- stage 2 -------------------------------------------------------------------
    def walk(self, path, depth=5):
        entries = self.console.listdir(path)
        if entries is None:
            return None
        tree = {}
        for name, facts in sorted(entries.items()):
            if facts["type"] == "dir" and depth > 0:
                tree[name + "/"] = self.walk(f"{path}/{name}", depth - 1)
            else:
                tree[name] = facts["size"]
        return tree

    def take_snapshot(self):
        self.errors_before = self.console.errors()
        self.obs["mounted_from_before"] = self.console.where(ob.TITLE)
        self.log(f"   {self.errors_before}; {ob.TITLE} mounted from: {self.obs['mounted_from_before']}")
        self.listing_before = self.console.listdir(ob.DATA)
        self.save("data-listing-before.json", self.listing_before if self.listing_before is not None else {"absent": True})
        if self.args.afterglow and Path(ob.AFTERGLOW_OFF).name in (self.listing_before or {}):
            raise Stop(f"{ob.AFTERGLOW_OFF} exists: it keeps the classic browser whatever the session writes. "
                       "Remove it, or run without --afterglow", "STOP")
        skipped = []
        for name, facts in sorted((self.listing_before or {}).items()):
            if facts["type"] != "file" or name in ob.DEV_FILES or ob.DIAGNOSTIC.match(name) or \
                    ob.SHOT.match(name) or name.endswith(".upload"):
                continue
            if facts["size"] > SNAPSHOT_LIMIT:
                skipped.append(name)
                continue
            data = self.console.read(f"{ob.DATA}/{name}")
            if data is None or len(data) != facts["size"]:
                raise Stop(f"snapshot of {name}: read {None if data is None else len(data)} of {facts['size']} bytes")
            self.save(f"state-before/{name}", data)
            self.snapshot[name] = sha(data)
        self.save("state-before.sha256.json", self.snapshot)
        if skipped:
            self.note(f"larger than {SNAPSHOT_LIMIT >> 20} MiB, not copied and never written by the session: {skipped}")
        self.log(f"   {ob.DATA}: {'absent' if self.listing_before is None else str(len(self.listing_before)) + ' entries'}; "
                 f"{len(self.snapshot)} state files copied: {sorted(self.snapshot)}")
        if "youtube.json" in self.snapshot:
            self.log(f"   saved provider state: {ob.redact_state((self.out / 'state-before/youtube.json').read_bytes())}")
        installed = self.walk(INSTALL)
        self.save("installed-before.json", installed if installed is not None else {"absent": True})
        self.log(f"   {INSTALL}: {'absent' if installed is None else 'listed'}")

    # --- stage 3 -------------------------------------------------------------------
    def install(self):
        code, output = self.console.install(ob.TITLE, self.candidate["folder"])
        self.save("install.txt", output)
        self.log("   " + (output.splitlines() or ["(no output)"])[0])
        if code != 0:
            raise Stop(f"install refused or failed ({code}): {output[-300:]}", "STOP" if code == 3 else "FAIL")
        weaker = re.search(r"size-only checks (\d+)", output)
        if weaker and int(weaker.group(1)):
            self.note(f"{weaker.group(1)} signed file(s) could only be compared by size (the server returned an ELF view): "
                      "weaker executable verification, as the protocol labels it")
        if self.args.settle > 0:
            code, output = self.console.settle(ob.TITLE, self.candidate["folder"], self.args.settle)
            self.save("settle.txt", output)
            self.log("   " + (output.splitlines() or ["(no output)"])[-1])
            if code != 0:
                raise Stop(f"the install did not survive settling: {output[-300:]}")

    # --- stage 4 -------------------------------------------------------------------
    def launch(self):
        self.idle_check("before launch")
        for name, port in (("kernel log", self.console.klog_port), ("payload receiver", self.console.elf_port)):
            if not self.console.port_open(port):
                raise Stop(f"{name} port {port} is not answering: not launching", "STOP")
        try:
            self.step("registration: mounted from the installed folder",
                      lambda: (lambda where: (where.startswith(f"folder {INSTALL}"), where))(self.console.where(ob.TITLE)),
                      T["register"], interval=10)
        except Stop as stop:
            raise Stop(f"{stop.reason}: not launching an unregistered title", "STOP") from None
        listing = self.console.listdir(ob.DATA) or {}
        for name in ob.DEV_FILES:
            if name in listing:  # left by an earlier development run: it would be read as this one's
                self.console.delete(f"{ob.DATA}/{name}")
        for name in ob.AFTERGLOW_FILES if self.args.afterglow else ():
            if name in listing:  # the app would start in Afterglow and the classic core path could not run
                self.console.delete(f"{ob.DATA}/{name}")
                self.note(f"{name} was already in the data folder: removed for this launch"
                          + (", put back with the saved state at the end" if name in self.snapshot else ""))
        self.shots_before = {name for name in listing if ob.SHOT.match(name)}
        self.log_pos = self.console.size(ob.LOG_FILE) or 0
        self.sm_pos = self.console.size(SHADOWMOUNT_LOG) or 0
        self.console.klog_start(str(self.out / "klog.txt"))
        self.launched = True  # from here the teardown looks for the title, delivered or not
        code, output = self.console.controller("launch", ob.TITLE)
        if code != 0:
            raise Stop(f"the launch payload was not delivered ({code}): {output[-200:]}; no second attempt")
        self.log("   launch payload delivered once")
        self.step("launch: the title has a sandbox", lambda: self.title_running(True), T["launch"], interval=3)
        build = self.candidate["build"]

        def entered():
            status = self.status()
            if status and status["build"] != build:
                raise Stop(f"the running build is {status['build']}, not the candidate's {build}")
            return bool(status), f"build {build}" if status else "no status file yet"
        self.step("entry: status written by the candidate build", entered, T["entry"], interval=2)

        start = self.clock.now()
        while True:
            verdict = ob.storage_verdict(self.read_log())
            if verdict == "failed":
                raise Stop("Lapy storage failed (see console.log)")
            if verdict or self.clock.now() - start >= T["storage_line"]:
                break
            self.clock.sleep(2)
        self.log(f"   PASS storage: status file written under {ob.DATA}"
                 + (", log says storage=ready" if verdict else ""))
        if not verdict:
            self.note("the Lapy storage line was not seen in console.log; the status file under /data is the evidence")
        beat = []

        def ready():
            beat.append(self.status())
            ok, detail = ob.pc_heartbeat(beat[-2] if len(beat) > 1 else None, beat[-1], build)
            if not ok:
                return False, detail
            drawn, what = ob.pc_browser(self.ui())
            return drawn, f"{detail}; {what}"
        self.step("ready: frame loop advancing, browser drawn", ready, T["ready"])

    # --- stage 5 -------------------------------------------------------------------
    # Hard gates are facts the engine reports whatever draws the screen: the
    # status line (screen number, decoder, position), the provider's log lines
    # and the saved state. What only the UI snapshot can show is observed and
    # recorded ("soft"): a miss is noted and the session goes on.

    def on_screen(self, number):
        status = self.status()
        return bool(status) and status["scr"] == number, f"scr={status['scr']}" if status else "no status"

    def soft(self, label, action):
        """Run an observe-and-record step. Returns its result, or None on a miss."""
        try:
            return action()
        except Stop as stop:
            for entry in reversed(self.steps):
                if entry["result"] == "FAIL":
                    entry["result"] = "SOFT-MISS"
                    break
            self.soft_misses.append(label)
            self.note(f"SOFT MISS {label}: {stop.reason}")
            return None

    def focus_row(self, pattern, what):
        """Move the cursor onto the row whose text matches, by the position the
        snapshot reports after every press. Cross is never pressed on a row
        whose text was not read back as the wanted one."""
        def located():
            snapshot = self.ui()
            ok = bool(ob.find_row(snapshot, pattern)) and int(ob.ui_focus(snapshot).get("index") or 0) > 0
            return ok and not ob.keyboard_open(snapshot), ob.ui_line(snapshot)
        self.step(f"{what}: a row matching /{pattern}/ is listed", located, T["rows"])
        columns, blocked = None, set()
        for _ in range(16):
            snapshot = self.ui()
            focus = ob.ui_focus(snapshot)
            current, target = int(focus.get("index") or 0), ob.find_row(snapshot, pattern)
            if not current or not target:
                break
            if current == target:
                if re.search(pattern, str(focus.get("text", "")), re.IGNORECASE):
                    self.log(f"   {what}: cursor on row {current}: \"{focus.get('text')}\" [{ob.focus_kind(snapshot) or 'kind not shown'}]")
                    return focus
                break
            key = ob.next_key(current, target, columns, blocked)
            if key is None:
                break
            self.command(f"key {key}")
            start, moved = self.clock.now(), current
            while moved == current and self.clock.now() - start < T["focus"]:
                self.clock.sleep(0.5)
                moved = int(ob.ui_focus(self.ui()).get("index") or 0) or current
            if moved == current:
                blocked.add(key)
            else:
                blocked.clear()
                if key in ("down", "up") and columns is None:
                    columns = abs(moved - current)
        raise Stop(f"{what}: the cursor did not reach a row matching /{pattern}/: {ob.ui_line(self.ui())}")

    def open_row(self, pattern, what, network, timeout):
        """Cross on the matching row; the provider's log must show the list it opened."""
        self.focus_row(pattern, what)
        seen = len(ob.catalog_lines(self.read_log()))
        self.command("key cross")
        detail = self.step(f"{what}: the provider listed what the row opens", lambda: ob.pc_catalog(self.read_log(), seen, network),
                           timeout, interval=2)
        return detail

    def browse_to_video(self):
        """Discover -> a topic -> the first playable row. No keyboard on this route."""
        args = self.args
        self.open_row(args.root_row, "(c) main menu", False, T["catalog"])
        self.obs["topic_rows"] = ob.ui_rows(self.ui())
        self.obs["service_answer"] = self.open_row(args.topic_row, "(c) topic", True, T["search"])
        self.log(f"   (b,c) the built-in service answered: {self.obs['service_answer']}")

        def cursor_ready():
            snapshot = self.ui()
            return ob.pc_browser(snapshot)[0] and ob.ui_rows(snapshot) != self.obs["topic_rows"], ob.ui_line(snapshot)
        try:
            self.step("(c) the video list is drawn with a cursor", cursor_ready, T["rows"], record=False)
        except Stop:
            # A list drawn without a cursor takes it at the first direction press.
            self.log("   (c) no cursor on the new list yet: one Right to place it")
            self.command("key right")
            self.step("(c) the video list is drawn with a cursor", cursor_ready, T["rows"])
        self.obs["video_rows"] = ob.ui_rows(self.ui())
        for _ in range(6):  # a channel or playlist among the results is a folder: step past it
            if ob.focus_kind(self.ui()) != "folder":
                break
            index = int(ob.ui_focus(self.ui()).get("index") or 0)
            self.key("right", "(d) next result",
                     lambda index=index: (ob.ui_focus(self.ui()).get("index") != index, ob.ui_line(self.ui())), T["focus"])
        if ob.focus_kind(self.ui()) == "folder":
            raise Stop(f"(d) no playable row at the start of the results: {ob.ui_line(self.ui())}")
        focus = ob.ui_focus(self.ui())
        seen = len(ob.activations(self.read_log()))
        self.command("key cross")

        def activated():
            new = ob.activations(self.read_log())[seen:]
            if new and new[-1]["kind"] != "playable":
                raise Stop(f"(d) Cross opened a folder, not a video: {new[-1]}")
            return bool(new), new[-1] if new else "no activation line yet"
        self.video = self.step("(d) the provider activated a playable row", activated, T["activate"])
        self.video["row"] = str(focus.get("text", ""))
        self.obs["video"] = self.video

    def functional(self):
        args = self.args
        self.functional_deadline = self.clock.now() + T["functional"]
        self.soft_misses, self.ui_lost, self.video = [], False, {"id": "", "title": ""}

        # (a) browser
        snapshot = self.ui()
        self.save("browser-ui.json", snapshot)
        self.obs["root_rows"] = ob.ui_rows(snapshot)
        self.log(f"   (a) browser: {ob.ui_line(snapshot)}; rows {self.obs['root_rows']}")
        self.note("no scanout capture at the browser: there the stick click belongs to voice search, "
                  "so the UI snapshot (browser-ui.json) stands in for it")

        # (b) the built-in service: nothing to pair, it must be listening
        self.obs["service"] = self.step("(b) built-in service started", lambda: ob.pc_service(self.read_log()),
                                        T["service"], interval=2)

        # (c) a video without the keyboard, (d) play it
        self.browse_to_video()
        resumed = []

        def opened():
            failed = ob.resolve_failed(self.read_log())
            if failed:
                raise Stop(f"(d) the stream did not resolve: {failed}")
            status = self.status()
            if status and status["scr"] == ob.SCREEN_RESUME and not resumed:
                resumed.append(True)
                self.note("a resume prompt appeared (saved position): accepted with Cross")
                self.command("key cross")
            ok = bool(status) and status["scr"] == ob.SCREEN_PLAYER and status["active"] == 1 and not status["fatal"]
            return ok, f"scr={status['scr']} active={status['active']} fatal={status['fatal']}" if status else "no status"
        self.step("(d) player open, decoder active", opened, T["open"])
        self.playing("(d) position advancing")

        # (e) seek first: a development command, no key, nothing on screen to get wrong
        before = self.status()
        if not before:
            raise Stop("(e) no status before the seek")
        self.command("seek +10")
        self.step("(e) +10 s seek landed", lambda: ob.pc_seek(before["pos"], self.status(), 10.0), T["seek"])

        # (e) pause, (e) captions, (h) capture, (f) options: observed and recorded
        window = []
        paused = self.soft("(e) pause", lambda: self.toggle(
            "(e) paused", lambda: (window.append(self.status()), ob.pc_paused(window, self.ui()))[1])) is not None
        self.soft("(e) caption picker", lambda: self.visit("down", ob.SCREEN_CAPTIONS, "(e) caption picker", self.read_captions))
        if not self.ui_lost:
            self.soft("(h) capture", self.capture)
            self.soft("(f) playback options", lambda: self.visit("r2", ob.SCREEN_OPTIONS, "(f) playback options", self.read_options))
        if paused and not self.ui_lost:
            self.command("key cross")
            try:
                self.playing("(e) resumed, position advancing", timeout=T["toggle"] + 4, record=False)
            except Stop:
                self.log("   (e) not resumed after one press; pressing once more")
                self.command("key cross")
                self.playing("(e) resumed, position advancing", timeout=T["toggle"] + 4)

        # (g) end, statistics, history
        now = self.status() or before
        remaining = now["dur"] - now["pos"]
        mode = args.end if args.end != "auto" else ("eof" if 0 < remaining <= 120 and not (paused and self.ui_lost) else "stop")
        self.obs["end_mode"] = mode
        if mode == "eof":
            self.step("(g) played to the end (complete=1)", lambda: ob.pc_complete(self.status()),
                      max(remaining, 0) + T["eof_margin"])
            if self.soft("(g) player retired by itself", lambda: self.step("(g) player retired by itself", self.inactive, T["stopped"])) is None:
                self.command("stop")
                self.step("(g) player retired after the stop command", self.inactive, T["stopped"])
        else:
            self.stop_playback()
        stats = {}

        def statistics():
            stats.update(ob.parse_stats(self.read_log()) or {})
            return ob.pc_stats(stats or None)
        self.step("(d,g) engine statistics for the playback", statistics, T["stats"], interval=2)
        self.obs["stats"] = stats
        self.step(f"(g) history record for {self.video['id']}",
                  lambda: ob.pc_history(self.read_state(), self.video["id"], mode == "eof", self.video["title"]),
                  T["history"], interval=2)
        record = ob.history_record(self.state_bytes, self.video["id"], self.video["title"]) or {}
        self.obs["history"] = {key: record.get(key) for key in ("id", "title", "played", "position", "duration")}
        self.save("state-after/youtube.json", self.state_bytes)
        self.step("(b) the service's address is kept out of the saved state", lambda: ob.pc_state_local(self.read_state()),
                  T["service"], interval=2)

        # optional: the keyboard route, after everything that matters is on record
        if args.with_search and not self.ui_lost:
            self.soft("(c) search by keyboard", self.search)
        self.obs["soft_misses"] = self.soft_misses

    def stop_playback(self):
        """Circle; when the confirmation screen appears, Cross on it (STOP has the
        cursor there). Whatever the interface did, the hard fact is the player
        retired: the development stop command settles it when the keys did not."""
        def by_keys():
            if self.ui_lost:
                raise Stop("the interface is not on the player: keys are not used")
            self.command("key circle")
            self.step("(g) stop confirmation open (screen 20)", lambda: self.on_screen(ob.SCREEN_STOP_DIALOG), T["dialog"])
            self.save("stop-dialog-ui.json", self.ui())
            self.command("key cross")
            return self.step("(g) stopped through the confirmation", self.inactive, T["stopped"])
        if self.soft("(g) stop with confirmation", by_keys) is None:
            if not self.inactive()[0]:
                self.command("stop")
            self.step("(g) player retired after the stop command", self.inactive, T["stopped"])
        self.obs["stopped_by"] = "development stop command" if "(g) stop with confirmation" in self.soft_misses else "confirmation dialog"

    def visit(self, key, screen, label, read):
        """Open a picker from the player, record what it lists, return. If the
        player does not come back, no further key is sent to an unknown screen."""
        self.command(f"key {key}")
        try:
            self.step(f"{label} open (screen {screen})", lambda: self.on_screen(screen), T["picker"])
            result = read(self.ui())
        finally:
            for _ in range(2):
                if self.on_screen(ob.SCREEN_PLAYER)[0]:
                    break
                self.command("key circle")
                start = self.clock.now()
                while not self.on_screen(ob.SCREEN_PLAYER)[0] and self.clock.now() - start < T["picker"]:
                    self.clock.sleep(1)
            else:
                if not self.on_screen(ob.SCREEN_PLAYER)[0]:
                    self.ui_lost = True
                    self.note(f"{label}: the player did not come back after two Circle presses; "
                              "no more keys are sent, the rest uses development commands")
        return result

    def read_captions(self, snapshot):
        rows = ob.ui_rows(snapshot)
        offered, current = ob.caption_tracks(rows)
        self.obs["captions"] = {"docs": ob.ui_docs(snapshot), "rows": rows, "offered": offered, "current": current}
        self.log(f"   (e) caption picker {ob.ui_docs(snapshot)}: offered {offered or 'none read'}; showing {current or 'none read'}")
        return True

    def read_options(self, snapshot):
        rows = ob.ui_rows(snapshot)
        self.obs["options"] = {"docs": ob.ui_docs(snapshot), "rows": rows, "total": ob.ui_focus(snapshot).get("total"),
                               "quality": ob.quality_rows(rows)}
        self.log(f"   (f) playback options {ob.ui_docs(snapshot)}: {rows}; quality entries {self.obs['options']['quality'] or 'none read'}")
        return True

    def search(self):
        """Square, the app's keyboard, a query: every observable recorded."""
        query = self.args.search_query
        searches, lists = len(ob.searches(self.read_log())), len(ob.catalog_lines(self.log_text))
        self.command("key square")
        try:
            self.step("(c) keyboard open", lambda: (ob.keyboard_open(self.ui()), ob.ui_line(self.ui())), T["keyboard"])
            self.command(f"type {query}")
            self.soft("(c) typed text shown", lambda: self.step(
                "(c) the keyboard shows the typed text",
                lambda: (query in str(ob.ui_modal(self.ui()).get("text", "")), str(ob.ui_modal(self.ui()).get("text", ""))[:80]),
                T["focus"]))
            self.command("kbdone")

            def answered():
                new = ob.searches(self.read_log())[searches:]
                if query not in new:
                    return False, f"no Search line for the query yet ({new})"
                return ob.pc_catalog(self.log_text, lists)
            self.obs["search"] = self.step("(c) search submitted and listed", answered, T["search"], interval=2)
            self.obs["search_rows"] = ob.ui_rows(self.ui())
            return True
        finally:
            if ob.keyboard_open(self.ui()):
                self.command("kbclose")

    def toggle(self, label, probe):
        """Cross once; a second press only if the first one visibly did nothing
        (with hidden controls the first press only shows them)."""
        for attempt in (1, 2):
            self.command("key cross")
            try:
                return self.step(label, probe, T["toggle"], record=attempt == 2)
            except Stop:
                if attempt == 2:
                    raise
                self.log(f"   {label}: not yet after one press; pressing once more")

    state_bytes = b""

    def read_state(self):
        self.state_bytes = self.console.read(ob.STATE_FILE) or b""
        return self.state_bytes

    def inactive(self):
        status = self.status()
        ok = bool(status) and status["active"] == 0 and status["scr"] != ob.SCREEN_PLAYER
        return ok, f"scr={status['scr']} active={status['active']}" if status else "no status"

    def playing(self, label, timeout=None, record=True):
        window = []
        return self.step(label, lambda: (window.append(self.status()), ob.pc_playing(window))[1],
                         timeout or T["playing"], record=record)

    def capture(self):
        """One scanout capture on the player (stick click = capture there)."""
        self.command("key l3")
        seen = {}

        def written():
            listing = self.console.listdir(ob.DATA) or {}
            new = sorted(name for name in listing if ob.SHOT.match(name) and name not in self.shots_before)
            if not new:
                return False, "no new evo_shot file"
            name, size = new[-1], listing[new[-1]]["size"]
            stable = size > 54 and seen.get(name) == size
            seen[name] = size
            return stable, f"{name} {size} bytes"
        self.step("(h) scanout capture written", written, T["capture"], interval=2)
        name = max(seen)
        data = self.console.read(f"{ob.DATA}/{name}") or b""
        if data[:2] != b"BM" or len(data) != seen[name]:
            raise Stop(f"(h) capture {name} read back as {len(data)} bytes, not a bitmap of {seen[name]}")
        self.save("playback-scanout.bmp", data)
        self.obs["capture"] = {"file": name, "bytes": len(data), "sha256": sha(data)}
        self.log(f"   (h) capture saved: playback-scanout.bmp ({len(data)} bytes)")
        return True

    # --- stage 6 -------------------------------------------------------------------
    def close_title(self):
        """quit -> parked=1 -> one exact-title close. True when the title is gone."""
        if ob.TITLE not in self.console.titles():
            self.log("   the title is not running: nothing to close")
            return True
        parked = False
        try:
            if self.status():
                self.command("quit")
                self.step("teardown: parked=1 (media released, GPU idle)",
                          lambda: (lambda s: (bool(s) and s["parked"] == 1, f"parked={s['parked']}" if s else "no status"))(
                              self.status()), T["park"])
                parked = True
            else:
                self.log("   no status file: the app cannot be asked to quit")
        except Stop as stop:
            self.log(f"   the app did not park: {stop.reason}")
        if ob.TITLE not in self.console.titles():
            self.log("   the title exited on its own: no close payload sent")
            return True
        if not parked:
            if not self.args.allow_unparked_close:
                self.log("   LEFT RUNNING: the title did not park and --allow-unparked-close was not given; "
                         "no forced close is sent. Close it from the PS5 and tell the owner.")
                return False
            self.log("   WARNING: forced close of a title that did not park (--allow-unparked-close)")
            self.obs["forced_close"] = True
        code, output = self.console.controller("close", ob.TITLE)
        self.log(f"   close payload for {ob.TITLE} sent once (delivery status {code})")
        try:
            self.step("teardown: the title has no sandbox", lambda: self.title_running(False), T["release"], interval=3)
        except Stop as stop:
            self.log(f"   NOT RELEASED: {stop.reason}; no second close")
            return False
        return True

    def restore(self):
        """Development files removed; the copied state put back byte for byte."""
        listing = self.console.listdir(ob.DATA) or {}
        for name in sorted(listing):
            created_shot = ob.SHOT.match(name) and name not in self.shots_before
            # The Afterglow stage removes its own override file; this is for a stage that could not.
            own_switch = self.afterglow_touched and name in ob.AFTERGLOW_FILES
            if name in ob.DEV_FILES or created_shot or own_switch:
                self.console.delete(f"{ob.DATA}/{name}")
        report = {"identical": [], "restored": [], "removed": [], "failed": [], "left": []}
        if self.args.keep_state:
            self.note("--keep-state: the provider state of this session (history) stays on the console")
        else:
            for name, digest in sorted(self.snapshot.items()):
                path = f"{ob.DATA}/{name}"
                now = self.console.read(path)
                if now is not None and sha(now) == digest:
                    report["identical"].append(name)
                    continue
                code, output = self.console.put(path, (self.out / "state-before" / name).read_bytes())
                back = self.console.read(path)
                ok = code == 0 and back is not None and sha(back) == digest
                report["restored" if ok else "failed"].append(name)
            for name in ob.STATE_CREATED:
                if name in listing and name not in (self.listing_before or {}):
                    report["removed" if self.console.delete(f"{ob.DATA}/{name}") else "failed"].append(name)
        after = self.console.listdir(ob.DATA) or {}
        self.save("data-listing-after.json", after)
        known = set(self.listing_before or {})
        report["left"] = sorted(name for name in after if name not in known and not ob.DIAGNOSTIC.match(name))
        leftover = [name for name in after if name in ob.DEV_FILES
                    or (self.afterglow_touched and name in ob.AFTERGLOW_FILES and name not in self.snapshot)]
        self.obs["restore"] = report
        self.log(f"   state: {len(report['identical'])} unchanged, restored {report['restored']}, "
                 f"removed {report['removed']}, failed {report['failed']}; new and left in place: {report['left']}")
        return not report["failed"] and not leftover

    def teardown(self):
        clean = True
        if self.launched:
            gone = self.close_title()
            self.console.klog_stop()
            self.read_log()
            self.release_evidence()
            if not gone:
                return False
        if self.snapshot or self.launched:
            clean = self.restore() and clean
        for name, port in (("kernel log", self.console.klog_port), ("payload receiver", self.console.elf_port)):
            up = self.console.port_open(port)
            self.log(f"   health: {name} port {port} {'answers' if up else 'DOES NOT ANSWER'}")
            clean = clean and up
        errors = self.console.errors()
        if errors != self.errors_before:
            self.log(f"   NEW ERROR RECORD since the session began: {errors} (was: {self.errors_before})")
            self.obs["new_error_record"] = errors
            clean = False
        else:
            self.log(f"   health: error records unchanged ({errors})")
        titles = self.console.titles()
        self.log(f"   final idle check: {'no title is running' if not titles else 'RUNNING ' + ' '.join(titles)}")
        if titles and ob.TITLE not in titles:
            self.note("another title was started by someone else during the teardown: not this session's to judge")
        return clean and ob.TITLE not in titles

    def release_evidence(self):
        """What the logs say about the release. Recorded, not required: the
        release itself is the title's sandbox being gone."""
        klog = self.out / "klog.txt"
        if klog.is_file():
            text = klog.read_text(errors="replace")
            self.obs["klog"] = {"lines": text.count("\n"), "naming_title": sum(ob.TITLE in line for line in text.splitlines())}
        size = self.console.size(SHADOWMOUNT_LOG)
        if size is not None and size > getattr(self, "sm_pos", size):
            piece = (self.console.read(SHADOWMOUNT_LOG, self.sm_pos) or b"").decode("utf-8", "replace")
            self.save("shadowmount.log", piece)
            self.obs["shadowmount_released"] = f"runtime layers released: {ob.TITLE}" in piece
        self.log(f"   release evidence: klog {self.obs.get('klog')}, "
                 f"ShadowMount release line {self.obs.get('shadowmount_released', 'not available')}")

    def release_lock(self):
        if not self.lock_path:
            return
        if self.teardown_clean is False or (self.console_down and self.idle):
            self.log(f"   LOCK KEPT: {self.lock_path} still says \"{SESSION_NAME}\". The console's state is not proven; "
                     "the owner decides when to remove it.")
            return
        released = ob.lock_release(self.lock_path, SESSION_NAME)
        self.log(f"   lock {'released' if released else 'NOT released: its content is no longer ours'}: {self.lock_path}")

    # --- the session ---------------------------------------------------------------
    def run(self):
        verdict = "pass"
        try:
            with self.stage(0, "local preconditions and lock"):
                self.preconditions()
            with self.stage(1, "idle check (first console operation, read-only)"):
                self.idle_check("first check")
                self.idle = True
            with self.stage(2, "snapshot of provider state and installed title"):
                self.take_snapshot()
            with self.stage(3, "verified install and settle"):
                self.install()
            with self.stage(4, "launch and evidence ladder to ready"):
                self.launch()
            core = None
            try:
                with self.stage(5, "core path through the development interface"):
                    self.functional()
            except Stop as stop:
                if not (self.args.afterglow and self.args.afterglow_after_core_fail):
                    raise
                core = stop  # still the session's verdict; the stage itself checks that the app is on its browser
                self.note("the core path failed; --afterglow-after-core-fail: the Afterglow stage is still tried")
            if self.args.afterglow:
                with self.stage("5a", "Afterglow interface (optional stage, same launch)"):
                    afterglow_stage.AfterglowStage(self, Stop).run()
            if core is not None:
                raise core
        except Stop as stop:
            verdict = "no-run" if stop.verdict == "STOP" and not self.launched else "fail"
        except self.Down:
            # Not answering at the very first operation: nothing was sent or changed.
            self.console_down, verdict = True, "unresolved" if self.idle else "no-run"
        except KeyboardInterrupt:
            self.log("interrupted: running the teardown")
            verdict = "fail"
        finally:
            self.functional_deadline = None
            try:
                with self.stage(6, "teardown, state restoration, health, lock"):
                    if self.console_down:
                        raise Stop("the console stopped answering: nothing more is sent, nothing is retried")
                    if self.busy:
                        self.log("   a title is running that this session did not start: no further console operation"
                                 + ("; the verified install stays in place" if self.idle else ""))
                    elif self.idle:
                        self.teardown_clean = self.teardown()
                        if not self.teardown_clean:
                            raise Stop("teardown not proven clean (see above)")
                    else:
                        self.log("   nothing was changed on the console: no console operation in the teardown")
            except Stop:
                if (self.console_down and self.idle) or self.teardown_clean is False:
                    verdict = "unresolved"
            except self.Down:
                self.console_down, verdict = True, "unresolved"
            finally:
                self.console.finish()
                self.release_lock()
        self.finish(verdict)
        return verdict

    def finish(self, verdict):
        plan = [f"{line}{f'  (x{count})' if count > 1 else ''}" for line, count in self.console.ops.plan]
        self.save("result.json", {
            "verdict": verdict, "dry_run": self.args.dry_run, "candidate": getattr(self, "candidate", None),
            "stages": self.stages, "steps": self.steps, "notes": self.notes, "observations": self.obs,
            "console_operations": self.console.ops.count, "teardown_clean": self.teardown_clean,
            "console_down": self.console_down, "launches": int(self.launched)})
        self.save("console-operations.txt", "\n".join(plan) + "\n")
        if self.args.dry_run:
            self.log("DRY RUN - console operations a real run performs, in order (repeats folded):")
            for index, line in enumerate(plan, 1):
                self.log(f"   {index:3d}. {line}")
        afterglow = self.obs.get("afterglow")
        self.log(f"RESULT: {verdict.upper()}{' (DRY RUN, simulated console)' if self.args.dry_run else ''}; "
                 f"stages {' '.join(str(s['stage']) + ':' + s['result'] for s in self.stages)}"
                 + (f"; Afterglow {afterglow['result'].upper()}" if afterglow else "") + f"; evidence: {self.out}")
        if verdict == "unresolved":
            self.log("The console's state is not proven. Do not retry anything: report to the owner.")


EXIT = {"pass": 0, "fail": 1, "no-run": 3, "unresolved": 4}


def parse(argv=None):
    home = Path.home()
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--host", default=os.environ.get("PS5_HOST"), help="console address (or PS5_HOST)")
    parser.add_argument("--lock-file", default=os.environ.get("PS5_LOCK"), help="this console's shared lock file (or PS5_LOCK)")
    parser.add_argument("--candidate", required=True, help="frozen candidate: <dir>/PPSA99142/ and <dir>/SHA256SUMS")
    parser.add_argument("--protocol", default=str(home / "ps5-homebrew-dev-protocol"))
    parser.add_argument("--out", default=str(home / "pt-console" / time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())))
    parser.add_argument("--root-row", default=r"\bDiscover\b", help="main-menu row to open (regular expression on its text)")
    parser.add_argument("--topic-row", default=r"\bScience\b", help="row of that list whose videos are listed by the service")
    parser.add_argument("--with-search", action="store_true",
                        help="after the playback, also search through the app's keyboard (observed, never a gate)")
    parser.add_argument("--search-query", default="Me at the zoo")
    parser.add_argument("--settle", type=int, default=130, help="seconds to watch the install before the second compare")
    parser.add_argument("--end", choices=("auto", "eof", "stop"), default="auto")
    parser.add_argument("--keep-state", action="store_true", help="leave the session's history on the console")
    parser.add_argument("--allow-unparked-close", action="store_true",
                        help="send the close payload even when the app did not park (a forced close)")
    parser.add_argument("--afterglow", action="store_true",
                        help="after the core path, in the same launch: switch the Afterglow interface on, record its frame, "
                             "capture Home, play one video from it, switch it off again")
    parser.add_argument("--afterglow-other", choices=("square", "triangle", "none"), default="square",
                        help="the key that opens one other Afterglow screen from Home (Square: Search; Triangle: a video's page)")
    parser.add_argument("--afterglow-after-core-fail", action="store_true",
                        help="try the Afterglow stage even when the core path failed, if the app is on its browser")
    parser.add_argument("--dry-run", action="store_true", help="simulated console: prints the console operations, sends nothing")
    parser.add_argument("--i-have-owner-approval", action="store_true")
    args = parser.parse_args(argv)
    if not args.i_have_owner_approval:
        parser.error("refusing to run: a console session needs the owner's approval (--i-have-owner-approval)")
    if not args.lock_file:
        parser.error("give the console's lock file with --lock-file or PS5_LOCK")
    if not args.host and not args.dry_run:
        parser.error("give the console address with --host or PS5_HOST")
    return args


def main(argv=None):
    args = parse(argv)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    import transport

    def echo(line):
        print(line, flush=True)
    if args.dry_run:
        import sim
        clock = sim.SimClock()
        console = sim.SimConsole(clock, echo=echo, evidence=out)
    else:
        clock = WallClock()
        console = transport.Console(args.host, args.protocol, out, echo=echo)
    return EXIT[Session(args, console, clock, transport.ConsoleDown).run()]


if __name__ == "__main__":
    sys.exit(main())
