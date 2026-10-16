#!/usr/bin/env python3
"""Writes the fixture catalogs of the settings screen.

Every row has the provider's own id; the facts are the ones docs/afterglow/requests/e.md asks for.
Names, versions and times are the invented ones of the proposal.

usage: python3 tests/afterglow/tools/fixtures_settings.py tests/afterglow/fixtures
"""
import copy
import json
import os
import re
import sys

ROOT = sys.argv[1]


def file_of(catalog, page=0):
    name = "root" if catalog == "" else re.sub(r"[^A-Za-z0-9_-]", "_", catalog)
    return "catalog/%s%s.json" % (name, ".p%d" % page if page else "")


def write(fixture, catalog, rows, more=False, page=0):
    path = os.path.join(ROOT, fixture, file_of(catalog, page))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write('{\n "items": [\n')
        out.write(",\n".join("  " + json.dumps(row, ensure_ascii=False) for row in rows))
        out.write('\n ],\n "has_more": %s\n}\n' % ("true" if more else "false"))


def write_error(fixture, catalog, message):
    path = os.path.join(ROOT, fixture, file_of(catalog))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps({"error": message}, ensure_ascii=False) + "\n")


def profile(fixture, id, name, guest, signed_in):
    path = os.path.join(ROOT, fixture, "profile.json")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path):
        return  # a set another package already described keeps its profile
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps({"id": id, "name": name, "guest": guest, "signed_in": signed_in}) + "\n")


def row(id, label, kind, detail="", value=None, **facts):
    title = "%s: %s" % (label, value) if value not in (None, "") and kind not in ("choice", "info", "page") else label
    out = {"id": id, "is_folder": True, "interface": True, "title": title, "subtitle": detail, "label": label, "kind": kind}
    if value is not None:
        out["value"] = value
    for name, fact in facts.items():
        out[name] = fact
    return out


def switch(id, label, on, detail="", **facts):
    return row(id, label, "switch", detail, "On" if on else "Off", on=on, **facts)


def chosen(id, label, kind, detail, values, index, **facts):
    return row(id, label, kind, detail, values[index], index=index, count=len(values), **facts)


def choices(prefix, values, index, detail="", ids=None, facts=None, **shared):
    out = []
    for i, value in enumerate(values):
        own = dict(shared)
        if facts:
            own.update(facts[i])
        out.append(row("%s:%s" % (prefix, ids[i] if ids else i), value, "choice", "Selected" if i == index else detail, selected=(i == index), **own))
    return out


PREVIEW_ART = "art://ridges/6/409/1280x720"

SIZES = ["%d %%" % p for p in range(10, 201, 10)]
STYLES = ["Player's own", "White, shadow", "Box", "White on black", "Yellow, shadow", "Yellow box", "Yellow on black"]
STYLE_FACTS = [
    {"text_rgb": "ffffff", "background_alpha": 140, "edge": "none"},
    {"text_rgb": "e4e4e4", "background_alpha": 0, "edge": "shadow"},
    {"text_rgb": "ffffff", "background_alpha": 178, "edge": "none"},
    {"text_rgb": "e4e4e4", "background_alpha": 255, "edge": "outline"},
    {"text_rgb": "ffff00", "background_alpha": 0, "edge": "shadow"},
    {"text_rgb": "ffff00", "background_alpha": 100, "edge": "outline"},
    {"text_rgb": "ffff00", "background_alpha": 255, "edge": "outline"},
]
POSITIONS = ["Bottom"] + ["%g %%" % (p / 10.0) for p in range(0, 1001, 25)]
POSITION_NUMBERS = [-1] + list(range(0, 1001, 25))
LANGUAGES = [("en", "English"), ("de", "Deutsch"), ("es", "Español"), ("fr", "Français"), ("it", "Italiano"), ("nl", "Nederlands"),
             ("pl", "Polski"), ("pt", "Português"), ("sv", "Svenska"), ("da", "Dansk"), ("fi", "Suomi"), ("nb", "Norsk bokmål"),
             ("cs", "Čeština"), ("hu", "Magyar"), ("ro", "Română"), ("tr", "Türkçe"), ("uk", "Українська"), ("el", "Ελληνικά"),
             ("id", "Bahasa Indonesia"), ("vi", "Tiếng Việt"), ("ru", "Русский"), ("ca", "Català"), ("hr", "Hrvatski"), ("sk", "Slovenčina")]
RATES = ["Automatic", "30 fps", "60 fps"]
THEMES = ["Afterglow", "Onyx", "Daybreak", "High contrast"]
THEME_FACTS = [{"look": "afterglow"}, {"look": "onyx"}, {"look": "daybreak"}, {"look": "contrast"}]
UI_SCALES = ["%gx" % (s / 100.0) for s in (40, 50, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140)]
UI_SCALE_NUMBERS = [40, 50, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 125, 130, 135, 140]
GRID_SCALES = ["%gx" % (s / 100.0) for s in (50, 60, 70, 80, 90, 100, 110, 120, 125, 130, 135, 140, 150)]
GRID_NUMBERS = [50, 60, 70, 80, 90, 100, 110, 120, 125, 130, 135, 140, 150]
COUNTRIES = ["United Kingdom", "Ireland", "Germany", "Austria", "Switzerland", "France", "Belgium", "Netherlands", "Luxembourg", "Spain",
             "Portugal", "Italy", "Greece", "Denmark", "Sweden", "Norway", "Finland", "Iceland", "Poland", "Czechia", "Slovakia", "Hungary",
             "Romania", "Bulgaria", "Croatia", "Slovenia", "Estonia", "Latvia", "Lithuania", "Ukraine", "Türkiye", "Canada", "United States",
             "Mexico", "Brazil", "Argentina", "Chile", "Australia", "New Zealand", "Japan", "South Korea", "India", "Indonesia", "Vietnam",
             "South Africa", "Egypt", "Morocco"]
