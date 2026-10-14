#!/usr/bin/env python3
"""Writes the boards of the player (tests/afterglow/boards/player*.json).

A board of the player names a PlayerView (fixtures/<set>/player/<name>.json),
changes it before the first frame ("view") and in its steps ("set"), and says
what must hold. The views of the proposal's boards 16 to 24 share their parts
(the related videos, the caption tracks, the options), so they are written
once here. Every name, title, number and message is invented.

    python3 tests/afterglow/tools/player_boards.py
"""
import json
import pathlib

BOARDS = pathlib.Path(__file__).resolve().parents[1] / "boards"
DOT = " · "


def hint(button, label, second=None):
    out = {"button": button, "label": label}
    if second:
        out["second"] = second
    return out


def card(title, channel, meta, seconds, art, badges=0):
    return {"id": "vid" + art.split("/")[-1], "title": title, "channel": channel, "meta": meta, "duration_s": seconds,
            "thumb": "art://" + art, "badges": badges}


K4, CC = 8, 32
AURORA = card("Aurora Chasing at -30° (We Almost Missed It)", "Northlight Atlas", "2.2M views" + DOT + "8 months ago", 1198, "aurora/0/145", K4 | CC)
RELATED = [
    AURORA,
    card("Blue Hour on the Northern Coast, No Talking", "Northlight Atlas", "640K views" + DOT + "2 months ago", 2490, "ocean/1/241", K4),
    card("The Forest at Night Sounds Nothing Like You Think", "Northlight Atlas", "380K views" + DOT + "6 weeks ago", 1329, "aurora/2/313"),
    card("A Year on a Tidal Island", "Northlight Atlas", "4.8M views" + DOT + "2 years ago", 3484, "ocean/3/385", K4 | CC),
    card("Teal Dawn Above the Valley", "Northlight Atlas", "410K views" + DOT + "10 months ago", 920, "ridges/2/421", K4),
    card("Crossing the Salt Flats by Bicycle", "Vantor Trails", "720K views" + DOT + "3 months ago", 2000, "dunes/3/133", K4),
    card("Moonrise Over the Dune Sea in Real Time", "Vantor Trails", "540K views" + DOT + "1 year ago", 372, "dunes/1/373", K4),
    card("Violet Hour: A Short Film", "Slowreel", "233K views" + DOT + "3 weeks ago", 451, "ridges/6/409"),
]
UP_NEXT = {
    "open": True, "chip": "Autoplay is on", "note": "From Northlight Atlas and channels like it", "cards": RELATED[:5], "focus": 0,
    "first": 0, "total": 5,
    "hints": [hint("cross", "Play now"), hint("square", "Add to queue"), hint("circle", "Close")],
}

TRACKS = [("Off", ""), ("English", ""), ("English", "Automatic"), ("Deutsch", "Translated"), ("Español", "Translated"), ("Français", ""),
          ("Italiano", "Translated"), ("Polski", "Translated"), ("Português (Brasil)", "Translated")]
CAPTIONS = {
    "open": True, "kind": "captions", "title": "Captions", "tabs": ["Captions", "Second language"], "tab": 0,
    "rows": [{"label": label, "value": value, "current": i == 1, "focused": i == 1} for i, (label, value) in enumerate(TRACKS)],
    "note": "Style, size and position are in Settings, under Captions.",
    "hints": [hint("cross", "Choose"), hint("l2", "First or second", "r2")],
}
SPOKEN = {"lines": ["The wind did not stop until morning."], "second": ["Der Wind hörte erst am Morgen auf."], "size": 38, "second_rgb": "#ffe9a8"}

OPTION_ROWS = [("Quality", "Auto", "quality"), ("Speed", "1×", "speed"), ("Audio track", "English", "audio"), ("Volume", "100 %", "audio"),
               ("Video sizing", "Fit", "grid"), ("Sleep timer", "Off", "clock"), ("SponsorBlock", "On", "skip"), ("Playback details", "", "info")]
