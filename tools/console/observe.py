"""Pure parts of the console session: no sockets, no subprocesses.

Parsers for the development status line, the UI snapshot and the diagnostic
log, the postconditions each scripted step must observe, the shared lock and
the frozen-candidate check.
"""
import hashlib
import json
import os
import re
from pathlib import Path

TITLE = "PPSA99142"
DATA = "/data/prosperotube"
CMD_FILE = f"{DATA}/dev_command"
STATUS_FILE = f"{DATA}/dev_status"
UI_FILE = f"{DATA}/dev_ui.json"
LOG_FILE = f"{DATA}/console.log"
STATE_FILE = f"{DATA}/youtube.json"

# Written by this session or by the instrumented build; never restored, removed at the end.
DEV_FILES = ("dev_command", "dev_status", "dev_status.tmp", "dev_ui.json", "dev_ui.json.tmp",
             ".dev_command.upload")
# The app's own diagnostics: left in place, never part of the provider state.
DIAGNOSTIC = re.compile(r"^(console\.log(\.1)?|lapy-helper\.log|compatibility_report\.txt)$")
SHOT = re.compile(r"^evo_shot_\d{3}\.bmp$")
# Provider state this session may create when the console had none.
STATE_CREATED = ("youtube.json", "youtube.json.tmp", "active.srt", "active.mp4")

# evo::ScreenId as dev_status reports it (scr=). The status line is written by
# the engine whatever draws the screen, so these identify a screen in both
# interface layouts; the UI snapshot only adds what is on it.
SCREEN_PLAYER = 2
SCREEN_RESUME = 3
SCREEN_CAPTIONS = 18
SCREEN_STOP_DIALOG = 20
SCREEN_BROWSER = 23
SCREEN_OPTIONS = 31
SCREEN_CLOSED = 32
# The browser is the provider's own RmlUi document ("youtube": cards without
# ids, focus by position) on the console; the kit's grid ("hui_provider",
# row-N ids) is the other layout the build can draw.
BROWSER_DOCS = ("youtube", "hui_provider")
KEYBOARD_DOCS = ("keyboard", "hui_keyboard")
BUILD_ID = re.compile(rb"prosperotube-[0-9a-f]{7,8}_\d{4}-\d{4}")

_INT = ("t", "scr", "be", "fatal", "eof", "complete", "active", "parked")
_FLOAT = ("pos", "dur", "fps")


def parse_status(data):
    """The status line as a dict, or None for anything but one complete line."""
    if not data:
        return None
    text = data.decode("utf-8", "replace") if isinstance(data, bytes) else data
    if not text.endswith("\n") or text.count("\n") != 1:
        return None
    fields = dict(part.split("=", 1) for part in text.split() if "=" in part)
    if "build" not in fields or any(key not in fields for key in _INT + _FLOAT):
        return None
    try:
        status = {"build": fields["build"]}
        status.update({key: int(fields[key]) for key in _INT})
        status.update({key: float(fields[key]) for key in _FLOAT})
    except ValueError:
        return None
    return status


def parse_ui(data):
    """{"seq", "t_ms", "ui"} from dev_ui.json, or None when it is not a snapshot."""
    if not data:
        return None
    try:
        snapshot = json.loads(data)
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(snapshot, dict) or not isinstance(snapshot.get("ui"), dict) or \
            not isinstance(snapshot.get("seq"), int):
        return None
    return snapshot


def ui_docs(snapshot):
    """Every visible document: a kit screen first ("hui_*"), then RmlUi's own
    ("youtube", "keyboard", "dialog"...). Never rely on one name alone."""
    return [str(doc) for doc in (snapshot or {}).get("ui", {}).get("docs") or []]


def ui_doc(snapshot):
    docs = ui_docs(snapshot)
    return docs[0] if docs else ""


def ui_modal(snapshot):
    return (snapshot or {}).get("ui", {}).get("modal") or {}


def keyboard_open(snapshot):
    """The app's own keyboard is up: RmlUi reports it as a modal over the
    browser, the kit as its own document over a kit screen."""
    return ui_modal(snapshot).get("kind") == "keyboard" or any(doc in KEYBOARD_DOCS for doc in ui_docs(snapshot))


def focus_kind(snapshot):
    """"folder", "playable" or None when the layout does not say (the kit's
    rows carry no class): the provider's cards are "tube-card [folder]"."""
    classes = str(ui_focus(snapshot).get("class", "")).split()
    if "tube-card" not in classes:
        return None
    return "folder" if "folder" in classes else "playable"


def ui_screen(snapshot):
    return ((snapshot or {}).get("ui", {}).get("screen") or {}).get("id", -1)


def ui_focus(snapshot):
    return (snapshot or {}).get("ui", {}).get("focused") or {}


def ui_rows(snapshot):
    return [str(item.get("text", "")) for item in ui_focus(snapshot).get("focusable") or []]


