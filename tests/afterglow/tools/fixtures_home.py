#!/usr/bin/env python3
"""Writes the fixture catalogs of a Guest's Home that are videos.

What YouTube recommends to a session without an account (the provider's notice
and its leads, no video: what a new Guest gets) and the topics' own shelves
("home-topic:<topic>"). Every channel, title and number is invented sample
content, in the style of the sets' other Home catalogs. Only the files named
here are written; the Guest's other Home catalogs (discover, history,
section:trending) have no generator.

usage: python3 tests/afterglow/tools/fixtures_home.py tests/afterglow/fixtures
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])
OVERVIEW = "An invented description for the pictures of the interface."
CHANNELS = {1: "Glassfern Games", 4: "Nacre Sound", 6: "Tidewrit", 7: "Quarterline", 9: "Mossbyte", 10: "Fennwick Hours"}

# number: (title, channel, seconds or 0 for live, views, published, art, badges)
VIDEOS = {
    1: ("I Rebuilt a Whole City in One Weekend (No Mods)", 1, 1601, "842K views", "5 days ago", "city/1/23", []),
    4: ("Late Night Synth Session — Live from the Boathouse", 4, 0, "2,184 watching", "", "stage/0/59", []),
    7: ("Every Goal From a Wild Final Day", 7, 588, "1.9M views", "16 hours ago", "pitch/0/85", ["new"]),
    10: ("Rainy Window, Warm Lamp — 3 Hours of Calm Piano", 10, 10800, "6.1M views", "1 year ago", "blobs/1/121", []),
    14: ("Speedrunning My Own Level and Regretting It", 1, 902, "1.1M views", "2 weeks ago", "synth/1/169", []),
    16: ("A Map of Every Night Train Still Running", 6, 1000, "1.4M views", "3 weeks ago", "globe/1/193", ["cc"]),
    27: ("Hard Court Highlights: Game Seven", 7, 734, "2.6M views", "1 day ago", "pitch/2/325", ["new"]),
    40: ("Grid Runner — Full Synthwave Set", 4, 3380, "730K views", "3 months ago", "synth/2/445", []),
    41: ("Lo-Fi Morning Mix for Slow Coffee", 10, 3850, "2.3M views", "7 months ago", "blobs/4/289", []),
    42: ("Sunrise Set From the Lighthouse Roof", 4, 4360, "196K views", "5 weeks ago", "ocean/0/361", ["4k"]),
    43: ("Neon Rain: Four Hours of City Ambience", 10, 14400, "2.9M views", "6 months ago", "city/2/433", []),
    44: ("The Boss Nobody Was Meant to Beat", 1, 1284, "3.1M views", "4 days ago", "synth/0/457", ["new"]),
    45: ("Every Secret Room in the First World", 9, 1745, "512K views", "1 month ago", "code/1/469", ["cc"]),
    46: ("A Cosy Farm, Forty Hours Later", 1, 2210, "688K views", "3 weeks ago", "city/3/481", []),
    47: ("The Week in Six Maps", 6, 642, "310K views", "2 days ago", "globe/2/493", ["new", "cc"]),
    48: ("How a Harbour Town Voted to Keep Its Ferry", 6, 1126, "94K views", "6 days ago", "ocean/2/505", ["cc"]),
    49: ("Why the Night Trains Are Coming Back", 6, 815, "1.2M views", "1 month ago", "globe/3/517", ["cc"]),
    50: ("The Longest Rally of the Season", 7, 367, "4.4M views", "3 days ago", "pitch/1/529", []),
    51: ("Final Lap, Three Abreast", 7, 498, "870K views", "1 week ago", "pitch/3/541", ["4k"]),
}
SHELVES = {
    "music": (4, 10, 40, 41, 42, 43),
    "gaming": (1, 14, 44, 45, 46),
    "news": (16, 47, 48, 49),
    "sports": (7, 27, 50, 51),
    "live": (4, 1),
}


def video(number):
    title, channel, length, views, published, art, badges = VIDEOS[number]
    row = {"id": "vid%02dxxxxxx" % number, "title": title, "subtitle": CHANNELS[channel],
           "channel_id": "UCfixture%015d" % channel, "art_url": "art://" + art, "views": views}
    if length:
        row["published"] = published
        row["duration_sec"] = length
    else:
        row["is_live"] = True
    if badges:
        row["badges"] = badges
    row["overview"] = OVERVIEW
    return row


def notice(row, title, subtitle):
    return {"id": row, "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}


def write(name, catalog, items):
    folder = ROOT / name / "catalog"
    folder.mkdir(parents=True, exist_ok=True)
    file = "".join(c if c.isalnum() or c in "-_" else "_" for c in catalog) + ".json"
    with open(folder / file, "w", encoding="utf-8", newline="\n") as out:
        json.dump({"items": items, "has_more": False}, out, ensure_ascii=False, indent=1)
        out.write("\n")


# What the provider lists for a Guest's "library:home": YouTube answers a session without an account with a notice alone.
write("guest", "library:home", [
    notice("account:notice", "Home recommendations need a signed-in account", "YouTube provides Home recommendations only to signed-in accounts"),
    notice("account:notice", "Search", "Square searches YouTube from any list"),
    notice("discover", "Discover", "Topics and YouTube sections"),
    notice("library", "Accounts & library", "Sign in to YouTube to get Home recommendations")])
for topic, numbers in SHELVES.items():
    write("guest", "home-topic:" + topic, [video(number) for number in numbers])
# A Guest for whom nothing answers with a video: the page says it is empty.
write("empty", "library:home", [])
for topic in SHELVES:
    write("empty", "home-topic:" + topic, [])
write("signed-in", "home-topic:live", [video(4), video(1)])
