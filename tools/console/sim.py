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

# The Afterglow interface as native/afterglow reports it (frame_draw.cpp, screens/home.cpp, data.cpp):
# Home's shelves per profile (id, title, kind), the rail's destinations, the faces the mount writes
# into the data folder, and how long its moments take. Every title is invented.
AG_HOME = {"guest": [("topics", "Browse by topic", "topics"), ("trending", "Trending now", "videos")],
           "account": [("continue", "Continue watching", "videos"), ("recommended", "Recommended for you", "videos"),
                       ("subscriptions", "New from your subscriptions", "videos")]}
AG_TOPICS = [(row[0], row[1]) for row in DISCOVER if row[0].startswith(("search:", "section:"))]
AG_VIDEOS = [(row[0], row[1]) for row in VIDEOS] + [("simVideo0007", "Как работает прилив"), ("simVideo0008", "潮の満ち引き")]
AG_RAIL = [("home", "Home"), ("search", "Search"), ("subscriptions", "Subscriptions"), ("library", "Library"),
           ("notifications", "Notifications"), ("settings", "Settings"), ("accounts", "Switch profile")]
AG_FONTS = ("afterglow-inter-regular.ttf", "afterglow-inter-semibold.ttf", "afterglow-montserrat-medium.ttf",
            "afterglow-dejavu-sans-mono.ttf")
AG_LOOK_SECONDS, AG_OPENING_SECONDS, AG_LOAD_SECONDS, AG_PICTURE_SECONDS, AG_ATTRACT_SECONDS = 1.0, 2.0, 1.5, 0.4, 45.0
AG_OUTPUT = (3840, 2160)
# The interface's sound module (native/afterglow/sound/app/sound_app.cpp): how often it looks at its
# two files, the button Home answers with no cue, and its lines in the app's log. The counts, the
# handle and the code are invented.
AG_SOUND_LOOK_SECONDS = 2.0
AG_MUTE_KEYS = ("l1",)
AG_SOUND_BANK = "afterglow: sound: 47 recordings, 28 cues served, 0 silent, 3184 KiB"
AG_SOUND_NO_BANK = "afterglow: sound: 0 recordings, 0 cues served, 28 silent, 0 KiB"
AG_SOUND_OPEN = "afterglow: sound: port open, handle 3"
AG_SOUND_FAILED = "afterglow: sound: the port failed (0x80260005); the interface stays silent"
AG_PULSE = "afterglow: vibration: mode 0x00000000, first pulse 0x00000000"


def clip(text, cap):
    return text if len(text) <= cap else text[:cap] + "..."


