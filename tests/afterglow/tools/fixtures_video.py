#!/usr/bin/env python3
"""Writes the fixture catalogs of the video page and the comments.

Every name, title, number and comment is invented (the interface proposal's
sample set). A catalog is written as the provider answers it: the rows of
details:, of the actions menu, of a discussion, and the rows that act when they
are listed.

usage: python3 tests/afterglow/tools/fixtures_video.py tests/afterglow/fixtures
"""
import json
import os
import re
import sys

ROOT = sys.argv[1]
DASH = "—"
WRITTEN = []


def name_of(catalog, page=0):
    name = re.sub(r"[^A-Za-z0-9_-]", "_", catalog) if catalog else "root"
    return name + (".p%d" % page if page else "") + ".json"


def write(fixture, catalog, rows=None, more=False, page=0, error=None):
    folder = os.path.join(ROOT, fixture, "catalog")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name_of(catalog, page))
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        if error is not None:
            out.write(json.dumps({"error": error}, ensure_ascii=False) + "\n")
        else:
            out.write('{\n "items": [\n')
            out.write(",\n".join("  " + json.dumps(row, ensure_ascii=False) for row in rows))
            out.write('\n ],\n "has_more": %s\n}\n' % ("true" if more else "false"))
    WRITTEN.append(path)


def profile(fixture, ident, name, guest, signed_in):
    folder = os.path.join(ROOT, fixture)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "profile.json")
    if os.path.exists(path):
        return  # a set another package also uses: its profile is left as it is
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps({"id": ident, "name": name, "guest": guest, "signed_in": signed_in}) + "\n")


def notice(ident, title, subtitle=""):
    return {"id": "account:notice:" + ident, "is_folder": True, "interface": True, "title": title, "subtitle": subtitle}


def entry(ident, title, subtitle=""):
    row = {"id": ident, "is_folder": True, "interface": True, "title": title}
    if subtitle:
        row["subtitle"] = subtitle
    return row


CHANNELS = {
    "atlas": ("UCNorthlightAtlas0000001", "Northlight Atlas", "2.41M subscribers"),
    "trails": ("UCVantorTrails000000001", "Vantor Trails", "960K subscribers"),
    "slowreel": ("UCSlowreel0000000000001", "Slowreel", "529K subscribers"),
    "orbit": ("UCOrbitwell000000000001", "Orbitwell", "3.9M subscribers"),
    "fennwick": ("UCFennwickHours00000001", "Fennwick Hours", "5.3M subscribers"),
    "nacre": ("UCNacreSound0000000001", "Nacre Sound", "412K subscribers"),
}


def video(ident, title, channel, art, seconds, views, age, badges=(), resume=0, live=False):
    assert len(ident) == 11, ident
    row = {"id": ident, "title": title, "subtitle": CHANNELS[channel][1], "channel_id": CHANNELS[channel][0],
           "art_url": "art://" + art + "/320x180", "duration_sec": seconds, "views": views, "published": age}
    if badges:
        row["badges"] = list(badges)
    if resume:
        row["resume_sec"] = resume
    if live:
        row["is_live"] = True
    return row


RELATED = [
    video("aur0raChase", "Aurora Chasing at -30° (We Almost Missed It)", "atlas", "aurora/0/145", 1198, "2.2M views", "8 months ago", ("4k", "cc")),
    video("blueHourNth", "Blue Hour on the Northern Coast, No Talking", "atlas", "ocean/1/241", 2490, "640K views", "2 months ago", ("4k",)),
    video("forestNight", "The Forest at Night Sounds Nothing Like You Think", "atlas", "aurora/2/313", 1329, "380K views", "6 weeks ago"),
    video("tidalIsland", "A Year on a Tidal Island", "atlas", "ocean/3/385", 3484, "4.8M views", "2 years ago", ("4k", "cc")),
    video("tealDawnVal", "Teal Dawn Above the Valley", "atlas", "ridges/2/421", 920, "410K views", "10 months ago", ("4k",)),
    video("duskRunEmbr", "Dusk Run: Ember Ridge in One Take", "trails", "ridges/3/397", 768, "76K views", "2 days ago", ("new",)),
    video("violetHour1", "Violet Hour: A Short Film", "slowreel", "ridges/6/409", 451, "233K views", "3 weeks ago"),
    video("moonriseDun", "Moonrise Over the Dune Sea in Real Time", "trails", "dunes/1/373", 372, "540K views", "1 year ago", ("4k",)),
]
RELATED_MORE = [
    video("saltFlatsBk", "Crossing the Salt Flats by Bicycle", "trails", "dunes/3/133", 2000, "720K views", "3 months ago", ("4k",)),
    video("lanternWalk", "Night Walk Through the Lantern District", "trails", "city/0/253", 3138, "1.6M views", "4 months ago", ("4k",)),
    video("sunriseSet1", "Sunrise Set From the Lighthouse Roof", "nacre", "ocean/0/361", 4360, "196K views", "5 weeks ago"),
    video("forestAir01", "We Mapped a Forest From the Air", "orbit", "globe/0/301", 1065, "1.0M views", "4 months ago", ("cc",)),
    video("sunsetWorld", "What Would a Sunset Look Like on Another World?", "orbit", "space/0/35", 847, "3.4M views", "2 months ago", ("4k", "cc"), resume=720),
    video("rainyWindow", "Rainy Window, Warm Lamp " + DASH + " 3 Hours of Calm Piano", "fennwick", "blobs/1/121", 10800, "6.1M views", "1 year ago"),
]

