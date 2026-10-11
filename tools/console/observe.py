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

SCREEN_PLAYER = 2
SCREEN_RESUME = 3
SCREEN_OPTIONS = 31
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


def ui_doc(snapshot):
    """The document that owns the screen: hui_provider, hui_keyboard, hui_osd..."""
    docs = (snapshot or {}).get("ui", {}).get("docs") or []
    return docs[0] if docs else ""


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
    return (f"seq={(snapshot or {}).get('seq')} screen={ui_screen(snapshot)} doc={ui_doc(snapshot) or '-'} "
            f"focus={focus.get('id', '-')} \"{str(focus.get('text', ''))[:60]}\" "
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
    """The provider screen is drawn with a cursor: rows, or the pairing card."""
    if ui_doc(snapshot) != "hui_provider":
        return False, f"doc {ui_doc(snapshot) or 'none'}"
    focus = ui_focus(snapshot)
    if not focus.get("id"):
        return False, "provider screen without a focused element"
    return True, f"{focus.get('id')} of {focus.get('total', 0)}: {ui_rows(snapshot)[:8]}"


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


def pc_rows(snapshot, pattern, doc="hui_provider"):
    if ui_doc(snapshot) != doc:
        return False, f"doc {ui_doc(snapshot) or 'none'}"
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
    """The UI says paused and the position held still across two status writes."""
    if not ui_paused(snapshot):
        return False, "UI does not report paused"
    live = [s for s in samples if s]
    if len(live) < 2 or live[-1]["t"] <= live[-2]["t"]:
        return False, "need two status writes"
    if abs(live[-1]["pos"] - live[-2]["pos"]) > tolerance:
        return False, f"pos still moving {live[-2]['pos']:.2f} -> {live[-1]['pos']:.2f}"
    return True, f"held at {live[-1]['pos']:.2f} for {live[-1]['t'] - live[-2]['t']} s"


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


def pc_paired(data, base):
    """The saved provider state names this companion and holds a token."""
    state = parse_state(data)
    if state is None:
        return False, "no readable youtube.json"
    source = str(state.get("source", ""))
    if base.rstrip("/") not in source:
        return False, "saved companion address is not this one"
    if len(str(state.get("token", ""))) < 24:
        return False, "address saved, no token yet"
    return True, "address saved and a token stored"


def history_record(data, video):
    for record in (parse_state(data) or {}).get("records") or []:
        if isinstance(record, dict) and record.get("id") == video:
            return record
    return None


def pc_history(data, video, need_played):
    record = history_record(data, video)
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