COUNTRY_IDS = ["GB", "IE", "DE", "AT", "CH", "FR", "BE", "NL", "LU", "ES", "PT", "IT", "GR", "DK", "SE", "NO", "FI", "IS", "PL", "CZ", "SK",
               "HU", "RO", "BG", "HR", "SI", "EE", "LV", "LT", "UA", "TR", "CA", "US", "MX", "BR", "AR", "CL", "AU", "NZ", "JP", "KR", "IN",
               "ID", "VN", "ZA", "EG", "MA"]
MENU = [("like", "Like video"), ("dislike", "Dislike video"), ("unrate", "Remove video rating"), ("watch-later-add", "Add to Watch later"),
        ("subscribe", "Subscribe on YouTube"), ("not-interested", "Not interested"), ("dont-recommend", "Don't recommend channel"),
        ("hide", "Hide from this list"), ("playlist-add", "Add to a playlist"), ("play-next", "Play next"), ("related", "Related videos"),
        ("block", "Block or unblock channel"), ("description", "Open description"), ("open-channel", "Open channel"), ("share", "Share link"),
        ("refresh", "Refresh section")]


def main_list(o=None):
    """The catalog "settings" for Mara; `o` replaces facts of single rows."""
    o = o or {}
    rows = [
        chosen("settings:frame-rate", "Frame rate", "list", "Videos above this rate are played in the next one below", RATES, 2),
        row("settings:codec", "Video codec", "cycle", "Automatic takes what the PS5 decodes in hardware; AV1 is decoded in software", "VP9"),
        row("settings:video-rotate", "Rotation", "list", "Clockwise, in quarter turns", "0 degrees", index=0, count=4),
        switch("settings:video-mirror", "Mirror the picture", False, "Left and right swapped, before the rotation"),
        row("settings:video-aspect", "Aspect ratio", "list", "The video's own, or one you set; independent of zoom", "Original", index=0, count=11),
        row("settings:pixel-ratio", "Display pixel ratio", "list", "A correction for displays that are not 16:9; applies to this PS5", "1:1", index=0, count=6),
        row("settings:video-zoom", "Sizing and zoom", "list", "Fit, fill, stretch or a zoom from 50 % to 300 %", "Fit", index=0, count=61),
        row("settings:time-display", "Time shown", "list", "What stands at the end of the timeline", "Time remaining", index=1, count=3),
        row("settings:speed", "Speed", "slider", "For every video that is not live; live streams play at normal speed", "1x", index=7, count=30),
        row("settings:speed-mode", "Remember speed for", "list", "Where a speed chosen while watching is kept", "All videos", index=0, count=4),
        switch("settings:captions", "Show captions", True, "Captions open with the video when it offers your language.", preview="caption.on"),
        row("settings:languages", "Language", "list", "Used when a video has it. Otherwise the video's own language is offered.", "English", index=0, count=len(LANGUAGES)),
        row("settings:audio-languages", "Audio language", "list", "Preferred when a video offers more than one sound track", "Original audio", index=0, count=len(LANGUAGES) + 1),
        switch("settings:automatic", "Automatic captions", True, "Captions YouTube wrote by listening are offered when nobody wrote any by hand."),
        switch("settings:sponsor", "Skip segments", True, "Sponsor reads, intros and the other categories you choose below are skipped."),
        row("settings:dearrow", "DeArrow", "page", "Community titles and thumbnails", "Titles on · thumbnails off"),
        row("settings:player-chat", "Live chat beside the video", "cycle", "Off, on the left or on the right", "Off"),
        row("settings:sleep-timer", "Sleep timer", "slider", "Playback pauses after this long without a press", "Off", index=0, count=21),
        row("settings:player-ok", "Cross while the controls are hidden", "list", "What the first press does", "Show controls", index=0, count=4),
        row("settings:player-hide", "Hide the controls after", "slider", "Seconds without a press", "3 seconds", index=3, count=16),
        row("settings:seek-interval", "Seek step", "slider", "How far Left and Right jump", "10 seconds", index=5, count=10),
        row("settings:player-seek", "Seeking", "list", "Jump at once, or confirm the place first", "At once", index=0, count=3),
        row("settings:remote", "Phone pairing", "link", "Link the YouTube app with a TV code", "Mara's phone is linked", screen="link_phone"),
        row("settings:sb", "SponsorBlock", "page", "Categories, short segments, excluded channels", "Skipping 3 categories"),
        row("settings:playback-mode", "At the end of a video", "list", "What plays next, if anything", "Continuous playback", index=0, count=7),
        switch("settings:queue-respects-mode", "Queue follows the playback mode", True, "Off: queued videos continue even when playback would stop"),
        switch("settings:ask-stream", "Choose the stream before playback", False, "On shows the available qualities before every video"),
        row("settings:history", "Watch history", "list", "What this PS5 records and what the account is told", "Automatic", index=0, count=3),
        switch("settings:search-history", "Search history", True, "Recent searches are kept for this profile"),
        row("settings:search", "Search options", "page", "Upload date, duration, type, features and sorting", "No filters"),
        row("settings:sections", "Menu sections", "page", "Show, hide and order the main menu, Discover and pinned sections", "22 sections"),
        row("settings:channel-sort", "Order of subscribed channels", "list", "How the list of channels is sorted for this profile", "Account order", index=0, count=3),
        row("settings:boot-section", "Start in", "list", "The section this profile opens with", "Home", index=1, count=17),
        switch("settings:account-on-boot", "Choose account at startup", False, "Shows the profiles each time ProsperoTube starts; applies to this PS5"),
        row("settings:clear-history", "Clear local history", "action", "Removes resume positions and watched marks of this profile", danger=True,
            confirm="Resume positions and watched marks of this profile are removed from this PS5. YouTube's own history is not touched.",
            confirm_label="Clear", done="Local history cleared"),
        row("settings:clear-queue", "Clear play queue", "action", "Removes this profile's queued videos; favourites stay",
            confirm="The play queue of this profile is emptied.", confirm_label="Clear", done="Play queue cleared"),
        row("settings:extra:captions", "Caption style, size and position", "page", "How captions look in the player", "120 % · box"),
        row("settings:extra:audio", "Player volume and audio delay", "page", "Volume, auto volume and sound delay", "Auto volume on · 100 %"),
        row("settings:extra:player", "Player controls, keys and Shorts", "page", "Clock, quality info, exit, D-pad, seek preview and more"),
        row("settings:content-language", "Content language", "list", "The language YouTube uses for titles and sections", "English", index=1, count=len(LANGUAGES) + 1),
        row("settings:content-country", "Content country", "list", "The country of recommendations and Trending", "United Kingdom", index=1, count=len(COUNTRIES) + 1),
        row("settings:interface-language", "Interface language", "list", "The language of ProsperoTube's menus and messages", "As the PS5", index=0, count=22),
        row("settings:ui", "Interface", "page", "Theme, scale, cards and menus", "Afterglow · sounds on"),
        row("settings:hide", "Hide content", "page", "Shorts, mixes, watched, live and upcoming videos by section", "Watched videos leave Home"),
        row("settings:thumbnails", "Thumbnails", "list", "The published thumbnail, or a frame of the video", "Published", index=0, count=4),
        row("settings:experimental", "Experimental features", "page", "What the Experimental mark on a row means", tag="Experimental"),
        row("settings:accounts", "Account settings", "page", "Separate settings per account and the profile password", "Mara · 2 profiles"),
        row("settings:backup", "Backup and restore", "page", "Local backups, automatic backup and subscriptions import", "Last backup today, 09:12"),
        row("settings:about", "About ProsperoTube", "page", "Version, author and credits", "ProsperoTube 1.000.000",
            update="1.001.000 is ready to install", attention=True),
    ]
    for key, name in (("player", "Player"), ("captions", "Captions"), ("audio", "Audio"), ("content", "Content & language"),
                      ("interface", "Interface"), ("sponsorblock", "SponsorBlock"), ("dearrow", "DeArrow")):
        rows.append(row("settings:reset:" + key, "Defaults for " + name, "reset", "Every setting of this part returns to its default",
                        confirm="Every setting under %s returns to its default for Mara. Nothing else changes." % name))
    for item in rows:
        if item["id"] in o:
            item.update(o[item["id"]])
            if "value" in o[item["id"]]:
                item["title"] = "%s: %s" % (item["label"], item["value"])
    return rows