def ui_paused(snapshot):
    player = (snapshot or {}).get("ui", {}).get("native_player") or {}
    return bool(player.get("paused")) or ui_focus(snapshot).get("id") == "osd-paused"


def ui_line(snapshot):
    """One line for the log: what a person would say is on screen."""
    focus = ui_focus(snapshot)
    return (f"seq={(snapshot or {}).get('seq')} screen={ui_screen(snapshot)} docs={','.join(ui_docs(snapshot)) or '-'} "
            f"focus={focus.get('id') or focus.get('class') or '-'} \"{str(focus.get('text', ''))[:60]}\" "
            f"({focus.get('index', 0)}/{focus.get('total', 0)})")


# --- postconditions: each returns (ok, detail) -------------------------------

def pc_heartbeat(previous, current, build):
    """The exact build is running and its frame loop moved between two reads."""
    if not current:
        return False, "no status"
    if current["build"] != build:
        return False, f"build {current['build']} is not the candidate's {build}"
    if current["parked"]:
        return False, "parked"
    if not previous or current["t"] <= previous["t"]:
        return False, f"t={current['t']} not advancing yet"
    return True, f"build {build}, t {previous['t']} -> {current['t']}"


def pc_browser(snapshot):
    """The browser is drawn with a cursor on one of its rows. Either layout: the
    screen is named by its number or its document, the cursor by its position
    among the rows (the provider's cards have no id)."""
    if ui_screen(snapshot) != SCREEN_BROWSER and not any(doc in BROWSER_DOCS for doc in ui_docs(snapshot)):
        return False, f"screen {ui_screen(snapshot)}, docs {ui_docs(snapshot) or 'none'}"
    focus = ui_focus(snapshot)
    rows = ui_rows(snapshot)
    if not rows or not 1 <= int(focus.get("index") or 0) <= len(rows):
        return False, f"browser without a cursor on a row: {ui_line(snapshot)}"
    return True, f"row {focus.get('index')} of {focus.get('total', len(rows))}: {rows[:8]}"


def next_key(current, target, columns, blocked):
    """The D-pad press that moves the cursor from row `current` toward `target`
    (1-based positions in a grid read left to right). Before a vertical move
    showed how many columns the grid has, go sideways first and down when
    sideways did nothing; `blocked` holds the presses that did nothing here."""
    forward = target > current
    if columns:
        same_row = (target - 1) // columns == (current - 1) // columns
        choices = [("right" if forward else "left")] if same_row else [("down" if forward else "up")]
    else:
        choices = ["right", "down"] if forward else ["left", "up"]
    return next((key for key in choices if key not in blocked), None)


_CATALOG = re.compile(r"provider: '([^']*)' catalog parent='([^']*)' -> (\d+) rows "
                      r"\(folder_level=(\d+) has_more=(\d+) total=(-?\d+)\)")
_ACTIVATE = re.compile(r"provider: '[^']*' activate '([^']*)' \((folder|playable)\) title='(.*)'\s*$")
_SEARCH = re.compile(r"provider: Search query='(.*)'\s*$")


def catalog_lines(log_text):
    """Every list the provider handed to the browser, in order. A list under a
    network parent (a topic, a search) exists only if the service answered."""
    return [{"parent": m.group(2), "rows": int(m.group(3)), "folders": m.group(4) == "1", "more": m.group(5) == "1"}
            for m in _CATALOG.finditer(log_text)]


def activations(log_text):
    return [{"id": m.group(1), "kind": m.group(2), "title": m.group(3)}
            for line in log_text.splitlines() if (m := _ACTIVATE.search(line))]


def searches(log_text):
    return [m.group(1) for line in log_text.splitlines() if (m := _SEARCH.search(line))]


def pc_catalog(log_text, seen, network=False):
    """A list arrived after the first `seen` ones, with rows. network=True also
    wants a parent that is not a local menu."""
    new = catalog_lines(log_text)[seen:]
    if not new:
        return False, "no new catalog line in console.log"
    last = new[-1]
    if last["rows"] <= 0:
        return False, f"catalog parent='{last['parent']}' returned no rows"
    if network and last["parent"] in ("", "discover"):
        return False, f"the newest list is the local menu '{last['parent']}'"
    return True, f"catalog parent='{last['parent']}' -> {last['rows']} rows ({'folders' if last['folders'] else 'videos'})"


def resolve_failed(log_text):
    return next((line.strip()[-120:] for line in log_text.splitlines() if "provider: resolve failed" in line), None)


def pc_doc(snapshot, name, after_seq=-1):
    if (snapshot or {}).get("seq", -1) <= after_seq:
        return False, "no new snapshot"
    return ui_doc(snapshot) == name, f"doc {ui_doc(snapshot) or 'none'}"


def find_row(snapshot, pattern):
    """1-based index of the first row whose title matches, else 0."""
    expression = re.compile(pattern, re.IGNORECASE)
    for index, text in enumerate(ui_rows(snapshot), 1):
        if expression.search(text):
            return index
    return 0