QUALITIES = [("Auto", "2160p60 now"), ("2160p60", "4K"), ("1440p60", ""), ("1080p60", "HD"), ("720p60", ""), ("480p", ""), ("360p", ""), ("144p", "")]
OPTIONS = {
    "open": True, "kind": "options", "title": "Playback",
    "rows": [{"label": label, "value": value, "icon": icon, "chevron": True, "focused": i == 0} for i, (label, value, icon) in enumerate(OPTION_ROWS)],
    "choices_title": "Quality",
    "choices": [{"label": label, "note": note, "current": i == 0} for i, (label, note) in enumerate(QUALITIES)],
    "hints": [hint("cross", "Open"), hint("circle", "Close")],
}
SPEEDS = [("0.5×", ""), ("0.75×", ""), ("1×", "Normal"), ("1.25×", ""), ("1.5×", ""), ("1.75×", ""), ("2×", ""), ("3×", "")]

SKIPPED = {"shown": True, "kind": "skip", "title": "Sponsor skipped", "detail": "4:10 to 5:25" + DOT + "1 min 15 s", "rgb": "#00d400",
           "action": hint("cross", "Undo"), "left": 0.34}
ASK = {"shown": True, "kind": "ask", "label": "SponsorBlock", "title": "Skip this sponsor segment? It ends at 5:25.", "rgb": "#00d400",
       "action": hint("cross", "Skip"), "second": hint("circle", "Keep"), "left": -1}
NEXT = {"shown": True, "watched": "Seven Days Alone on the Ridge Line — A Silent Hiking Film", "card": AURORA, "seconds_left": 2.0, "seconds_total": 5, "focus": 0}
STOP = {"shown": True, "kind": "question", "title": "Stop watching?",
        "body": "Your place is kept: 31:12 of 48:12. The video is waiting under Continue watching.",
        "buttons": [{"label": "Keep watching", "primary": True}, {"label": "Stop"}], "focus": 0}
DETAILS = {
    "shown": True,
    "lines": ["VIDEO  vp9  3840x2160  59.940 fps  (Hardware)", "AUDIO  opus  eng  2ch  48000 Hz", "SUBS  ON  /  EXTERNAL",
              "RENDER 60 fps  /  DECODE 60 fps  /  UPSCALER Off", "QUEUES  video 61/96  audio 58/96  pcm 4", "CLOCKS  video 1882.41  audio 1882.43  delta -0.020"],
    "graphs": [{"label": "GPU", "value": "31%", "samples": [0.2, 0.28, 0.31, 0.26, 0.3, 0.36, 0.33, 0.29, 0.31, 0.3, 0.34, 0.31]},
               {"label": "RAM", "value": "742 MB", "samples": [0.4, 0.41, 0.41, 0.42, 0.42, 0.43, 0.44, 0.44, 0.44, 0.45, 0.45, 0.45]},
               {"label": "CPU", "value": "18%", "samples": [0.12, 0.2, 0.16, 0.18, 0.22, 0.15, 0.17, 0.19, 0.16, 0.2, 0.18, 0.18]}],
}


def board(name, player, fixture="signed-in", view=None, steps=None, expect=None, **more):
    out = {"name": name, "fixture": fixture, "player": player}
    if view:
        out["view"] = view
    if steps:
        out["steps"] = steps
    out.update(more)
    out["expect"] = {"screen": "player", **(expect or {})}
    return out


