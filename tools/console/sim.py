"""A console that exists only in memory, for --dry-run and the unit tests.

It has the methods of transport.Console and answers them the way the
instrumented build is expected to (the status line, the UI snapshot and the
log lines are modelled on the build's sources), so a dry run walks the whole
session and prints every console operation a real run would perform. It proves
the order of operations and the session's logic, not the console's behaviour.
"""
import json
from pathlib import Path

import observe as ob
from transport import ConsoleDown, OpLog  # noqa: F401 - ConsoleDown re-exported for callers


# (id, title, detail, is_folder): the root rows as the first console run showed them.
ROOT = [("discover", "Discover", "Search public videos by topic", True),
        ("history", "Continue watching & history", "Stored on this PS5", True),
        ("favorites", "Favorites", "Your local saved videos", True),
        ("queue", "Play queue", "R2 on a video to add or remove", True),
        ("subscriptions", "Followed channels", "Local follows, independent of Google accounts", True),
        ("settings", "YouTube preferences", "Quality, codec, captions and SponsorBlock", True),
        ("app:settings", "Player settings", "Display, audio and controller preferences", True),
        ("app:exit", "Close ProsperoTube", "Release playback, then close from the PS5 switcher", True)]
DISCOVER = [("library:home", "Home recommendations", "What YouTube recommends to this profile", True),
            ("library", "Accounts", "Sign-in to YouTube arrives in a later version", True),
            ("search:science", "Science", "Explore science videos", True),
            ("search:technology", "Technology", "Explore technology videos", True),
            ("section:gaming", "Gaming", "YouTube gaming shelves", True),
            ("section:news", "News", "YouTube news shelves", True),
            ("section:music", "Music", "YouTube music shelves", True),
            ("search:documentary", "Documentaries", "Explore documentaries", True)]
VIDEOS = [("simVideo0001", "How tides work", "Ocean Lab", False), ("simVideo0002", "Inside a volcano", "Geo Films", False),
          ("simVideo0003", "The speed of light", "Physics Hour", False), ("simVideo0004", "Deep sea life", "Ocean Lab", False),
          ("simVideo0005", "Why the sky is blue", "Physics Hour", False), ("simVideo0006", "Building a bridge", "Works", False)]
OPTIONS = ["Video actions", "Video presets: Custom limits", "English", "Chapters", "Normal speed", "Seek interval: 10 seconds"]
CAPTIONS = ["Off", "English (current) [PRIMARY]", "French"]
COLUMNS = 4
SCREENS = {"browser": (23, "EmbyBrowse"), "player": (2, "Player"), "subtitles": (18, "SubtitlePicker"),
           "options": (31, "AudioTrackPicker"), "dialog": (20, "ExitConfirm"), "closed": (32, "SafeToClose")}


