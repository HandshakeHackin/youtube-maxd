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
import observe as ob  # noqa: E402

SESSION_NAME = "ProsperoTube"
INSTALL = f"/data/homebrew/{ob.TITLE}"
SHADOWMOUNT_LOG = "/data/shadowmount/debug.log"
SNAPSHOT_LIMIT = 16 << 20

# Seconds. Every wait in the session is one of these: none is open-ended.
T = {"launch": 45, "entry": 90, "storage_line": 15, "ready": 90, "register": 120, "keyboard": 10,
     "submit": 15, "search": 60, "service": 20, "focus": 5, "open": 90, "playing": 30,
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
    def expect_doc(self, name):
        return lambda: ob.pc_doc(self.ui(), name)

    def on_player(self):
        snapshot = self.ui()
        return ob.ui_screen(snapshot) == ob.SCREEN_PLAYER, ob.ui_line(snapshot)

    def keyboard_entry(self, open_key, text, what):
        self.key(open_key, f"{what}: keyboard open", self.expect_doc("hui_keyboard"), T["keyboard"])
        self.command(f"type {text}")
        self.command("kbdone")
        self.step(f"{what}: submitted, back on the browser", self.expect_doc("hui_provider"), T["submit"])

    def toggle(self, label, probe):
        """Cross once; a second press only if the first one visibly did nothing
        (with hidden controls the first press may only show them)."""
        for attempt in (1, 2):
            self.command("key cross")
            try:
                return self.step(label, probe, T["toggle"], record=attempt == 2)
            except Stop:
                if attempt == 2:
                    raise
                self.log(f"   {label}: not yet after one press; pressing once more")

    def functional(self):
        args = self.args
        self.functional_deadline = self.clock.now() + T["functional"]

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

        # (c) search
        self.keyboard_entry("square", args.search_query, "(c) search")
        self.step(f"(c) result rows include /{args.expect_title}/",
                  lambda: ob.pc_rows(self.ui(), args.expect_title), T["search"])
        self.obs["search_rows"] = ob.ui_rows(self.ui())
        self.step("(b) the service's address is kept out of the saved state", lambda: ob.pc_state_local(self.read_state()),
                  T["service"], interval=2)

        # (d) play
        target = ob.find_row(self.ui(), args.expect_title)
        for _ in range(target):
            index = ob.ui_focus(self.ui()).get("index", 0)
            if index == target:
                break
            self.key("right", f"(d) focus moves toward row {target}",
                     lambda index=index: (ob.ui_focus(self.ui()).get("index", 0) == index + 1, ob.ui_line(self.ui())),
                     T["focus"])
        focus = ob.ui_focus(self.ui())
        if focus.get("index") != target or not re.search(args.expect_title, str(focus.get("text", "")), re.IGNORECASE):
            raise Stop(f"(d) the cursor is not on the expected video: {ob.ui_line(self.ui())}")
        self.command("key cross")
        resumed = []

        def opened():
            status = self.status()
            if status and status["scr"] == ob.SCREEN_RESUME and not resumed:
                resumed.append(True)
                self.note("a resume prompt appeared (saved position): accepted with Cross")
                self.command("key cross")
            ok = bool(status) and status["scr"] == ob.SCREEN_PLAYER and status["active"] == 1 and not status["fatal"]
            return ok, f"scr={status['scr']} active={status['active']} fatal={status['fatal']}" if status else "no status"
        self.step("(d) player open, decoder active", opened, T["open"])
        self.playing("(d) position advancing")

        # (e) pause, captions, capture
        window = []
        self.toggle("(e) paused", lambda: (window.append(self.status()), ob.pc_paused(window, self.ui()))[1])
        self.key("down", "(e) caption picker open",
                 lambda: (lambda s: (ob.ui_doc(s) == "hui_subtitles" and bool(ob.ui_rows(s)), ob.ui_line(s)))(self.ui()),
                 T["picker"])
        rows = ob.ui_rows(self.ui())
        offered, current = ob.caption_tracks(rows)
        self.obs["captions"] = {"rows": rows, "offered": offered, "current": current}
        self.log(f"   (e) captions offered: {offered or 'none'}; showing: {current or 'none'}")
        if offered and not current:
            self.note("captions are offered but none is marked current: check the capture, toggle with Up by hand")
        self.key("circle", "(e) back on the player", self.on_player, T["picker"])
        self.capture()

        # (f) options and quality
        self.options()

        # (e) resume, seek
        self.command("key cross")
        try:
            self.playing("(e) resumed, position advancing", timeout=T["toggle"] + 4, record=False)
        except Stop:
            self.log("   (e) not resumed after one press; pressing once more")
            self.command("key cross")
            self.playing("(e) resumed, position advancing", timeout=T["toggle"] + 4)
        before = self.status()
        if not before:
            raise Stop("(e) no status before the seek")
        self.command("seek +10")
        self.step("(e) +10 s seek landed", lambda: ob.pc_seek(before["pos"], self.status(), 10.0), T["seek"])

        # (g) end, statistics, history
        now = self.status() or before
        remaining = now["dur"] - now["pos"]
        mode = args.end if args.end != "auto" else ("eof" if 0 < remaining <= 120 else "stop")
        self.obs["end_mode"] = mode
        if mode == "eof":
            self.step("(g) played to the end (complete=1)", lambda: ob.pc_complete(self.status()),
                      max(remaining, 0) + T["eof_margin"])
            try:
                self.step("(g) player retired by itself", self.inactive, T["stopped"], record=False)
            except Stop:
                self.note("still active after the end: sent the development \"stop\" command")
                self.command("stop")
                self.step("(g) player retired after stop", self.inactive, T["stopped"])
        else:
            self.key("circle", "(g) stop confirmation, STOP focused", lambda: ob.pc_dialog(self.ui(), "STOP"), T["dialog"])
            self.key("cross", "(g) stopped, back from the player", self.inactive, T["stopped"])
        stats = {}

        def statistics():
            stats.update(ob.parse_stats(self.read_log()) or {})
            return ob.pc_stats(stats or None)
        self.step("(d,g) engine statistics for the playback", statistics, T["stats"], interval=2)
        self.obs["stats"] = stats
        self.step(f"(g) history record for {args.video_id}",
                  lambda: ob.pc_history(self.read_state(), args.video_id, mode == "eof"), T["history"], interval=2)
        record = ob.history_record(self.state_bytes, args.video_id) or {}
        self.obs["history"] = {key: record.get(key) for key in ("id", "title", "played", "position", "duration")}
        self.save("state-after/youtube.json", self.state_bytes)

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
        """One scanout capture on the paused player (stick click = capture there)."""
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

    def options(self):
        """R2: the playback options. Walk the list once to read every entry, open
        the video presets when there are any, and return to the player."""
        def listed():
            snapshot = self.ui()
            ok = ob.ui_screen(snapshot) == ob.SCREEN_OPTIONS and bool(ob.ui_focus(snapshot).get("id"))
            return ok, ob.ui_line(snapshot)
        self.key("r2", "(f) playback options open", listed, T["picker"])
        snapshot = self.ui()
        total = int(ob.ui_focus(snapshot).get("total") or 1)
        entries = []

        def see(snapshot):
            for text in ob.ui_rows(snapshot) + [str(ob.ui_focus(snapshot).get("text", ""))]:
                if text and text not in entries:
                    entries.append(text)
        see(snapshot)
        # The list shows a window of its rows: move down only until a quality entry was read.
        for _ in range(min(total - 1, 40)):
            index = int(ob.ui_focus(self.ui()).get("index") or 0)
            if ob.quality_rows(entries) or index >= total:
                break
            self.key("down", "(f) next option",
                     lambda index=index: (ob.ui_focus(self.ui()).get("index") == index + 1, ob.ui_line(self.ui())),
                     T["focus"])
            see(self.ui())
        presets = None
        top = ob.ui_focus(self.ui())
        if self.args.open_presets and re.search(r"video presets", str(top.get("text", "")), re.IGNORECASE):
            def inside():
                focus = ob.ui_focus(self.ui())
                changed = focus.get("total") != top.get("total") or focus.get("text") != top.get("text")
                return changed and bool(ob.ui_rows(self.ui())), ob.ui_line(self.ui())
            self.key("cross", "(f) video presets open", inside, T["picker"])
            presets = ob.ui_rows(self.ui())
            self.key("circle", "(f) back to the options",
                     lambda: (ob.ui_focus(self.ui()).get("total") == top.get("total"), ob.ui_line(self.ui())), T["picker"])
        quality = ob.quality_rows(entries)
        self.obs["options"] = {"entries_seen": entries, "total": total, "quality": quality, "presets": presets}
        self.log(f"   (f) options read ({len(entries)} of {total}): {entries}")
        if not quality:
            raise Stop(f"(f) no quality entry among the playback options: {entries}")
        self.log(f"   PASS (f) quality entries: {quality}" + (f"; presets: {presets}" if presets else ""))
        self.key("circle", "(f) back on the player", self.on_player, T["picker"])

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
            if name in ob.DEV_FILES or created_shot:
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
        leftover = [name for name in after if name in ob.DEV_FILES]
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
            with self.stage(5, "core path through the development interface"):
                self.functional()
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
        self.log(f"RESULT: {verdict.upper()}{' (DRY RUN, simulated console)' if self.args.dry_run else ''}; "
                 f"stages {' '.join(str(s['stage']) + ':' + s['result'] for s in self.stages)}; evidence: {self.out}")
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
    parser.add_argument("--video-id", default="jNQXAC9IVRw")
    parser.add_argument("--search-query", default="Me at the zoo")
    parser.add_argument("--expect-title", default="Me at the zoo", help="regular expression the result row must match")
    parser.add_argument("--settle", type=int, default=130, help="seconds to watch the install before the second compare")
    parser.add_argument("--end", choices=("auto", "eof", "stop"), default="auto")
    parser.add_argument("--open-presets", action="store_true",
                        help="also open the video presets list when the cursor reaches it (more key presses)")
    parser.add_argument("--keep-state", action="store_true", help="leave the session's history on the console")
    parser.add_argument("--allow-unparked-close", action="store_true",
                        help="send the close payload even when the app did not park (a forced close)")
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
