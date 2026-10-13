#!/usr/bin/env python3
"""Writes the fixture catalogs of the search and results screens.

Every channel, title and number is the interface proposal's invented sample
content. Only the files named here are written; a set's profile.json is left
alone when it exists.

usage: python3 tests/afterglow/tools/fixtures_search.py tests/afterglow/fixtures
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
DOT = " · "
# A card is 394 pixels wide: the harness paints nothing larger than it shows,
# which keeps a board with a dozen pictures quick.
ART_SIZE = "/400x225"


def fnv(text):
    value = 2166136261
    for byte in text.encode():
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def channel_id(slug, pair):
    """An invented channel id whose avatar takes the proposal's colours (design.cpp, tint_of)."""
    for n in range(1000):
        candidate = "UC%s%03d" % (slug, n)
        if fnv(candidate) % 12 == pair:
            return candidate
    raise SystemExit("no id for " + slug)


CHANNELS = {}
for index, (name, handle, subscribers) in enumerate([
        ("Northlight Atlas", "@northlightatlas", "2.41M"), ("Glassfern Games", "@glassfern", "1.08M"),
        ("Orbitwell", "@orbitwell", "3.9M"), ("Ostrel Kitchen", "@ostrelkitchen", "884K"),
        ("Nacre Sound", "@nacresound", "412K"), ("Lathe & Lumen", "@latheandlumen", "655K"),
        ("Tidewrit", "@tidewrit", "1.7M"), ("Quarterline", "@quarterline", "2.2M"),
        ("Slowreel", "@slowreel", "529K"), ("Mossbyte", "@mossbyte", "301K"),
        ("Fennwick Hours", "@fennwickhours", "5.3M"), ("Vantor Trails", "@vantortrails", "960K")]):
    CHANNELS[index] = {"id": channel_id(handle[1:], index), "name": name, "handle": handle, "subscribers": subscribers}


def seconds(length):
    total = 0
    for part in length.split(":"):
        total = total * 60 + int(part)
    return total


