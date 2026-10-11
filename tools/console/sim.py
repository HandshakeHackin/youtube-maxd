"""A console that exists only in memory, for --dry-run and the unit tests.

It has the methods of transport.Console and answers them the way the
instrumented build is expected to (the status line, the UI snapshot and the
log lines are modelled on the build's sources), so a dry run walks the whole
session and prints every console operation a real run would perform. It proves
the order of operations and the session's logic, not the console's behaviour.
"""
import json
import re
from pathlib import Path

import observe as ob
from transport import ConsoleDown, OpLog  # noqa: F401 - ConsoleDown re-exported for callers

ROOT_ROWS = ["Home", "Discover", "Subscriptions", "History", "Favorites", "Queue", "Accounts & library", "Settings"]
OPTIONS = ["Video actions", "Video presets: Custom limits", "English", "Chapters", "Normal speed", "Seek interval: 10 seconds"]
PRESETS = ["Custom limits", "2160p 60 fps", "1080p 60 fps", "720p 30 fps", "360p 30 fps"]
CAPTIONS = ["Off", "English (current) [PRIMARY]", "French"]


class SimClock:
    def __init__(self):
        self.time = 1000.0

    def now(self):
        return self.time

    def sleep(self, seconds):
        self.time += seconds


class SimConsole:
    klog_port, elf_port = 3232, 9021

    def __init__(self, clock, echo=print, evidence=None, **scenario):
        self.clock = clock
        self.ops = OpLog(str(Path(evidence) / "ops.jsonl") if evidence else None, echo)
        self.scenario = {"busy": [], "busy_before_launch": [], "parks": True, "closes": True, "title": "Me at the zoo",
                         "duration": 19.0, "state": None, "down_after": None, "storage": "ready", **scenario}
        self.files = {}
        if self.scenario["state"] is not None:
            self.files[ob.STATE_FILE] = self.scenario["state"]
        self.running, self.app, self.build = list(self.scenario["busy"]), None, "prosperotube-00000000_0000-0000"
        self.registered = False
        self.seq, self.last_body = 0, None
        self.title_checks = 0

    # --- the modelled app ----------------------------------------------------------
    def _log(self, line):
        self.files[ob.LOG_FILE] = self.files.get(ob.LOG_FILE, b"") + (line + "\n").encode()

    def _position(self):
        app = self.app
        if app["paused"] or not app["active"]:
            return app["pos"]
        return min(app["pos"] + max(0.0, self.clock.now() - app["since"]), self.scenario["duration"])

    def _hold(self):
        self.app["pos"], self.app["since"] = self._position(), self.clock.now()

    def _stop_playback(self, complete):
        app = self.app
        self._hold()
        frames = int(app["pos"] * 15)
        self._log(f"stats frames_in={frames} converted={frames} published={frames} late_drop=0 seek_disc=0")
        self._log(f"stats seek_req={app['seeks']} ok={app['seeks']} fail=0 clock_resets=0 seek_to_first_ms=300")
        state = json.loads(self.files.get(ob.STATE_FILE, b"{}"))
        state.setdefault("records", []).append(
            {"id": "jNQXAC9IVRw", "title": self.scenario["title"], "played": complete,
             "position": 0 if complete else int(app["pos"]), "duration": int(self.scenario["duration"])})
        self.files[ob.STATE_FILE] = json.dumps(state).encode()
        app.update(active=False, complete=complete, screen="browser")

    def _tick(self):
        app = self.app
        if not app:
            return
        now = self.clock.now()
        if app["quit_at"] is not None and now >= app["quit_at"] and self.scenario["parks"]:
            app["parked"] = True
        if app["active"] and not app["paused"] and self._position() >= self.scenario["duration"]:
            self._stop_playback(True)
        if app["parked"] and app["status_frozen"]:
            return
        screen = {"player": 2, "subtitles": 18, "options": 31, "presets": 31, "dialog": 20}.get(app["screen"], 23)
        duration = self.scenario["duration"] if app["active"] or app["complete"] else 0.0
        self.files[ob.STATUS_FILE] = (
            f"build={self.build} t={int(now - app['boot'])} scr={screen} be={1 if app['active'] else 0} "
            f"pos={self._position():.2f} dur={duration:.1f} fps={15.0 if app['active'] else 0.0:.1f} fatal=0 "
            f"eof={int(app['complete'])} complete={int(app['complete'])} active={int(app['active'])} "
            f"parked={int(app['parked'])}\n").encode()
        app["status_frozen"] = app["parked"]
        body = self._ui()
        if body != self.last_body:
            self.last_body, self.seq = body, self.seq + 1
            self.files[ob.UI_FILE] = json.dumps({"seq": self.seq, "t_ms": int(now * 1000), "ui": body}).encode()

    def _ui(self):
        app = self.app
        screen = app["screen"]

        def focused(doc, rows, at, prefix="row"):
            return doc, {"id": f"{prefix}-{at}", "tag": "hui", "class": "focused", "text": rows[at], "doc": doc,
                         "index": at + 1, "total": len(rows),
                         "focusable": [{"id": f"{prefix}-{i}", "text": text} for i, text in enumerate(rows)]}
        if screen == "keyboard":
            doc, focus = "hui_keyboard", {"id": "key-r0c0", "text": "q", "index": 0, "total": 0, "focusable": []}
        elif screen == "browser":
            doc, focus = focused("hui_provider", app["rows"], app["focus"])
        elif screen == "subtitles":
            doc, focus = focused("hui_subtitles", CAPTIONS, 1)
        elif screen == "options":
            doc, focus = focused("hui_list", OPTIONS, app["menu"])
        elif screen == "presets":
            doc, focus = focused("hui_list", PRESETS, 0)
        elif screen == "dialog":
            doc, focus = focused("hui_dialog", ["KEEP WATCHING", "STOP"], 1, "action")
        else:
            doc = "hui_osd"
            focus = {"id": "osd-paused" if app["paused"] else "osd-playing", "text": self.scenario["title"],
                     "index": 1, "total": 1, "focusable": []}
        number = {"player": 2, "subtitles": 18, "options": 31, "presets": 31, "dialog": 20}.get(screen, 23)
        return {"screen": {"id": number, "name": screen}, "docs": [doc], "focused": focus, "also_highlighted": [],
                "modal": None, "toast": None,
                "native_player": {"osd_visible": True, "paused": app["paused"], "active_overlay": None}
                if number == 2 else None}

    def _command(self, line):
        app = self.app
        if not app or app["quit_at"] is not None:
            return False
        screen = app["screen"]
        if line.startswith("type "):
            app["typed"] = line[5:]
        elif line == "kbdone" and screen == "keyboard":
            state = json.loads(self.files.get(ob.STATE_FILE, b"{}"))
            if app["keyboard"] == "source":
                state["source"], state["token"] = app["typed"], ""
            else:
                state["source"] = re.sub(r"#.*", "", state.get("source", ""))
                state["token"] = "t" * 43
                app["rows"], app["focus"] = ["Another video", self.scenario["title"], "A third video"], 0
            self.files[ob.STATE_FILE] = json.dumps(state).encode()
            app["screen"] = "browser"
        elif line == "quit":
            if app["active"]:
                self._stop_playback(False)
            self._log("soft close: media released")
            app["quit_at"] = self.clock.now() + 3
        elif line == "stop" and app["active"]:
            self._stop_playback(False)
        elif line.startswith("seek ") and app["active"]:
            self._hold()
            app["pos"] = min(app["pos"] + float(line[5:]), self.scenario["duration"])
            app["seeks"] += 1
        elif line.startswith("key "):
            self._key(line[4:], screen)
        return True

    def _key(self, key, screen):
        app = self.app
        if screen == "browser":
            if key in ("options", "square"):
                app.update(screen="keyboard", keyboard="source" if key == "options" else "search", typed="")
            elif key == "right":
                app["focus"] = min(app["focus"] + 1, len(app["rows"]) - 1)
            elif key == "cross" and app["rows"][app["focus"]] == self.scenario["title"]:
                app.update(screen="player", active=True, paused=False, complete=False, pos=0.0,
                           since=self.clock.now() + 2)  # two seconds to resolve and open
        elif screen == "player":
            if key == "cross":
                self._hold()
                app["paused"] = not app["paused"]
            elif key == "down":
                app["screen"] = "subtitles"
            elif key == "r2":
                app.update(screen="options", menu=0)
            elif key == "circle":
                app["screen"] = "dialog"
            elif key == "l3":
                name = f"{ob.DATA}/evo_shot_000.bmp"
                self.files[name] = b"BM" + bytes(4094)
                self._log(f"screenshot: wrote {name}")
        elif screen == "subtitles" and key == "circle":
            app["screen"] = "player"
        elif screen == "options":
            if key == "down":
                app["menu"] = min(app["menu"] + 1, len(OPTIONS) - 1)
            elif key == "cross" and OPTIONS[app["menu"]].startswith("Video presets"):
                app["screen"] = "presets"
            elif key == "circle":
                app["screen"] = "player"
        elif screen == "presets" and key == "circle":
            app["screen"] = "options"
        elif screen == "dialog" and key == "cross":
            self._stop_playback(False)

    # --- transport.Console's interface -------------------------------------------------
    def _op(self, kind, detail, write=False):
        limit = self.scenario["down_after"]
        if limit is not None and self.ops.count >= limit:
            raise ConsoleDown("simulated: the console stopped answering")
        self.ops.record(kind, detail, write)
        self._tick()

    def titles(self):
        self._op("ftp-list", "/mnt/sandbox (running titles, ps5-console.py titles)")
        self.title_checks += 1
        if self.title_checks == 2 and self.scenario["busy_before_launch"]:
            self.running = list(self.scenario["busy_before_launch"])
        return list(self.running)

    def where(self, title):
        self._op("ftp-read", f"/user/app/{title}/mount.lnk (ps5-console.py where)")
        return f"folder /data/homebrew/{title}" if self.registered else \
            "unknown: no mount link (not mounted by ShadowMount Plus yet)"

    def errors(self):
        self._op("ftp-list", "/system_data/priv/error/history (ps5-console.py errors)")
        return "error records: 0; newest: none"

    def install(self, title, folder):
        self._op("ftp-install", f"{folder} -> /data/homebrew/{title} (ps5-console.py install: "
                 "temp name, readback, rename; signed files by stored-SELF or size)", True)
        builds = ob.BUILD_ID.findall((Path(folder) / "eboot.bin").read_bytes())
        self.build = builds[0].decode() if builds else self.build
        self.registered = True
        count = sum(1 for item in Path(folder).rglob("*") if item.is_file())
        return 0, (f"installed {title}: {count} sent, 0 unchanged, {count} files; eboot (simulated); "
                   "size mismatches 0; not in this build 0; size-only checks 0")

    def settle(self, title, folder, seconds):
        self._op("ftp-settle", f"{seconds} s watch, then compare /data/homebrew/{title} again (ps5-console.py settle)")
        self.clock.sleep(seconds)
        return 0, f"settled {seconds} s: (simulated) files compared, 0 differ"

    def put(self, remote, data, note=None):
        self._op("ftp-put", f"{remote} ({note or str(len(data)) + ' bytes'}; temp name, readback, rename)", True)
        if remote == ob.CMD_FILE:
            if not self._command(data.decode().strip()):
                self.files[remote] = data  # nobody reads it: it stays
        else:
            self.files[remote] = data
        return 0, f"put {remote}: {len(data)} bytes, sha256"

    def read(self, remote, offset=0):
        self._op("ftp-read", remote)
        data = self.files.get(remote)
        return None if data is None else data[offset:]

    def size(self, remote):
        self._op("ftp-size", remote)
        data = self.files.get(remote)
        return None if data is None else len(data)

    def listdir(self, remote):
        self._op("ftp-list", remote)
        prefix = remote.rstrip("/") + "/"
        names = {path[len(prefix):]: {"type": "file", "size": len(data)}
                 for path, data in self.files.items() if path.startswith(prefix) and "/" not in path[len(prefix):]}
        return names or None

    def delete(self, remote):
        self._op("ftp-delete", remote, True)
        return self.files.pop(remote, None) is not None

    def port_open(self, port):
        self._op("tcp-connect", f"port {port} (connect and close, nothing sent)")
        return True

    def klog_start(self, path):
        self._op("klog", f"record port {self.klog_port} -> {Path(path).name}")
        Path(path).write_text("(simulated kernel log)\n")

    def klog_stop(self):
        pass

    def controller(self, mode, title):
        self._op("payload", f"{mode} {title} -> port {self.elf_port} (send-controller.sh)", True)
        if mode == "launch":
            self.running = [title]
            self.app = {"boot": self.clock.now(), "screen": "browser", "rows": list(ROOT_ROWS), "focus": 0,
                        "active": False, "paused": False, "complete": False, "pos": 0.0, "since": 0.0, "seeks": 0,
                        "parked": False, "status_frozen": False, "quit_at": None, "keyboard": "", "typed": "", "menu": 0}
            self._log(f"Lapy: path=/download0/lapy status=0 storage={self.scenario['storage']}")
            self._log("Application: entering frame loop")
            if self.scenario["storage"] != "ready":
                self.app, self.running = None, []
        elif mode == "close" and self.scenario["closes"] and title in self.running:
            self.running.remove(title)
            self.app = None
        return 0, ""

    def finish(self):
        pass
