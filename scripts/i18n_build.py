#!/usr/bin/env python3
"""Turn native/i18n/<language>.json into the compiled tables of native/tube_i18n_data.h.

Every catalog is checked against native/i18n/template.json and against the
fonts the package ships: slots kept, no control characters, a sane length,
every letter present in a browser font. Missing and stale texts are counted,
not fatal: a missing text is shown in English. --check fails when the
committed header is not what the catalogs produce.
"""
from pathlib import Path
import argparse
import json
import re
import struct
import sys
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
CATALOGS = ROOT / "native/i18n"
HEADER = ROOT / "native/tube_i18n_data.h"
FONTS = ROOT / "third_party/evo/projects/evoplayer/assets"
BROWSER_FONTS = ("LatoLatin-Regular", "LatoLatin-Bold", "Roboto-Regular", "NotoSans-Regular", "NotoSans-Bold", "DejaVuSans", "DejaVuSans-Bold")
KIT_FONTS = ("inter-regular", "inter-semibold", "montserrat-medium", "dejavu-sans-mono")
FORMS = ("one", "few", "many", "other")
# Plural rule of tube_i18n.c and the forms a counted text should give.
PLURALS = {
    "ru": (2, ("one", "few", "many", "other")), "uk": (2, ("one", "few", "many", "other")),
    "pl": (3, ("one", "few", "many", "other")), "cs": (4, ("one", "few", "other")), "ro": (5, ("one", "few", "other")),
    "fr": (1, ("one", "other")), "pt-BR": (1, ("one", "other")),
    "id": (6, ("other",)), "vi": (6, ("other",)), "tr": (6, ("other",)), "hu": (6, ("other",)),
}
DEFAULT_PLURAL = (0, ("one", "other"))
SLOT = re.compile(r"\{\d\}")


def truetype_codepoints(path):
    """The characters a TrueType or OpenType file maps (cmap formats 4 and 12)."""
    data = path.read_bytes()
    tables = {}
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        tables[tag] = offset
    cmap = tables[b"cmap"]
    found = set()
    for i in range(struct.unpack_from(">H", data, cmap + 2)[0]):
        offset = cmap + struct.unpack_from(">I", data, cmap + 8 + 8 * i)[0]
        kind = struct.unpack_from(">H", data, offset)[0]
        if kind == 4:
            count = struct.unpack_from(">H", data, offset + 6)[0] // 2
            ends = struct.unpack_from(f">{count}H", data, offset + 14)
            starts = struct.unpack_from(f">{count}H", data, offset + 16 + 2 * count)
            deltas = struct.unpack_from(f">{count}h", data, offset + 16 + 4 * count)
            ranges = offset + 16 + 6 * count
            offsets = struct.unpack_from(f">{count}H", data, ranges)
            for segment in range(count):
                for code in range(starts[segment], ends[segment] + 1):
                    if code == 0xFFFF:
                        continue
                    if offsets[segment]:
                        glyph = struct.unpack_from(">H", data, ranges + 2 * segment + offsets[segment] + 2 * (code - starts[segment]))[0]
                    else:
                        glyph = (code + deltas[segment]) & 0xFFFF
                    if glyph:
                        found.add(code)
        elif kind == 12:
            for group in range(struct.unpack_from(">I", data, offset + 12)[0]):
                first, last, _ = struct.unpack_from(">III", data, offset + 16 + 12 * group)
                found.update(range(first, last + 1))
    return found


def kit_codepoints(path):
    """The characters baked into one .huifont atlas of the player interface."""
    data = path.read_bytes()
    count = struct.unpack_from("<I", data, 32)[0]
    return {struct.unpack_from("<I", data, 40 + 24 * i)[0] for i in range(count)}


def font_coverage():
    browser, kit = None, None
    try:
        browser = set().union(*(truetype_codepoints(FONTS / "fonts" / (name + ".ttf")) for name in BROWSER_FONTS))
        kit = set.intersection(*(kit_codepoints(FONTS / "hui/fonts" / (name + ".huifont")) for name in KIT_FONTS))
    except (OSError, KeyError, struct.error) as problem:
        print(f"warning: packaged fonts not readable ({problem}); letters are not checked", file=sys.stderr)
    return browser, kit


def c_string(text):
    out = []
    for byte in text.encode("utf-8"):
        if byte in (0x22, 0x5C):
            out.append("\\" + chr(byte))
        elif 0x20 <= byte < 0x7F:
            out.append(chr(byte))
        else:
            out.append("\\%03o" % byte)
    return '"' + "".join(out) + '"'