# key: (id, title, channel, length, views, published, art, badges, live)
VIDEOS = {
    0: ("ridgeLine7d", "Seven Days Alone on the Ridge Line — A Silent Hiking Film", 0, "48:12", "1.2M views", "3 weeks ago", "ridges/0/11", ["4k", "cc"]),
    1: ("cityWeekend", "I Rebuilt a Whole City in One Weekend (No Mods)", 1, "26:41", "842K views", "5 days ago", "city/1/23", []),
    2: ("sunsetWorld", "What Would a Sunset Look Like on Another World?", 2, "14:07", "3.4M views", "2 months ago", "space/0/35", ["4k", "cc"]),
    3: ("onePanSixDn", "One Pan, Six Dinners: The Lazy Week Plan", 3, "18:33", "410K views", "1 day ago", "plate/0/47", ["new", "cc"]),
    4: ("synthBoatLv", "Late Night Synth Session — Live from the Boathouse", 4, "", "2,184 watching", "", "stage/0/59", []),
    5: ("deskSetupSc", "The Desk Setup I Would Build Again From Scratch", 5, "12:05", "267K views", "1 week ago", "desk/0/61", ["4k"]),
    9: ("tinyRenderr", "Writing a Tiny Renderer in an Afternoon", 9, "1:02:44", "148K views", "2 weeks ago", "code/0/109", ["cc"]),
    10: ("rainyWindow", "Rainy Window, Warm Lamp — 3 Hours of Calm Piano", 10, "3:00:00", "6.1M views", "1 year ago", "blobs/1/121", []),
    11: ("saltFlatsBk", "Crossing the Salt Flats by Bicycle", 11, "33:20", "720K views", "3 months ago", "dunes/3/133", ["4k"]),
    12: ("auroraChs30", "Aurora Chasing at -30° (We Almost Missed It)", 0, "19:58", "2.2M views", "8 months ago", "aurora/0/145", ["4k", "cc"]),
    16: ("nightTrains", "A Map of Every Night Train Still Running", 6, "16:40", "1.4M views", "3 weeks ago", "globe/1/193", ["cc"]),
    20: ("blueHourNth", "Blue Hour on the Northern Coast, No Talking", 0, "41:30", "640K views", "2 months ago", "ocean/1/241", ["4k"]),
    21: ("lanternWalk", "Night Walk Through the Lantern District", 11, "52:18", "1.6M views", "4 months ago", "city/0/253", ["4k"]),
    24: ("lofiMorning", "Lo-Fi Morning Mix for Slow Coffee", 10, "", "8,412 watching", "", "blobs/4/289", []),
    25: ("forestAirMp", "We Mapped a Forest From the Air", 2, "17:45", "1.0M views", "4 months ago", "globe/0/301", ["cc"]),
    26: ("forestNight", "The Forest at Night Sounds Nothing Like You Think", 0, "22:09", "380K views", "6 weeks ago", "aurora/2/313", []),
    30: ("lighthouseS", "Sunrise Set From the Lighthouse Roof", 4, "1:12:40", "196K views", "5 weeks ago", "ocean/0/361", []),
    31: ("moonDuneSea", "Moonrise Over the Dune Sea in Real Time", 11, "6:12", "540K views", "1 year ago", "dunes/1/373", ["4k"]),
    32: ("tidalIsland", "A Year on a Tidal Island", 0, "58:04", "4.8M views", "2 years ago", "ocean/3/385", ["4k", "cc"]),
    33: ("duskEmberRd", "Dusk Run: Ember Ridge in One Take", 11, "12:48", "76K views", "2 days ago", "ridges/3/397", ["new"]),
    34: ("violetHourF", "Violet Hour: A Short Film", 8, "7:31", "233K views", "3 weeks ago", "ridges/6/409", []),
    35: ("tealDawnVly", "Teal Dawn Above the Valley", 0, "15:20", "410K views", "10 months ago", "ridges/2/421", ["4k"]),
    36: ("neonRain4hr", "Neon Rain: Four Hours of City Ambience", 10, "4:00:00", "2.9M views", "6 months ago", "city/2/433", []),
    37: ("gridRunnerS", "Grid Runner — Full Synthwave Set", 4, "56:20", "730K views", "3 months ago", "synth/2/445", []),
}


def video(key, **more):
    vid, title, channel, length, views, published, art, badges = VIDEOS[key]
    row = {"id": vid, "title": title, "subtitle": CHANNELS[channel]["name"], "channel_id": CHANNELS[channel]["id"],
           "art_url": "art://" + art + ART_SIZE, "views": views}
    if length:
        row["duration_sec"] = seconds(length)
        row["published"] = published
    else:
        row["is_live"] = True
    if badges:
        row["badges"] = badges
    row.update(more)
    return row


def channel(key, subscribed=None, **more):
    c = CHANNELS[key]
    row = {"id": "channel:" + c["id"], "title": c["name"], "subtitle": c["subscribers"] + " subscribers", "is_folder": True,
           "handle": c["handle"], "subscribers": c["subscribers"] + " subscribers"}
    if subscribed is not None:
        row["subscribed"] = subscribed
    row.update(more)
    return row


def playlist(pid, title, owner, art, count):
    return {"id": "playlist:" + pid, "title": title, "subtitle": CHANNELS[owner]["name"], "art_url": "art://" + art + ART_SIZE, "is_folder": True,
            "count": count, "owner": CHANNELS[owner]["name"]}


def lead(results, summary="No filter is set"):
    """The two rows the provider puts in front of a typed search."""
    first = {"id": "settings:search", "title": "Search options", "subtitle": summary, "is_folder": True, "interface": True}
    if results:
        first["results"] = results
    return [first, {"id": "search-suggest", "title": "Suggestions", "subtitle": "Completions for this search", "is_folder": True, "interface": True}]