def captions_page(o=None):
    o = o or {}
    rows = [
        row("settings:subtitle-size", "Size", "slider", "The preview shows the size on your screen.", "120 %", index=11, count=20, number=120,
            preview="caption.size", preview_art_url=PREVIEW_ART),
        row("settings:subtitle-style", "Style", "list", "How the letters are kept apart from the picture.", "Box", index=2, count=7, preview="caption.style",
            **STYLE_FACTS[2]),
        row("settings:subtitle-position", "Position", "slider", "Where captions sit when the video does not place them itself.", "Bottom", index=0,
            count=len(POSITIONS), number=-1, preview="caption.position"),
        switch("settings:more-subtitles", "Offer translated captions", True, "Adds YouTube's automatic translations to the list; opening a video takes a little longer."),
        switch("settings:caption-memory", "Remember per channel", False, "Keeps the caption choice you make while watching, channel by channel."),
    ]
    for item in rows:
        if item["id"] in o:
            item.update(o[item["id"]])
    return rows


def size_choices(index):
    return choices("settings:subtitle-size", SIZES, index, "Caption text, against the player's own size",
                   facts=[{"number": p} for p in range(10, 201, 10)], preview="caption.size", preview_art_url=PREVIEW_ART)


def style_choices(index):
    return choices("settings:subtitle-style", STYLES, index, "Text colour, background and edge of captions", facts=STYLE_FACTS,
                   preview="caption.style", preview_art_url=PREVIEW_ART)


def scheme_choices(index):
    rows = choices("settings:ui:scheme", THEMES, index, "The colours of the whole interface", facts=THEME_FACTS, preview="theme")
    rows.append(row("settings:ui:scheme:classic", "Classic colours", "page", "The colour schemes of earlier versions; the classic interface wears them"))
    return rows