def check_text(tag, source, text, entry, problems, notes):
    if not isinstance(text, str) or not text:
        problems.append(f"{tag}: empty translation of {source!r}")
        return False
    if any(unicodedata.category(c) in ("Cc", "Cf", "Zl", "Zp", "Co", "Cn") and c != "​" for c in text):
        problems.append(f"{tag}: control or format character in the translation of {source!r}")
    if unicodedata.normalize("NFC", text) != text:
        problems.append(f"{tag}: translation of {source!r} is not in composed (NFC) form")
    if sorted(SLOT.findall(text)) != sorted(SLOT.findall(entry["source"] if entry.get("pattern") else "")):
        problems.append(f"{tag}: slots of {source!r} do not match: {text!r}")
    if text != text.strip():
        problems.append(f"{tag}: translation of {source!r} starts or ends with a space")
    if len(text.encode("utf-8")) > 480 or len(text) > 3 * len(source) + 24:
        problems.append(f"{tag}: translation of {source!r} is far longer than its source")
    elif len(text) > 1.6 * len(source) + 10:
        notes["long"] += 1
    if "toast" in entry["context"] and len(text.encode("utf-8")) > 255:
        problems.append(f"{tag}: toast text {source!r} exceeds 255 bytes")
    return True


def load():
    template = json.loads((CATALOGS / "template.json").read_text(encoding="utf-8"))["strings"]
    by_source = {entry["source"]: entry for entry in template}
    browser, kit = font_coverage()
    problems, report, catalogs = [], [], []
    for file in sorted(CATALOGS.glob("*.json")):
        if file.name in ("template.json", "patterns.json"):
            continue
        catalog = json.loads(file.read_text(encoding="utf-8"))
        tag = catalog.get("language", "")
        if tag != file.stem or not re.fullmatch(r"[a-z]{2,3}(-[A-Za-z0-9]{2,8})?", tag) or not catalog.get("name") or not catalog.get("english"):
            problems.append(f"{file.name}: needs \"language\" equal to the file name, \"name\" and \"english\"")
            continue
        rule, wanted = PLURALS.get(tag, DEFAULT_PLURAL)
        notes = {"long": 0, "forms": 0}
        strings = catalog.get("strings", {})
        stale = sorted(source for source in strings if source not in by_source)
        letters, used = set(catalog["name"]), {}
        for source, value in strings.items():
            entry = by_source.get(source)
            if entry is None:
                continue
            if isinstance(value, dict):
                if not entry.get("one"):
                    problems.append(f"{tag}: {source!r} is not a counted text, give one translation")
                    continue
                if "other" not in value or any(form not in FORMS for form in value):
                    problems.append(f"{tag}: counted text {source!r} needs \"other\" and only one/few/many/other")
                    continue
                if any(form not in value for form in wanted):
                    notes["forms"] += 1
                forms = {form: text for form, text in value.items() if check_text(tag, source, text, entry, problems, notes)}
            elif check_text(tag, source, value, entry, problems, notes):
                forms = {"other": value}
                if entry.get("one") and len(wanted) > 1:
                    notes["forms"] += 1
            else:
                continue
            used[source] = forms
            for text in forms.values():
                letters.update(text)
        missing = [entry["source"] for entry in template if entry["source"] not in used]
        # Texts that only the Afterglow interface shows are counted apart: it is filled in its own pass.
        only_afterglow = {entry["source"] for entry in template if entry["context"] == ["afterglow"]}
        missing_afterglow = [source for source in missing if source in only_afterglow]
        boxes = sorted(c for c in letters if browser is not None and ord(c) not in browser and not c.isspace())
        if boxes:
            problems.append(f"{tag}: no browser font holds {' '.join('U+%04X %s' % (ord(c), c) for c in boxes[:20])}")
        # The player interface is drawn in a language only when its catalog asks for it ("player_interface": true)
        # and the baked fonts hold every letter of every text: a half-translated player is worse than an English one.
        kit_letters = [c for c in letters if kit is not None and ord(c) not in kit and not c.isspace()]
        kit_ready = kit is not None and catalog.get("player_interface") is True and len(missing) == len(missing_afterglow) and not kit_letters
        if catalog.get("player_interface") is True and not kit_ready:
            problems.append(f"{tag}: asks for the player interface, but texts are missing or its fonts lack {len(kit_letters)} letters")
        coverage = 100.0 * (len(template) - len(missing)) / len(template)
        others = len(template) - len(only_afterglow)
        report.append({"tag": tag, "name": catalog["name"], "texts": len(used), "missing": len(missing), "stale": len(stale),
                       "coverage": coverage, "long": notes["long"], "forms": notes["forms"], "kit": kit_ready,
                       "kit_letters": len(kit_letters), "afterglow": len(only_afterglow), "missing_afterglow": len(missing_afterglow),
                       "coverage_classic": 100.0 * (others - (len(missing) - len(missing_afterglow))) / max(others, 1)})
        catalogs.append({"tag": tag, "name": catalog["name"], "english": catalog["english"], "plural": rule, "kit": kit_ready, "used": used})
    return template, catalogs, report, problems