DESCRIPTION = ("Seven days along the ridge line with one pack and no plan for the weather. Filmed in silence: no narration "
               "and no music, only what the microphones heard. Route notes, elevation and the full gear list are below, "
               "with a chapter for every day of the walk.")
DESCRIPTION_FULL = (DESCRIPTION + " Day one starts at the trailhead at dusk and ends at the first camp, a flat shelf under the "
                    "saddle. Day four is the storm: nothing was planned, the tent held, the microphones kept running. The descent "
                    "on day seven follows the old hut path; the hut itself has been closed for years. Gear: one 55 litre pack, a "
                    "two-wall tent, a gas stove, three lenses and far too many batteries. No drone was flown anywhere along the "
                    "ridge. Thank you for walking with us.")


def details(fixture, ident, title, channel, art, *, views, age, likes, seconds=0, resume=0, quality="", fps=0, chapters=0,
            segments=0, captions=0, subscribed=False, rating="", saved=False, overview=DESCRIPTION, related=True,
            description=True, live=False, subtitle=None):
    channel_id, channel_name, subscribers = CHANNELS[channel] if isinstance(channel, str) else channel
    if subtitle is None:
        parts = (["Live"] if live else []) + (["4K"] if "4k" in quality else [])
        if seconds and not live:
            parts.append("%d:%02d" % (seconds // 60, seconds % 60) if seconds < 3600 else "%d:%02d:%02d" % (seconds // 3600, seconds // 60 % 60, seconds % 60))
        subtitle = " | ".join(parts) if parts else "Video details"
    # The first row is the video; its facts are section 5.5's and package B's additions (docs/afterglow/requests/B.md).
    head = {"id": "account:notice:title", "is_folder": True, "title": title, "subtitle": subtitle, "overview": overview,
            "art_url": "art://" + art, "views": views, "published": age, "likes": likes, "channel": channel_name,
            "channel_id": channel_id, "channel_art_url": "", "subscribers": subscribers, "subscribed": subscribed, "rating": rating,
            "saved": saved, "chapters": chapters, "segments": segments, "captions": captions, "fps": fps, "quality": quality,
            "duration": seconds, "resume": resume, "live": live}
    rows = [head,
            {"id": "channel:" + channel_id, "is_folder": True, "title": channel_name, "subtitle": subscribers, "art_url": ""},
            notice("published", age, "Published"),
            notice("views", views),
            notice("likes", likes + " likes")]
    if description:
        rows.append(entry("description:" + ident, "Read the description"))
    if related:
        rows.append(entry("library:suggestions:" + ident, "Related videos"))
    write(fixture, "details:" + ident, rows)
    return channel_id, channel_name


def applied(fixture, row, serial, title, error=None, done="YouTube account updated", done_line="The companion confirmed this change"):
    """An account change: the row that prepares it answers with the row that sends it."""
    write(fixture, row, [entry("account:apply:%d" % serial, title, "Confirm for YouTube profile: Mara")])
    if error:
        write(fixture, "account:apply:%d" % serial, error=error)
    else:
        write(fixture, "account:apply:%d" % serial, [notice("done", done, done_line)])


def menu(fixture, ident, channel_id, signed_in=True, liked=False, saved=False, subscribed=False, extra=()):
    rows = [entry("local:video:m:next:" + ident, "Play next"),
            entry("local:video:m:restart:" + ident, "Play from start"),
            entry("local:video:m:incognito:" + ident, "Play incognito")]
    if signed_in:
        rows += [entry("account:confirm:%s:%s" % ("unrate" if liked else "like", ident), "Remove video rating" if liked else "Like video"),
                 entry("account:confirm:watch_later_%s:%s" % ("remove" if saved else "add", ident),
                       "Remove from YouTube Watch later" if saved else "Add to YouTube Watch later"),
                 entry("playlist:pick:add:" + ident, "Add to a playlist"),
                 entry("account:confirm:%s:%s" % ("unsubscribe" if subscribed else "subscribe", channel_id),
                       "Unsubscribe on YouTube" if subscribed else "Subscribe on YouTube")]
    else:
        rows.append(notice("profile", "Choose an account profile first", "Likes, playlists and subscriptions need a YouTube account"))
    rows += [entry("channel:" + channel_id, "Open channel"),
             entry("description:" + ident, "Read the description"),
             entry("details:" + ident, "Video details"),
             entry("library:suggestions:" + ident, "Related videos")]
    if signed_in:
        rows += [entry("account:confirm:not_interested:" + ident, "Not interested"),
                 entry("account:confirm:dont_recommend_channel:" + ident, "Don't recommend this channel")]
    rows += [entry("local:video:m:block:" + ident, "Block channel"),
             entry("local:video:m:watched:" + ident, "Mark as watched"),
             entry("local:video:m:sbexclude:" + ident, "Exclude channel from SponsorBlock"),
             entry("link:video:" + ident, "Share link")]
    rows += list(extra)
    write(fixture, "menu:" + ident, rows)
    # What the local rows answer when they are listed.
    write(fixture, "local:video:m:next:" + ident, [notice("done", "Plays next", "After the current video")])
    write(fixture, "local:video:m:incognito:" + ident, [notice("done", "Incognito", "The next video leaves no history")])
    write(fixture, "local:video:m:block:" + ident, [notice("done", "Channel blocked", "Its videos are hidden on this profile")])
    write(fixture, "local:video:m:watched:" + ident, [notice("done", "Marked as watched")])
    write(fixture, "local:video:m:sbexclude:" + ident, [notice("done", "Channel excluded from SponsorBlock")])


def links(fixture, ident, channel_id, playlist=None):
    rows = [entry("link:row:video", "Video link"), entry("link:row:embed", "Embed link"), entry("link:row:channel", "Channel link")]
    rows[0]["overview"] = "https://youtu.be/" + ident
    rows[1]["overview"] = "https://www.youtube.com/embed/" + ident
    rows[2]["overview"] = "https://www.youtube.com/channel/" + channel_id
    if playlist:
        rows.append(entry("link:row:playlist", "Playlist link"))
        rows[-1]["overview"] = "https://www.youtube.com/playlist?list=" + playlist
    write(fixture, "link:video:" + ident, rows)


def comment(ident, author, age, text, likes, replies=0, **marks):
    """A comment as the provider lists it, with the facts of section 5.5."""
    parts = [age, likes + " likes"]
    if replies:
        parts.append("%d replies" % replies)
    for mark, word in (("pinned", "Pinned"), ("owner", "Creator"), ("member", "Member"), ("hearted", "Creator's heart"),
                       ("liked", "You like it")):
        if marks.get(mark):
            parts.append(word)
    row = {"id": ident, "is_folder": True, "title": author, "subtitle": " | ".join(parts), "overview": text, "author": author,
           "published": age, "likes": likes, "replies": replies, "author_art_url": ""}
    for mark in ("pinned", "owner", "member", "hearted", "liked", "disliked"):
        if marks.get(mark):
            row[mark] = True
    return row


def comment_menu(fixture, row_id, cursor, serial, liked=False, fail=None):
    """What can be done with a comment: Data::menu(row) names this catalog."""
    rows = [entry("comment:confirm:%s:%s" % ("none" if liked else "like", cursor), "Remove comment rating" if liked else "Like comment"),
            entry("comment:confirm:dislike:" + cursor, "Dislike comment"),
            entry("comment:reply:" + cursor, "Reply to comment")]
    write(fixture, "menu:" + row_id, rows)
    for action in ("like", "none", "dislike"):
        write(fixture, "comment:confirm:%s:%s" % (action, cursor), [entry("account:apply:%d" % serial, "Like comment", "Confirm for YouTube profile: Mara")])
    if fail:
        write(fixture, "account:apply:%d" % serial, error=fail)
    else:
        write(fixture, "account:apply:%d" % serial, [notice("done", "YouTube account updated", "The companion confirmed this change")])
    # A reply is reviewed, then sent once.
    write(fixture, "comment:reply:" + cursor, [entry("comment:review:%d" % (serial + 1), "Post this comment", "Cross posts once for the selected YouTube profile")])
    write(fixture, "comment:review:%d" % (serial + 1), [entry("account:apply:%d" % (serial + 1), "Post this comment")])
    if fail:
        write(fixture, "account:apply:%d" % (serial + 1), error="The comment was not confirmed. Reload the comments before writing it again.")
    else:
        write(fixture, "account:apply:%d" % (serial + 1), [notice("done", "Comment posted", "Reload the comments to see it")])


ATLAS = "Northlight Atlas"
THREAD = [
    comment("discussion:replies:rpAtlasPinned", ATLAS, "3 weeks ago",
            "Thank you for walking with us. Route notes and the full gear list are in the description; the storm on day four was not planned.",
            "4.1K", 38, pinned=True, owner=True),
    comment("discussion:replies:rpMirela0001", "Mirela K.", "2 weeks ago",
            "No voice-over, no music, just wind and boots for forty-eight minutes. I did not look at my phone once.", "2.8K", 12, hearted=True),
    comment("discussion:replies:rpQuietpixel1", "quietpixel", "2 weeks ago",
            "29:10 " + DASH + " the light coming over the ridge. Rewound it four times.", "1.9K", 4),
    comment("discussion:replies:rpTobias00001", "Tobias Renk", "12 days ago",
            "Watched this on the big screen with the lights off. The fog at first camp looks unreal in 4K.", "904", 2),
    comment("discussion:replies:rpAnouk000001", "Anouk V.", "9 days ago",
            "My grandfather walked this ridge in the sixties. He recognised the saddle hut straight away.", "3.3K", 21, hearted=True),
    comment("discussion:replies:rpSamOutside1", "sam is outside", "6 days ago", "Which stove is that at 6:40? Asking for a very cold friend.", "187", 3),
    comment("discussion:leaf:1:6", "Halden B.", "2 days ago", "The saddle hut at 22:30 is where I proposed. Thank you for filming it empty.", "96"),
    comment("discussion:leaf:1:7", "Ines Arkwright", "1 day ago", "Watched all forty-eight minutes on mute by accident. It works that way too.", "41", member=True),
]
THREAD_MORE = [
    comment("discussion:leaf:2:0", "Okonkwo R.", "22 hours ago", "The sound of the tent in the storm is the best thing on this channel.", "38"),
    comment("discussion:replies:rpDagny000001", "Dagny", "20 hours ago", "Day six, the cloud inversion. I paused it and just sat there.", "112", 1),
    comment("discussion:leaf:2:2", "small hours", "14 hours ago", "Volume up, lights off, perfect.", "27"),
    comment("discussion:leaf:2:3", "lantern.keys", "9 hours ago",
            "Came for the ridge, stayed for the forty seconds of a kettle boiling. Nobody else films the waiting, and the waiting is "
            "most of a walk like this one. I timed it: eleven minutes of this film are somebody sitting still, and none of them is "
            "wasted. The cut from the stove to the first light on the east face is the quietest good edit I have seen all year. "
            "Please never add music.", "64"),
    comment("discussion:leaf:2:4", "pedal steel pete", "5 hours ago", "Subscribed at 14:05.", "9"),
    comment("discussion:leaf:2:5", "Wren", "1 hour ago", "Who else is here before the long weekend?", "3"),
]
REPLIES_MIRELA = [
    comment("discussion:leaf:3:0", ATLAS, "2 weeks ago", "That is the best review this channel has had.", "612", owner=True),
]
REPLIES_MIRELA_MORE = [
    comment("discussion:leaf:4:0", "Tobias Renk", "13 days ago", "Same. I put the phone in another room for the second half.", "88"),
    comment("discussion:leaf:4:1", "mossgarden", "12 days ago", "Forty-eight minutes and it felt like ten.", "31"),
    comment("discussion:leaf:4:2", "Mirela K.", "12 days ago", "Now I want the three hour cut.", "140", liked=True),
]


def discussion(fixture, ident, can_post=True, count="1,284", thread=THREAD, more=THREAD_MORE):
    lead = entry("comment:post:" + ident, "Write a comment", count) if can_post else notice("comments", count, "Comments")
    write(fixture, "discussion:top:" + ident, [lead] + thread, more=bool(more))
    if more:
        write(fixture, "discussion:top:" + ident, more, page=1)
    newest = list(reversed(more or [])) + [row for row in reversed(thread) if not row.get("pinned")]
    pinned = [row for row in thread if row.get("pinned")]
    write(fixture, "discussion:newest:" + ident, [lead] + pinned + newest)
    if thread:
        write(fixture, "discussion:replies:rpMirela0001", REPLIES_MIRELA, more=True)
        write(fixture, "discussion:replies:rpMirela0001", REPLIES_MIRELA_MORE, page=1)


# ---- signed in: Mara ------------------------------------------------------------------
S = "signed-in"
profile(S, "mara", "Mara", False, True)
FEATURE = "pT5xRidgeLn"
TITLE = "Seven Days Alone on the Ridge Line " + DASH + " A Silent Hiking Film"
atlas_id, _ = details(S, FEATURE, TITLE, "atlas", "ridges/0/11/800x450", views="1.2M views", age="3 weeks ago", likes="48,213",
                      seconds=2892, resume=1872, quality="4k", fps=60, chapters=7, segments=4, captions=9)
write(S, "library:suggestions:" + FEATURE, RELATED, more=True)
write(S, "library:suggestions:" + FEATURE, RELATED_MORE, page=1)
menu(S, FEATURE, atlas_id)
links(S, FEATURE, atlas_id, playlist="PLquietFilms0001")
feature_row = video(FEATURE, TITLE, "atlas", "ridges/0/11/800x450", 2892, "1.2M views", "3 weeks ago", ("4k", "cc"))
write(S, "local:video:m:restart:" + FEATURE, [feature_row])
applied(S, "account:confirm:like:" + FEATURE, 101, "Like video")
applied(S, "account:confirm:unrate:" + FEATURE, 102, "Remove video rating")
applied(S, "account:confirm:watch_later_add:" + FEATURE, 103, "Add to YouTube Watch later")
applied(S, "account:confirm:watch_later_remove:" + FEATURE, 104, "Remove from YouTube Watch later")
applied(S, "account:confirm:subscribe:" + atlas_id, 105, "Subscribe on YouTube")
applied(S, "account:confirm:unsubscribe:" + atlas_id, 106, "Unsubscribe on YouTube")
applied(S, "account:confirm:not_interested:" + FEATURE, 107, "Not interested")
applied(S, "account:confirm:dont_recommend_channel:" + FEATURE, 108, "Don't recommend this channel")
write(S, "playlist:pick:add:" + FEATURE, [
    entry("playlist:confirm:add:%s:PLquietFilms0001" % FEATURE, "Quiet films", "14 videos"),
    entry("playlist:confirm:add:%s:PLweekendLong001" % FEATURE, "Weekend, the long ones", "6 videos"),
    entry("playlist:confirm:add:%s:PLshowToDad00001" % FEATURE, "Show to Dad", "3 videos"),
    entry("playlist:prompt:create:" + FEATURE, "New playlist"),
])
applied(S, "playlist:confirm:add:%s:PLquietFilms0001" % FEATURE, 111, "Add to Quiet films")
applied(S, "playlist:confirm:add:%s:PLweekendLong001" % FEATURE, 112, "Add to Weekend, the long ones")
applied(S, "playlist:confirm:add:%s:PLshowToDad00001" % FEATURE, 113, "Add to Show to Dad")
write(S, "description:" + FEATURE, [
    {"id": "description:text:1", "is_folder": True, "title": TITLE, "subtitle": "Description", "overview": DESCRIPTION_FULL},
    {"id": "channel:" + atlas_id, "is_folder": True, "title": "Northlight Atlas", "subtitle": "2.41M subscribers"},
    notice("published", "3 weeks ago", "Published"),
    notice("views", "1,204,381 views"),
    notice("likes", "48,213 likes"),
])
discussion(S, FEATURE)
comment_menu(S, "discussion:replies:rpMirela0001", "rcMirela0001", 301)
comment_menu(S, "discussion:replies:rpAtlasPinned", "rcAtlasPinned", 311)
comment_menu(S, "discussion:replies:rpQuietpixel1", "rcQuietpixel1", 321,
             fail="The comment rating was not confirmed.")
write(S, "comment:post:" + FEATURE, [entry("comment:review:331", "Post this comment", "Cross posts once for the selected YouTube profile")])
write(S, "comment:review:331", [entry("account:apply:331", "Post this comment")])
write(S, "account:apply:331", [notice("done", "Comment posted", "Reload the comments to see it")])

# A second video: nothing watched yet, already liked, saved and subscribed; YouTube refuses the change of its rating.
SECOND = "aur0raChase"
details(S, SECOND, "Aurora Chasing at -30° (We Almost Missed It)", "atlas", "aurora/0/145/800x450", views="2.2M views",
        age="8 months ago", likes="96K", seconds=1198, quality="4k", chapters=5, captions=4, subscribed=True, rating="like", saved=True,
        overview="Three nights on the frozen lake waiting for a gap in the cloud. It came at four in the morning on the last night, "
                 "for eleven minutes. This is all of them.")
write(S, "library:suggestions:" + SECOND, RELATED[1:6])
menu(S, SECOND, atlas_id, liked=True, saved=True, subscribed=True)
links(S, SECOND, atlas_id)
applied(S, "account:confirm:unrate:" + SECOND, 201, "Remove video rating",
        error="YouTube did not confirm the account change. Check your account before repeating it.")
applied(S, "account:confirm:watch_later_remove:" + SECOND, 202, "Remove from YouTube Watch later")

# A live stream: no length, no place to resume, nothing related named.
LIVE = "synthLive01"
details(S, LIVE, "Late Night Synth Session " + DASH + " Live from the Boathouse", "nacre", "stage/0/59/800x450", views="2,184 watching",
        age="Started 40 minutes ago", likes="1,902", live=True, related=False, captions=0,
        overview="A new set from the boathouse, live. If a piece falls apart, it was jazz.")
menu(S, LIVE, CHANNELS["nacre"][0])
links(S, LIVE, CHANNELS["nacre"][0])

# ---- Guest ----------------------------------------------------------------------------
G = "guest"
profile(G, "guest", "", True, False)
details(G, FEATURE, TITLE, "atlas", "ridges/0/11/800x450", views="1.2M views", age="3 weeks ago", likes="48,213", seconds=2892,
        resume=1872, quality="4k", fps=60, chapters=7, segments=4, captions=9)
write(G, "library:suggestions:" + FEATURE, RELATED)
menu(G, FEATURE, atlas_id, signed_in=False)
links(G, FEATURE, atlas_id)
write(G, "local:video:m:restart:" + FEATURE, [feature_row])
discussion(G, FEATURE, can_post=False, more=None)

# ---- nothing there --------------------------------------------------------------------
E = "empty"
profile(E, "mara", "Mara", False, True)
details(E, FEATURE, TITLE, "atlas", "ridges/0/11/800x450", views="12 views", age="1 hour ago", likes="0", seconds=2892, overview="",
        related=False, description=False)
write(E, "menu:" + FEATURE, [])
links(E, FEATURE, atlas_id)
write(E, "discussion:top:" + FEATURE, [entry("comment:post:" + FEATURE, "Write a comment", "0")])
write(E, "discussion:newest:" + FEATURE, [entry("comment:post:" + FEATURE, "Write a comment", "0")])

# ---- long texts -----------------------------------------------------------------------
L = "long-texts"
profile(L, "maximiliane", "Maximiliane-Annabelle", False, True)
LONG_CHANNEL = ("UCHochgebirgsfilme00001", "Norddeutsche Gesellschaft für Hochgebirgswanderfilme e. V. " + DASH + " Offizieller Kanal",
                "12.345.678 Abonnenten")
LONG_TITLE = ("Sieben Tage allein auf dem Grat: Donaudampfschifffahrtsgesellschaftskapitän wandert ohne Kommentar, ohne Musik und "
              "ohne Plan über die Hochgebirgshöhenwanderwege " + DASH + " der stille Wanderfilm in voller Länge")
long_id, long_name = details(
    L, FEATURE, LONG_TITLE, LONG_CHANNEL, "ridges/0/11/800x450", views="1.204.381.002 Aufrufe", age="vor 11 Jahren und 3 Monaten",
    likes="1.482.213", seconds=40953, resume=36119, quality="4k,hdr", fps=120, chapters=124, segments=38, captions=112,
    overview="Sieben Tage auf dem Grat mit einem Rucksack und ohne Plan für das Wetter. Gefilmt in Stille: keine Erzählung "
             "und keine Musik, nur was die Mikrofone gehört haben. Routennotizen, Höhenprofil und die vollständige "
             "Ausrüstungsliste stehen weiter unten, zusammen mit einem Kapitel für jeden einzelnen Tag der Wanderung und "
             "den Koordinaten aller Lagerplätze.")
CHANNELS["long"] = LONG_CHANNEL
write(L, "library:suggestions:" + FEATURE, [
    video("aur0raChase", "Polarlichtjagd bei -30° (fast hätten wir es verpasst) " + DASH + " die ungekürzte Nachtaufnahme", "long", "aurora/0/145", 359999, "2.204.118.377 Aufrufe", "vor 8 Monaten", ("4k", "cc")),
    video("blueHourNth", "Blauestundenküstenwanderungsaufzeichnung", "long", "ocean/1/241", 2490, "640.000 Aufrufe", "vor 2 Monaten", ("new", "4k")),
    video("forestNight", "Der Wald klingt nachts ganz anders, als du denkst, und zwar aus mehreren Gründen", "long", "aurora/2/313", 1329, "380.000 Aufrufe", "vor 6 Wochen"),
    video("tidalIsland", "Ein Jahr auf einer Gezeiteninsel", "long", "ocean/3/385", 3484, "4,8 Mio. Aufrufe", "vor 2 Jahren", ("4k",)),
    video("tealDawnVal", "Türkisfarbene Morgendämmerung über dem Tal", "long", "ridges/2/421", 920, "410.000 Aufrufe", "vor 10 Monaten"),
    video("duskRunEmbr", "Dämmerungslauf", "long", "ridges/3/397", 768, "76.000 Aufrufe", "vor 2 Tagen", ("new",)),
])
menu(L, FEATURE, long_id, extra=(entry("account:confirm:history_remove:" + FEATURE, "Aus dem YouTube-Wiedergabeverlauf dieses Kontos dauerhaft entfernen"),))
links(L, FEATURE, long_id, playlist="PLhochgebirgswanderfilme0000000001")
long_comments = [
    comment("discussion:replies:rpLongPinned1", LONG_CHANNEL[1], "vor 11 Jahren und 3 Monaten (bearbeitet)",
            "Vielen Dank fürs Mitwandern. Die Routennotizen und die vollständige Ausrüstungsliste stehen in der Beschreibung; der "
            "Sturm am vierten Tag war nicht geplant, das Zelt hat gehalten, und die Mikrofone liefen einfach weiter.", "1.234.567", 12345,
            pinned=True, owner=True),
    comment("discussion:replies:rpLongSecond1", "Maximiliane-Annabelle von Hohenzollern-Sigmaringen-Wolkenstein", "vor 2 Wochen",
            "Keine Stimme, keine Musik, nur Wind und Stiefel, achtundvierzig Minuten lang. Ich habe kein einziges Mal auf mein Telefon "
            "geschaut, und das ist mir seit Jahren bei keinem Film mehr passiert. Besonders die elf Minuten, in denen jemand einfach "
            "nur still sitzt und auf den Kocher wartet: Niemand sonst filmt das Warten, dabei besteht eine solche Wanderung fast nur "
            "daraus. Der Schnitt vom Kocher zum ersten Licht an der Ostwand ist der leiseste gute Schnitt, den ich dieses Jahr gesehen "
            "habe. Bitte fügt niemals Musik hinzu, und bitte kürzt die Fassung nicht für irgendeine Plattform.", "98.765", 432,
            hearted=True, member=True),
    comment("discussion:leaf:1:2", "q", "vor 3 Min.", "Schön.", "1"),
    comment("discussion:leaf:1:3", "Rechtsschutzversicherungsgesellschaftenvergleichsportal", "vor 1 Std.",
            "Donaudampfschifffahrtsgesellschaftskapitänsmützenabzeichenherstellerverbandsvorsitzendenstellvertreter.", "12"),
]
discussion(L, FEATURE, count="1.284.019", thread=long_comments, more=None)
write(L, "discussion:replies:rpLongSecond1", REPLIES_MIRELA)

print("%d fixture files" % len(WRITTEN))
