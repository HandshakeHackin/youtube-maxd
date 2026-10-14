#!/usr/bin/env python3
"""Writes the fixture sets of the library, notifications and grid screens (the d-* sets).

Every name, title and number is invented (the proposal's sample data).

usage: python3 tests/afterglow/tools/fixtures_library.py tests/afterglow/fixtures
"""
import json
import os
import re
import sys

ROOT = sys.argv[1]

CHANNELS = [
    ("Northlight Atlas", "@northlightatlas", "2.41M subscribers"),
    ("Glassfern Games", "@glassfern", "1.08M subscribers"),
    ("Orbitwell", "@orbitwell", "3.9M subscribers"),
    ("Ostrel Kitchen", "@ostrelkitchen", "884K subscribers"),
    ("Nacre Sound", "@nacresound", "412K subscribers"),
    ("Lathe & Lumen", "@latheandlumen", "655K subscribers"),
    ("Tidewrit", "@tidewrit", "1.7M subscribers"),
    ("Quarterline", "@quarterline", "2.2M subscribers"),
    ("Slowreel", "@slowreel", "529K subscribers"),
    ("Mossbyte", "@mossbyte", "301K subscribers"),
    ("Fennwick Hours", "@fennwickhours", "5.3M subscribers"),
    ("Vantor Trails", "@vantortrails", "960K subscribers"),
]

DASH = "—"
# title, channel, seconds (0 = live), views, published, scene, palette, seed, badges
VIDEOS = [
    ("Seven Days Alone on the Ridge Line " + DASH + " A Silent Hiking Film", 0, 2892, "1.2M views", "3 weeks ago", "ridges", 0, 11, ["4k", "cc"]),
    ("I Rebuilt a Whole City in One Weekend (No Mods)", 1, 1601, "842K views", "5 days ago", "city", 1, 23, []),
    ("What Would a Sunset Look Like on Another World?", 2, 847, "3.4M views", "2 months ago", "space", 0, 35, ["4k", "cc"]),
    ("One Pan, Six Dinners: The Lazy Week Plan", 3, 1113, "410K views", "1 day ago", "plate", 0, 47, ["new", "cc"]),
    ("Late Night Synth Session " + DASH + " Live from the Boathouse", 4, 0, "2,184 watching", "", "stage", 0, 59, []),
    ("The Desk Setup I Would Build Again From Scratch", 5, 725, "267K views", "1 week ago", "desk", 0, 61, ["4k"]),
    ("Why Do Tides Arrive Late? A 10-Minute Explainer", 6, 626, "95K views", "4 days ago", "ocean", 1, 73, ["cc"]),
    ("Every Goal From a Wild Final Day", 7, 588, "1.9M views", "16 hours ago", "pitch", 0, 85, ["new"]),
    ("How One Long Take Changes a Whole Scene", 8, 1275, "530K views", "1 month ago", "blobs", 3, 97, ["cc"]),
    ("Writing a Tiny Renderer in an Afternoon", 9, 3764, "148K views", "2 weeks ago", "code", 0, 109, ["cc"]),
    ("Rainy Window, Warm Lamp " + DASH + " 3 Hours of Calm Piano", 10, 10800, "6.1M views", "1 year ago", "blobs", 1, 121, []),
    ("Crossing the Salt Flats by Bicycle", 11, 2000, "720K views", "3 months ago", "dunes", 3, 133, ["4k"]),
    ("Aurora Chasing at -30° (We Almost Missed It)", 0, 1198, "2.2M views", "8 months ago", "aurora", 0, 145, ["4k", "cc"]),
    ("The Smallest Synth I Own Is the One I Use Most", 4, 494, "64K views", "6 days ago", "synth", 0, 157, []),
    ("Speedrunning My Own Level and Regretting It", 1, 902, "1.1M views", "2 weeks ago", "synth", 1, 169, []),
    ("Dune Driving School: Don't Do What I Did", 11, 1476, "388K views", "5 months ago", "dunes", 0, 181, []),
    ("A Map of Every Night Train Still Running", 6, 1000, "1.4M views", "3 weeks ago", "globe", 1, 193, ["cc"]),
    ("The Ring Problem, Explained With a Bowl of Marbles", 2, 712, "905K views", "1 month ago", "space", 1, 205, ["cc"]),
    ("Five Knife Skills That Changed My Cooking", 3, 671, "2.7M views", "1 year ago", "plate", 1, 217, []),
    ("Building a Lamp From One Sheet of Brass", 5, 1689, "312K views", "9 days ago", "desk", 2, 229, []),
    ("Blue Hour on the Northern Coast, No Talking", 0, 2490, "640K views", "2 months ago", "ocean", 1, 241, ["4k"]),
    ("Night Walk Through the Lantern District", 11, 3138, "1.6M views", "4 months ago", "city", 0, 253, ["4k"]),
    ("Tonight's Top Stories in Eight Minutes", 6, 482, "212K views", "3 hours ago", "globe", 2, 265, ["new"]),
    ("Clay Court Classics: The Longest Rally", 7, 807, "455K views", "2 weeks ago", "pitch", 1, 277, []),
    ("Lo-Fi Morning Mix for Slow Coffee", 10, 0, "8,412 watching", "", "blobs", 4, 289, []),
    ("We Mapped a Forest From the Air", 2, 1065, "1.0M views", "4 months ago", "globe", 0, 301, ["cc"]),
    ("The Forest at Night Sounds Nothing Like You Think", 0, 1329, "380K views", "6 weeks ago", "aurora", 2, 313, []),
    ("Hard Court Highlights: Game Seven", 7, 615, "3.1M views", "2 days ago", "pitch", 2, 325, ["new"]),
    ("Building a Pocket Console From Spare Parts", 5, 2090, "1.3M views", "7 months ago", "desk", 1, 337, []),
    ("Why Your Shadows Look Wrong (and the Fix)", 9, 1143, "88K views", "3 days ago", "code", 1, 349, ["new", "cc"]),
    ("Sunrise Set From the Lighthouse Roof", 4, 4360, "196K views", "5 weeks ago", "ocean", 0, 361, []),
    ("Moonrise Over the Dune Sea in Real Time", 11, 372, "540K views", "1 year ago", "dunes", 1, 373, ["4k"]),
    ("A Year on a Tidal Island", 0, 3484, "4.8M views", "2 years ago", "ocean", 3, 385, ["4k", "cc"]),
    ("Dusk Run: Ember Ridge in One Take", 11, 768, "76K views", "2 days ago", "ridges", 3, 397, ["new"]),
    ("Violet Hour: A Short Film", 8, 451, "233K views", "3 weeks ago", "ridges", 6, 409, []),
    ("Teal Dawn Above the Valley", 0, 920, "410K views", "10 months ago", "ridges", 2, 421, ["4k"]),
    ("Neon Rain: Four Hours of City Ambience", 10, 14400, "2.9M views", "6 months ago", "city", 2, 433, []),
    ("Grid Runner " + DASH + " Full Synthwave Set", 4, 3380, "730K views", "3 months ago", "synth", 2, 445, []),
]


