"""The optional Afterglow stage of the console session (session.py --afterglow).

It runs after the classic core path, in the same launch, and leaves the app as
it found it: the interface is switched on with the override file, its frame is
read, Home is captured from the scanout, one video is played from Home through
the existing player, and the interface is switched off again.

The interface's sound module opens an audio port of its own at its first
voice. So two more override files are written before the switch and keep the
module silent, and its port shut, while everything else is judged. Only as the
last step is the sound let go, and what the port does is recorded; then every
file the stage wrote is removed.

Every step records PASS, FAIL or NOTE with its numbers. Only facts of the
engine and of the console's files are hard (they fail the session): the
interface appearing at all, playback after Cross on a video, the override
files gone and the classic browser back. Everything read from the interface's
own snapshot is soft, and the sound step is a PASS or a NOTE whatever it finds.
A step that cannot be judged becomes a NOTE and the bytes of the snapshot it
was looking at are saved; nothing in here ends the session before its teardown
except a console that stops answering.

Like session.py, this file never opens a connection: every console operation
goes through the session's transport object.
"""
import hashlib
import json
import re

import observe as ob

# Seconds. Every wait of the stage is one of these: none is open-ended.
T = {"ready": 10, "on": 40, "home": 45, "pictures": 30, "rest": 6, "move": 6, "settle": 1.5, "screen": 12, "capture": 40,
     "open": 90, "playing": 30, "dialog": 8, "stopped": 20, "back": 20, "off": 20, "idle": 25, "budget": 12 * 60,
     # The sound step: the app looks at its sound files every two seconds; how long the port's line may
     # take; the time between the two reads of the frame's counter; what the step may add to the budget.
     "sound_look": 5, "sound_line": 10, "beat": 1, "sound": 90}
SWITCH_TEXT = b"ProsperoTube console session\n"
# What the stage writes, in the order it writes it. The two files that silence the interface's sound
# module come before the switch: its own audio port, opened by the first voice, stays shut while the
# picture, the stats and playback are judged, and the pad gets no pulse.
SILENCERS = ((ob.AFTERGLOW_SOUND_OFF, "Afterglow override: no sound"),
             (ob.AFTERGLOW_VIBRATION_OFF, "Afterglow override: no vibration"))
SWITCH = (ob.AFTERGLOW_ON, "Afterglow override: on")
SOUND_STEP = "(s) sound: the port opens at the first cue"
REMOVED_STEP = "(g) every file the stage wrote is removed"
WAKE_KEY = "l1"  # a button Home does nothing with: it ends attract mode, or restarts the 45 s that lead to it
STEPS_TO_RAIL = 6


class GiveUp(Exception):
    """A wait that has its answer before its time is up: the answer is no."""


def slug(label):
    return re.sub(r"[^a-z0-9]+", "-", label.lower()).strip("-")[:40]