def interface_page(o=None):
    o = o or {}
    rows = [
        chosen("settings:ui:scheme", "Theme", "list", "The colours of the whole interface; the page changes with it", THEMES, 0, preview="theme", look="afterglow"),
        chosen("settings:ui:scale", "Interface scale", "slider", "The size of everything; smaller shows more", UI_SCALES, 10, number=100, preview="card.scale"),
        chosen("settings:ui:grid", "Card size", "slider", "The size of the video cards; smaller shows more cards on a page", GRID_SCALES, 5, number=100,
               preview="card.grid", preview_art_url="art://ridges/0/11/640x360"),
        row("settings:ui:title-lines", "Title lines", "slider", "Lines a card keeps for its title", "2", index=1, count=4, number=2, preview="card.title_lines"),
        switch("settings:ui:title-scroll", "Scroll long titles", False, "The focused card's title moves when it does not fit"),
        row("settings:ui:scroll-speed", "Title scroll speed", "slider", "How fast a long title moves", "1x", index=0, count=15),
        switch("settings:ui:subtitle-lines", "Two lines of details", False, "Channel, views and age on two lines under a card", preview="card.subtitle_lines"),
        switch("settings:ui:rounded", "Rounded corners", True, "Round or square corners for cards and pictures", preview="card.corners"),
        row("settings:ui:exit", "Circle on the main menu", "list", "Whether Circle closes ProsperoTube from Home", "Does nothing", index=0, count=3),
        row("settings:menu", "Video actions menu", "page", "Show, hide and order the rows of the actions menu", "16 rows"),
        switch("settings:interface:sounds", "Interface sounds", True, "Focus, confirm and notice sounds; the video's own sound is not touched"),
        row("settings:interface:volume", "Interface volume", "slider", "Separate from the player's volume", "7", index=7, count=11),
        switch("settings:interface:vibration", "Vibration", True, "A short pulse at the end of a list and when playback starts"),
        switch("settings:interface:reduce-motion", "Reduce motion", False, "Screens fade instead of travelling; nothing drifts"),
    ]
    for item in rows:
        if item["id"] in o:
            item.update(o[item["id"]])
    return rows


def menu_rows(order=None, hidden=()):
    keys = order or [key for key, _ in MENU]
    names = dict(MENU)
    rows = []
    for i, key in enumerate(keys):
        shown = key not in hidden
        rows.append(row("settings:menu:" + key, names[key], "order", "A row of the video actions menu, in the browser and in the player",
                        "Shown" if shown else "Hidden", on=shown, up="settings:menu:%s:up" % key if i > 0 else "",
                        down="settings:menu:%s:down" % key if i + 1 < len(keys) else "", toggle="settings:menu:%s:toggle" % key))
    rows.append(row("settings:menu:reset", "Restore the default menu", "reset", "Every row shown, in the original order",
                    confirm="Every row of the actions menu is shown again, in its original order."))
    return rows


def sb_rows(o=None):
    o = o or {}
    actions = ["Skip silently", "Skip with message", "Ask before skipping", "Do nothing"]
    rows = []
    for key, name, action in (("sponsor", "Sponsor", 1), ("intro", "Intro", 3), ("outro", "Outro", 3), ("interaction", "Interaction reminder", 1),
                              ("selfpromo", "Self promotion", 1), ("music_offtopic", "Non-music section", 3), ("preview", "Preview or recap", 3),
                              ("filler", "Filler", 3), ("poi_highlight", "Highlight", 2)):
        rows.append(row("settings:sb:action:" + key, name, "cycle", "What happens when a video reaches such a segment", actions[action]))
    rows += [
        row("settings:sb:minimum", "Ignore short segments", "slider", "Segments shorter than this are left alone", "Off", index=0, count=41),
        switch("settings:sb:repeat", "Skip once only", False, "A segment you rewound into is not skipped again"),
        row("settings:sb:exclusions", "Excluded channels", "page", "Channels whose videos are never skipped", "None"),
        row("settings:sb:colors", "Markers on the timeline", "page", "Which categories are drawn under the timeline", "9 categories"),
        switch("settings:sb:paid", "Paid-content notices", True, "Shows YouTube's own disclosure when playback starts"),
        switch("settings:sb:alt", "Use the alternative server", False, "For when SponsorBlock does not answer; segments then come from the address below"),
        row("settings:sb:server", "Alternative server", "text", "The https address of a SponsorBlock server; applies to this PS5", "Not set", text="https://"),
        row("settings:sb:about", "About SponsorBlock", "info", "Project page; open the address on another device", "sponsor.ajay.app"),
        row("settings:sb:status", "Service status", "info", "ProsperoTube does not probe it", "status.sponsor.ajay.app"),
    ]
    for item in rows:
        if item["id"] in o:
            item.update(o[item["id"]])
    return rows


def backup_rows(notice=False):
    rows = []
    if notice:
        rows.append(row("account:notice:backup", "Backup created", "info", "Kept as backup-20261013-2147", "just now"))
    rows += [
        row("settings:backup:create", "Create a backup now", "action", "A dated copy of settings and library on this PS5; sign-ins to YouTube are not included", done="Backup created"),
        row("settings:backup:list", "Restore or delete a backup", "page", "The ten newest are kept", "4 kept" if notice else "3 kept"),
        row("settings:backup:auto:7", "Automatic backup", "cycle", "Made when ProsperoTube starts; applies to this PS5", "Daily"),
        row("settings:backup:import", "Import subscriptions", "page", "Follow the channels of an export from another app"),
        row("account:notice:backup-accounts", "Sign-ins are not part of a backup", "info", "Restoring a backup neither adds nor removes an account on this PS5",
            overview="What a backup holds for an account is used only while that sign-in is still on this PS5. An account that signs in again starts as a new profile."),
    ]
    return rows


def credits():
    """The credit rows of native/tube_about.h, with its string macros resolved."""
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "..", "..", "..", "native", "tube_about.h"), encoding="utf-8") as source:
        header = source.read()
    token = re.compile(r'"((?:[^"\\]|\\.)*)"|([A-Z][A-Z0-9_]*)')
    macros = {}

    def value(expression):
        return "".join(macros[name] if name else literal.replace('\\"', '"') for literal, name in token.findall(expression))
    for name, expression in re.findall(r'^#define (TUBE_ABOUT_[A-Z_]+) ((?:"[^"\n]*"|[A-Z_]+| )+)$', header, re.M):
        try:
            macros[name] = value(expression)
        except KeyError:
            pass  # a macro of the build (the packaged version), not a text
    table = header[header.index("tube_about_credit credits[] = {"):]
    rows = []
    for line in table[:table.index("};")].splitlines()[1:]:
        # Three fields, each string literals and macro names side by side; commas inside a literal are not separators.
        if not line.strip().startswith("{"):
            continue
        fields, field, quoted, escaped = [], "", False, False
        for c in line.strip()[1:].rsplit("}", 1)[0]:
            if c == "," and not quoted:
                fields.append(field)
                field = ""
                continue
            field += c
            quoted = quoted != (c == '"' and not escaped)
            escaped = c == "\\" and not escaped
        fields.append(field)
        assert len(fields) == 3, line
        rows.append({"id": "account:notice:credit:%d" % len(rows), "is_folder": True, "interface": True,
                     "title": value(fields[0]), "subtitle": value(fields[1])})
    return rows


