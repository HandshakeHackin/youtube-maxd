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
            "instrumented": b"dev_command" in eboot and b"dev_status" in eboot}