CARD = "448x252"   # a card's picture is drawn 394 x 222
SMALL = "256x144"  # a notification's 171 x 96


def channel_id(index):
    slug = re.sub(r"[^a-z0-9]", "", CHANNELS[index][0].lower())
    return ("UC" + slug + "0" * 22)[:24]


def video_id(index):
    slug = re.sub(r"[^a-z0-9]", "", VIDEOS[index][0].lower())
    return (slug[:8] + "%03d" % index)[:11].ljust(11, "x")


def video(index, **extra):
    title, channel, seconds, views, published, scene, palette, seed, badges = VIDEOS[index]
    row = {
        "id": video_id(index),
        "title": title,
        "subtitle": CHANNELS[channel][0],
        "channel_id": channel_id(channel),
        "art_url": "art://%s/%d/%d/%s" % (scene, palette, seed, extra.pop("art", CARD)),
        "duration_sec": seconds,
        "views": views,
        "published": published,
    }
    if badges:
        row["badges"] = badges
    if seconds == 0:
        row["is_live"] = True
    progress = extra.pop("progress", None)
    if progress is not None and seconds > 0:
        row["resume_sec"] = int(seconds * progress)
        if progress >= 1.0:
            row["played"] = True
    row.update(extra)
    return row


def channel(index, **extra):
    row = {"id": "channel:" + channel_id(index), "title": CHANNELS[index][0], "subtitle": CHANNELS[index][2], "is_folder": True,
           "handle": CHANNELS[index][1], "subscribers": CHANNELS[index][2]}
    row.update(extra)
    return row