def about_rows():
    return [
        row("account:notice:version", "ProsperoTube 1.000.000", "info", "A native YouTube client for PS5 homebrew; GPL-3.0-or-later"),
        row("account:notice:author", "Brought to you by BlackBearReloaded", "info", "Not affiliated with or endorsed by YouTube or Google"),
        row("settings:about:credits", "Credits", "link", "What ProsperoTube builds on; the full notices ship with the package", screen="reader"),
    ]


BUTTONS = [("like", "Like"), ("dislike", "Dislike"), ("subscribe", "Subscribe"), ("playlist", "Add to playlist"), ("queue", "Playback queue"),
           ("channel", "Open channel"), ("search", "Search"), ("description", "Description"), ("share", "Share link"), ("sponsorblock", "SponsorBlock"),
           ("chat", "Live chat"), ("screen-off", "Screen off"), ("sound", "Mute and volume"), ("speed", "Video speed"), ("zoom", "Video zoom"),
           ("repeat", "Playback mode"), ("next", "Next video"), ("previous", "Previous video"), ("seek-interval", "Seek interval"),
           ("rotate", "Rotate video"), ("mirror", "Mirror video")]


def signed_in():
    f = "signed-in"
    profile(f, "mara", "Mara", False, True)
    write(f, "settings", main_list())
    write(f, "settings:extra:captions", captions_page())
    write(f, "settings:extra:audio", [
        row("settings:player-volume", "Player volume", "slider", "Above 100 % the sound is boosted and may distort", "100 %", index=20, count=61, number=100),
        switch("settings:auto-volume", "Auto volume", True, "Loud videos are turned down to YouTube's reference level"),
        switch("settings:player-mute", "Mute", False, "Silences the player until it is switched back"),
        row("settings:audio-delay", "Audio delay", "slider", "Shifts the sound against the picture", "No delay", index=14, count=29),
        switch("settings:audio-delay-enabled", "Use the audio delay", False, "Choose a delay first: there is nothing to switch on yet", disabled=True),
    ])
    write(f, "settings:extra:player", [
        switch("settings:player-clock", "Clock in the controls", False, "The time of day beside the quality"),
        switch("settings:quality-info", "Quality in the controls", True, "Resolution, frame rate and codec of what is playing"),
        switch("settings:quality-bitrate", "Bitrate in the quality", False, "Adds the stream's bitrate"),
        row("settings:time-format", "Clock format", "list", "For the clock and the estimated finish time", "24-hour clock", index=0, count=2),
        row("settings:player-exit", "Circle while watching", "list", "How a video is left", "Ask before stopping", index=0, count=3),
        row("settings:keys-left-right", "Left and Right", "list", "Quick skip works while the controls are hidden", "Seek", index=0, count=5),
        row("settings:keys-up-down", "Up and Down", "list", "Quick skip works while the controls are hidden", "Captions and the caption list", index=0, count=7),
        row("settings:seek-preview", "Picture while seeking", "list", "Small pictures of the place you are seeking to", "Single frame", index=1, count=4),
        row("settings:speed-list", "Speeds offered", "list", "Which speeds the speed picker lists", "Long list", index=1, count=3),
        switch("settings:loop-shorts", "Loop Shorts", False, "A Short repeats until another video is chosen"),
        switch("settings:remember-short-position", "Resume short videos", True, "Off: videos under six minutes start from the beginning"),
        switch("settings:player-suggestions", "Suggestions while watching", True, "Related videos, the queue and the playlist during playback"),
        switch("settings:chapter-notification", "Announce chapters", False, "Says a chapter's name when it starts"),
        switch("settings:error-fixer", "Recover from playback errors", True, "Reloads the stream or lowers its quality after decoder errors and long buffering"),
        row("settings:video-buffer", "Read ahead", "list", "How far ahead of playback a video is loaded", "Medium", index=1, count=4),
        row("settings:extra:buttons", "Player buttons", "page", "The actions the options of the player show", "18 of 21 shown"),
        row("settings:extra:dimming", "Dimming and screen off", "page", "Dim the picture while idle", "Off"),
        switch("settings:use-section-playlist", "Continue with the section", True, "After a video from a section or a channel, the next one listed there plays"),
        switch("settings:dislikes", "Show dislikes", False, "Return YouTube Dislike then learns which videos you open details for"),
    ])
    write(f, "settings:ui", interface_page())
    write(f, "settings:sb", sb_rows())
    write(f, "settings:dearrow", [
        switch("settings:dearrow:titles", "Community titles", True, "Calmer titles written by viewers; off restores the original ones"),
        switch("settings:dearrow:thumbnails", "Community thumbnails", False, "A frame of the video chosen by viewers; applies to newly loaded lists"),
        row("settings:dearrow", "Attribution", "info", "Ajay Ramachandran and contributors; CC BY-NC-SA 4.0", "dearrow.ajay.app"),
        row("settings:dearrow:about", "About DeArrow", "info", "Project page; open the address on another device", "dearrow.ajay.app"),
        row("settings:dearrow:status", "Service status", "info", "ProsperoTube does not probe it", "status.sponsor.ajay.app"),
    ])
    write(f, "settings:accounts", [
        switch("settings:separate-settings", "Separate settings per account", True, "On: each profile keeps its own preferences; applies to this PS5"),
        row("settings:lock:set", "Protect this profile with a password", "text", "Asked before the profile is selected; this PS5 stores a salted hash, never the text",
            secret=True, tag="Experimental"),
    ])
    write(f, "settings:backup", backup_rows())
    write(f, "settings:about", about_rows())
    # ---- lists of choices ----
    write(f, "settings:subtitle-size", size_choices(11))
    write(f, "settings:subtitle-style", style_choices(2))
    write(f, "settings:subtitle-position", choices("settings:subtitle-position", POSITIONS, 0, "Height of captions above the bottom edge",
                                                   facts=[{"number": n} for n in POSITION_NUMBERS], preview="caption.position", preview_art_url=PREVIEW_ART))
    write(f, "settings:languages", choices("settings:lang", [name for _, name in LANGUAGES], 0, "Captions in this language when a video offers them",
                                           ids=[code for code, _ in LANGUAGES]))
    write(f, "settings:frame-rate", choices("settings:frame-rate", RATES, 2, "Videos above this rate play in the next one below", ids=[0, 30, 60]))
    write(f, "settings:ui:scheme", scheme_choices(0))
    write(f, "settings:ui:scale", choices("settings:ui:scale", UI_SCALES, 10, "The size of everything in the interface", facts=[{"number": n} for n in UI_SCALE_NUMBERS],
                                          preview="card.scale"))
    write(f, "settings:ui:grid", choices("settings:ui:grid", GRID_SCALES, 5, "The size of the video cards", facts=[{"number": n} for n in GRID_NUMBERS], preview="card.grid"))
    write(f, "settings:ui:title-lines", choices("settings:ui:title-lines", ["1", "2", "3", "4"], 1, "Lines a card keeps for its title",
                                                facts=[{"number": n} for n in (1, 2, 3, 4)], preview="card.title_lines"))
    countries = [row("settings:content-country:default", "As YouTube chooses", "choice", "Let YouTube choose the country", selected=False)]
    countries += choices("settings:content-country", COUNTRIES, 0, "Country of recommendations, Trending and section shelves", ids=COUNTRY_IDS)
    write(f, "settings:content-country", countries[:24], more=True)
    write(f, "settings:content-country", countries[24:], page=1)
    hide = [("shorts_all", "Shorts everywhere", False), ("mixes", "Mixes", False), ("watched_watch_later", "Watched videos in Watch later", False),
            ("watched_home", "Watched videos on Home", True), ("watched_subscriptions", "Watched videos in Subscriptions", False),
            ("watched_notifications", "Watched videos in Notifications", False), ("shorts_subscriptions", "Shorts in Subscriptions", False),
            ("shorts_search", "Shorts in Search", False), ("shorts_home", "Shorts on Home", False), ("shorts_channel", "Shorts on channels", False),
            ("shorts_history", "Shorts in History", False), ("shorts_trending", "Shorts in Trending", False)]
    # The player buttons arrive in two parts: a switch changed on the second part keeps its row and the scroll.
    buttons = [switch("settings:button-" + key, name, key not in ("share", "rotate", "mirror"), "Shown among the options in the player", section="Player buttons")
               for key, name in BUTTONS]
    write(f, "settings:extra:buttons", buttons[:14], more=True)
    write(f, "settings:extra:buttons", buttons[14:], page=1)
    answer = copy.deepcopy(buttons[:14])  # what the provider answers a change with: the first part of the list
    write(f, "settings:button-next", answer, more=True)
    hide_reset = [row("settings:hide:reset", "Restore the default", "reset", "Watched videos leave Home, nothing else is hidden",
                      confirm="Watched videos leave Home again and nothing else is hidden.")]
    write(f, "settings:hide", [switch("settings:hide:" + key, name, on, "Left out of newly loaded lists of this profile", section="Hide") for key, name, on in hide] + hide_reset)
    write(f, "settings:hide:watched_home", [switch("settings:hide:" + key, name, on and key != "watched_home", "Left out of newly loaded lists of this profile", section="Hide")
                                           for key, name, on in hide] + hide_reset)
    write(f, "settings:menu", menu_rows())
    sections = []
    for i, name in enumerate(["Discover", "Recent searches", "Continue watching & history", "Favorites", "Play queue", "Followed channels"]):
        sections.append(row("settings:section:%d" % i, name, "order", "A section of the main menu", "Shown", on=True, section="Main menu",
                            up="settings:section:%d:up" % i if i else "", down="settings:section:%d:down" % i if i < 5 else "", toggle="settings:section:%d:toggle" % i))
    for i, name in enumerate(["Home recommendations", "Science", "Technology", "Gaming", "News", "Music", "Sports", "Live"]):
        n = i + 7
        sections.append(row("settings:section:%d" % n, name, "order", "A section of Discover", "Shown" if i != 4 else "Hidden", on=i != 4, section="Discover",
                            up="settings:section:%d:up" % n if i else "", down="settings:section:%d:down" % n if i < 7 else "", toggle="settings:section:%d:toggle" % n))
    for i, name in enumerate(["Northlight Atlas", "Nacre Sound"]):
        sections.append(row("settings:pin:channel:UCsample%02d" % i, name, "order", "A pinned channel on the main menu", "Pinned", on=True, section="Pinned",
                            up="settings:pin:up:channel:UCsample%02d" % i if i else "", down="settings:pin:down:channel:UCsample%02d" % i if i < 1 else "",
                            toggle=""))
    write(f, "settings:sections", sections)
    write(f, "settings:backup:list", [
        row("settings:backup:entry:backup-20261013-0912", "Today, 09:12", "page", "The newest backup", "184 KiB"),
        row("settings:backup:entry:backup-20261012-2104", "Yesterday, 21:04", "page", "", "183 KiB"),
        row("settings:backup:entry:backup-20261005-0830", "5 October, 08:30", "page", "", "171 KiB"),
    ])
    write(f, "settings:backup:entry:backup-20261013-0912", [
        row("settings:backup:restore:7:backup-20261013-0912", "Restore this backup", "action",
            "Replaces the settings and library on this PS5; the current state is kept as a safety copy",
            confirm="The settings and the library of this PS5 are replaced by the backup of today, 09:12. What is here now is kept as a safety copy.",
            confirm_label="Restore", done="Backup restored",
            overview="Backup of today, 09:12. Sign-ins to YouTube are not part of it: the accounts on this PS5 stay as they are."),
        row("settings:backup:delete:7:backup-20261013-0912", "Delete this backup", "action", "Removes this file only", danger=True,
            confirm="The backup of today, 09:12 is deleted. Nothing else changes.", confirm_label="Delete", done="Backup deleted"),
    ])
    classic = [("Classic ProsperoTube", "101116", "ef3340"), ("Teal", "16262b", "1fb5a3"), ("Dark grey", "212121", "ef3340"), ("Red", "1f1414", "e53935"),
               ("Dark grey (true black)", "000000", "ef3340"), ("Teal (true black)", "000000", "1fb5a3"), ("Dark grey monochrome", "212121", "8a8a8a"),
               ("Dark blue", "0d1626", "3d8bff"), ("Dark blue (true black)", "000000", "3d8bff")]
    write(f, "settings:ui:scheme:classic", [
        row("settings:ui:scheme:%d" % (i + 4), name, "choice", "A colour scheme of the classic interface; this interface keeps its own look", selected=False,
            preview="theme", look="", background=ground, accent=accent) for i, (name, ground, accent) in enumerate(classic)])
    # ---- answers of acting rows: the list the provider lists in answer ----
    write(f, "settings:captions", main_list({"settings:captions": {"value": "Off", "on": False}}))
    write_error(f, "settings:automatic", "Automatic captions could not be switched: the settings file on this PS5 could not be written.")
    write(f, "settings:codec", main_list({"settings:codec": {"value": "AV1"}}))
    write(f, "settings:subtitle-size:12", size_choices(12))
    write(f, "settings:subtitle-size:13", size_choices(13))
    write(f, "settings:subtitle-size:10", size_choices(10))
    write(f, "settings:subtitle-size:19", size_choices(19))
    write(f, "settings:subtitle-style:3", style_choices(3))
    write(f, "settings:subtitle-style:4", style_choices(4))
    write(f, "settings:subtitle-style:1", style_choices(1))
    write(f, "settings:ui:scheme:1", scheme_choices(1))
    write(f, "settings:ui:scheme:2", scheme_choices(2))
    write(f, "settings:ui:grid:6", choices("settings:ui:grid", GRID_SCALES, 6, "The size of the video cards", facts=[{"number": n} for n in GRID_NUMBERS], preview="card.grid"))
    write(f, "settings:ui:grid:7", choices("settings:ui:grid", GRID_SCALES, 7, "The size of the video cards", facts=[{"number": n} for n in GRID_NUMBERS], preview="card.grid"))
    write(f, "settings:ui:title-lines:2", choices("settings:ui:title-lines", ["1", "2", "3", "4"], 2, "Lines a card keeps for its title",
                                                  facts=[{"number": n} for n in (1, 2, 3, 4)], preview="card.title_lines"))
    write(f, "settings:ui:rounded", interface_page({"settings:ui:rounded": {"value": "Off", "on": False}}))
    write(f, "settings:lang:de", choices("settings:lang", [name for _, name in LANGUAGES], 1, "Captions in this language when a video offers them",
                                         ids=[code for code, _ in LANGUAGES]))
    keys = [key for key, _ in MENU]
    moved = [keys[1], keys[0]] + keys[2:]
    write(f, "settings:menu:dislike:up", menu_rows(moved))
    write(f, "settings:menu:dislike:toggle", menu_rows(hidden=("dislike",)))
    write_error(f, "settings:menu:unrate:up", "The menu could not be saved. Its order is as before.")
    write(f, "settings:reset:captions", main_list({"settings:captions": {"value": "Off", "on": False}, "settings:automatic": {"value": "Off", "on": False}}))
    write(f, "settings:backup:create", backup_rows(notice=True))
    write(f, "settings:sb:server", sb_rows({"settings:sb:server": {"value": "sb.example.org", "text": "https://sb.example.org"}}))
    write(f, "settings:sb:action:intro", sb_rows({"settings:sb:action:intro": {"value": "Skip silently"}}))
    write(f, "settings:clear-history", main_list())
    write_error(f, "settings:backup:import", "The import folder on this PS5 could not be opened.")
    # The credits as the provider lists them: the rows of native/tube_about.h. The frame's boards read them in "guest".
    write(f, "settings:about:credits", credits())
    write("guest", "settings:about:credits", credits())