def pc_rows(snapshot, pattern):
    """A listed row matches, whatever document lists it; not under a keyboard."""
    if keyboard_open(snapshot):
        return False, "a keyboard is open"
    index = find_row(snapshot, pattern)
    rows = ui_rows(snapshot)
    return bool(index), f"row {index} of {len(rows)} matches /{pattern}/: {rows[:6]}"


def pc_playing(samples, minimum=0.5):
    """Decoder active without a fatal error and the position moved with the clock."""
    live = [s for s in samples if s and s["scr"] == SCREEN_PLAYER and s["active"] == 1]
    if any(s["fatal"] for s in samples if s):
        return False, "decoder fatal"
    if len(live) < 2:
        return False, "player not active yet"
    first, last = live[0], live[-1]
    moved = last["pos"] - first["pos"]
    if last["t"] <= first["t"] or moved < minimum:
        return False, f"pos {first['pos']:.2f} -> {last['pos']:.2f}"
    return True, (f"backend {last['be']}, {last['fps']:.1f} fps, pos {first['pos']:.2f} -> "
                  f"{last['pos']:.2f} of {last['dur']:.1f} in {last['t'] - first['t']} s")


def pc_paused(samples, snapshot, tolerance=0.05):
    """The position held still while the decoder stayed active: across two
    status writes when the snapshot also says paused (the engine's own flag),
    across three when the snapshot is silent."""
    live = [s for s in samples if s]
    need = 2 if ui_paused(snapshot) else 3
    if len(live) < need or any(b["t"] <= a["t"] for a, b in zip(live[-need:], live[-need + 1:])):
        return False, f"need {need} status writes"
    held = live[-need:]
    if any(s["active"] != 1 or s["fatal"] for s in held):
        return False, "decoder not active"
    if max(s["pos"] for s in held) - min(s["pos"] for s in held) > tolerance:
        return False, f"pos still moving {held[0]['pos']:.2f} -> {held[-1]['pos']:.2f}"
    return True, (f"held at {held[-1]['pos']:.2f} for {held[-1]['t'] - held[0]['t']} s"
                  f"{'' if ui_paused(snapshot) else ' (snapshot silent: by position alone)'}")


def pc_seek(before, sample, delta, tolerance=2.0):
    """Position landed near before+delta (capped by the duration)."""
    if not sample or sample["fatal"]:
        return False, "no status or decoder fatal"
    target = before + delta
    if sample["dur"] > 0:
        target = min(target, sample["dur"])
    if sample["pos"] < target - tolerance:
        return False, f"pos {sample['pos']:.2f}, target {target:.2f}"
    if sample["pos"] > target + delta:
        return False, f"pos {sample['pos']:.2f} overshot target {target:.2f}"
    return True, f"pos {before:.2f} -> {sample['pos']:.2f} (target {target:.2f})"


def pc_complete(sample):
    if not sample:
        return False, "no status"
    if sample["fatal"]:
        return False, "decoder fatal"
    return sample["complete"] == 1, (f"eof={sample['eof']} complete={sample['complete']} "
                                     f"pos {sample['pos']:.2f} of {sample['dur']:.1f}")


def pc_dialog(snapshot, label):
    focus = ui_focus(snapshot)
    ok = ui_doc(snapshot) == "hui_dialog" and str(focus.get("text", "")).upper() == label.upper()
    return ok, f"doc {ui_doc(snapshot) or 'none'}, focused \"{focus.get('text', '')}\" of {ui_rows(snapshot)}"


QUALITY = r"video presets|\b\d{3,4}p\b"


def quality_rows(rows):
    expression = re.compile(QUALITY, re.IGNORECASE)
    return [row for row in rows if expression.search(row)]


def caption_tracks(rows):
    """Tracks a viewer could turn on, and the ones marked as showing now."""
    offered = [row for row in rows if not re.match(r"\s*(off|none|disabled?|no subtitles)\b", row, re.IGNORECASE)]
    return offered, [row for row in rows if "(current)" in row]


_STATS = re.compile(r"stats frames_in=(\d+) converted=(\d+) published=(\d+) late_drop=(\d+) seek_disc=(\d+)")
_SEEKS = re.compile(r"stats seek_req=(\d+) ok=(\d+) fail=(\d+)")


def parse_stats(log_text):
    """The last engine stop statistics in a piece of the log, or None."""
    stats = None
    for match in _STATS.finditer(log_text):
        stats = dict(zip(("frames_in", "converted", "published", "late_drop", "seek_discarded"),
                         map(int, match.groups())))
    if stats is None:
        return None
    for match in _SEEKS.finditer(log_text):
        stats.update(zip(("seek_requests", "seek_ok", "seek_failed"), map(int, match.groups())))
    return stats


def pc_stats(stats):
    if not stats:
        return False, "no stop statistics in the log yet"
    if stats["published"] <= 0:
        return False, f"no frame published: {stats}"
    late = stats["late_drop"] / stats["published"]
    return True, (f"{stats['published']} frames published, {stats['late_drop']} late drops "
                  f"({late:.1%}{', ABOVE 5%' if late > 0.05 else ''}), seeks ok {stats.get('seek_ok', '?')} "
                  f"failed {stats.get('seek_failed', '?')}")