def render(template, catalogs):
    plain = sorted((entry["source"] for entry in template if not entry.get("pattern")), key=lambda text: text.encode("utf-8"))
    keys = [entry for entry in template if entry.get("pattern")]
    variants = []
    for key, entry in enumerate(keys):
        need = sum(1 << slot for slot in entry.get("need", []))
        for text in [entry["source"]] + ([entry["one"]] if entry.get("one") else []):
            variants.append((text, entry["args"], key, 1 if entry.get("one") else 0, need))
    # The most specific shape first: the generic "<label>: <value>" is tried last.
    variants.sort(key=lambda item: (-len(SLOT.sub("", item[0])), item[0].encode("utf-8")))
    out = ["/* Generated by scripts/i18n_build.py from the catalogs in native/i18n. Do not edit. */",
           f"#define TUBE_I18N_SOURCES {len(plain)}", f"#define TUBE_I18N_PATTERNS {len(variants)}",
           f"#define TUBE_I18N_PATTERN_KEYS {len(keys)}", f"#define TUBE_I18N_LANGUAGES {len(catalogs)}",
           "static const char *const tube_i18n_sources[TUBE_I18N_SOURCES] = {"]
    out += ["    " + c_string(text) + "," for text in plain]
    out += ["};", "static const tube_i18n_pattern_t tube_i18n_patterns[TUBE_I18N_PATTERNS] = {"]
    out += [f"    {{{c_string(text)}, {c_string(kinds)}, {key}, {counted}, {need}}}," for text, kinds, key, counted, need in variants]
    out.append("};")
    for catalog in catalogs:
        name = re.sub(r"[^a-z0-9]", "_", catalog["tag"].lower())
        used = catalog["used"]
        out.append(f"static const char *const tube_i18n_{name}_texts[TUBE_I18N_SOURCES] = {{")
        out += ["    " + (c_string(used[text]["other"]) if text in used else "0") + "," for text in plain]
        out += ["};", f"static const char *const tube_i18n_{name}_patterns[TUBE_I18N_PATTERN_KEYS][FORMS] = {{"]
        for entry in keys:
            forms = used.get(entry["source"], {})
            out.append("    {" + ", ".join(c_string(forms[form]) if form in forms else "0" for form in FORMS) + "},")
        out.append("};")
    out.append("static const tube_i18n_catalog_t tube_i18n_catalogs[TUBE_I18N_LANGUAGES + 1] = {")
    for catalog in catalogs:
        name = re.sub(r"[^a-z0-9]", "_", catalog["tag"].lower())
        out.append(f"    {{{c_string(catalog['tag'])}, {c_string(catalog['name'])}, {c_string(catalog['english'])}, {catalog['plural']}, "
                   f"{1 if catalog['kit'] else 0}, tube_i18n_{name}_texts, tube_i18n_{name}_patterns}},")
    out += ["    {0, 0, 0, 0, 0, 0, 0}", "};", ""]
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail when the committed header is stale")
    parser.add_argument("--report", action="store_true", help="print the coverage table as JSON")
    options = parser.parse_args()
    template, catalogs, report, problems = load()
    for line in problems[:60]:
        print("error: " + line, file=sys.stderr)
    if problems:
        print(f"{len(problems)} problems", file=sys.stderr)
        return 1
    if options.report:
        print(json.dumps(report, ensure_ascii=False, indent=1))
    else:
        for row in report:
            print(f"{row['tag']:6} {row['coverage']:5.1f}% of {len(template)} texts; missing {row['missing']} ({row['missing_afterglow']} of Afterglow's "
                  f"{row['afterglow']}), stale {row['stale']}, "
                  f"long {row['long']}, plural forms to add {row['forms']}; player interface fonts: "
                  + ("ready" if row["kit"] else f"{row['kit_letters']} letters short"))
    text = render(template, catalogs)
    if options.check:
        if not HEADER.exists() or HEADER.read_text(encoding="utf-8") != text:
            print("native/tube_i18n_data.h is stale: run python3 scripts/i18n_build.py", file=sys.stderr)
            return 1
        return 0
    HEADER.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