def file_of(catalog, page=0):
    name = re.sub(r"[^A-Za-z0-9_-]", "_", catalog) if catalog else "root"
    return name + (".p%d" % page if page else "") + ".json"


def write(fixture, catalog, items, more=False, page=0):
    folder = os.path.join(ROOT, fixture, "catalog")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, file_of(catalog, page)), "w", encoding="utf-8", newline="\n") as out:
        out.write("{\n \"items\": [\n")
        out.write(",\n".join("  " + json.dumps(item, ensure_ascii=False) for item in items))
        out.write("\n ],\n \"has_more\": %s\n}\n" % ("true" if more else "false"))


def write_error(fixture, catalog, message, offline=False):
    folder = os.path.join(ROOT, fixture, "catalog")
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, file_of(catalog)), "w", encoding="utf-8", newline="\n") as out:
        document = {"error": message}
        if offline:
            document["offline"] = True
        out.write(json.dumps(document, ensure_ascii=False) + "\n")


def profile(fixture, **values):
    os.makedirs(os.path.join(ROOT, fixture), exist_ok=True)
    with open(os.path.join(ROOT, fixture, "profile.json"), "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps(values, ensure_ascii=False) + "\n")


def notice(title, subtitle=""):
    return {"id": "account:notice", "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}


def acting(row, title, subtitle=""):
    return {"id": row, "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}


# The actions menu of a video as the provider lists it, cut down to what the pages of package D use.
# acts: the acting rows that have an answer in this set; refused: the one the provider refuses.
def menu(fixture, index, acts, scope=7, account=True, refused=None, moves=()):
    vid = video_id(index)
    rows = [acting("local:queue-move:%d:99:%s:%s" % (scope, direction, vid),
                   "Move up" if direction == "up" else "Move down", "Saved for the selected profile on this PS5")
            for direction in moves]
    if account:
        rows += [acting("account:confirm:like:" + vid, "Like video", "YouTube profile: mara"),
                 acting("account:confirm:unrate:" + vid, "Remove video rating", "YouTube profile: mara"),
                 acting("account:confirm:watch_later_remove:" + vid, "Remove from YouTube Watch later", "YouTube profile: mara")]
    rows += [acting("local:video:%d:next:%s" % (scope, vid), "Play next", "Saved for the selected profile on this PS5"),
             acting("local:video:%d:forget:%s" % (scope, vid), "Remove from local history", "Saved for the selected profile on this PS5"),
             acting("local:video:%d:queue:%s" % (scope, vid), "Remove from the play queue", "Saved for the selected profile on this PS5"),
             acting("local:video:%d:favorite:%s" % (scope, vid), "Remove from favorites", "Saved for the selected profile on this PS5")]
    write(fixture, "menu:" + vid, rows)
    for direction in moves:
        write(fixture, "local:queue-move:%d:99:%s:%s" % (scope, direction, vid),
              [notice("Moved up in queue" if direction == "up" else "Moved down in queue", "Circle returns to video actions")])
    said = {"forget": "Removed from local history", "queue": "Removed from the play queue", "favorite": "Removed from favorites"}
    titles = {"watch_later_remove": "Remove from YouTube Watch later", "unrate": "Remove video rating"}
    for act in acts:
        if act in said:
            write(fixture, "local:video:%d:%s:%s" % (scope, act, vid), [notice(said[act], "Circle returns to video actions")])
        else:
            serial = 40 + index
            write(fixture, "account:confirm:%s:%s" % (act, vid), [acting("account:apply:%d" % serial, titles[act], "Confirm for YouTube profile: mara")])
            write(fixture, "account:apply:%d" % serial, [notice("YouTube updated the account", "Circle returns to video actions")])
    if refused:
        write_error(fixture, "local:video:%d:%s:%s" % (scope, refused, vid), "Reopen video actions for the current profile.")


HISTORY = [0, 9, 1, 2, 3, 20, 16, 5, 12, 7, 22, 29, 33, 27, 19, 13, 6, 8, 10, 11, 14, 15, 17, 18]
WATCHED = ["Today, 20:14", "Today, 18:40", "Yesterday", "Yesterday", "Sunday", "Sunday", "Last week", "Last week",
           "Last week", "Last week", "2 weeks ago", "2 weeks ago", "2 weeks ago", "2 weeks ago", "3 weeks ago", "3 weeks ago",
           "3 weeks ago", "Last month", "Last month", "Last month", "Last month", "2 months ago", "2 months ago", "2 months ago"]
LEFT_AT = [0.65, 0.45, 0.3, 0.85, 0.12, 1.0, 1.0, 0.4, 1.0, 1.0, 0.72, 0.2, 1.0, 0.55, 0.08, 1.0, 1.0, 0.33, 0.05, 0.9, 1.0, 0.6, 1.0, 0.25]
WATCH_LATER = [26, 21, 25, 28, 30, 31, 32, 34, 35, 36, 37, 23]
LIKED = [2, 12, 32, 37, 9, 17]
QUEUE = [7, 22, 13]
FAVORITES = [0, 10, 36, 20, 28]
PLAYLIST = [12, 26, 20, 32, 35, 0, 34, 33, 31, 11, 15, 21, 30, 6, 16, 25, 2, 17, 10, 36, 37, 13, 8, 5]
PLAYLISTS = [
    ("PLnorthernlights0001", "Northern Lights, Start to Finish", "Northlight Atlas", 24, "Updated 3 days ago", "aurora", 0, 145),
    ("PLweeknightcooking02", "Weeknight Cooking With One Pan", "Ostrel Kitchen", 18, "Updated last week", "plate", 1, 217),
    ("PLslowmorningmusic03", "Slow Morning Music", "Mara", 42, "Updated yesterday", "blobs", 4, 289),
    ("PLbuildsforrainyday4", "Builds for a Rainy Weekend", "Mara", 9, "Updated 2 months ago", "desk", 1, 337),
    ("PLspaceonthetable005", "Space, Explained at the Kitchen Table", "Orbitwell", 31, "Updated 5 days ago", "space", 1, 205),
]
# who, what happened, the video, when, unread
NOTICES = [
    (7, "uploaded", 7, "16 hours ago", True), (6, "uploaded", 22, "3 hours ago", True), (4, "is live now", 4, "Started 1 h 42 min ago", True),
    (9, "uploaded", 29, "3 days ago", False), (0, "replied to your comment", 0, "4 days ago", False), (11, "uploaded", 33, "2 days ago", False),
    (5, "uploaded", 19, "5 days ago", False), (2, "uploaded", 17, "6 days ago", False), (3, "uploaded", 3, "1 week ago", False),
    (1, "premiered", 14, "1 week ago", False), (10, "is live now", 24, "Started 2 weeks ago", False), (8, "uploaded", 34, "3 weeks ago", False),
]


def history_row(position, **extra):
    return video(HISTORY[position], progress=LEFT_AT[position], watched=WATCHED[position], **extra)


def playlist_row(entry):
    pid, title, owner, count, updated, scene, palette, seed = entry
    return {"id": "library:playlist:" + pid, "title": title, "subtitle": owner, "is_folder": True, "art_url": "art://%s/%d/%d/%s" % (scene, palette, seed, CARD),
            "count": count, "owner": owner, "updated": updated}


def notification(entry):
    who, what, index, when, unread = entry
    row = video(index, art=SMALL)
    row["title"] = "%s %s: %s" % (CHANNELS[who][0], what, VIDEOS[index][0])
    row["subtitle"] = when
    row["channel"] = CHANNELS[who][0]
    row["channel_id"] = channel_id(who)
    row["published"] = when
    row["channel_art_url"] = ""
    row["unread"] = unread
    for dropped in ("views", "badges"):
        row.pop(dropped, None)
    return row


def console_lists(fixture, total=True):
    write(fixture, "history", [history_row(i, **({"total": len(HISTORY)} if total and i == 0 else {})) for i in range(8)], more=True)
    write(fixture, "history", [history_row(i) for i in range(8, 16)], more=True, page=1)
    write(fixture, "history", [history_row(i) for i in range(16, 24)], page=2)
    write(fixture, "queue", [video(i) for i in QUEUE])
    write(fixture, "favorites", [video(i) for i in FAVORITES])
    write(fixture, "subscriptions", [channel(i, subtitle="Followed locally on this PS5") for i in range(12)])


# ---- a signed-in profile -------------------------------------------------------------
S = "d-signed-in"
profile(S, id="mara", name="Mara", guest=False, signed_in=True)
console_lists(S)
write(S, "library:watch_later", [video(i) for i in WATCH_LATER])
write(S, "library:playlists", [playlist_row(entry) for entry in PLAYLISTS])
write(S, "library:playlist:LL", [video(i) for i in LIKED])
write(S, "library:history", [video(i, progress=p) for i, p in ((12, 1.0), (26, 0.5), (3, 1.0), (17, 0.2), (30, 1.0), (21, 0.7), (8, 1.0), (34, 1.0))])
write(S, "library:notifications", [notification(entry) for entry in NOTICES[:6]], more=True)
write(S, "library:notifications", [notification(entry) for entry in NOTICES[6:]], page=1)
menu(S, HISTORY[0], ["forget"])
menu(S, HISTORY[1], [], refused="forget")
menu(S, WATCH_LATER[0], ["watch_later_remove"])
menu(S, LIKED[0], ["unrate"])
menu(S, QUEUE[0], ["queue"], moves=("down",))
menu(S, QUEUE[1], [], moves=("up", "down"))
menu(S, QUEUE[2], [], moves=("up",))
menu(S, FAVORITES[1], ["favorite"])
# A playlist: the list itself first, then its videos in order, eight to a page.
head = playlist_row(PLAYLISTS[0])
write(S, "library:playlist:PLnorthernlights0001", [head] + [video(i) for i in PLAYLIST[:8]], more=True)
write(S, "library:playlist:PLnorthernlights0001", [video(i) for i in PLAYLIST[8:16]], more=True, page=1)
write(S, "library:playlist:PLnorthernlights0001", [video(i) for i in PLAYLIST[16:]], page=2)
# A section with sections, a playlist and a channel among its videos.
write(S, "section:gaming", [
    {"id": "section:gaming:live", "title": "Live now", "subtitle": "12 streams", "is_folder": True, "art_url": "art://stage/0/59/" + CARD},
    {"id": "section:gaming:trending", "title": "Trending in gaming", "subtitle": "Updated today", "is_folder": True, "art_url": "art://synth/1/169/" + CARD},
    {"id": "section:gaming:builders", "title": "Builders and long projects", "subtitle": "", "is_folder": True},
    video(1), video(14),
    channel(1, subscribed=True),
    {"id": "playlist:PLglassfernbuild006", "title": "Whole Cities, One Weekend at a Time", "subtitle": "Glassfern Games", "is_folder": True,
     "art_url": "art://city/1/23/" + CARD, "count": 12, "owner": "Glassfern Games"},
    video(28), video(9), video(29), video(37), video(13),
])
write(S, "section:gaming:live", [video(4), video(24)])
# Recommendations with the filters YouTube offers above them.
CHIPS = [("All", "a"), ("Music", "b"), ("Gaming", "c"), ("Live", "d"), ("Recently uploaded", "e"), ("Watched", "f"), ("New to you", "g")]


def chips(selected):
    return [{"id": "library:filter:" + (letter * 36), "title": label, "subtitle": "", "is_folder": True, "chip": "filter", "selected": letter == selected}
            for label, letter in CHIPS]


write(S, "library:home", chips("a") + [video(i) for i in (0, 12, 2, 7, 22, 3, 9, 16, 20, 5, 27, 33)], more=True)
write(S, "library:home", [video(i) for i in (1, 6, 8, 10, 11, 13, 14, 15)], page=1)
write(S, "library:filter:" + "c" * 36, chips("c") + [video(i) for i in (1, 14, 28, 9, 29)])

# ---- Guest ---------------------------------------------------------------------------
G = "d-guest"
profile(G, id="guest", name="", guest=True, signed_in=False)
write(G, "history", [history_row(i) for i in (0, 2, 4)])
write(G, "queue", [])
write(G, "favorites", [video(i) for i in (10, 36)])
write(G, "subscriptions", [channel(i, subtitle="Followed locally on this PS5") for i in (0, 6, 9)])
write(G, "section:trending", [video(i) for i in (7, 2, 14, 16, 32, 27, 22, 3)])
write(G, "section:shorts", [notice("This section is not available from YouTube for this profile", "YouTube offers Shorts to signed-in accounts in this region only.")])
write(G, "library:watch_later", [notice("Choose an account profile first", "Discover > Accounts & library > Profile")])
menu(G, HISTORY[0], ["forget"], scope=3, account=False)

# ---- nothing kept yet ----------------------------------------------------------------
E = "d-empty"
profile(E, id="mara", name="Mara", guest=False, signed_in=True)
for catalog in ("history", "queue", "favorites", "subscriptions", "library:watch_later", "library:playlists", "library:playlist:LL",
                "library:notifications", "library:playlist:PLslowmorningmusic03", "section:gaming"):
    write(E, catalog, [])

# ---- long texts ----------------------------------------------------------------------
L = "d-long-texts"
profile(L, id="long", name="Maximiliane-Friederike von Hohenstaufen-Wittelsbach zu Schaumburg-Lippe", guest=False, signed_in=True)
LONG_TITLES = [
    "Donaudampfschifffahrtsgesellschaftskapitänsmützenabzeichenhersteller: warum es dieses Wort wirklich gibt",
    "Sieben Tage allein auf dem Grat " + DASH + " ein stiller Wanderfilm in voller Länge, ohne Musik und ohne Kommentar",
    "Rechtsschutzversicherungsgesellschaften verständlich erklärt: ein außergewöhnlich langer Titel zum Prüfen der Zeilenumbrüche",
    "Ich habe an einem einzigen verregneten Wochenende eine vollständige Großstadt neu aufgebaut (ohne Modifikationen)",
    "Wie sähe ein Sonnenuntergang auf einer anderen Welt aus? Eine ausführliche Betrachtung mit Kreide und Küchentisch",
    "Eine Pfanne, sechs Abendessen: der bequeme Wochenplan für vielbeschäftigte Menschen mit kleinem Kühlschrank",
    "Kraftfahrzeughaftpflichtversicherung",
    "Kurz",
]
LONG_CHANNELS = [
    "Nordlicht-Atlas Dokumentarfilmgesellschaft für besonders ausführliche Naturaufnahmen",
    "Glasfarn Spieleentwicklungsgemeinschaft",
    "Umlaufbahnbrunnen Wissenschaftsvermittlung und Sternbeobachtung e. V.",
    "K",
]
LONG_VIEWS = ["1.234.567.890 Aufrufe", "987.654.321 Aufrufe", "12,3 Mrd. Aufrufe", "3 Aufrufe"]
LONG_AGES = ["vor 11 Monaten und 3 Wochen", "vor mehr als 12 Jahren", "vor 1 Tag", "gerade eben"]
LONG_WATCHED = ["Vorgestern um 23:59 Uhr", "Am vergangenen Donnerstagnachmittag", "Gestern", "Heute, 20:14 Uhr"]


def long_video(position, **extra):
    row = video(HISTORY[position], **extra)
    row["title"] = LONG_TITLES[position % len(LONG_TITLES)]
    row["subtitle"] = LONG_CHANNELS[position % len(LONG_CHANNELS)]
    row["views"] = LONG_VIEWS[position % len(LONG_VIEWS)]
    row["published"] = LONG_AGES[position % len(LONG_AGES)]
    if row["duration_sec"]:
        row["duration_sec"] = 359999 if position % 3 == 0 else row["duration_sec"]
    return row


write(L, "history", [long_video(i, progress=LEFT_AT[i], watched=LONG_WATCHED[i % 4], **({"total": 1234567} if i == 0 else {})) for i in range(8)], more=True)
write(L, "history", [long_video(i, progress=LEFT_AT[i], watched=LONG_WATCHED[i % 4]) for i in range(8, 16)], page=1)
write(L, "queue", [long_video(i) for i in range(3)])
write(L, "favorites", [long_video(i) for i in range(5)])
write(L, "library:watch_later", [long_video(i, **({"total": 123} if i == 0 else {})) for i in range(12)], more=True)
write(L, "library:playlist:LL", [long_video(i) for i in range(6)])
write(L, "library:playlists", [
    {"id": "library:playlist:PLlangertitel%07d" % i, "title": LONG_TITLES[i], "subtitle": LONG_CHANNELS[i % 4], "is_folder": True,
     "art_url": "art://%s/%d/%d/%s" % (VIDEOS[HISTORY[i]][5], VIDEOS[HISTORY[i]][6], VIDEOS[HISTORY[i]][7], CARD),
     "count": [1, 1234567, 5000, 24, 18, 7][i], "owner": LONG_CHANNELS[i % 4], "updated": LONG_AGES[i % 4]} for i in range(6)])
long_notices = []
for i, entry in enumerate(NOTICES[:6]):
    row = notification(entry)
    who = LONG_CHANNELS[i % 4]
    row["title"] = "%s hat ein neues Video hochgeladen: %s" % (who, LONG_TITLES[i])
    row["channel"] = who
    row["published"] = LONG_AGES[i % 4]
    row["subtitle"] = row["published"]
    long_notices.append(row)
write(L, "library:notifications", long_notices)
long_head = {"id": "library:playlist:PLlangertitel0000001", "title": LONG_TITLES[1], "subtitle": LONG_CHANNELS[0], "is_folder": True,
             "count": 1234567, "owner": LONG_CHANNELS[0], "updated": LONG_AGES[0]}
write(L, "library:playlist:PLlangertitel0000001", [long_head,
      {"id": "section:langer:abschnitt", "title": "Besonders ausführliche Zusammenstellungen für lange Winterabende", "subtitle": "1.234 Videos in dieser Zusammenstellung", "is_folder": True},
      {"id": "channel:" + channel_id(0), "title": LONG_CHANNELS[0], "subtitle": "123.456.789 Abonnenten", "is_folder": True, "handle": "@nordlichtatlasdokumentarfilmgesellschaft", "subscribers": "123.456.789 Abonnenten", "subscribed": True},
      {"id": "playlist:PLlangertitel0000002", "title": LONG_TITLES[2], "subtitle": LONG_CHANNELS[2], "is_folder": True, "art_url": "art://aurora/0/145/" + CARD, "count": 1234567, "owner": LONG_CHANNELS[2]}]
      + [long_video(i) for i in range(8)])

# ---- YouTube cannot be reached; what the console keeps still answers -------------------
O = "d-offline"
profile(O, id="mara", name="Mara", guest=False, signed_in=True)
console_lists(O)
for catalog in ("library:watch_later", "library:playlist:LL", "library:notifications", "library:playlist:PLnorthernlights0001"):
    write_error(O, catalog, "The YouTube service cannot be reached.", offline=True)
write_error(O, "library:playlists", "YouTube answered with an error (503). Try again in a moment.")
write_error(O, "section:gaming", "YouTube answered with an error (503). Try again in a moment.")
# A list whose first page arrives and whose second is refused.
write(O, "section:trending", [video(i) for i in (7, 2, 14, 16, 32, 27, 22, 3)], more=True)
folder = os.path.join(ROOT, O, "catalog")
with open(os.path.join(folder, file_of("section:trending", 1)), "w", encoding="utf-8", newline=chr(10)) as out:
    out.write(json.dumps({"error": "YouTube did not send the rest of this list. Press Down to ask again."}) + chr(10))

# ---- YouTube answers with an error ---------------------------------------------------
F = "d-failed"
profile(F, id="mara", name="Mara", guest=False, signed_in=True)
write_error(F, "library:notifications", "YouTube answered with an error (503). Try again in a moment.")

# ---- notifications as the service words them today: one sentence and a time, no channel ----
T = "d-service-today"
profile(T, id="mara", name="Mara", guest=False, signed_in=True)
plain = []
for entry in NOTICES[:5]:
    row = notification(entry)
    for dropped in ("channel", "published", "channel_art_url", "channel_id"):
        row.pop(dropped, None)
    plain.append(row)
write(T, "library:notifications", plain)
for fixture, count in ((S, 13), (E, 0), (L, 1234), (T, 9)):
    write(fixture, "library:notification-count", [{"id": "library:notification-count", "title": "Notifications", "is_folder": True, "unseen_count": str(count)}])
write_error(O, "library:notification-count", "The YouTube service cannot be reached.", offline=True)
write(F, "library:notification-count", [{"id": "library:notification-count", "is_folder": True, "unseen_count": "unknown"}])
print("fixtures written")
for i in (HISTORY[0], HISTORY[1], HISTORY[2], WATCH_LATER[0], LIKED[0], QUEUE[0], FAVORITES[0], FAVORITES[1], PLAYLIST[0], 7, 22, 4, 1):
    print(i, video_id(i), VIDEOS[i][0][:40])
print(channel_id(0), channel_id(1), channel_id(7), channel_id(6))