def sim_bmp(width=96, height=54, flat=False):
    """A small 24-bit bottom-up BMP, as the build writes the scanout: a pattern, or one colour."""
    stride = (width * 3 + 3) & ~3
    rows = bytearray()
    for y in range(height):
        for x in range(width):
            rows += bytes(3) if flat else bytes(((x * 5) % 256, (y * 9) % 256, (x * 3 + y * 7) % 256))
        rows += bytes(stride - width * 3)
    header = (b"BM" + (54 + len(rows)).to_bytes(4, "little") + bytes(4) + (54).to_bytes(4, "little")
              + (40).to_bytes(4, "little") + width.to_bytes(4, "little") + height.to_bytes(4, "little")
              + (1).to_bytes(2, "little") + (24).to_bytes(2, "little") + bytes(4) + len(rows).to_bytes(4, "little") + bytes(16))
    return header + bytes(rows)


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
                         "keyboard_visible": True,
                         # The Afterglow interface. afterglow: "ok", "never" (the build ignores the override
                         # file) or "unwritable" (the file cannot be put); ag_snapshot: "ok", "nan" (a number printf wrote as nan) or "cut" (not JSON at
                         # all); ag_home: "ready", "offline", "empty" or "loading" (never answers);
                         # ag_play: "ok", "ignored" (Cross does nothing) or "refused" (a toast);
                         # ag_unresolved: how many of a shelf's first cards do not resolve;
                         # capture: "ok", "refused", "unknown" (a build without the command), "flat" or "silent";
                         # ag_hang_on: a key after which the app takes no more commands.
                         "afterglow": "ok", "ag_snapshot": "ok", "ag_profile": "guest", "ag_home": "ready",
                         "ag_pictures": True, "ag_text_engine": True, "ag_rail": True, "ag_rail_closes": True,
                         "ag_other_opens": True, "ag_other_returns": True, "ag_play": "ok", "ag_unresolved": 0,
                         "ag_back": True, "ag_attract_at_start": False, "capture": "ok", "ag_hang_on": None,
                         # ag_asked: the snapshot also carries what docs/afterglow/requests/drv.md asks for.
                         "ag_asked": False,
                         # ag_sound: what comes of the sound module's own audio port at its first voice:
                         # "ok" (it opens), "fails" (it does not: the interface stays silent and works on),
                         # "dies" (the app is gone with it) or "mute" (the package has no recordings, so
                         # there never is a voice); unwritable: files the console refuses to take.
                         "ag_sound": "ok", "unwritable": (),
                         "files": {}, **scenario}
        self.files = dict(self.scenario["files"])  # what the data folder held before the session
        if self.scenario["state"] is not None:
            self.files[ob.STATE_FILE] = self.scenario["state"]
        self.running, self.app, self.build = list(self.scenario["busy"]), None, "prosperotube-00000000_0000-0000"
        self.registered = False
        self.seq, self.last_body = 0, None
        self.title_checks = 0
        self.exit_asked = 0  # Circle on the interface's last screen: the session must never cause one

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
        self._ag_tick(now)
        if self.app is None:  # gone with the sound module's port: what it last wrote is what stays
            return
        body = self._ui()
        if body != self.last_body:  # the app writes the file only when the document changed
            self.last_body, self.seq = body, self.seq + 1
            if not self._ag_shown():
                self.files[ob.UI_FILE] = json.dumps({"seq": self.seq, "t_ms": int(now * 1000), "ui": body}).encode()
                return
            # The frame's snapshot is written by the app's own C code: no spaces, and what it gets wrong stays wrong.
            text = json.dumps({"seq": self.seq, "t_ms": int(now * 1000), "ui": body}, separators=(",", ":"), ensure_ascii=False)
            if self.scenario["ag_snapshot"] == "nan":
                text = re.sub(r'"fill":[0-9.]+', '"fill":nan', text)
            elif self.scenario["ag_snapshot"] == "cut":
                text = text[:text.index('"focusable"')] + '"focusable":[{"id":"topics/0","text":"Sci'
            self.files[ob.UI_FILE] = text.encode()

    # --- the modelled Afterglow interface ---------------------------------------------
    def _ag_shown(self):
        app = self.app
        return bool(app) and app["ag"]["enabled"] and not app["ag"]["broken"] and app["screen"] == "browser" and \
            not app["keyboard"] and not app["parked"] and app["quit_at"] is None

    def _ag_tick(self, now):
        """The override files are looked at once a second; the frame is built the first time it draws."""
        app = self.app
        ag = app["ag"]
        wanted = ob.AFTERGLOW_ON in self.files and ob.AFTERGLOW_OFF not in self.files and self.scenario["afterglow"] == "ok"
        if wanted != ag["wanted"]:
            ag["wanted"], ag["flip_at"] = wanted, now + AG_LOOK_SECONDS
        if ag["flip_at"] is not None and now >= ag["flip_at"]:
            ag["enabled"], ag["flip_at"] = ag["wanted"], None
        if not self._ag_shown():
            ag["was_shown"] = False
            return
        frame = ag["frame"]
        if frame is None:
            frame = ag["frame"] = {"born": now, "home_at": None, "stack": [["opening", ""]], "rail": False, "rail_focus": 0,
                                   "row": 0, "cols": {}, "idle": now, "attract": False, "toasts": [], "moved_at": now,
                                   "key_at": -10.0}
            # The sound module is made with the frame. Its recordings are read whatever its switches
            # say; its first look at the two files comes with the frame's first step, and with sounds
            # on that step plays the welcome of a cold start: the module's first voice.
            self._log(AG_SOUND_NO_BANK if self.scenario["ag_sound"] == "mute" else AG_SOUND_BANK)
            self._log(f"afterglow: interface up, look afterglow, {AG_OUTPUT[0]}x{AG_OUTPUT[1]}")
            if not self.scenario["ag_text_engine"]:
                self._log("afterglow: the text engine did not start (no font file could be opened); the kit's baked fonts draw")
            for name in AG_FONTS:  # the faces of the asset bundle, written once for the text engine
                self.files.setdefault(f"{ob.DATA}/{name}", bytes(1024))
            switches = self._ag_switches()
            ag["sound"] = {"files": switches, "look_at": None, "sounds": switches[0], "pulses": switches[1],
                           "port": "closed", "pulsed": False}
            self._ag_voice()
            if self.app is None:
                return
        sound = ag["sound"]
        switches = self._ag_switches()
        if switches != sound["files"]:  # looked at every two seconds, and only while the interface is stepped
            sound["files"], sound["look_at"] = switches, now + AG_SOUND_LOOK_SECONDS
        if sound["look_at"] is not None and now >= sound["look_at"]:
            sound.update(sounds=sound["files"][0], pulses=sound["files"][1], look_at=None)
        if not ag["was_shown"]:  # back on screen after the player or the classic browser had it
            ag["was_shown"], frame["idle"] = True, now
        if frame["stack"][-1][0] == "opening" and now - frame["born"] >= AG_OPENING_SECONDS:
            frame["stack"], frame["home_at"], frame["moved_at"] = [["home", ""]], now, now
            if self.scenario["ag_attract_at_start"]:
                frame["idle"] = now - AG_ATTRACT_SECONDS
        rows = self._ag_rows(now)
        if frame["stack"][-1][0] == "home" and not frame["rail"] and rows and rows[frame["row"]][2] == "videos" and \
                now - frame["idle"] >= AG_ATTRACT_SECONDS:
            frame["attract"] = True

    def _ag_switches(self):
        """(sounds, pulses) as the sound module's two files in the data folder say."""
        return ob.AFTERGLOW_SOUND_OFF not in self.files, ob.AFTERGLOW_VIBRATION_OFF not in self.files

    def _ag_voice(self):
        """A cue of the interface. With afterglow-sound.off seen in the data folder no voice is
        posted and no port is ever opened. Without it the first voice makes the module open its
        own audio port, and the app's log says what came of that."""
        sound = self.app["ag"]["sound"]
        mode = self.scenario["ag_sound"]
        if not sound["sounds"] or sound["port"] != "closed" or mode == "mute":  # a cue without a recording is silent
            return
        if mode == "dies":  # the process is gone: no status, no snapshot and no command after this
            self.app, self.running = None, [title for title in self.running if title != ob.TITLE]
        elif mode == "fails":
            sound["port"] = "failed"
            self._log(AG_SOUND_FAILED)
        else:
            sound["port"] = "open"
            self._log(AG_SOUND_OPEN)

    def _ag_pulse(self):
        """A move Home refuses asks the pad for a pulse; the first one is said in the log.
        With afterglow-vibration.off seen in the data folder none reaches the pad."""
        sound = self.app["ag"]["sound"]
        if sound["pulses"] and not sound["pulsed"]:
            sound["pulsed"] = True
            self._log(AG_PULSE)

    def _ag_plan(self):
        return AG_HOME[self.scenario["ag_profile"]]

    def _ag_rows(self, now):
        """Home's shelves that have something on them: [(id, title, kind, [(card id, title)])]."""
        frame = self.app["ag"]["frame"]
        if frame is None or frame["home_at"] is None or self.scenario["ag_home"] != "ready" or \
                now - frame["home_at"] < AG_LOAD_SECONDS:
            return []
        return [(shelf, title, kind, AG_TOPICS if kind == "topics" else AG_VIDEOS) for shelf, title, kind in self._ag_plan()]

    def _ag_snapshot(self):
        """Frame::snapshot_json() of the modelled frame, as a dict."""
        now, frame = self.clock.now(), self.app["ag"]["frame"]
        top, argument = frame["stack"][-1]
        rows = self._ag_rows(now)
        loaded = frame["home_at"] is not None and now - frame["home_at"] >= AG_LOAD_SECONDS
        load = "loading" if not loaded or self.scenario["ag_home"] == "loading" else \
            {"ready": "ready", "empty": "ready", "offline": "offline"}[self.scenario["ag_home"]]
        cards = sum(len(items) for _, _, kind, items in rows if kind == "videos")
        ready = min(cards, int((now - frame["home_at"] - AG_LOAD_SECONDS) / AG_PICTURE_SECONDS)) \
            if rows and self.scenario["ag_pictures"] else 0
        focusable, state = [], {"lists": {}}
        if top == "opening":
            focus = {"id": "opening", "text": "ProsperoTube", "index": 0, "total": 1, "rect": [856.0, 348.0, 208.0, 208.0]}
            state = {"left": "0", "lists": {}}
        elif top == "home":
            state = {"load": load, "profile": self.scenario["ag_profile"], "shelves": str(len(rows)),
                     "row": rows[frame["row"]][0] if rows else "", "hero": AG_VIDEOS[0][1] if rows else "",
                     "invite": "1" if self.scenario["ag_profile"] == "guest" else "0",
                     "lists": {f"shelf:{shelf}": [title for _, title in items] for shelf, _, _, items in rows}}
            if rows:
                shelf, _, _, items = rows[frame["row"]]
                column = frame["cols"].get(shelf, 0)
                focus = {"id": f"{shelf}/{column}", "text": items[column][1], "index": column, "total": len(items),
                         "rect": [120.0 + 272.0 * column, 508.0 + 232.0 * frame["row"], 248.0, 136.0]}
                focusable = [{"id": f"{name}/{index}", "text": title}
                             for name, _, _, cards_ in rows for index, (_, title) in enumerate(cards_)]
            else:
                focus = {"id": "loading" if load == "loading" else "offline" if load == "offline" else "empty", "text": "",
                         "index": 0, "total": 1, "rect": [120.0, 360.0, 900.0, 200.0]}
        else:  # a page that is not built yet: a real screen that names itself and goes back on Circle
            focus = {"id": "placeholder", "text": top, "index": 0, "total": 1, "rect": [120.0, 240.0, 900.0, 280.0]}
            focusable = [{"id": "placeholder", "text": top}]
            state = {"placeholder": "1", "argument": argument, "lists": {}}
        if frame["rail"]:
            screen, label = AG_RAIL[frame["rail_focus"]]
            focus = {"id": f"rail/{screen}", "text": label, "index": frame["rail_focus"], "total": len(AG_RAIL),
                     "rect": [24.0, 264.0 + 84.0 * frame["rail_focus"], 344.0, 72.0]}
            focusable = []
        busy = now - frame["key_at"] < 2.0
        drawn = {"opening": (36, 9, 1.2), "home": (212 + 6 * ready, 23, 2.9)}.get(top, (48, 11, 1.4))
        asked = {}
        if self.scenario["ag_asked"]:
            engine = int(self.scenario["ag_text_engine"])
            asked = {"output_w": AG_OUTPUT[0], "output_h": AG_OUTPUT[1], "text_engine": engine, "glyph_pages": 2 * engine,
                     "glyphs": 412 * engine, "fonts": 6 * engine, "missing_glyphs": 0}
            focus = dict(focus, kind="video" if "/" in focus["id"] and not focus["id"].startswith(("topics/", "rail/")) else "other")
        return {"interface": "afterglow", "look": "afterglow", "screen": top,
                "stack": [{"screen": name, "argument": value} for name, value in frame["stack"]],
                "moving": int(now - frame["moved_at"] < 0.4),
                "rail_open": int(frame["rail"]), "attract": int(frame["attract"]), "dialog": 0,
                "focus": focus, "focusable": focusable, "toasts": list(frame["toasts"]), "state": state,
                "stats": {"frame_ms": 16.7 + round((now * 7) % 1, 1) * 0.1, "worst_frame_ms": 24.9 if busy else 17.1,
                          "update_ms": 0.42, "draw_ms": 1.63, "instances": drawn[0], "runs": drawn[1], "fill": drawn[2],
                          "pictures_ready": ready, "pictures_pending": cards - ready, "picture_bytes": ready * 98304,
                          "frames": int((now - frame["born"]) * 60),  # the frame's steps since it was built
                          "data_pending": 0 if loaded and load != "loading" else 2, **asked}}

    def _ag_key(self, key):
        """The pad while the interface is shown (frame.cpp, screens/home.cpp, screens/stubs.cpp)."""
        app, now = self.app, self.clock.now()
        frame = app["ag"]["frame"]
        frame["key_at"] = now
        if frame["attract"]:  # the press that ends attract mode does nothing else
            frame["attract"], frame["idle"] = False, now
            return
        frame["idle"] = now
        top = frame["stack"][-1][0]
        rows = self._ag_rows(now)
        if frame["rail"]:
            if key == "down" and frame["rail_focus"] + 1 < len(AG_RAIL):
                frame["rail_focus"] += 1
            elif key == "up" and frame["rail_focus"] > 0:
                frame["rail_focus"] -= 1
            elif key == "cross":
                frame.update(rail=False, stack=[[AG_RAIL[frame["rail_focus"]][0], ""]], moved_at=now)
            elif key in ("right", "circle") and self.scenario["ag_rail_closes"]:
                frame["rail"] = False
            return
        if top == "opening":
            return
        if top != "home":
            if key == "circle" and len(frame["stack"]) > 1 and self.scenario["ag_other_returns"]:
                frame["stack"].pop()
                frame["moved_at"] = now
            elif key == "circle" and len(frame["stack"]) == 1:
                self.exit_asked += 1
            elif key == "left":
                frame.update(rail=True, rail_focus=0)
            return
        if key == "square" and self.scenario["ag_other_opens"] and self.scenario["ag_profile"] == "guest":
            frame["stack"].append(["search", ""])
            frame["moved_at"] = now
        elif key == "circle":
            self.exit_asked += 1  # Circle on the interface's last screen: the app's rule for leaving
        elif key == "left" and (not rows or frame["cols"].get(rows[frame["row"]][0], 0) == 0):
            if self.scenario["ag_rail"]:
                frame.update(rail=True, rail_focus=0)
        elif rows:
            shelf, _, kind, items = rows[frame["row"]]
            column = frame["cols"].get(shelf, 0)
            if key == "right" and column + 1 < len(items):
                frame["cols"][shelf] = column + 1
            elif key == "left":
                frame["cols"][shelf] = column - 1
            elif key == "down" and frame["row"] + 1 < len(rows):
                frame["row"] += 1
            elif key == "up" and frame["row"] > 0:
                frame["row"] -= 1
            elif key in ("right", "down", "up"):
                self._ag_pulse()  # past the last card or shelf: refused, with the error cue and a pulse
            elif key == "triangle" and kind == "videos" and self.scenario["ag_other_opens"]:
                frame["stack"].append(["video", items[column][0]])
                frame["moved_at"] = now
            elif key == "cross" and kind == "topics":
                frame["stack"].append(["grid", items[column][0]])
                frame["moved_at"] = now
            elif key == "cross":
                self._ag_play(items[column], column)

    def _ag_play(self, card, column):
        """Cross on a video: offered to the player the classic browser uses (no activation line in the log)."""
        app, mode = self.app, self.scenario["ag_play"]
        if mode == "refused":
            app["ag"]["frame"]["toasts"].append("This video could not be opened.")
        elif mode == "ok" and column < self.scenario["ag_unresolved"]:
            self._log(f"EVO boot: provider: resolve failed for youtube/{card[0]}")
        elif mode == "ok":
            app.update(screen="player", active=True, paused=False, complete=False, pos=0.0, video=(card[0], card[1], "", False),
                       controls=False, since=self.clock.now() + 2)
            if not self.scenario["ag_back"]:
                app["ag"]["broken"] = True  # the frame does not come back after the player

    def _ui(self):
        app = self.app
        screen = app["screen"]
        number, name = SCREENS[screen]
        docs, focus, modal = [], None, None
        if self._ag_shown() and app["ag"]["frame"] is not None:
            # The frame's own snapshot stands where the classic layouts report the focused element;
            # the browser's document stays listed behind it, as a provider's screen does behind a kit screen.
            return {"screen": {"id": number, "name": name}, "docs": [ob.AFTERGLOW_DOC, "youtube"],
                    "focused": self._ag_snapshot(), "also_highlighted": [], "modal": None, "toast": None,
                    "native_player": None}

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

    def _capture(self):
        """The development capture command: the scanout as the next free evo_shot file."""
        mode = self.scenario["capture"]
        if mode == "unknown":  # a build made before the command existed
            self._log("bc REMOTE_CMD unknown")
        elif mode == "refused":
            self._log("bc DEV_CAPTURE failed cannot write the capture file")
        elif mode != "silent":
            name = next(path for path in (f"{ob.DATA}/evo_shot_{slot:03d}.bmp" for slot in range(1000)) if path not in self.files)
            self.files[name] = sim_bmp(flat=mode == "flat")
            self._log(f"bc DEV_CAPTURE wrote {name} 96x54")

    def _command(self, line):
        app = self.app
        if not app or app["quit_at"] is not None or app["hung"]:
            return False
        self._log(f"bc REMOTE_CMD {line}")
        if line == "shot":
            self._capture()
        elif line.startswith("key ") and not app["keyboard"] and self._ag_shown() and app["ag"]["frame"] is not None:
            woke = app["ag"]["frame"]["attract"]
            self._ag_key(line[4:])
            app["hung"] = line[4:] == self.scenario["ag_hang_on"]
            if woke or line[4:] not in AG_MUTE_KEYS:  # a press the interface answers has its cue; so has the end of attract mode
                self._ag_voice()
        elif line.startswith("type ") and app["keyboard"]:
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
        if (remote == ob.AFTERGLOW_ON and self.scenario["afterglow"] == "unwritable") or remote in self.scenario["unwritable"]:
            return 1, f"put {remote}: the readback differs (simulated)"
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
                        "menu": 0, "controls": False, "video": ("", "", "", False), "hung": False,
                        "ag": {"wanted": False, "flip_at": None, "enabled": False, "frame": None, "broken": False,
                               "was_shown": False}}
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