def storage_verdict(log_text):
    """"ready", "failed" or None from the Lapy line of this launch."""
    verdict = None
    for line in log_text.splitlines():
        if "Lapy: path=" not in line:
            continue
        if "storage=ready" in line:
            verdict = "ready"
        elif "storage=failed" in line or "durable storage unavailable" in line:
            verdict = "failed"
    return verdict


def parse_state(data):
    try:
        state = json.loads(data) if data else None
    except (ValueError, UnicodeDecodeError):
        return None
    return state if isinstance(state, dict) else None


SERVICE_LINE = re.compile(r"embedded service: (listening on 127\.0\.0\.1:(\d+)|did not start: (.*))")


def pc_service(log_text):
    """The app's log says its built-in service listens on a loopback port in the
    console's usable range, and no line about it shows a token."""
    found = None
    for line in log_text.splitlines():
        match = SERVICE_LINE.search(line)
        if not match:
            continue
        if re.search(r"[0-9A-Fa-f]{32,}|Bearer", line):
            return False, "a service line in the log carries what looks like a token"
        found = match
    if found is None:
        return False, "no service line in console.log yet"
    if found.group(3) is not None:
        return False, f"the service did not start: {found.group(3)[:80]}"
    port = int(found.group(2))
    if not 20000 <= port <= 39999:
        return False, f"the service listens on port {port}, outside 20000-39999"
    return True, f"listening on 127.0.0.1:{port}"


def service_failed(log_text):
    return any((match := SERVICE_LINE.search(line)) and match.group(3) is not None for line in log_text.splitlines())


def pc_state_local(data):
    """The saved provider state never holds the loopback address or its token:
    the service's address lives in memory only."""
    state = parse_state(data)
    if state is None:
        return False, "no readable youtube.json"
    if "127.0.0.1" in str(state.get("source", "")):
        return False, "the loopback address was written to youtube.json"
    return True, "youtube.json holds no service address"


def history_record(data, video, title=None):
    """The record of the video that was activated: by its id (either may carry
    a prefix), else by its exact title."""
    records = [r for r in (parse_state(data) or {}).get("records") or [] if isinstance(r, dict)]
    return (next((r for r in records if r.get("id") == video), None)
            or next((r for r in records if video and r.get("id")
                     and (video in str(r["id"]) or str(r["id"]) in video)), None)
            or next((r for r in records if title and r.get("title") == title), None))


def pc_history(data, video, need_played, title=None):
    record = history_record(data, video, title)
    if record is None:
        return False, f"no record for {video}"
    detail = (f"played={record.get('played')} position={record.get('position')} "
              f"duration={record.get('duration')} title=\"{str(record.get('title', ''))[:40]}\"")
    if need_played:
        return bool(record.get("played")), detail
    return bool(record.get("played")) or float(record.get("position") or 0) > 0, detail


def redact_state(data):
    """A copy of the provider state safe to print: the token is not shown."""
    state = parse_state(data)
    if state is None:
        return None
    return {"source": re.sub(r"#.*", "#<code>", str(state.get("source", ""))),
            "token": "<present>" if state.get("token") else "<none>",
            "records": len(state.get("records") or [])}


# --- the Afterglow interface ---------------------------------------------------
# docs/afterglow/ARCHITECTURE.md, section 12a. While the interface is shown the
# instrumented build reports the document "afterglow" and puts the frame's own
# snapshot (Frame::snapshot_json) where the classic layouts put the focused
# element: dev_ui.json is {"seq", "t_ms", "ui": {"docs": ["afterglow", ...],
# "focused": {"interface": "afterglow", "screen", "stack", "focus", "state",
# "stats", ...}}}. The screen's own values under "state" are text.

AFTERGLOW_DOC = "afterglow"
AFTERGLOW_ON = f"{DATA}/afterglow.on"     # the session's switch: looked at once a second by the app
AFTERGLOW_OFF = f"{DATA}/afterglow.off"   # keeps the classic browser; never written by the session
AFTERGLOW_FILES = ("afterglow.on", ".afterglow.on.upload")
AG_STATS = ("frame_ms", "worst_frame_ms", "update_ms", "draw_ms", "instances", "runs", "fill",
            "pictures_ready", "pictures_pending", "picture_bytes", "data_pending")
# Asked of the frame owner (docs/afterglow/requests/drv.md) and read as soon as a build reports them.
AG_TEXT_STATS = ("text_engine", "glyph_pages", "glyphs", "fonts", "missing_glyphs")
# Home's shelves of tiles (native/afterglow/data.cpp): Cross opens a list there, not a video.
AG_TOPIC_SHELVES = ("topics",)
# What the candidate's eboot must hold for the stage: the mount's override file name and the
# breadcrumb of the development capture command (scripts/prepare_native.py).
AG_MOUNT_MARK = b"afterglow.on"
AG_CAPTURE_MARK = b"DEV_CAPTURE"