def write(file, about, boards):
    (BOARDS / file).write_text(json.dumps({"about": about, "boards": boards}, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")


def approved():
    """Boards 16 to 24 of the proposal, as prepared states: nothing glides into place."""
    return [
        board("player-controls", "ridge",
              expect={"focus": "osd-playing", "state": {"controls": "shown", "playback": "playing", "chapter": "5", "chapters": "7", "segments": "4", "sheet": "", "notice": ""}}),
        board("player-seek", "ridge", view={"scrubbing": True, "scrub_target": 2059, "preview": [{"art": "art://ridges/3/397", "offset": 0}]},
              expect={"focus": "osd-scrub", "state": {"playback": "seeking", "target": "2059", "position": "1882", "preview": "1", "chapter": "5"}}),
        board("player-up-next", "ridge", view={"up_next": UP_NEXT},
              expect={"focus": "upnext/0", "state": {"up_next": "1", "cards": "5"}}),
        board("player-captions", "ridge", view={"sheet": CAPTIONS, "captions": SPOKEN},
              expect={"focus": "sheet/1", "state": {"sheet": "captions", "rows": "9", "captions": "2"}}),
        board("player-options", "ridge", view={"sheet": OPTIONS},
              expect={"focus": "sheet/0", "state": {"sheet": "options", "rows": "8"}}),
        board("player-live-chat", "live", view={"chat": {"side": 2}},
              expect={"focus": "osd-playing", "state": {"live": "1", "chat": "theatre", "chat_lines": "11"}}),
        board("player-skip", "ridge", view={"position": 329, "buffered": 520, "notice": SKIPPED},
              expect={"focus": "osd-playing", "state": {"notice": "skip", "chapter": "1", "position": "329"}}),
        board("player-next-video", "ridge", view={"position": 2892, "next": NEXT},
              expect={"focus": "next/play", "state": {"next": "1"}}),
        board("player-stop", "ridge", view={"paused": True, "position": 1880, "dialog": STOP},
              expect={"focus": "dialog/0", "dialog": 1, "state": {"playback": "paused"}}),
    ]


def states():
    """Every other state the player has."""
    carousel = [{"art": "art://ridges/3/397", "offset": 0}, {"art": "art://ridges/0/11", "offset": -1}, {"art": "art://ridges/6/409", "offset": 1},
                {"art": "art://ridges/2/421", "offset": -2}, {"art": "art://dunes/3/133", "offset": 2}]
    replay = {"side": 2, "theatre": True, "title": "Chat replay", "lines": [
        {"author": "quietpixel", "text": "the light coming over the ridge. Rewound it four times."},
        {"author": "Mirela K.", "text": "no voice-over, no music, just wind and boots"},
        {"author": "Northlight Atlas", "text": "Route notes and the gear list are in the description.", "badge": 3}],
        "hints": [hint("cross", "Pause"), hint("triangle", "Up next"), hint("options", "Options")]}
    return [
        board("player-paused", "ridge", view={"paused": True},
              expect={"focus": "osd-paused", "state": {"playback": "paused"}}),
        board("player-loading", "ridge", view={"loading": True, "meta": "Buffering...", "channel": "", "position": 2059, "buffered": -1},
              expect={"state": {"playback": "loading"}}),
        board("player-hidden", "ridge", view={"chrome": 0, "captions": {"lines": ["We left the hut before the light came.", "Nobody spoke for an hour."], "size": 50}},
              expect={"focus": "osd-playing", "state": {"controls": "hidden", "captions": "2"}}),
        board("player-captions-styled", "ridge", view={"chrome": 0, "captions": {
            "lines": ["We left the hut before the light came."], "second": ["Wir verließen die Hütte vor dem ersten Licht."], "size": 60, "custom": True,
            "text_rgb": "#ffff00", "plate_alpha": 0, "edge": 1, "second_top": True, "second_rgb": "#66e3ff"}},
              expect={"state": {"controls": "hidden", "captions": "2"}}),
        board("player-seek-carousel", "ridge", view={"scrubbing": True, "scrub_target": 2059, "preview": carousel},
              expect={"focus": "osd-scrub", "state": {"preview": "5"}}),
        board("player-seek-back", "ridge", view={"scrubbing": True, "scrub_target": 1210, "preview": []},
              expect={"focus": "osd-scrub", "state": {"playback": "seeking", "preview": "0", "chapter": "3", "target": "1210"}}),
        board("player-seek-edge", "ridge", view={"scrubbing": True, "scrub_target": 2880, "preview": carousel},
              expect={"focus": "osd-scrub", "state": {"chapter": "7"}}),
        board("player-up-next-third", "ridge", view={"up_next": {**UP_NEXT, "focus": 2}},
              expect={"focus": "upnext/2"}),
        board("player-up-next-more", "ridge", view={"up_next": {**UP_NEXT, "cards": RELATED, "total": 8, "focus": 6}},
              expect={"focus": "upnext/6", "state": {"cards": "8"}}),
        board("player-up-next-loading", "ridge", view={"up_next": {**UP_NEXT, "cards": [], "loading": True, "total": 0}},
              expect={"state": {"up_next": "1", "cards": "0"}}),
        board("player-up-next-empty", "ridge", view={"up_next": {**UP_NEXT, "cards": [], "chip": "Autoplay is off", "note": "", "total": 0,
                                                              "message": "Related videos could not be loaded. The video keeps playing."}},
              expect={"state": {"up_next": "1", "cards": "0"}}),
        board("player-captions-second", "ridge", view={"sheet": {**CAPTIONS, "tab": 1, "rows": [
            {"label": label, "value": value, "current": i == 3, "focused": i == 4} for i, (label, value) in enumerate(TRACKS)]}, "captions": SPOKEN},
              expect={"focus": "sheet/4", "state": {"sheet": "captions"}}),
        board("player-options-speed", "ridge", view={"sheet": {**OPTIONS, "rows": [
            {"label": label, "value": value, "icon": icon, "chevron": True, "focused": i == 1} for i, (label, value, icon) in enumerate(OPTION_ROWS)],
            "choices_title": "Speed", "choices": [{"label": label, "note": note, "current": i == 2} for i, (label, note) in enumerate(SPEEDS)]}},
              expect={"focus": "sheet/1"}),
        board("player-options-empty", "ridge", view={"sheet": {"open": True, "kind": "options", "title": "Audio track", "rows": [], "choices": [],
                                                           "empty_title": "No audio tracks", "empty_hint": "This video has one soundtrack and nothing to choose between.",
                                                           "hints": [hint("circle", "Close")]}},
              expect={"state": {"sheet": "options", "rows": "0"}}),
        board("player-live", "live",
              expect={"focus": "osd-playing", "state": {"live": "1", "chat": "off", "controls": "shown"}}),
        board("player-live-chat-over", "live", view={"chat": {"side": 2, "theatre": False, "compose": "", "filter": ""}},
              expect={"state": {"chat": "right", "chat_lines": "11"}}),
        board("player-live-chat-left", "live", view={"chat": {"side": 1, "theatre": False, "compose": "", "filter": "", "title": "Top chat"}},
              expect={"state": {"chat": "left"}}),
        board("player-live-chat-waiting", "live", view={"chat": {"side": 2, "theatre": False, "compose": "", "filter": "", "lines": [],
                                                              "status": "Chat is unavailable for this stream right now. Choose it again in the options to retry."}},
              expect={"state": {"chat": "right", "chat_lines": "0"}}),
        board("player-chat-replay", "ridge", view={"chat": replay},
              expect={"state": {"chat": "theatre", "live": "0", "chat_lines": "3"}}),
        board("player-ask", "ridge", view={"position": 251, "chrome_hidden": True, "notice": ASK},
              expect={"focus": "sponsor-review", "state": {"notice": "ask", "controls": "hidden"}}),
        board("player-notice", "ridge", view={"notice": {"shown": True, "kind": "info", "label": "Paid promotion", "title": "This video includes a paid promotion.", "left": 0.7}},
              expect={"state": {"notice": "info"}}),
        board("player-notice-error", "ridge", view={"notice": {"shown": True, "kind": "error", "label": "Connection lost", "title": "No data from the server. Trying again...", "left": 0.5}},
              expect={"state": {"notice": "error"}}),
        board("player-notice-next", "ridge", view={"position": 2890, "notice": {"shown": True, "kind": "next", "title": "The next video is on its way", "detail": "Any button cancels"}},
              expect={"state": {"notice": "next"}}),
        board("player-details", "ridge", view={"stats": DETAILS, "fps": 60},
              expect={"state": {"details": "1"}}),
        board("player-dimmed", "ridge", view={"paused": True, "dim": 0.6},
              expect={"state": {"dim": "60"}}),
        board("player-resume", "ridge", view={"chrome": 0, "dialog": {"shown": True, "kind": "question", "title": "Continue where you stopped?",
                                                                  "body": "Stopped at 31:12 of 48:12.", "buttons": [{"label": "Resume", "primary": True}, {"label": "Start over"}], "focus": 0}},
              expect={"focus": "dialog/0", "dialog": 1}),
        board("player-toast", "ridge", view={"chrome": 0, "toast": {"shown": True, "title": "Playing at 1080p", "message": "2160p is not offered for this video", "kind": 0}},
              expect={"state": {"toast": "1"}}),
    ]


def app_forms():
    """What the app can feed today: the lists EVO's pickers hold, the buttons the player answers to."""
    rows = [("Video actions", "Like, save, share and more for this video", "", "", True), ("Video presets", "One resolution, frame rate and codec for this profile", "Custom limits", "", True),
            ("AUTO", "The best quality available", "", "", False), ("2160p", "3840x2160  -  VP9  -  18.4 Mbps  -  60 fps", "", "", False),
            ("1080p", "1920x1080  -  AVC  -  4.5 Mbps  -  30 fps", "", "", False), ("eng", "opus  2 CH  48000 Hz", "", "STEREO", False),
            ("Continuous playback", "Playback mode; saved for this profile", "", "", False), ("Time display", "Remaining and finish times account for playback speed", "remaining", "", False),
            ("Speed", "Keep this position and preserve audio pitch", "1.25x", "", False)]
    options = {"open": True, "kind": "options", "title": "Playback options", "first": 0, "total": 62,
               "rows": [{"label": label, "detail": detail, "value": value, "tag": tag, "chevron": chevron, "current": i == 2, "focused": i == 3}
                        for i, (label, detail, value, tag, chevron) in enumerate(rows)],
               "hints": [hint("cross", "Choose"), hint("circle", "Back")]}
    tracks = {"open": True, "kind": "captions", "title": "Captions", "pills": ["SIZE  MEDIUM", "SYNC: +100 ms"], "first": 0, "total": 8,
              "rows": [{"label": "Off", "detail": "No captions", "focused": False},
                       {"label": "English", "detail": "Written by the channel", "current": True, "tag": "PRIMARY"},
                       {"label": "English (auto-generated)", "detail": "Made by speech recognition", "focused": True},
                       {"label": "Deutsch", "detail": "Machine translation", "tag": "TRANSLATED"},
                       {"label": "Español", "detail": "Machine translation", "tag": "SECONDARY"},
                       {"label": "Auto-sync", "detail": "Not available for this video", "action": True, "disabled": True}],
              "preview": "The quick brown fox jumps over the lazy dog", "preview_size": 50,
              "hints": [hint("cross", "Choose"), hint("square", "Size"), hint("dpad", "Sync"), hint("circle", "Back")]}
    related = {"open": True, "title": "Related videos", "chip": "Autoplay is off", "marks_next": False, "note": "", "first": 0, "total": 9, "focus": 1,
               "cards": [{**c, "channel": c["channel"] + DOT + c["meta"].split(DOT)[0], "thumb": "", "badges": 0, "duration_s": 0} for c in RELATED],
               "hints": [hint("cross", "Play"), hint("circle", "Back")]}
    return [
        board("player-app", "plain", fixture="empty",
              expect={"focus": "osd-playing", "state": {"chapters": "0", "segments": "0", "chapter": "0"}}),
        board("player-app-seek", "plain", fixture="empty", view={"scrubbing": True, "scrub_target": 1404},
              expect={"focus": "osd-scrub", "state": {"preview": "0"}}),
        board("player-app-options", "plain", fixture="empty", view={"chrome": 0, "sheet": options},
              expect={"focus": "sheet/3", "state": {"sheet": "options", "rows": "9", "controls": "hidden"}}),
        board("player-app-captions", "plain", fixture="empty", view={"chrome": 0, "sheet": tracks},
              expect={"focus": "sheet/2", "state": {"sheet": "captions", "rows": "6"}}),
        board("player-app-related", "plain", fixture="empty", view={"chrome": 0, "up_next": related},
              expect={"focus": "upnext/1", "state": {"cards": "8"}}),
        board("player-app-stop", "plain", fixture="empty", view={"chrome": 0, "dialog": {**STOP, "body": "Your place is kept: 12:34 of 41:30. The video is waiting under Continue watching.", "focus": 1}},
              expect={"focus": "dialog/1", "dialog": 1}),
    ]


def long_texts():
    """German-length titles, a very long channel name, big numbers, a video longer than an hour."""
    long_cards = [
        card("Polarlichtjagd bei -30° (fast hätten wir es verpasst) — die ganze Nacht in Echtzeit, ohne Schnitt und ohne Musik", "Nordlicht-Atlas Dokumentarfilmgesellschaft",
             "2.234.567 Aufrufe" + DOT + "vor 8 Monaten", 11998, "aurora/0/145", K4 | CC),
        card("Blaue Stunde an der Nordküste", "Nordlicht-Atlas Dokumentarfilmgesellschaft für Hochgebirgswanderungen", "640.000 Aufrufe", 2490, "ocean/1/241", K4),
        card("Donaudampfschifffahrtsgesellschaftskapitänsmützenabzeichen", "Tidewrit", "380.000 Aufrufe", 1329, "aurora/2/313"),
        card("Ein Jahr auf einer Gezeiteninsel", "Nordlicht-Atlas", "4,8 Mio. Aufrufe", 3484, "ocean/3/385", K4 | CC),
        card("Türkisfarbene Morgendämmerung über dem Tal", "Nordlicht-Atlas", "410.000 Aufrufe", 920, "ridges/2/421", K4),
    ]
    sheet_rows = [("Wiedergabequalität und Bildwiederholrate", "Automatisch (2160p60)", "quality"), ("Wiedergabegeschwindigkeit", "1,25×", "speed"),
                  ("Tonspur und Audiobeschreibung", "Deutsch (Originalfassung)", "audio"), ("Lautstärke", "100 %", "audio"),
                  ("Bildgröße und Seitenverhältnis", "Einpassen", "grid"), ("Einschlaf-Timer", "Aus", "clock"),
                  ("SponsorBlock-Segmente überspringen", "Ein", "skip"), ("Wiedergabedetails für Technikbegeisterte", "", "info")]
    options = {"open": True, "kind": "options", "title": "Wiedergabeoptionen und Bildeinstellungen",
               "rows": [{"label": label, "value": value, "icon": icon, "chevron": True, "focused": i == 2} for i, (label, value, icon) in enumerate(sheet_rows)],
               "choices_title": "Tonspur und Audiobeschreibung",
               "choices": [{"label": "Deutsch (Originalfassung)", "note": "Opus Stereo", "current": True}, {"label": "English", "note": "Synchronfassung, AAC Stereo"},
                           {"label": "Português (Brasil)", "note": "Synchronfassung"}],
               "hints": [hint("cross", "Einstellung öffnen"), hint("circle", "Wiedergabeoptionen schließen")]}
    captions = {"open": True, "kind": "captions", "title": "Untertitel und Zweitsprache", "tabs": ["Untertitel", "Zweite Sprache"], "tab": 0,
                "rows": [{"label": "Aus", "focused": False}, {"label": "Deutsch (automatisch erzeugt, mit Sprechererkennung)", "value": "Automatisch", "current": True, "focused": True},
                         {"label": "English (United Kingdom)", "value": "Übersetzt"}, {"label": "Português (Brasil)", "value": "Maschinell übersetzt"},
                         {"label": "Ελληνικά", "value": "Übersetzt"}, {"label": "Русский", "value": "Übersetzt"}],
                "note": "Stil, Größe und Position der Untertitel findest du in den Einstellungen unter Untertitel.",
                "hints": [hint("cross", "Auswählen"), hint("l2", "Erste oder zweite Sprache", "r2")]}
    spoken = {"lines": ["Der Wind hörte erst am nächsten Morgen auf, als wir schon längst nicht mehr damit gerechnet hatten und die Zelte abbauten."],
              "second": ["The wind did not stop until morning."], "size": 38}
    chat = {"side": 2, "theatre": True, "title": "Live-Chat", "filter": "Top-Chat", "compose": "Nachricht schreiben", "lines": [
        {"author": "Donaudampfschifffahrtsgesellschaftskapitän Friedrich-Wilhelm", "text": "Wahnsinnsaufnahmen!"},
        {"author": "Nordlicht-Atlas", "badge": 3, "text": "Die nächste Etappe ist neu für uns. Falls sie schiefgeht, war es Absicht, und wir schneiden nichts davon heraus, versprochen, auch nicht den Sturz."},
        {"author": "Wren (mod)", "badge": 2, "text": "Routennotizen und Ausrüstungsliste sind angepinnt."},
        {"author": "kurz", "text": "ok"}],
        "hints": [hint("cross", "Pause"), hint("l2", "Im Chat blättern", "r2"), hint("triangle", "Chat ausblenden"), hint("options", "Wiedergabeoptionen")]}
    return [
        board("player-long-controls", "ridge", fixture="long-texts",
              expect={"focus": "osd-playing", "state": {"chapter": "5", "duration": "11532"}}),
        board("player-long-seek", "ridge", fixture="long-texts", view={"scrubbing": True, "scrub_target": 11400, "preview": [{"art": "art://ridges/3/397", "offset": 0}]},
              expect={"focus": "osd-scrub", "state": {"chapter": "7"}}),
        board("player-long-up-next", "ridge", fixture="long-texts", view={"up_next": {
            "open": True, "title": "Als Nächstes", "chip": "Automatische Wiedergabe ist eingeschaltet", "note": "Von Nordlicht-Atlas Dokumentarfilmgesellschaft und ähnlichen Kanälen",
            "cards": long_cards, "focus": 0, "hints": [hint("cross", "Jetzt abspielen"), hint("square", "Zur Warteschlange hinzufügen"), hint("circle", "Schließen")]}},
              expect={"focus": "upnext/0"}),
        board("player-long-captions", "ridge", fixture="long-texts", view={"sheet": captions, "captions": spoken},
              expect={"focus": "sheet/1"}),
        board("player-long-options", "ridge", fixture="long-texts", view={"sheet": options},
              expect={"focus": "sheet/2"}),
        board("player-long-notice", "ridge", fixture="long-texts", view={"notice": {
            "shown": True, "kind": "ask", "label": "SponsorBlock", "title": "Diesen Abschnitt mit Eigenwerbung und Zuschauerinteraktion überspringen? Er endet bei 2:07:45.",
            "rgb": "#ffff00", "action": hint("cross", "Überspringen"), "second": hint("circle", "Behalten")}},
              expect={"focus": "sponsor-review"}),
        board("player-long-next-video", "ridge", fixture="long-texts", view={"position": 11532, "next": {
            "shown": True, "watched": "", "card": long_cards[0], "seconds_left": 4.2, "seconds_total": 5, "focus": 1}},
              expect={"focus": "next/cancel"}),
        board("player-long-stop", "ridge", fixture="long-texts", view={"paused": True, "dialog": {
            "shown": True, "kind": "question", "title": "Wiedergabe beenden und zur Übersicht zurückkehren?",
            "body": "Deine Position bleibt gespeichert: 2:05:22 von 3:12:12. Das Video wartet unter „Weiterschauen“ auf dich, bis du es zu Ende gesehen hast.",
            "buttons": [{"label": "Weiterschauen", "primary": True}, {"label": "Wiedergabe beenden"}], "focus": 0}},
              expect={"focus": "dialog/0", "dialog": 1}),
        board("player-long-chat", "ridge", fixture="long-texts", view={"chat": chat, "live": True, "live_status": "12.184 Zuschauer" + DOT + "gestartet vor 1 Std. 42 Min."},
              expect={"state": {"chat": "theatre"}}),
    ]


def motion():
    """Transitions, as their steps drive them; reduced motion; the cues the moments ask for."""
    return [
        board("player-pause-pop", "ridge", steps=[{"set": {"paused": True}, "wait": 0.1}],
              expect={"focus": "osd-paused", "state": {"playback": "paused", "cue": "toggle"}}),
        board("player-resumes", "ridge", view={"paused": True}, steps=[{"set": {"paused": False}, "wait": 0.6}],
              expect={"focus": "osd-playing", "state": {"cue": "resume"}}),
        board("player-seek-starts", "ridge", steps=[{"set": {"scrubbing": True, "scrub_target": 1892, "preview": [{"art": "art://ridges/3/397", "offset": 0}]}, "wait": 0.05},
                                                  {"set": {"scrub_target": 2059}, "wait": 0.1}],
              expect={"focus": "osd-scrub", "state": {"cue": "tick"}}),
        board("player-seek-detent", "ridge", view={"scrubbing": True, "scrub_target": 2300}, steps=[{"set": {"scrub_target": 2340}, "wait": 0.02}],
              expect={"state": {"chapter": "6", "cue": "tick"}}),
        board("player-seek-lands", "ridge", view={"scrubbing": True, "scrub_target": 2059},
              steps=[{"set": {"scrubbing": False, "position": 2059}, "wait": 0.02}, {"wait": 0.8}],
              expect={"focus": "osd-playing", "state": {"position": "2059", "cue": "select"}}),
        board("player-seek-cancelled", "ridge", view={"scrubbing": True, "scrub_target": 2059}, steps=[{"set": {"scrubbing": False}, "wait": 0.5}],
              expect={"state": {"position": "1882", "cue": "back"}}),
        board("player-sheet-arrives", "ridge", steps=[{"set": {"sheet": OPTIONS}, "wait": 0.12}],
              expect={"focus": "sheet/0", "state": {"sheet": "options", "cue": "modal_open"}}),
        board("player-sheet-moves", "ridge", view={"sheet": OPTIONS}, steps=[{"set": {"sheet": {"focus": 1}}, "wait": 0.08}, {"set": {"sheet": {"focus": 2}}, "wait": 0.4}],
              expect={"focus": "sheet/2", "state": {"cue": "focus"}}),
        board("player-sheet-leaves", "ridge", view={"sheet": CAPTIONS}, steps=[{"set": {"sheet": {"open": False}}, "wait": 0.1}],
              expect={"focus": "osd-playing", "state": {"sheet": "", "cue": "modal_close"}}),
        board("player-sheet-reduced", "ridge", reduced_motion=True, steps=[{"set": {"sheet": OPTIONS}, "wait": 0.12}],
              expect={"focus": "sheet/0", "state": {"reduced": "1"}}),
        board("player-up-next-rises", "ridge", steps=[{"set": {"up_next": UP_NEXT}, "wait": 0.14}],
              expect={"focus": "upnext/0", "state": {"cue": "modal_open"}}),
        board("player-up-next-moves", "ridge", view={"up_next": UP_NEXT}, steps=[{"set": {"up_next": {"focus": 1}}, "wait": 0.06}],
              expect={"focus": "upnext/1", "state": {"cue": "focus"}}),
        board("player-chat-opens", "live", steps=[{"set": {"chat": {"side": 2}}, "wait": 0.16}],
              expect={"state": {"chat": "theatre", "cue": "modal_open"}}),
        board("player-chat-new-line", "live", view={"chat": {"side": 2}},
              steps=[{"set": {"chat": {"lines": [{"author": "Dagny", "text": "can hear the water under the floor between songs"},
                                                  {"author": "tape.loop", "text": "this is the good part", "badge": 1},
                                                  {"author": "Halden B.", "text": "one more, one more"}]}}, "wait": 0.1}],
              expect={"state": {"chat_lines": "3"}}),
        board("player-skip-arrives", "ridge", view={"position": 250}, steps=[{"set": {"position": 329, "notice": SKIPPED}, "wait": 0.12}],
              expect={"state": {"notice": "skip", "cue": "notify"}}),
        board("player-skip-leaves", "ridge", view={"position": 329, "notice": SKIPPED}, steps=[{"set": {"notice": {"shown": False}}, "wait": 0.2}],
              expect={"state": {"notice": ""}}),
        board("player-next-arrives", "ridge", view={"position": 2890}, steps=[{"set": {"position": 2892, "next": NEXT}, "wait": 0.2}],
              expect={"focus": "next/play", "state": {"next": "1", "cue": "modal_open"}}),
        board("player-stop-arrives", "ridge", steps=[{"set": {"paused": True, "dialog": {**STOP, "focus": 1}}, "wait": 0.12}, {"set": {"dialog": {"focus": 0}}, "wait": 0.5}],
              expect={"focus": "dialog/0", "dialog": 1, "state": {"cue": "focus"}}),
        board("player-controls-arrive", "ridge", settle=0.06,
              expect={"focus": "osd-playing"}),
        board("player-controls-reduced", "ridge", reduced_motion=True, settle=0.06,
              expect={"focus": "osd-playing", "state": {"reduced": "1"}}),
        board("player-controls-leave", "ridge", steps=[{"set": {"chrome": 0}, "wait": 0.5}],
              expect={"state": {"controls": "hidden"}}),
        board("player-another-video", "ridge", steps=[{"set": {"title": "Aurora Chasing at -30° (We Almost Missed It)", "position": 0, "chapters": [], "segments": [], "buffered": 40, "duration": 1198, "tags": ["4K", "CC"]}, "wait": 0.5}],
              expect={"state": {"position": "0", "chapters": "0", "cue": "launch"}}),
    ]


if __name__ == "__main__":
    write("player.json", "The player's controls in the Afterglow design: boards 16 to 24 of the proposal, as prepared states. Written by tests/afterglow/tools/player_boards.py.", approved())
    write("player-states.json", "The player: every state beside the nine approved boards. Written by tests/afterglow/tools/player_boards.py.", states())
    write("player-app.json", "The player with what the app can feed today (the empty fixture set). Written by tests/afterglow/tools/player_boards.py.", app_forms())
    write("player-long.json", "The player with long and translated texts. Written by tests/afterglow/tools/player_boards.py.", long_texts())
    write("player-motion.json", "The player's transitions, reduced motion and cues. Written by tests/afterglow/tools/player_boards.py.", motion())
    print("wrote", sum(len(f()) for f in (approved, states, app_forms, long_texts, motion)), "boards")