def guest():
    f = "guest"
    o = {
        "settings:remote": {"value": "No phone linked"},
        "settings:accounts": {"value": "Guest · no account on this PS5"},
        "settings:backup": {"value": "No backup yet"},
        "settings:about": {"update": "1.000.000 is the latest version", "attention": False},
        "settings:extra:captions": {"value": "100 % · player's own"},
        "settings:extra:audio": {"value": "100 %"},
        "settings:codec": {"value": "Automatic"},
        "settings:dearrow": {"value": "Off"},
        "settings:sb": {"value": "Skipping sponsors"},
        "settings:history": {"value": "On this PS5 only"},
    }
    rows = [r for r in main_list(o) if not r["id"].startswith("settings:reset:")]
    for r in rows:
        if "confirm" in r:
            r["confirm"] = r["confirm"].replace("for Mara", "for Guest")
    write(f, "settings", rows)
    write(f, "settings:accounts", [
        switch("settings:separate-settings", "Separate settings per account", False, "Off: every profile uses the same preferences; applies to this PS5"),
        row("account:notice", "Password protection", "info", "Select an account profile to protect it with a password", tag="Experimental"),
    ])


def empty():
    f = "empty"
    profile(f, "mara", "Mara", False, True)
    write(f, "settings", [])
    write(f, "settings:hide", [])


