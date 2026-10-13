#!/usr/bin/env python3
"""Collect the interface texts ProsperoTube can translate into native/i18n/template.json.

The provider, its headers, the overlays and the browser document are scanned
for the English texts that reach one of the localization hooks (see
docs/LOCALIZATION.md). The output is deterministic; --check fails when the
committed template no longer matches the sources.
"""
from pathlib import Path
import argparse
import ast
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / "native/i18n/template.json"
MANUAL = ROOT / "native/i18n/patterns.json"
sys.path.insert(0, str(ROOT / "scripts"))
from i18n_overlay import rml_texts  # noqa: E402  (the same rule marks and extracts browser texts)

# Calls whose literal arguments never reach the screen.
SILENT = {
    "strcmp", "strncmp", "strcasecmp", "strncasecmp", "strstr", "strchr", "strrchr", "strpbrk", "strspn", "strcspn",
    "memcmp", "memcpy", "sscanf", "str", "num", "boolean", "fopen", "freopen", "fprintf", "printf", "puts", "fputs", "perror",
    "getenv", "remove", "rename", "evo_data_path", "evo_bt", "evo_boot_log", "evo_log", "evo_log_error", "evo_log_info",
    "assert", "_Static_assert", "static_assert", "defined", "job_string", "job_number", "job_bool", "job_begin", "request",
    "evo_net_request_async", "evo_net_get_async", "evo_feedback", "FIELD", "valid_prefix", "prefix", "starts", "ends",
    "find", "rfind", "compare", "getElementById", "GetElementById", "SetClass", "SetAttribute", "GetAttribute", "SetProperty",
    "DirtyVariable", "Bind", "BindFunc", "BindEventCallback", "RegisterMember", "RegisterTransformFunc", "IsClassSet",
    "SetInnerRML", "QuerySelector", "strtok", "open", "access", "stat", "mkdir", "unlink", "dlsym", "tube_log", "trace",
    "channel_name_index", "count_label", "tube_extra_group_find", "evo_provider_set_enabled", "setPendingProvider",
    "has_key", "is_id", "id_is", "parent_is", "json", "field", "member", "LoadDocument", "load_font", "LoadFontFace",
}
SILENT_PREFIXES = ("cJSON_", "evo_net_", "evo_log", "std::str", "sce", "pthread_", "av_", "avformat_", "tube_js_", "mbedtls_")
# Calls whose literal arguments are shown: name -> (context, argument indexes that are identities or addresses).
SHOWN = {
    "CATALOG_FOLDER": ("browser", {0}), "LINK_ROW": ("browser", {0, 3}), "folder": ("browser", {0, 1}),
    "tube_set_error": ("status", set()), "toast": ("toast", set()), "prompt_arm": ("browser", set()),
    "evo_rmlui_provider_set_status": ("status", set()), "evo_rmlui_provider_set_loading": ("status", set()),
    "SetStatus": ("status", set()), "SetLoading": ("status", set()), "ui::text": ("kit", set()),
    "evo_keyboard_open": ("kit", {1, 2, 3, 4}),
}
FORMATTERS = {"snprintf": 2, "std::snprintf": 2, "sprintf": 1}
# A formatted destination with one of these words in its name holds a text for the screen.
TEXT_NAME = re.compile(r"label|detail|hint|title|text|line|note|message|summary|status|caption|overview|notice|reason|when|value|name|buffer|buf\b")
ID_NAME = re.compile(r"\b(id|url|path|key|target|expected|query|parent|address|catalog|file|header|headers|rerun|link|body|cursor|code|token|json|request)\b|_id\b|_url\b|_path\b")
NAMES = {"ProsperoTube", "YouTube", "SponsorBlock", "DeArrow", "Return YouTube Dislike", "PT", "AV1", "AVC", "HEVC", "VP9", "HDR", "HD", "4K", "3D", "VR180", "EVO",
         "PROSPEROTUBE", "YOUTUBE", "SPONSORBLOCK"}