class AfterglowStage:
    def __init__(self, session, stop):
        self.s, self.Stop = session, stop
        self.report = session.obs.setdefault("afterglow", {"result": "not-run"})
        self.on = False          # the switch was written by this stage: the interface is asked for
        self.written = []        # every override file the stage put on the console, in the order it did
        self.gone = set()        # those of them the data folder no longer lists
        self.alive = True        # the app still takes commands
        self.lost = ""           # why the interface is no longer known to be on Home
        self.raw = self.snapshot = self.frame = self.classic = None
        self.repaired = False
        self.phase, self.samples = None, {"rest": [], "moving": []}
        self.saved = 0
        self.last_key = session.clock.now()
        self.deadline = session.clock.now() + T["budget"]
        self.mark = 0            # where the stage's part of the app's log begins

    # --- recording -------------------------------------------------------------------
    def record(self, label, result, detail, hard=False, **numbers):
        entry = {"step": label, "result": result, "detail": str(detail), "stage": "afterglow",
                 "gate": "hard" if hard else "soft"}
        if numbers:
            entry["numbers"] = numbers
        self.s.steps.append(entry)
        self.s.log(f"   {result} {label}: {detail}")
        return result == "PASS"

    def unjudged(self, label, why):
        self.saved += 1
        name = f"afterglow/unjudged-{self.saved:02d}-{slug(label)}.json"
        self.s.save(name, self.raw or b"")
        self.record(label, "NOTE", f"cannot be judged: {why}; the snapshot's bytes are in {name}")

    def judge(self, label, action):
        """One step. It records its own PASS or FAIL; what it cannot judge, and
        anything that goes wrong in this driver, becomes a NOTE with the
        snapshot's bytes saved. Only a silent console is passed on."""
        try:
            return action(label)
        except (self.s.Down, KeyboardInterrupt):
            raise
        except ob.Unjudged as why:
            self.unjudged(label, str(why))
        except self.Stop as stop:  # a command the app did not take
            self.alive = False
            self.record(label, "FAIL", stop.reason)
        except Exception as error:  # noqa: BLE001 - an unexpected snapshot must not cost the teardown its evidence
            self.unjudged(label, f"{type(error).__name__}: {error}")
        return None

    # --- reading and waiting ---------------------------------------------------------
    def read(self):
        """The frame's snapshot as the build reports it now, or None. Keeps the
        bytes, and the numbers of its stats while a measurement runs."""
        s = self.s
        self.raw = s.console.read(ob.UI_FILE)
        snapshot = ob.parse_ui(self.raw)
        if snapshot is None and self.raw:
            snapshot, repairs = ob.parse_ui_repaired(self.raw)
            if snapshot is not None and not self.repaired:
                self.repaired = True
                self.report["snapshot_repairs"] = repairs
                s.save("afterglow/repaired-snapshot.json", self.raw)
                s.note(f"Afterglow: dev_ui.json is not valid JSON as the build writes it ({', '.join(repairs)}); it is read "
                       "after repair, its bytes are in afterglow/repaired-snapshot.json")
        self.snapshot = snapshot
        if snapshot and snapshot["seq"] != s.last_ui_seq:
            s.last_ui_seq = snapshot["seq"]
            with open(s.out / "ui.jsonl", "a") as handle:
                handle.write(json.dumps(snapshot) + "\n")
        self.frame = ob.ag_frame(snapshot)
        if self.frame is not None and self.phase:
            values, _ = ob.ag_stats(self.frame)
            if values:
                self.samples[self.phase].append(values)
        return self.frame

    def wait(self, probe, timeout, interval=1.0):
        """A bounded wait for an observed fact: (observed, detail)."""
        clock = self.s.clock
        start = clock.now()
        while True:
            try:
                ok, detail = probe()
            except GiveUp as answer:
                return False, str(answer)
            if ok:
                return True, detail
            if clock.now() - start >= timeout:
                return False, f"not observed within {timeout} s (last: {detail})"
            if clock.now() > self.deadline:
                return False, f"the stage's time budget is used up (last: {detail})"
            clock.sleep(interval)

    def send(self, line):
        self.s.command(line)  # raises Stop when the app does not take it
        if line.startswith("key "):
            self.last_key = self.s.clock.now()

    # --- the stage's own files -----------------------------------------------------------
    def write(self, path, note):
        """One override file, put as every file of the session is (temporary
        name, readback, rename). It is on the stage's list from the attempt on:
        what a put that failed left behind is removed with the rest. Returns
        what went wrong, or nothing."""
        self.written.append(path)
        code, output = self.s.console.put(path, SWITCH_TEXT, note=note)
        return "" if code == 0 else (output[-200:] or f"the upload ended with status {code}")

    def remove(self, paths):
        """Delete these files of the stage and read the data folder back.
        Returns every name of the stage's the folder still lists: a file, or
        the temporary copy of a put that failed."""
        s = self.s
        for path in paths:
            for name in ob.override_names(path):
                s.console.delete(f"{ob.DATA}/{name}")
        listing = s.console.listdir(ob.DATA) or {}
        left = {path: [name for name in ob.override_names(path) if name in listing] for path in self.written}
        self.gone = {path for path, names in left.items() if not names}
        return [name for names in left.values() for name in names]

    def need_app(self):
        if not self.alive:
            raise ob.Unjudged("the app stopped taking commands")

    def need_frame(self):
        """A snapshot of the frame, or Unjudged: three looks a second apart."""
        self.need_app()
        for look in range(3):
            if self.read() is not None:
                return self.frame
            if look < 2:
                self.s.clock.sleep(1)
        if self.snapshot is None:
            raise ob.Unjudged("dev_ui.json is not a snapshot that can be read")
        raise ob.Unjudged("the interface's snapshot is not reported: " + ob.ui_line(self.snapshot))

    def at_home(self):
        """The frame's snapshot with Home on top, the rail and the dialog shut
        and the focus on a card; Unjudged otherwise."""
        if self.lost:
            raise ob.Unjudged(f"the interface is not known to be on Home ({self.lost})")
        frame = self.need_frame()
        if ob.ag_screen(frame) != "home" or ob.ag_flag(frame, "rail_open") or ob.ag_flag(frame, "dialog"):
            raise ob.Unjudged("Home is not what has the pad: " + ob.ag_line(frame))
        if ob.ag_place(frame) is None:
            raise ob.Unjudged(f"the focus id \"{ob.ag_focus(frame).get('id')}\" is not <shelf>/<index>")
        return frame

    def awake(self):
        """Left alone for 45 s, Home shows off its videos, and the press that
        ends that does nothing else. One press of a button Home ignores ends
        it, or restarts the count when it is near."""
        frame = self.read()
        if frame is None or ob.ag_screen(frame) != "home":
            return
        if ob.ag_flag(frame, "attract"):
            self.send(f"key {WAKE_KEY}")
            self.wait(lambda: (not ob.ag_flag(self.read(), "attract"), "attract mode still on"), T["move"], 0.5)
            self.s.clock.sleep(1.5)  # the page's furniture fades back in
            self.s.log(f"   attract mode was on: ended with one press of {WAKE_KEY}, which Home ignores")
            self.report["attract_ended"] = self.report.get("attract_ended", 0) + 1
        elif self.s.clock.now() - self.last_key > T["idle"]:
            self.send(f"key {WAKE_KEY}")

    def press(self, key, probe, timeout=None):
        """One key, then the bounded wait for what it must change. What the
        frame reports meanwhile is its cost while something moves."""
        self.awake()
        self.send(f"key {key}")
        self.phase = "moving"
        try:
            return self.wait(probe, timeout or T["move"], 0.5)
        finally:
            self.phase = None

    def focus_is(self, shelf, index):
        frame = self.read()
        return ob.ag_place(frame) == (shelf, index), ob.ag_line(frame)

    def row_is(self, shelf):
        frame = self.read()
        place = ob.ag_place(frame)
        return place is not None and place[0] == shelf and ob.ag_state(frame, "row", shelf) == shelf, ob.ag_line(frame)

    # --- the stage ---------------------------------------------------------------------
    def run(self):
        s = self.s
        s.functional_deadline = None  # the stage keeps its own time budget
        self.mark = len(s.read_log())
        try:
            if self.judge("(pre) the app is on its browser with nothing playing", self.precondition):
                try:
                    self.body()
                finally:
                    self.report["log"] = ob.ag_log_lines(s.log_text[self.mark:])
                self.judge("(g) switched off", self.switch_off)
        finally:
            self.summarize()
        if self.report["hard_failed"]:
            raise self.Stop("Afterglow: " + "; ".join(self.report["hard_failed"]))

    def body(self):
        if not self.judge("(a) switched on: the frame draws", self.switch_on):
            return
        self.judge("(c) Home lists its shelves", self.home_listed)
        self.judge("(c) pictures become ready", self.pictures)
        self.judge("(b) frame statistics at rest", self.stats_at_rest)
        self.judge("(b) output resolution", self.output_size)
        self.judge("(c) focus ids are <shelf>/<index>", self.focus_ids)
        self.judge("(c) texts are present", self.texts)
        self.judge("(c) glyphs and fonts of the text engine", self.text_engine)
        # The picture is taken as soon as Home is real and before any key is sent:
        # nothing the navigation steps find can cost the session its main evidence.
        self.judge("(e) scanout capture of Home", lambda label: self.capture(label, "afterglow-home"))
        self.judge("(d) focus moves along a shelf", self.move_along)
        self.judge("(d) focus moves between shelves", self.move_between)
        self.judge("(d) the rail opens and closes", self.rail)
        self.judge("(d) another screen opens and Circle returns", self.other_screen)
        self.judge("(b) frame statistics while moving", self.stats_moving)
        self.judge("(f) Cross on a video starts playback", self.play)
        # Last of all, when nothing is left that it could cost: the sound module's own audio port.
        self.judge(SOUND_STEP, self.sound)

    def precondition(self, label):
        s = self.s
        window = [s.status()]

        def ready():
            window.append(s.status())
            earlier, now = window[-2], window[-1]
            if not now:
                return False, "no status"
            fine = now["scr"] == ob.SCREEN_BROWSER and now["active"] == 0 and not now["fatal"] and not now["parked"]
            return fine and bool(earlier) and now["t"] > earlier["t"], \
                f"scr={now['scr']} active={now['active']} fatal={now['fatal']} parked={now['parked']} t={now['t']}"
        ok, detail = self.wait(ready, T["ready"])
        self.classic = s.ui()  # what the classic browser showed: the stage must leave it so
        if ok and ob.keyboard_open(self.classic):
            ok, detail = False, "a keyboard is open"
        if not ok:
            self.record(label, "NOTE", f"the stage is not run: {detail}")
            return False
        s.save("afterglow/classic-before.json", self.classic or {})
        return self.record(label, "PASS", f"{detail}; {ob.ui_line(self.classic)}")

    # (a) ---------------------------------------------------------------------------------
    def switch_on(self, label):
        s = self.s
        s.afterglow_touched = True  # from here the teardown removes the stage's files whatever happens
        # The sound module's files first: they are there when the interface takes its first step, so not
        # even the welcome under the opening is played. Without them the switch is not written at all.
        for path, note in SILENCERS + (SWITCH,):
            problem = self.write(path, note)
            if problem:
                unguarded = "" if path == ob.AFTERGLOW_ON else "; the interface is not switched on without it"
                return self.record(label, "FAIL", f"{ob.override_names(path)[0]} was not written: {problem}{unguarded}", hard=True)
        self.on = True

        def shown():
            frame = self.read()
            if frame is not None:
                return True, f"interface afterglow, look {frame.get('look', '?')}; {ob.ag_line(frame)}"
            if self.snapshot is None and ob.ag_raw_shown(self.raw):
                return True, "the bytes of dev_ui.json name the interface, but are not a snapshot that can be read"
            if ob.ag_shown(self.snapshot):
                return False, "the document afterglow is listed without the frame's snapshot: " + ob.ui_line(self.snapshot)
            return False, "the classic browser still draws: " + ob.ui_line(self.snapshot)
        start = s.clock.now()
        ok, detail = self.wait(shown, T["on"])
        numbers = {"seconds": round(s.clock.now() - start, 1)}
        first = ob.ag_stats(self.frame)[0]
        if "worst_frame_ms" in first:  # the frame that built the interface is among its first 120
            numbers["first_worst_frame_ms"] = first["worst_frame_ms"]
            detail += f"; {numbers['seconds']:.0f} s after the file, the longest of its first frames {first['worst_frame_ms']:.0f} ms"
        said = ob.ag_log_lines(s.read_log()[self.mark:])
        up = ob.ag_interface_up(s.log_text[self.mark:])
        if up:
            detail += f"; the mount logged look {up['look']}, {up['width']}x{up['height']}"
        elif not ok:
            detail += "; the interface's last line in the app's log: " + (said[-1] if said else "none at all")
        if ok and self.frame is not None:
            s.save("afterglow/first-snapshot.json", self.snapshot)
        elif ok:
            s.save("afterglow/unreadable-snapshot.json", self.raw or b"")
        return self.record(label, "PASS" if ok else "FAIL", detail, hard=True, **numbers)

    # (c) ---------------------------------------------------------------------------------
    def home_listed(self, label):
        self.need_frame()

        def listed():
            frame = self.read()
            if frame is None:
                return False, "no Afterglow snapshot"
            if ob.ag_screen(frame) != "home":
                return False, ob.ag_line(frame)  # the opening moment
            ok, detail = ob.pc_ag_home(frame)
            loading = ob.ag_state(frame, "load") == "loading"
            if not ok and not loading:
                raise GiveUp(detail)  # every shelf has answered and none has anything: waiting changes nothing
            return ok and not loading, detail
        ok, detail = self.wait(listed, T["home"])
        frame = self.need_frame()
        self.s.save("afterglow/home.json", self.snapshot)
        if not ok and ob.ag_screen(frame) == "home":
            ok, now = ob.pc_ag_home(frame)  # shelves are listed although something still loads
            detail = now if ok else detail
        self.report["home"] = {"screen": ob.ag_screen(frame), "load": ob.ag_state(frame, "load"),
                               "profile": ob.ag_state(frame, "profile"), "focus": ob.ag_focus(frame).get("id"),
                               "shelves": {shelf: titles for shelf, titles in ob.ag_lists(frame)}}
        return self.record(label, "PASS" if ok else "FAIL", detail)

    def pictures(self, label):
        self.at_home()
        seen = {}

        def ready():
            values, _ = ob.ag_stats(self.read())
            if "pictures_ready" not in values:
                if self.frame is None:
                    return False, "no Afterglow snapshot"
                raise ob.Unjudged("the snapshot's stats lack pictures_ready")
            seen.update(values)
            return values["pictures_ready"] > 0 and values.get("pictures_pending", 0) == 0, \
                (f"{values['pictures_ready']:.0f} ready, {values.get('pictures_pending', 0):.0f} pending, "
                 f"{values.get('picture_bytes', 0) / 1048576:.1f} MiB")
        start = self.s.clock.now()
        ok, detail = self.wait(ready, T["pictures"])
        numbers = {name: seen.get(name) for name in ("pictures_ready", "pictures_pending", "picture_bytes")}
        if not ok and seen.get("pictures_ready", 0) > 0:
            ok = True  # pictures arrived; some are still on their way or did not come
        return self.record(label, "PASS" if ok else "FAIL", detail, seconds=round(self.s.clock.now() - start, 1), **numbers)

    def focus_ids(self, label):
        ok, detail = ob.pc_ag_focus_ids(self.at_home())
        return self.record(label, "PASS" if ok else "FAIL", detail)

    def texts(self, label):
        ok, detail = ob.pc_ag_texts(self.at_home(), self.s.read_log()[self.mark:])
        return self.record(label, "PASS" if ok else "FAIL", detail)

    def text_engine(self, label):
        ok, detail, numbers = ob.pc_ag_text_engine(self.need_frame())
        return self.record(label, "PASS" if ok else "FAIL", detail, **numbers)

    # (b) ---------------------------------------------------------------------------------
    def measured(self, label, phase, samples):
        if not samples:
            raise ob.Unjudged("no snapshot carried the frame's stats")
        summary = ob.ag_summary(samples)
        missing = [name for name in ob.AG_STATS if name not in summary]
        self.report[f"stats_{phase}"] = {"samples": len(samples), "summary": summary, "missing": missing}
        self.s.save(f"afterglow/stats-{phase}.json", {"samples": samples, "summary": summary, "missing": missing})
        ok, detail = ob.pc_ag_draws(summary)
        detail = f"{len(samples)} samples: {detail}" + (f"; the snapshot lacks {missing}" if missing else "")
        return self.record(label, "PASS" if ok else "FAIL", detail,
                           **{name: values["median"] for name, values in summary.items()})

    def stats_at_rest(self, label):
        self.awake()
        frame = self.need_frame()
        self.samples["rest"], self.phase = [], "rest"
        clock = self.s.clock
        start = clock.now()
        try:
            while clock.now() - start < T["rest"]:
                self.read()
                clock.sleep(0.5)
        finally:
            self.phase = None
        self.report["rest_screen"] = ob.ag_screen(frame)
        return self.measured(label, "rest", self.samples["rest"])

    def stats_moving(self, label):
        if not self.samples["moving"]:
            raise ob.Unjudged("no key was followed by a snapshot of the frame")
        return self.measured(label, "moving", self.samples["moving"])

    def output_size(self, label):
        self.read()
        size = ob.ag_output(self.frame, self.s.read_log()[self.mark:])
        if not size:
            raise ob.Unjudged("the snapshot does not carry the output size (stats.output_w, stats.output_h: asked for in "
                              "docs/afterglow/requests/drv.md) and the mount's line \"interface up, look ..., WxH\" is not "
                              "in the app's log; the capture's own size stands in")
        self.report["output"] = size
        return self.record(label, "PASS", f"{size['width']}x{size['height']}, from {size['source']}",
                           width=size["width"], height=size["height"])

    # (e) ---------------------------------------------------------------------------------
    def capture(self, label, name):
        """The development capture command: no pad, any screen. The file is
        the one the stick click writes on the player."""
        s = self.s
        self.need_app()
        self.awake()
        if self.frame is not None:  # a page at rest: the hand-over over, the focus ring and the hero's picture arrived
            self.wait(lambda: (not ob.ag_flag(self.read(), "moving"), "a hand-over still runs"), T["move"], 0.5)
            s.clock.sleep(T["settle"])
            self.read()
        shows = ob.ag_line(self.frame) if self.frame is not None else "the display, with no snapshot to say what it shows"
        before = {entry for entry in (s.console.listdir(ob.DATA) or {}) if ob.SHOT.match(entry)}
        mark = len(s.read_log())
        self.send("shot")
        seen = {}

        def written():
            said = ob.capture_verdict(s.read_log()[mark:])
            if said and said[0] != "wrote":
                raise GiveUp(f"the build refused the capture: {said[1]}")
            listing = s.console.listdir(ob.DATA) or {}
            new = sorted(entry for entry in listing if ob.SHOT.match(entry) and entry not in before)
            if not new:
                return False, "no new evo_shot file"
            size = listing[new[-1]]["size"]
            stable = size > 54 and seen.get(new[-1]) == size
            seen[new[-1]] = size
            return stable, f"{new[-1]} {size} bytes"
        ok, detail = self.wait(written, T["capture"], interval=2)
        if not ok:
            return self.record(label, "FAIL", detail)
        remote = max(seen)
        data = s.console.read(f"{ob.DATA}/{remote}") or b""
        facts, problem = ob.bmp_facts(data)
        if facts is None or len(data) != seen[remote]:
            return self.record(label, "FAIL", f"{remote} read back as {len(data)} bytes: {problem or 'size changed'}")
        s.save(f"{name}.bmp", data)
        files = [f"{name}.bmp"]
        try:
            s.save(f"{name}.png", ob.bmp_to_png(data))
            files.append(f"{name}.png")
        except (ValueError, MemoryError) as error:
            s.note(f"{name}.png was not written ({error}); the BMP is the evidence")
        ok, detail = ob.pc_capture(facts)
        drawn = self.report.get("output")
        if drawn and (drawn["width"], drawn["height"]) != (facts["width"], facts["height"]):
            detail += f" (the interface draws at {drawn['width']}x{drawn['height']})"
        self.report.setdefault("captures", {})[name] = {
            "files": files, "console_file": remote, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "width": facts["width"], "height": facts["height"], "colours": facts["colours"], "lit": facts["lit"],
            "brightness": facts["brightness"], "shows": shows}
        return self.record(label, "PASS" if ok else "FAIL", f"{' and '.join(files)}: {detail}; taken of {shows}",
                           width=facts["width"], height=facts["height"], bytes=len(data), colours=facts["colours"],
                           lit=facts["lit"])

    # (d) ---------------------------------------------------------------------------------
    def move_along(self, label):
        frame = self.at_home()
        shelf, index = ob.ag_place(frame)
        cards = len(dict(ob.ag_lists(frame)).get(shelf, []))
        if cards < 2:
            raise ob.Unjudged(f"the focused shelf \"{shelf}\" has {cards} card(s)")
        key, target = ("right", index + 1) if index + 1 < cards else ("left", index - 1)
        ok, detail = self.press(key, lambda: self.focus_is(shelf, target))
        return self.record(label, "PASS" if ok else "FAIL", f"{key}: {shelf}/{index} -> {detail}")

    def move_between(self, label):
        frame = self.at_home()
        rows = [shelf for shelf, titles in ob.ag_lists(frame) if titles]
        row, index = ob.ag_place(frame)
        if len(rows) < 2 or row not in rows:
            raise ob.Unjudged(f"Home lists {len(rows)} shelf with cards: there is no other shelf to move to")
        there, back = ("down", "up") if rows.index(row) + 1 < len(rows) else ("up", "down")
        other = rows[rows.index(row) + (1 if there == "down" else -1)]
        ok, detail = self.press(there, lambda: self.row_is(other))
        if not ok:
            return self.record(label, "FAIL", f"{there}: {row}/{index} -> {detail}")
        ok, came = self.press(back, lambda: self.focus_is(row, index))
        return self.record(label, "PASS" if ok else "FAIL", f"{there}: {row} -> {detail}; {back}: {came}")

    def rail(self, label):
        frame = self.at_home()
        shelf, index = ob.ag_place(frame)
        if index > STEPS_TO_RAIL:
            raise ob.Unjudged(f"the focus is {index} cards from the rail")
        for step in range(index, 0, -1):  # the rail is past the first card
            ok, detail = self.press("left", lambda step=step: self.focus_is(shelf, step - 1))
            if not ok:
                return self.record(label, "FAIL", f"left toward the first card: {detail}")

        def opened():
            frame = self.read()
            return ob.ag_flag(frame, "rail_open") and str(ob.ag_focus(frame).get("id", "")).startswith("rail/"), ob.ag_line(frame)
        ok, detail = self.press("left", opened)
        if not ok:
            return self.record(label, "FAIL", f"Left at the first card did not open the rail: {detail}")
        focus = ob.ag_focus(self.frame)
        self.s.save("afterglow/rail.json", self.snapshot)
        self.report["rail"] = {"focus": focus.get("id"), "text": focus.get("text"), "destinations": focus.get("total")}

        def closed():
            frame = self.read()
            return frame is not None and not ob.ag_flag(frame, "rail_open") and ob.ag_place(frame) == (shelf, 0), ob.ag_line(frame)
        # Right closes the rail. Circle would too, but on Home with the rail shut Circle asks to leave the app.
        ok, shut = self.press("right", closed)
        if not ok:
            self.lost = "the rail did not close"
        return self.record(label, "PASS" if ok else "FAIL",
                           f"left: {focus.get('id')} \"{focus.get('text')}\" of {focus.get('total')}; right: {shut}")

    def video_shelf(self, frame):
        """(Home's shelves with cards, the one to take a video from)."""
        rows = [shelf for shelf, titles in ob.ag_lists(frame) if titles]
        videos = [shelf for shelf in rows if shelf not in ob.AG_TOPIC_SHELVES]
        if not videos:
            raise ob.Unjudged(f"Home lists no shelf of videos (shelves: {rows or 'none'})")
        # Not "continue" when there is another: a started video may answer with the resume prompt.
        return rows, next((shelf for shelf in videos if shelf != "continue"), videos[0])

    def go_to(self, rows, target):
        """Down or Up, one shelf at a time, each read back: (on the shelf, detail)."""
        for _ in range(len(rows)):
            row = ob.ag_place(self.at_home())[0]
            if row == target or row not in rows:
                break
            key = "down" if rows.index(target) > rows.index(row) else "up"
            after = rows[rows.index(row) + (1 if key == "down" else -1)]
            ok, detail = self.press(key, lambda after=after: self.row_is(after))
            if not ok:
                return False, f"{key} toward the shelf \"{target}\": {detail}"
        return ob.ag_place(self.at_home())[0] == target, ob.ag_line(self.frame)

    def other_screen(self, label):
        key = self.s.args.afterglow_other
        if key == "none":
            raise ob.Unjudged("not asked for (--afterglow-other none)")
        # Square opens Search for a Guest only. For an account it keeps the video in focus for later
        # (an entry in that person's YouTube list): a session never presses it there and opens the
        # video's page instead.
        if key == "square" and ob.ag_state(self.at_home(), "profile") != "guest":
            key = "triangle"
        if key == "triangle":  # a video's page: the focus must be on a video
            rows, target = self.video_shelf(self.at_home())
            ok, detail = self.go_to(rows, target)
            if not ok:
                return self.record(label, "FAIL", detail)
        depth = len(ob.ag_stack(self.at_home()))

        def opened():
            frame = self.read()
            if frame is None:
                if ob.keyboard_open(self.snapshot):
                    raise GiveUp("a keyboard opened over the interface")
                return False, "no Afterglow snapshot"
            stack = ob.ag_stack(frame)
            return len(stack) > depth and stack[-1] != "home", ob.ag_line(frame)
        ok, detail = self.press(key, opened, T["screen"])
        if ob.keyboard_open(self.snapshot):
            self.send("kbclose")
        if not ok:
            if self.read() is None or len(ob.ag_stack(self.frame)) != depth or ob.ag_screen(self.frame) != "home":
                self.lost = f"after {key}: {ob.ag_line(self.frame)}"
            return self.record(label, "FAIL", f"{key} did not open another screen: {detail}")
        name = ob.ag_screen(self.frame)
        self.wait(lambda: (not ob.ag_flag(self.read(), "moving"), "the hand-over still runs"), T["move"], 0.5)
        self.s.save("afterglow/other-screen.json", self.snapshot)
        self.report["other_screen"] = {"key": key, "screen": name, "stack": ob.ag_stack(self.frame),
                                       "focus": ob.ag_focus(self.frame).get("id"), "stats": ob.ag_stats(self.frame)[0]}

        def home_again():
            frame = self.read()
            return frame is not None and len(ob.ag_stack(frame)) == depth and ob.ag_screen(frame) == "home", ob.ag_line(frame)
        came = "not tried"
        for _ in range(2):
            # Circle only while a screen lies above Home: on Home itself it asks to leave the app.
            if self.read() is None or len(ob.ag_stack(self.frame)) <= depth:
                break
            ok, came = self.press("circle", home_again, T["screen"])
            if ok:
                break
        ok, now = home_again()
        if not ok:
            self.lost = f"Circle did not return from {name}: {now}"
        return self.record(label, "PASS" if ok else "FAIL", f"{key} opened {name} ({detail}); Circle: {came if ok else now}")

    # (f) ---------------------------------------------------------------------------------
    def play(self, label):
        rows, target = self.video_shelf(self.at_home())
        ok, detail = self.go_to(rows, target)
        if not ok:
            return self.record(label, "FAIL", detail)
        # Home as it looks with a video in focus: the hero and the page's colour are that video's.
        self.judge("(e) scanout capture of Home, a video in focus", lambda step: self.capture(step, "afterglow-home-video"))
        outcome = "not tried"
        for attempt in (1, 2):
            frame = self.at_home()
            shelf, index = ob.ag_place(frame)
            titles = dict(ob.ag_lists(frame)).get(shelf, [])
            title = str(ob.ag_focus(frame).get("text", ""))
            kind = ob.ag_focus(frame).get("kind", "video")  # asked of the frame owner; a build that says it is believed
            # Cross is pressed only on a card that was read back as a listed video.
            if shelf != target or index >= len(titles) or not title.strip() or title != titles[index] or kind != "video":
                raise ob.Unjudged(f"the focus was not read back as a video of \"{target}\": {ob.ag_line(frame)}")
            outcome, detail = self.start(shelf, index, title)
            if outcome != "unresolved":
                break
            if attempt == 2 or index + 1 >= len(titles):
                return self.record(label, "FAIL", f"{shelf}/{index} \"{title[:50]}\": {detail}", hard=True)
            self.s.note(f"Afterglow: {shelf}/{index} \"{title[:50]}\" did not resolve ({detail}); the next card is tried once")
            ok, moved = self.press("right", lambda: self.focus_is(shelf, index + 1))
            if not ok:
                return self.record(label, "FAIL", f"{shelf}/{index} did not resolve and Right did not move on: {moved}", hard=True)
        if outcome == "screen":
            raise ob.Unjudged(f"Cross on {shelf}/{index} opened a screen, not the player: the card is not a video ({detail})")
        if outcome != "playing":
            return self.record(label, "FAIL", f"{shelf}/{index} \"{title[:50]}\": {detail}", hard=True)
        self.report["played"] = {"focus": f"{shelf}/{index}", "title": title}
        self.record(label, "PASS", f"{shelf}/{index} \"{title[:50]}\": player open, decoder active ({detail})", hard=True)
        window = []
        ok, detail = self.wait(lambda: (window.append(self.s.status()), ob.pc_playing(window))[1], T["playing"])
        self.record("(f) position advancing", "PASS" if ok else "FAIL", detail, hard=True)
        self.judge("(f) stopped through the confirmation", self.stop)
        self.judge("(f) the interface shown again is Afterglow", self.shown_again)
        return True

    def start(self, shelf, index, title):
        """Cross, then what the engine reports: ("playing" | "unresolved" |
        "screen" | "failed", detail)."""
        s = self.s
        depth = len(ob.ag_stack(self.frame))
        toasts = list((self.frame or {}).get("toasts") or [])
        self.awake()
        mark = self.play_mark = len(s.read_log())
        self.send("key cross")
        resumed, outcome = [], ["failed"]

        def opened():
            failed = ob.resolve_failed(s.read_log()[mark:])
            if failed:
                outcome[0] = "unresolved"
                raise GiveUp(f"the stream did not resolve: {failed}")
            status = s.status()
            if not status:
                return False, "no status"
            if status["fatal"]:
                raise GiveUp("decoder fatal")
            if status["scr"] == ob.SCREEN_RESUME and not resumed:
                resumed.append(True)
                s.note("Afterglow: a resume prompt appeared (saved position): accepted with Cross")
                self.send("key cross")
            if status["scr"] == ob.SCREEN_PLAYER and status["active"] == 1:
                return True, f"scr={status['scr']} active=1 fatal=0"
            if status["scr"] == ob.SCREEN_BROWSER and self.read() is not None:
                if len(ob.ag_stack(self.frame)) > depth:
                    outcome[0] = "screen"
                    raise GiveUp(ob.ag_line(self.frame))
                new = [text for text in (self.frame.get("toasts") or []) if text not in toasts]
                if new:
                    raise GiveUp(f"the interface answered with a toast: \"{new[-1]}\"")
            return False, f"scr={status['scr']} active={status['active']} fatal={status['fatal']}"
        ok, detail = self.wait(opened, T["open"])
        if outcome[0] == "screen":  # leave the screen it opened: Circle only above Home
            self.press("circle", lambda: (len(ob.ag_stack(self.read())) == depth, ob.ag_line(self.frame)), T["screen"])
            if self.read() is None or len(ob.ag_stack(self.frame)) != depth:
                self.lost = "Circle did not return from the screen Cross opened"
        return ("playing" if ok else outcome[0]), detail

    def stop(self, label):
        """Select Stop only from the mounted dialog's verified focus.
        Unknown or unresponsive focus uses the explicit development stop."""
        s = self.s
        self.send("key circle")
        ok, detail = self.wait(lambda: s.on_screen(ob.SCREEN_STOP_DIALOG), T["dialog"])
        if ok:
            focus = [None]
            def stop_focus(wanted=None):
                snapshot = s.ui()
                player = ob.ui_focus(snapshot)
                at = player.get("focus") or {}
                index = at.get("index")
                valid = (ob.ui_screen(snapshot) == ob.SCREEN_STOP_DIALOG and
                         "afterglow_player" in ob.ui_docs(snapshot) and
                         player.get("interface") == "afterglow" and player.get("screen") == "player" and
                         player.get("dialog") == 1 and index in (0, 1) and at.get("total") == 2 and
                         at.get("id") == f"dialog/{index}")
                focus[0] = index if valid else None
                return valid and (wanted is None or index == wanted), f"mounted stop focus={focus[0]}"
            ok, detail = self.wait(stop_focus, T["dialog"])
            s.save("afterglow/stop-dialog-ui.json", s.ui())
            if ok and focus[0] == 0:
                self.send("key right")
                ok, detail = self.wait(lambda: stop_focus(1), T["dialog"])
            if ok and focus[0] == 1:
                self.send("key cross")
                ok, detail = self.wait(s.inactive, T["stopped"])
        self.record(label, "PASS" if ok else "FAIL", detail)
        if not ok:
            if not s.inactive()[0]:
                self.send("stop")
            ok, detail = self.wait(s.inactive, T["stopped"])
            self.record("(f) player retired after the stop command", "PASS" if ok else "FAIL", detail, hard=True)
        stats = ob.parse_stats(s.read_log()[self.play_mark:])
        if stats:
            self.report["playback_stats"] = stats
            self.record("(f) engine statistics for the playback", "PASS" if ob.pc_stats(stats)[0] else "FAIL", ob.pc_stats(stats)[1])
        return ok

    def shown_again(self, label):
        def again():
            frame = self.read()
            if frame is not None:
                return True, ob.ag_line(frame)
            return False, ob.ui_line(self.snapshot)
        ok, detail = self.wait(again, T["back"])
        if ok:
            self.s.save("afterglow/after-playback.json", self.snapshot)
        return self.record(label, "PASS" if ok else "FAIL", detail)

    # (s) ---------------------------------------------------------------------------------
    def stroll(self, frame):
        """Presses that only move the focus on Home and bring it back: along
        the shelf and back, to the next shelf and back. None of them can play,
        open or change anything. [(key, the probe for what it must show)]"""
        shelf, index = ob.ag_place(frame)
        rows = [name for name, titles in ob.ag_lists(frame) if titles]
        cards = len(dict(ob.ag_lists(frame)).get(shelf, []))
        plan = []
        if cards > 1:  # Left is never pressed on the first card: past it is the rail
            there, back, beside = ("right", "left", index + 1) if index + 1 < cards else ("left", "right", index - 1)
            plan += [(there, lambda: self.focus_is(shelf, beside)), (back, lambda: self.focus_is(shelf, index))]
        if len(rows) > 1 and shelf in rows:
            there, back = ("down", "up") if rows.index(shelf) + 1 < len(rows) else ("up", "down")
            other = rows[rows.index(shelf) + (1 if there == "down" else -1)]
            plan += [(there, lambda: self.row_is(other)), (back, lambda: self.focus_is(shelf, index))]
        return plan

    def sound(self, label):
        """The sound module's own audio port, which it opens on a thread of
        its own at its first voice, beside the player's. The file that kept
        the module silent is removed, the app is given time to notice, and the
        focus is moved on Home: each move is a cue. Recorded from the app's
        log: the recordings found, the port open or the line of its failure;
        from two reads of the snapshot: that the frame goes on.

        A PASS or a NOTE, never a FAIL: nothing here is a gate. An interface
        that is lost says so and gets no further key; switching off and the
        removal of the files follow as after any step."""
        s = self.s
        name = ob.override_names(ob.AFTERGLOW_SOUND_OFF)[0]
        if ob.AFTERGLOW_SOUND_OFF not in self.written or ob.AFTERGLOW_SOUND_OFF in self.gone:
            raise ob.Unjudged(f"{name} is not in place: the sound module was not kept silent by this stage")
        try:
            plan = self.stroll(self.at_home())
            if not plan:
                raise ob.Unjudged("Home has a single card: no press only moves the focus")
            earlier = self.read()
            s.clock.sleep(T["beat"])
            beats, counted = ob.pc_ag_beat(earlier, self.read())
            if beats is not True:
                if beats is False:
                    self.alive = False
                    self.lost = f"lost before the sound step: {counted}"
                raise ob.Unjudged(f"frame progress was not established before enabling sound: {counted}")
        except ob.Unjudged as why:  # an interface not known to be on Home keeps its silence
            raise ob.Unjudged(f"{why}; {name} stays in place until the interface is switched off: the port is not asked for") from None
        self.deadline = max(self.deadline, s.clock.now() + T["sound"])
        before = ob.ag_sound(s.read_log()[self.mark:])
        left = self.remove([ob.AFTERGLOW_SOUND_OFF])
        if any(part in left for part in ob.override_names(ob.AFTERGLOW_SOUND_OFF)):
            raise ob.Unjudged(f"{name} is still listed after its removal: the sound module stays silent")
        s.log(f"   {name} removed: the interface's next cue asks for its own audio port")
        pressed, stuck, refused = [], "", ""
        try:
            s.clock.sleep(T["sound_look"])  # the app looks at the file every two seconds
            for key, probe in plan:
                ok, detail = self.press(key, probe)
                pressed.append(key)
                if not ok:  # no further key to an interface that did not follow this one
                    stuck = f"{key}: {detail}"
                    break
            if not stuck:
                self.wait(lambda: (ob.ag_sound(s.read_log()[self.mark:])["port"] is not None, "no line of the port yet"),
                          T["sound_line"])
        except self.Stop as stop:  # a command the app did not take
            self.alive, refused = False, stop.reason
        earlier = self.read()
        s.clock.sleep(T["beat"])
        beats, counted = ob.pc_ag_beat(earlier, self.read())
        said = ob.ag_sound(s.read_log()[self.mark:])
        port, bank = said["port"], said["recordings"]
        facts = [f"{name} removed, {T['sound_look']} s for the app to notice"]
        if refused:
            facts.append(f"after {', '.join(pressed) or 'no press'}: {refused}")
        elif stuck:
            facts.append(f"the focus did not follow {stuck}")
        else:
            facts.append(f"{', '.join(pressed)}: the focus followed each press")
        facts.append({"open": "the port opened", "failed": "THE PORT DID NOT OPEN, the interface stays silent"}.get(
            port, "no line of the port in the app's log") + (f" (\"{said['line']}\")" if said["line"] else ""))
        if not bank:
            facts.append("no line of the recordings in the stage's part of the log")
        else:
            facts.append(f"{bank['files']} recordings, {bank['served']} cues served, {bank['silent']} silent"
                         + ("" if bank["served"] else ": no cue has a recording, so no voice is posted and no port is asked for"))
        facts.append(f"the port's line was already there while {name} was in place" if before["port"]
                     else "no line of the port while the file was in place")
        if said["vibration"]:
            facts.append(f"the pad was asked for a pulse although afterglow-vibration.off is in place (\"{said['vibration'][-1]}\")")
        if refused or beats is False:
            self.lost = f"lost in the sound step: {refused or counted}"
            facts.append(f"THE INTERFACE IS LOST after the sound was let go: {counted}")
        else:
            facts.append(f"the frame goes on: {counted}" if beats else f"whether the frame goes on is not known: {counted}")
            s.save("afterglow/after-sound.json", self.snapshot or {})
        ok = port == "open" and bank is not None and beats is True and not (stuck or refused or before["port"] or said["vibration"])
        counter = [ob.ag_frames(earlier), ob.ag_frames(self.frame)]
        self.report["sound"] = {"port": port, "line": said["line"], "handle": said["handle"], "recordings": bank,
                                "silent_while_off": before["port"] is None, "keys": pressed, "frames": counter,
                                "advancing": beats, "lost": bool(self.lost), "vibration": said["vibration"],
                                "other": said["other"]}
        numbers = {"port": port or "none", "presses": len(pressed), "frames_before": counter[0], "frames_after": counter[1]}
        if bank:
            numbers.update(recordings=bank["files"], cues_served=bank["served"], cues_silent=bank["silent"])
        return self.record(label, "PASS" if ok else "NOTE", "; ".join(facts), **numbers)

    # (g) ---------------------------------------------------------------------------------
    def switch_off(self, label):
        """Remove the switch and write nothing: the classic browser draws
        again. Then remove whatever else the stage wrote, so that the console
        is as the stage found it. The switch goes first: a file that keeps the
        sound module silent is taken away only when the interface no longer
        draws, unless the sound step has already let it go."""
        s = self.s
        if not self.written:
            return None
        back = True
        if self.on:
            status = s.status()
            if status and status["active"] == 1 and self.alive:  # a video would hide the browser: the engine's own stop
                try:
                    self.send("stop")
                    self.wait(s.inactive, T["stopped"])
                except self.Stop as stop:
                    self.alive = False
                    s.note(f"Afterglow: {stop.reason}")
            gone = ob.override_names(ob.AFTERGLOW_ON)[0] not in self.remove([ob.AFTERGLOW_ON])
            self.record("(g) afterglow.on removed, nothing else written", "PASS" if gone else "FAIL",
                        "the file is gone from the data folder" if gone else "the file is still listed", hard=True)

            back, detail = self.wait(lambda: ob.pc_classic_again(s.ui(), self.classic), T["off"])
            self.record("(g) the classic browser draws again", "PASS" if back else "FAIL", detail, hard=True)
        # The switch first here too, for a stage that could not write it whole.
        left = self.remove(sorted((path for path in self.written if path not in self.gone), key=lambda path: path != ob.AFTERGLOW_ON))
        names = ", ".join(ob.override_names(path)[0] for path in self.written)
        clean = self.record(REMOVED_STEP, "PASS" if not left else "FAIL",
                            f"{names}: none is listed in the data folder any more" if not left else f"still listed: {', '.join(left)}",
                            hard=True)
        return back and clean

    def summarize(self):
        steps = [entry for entry in self.s.steps if entry.get("stage") == "afterglow"]
        failed = [entry["step"] for entry in steps if entry["result"] == "FAIL"]
        notes = [entry["step"] for entry in steps if entry["result"] == "NOTE"]
        self.report.update(
            result="fail" if failed else "not-run" if not self.on else "partial" if notes else "pass",
            passed=sum(entry["result"] == "PASS" for entry in steps), failed=failed, notes=notes,
            hard_failed=[entry["step"] for entry in steps if entry["result"] == "FAIL" and entry["gate"] == "hard"])
        self.s.log(f"   Afterglow: {self.report['result'].upper()} - {self.report['passed']} passed, {len(failed)} failed"
                   f"{' ' + str(failed) if failed else ''}, {len(notes)} not judged{' ' + str(notes) if notes else ''}")