def clip(text, cap):
    return text if len(text) <= cap else text[:cap] + "..."


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
        # layout: "rml" is what the console showed (the provider's own document);
        # "kit" is the other browser the build can draw. Both must be driven.
        self.scenario = {"busy": [], "busy_before_launch": [], "parks": True, "closes": True, "duration": 600.0,
                         "state": None, "down_after": None, "storage": "ready", "service": "ready",
                         "layout": "rml", "topic_rows": len(VIDEOS), "first_cross_shows_controls": True,
                         "captions_open": True, "picker_closes": True, "exit_dialog": True, "resolves": True,
                         "keyboard_visible": True, **scenario}
        self.files = {}
        if self.scenario["state"] is not None:
            self.files[ob.STATE_FILE] = self.scenario["state"]
        self.running, self.app, self.build = list(self.scenario["busy"]), None, "prosperotube-00000000_0000-0000"
        self.registered = False
        self.seq, self.last_body = 0, None
        self.title_checks = 0

    # --- the modelled app ----------------------------------------------------------
    def _log(self, line):
        stamp = f"[{self.clock.now():.3f}] INFO  "
        self.files[ob.LOG_FILE] = self.files.get(ob.LOG_FILE, b"") + (stamp + line + "\n").encode()

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
        frames = int(app["pos"] * 30)
        self._log(f"stats frames_in={frames} converted={frames} published={frames} late_drop=0 seek_disc=0")
        self._log(f"stats seek_req={app['seeks']} ok={app['seeks']} fail=0 clock_resets=0 seek_to_first_ms=300")
        state = json.loads(self.files.get(ob.STATE_FILE, b"{}"))
        state.setdefault("records", []).append(
            {"id": app["video"][0], "title": app["video"][1], "played": complete,
             "position": 0 if complete else int(app["pos"]), "duration": int(self.scenario["duration"])})
        self.files[ob.STATE_FILE] = json.dumps(state).encode()
        app.update(active=False, complete=complete, paused=False, screen="browser", controls=False)

    def _list(self, parent, rows):
        app = self.app
        app.update(rows=rows, focus=0, parent=parent)
        self._log(f"EVO boot: provider: RequestPage parent='{parent}' page=0")
        folders = int(bool(rows) and all(row[3] for row in rows))
        self._log(f"EVO boot: provider: 'youtube' catalog parent='{parent}' -> {len(rows)} rows "
                  f"(folder_level={folders} has_more=0 total={len(rows)})")

    def _tick(self):
        app = self.app
        if not app:
            return
        now = self.clock.now()
        if app["quit_at"] is not None and now >= app["quit_at"] and self.scenario["parks"]:
            app["parked"], app["screen"] = True, "closed"
        if app["active"] and not app["paused"] and self._position() >= self.scenario["duration"]:
            self._stop_playback(True)
        if app["parked"] and app["status_frozen"]:
            return
        number = SCREENS[app["screen"]][0]
        duration = self.scenario["duration"] if app["active"] or app["complete"] else 0.0
        self.files[ob.STATUS_FILE] = (
            f"build={self.build} t={int(now - app['boot'])} scr={number} be={1 if app['active'] else 0} "
            f"pos={self._position():.2f} dur={duration:.1f} fps={30.0 if app['active'] else 60.0:.1f} fatal=0 "
            f"eof={int(app['complete'])} complete={int(app['complete'])} active={int(app['active'])} "
            f"parked={int(app['parked'])}\n").encode()
        app["status_frozen"] = app["parked"]
        body = self._ui()
        if body != self.last_body:  # the app writes the file only when the document changed
            self.last_body, self.seq = body, self.seq + 1
            self.files[ob.UI_FILE] = json.dumps({"seq": self.seq, "t_ms": int(now * 1000), "ui": body}).encode()

    def _ui(self):
        app = self.app
        screen = app["screen"]
        number, name = SCREENS[screen]
        docs, focus, modal = [], None, None

        def kit(doc, rows, at, prefix="row"):
            return [doc], {"id": f"{prefix}-{at}", "tag": "hui", "class": "focused", "text": rows[at], "doc": doc,
                           "index": at + 1, "total": len(rows), "rect": {"x": 0, "y": 0, "w": 0, "h": 0},
                           "focusable": [{"id": f"{prefix}-{i}", "text": text} for i, text in enumerate(rows)]}
        if screen == "browser" and app["rows"]:
            if self.scenario["layout"] == "kit":
                docs, focus = kit("hui_provider", [row[1] for row in app["rows"]], app["focus"])
            else:  # the provider's RmlUi document: cards without ids, the title's initial as artwork
                texts = [f"{row[1][0]} {row[1]} {row[2]}" for row in app["rows"]]
                row = app["rows"][app["focus"]]
                docs = ["youtube"]
                focus = {"id": "", "tag": "div", "class": "tube-card folder" if row[3] else "tube-card",
                         "text": clip(texts[app["focus"]], 80), "doc": "youtube", "index": app["focus"] + 1,
                         "total": len(texts), "rect": {"x": 128, "y": 288, "w": 864, "h": 664},
                         "focusable": [{"id": "", "text": clip(text, 48)} for text in texts]}
        elif screen == "browser":
            docs = ["youtube"]
        elif screen == "subtitles":
            docs, focus = kit("hui_subtitles", CAPTIONS, 1)
        elif screen == "options":
            docs, focus = kit("hui_list", OPTIONS, app["menu"])
        elif screen == "player":
            docs = ["hui_osd"]
            if app["paused"] or app["controls"]:
                focus = {"id": "osd-paused" if app["paused"] else "osd-playing", "tag": "hui", "class": "focused",
                         "text": app["video"][1], "doc": "hui_osd", "index": 1, "total": 1, "focusable": []}
        # dialog and closed screens: an overlay over a screen the snapshot cannot name
        if app["keyboard"] and self.scenario["keyboard_visible"]:
            docs = docs + ["keyboard"]
            focus = {"id": "k0", "tag": "div", "class": "kb-key focused", "text": "q", "doc": "keyboard",
                     "index": 1, "total": 10, "focusable": []}
            modal = {"kind": "keyboard", "text": f"Search YouTube videos... {app['typed']} {len(app['typed'])} / 800 q w e r t y"}
        return {"screen": {"id": number, "name": name}, "docs": docs, "focused": focus, "also_highlighted": [],
                "modal": modal, "toast": None,
                "native_player": {"osd_visible": app["controls"], "paused": app["paused"], "active_overlay": None}
                if number == 2 else None}

    def _command(self, line):
        app = self.app
        if not app or app["quit_at"] is not None:
            return False
        self._log(f"bc REMOTE_CMD {line}")
        if line.startswith("type ") and app["keyboard"]:
            app["typed"] = line[5:]
        elif line == "kbdone" and app["keyboard"]:
            app["keyboard"] = False
            self._log(f"EVO boot: provider: Search query='{app['typed']}'")
            self._list(app["parent"], VIDEOS[:self.scenario["topic_rows"]])
        elif line == "kbclose":
            app["keyboard"] = False
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
        elif line.startswith("key ") and not app["keyboard"]:
            self._key(line[4:], app["screen"])
        return True

    def _key(self, key, screen):
        app = self.app
        if screen == "browser":
            at, count = app["focus"], len(app["rows"])
            if key == "square":
                app.update(keyboard=True, typed="")
            elif key == "right" and at % COLUMNS < COLUMNS - 1 and at + 1 < count:
                app["focus"] = at + 1
            elif key == "left" and at % COLUMNS:
                app["focus"] = at - 1
            elif key == "down" and at + COLUMNS < count:
                app["focus"] = at + COLUMNS
            elif key == "up" and at >= COLUMNS:
                app["focus"] = at - COLUMNS
            elif key == "circle" and app["parent"]:
                self._list("", ROOT)
            elif key == "cross" and count:
                row = app["rows"][at]
                self._log(f"EVO boot: provider: 'youtube' activate '{row[0]}' ({'folder' if row[3] else 'playable'}) "
                          f"title='{row[1]}'")
                if row[0] == "discover":
                    self._list("discover", DISCOVER)
                elif row[0].startswith("search:"):
                    self._list(row[0], VIDEOS[:self.scenario["topic_rows"]])
                elif row[0] == "app:exit":
                    app["quit_at"] = self.clock.now() + 3
                elif not row[3] and self.scenario["resolves"]:
                    app.update(screen="player", active=True, paused=False, complete=False, pos=0.0, video=row,
                               controls=False, since=self.clock.now() + 2)  # two seconds to resolve and open
                elif not row[3]:
                    self._log(f"EVO boot: provider: resolve failed for youtube/{row[0]}")
                elif row[3]:
                    self._list(row[0], [])
        elif screen == "player":
            if key == "cross":
                if self.scenario["first_cross_shows_controls"] and not app["controls"] and not app["paused"]:
                    app["controls"] = True  # hidden controls: the first press only shows them
                else:
                    self._hold()
                    app["paused"] = not app["paused"]
                    app["controls"] = app["paused"]
            elif key == "down" and self.scenario["captions_open"]:
                app["screen"] = "subtitles"
            elif key == "r2":
                app.update(screen="options", menu=0)
            elif key == "circle" and self.scenario["exit_dialog"]:
                app["screen"] = "dialog"
            elif key == "l3":
                name = f"{ob.DATA}/evo_shot_000.bmp"
                self.files[name] = b"BM" + bytes(4094)
                self._log(f"EVO boot: screenshot: wrote {name}")
        elif screen in ("subtitles", "options"):
            if key == "down" and screen == "options":
                app["menu"] = min(app["menu"] + 1, len(OPTIONS) - 1)
            elif key == "circle" and self.scenario["picker_closes"]:
                app["screen"] = "player"
        elif screen == "dialog":
            if key == "cross":
                self._stop_playback(False)
            elif key == "circle":
                app["screen"] = "player"

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
            self.app = {"boot": self.clock.now(), "screen": "browser", "rows": [], "focus": 0, "parent": "",
                        "active": False, "paused": False, "complete": False, "pos": 0.0, "since": 0.0, "seeks": 0,
                        "parked": False, "status_frozen": False, "quit_at": None, "keyboard": False, "typed": "",
                        "menu": 0, "controls": False, "video": ("", "", "", False)}
            self._log(f"EVO boot: Lapy: path=helper status=0 storage={self.scenario['storage']}")
            if self.scenario["service"] == "ready":
                self._log("embedded service: listening on 127.0.0.1:26757")
            else:
                self._log(f"embedded service: did not start: {self.scenario['service']}")
            self._list("", ROOT)
            self._log("EVO boot: Application: entering frame loop")
            if self.scenario["storage"] != "ready":
                self.app, self.running = None, []
        elif mode == "close" and self.scenario["closes"] and title in self.running:
            self.running.remove(title)
            self.app = None
        return 0, ""

    def finish(self):
        pass