def newer_provider():
    f = "newer-provider"
    profile(f, "mara", "Mara", False, True)
    rows = main_list()
    rows.insert(12, switch("settings:picture-in-picture", "Keep the video while browsing", True, "A row a later version of ProsperoTube lists; this interface does not know it yet"))
    rows.insert(30, row("settings:night-shift", "Warmer colours at night", "list", "Another row this interface was never told about", "After sunset", index=1, count=3))
    write(f, "settings", rows)
    write(f, "settings:extra:captions", captions_page() + [
        switch("settings:subtitle-karaoke", "Word by word", False, "A caption preference of a later version: it joins Appearance by its id"),
        switch("settings:caption-dictionary", "Look up words", False, "A row of the captions list no rule knows: it stands under More"),
    ])


def long_texts():
    f = "long-texts"
    profile(f, "maximiliane", "Maximiliane-Sophie von Hohenlohe-Langenburg", False, True)
    o = {
        "settings:frame-rate": {"value": "Auflösung und Bildrate bis höchstens 60 Bilder pro Sekunde"},
        "settings:codec": {"value": "VP9 (Softwaredekodierung)"},
        "settings:languages": {"value": "Deutsch (Österreich)"},
        "settings:extra:captions": {"value": "Untertiteleinstellungen und Übersetzungen · Weiß auf halbtransparentem Hintergrund"},
        "settings:extra:audio": {"value": "Lautstärkeanpassung an · 100 % · Tonverzögerung 250 ms"},
        "settings:content-language": {"value": "Deutsch"},
        "settings:content-country": {"value": "Vereinigtes Königreich Großbritannien und Nordirland"},
        "settings:ui": {"value": "Afterglow · Benutzeroberflächentöne an · Vibration aus"},
        "settings:sb": {"value": "Überspringt 3 Kategorien, fragt bei 2 weiteren nach"},
        "settings:dearrow": {"value": "Titel an · Vorschaubilder aus"},
        "settings:accounts": {"value": "Maximiliane-Sophie · 2 Profile · Kennwortschutz"},
        "settings:remote": {"value": "Maximiliane-Sophies Smartphone ist verbunden"},
        "settings:backup": {"value": "Letzte Sicherung heute, 09:12, zehn Sicherungen aufbewahrt"},
        "settings:about": {"value": "ProsperoTube 1.000.000", "update": "1.001.000 steht zur Installation bereit und wurde bereits heruntergeladen"},
    }
    write(f, "settings", main_list(o))
    write(f, "settings:extra:captions", [
        row("settings:subtitle-size", "Schriftgröße der Untertitel im Verhältnis zur Wiedergabe", "slider",
            "Die Vorschau zeigt die Größe so, wie sie auf Ihrem Bildschirm erscheint. Sehr große Untertitel können bei langen Zeilen in zwei Zeilen umbrechen, "
            "und bei Videos mit eingebrannten Texten wichtige Bildteile verdecken.", "200 %", index=19, count=20, number=200, preview="caption.size",
            preview_art_url=PREVIEW_ART),
        row("settings:subtitle-style", "Darstellung", "list", "Wie die Buchstaben vom Bild abgehoben werden.", "Weiß auf halbtransparentem Hintergrund", index=2,
            count=7, preview="caption.style", **STYLE_FACTS[2]),
        row("settings:subtitle-position", "Position", "slider", "Wo Untertitel stehen, wenn das Video sie nicht selbst platziert.", "97,5 %", index=40,
            count=len(POSITIONS), number=975, preview="caption.position"),
        switch("settings:more-subtitles", "Automatisch übersetzte Untertitel in der Untertitelliste anbieten", True,
               "Fügt die automatischen Übersetzungen von YouTube zur Liste hinzu; das Öffnen eines Videos dauert dadurch etwas länger."),
        switch("settings:caption-memory", "Untertitelwahl je Kanal merken", False, "Behält die Untertitelwahl, die Sie beim Ansehen treffen, Kanal für Kanal."),
        row("settings:subtitle-font", "Schriftart", "list", "Eine Zeile mit einem sehr langen Wert und einem Kennzeichen.",
            "Atkinson Hyperlegible (empfohlen für Legasthenie und Sehschwäche)", index=1, count=4, tag="Experimentell"),
    ])
    write(f, "settings:languages", choices("settings:lang", ["Deutsch (Österreich)", "Niederländisch (Belgien, automatisch erzeugt und maschinell übersetzt)",
                                                           "Portugiesisch (Brasilien)", "Vietnamesisch"], 0,
                                           "Untertitel in dieser Sprache verwenden, sobald ein Video sie anbietet", ids=["de-AT", "nl-BE", "pt-BR", "vi"]))


signed_in()
guest()
empty()
newer_provider()
long_texts()
print("fixtures written under", ROOT)