class Unjudged(Exception):
    """A step of the Afterglow stage cannot be judged from what the build reported."""


def parse_ui_repaired(data):
    """dev_ui.json that the strict reader refused, after the two repairs a C
    writer makes necessary: bytes that are not UTF-8 (a title cut inside a
    character) and what printf writes for a number that is none (nan, inf).
    Returns (snapshot or None, the repairs made)."""
    if not data:
        return None, []
    repairs = []
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        text = data.decode("utf-8", "replace")
        repairs.append("bytes that are not UTF-8")
    fixed = re.sub(r'(":\s*)-?(?:nan|inf(?:inity)?)\b', r"\1null", text, flags=re.IGNORECASE)
    if fixed != text:
        repairs.append("nan or inf written as a number")
    try:
        snapshot = json.loads(fixed)
    except ValueError:
        return None, repairs
    if not isinstance(snapshot, dict) or not isinstance(snapshot.get("ui"), dict) or \
            not isinstance(snapshot.get("seq"), int):
        return None, repairs
    return snapshot, repairs


def ag_shown(snapshot):
    """The frame's document is listed: the interface drew the last frame."""
    return AFTERGLOW_DOC in ui_docs(snapshot)


def ag_raw_shown(data):
    """The same fact from bytes that could not be read as a snapshot."""
    return bool(data) and re.search(rb'"interface"\s*:\s*"afterglow"|"docs"\s*:\s*\[\s*"afterglow"', data) is not None


def ag_frame(snapshot):
    """The frame's own snapshot, or None: not shown, or an RmlUi keyboard or
    dialog is up and the report names its element instead."""
    focused = (snapshot or {}).get("ui", {}).get("focused")
    if not ag_shown(snapshot) or not isinstance(focused, dict) or focused.get("interface") != "afterglow":
        return None
    return focused


def ag_screen(frame):
    return str((frame or {}).get("screen", ""))


def ag_stack(frame):
    stack = (frame or {}).get("stack")
    return [str(entry.get("screen", "")) for entry in stack if isinstance(entry, dict)] if isinstance(stack, list) else []


def ag_flag(frame, name):
    """One of the frame's numbers (moving, rail_open, attract, dialog) as a bool."""
    value = (frame or {}).get(name)
    return isinstance(value, (int, float)) and value != 0


def ag_focus(frame):
    focus = (frame or {}).get("focus")
    return focus if isinstance(focus, dict) else {}


def ag_state(frame, name, default=""):
    state = (frame or {}).get("state")
    return str(state.get(name, default)) if isinstance(state, dict) else default


def ag_lists(frame):
    """Home's shelves in the order they are drawn: [(shelf id, [titles])],
    from state.lists["shelf:<id>"]."""
    state = (frame or {}).get("state")
    lists = state.get("lists") if isinstance(state, dict) else None
    if not isinstance(lists, dict):
        return []
    return [(name[len("shelf:"):], [str(title) for title in titles])
            for name, titles in lists.items() if name.startswith("shelf:") and isinstance(titles, list)]


def ag_place(frame):
    """(shelf, index) of the focus on Home: its id is "<shelf>/<index>"."""
    match = re.fullmatch(r"(.+)/(\d+)", str(ag_focus(frame).get("id", "")))
    return (match.group(1), int(match.group(2))) if match else None


def ag_line(frame):
    """One line for the log: what the frame says is on screen."""
    if frame is None:
        return "no Afterglow snapshot"
    focus = ag_focus(frame)
    extras = "".join(f" {name}" for name in ("moving", "rail_open", "attract", "dialog") if ag_flag(frame, name))
    return (f"screen={ag_screen(frame) or '-'} stack={'>'.join(ag_stack(frame)) or '-'} "
            f"focus={focus.get('id') or '-'} \"{str(focus.get('text', ''))[:60]}\"{extras}")


def ag_stats(frame):
    """(the stats that are numbers, the names that are missing or are not)."""
    stats = (frame or {}).get("stats")
    if not isinstance(stats, dict):
        return {}, list(AG_STATS)
    values = {name: stats[name] for name in AG_STATS
              if isinstance(stats.get(name), (int, float)) and not isinstance(stats.get(name), bool)}
    return values, [name for name in AG_STATS if name not in values]