def notice(title, subtitle, **more):
    row = {"id": "account:notice", "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}
    row.update(more)
    return row


def history(queries, forgetting=False):
    rows = []
    if not forgetting:
        rows.append({"id": "search-history:remove", "title": "Remove a recent search", "subtitle": "Choose one query to forget", "is_folder": True, "interface": True})
        rows.append({"id": "search-history:clear:1", "title": "Clear recent searches", "subtitle": "Forget every query saved for the selected profile", "is_folder": True, "interface": True})
    for serial, query in queries:
        rows.append({"id": "search-history:%s:%d" % ("forget" if forgetting else "run", serial), "title": query,
                     "subtitle": "Cross forgets this query" if forgetting else "Cross searches again", "is_folder": True})
    return rows


def suggestions(rows):
    return [dict({"id": "search-suggest:run:1:%d" % index, "title": text, "subtitle": "Cross searches for this", "is_folder": True},
                 **({"detail": detail} if detail else {})) for index, (text, detail) in enumerate(rows)]


def name_of(catalog, page=0):
    name = "".join(c if (c.isascii() and (c.isalnum() or c in "-_")) else "_" for c in (catalog or "root"))
    # A character outside ASCII is several bytes, and the harness writes one "_" per byte.
    name = "".join("_" * len(c.encode()) if not c.isascii() else c for c in name) if not name.isascii() else name
    return name + (".p%d" % page if page else "") + ".json"


def file_name(catalog, page=0):
    out = []
    for byte in (catalog or "root").encode():
        c = chr(byte)
        out.append(c if (byte < 128 and (c.isalnum() or c in "-_")) else "_")
    return "".join(out) + (".p%d" % page if page else "") + ".json"


WRITTEN = []


def write(fixture, catalog, items=None, more=False, page=0, raw=None):
    folder = ROOT / fixture / "catalog"
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / file_name(catalog, page)
    if raw is None:
        lines = ",\n".join("  " + json.dumps(item, ensure_ascii=False) for item in items)
        text = "{\n \"items\": [\n%s\n ],\n \"has_more\": %s\n}\n" % (lines, "true" if more else "false") if items else \
            "{\n \"items\": [],\n \"has_more\": false\n}\n"
    else:
        text = json.dumps(raw, ensure_ascii=False) + "\n"
    path.write_text(text, encoding="utf-8", newline="\n")
    WRITTEN.append(str(path.relative_to(ROOT)))


def profile(fixture, value):
    folder = ROOT / fixture
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / "profile.json"
    if not path.exists():
        path.write_text(json.dumps(value) + "\n", encoding="utf-8", newline="\n")
        WRITTEN.append(str(path.relative_to(ROOT)))


AURORA_SUGGESTIONS = [("aurora chasing at -30", "Northlight Atlas"), ("aurora borealis live tonight", "Live"), ("aurora timelapse 4k", ""),
                      ("aurora forecast explained", "Orbitwell"), ("aurora sounds for sleep", "3 hours"), ("auroral arcs from orbit", "")]
RECENT = [(7, "aurora chasing"), (6, "one pan dinners"), (5, "tiny renderer"), (4, "night train map"), (3, "synth session live"),
          (2, "lighthouse sunrise set")]
POPULAR = [12, 26, 20, 32, 31, 35, 0, 2, 21, 36, 9, 16]
MATCHES = [12, 26, 20, 32, 31, 35, 0, 33]
PLAYLIST = playlist("PLnorthernLights0024", "Northern Lights, Start to Finish", 0, "aurora/0/145", 24)
FIRST_PAGE = [channel(0, True), video(12), video(26), video(20), PLAYLIST, video(32), video(31), video(35),
              video(0, resume_sec=1880), video(34), video(33), video(21)]
SECOND_PAGE = [video(11), video(30), channel(11, False), video(36), video(2), video(10), video(25), video(37)]

# ---- signed in: Mara, the profile of the proposal's boards ----------------------------------------
profile("signed-in", {"id": "mara", "name": "Mara", "guest": False, "signed_in": True})
write("signed-in", "search-history", history(RECENT))
write("signed-in", "search-history:forget:7", history(RECENT[1:], forgetting=True))
write("signed-in", "search-history:clear:1", [notice("No recent searches", "Square searches; queries are saved for the selected profile", state="empty")])
write("signed-in", "section:trending", [video(key) for key in POPULAR])
for typed in ("a", "au", "aur", "aur-", "auro", "auror", "aurora"):
    write("signed-in", "search-suggest:" + typed, suggestions(AURORA_SUGGESTIONS))
    write("signed-in", "search-preview:" + typed, [video(key) for key in MATCHES])
write("signed-in", "search-suggest:Q", suggestions([("quiet hiking films without narration", "Northlight Atlas"), ("quick weeknight dinners", "Ostrel Kitchen"),
                                                     ("quarter final highlights", "Quarterline")]))
write("signed-in", "search-preview:Q", [video(0), video(3), video(26), video(20)])
write("signed-in", "search-suggest:zzzz", [notice("No suggestions", "Circle returns to the search")])
write("signed-in", "search-preview:zzzz", [])
write("signed-in", "search:aurora", lead("1,240") + FIRST_PAGE, more=True)
write("signed-in", "search:aurora", SECOND_PAGE, page=1)
write("signed-in", "search-with:features=4k:aurora", lead("1,240", "4K") + FIRST_PAGE[:8])
write("signed-in", "search-with:date=month:aurora", lead("38", "This month") + [video(33), video(34), video(12), channel(0, True), video(26)])
write("signed-in", "search-with:date=month,features=4k:aurora", lead("12", "This month, 4K") + [video(12), video(35), video(20)])
write("signed-in", "search-with:order=view_count:aurora", lead("1,240", "View count") + [video(32), video(12), video(20), video(31), video(35), video(26)])
write("signed-in", "search:aurora chasing at -30", lead("86") + [video(12), channel(0, True), video(26), video(35), video(20)])
write("signed-in", "search:aurora chasing", lead("412") + [video(12), video(26), channel(0, True), video(35)])
write("signed-in", "search:zzzz", lead(""))
write("signed-in", "search-with:features=live:zzzz", lead("", "Live"))
write("signed-in", "search:broken", raw={"error": "YouTube did not answer this search. Try again in a moment."})

# ---- Guest: nothing is subscribed, nothing was watched ---------------------------------------------
write("guest", "search-history", [notice("No recent searches", "Square searches; queries are saved for the selected profile", state="empty")])
# guest/section:trending is Home's (its Trending shelf); the search screen shows the same list as "Popular right now".
write("guest", "search-suggest:aurora", suggestions(AURORA_SUGGESTIONS))
write("guest", "search-preview:aurora", [video(key) for key in MATCHES])
write("guest", "search:aurora", lead("1,240") + [channel(0), video(12), video(26), video(20), PLAYLIST, video(32), video(31), video(35)])

# ---- empty: a profile with search history switched off and a service that finds nothing --------------
write("empty", "search-history", [notice("Search history is off", "Turn it on in YouTube preferences for this profile")])
write("empty", "section:trending", [])
write("empty", "search-suggest:aurora", [notice("No suggestions", "Circle returns to the search")])
write("empty", "search-preview:aurora", [])
write("empty", "search:aurora", lead(""))
write("empty", "search-with:features=4k:aurora", lead("", "4K"))

# ---- long texts: German-length titles, long names, large numbers -------------------------------------
# Typed on the on-screen keyboard, so without umlauts; it is also a fixture file's name.
LONG_QUERY = "polarlichtjagd bei minus dreissig grad in nordnorwegen die komplette dokumentation in voller laenge"
LONG_CHANNEL = {"id": channel_id("nordlichtatlasdokumentarfilme", 0), "name": "Nordlicht-Atlas Dokumentarfilmgesellschaft für Hochgebirgs- und Polarregionen",
                "handle": "@nordlichtatlasdokumentarfilmgesellschaft", "subscribers": "12.345.678"}


def long_video(vid, title, length, views, published, art, badges, channel_name=LONG_CHANNEL["name"]):
    row = {"id": vid, "title": title, "subtitle": channel_name, "channel_id": LONG_CHANNEL["id"], "art_url": "art://" + art + ART_SIZE,
           "duration_sec": seconds(length), "views": views, "published": published}
    if badges:
        row["badges"] = badges
    return row


LONG_VIDEOS = [
    long_video("langTitel01", "Polarlichtjagd bei minus dreißig Grad: Wie wir das Naturschauspiel in Nordnorwegen beinahe vollständig verpasst hätten", "1:19:58",
               "1.234.567.890 Aufrufe", "vor 11 Monaten", "aurora/0/145", ["4k", "cc"]),
    long_video("langTitel02", "Hochgeschwindigkeitszugverbindungsfahrplanänderungsbenachrichtigung erklärt", "10:22:09",
               "987.654.321 Aufrufe", "vor 6 Wochen", "aurora/2/313", ["new", "4k"]),
    long_video("langTitel03", "Blaue Stunde an der nördlichen Küste — völlig ohne Kommentar, nur Wind, Wellen und Möwen", "41:30",
               "640.000 Aufrufe", "vor 2 Monaten", "ocean/1/241", ["4k"]),
    long_video("langTitel04", "Ein Jahr auf einer Gezeiteninsel", "58:04", "4,8 Mio. Aufrufe", "vor 2 Jahren", "ocean/3/385", ["4k", "cc"], "Tidewrit"),
    long_video("langTitel05", "Mondaufgang über dem Dünenmeer in Echtzeit: die ungekürzte Fassung mit Originalton und Kapitelmarken", "6:12:00",
               "540.000 Aufrufe", "vor 1 Jahr", "dunes/1/373", ["4k"]),
    long_video("langTitel06", "Türkisfarbene Morgendämmerung über dem Tal", "15:20", "410.000 Aufrufe", "vor 10 Monaten", "ridges/2/421", ["4k"]),
]
LONG_CHANNEL_ROW = {"id": "channel:" + LONG_CHANNEL["id"], "title": LONG_CHANNEL["name"], "subtitle": LONG_CHANNEL["subscribers"] + " Abonnenten",
                    "is_folder": True, "handle": LONG_CHANNEL["handle"], "subscribers": LONG_CHANNEL["subscribers"] + " Abonnenten", "subscribed": True}
LONG_PLAYLIST = {"id": "playlist:PLnordlichterKomplett", "title": "Nordlichter von Anfang bis Ende: sämtliche Expeditionen der vergangenen zwölf Winter in chronologischer Reihenfolge",
                 "subtitle": LONG_CHANNEL["name"], "art_url": "art://aurora/0/145" + ART_SIZE, "is_folder": True, "count": 1248, "owner": LONG_CHANNEL["name"]}
LONG_SUGGESTIONS = [
    ("polarlichtjagd bei minus dreissig grad in nordnorwegen die komplette dokumentation", LONG_CHANNEL["name"]),
    ("polarlichtvorhersage für die kommenden vierzehn tage ausführlich erklärt", "Orbitwell Wissenschaftskanal"),
    ("polarlicht zeitraffer in 4k", ""),
    ("polarlichter live heute nacht aus dem hochgebirge", "Live"),
    ("polarlichtgeräusche zum einschlafen", "12 Stunden 30 Minuten"),
]
write("long-texts", "search-history", history([
    (9, LONG_QUERY), (8, "hochgeschwindigkeitszugverbindungsfahrplanänderungsbenachrichtigung"), (7, "eine pfanne sechs abendessen der bequeme wochenplan für die ganze familie"),
    (6, "aurora"), (5, "winziger renderer an einem nachmittag geschrieben ohne bibliotheken")]))
write("long-texts", "section:trending", LONG_VIDEOS)
for typed in ("polarlicht", LONG_QUERY):
    write("long-texts", "search-suggest:" + typed, suggestions(LONG_SUGGESTIONS))
    write("long-texts", "search-preview:" + typed, LONG_VIDEOS)
write("long-texts", "search:" + LONG_QUERY, lead("1.234.567.890") + [LONG_CHANNEL_ROW] + LONG_VIDEOS[:3] + [LONG_PLAYLIST] + LONG_VIDEOS[3:])

for name in WRITTEN:
    print(name)