SPEC = re.compile(r"%(?:%|[-+ #0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z|j)?[diufgGeExXsc])")


def strip_comments(text):
    def keep(match):
        token = match.group(0)
        return token if token[0] in "\"'" else re.sub(r"[^\n]", " ", token)
    return re.sub(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|//[^\n]*|/\*.*?\*/', keep, text, flags=re.S)


def unescape(body):
    simple = {"n": "\n", "t": "\t", "\\": "\\", '"': '"', "'": "'", "r": "\r", "0": "\0", "a": "\a", "?": "?"}
    out, i, raw = [], 0, bytearray()

    def flush():
        if raw:
            out.append(raw.decode("utf-8", "replace"))
            raw.clear()
    while i < len(body):
        c = body[i]
        if c != "\\" or i + 1 >= len(body):
            flush(); out.append(c); i += 1
            continue
        n = body[i + 1]
        if n == "x":
            j = i + 2
            while j < len(body) and body[j] in "0123456789abcdefABCDEF":
                j += 1
            raw.append(int(body[i + 2:j] or "0", 16) & 255); i = j
        elif n in "01234567":
            j = i + 1
            while j < len(body) and j < i + 4 and body[j] in "01234567":
                j += 1
            raw.append(int(body[i + 1:j], 8) & 255); i = j
        else:
            flush(); out.append(simple.get(n, n)); i += 2
    flush()
    return "".join(out)


class Literal:
    __slots__ = ("text", "chain", "initializer", "before", "call_text", "arguments")


def scan_c(source):
    """Every string literal of a C or C++ text with the calls enclosing it."""
    source = strip_comments(source)
    macros = {name: unescape(body) for name, body in re.findall(r'^[ \t]*#[ \t]*define[ \t]+([A-Z][A-Z0-9_]*)[ \t]+"((?:\\.|[^"\\\n])*)"[ \t]*$', source, flags=re.M)}

    def directive(match):
        line = match.group(0)
        if re.match(r"[ \t]*#[ \t]*define[ \t]+\w+\(", line):       # a function-like macro: its body is code
            head = line.index(")") + 1
            return " " * head + line[head:].replace("\\\n", " \n")
        return re.sub(r"[^\n]", " ", line)
    source = re.sub(r"^[ \t]*#[^\n]*(?:\\\n[^\n]*)*", directive, source, flags=re.M)
    token = re.compile(r'"((?:\\.|[^"\\\n])*)"|\'(?:\\.|[^\'\\\n])*\'|[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*|[(){},;=?:\[\]]')
    found, stack, previous, last_identifier = [], [], "", ""
    statement = 0
    pending = None
    for match in token.finditer(source):
        piece = match.group(0)
        literal = None
        if piece[0] == '"':
            literal = unescape(match.group(1))
        elif piece in macros:
            literal = macros[piece]
        if literal is not None:
            if pending is not None and not source[pending[1]:match.start()].strip():
                pending[0].text += literal
                pending[1] = match.end()
                previous = '"'
                continue
            item = Literal()
            item.text = literal
            item.chain = [(frame["name"], frame["argument"]) for frame in stack if frame["kind"] == "call"]
            item.initializer = any(frame["kind"] == "initializer" for frame in stack)
            item.before = source[statement:match.start()]
            item.call_text = None
            item.arguments = None
            for frame in stack:
                if frame["kind"] == "call":
                    frame["literals"].append(item)
            found.append(item)
            pending = [item, match.end()]
            previous = '"'
            continue
        pending = None
        if piece == "(":
            stack.append({"kind": "call", "name": last_identifier if previous == "identifier" else "", "argument": 0, "start": match.end(), "literals": [], "commas": []})
        elif piece == ")":
            while stack and stack[-1]["kind"] != "call":
                stack.pop()
            if stack:
                frame = stack.pop()
                if frame["name"] in FORMATTERS:
                    edges = [frame["start"]] + frame["commas"] + [match.start() + 1]
                    arguments = [source[edges[i]:edges[i + 1] - 1] for i in range(len(edges) - 1)]
                    for item in frame["literals"]:
                        if item.arguments is None:
                            item.arguments = arguments
        elif piece == "{":
            stack.append({"kind": "initializer" if previous in ("=", ",", "{") or (stack and stack[-1]["kind"] == "initializer") else "block"})
            if stack[-1]["kind"] == "block":
                statement = match.end()
        elif piece == "}":
            while stack and stack[-1]["kind"] == "call":
                stack.pop()
            if stack:
                stack.pop()
            statement = match.end()
        elif piece == ",":
            if stack and stack[-1]["kind"] == "call":
                stack[-1]["argument"] += 1
                stack[-1]["commas"].append(match.end())
        elif piece == ";":
            if not any(frame["kind"] == "call" for frame in stack):
                statement = match.end()
        if piece[0].isalpha() or piece[0] == "_":
            last_identifier, previous = piece, "identifier"
        else:
            previous = piece
    return found


def letters(text):
    return re.findall(r"[A-Za-z]{2,}", text)


def looks_technical(text):
    text = text.replace(" & ", " and ")       # "Accounts & library" is a title, "a=1&b=2" is not
    if re.search(r"://|\\|\|\||[{}<>=&@#$^~`]|\b[a-z]+_[a-z_]+\b|^[a-z0-9_.:/+-]+$|^/|\.(json|txt|c|h|png|rml|log|srt|vtt|mp4|tmp|bak)\b|^\s*$", text):
        return True
    if re.search(r"[a-z0-9]/[a-z0-9]", text) or re.search(r"[A-Za-z]:[A-Za-z%]", text):
        return True
    return not letters(text)


def displayable(text, phrases=True):
    """A literal met outside a known call: only clear sentences and labels."""
    if looks_technical(text) or SPEC.search(text):
        return False
    words = letters(text)
    if " " in text.strip():
        return len(words) >= 2 and not re.fullmatch(r"[a-z0-9 ,;-]+", text) or (phrases and bool(re.fullmatch(r"[a-z]+( [a-z]+){1,4}", text)))
    return bool(re.fullmatch(r"[A-Z][a-z]{2,}[a-z0-9-]*|[A-Z]{4,}", text))


def tidy(text):
    text = text.strip()
    text = re.sub(r"^[;|,.] *", "", text)
    if text.endswith(":"):
        text = text[:-1].rstrip()
    return text


def pattern_of(fmt, arguments):
    """A printf format as a pattern: ("Up to {0}p", "n", singular or None), or None for a format without words."""
    plurals = []
    for argument in arguments:
        pair = re.search(r'\?\s*"([a-z]{0,3})"\s*:\s*"([a-z]{0,3})"\s*$', argument.strip())
        plurals.append((pair.group(1), pair.group(2)) if pair else None)
    lead = arguments[0].strip() if arguments else ""
    if fmt.startswith("%s") and len(fmt) > 2 and re.search(r'\?\s*"[^"]*"\s*:\s*""$|\?\s*""\s*:\s*"[^"]*"$', lead):
        fmt = fmt[2:]            # an optional lead such as "Selected: " is translated by the label rule
        plurals = plurals[1:]
    out, kinds, singular, index, position, counted = [], [], [], 0, 0, False
    for match in SPEC.finditer(fmt):
        out.append(fmt[position:match.start()]); singular.append(fmt[position:match.start()])
        position = match.end()
        spec = match.group(0)
        if spec == "%%":
            out.append("%"); singular.append("%")
            continue
        suffix = plurals[index] if index < len(plurals) else None
        index += 1
        if spec[-1] == "s" and suffix is not None and match.start() and fmt[match.start() - 1].isalpha():
            singular.append(suffix[0]); out.append(suffix[1]); counted = True
            continue
        slot = "{%d}" % len(kinds)
        out.append(slot); singular.append(slot)
        kinds.append("s" if spec[-1] in "sc" else "n")
    out.append(fmt[position:]); singular.append(fmt[position:])
    text, one = "".join(out).strip(), "".join(singular).strip()
    plain = re.sub(r"\{\d\}", "", text)
    if not kinds or len("".join(letters(plain))) < 3 or looks_technical(plain) or re.search(r"\}\{\d\}.", text):   # two slots in a row have no boundary to match

        return None
    label = re.fullmatch(r"([^:{}]{2,60}): \{0\}", text)
    if label and not counted:
        return label.group(1), "", None       # "<Label>: <value>" is translated in two parts
    return text, "".join(kinds), one if counted and one != text else None


class Collector:
    def __init__(self):
        self.entries = {}

    def add(self, source, context, kinds=None, one=None, need=None):
        source = source if kinds else tidy(source)
        halves = None if kinds else re.fullmatch(r"([^:]{2,60}): ([^:.]{1,40})", source)
        if halves and len(halves.group(2).split()) <= 4:
            self.add(halves.group(1), context)       # "<Label>: <value>": the two parts are translated on their own
            self.add(halves.group(2), context)
            return
        if not source or source in NAMES or len(source) > 300 or not (letters(source) or need):
            return
        entry = self.entries.setdefault(source, {"source": source, "context": set()})
        entry["context"].add(context)
        if kinds:
            entry["pattern"], entry["args"] = True, kinds
            if one:
                entry["one"] = one
            if need:
                entry["need"] = need

    def c_text(self, text, context, wording=()):
        def record(source, where, kinds=None, one=None):
            self.add(source, where, kinds, one)
            changed = source
            for old, new in wording:
                changed = changed.replace(old, new)
            if changed != source:
                self.add(changed, where, kinds, one.replace(old, new) if one else None)
        for item in scan_c(text):
            verdict, where = None, context
            for name, argument in reversed(item.chain):
                if name in FORMATTERS:
                    if item.arguments is None:
                        verdict = False
                        break
                    destination = item.arguments[0].strip() if item.arguments else ""
                    shown = bool(TEXT_NAME.search(destination)) and not ID_NAME.search(destination)
                    index = FORMATTERS[name]
                    if not shown:
                        verdict = False
                    elif argument == index:
                        made = pattern_of(item.text, item.arguments[index + 1:])
                        if made:
                            record(made[0], where, made[1] or None, made[2])
                        elif not SPEC.search(item.text) and not looks_technical(item.text):
                            record(item.text, where)
                        verdict = False
                    else:
                        fmt = item.arguments[index] if len(item.arguments) > index else ""
                        verdict = argument > index and bool(letters(SPEC.sub("", fmt)) or "%s" in fmt) and not looks_technical(item.text) and not SPEC.search(item.text)
                        verdict = verdict and not re.fullmatch(r"[a-z]{0,3}", item.text)
                    break
                if name in SHOWN:
                    where, skipped = SHOWN[name]
                    if context == "browser" and where == "kit":
                        where = "browser"
                    verdict = argument not in skipped and not looks_technical(item.text) and not SPEC.search(item.text)
                    break
                if name in SILENT or name.startswith(SILENT_PREFIXES):
                    verdict = False
                    break
            if verdict is None:
                if re.search(r"\b(?:case|strcmp|strncmp)\b[^;]*$", item.before[-80:]):
                    verdict = False
                else:
                    verdict = displayable(item.text, context == "browser")
            if verdict:
                record(item.text, where)


def wording_pairs(provider):
    block = re.search(r"static void embedded_wording\(.*?\{(.*?)\};", provider, flags=re.S)
    return [(unescape(a), unescape(b)) for a, b in re.findall(r'\{"((?:\\.|[^"\\])*)",\s*"((?:\\.|[^"\\])*)"\}', block.group(1))] if block else []


def collect():
    collector = Collector()
    provider = (ROOT / "native/tube_provider.c").read_text(encoding="utf-8")
    wording = wording_pairs(provider)
    skipped = {"tube_name_case_data.h", "tube_about_data.h", "tube_about.h", "tube_i18n.c", "tube_i18n.h", "tube_i18n_data.h", "tube_secondary_srt.inc"}
    for file in sorted((ROOT / "native").glob("tube_*.[ch]")):
        if file.name not in skipped:
            collector.c_text(file.read_text(encoding="utf-8"), "browser", wording if file.name in ("tube_provider.c", "tube_player_extra.c", "tube_backup.c") else ())
    for file in sorted((ROOT / "scripts").glob("*.py")):
        if not (file.name.endswith("_overlay.py") or file.name == "prepare_native.py") or file.name == "i18n_overlay.py":
            continue
        tree = ast.parse(file.read_text(encoding="utf-8"))
        replaced = set()      # the upstream text an overlay replaces is never shown
        for node in ast.walk(tree):
            if isinstance(node, ast.Call) and getattr(node.func, "id", "") == "replace" and len(node.args) >= 3:
                replaced.update(id(part) for part in ast.walk(node.args[1]))
                if re.search(r"\brml\b|\.rml", ast.unparse(node.args[0])) and "rcss" not in ast.unparse(node.args[0]):
                    for part in ast.walk(node.args[2]):      # a text put into the browser document
                        if isinstance(part, ast.Constant) and isinstance(part.value, str):
                            for text in rml_texts(">" + part.value + "<"):
                                if not re.search(r"\w\(", text):      # a call, not a sentence
                                    collector.add(text, "document")
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, str) and len(node.value) > 3 and id(node) not in replaced:
                if re.search(r"</[a-z]+>", node.value):
                    for text in rml_texts(node.value):
                        collector.add(text, "document")
                if '"' in node.value and re.search(r"[;(]", node.value):
                    collector.c_text(node.value, "kit")
    for text in rml_texts((ROOT / "native/youtube.rml").read_text(encoding="utf-8")):
        collector.add(text, "document")
    if MANUAL.exists():
        for entry in json.loads(MANUAL.read_text(encoding="utf-8")):
            if entry.get("args"):
                collector.add(entry["source"], entry.get("context", "browser"), entry["args"], entry.get("one"), entry.get("need"))
            else:
                collector.add(entry["source"], entry.get("context", "browser"))
    entries = []
    for source in sorted(collector.entries):
        entry = dict(collector.entries[source])
        entry["context"] = sorted(entry["context"])
        entries.append(entry)
    return entries


def render(entries):
    return json.dumps({"version": 1, "strings": entries}, ensure_ascii=False, indent=1) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail when the committed template is stale")
    parser.add_argument("--list", action="store_true", help="print the texts instead of writing the template")
    options = parser.parse_args()
    entries = collect()
    text = render(entries)
    if options.list:
        for entry in entries:
            print(("P " if entry.get("pattern") else "  ") + ",".join(entry["context"]) + "\t" + entry["source"] + ("\t| " + entry["one"] if entry.get("one") else ""))
        return 0
    patterns = sum(1 for entry in entries if entry.get("pattern"))
    if options.check:
        if not TEMPLATE.exists() or TEMPLATE.read_text(encoding="utf-8") != text:
            print("native/i18n/template.json is stale: run python3 scripts/i18n_extract.py", file=sys.stderr)
            return 1
        print(f"template current: {len(entries)} texts, {patterns} patterns")
        return 0
    TEMPLATE.parent.mkdir(parents=True, exist_ok=True)
    TEMPLATE.write_text(text, encoding="utf-8")
    print(f"wrote {TEMPLATE.relative_to(ROOT)}: {len(entries)} texts, {patterns} patterns")
    return 0


if __name__ == "__main__":
    sys.exit(main())