def ag_summary(samples):
    """{name: {"min", "median", "max"}} over the samples that carry the name."""
    summary = {}
    for name in AG_STATS:
        values = sorted(sample[name] for sample in samples if name in sample)
        if values:
            middle = values[len(values) // 2] if len(values) % 2 else (values[len(values) // 2 - 1] + values[len(values) // 2]) / 2
            summary[name] = {"min": values[0], "median": middle, "max": values[-1]}
    return summary


def ag_summary_line(summary):
    """The numbers a person reads first, on one line."""
    def middle(name, form="{:.1f}"):
        return form.format(summary[name]["median"]) if name in summary else "?"

    def top(name, form="{:.1f}"):
        return form.format(summary[name]["max"]) if name in summary else "?"
    megabytes = f"{summary['picture_bytes']['max'] / 1048576:.1f} MiB" if "picture_bytes" in summary else "? MiB"
    return (f"frame {middle('frame_ms')} ms (max {top('frame_ms')}, worst of 120 {top('worst_frame_ms')}), "
            f"update {middle('update_ms', '{:.2f}')} ms, draw {middle('draw_ms', '{:.2f}')} ms, "
            f"{middle('instances', '{:.0f}')} instances in {middle('runs', '{:.0f}')} runs, fill {middle('fill', '{:.2f}')}, "
            f"pictures {top('pictures_ready', '{:.0f}')} ready / {top('pictures_pending', '{:.0f}')} pending ({megabytes}), "
            f"data pending {top('data_pending', '{:.0f}')}")


def pc_ag_draws(summary):
    """The frame reports that it draws: a frame time, shapes and draw calls."""
    missing = [name for name in ("frame_ms", "instances", "runs") if name not in summary]
    if missing:
        raise Unjudged(f"the snapshot's stats lack {missing}")
    if summary["frame_ms"]["median"] <= 0 or summary["instances"]["max"] <= 0 or summary["runs"]["max"] <= 0:
        return False, "the frame reports no frame time or nothing drawn: " + ag_summary_line(summary)
    return True, ag_summary_line(summary)


def pc_ag_home(frame):
    """Home has shelves with something on them. Raises Unjudged when the
    snapshot does not say."""
    if frame is None:
        raise Unjudged("no Afterglow snapshot")
    if ag_screen(frame) != "home":
        return False, f"the screen is \"{ag_screen(frame)}\", not home"
    if not isinstance(frame.get("state"), dict) or not isinstance(frame["state"].get("lists"), dict):
        raise Unjudged("the snapshot has no state.lists")
    shelves = [(shelf, titles) for shelf, titles in ag_lists(frame) if titles]
    load = ag_state(frame, "load", "?")
    if not shelves:
        return False, f"no shelf is listed (load={load}, focus {ag_focus(frame).get('id') or '-'})"
    return True, f"load={load}, profile={ag_state(frame, 'profile', '?')}, " + \
        ", ".join(f"{shelf} ({len(titles)})" for shelf, titles in shelves)


def pc_ag_focus_ids(frame):
    """The focus id is "<shelf>/<index>" of a listed shelf and names the card
    whose title the focus carries; every focusable id has that form."""
    place, lists = ag_place(frame), dict(ag_lists(frame))
    focus = ag_focus(frame)
    if not focus or "id" not in focus:
        raise Unjudged("the snapshot has no focus id")
    if place is None or place[0] not in lists:
        return False, f"focus id \"{focus.get('id')}\" is not <shelf>/<index> of {sorted(lists)}"
    shelf, index = place
    if index >= len(lists[shelf]):
        return False, f"focus id {shelf}/{index} is past the shelf's {len(lists[shelf])} cards"
    if str(focus.get("text", "")) != lists[shelf][index]:
        return False, f"focus {shelf}/{index} says \"{focus.get('text')}\", the shelf lists \"{lists[shelf][index]}\""
    listed = frame.get("focusable") if isinstance(frame.get("focusable"), list) else []
    ids = [str(item.get("id", "")) for item in listed if isinstance(item, dict)]
    odd = [name for name in ids if not re.fullmatch(r".+/\d+", name) or name.rsplit("/", 1)[0] not in lists]
    if odd:
        return False, f"focusable ids outside <shelf>/<index>: {odd[:4]}"
    return True, f"focus {shelf}/{index} \"{str(focus.get('text'))[:50]}\", {len(ids)} focusable ids of that form"


def pc_ag_texts(frame, log_text):
    """Every card has a title, the focus and the hero have text, and the log
    does not say the text engine stayed down. That the glyphs are drawn is
    for the capture to show."""
    failed = ag_text_engine_failed(log_text)
    if failed is not None:
        return False, f"the text engine did not start ({failed}): the kit's baked fonts draw"
    lists = ag_lists(frame)
    if not lists:
        raise Unjudged("the snapshot lists no shelf to read texts from")
    titles = [title for _, shelf in lists for title in shelf]
    empty = sum(1 for title in titles if not title.strip())
    if empty or not str(ag_focus(frame).get("text", "")).strip():
        return False, f"{empty} of {len(titles)} cards have no title; focus text \"{ag_focus(frame).get('text', '')}\""
    foreign = sum(1 for title in titles if any(ord(char) > 0x24F for char in title))
    return True, (f"{len(titles)} titles, none empty ({foreign} with letters beyond Latin), "
                  f"hero \"{ag_state(frame, 'hero')[:50]}\"; no text-engine failure in the log")


def pc_ag_text_engine(frame):
    """Which fonts drew and how much: (ok, detail, numbers). The snapshot of
    today carries the texts whether or not a glyph was drawn, so until the
    frame reports these numbers the step cannot be judged."""
    stats = (frame or {}).get("stats")
    if not isinstance(stats, dict) or "text_engine" not in stats:
        raise Unjudged("the snapshot carries no numbers of the text engine (stats." + ", ".join(AG_TEXT_STATS)
                       + ": asked for in docs/afterglow/requests/drv.md); the capture is what shows the letters")
    numbers = {name: stats.get(name) for name in AG_TEXT_STATS}
    if not stats["text_engine"]:
        return False, "the kit's baked fonts draw, not the text engine", numbers
    if not stats.get("glyphs"):
        return False, "the text engine is up and has drawn no glyph", numbers
    return True, (f"{numbers['glyphs']} glyphs on {numbers['glyph_pages']} pages from {numbers['fonts']} font files, "
                  f"{numbers['missing_glyphs']} characters without a font"), numbers


def ag_output(frame, log_text):
    """The size the interface draws at: from the snapshot once it carries it
    (stats.output_w, stats.output_h), else from the mount's line in the log."""
    stats = (frame or {}).get("stats")
    stats = stats if isinstance(stats, dict) else {}
    width, height = stats.get("output_w"), stats.get("output_h")
    if isinstance(width, (int, float)) and isinstance(height, (int, float)) and width > 0 and height > 0:
        return {"width": int(width), "height": int(height), "source": "the snapshot"}
    up = ag_interface_up(log_text)
    return dict(up, source="the mount's line in the app's log") if up else None


def pc_classic_again(snapshot, before):
    """The classic browser is what draws again: the browser screen is reported
    without the interface's document. `before` is what it showed before the
    stage; a list without a cursor (an empty one) is still the classic browser."""
    if snapshot is None:
        return False, "no snapshot that can be read"
    if ag_shown(snapshot):
        return False, "the interface still draws: " + ui_line(snapshot)
    if ui_screen(snapshot) != SCREEN_BROWSER and not any(doc in BROWSER_DOCS for doc in ui_docs(snapshot)):
        return False, "not the browser: " + ui_line(snapshot)
    same = before is not None and ui_docs(snapshot) == ui_docs(before) and \
        ui_focus(snapshot).get("text") == ui_focus(before).get("text")
    return True, ("as before the stage: " if same else "the classic document draws: ") + ui_line(snapshot)


_AG_UP = re.compile(r"afterglow: interface up, look (\S+), (\d+)x(\d+)")
_AG_TEXT_DOWN = re.compile(r"afterglow: the text engine did not start \((.*?)\); ")
_DEV_CAPTURE = re.compile(r"bc DEV_CAPTURE (wrote|failed) (.*?)\s*$")


def ag_interface_up(log_text):
    """What the mount logged when it built the frame: look and output size."""
    found = None
    for found in _AG_UP.finditer(log_text):
        pass
    return {"look": found.group(1), "width": int(found.group(2)), "height": int(found.group(3))} if found else None


def ag_text_engine_failed(log_text):
    found = _AG_TEXT_DOWN.search(log_text)
    return found.group(1) if found else None


def ag_log_lines(log_text, limit=40):
    """Every line the interface wrote to the app's log, newest last."""
    return [line.strip()[-200:] for line in log_text.splitlines() if "afterglow: " in line][-limit:]


def capture_verdict(log_text):
    """What the build said about the development capture command in a piece of
    the log: ("wrote", detail), ("failed", reason), ("unknown", ...) when the
    build has no such command, or None while it has said nothing."""
    lines = log_text.splitlines()
    for line in lines:
        match = _DEV_CAPTURE.search(line)
        if match:
            return match.group(1), match.group(2)
    for index, line in enumerate(lines):
        if line.rstrip().endswith("bc REMOTE_CMD shot") and \
                any(later.rstrip().endswith("bc REMOTE_CMD unknown") for later in lines[index + 1:index + 3]):
            return "unknown", "this build has no capture command (bc REMOTE_CMD unknown)"
    return None


# --- the capture as pixels -------------------------------------------------------

def bmp_facts(data):
    """Size and a look at the pixels of an uncompressed 24- or 32-bit BMP:
    (facts, None) or (None, why it is not one)."""
    if not data or len(data) < 54 or data[:2] != b"BM":
        return None, "not a BMP"
    offset = int.from_bytes(data[10:14], "little")
    width = int.from_bytes(data[18:22], "little", signed=True)
    height = int.from_bytes(data[22:26], "little", signed=True)
    bits = int.from_bytes(data[28:30], "little")
    if int.from_bytes(data[30:34], "little") != 0 or bits not in (24, 32) or width <= 0 or height == 0:
        return None, f"unsupported BMP ({width}x{height}, {bits} bits)"
    step, rows = bits // 8, abs(height)
    stride = (width * step + 3) & ~3
    if len(data) < offset + stride * rows:
        return None, f"short BMP: {len(data)} bytes for {width}x{rows}"
    colours, lit, light, count = set(), 0, 0, 0
    for row in range(0, rows, max(1, rows // 48)):
        base = offset + row * stride
        for column in range(0, width, max(1, width // 64)):
            blue, green, red = data[base + column * step:base + column * step + 3]
            colours.add((red, green, blue))
            lit += (red + green + blue) > 24
            light += red + green + blue
            count += 1
    return {"width": width, "height": rows, "bits": bits, "bottom_up": height > 0, "offset": offset, "stride": stride,
            "samples": count, "colours": len(colours), "lit": round(lit / count, 3),
            "brightness": round(light / (count * 765), 3)}, None


def pc_capture(facts):
    """The capture is a picture of something: not one flat colour, not all dark."""
    if facts["colours"] < 8:
        return False, f"the capture is flat: {facts['colours']} colours among {facts['samples']} sampled points"
    if facts["lit"] < 0.02:
        return False, f"the capture is dark: {facts['lit']:.1%} of the sampled points are lit"
    return True, (f"{facts['width']}x{facts['height']}, {facts['colours']} colours among {facts['samples']} sampled "
                  f"points, {facts['lit']:.0%} lit")


def bmp_to_png(data):
    """The same picture as a PNG (8-bit RGB), with the standard library only."""
    import struct
    import zlib
    facts, problem = bmp_facts(data)
    if facts is None:
        raise ValueError(problem)
    width, rows, step, stride = facts["width"], facts["height"], facts["bits"] // 8, facts["stride"]
    lines = bytearray()
    order = range(rows - 1, -1, -1) if facts["bottom_up"] else range(rows)
    for row in order:
        base = facts["offset"] + row * stride
        pixels = data[base:base + width * step]
        line = bytearray(width * 3)
        line[0::3], line[1::3], line[2::3] = pixels[2::step], pixels[1::step], pixels[0::step]
        lines += b"\x00" + line

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, rows, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(lines), 6)) + chunk(b"IEND", b""))


# --- shared lock ---------------------------------------------------------------

class LockBusy(Exception):
    pass


def lock_acquire(path, name):
    """Create-new or nothing: the file's existence is the lock."""
    try:
        descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o644)
    except FileExistsError:
        try:
            holder = Path(path).read_text(errors="replace").strip()[:100]
        except OSError:
            holder = "unreadable"
        raise LockBusy(holder or "empty lock file") from None
    with os.fdopen(descriptor, "w") as handle:
        handle.write(name)


def lock_holder(path):
    try:
        return Path(path).read_text(errors="replace").strip()
    except FileNotFoundError:
        return None


def lock_release(path, name):
    """Delete only a lock that still says exactly our name. True when released."""
    try:
        content = Path(path).read_text(errors="replace")
    except FileNotFoundError:
        return False
    if content.strip() != name:
        return False
    os.unlink(path)
    return True


# --- frozen candidate ------------------------------------------------------------

class CandidateError(Exception):
    pass


def verify_candidate(directory, title=TITLE):
    """Check <directory>/<title>/ against <directory>/SHA256SUMS (either the
    protocol's layout, paths inside the title folder, or paths that start with
    the title folder). Every file of the folder must be listed and match."""
    directory = Path(directory)
    folder = directory / title
    sums = directory / "SHA256SUMS"
    for needed in (folder / "eboot.bin", folder / "sce_sys/param.json", sums):
        if not needed.is_file():
            raise CandidateError(f"missing {needed}")
    listed = {}
    for line in sums.read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64}) [ *](.+)", line.strip())
        if not match:
            continue
        name = match.group(2)
        name = name[2:] if name.startswith("./") else name
        if name.startswith(f"{title}/"):
            name = name[len(title) + 1:]
        elif (directory / name).is_file() and not (folder / name).is_file():
            continue  # an archive beside the folder: not part of what is installed
        listed[name] = match.group(1)
    files = sorted(item for item in folder.rglob("*") if item.is_file())
    for item in files:
        relative = item.relative_to(folder).as_posix()
        if relative not in listed:
            raise CandidateError(f"{relative} is not in SHA256SUMS")
        if hashlib.sha256(item.read_bytes()).hexdigest() != listed[relative]:
            raise CandidateError(f"{relative} differs from SHA256SUMS")
    missing = sorted(set(listed) - {item.relative_to(folder).as_posix() for item in files})
    if missing:
        raise CandidateError(f"listed but absent: {missing[0]}")
    eboot = (folder / "eboot.bin").read_bytes()
    builds = sorted({match.decode() for match in BUILD_ID.findall(eboot)})
    return {"folder": str(folder), "files": len(files),
            "eboot": hashlib.sha256(eboot).hexdigest(),
            "build": builds[0] if len(builds) == 1 else None,
            "instrumented": b"dev_command" in eboot and b"dev_status" in eboot,
            "afterglow": AG_MOUNT_MARK in eboot, "capture_command": AG_CAPTURE_MARK in eboot}
