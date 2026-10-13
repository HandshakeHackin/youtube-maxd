#!/usr/bin/env python3
"""Writes the fixture catalogs of the channel, channels and subscriptions screens.

Every channel, title and number is invented (the proposal's sample content and
a few more in its style). signed-in/library:subscriptions is also Home's third
shelf.

usage: python3 tests/afterglow/tools/fixtures_channel.py tests/afterglow/fixtures
"""
import json
import os
import sys

ROOT = sys.argv[1]
DOT = " · "

CHANNELS = [
    ("Northlight Atlas", "@northlightatlas", "2.41M", "184"),
    ("Glassfern Games", "@glassfern", "1.08M", "412"),
    ("Orbitwell", "@orbitwell", "3.9M", "96"),
    ("Ostrel Kitchen", "@ostrelkitchen", "884K", "231"),
    ("Nacre Sound", "@nacresound", "412K", "77"),
    ("Lathe & Lumen", "@latheandlumen", "655K", "58"),
    ("Tidewrit", "@tidewrit", "1.7M", "310"),
    ("Quarterline", "@quarterline", "2.2M", "1,204"),
    ("Slowreel", "@slowreel", "529K", "64"),
    ("Mossbyte", "@mossbyte", "301K", "142"),
    ("Fennwick Hours", "@fennwickhours", "5.3M", "39"),
    ("Vantor Trails", "@vantortrails", "960K", "118"),
    # A second page of subscriptions, in the same style.
    ("Quillhaven Notes", "@quillhavennotes", "74K", "51"),
    ("Brindlemoor Cycles", "@brindlemoor", "219K", "163"),
    ("Ossel & Wren", "@osselandwren", "1.3M", "88"),
    ("Tarnwick Radio", "@tarnwickradio", "46K", "420"),
    ("Vellumgate", "@vellumgate", "512K", "29"),
    ("Hollowpine Workshop", "@hollowpine", "338K", "71"),
]
UNSEEN = {0: 3, 1: 1, 2: 2, 6: 1, 10: 4}
LIVE = {4}


def cid(c):
    slug = CHANNELS[c][1][1:]
    return "UC" + (slug + "0" * 22)[:22]


# title, channel, length, views, age, scene, palette, seed, progress, flags, days since published
VIDEOS = [
    ("Seven Days Alone on the Ridge Line — A Silent Hiking Film", 0, "48:12", "1.2M views", "3 weeks ago", "ridges", 0, 11, 0.65, "4k cc", 21),
    ("I Rebuilt a Whole City in One Weekend (No Mods)", 1, "26:41", "842K views", "5 days ago", "city", 1, 23, 0.3, "", 5),
    ("What Would a Sunset Look Like on Another World?", 2, "14:07", "3.4M views", "2 months ago", "space", 0, 35, 0.85, "4k cc", 60),
    ("One Pan, Six Dinners: The Lazy Week Plan", 3, "18:33", "410K views", "1 day ago", "plate", 0, 47, 0.12, "new cc", 1),
    ("Late Night Synth Session — Live from the Boathouse", 4, "", "2,184 watching", "Streaming since yesterday", "stage", 0, 59, 0.0, "live", 1),
    ("The Desk Setup I Would Build Again From Scratch", 5, "12:05", "267K views", "1 week ago", "desk", 0, 61, 0.0, "4k", 7),
    ("Why Do Tides Arrive Late? A 10-Minute Explainer", 6, "10:26", "95K views", "4 days ago", "ocean", 1, 73, 0.0, "cc", 4),
    ("Every Goal From a Wild Final Day", 7, "9:48", "1.9M views", "16 hours ago", "pitch", 0, 85, 0.0, "new", 0),
    ("How One Long Take Changes a Whole Scene", 8, "21:15", "530K views", "1 month ago", "blobs", 3, 97, 0.0, "cc", 31),
    ("Writing a Tiny Renderer in an Afternoon", 9, "1:02:44", "148K views", "2 weeks ago", "code", 0, 109, 0.45, "cc", 14),
    ("Rainy Window, Warm Lamp — 3 Hours of Calm Piano", 10, "3:00:00", "6.1M views", "1 year ago", "blobs", 1, 121, 0.0, "", 365),
    ("Crossing the Salt Flats by Bicycle", 11, "33:20", "720K views", "3 months ago", "dunes", 3, 133, 0.0, "4k", 90),
    ("Aurora Chasing at -30° (We Almost Missed It)", 0, "19:58", "2.2M views", "8 months ago", "aurora", 0, 145, 0.0, "4k cc", 240),
    ("The Smallest Synth I Own Is the One I Use Most", 4, "8:14", "64K views", "5 days ago", "synth", 0, 157, 0.0, "", 5),
    ("Speedrunning My Own Level and Regretting It", 1, "15:02", "1.1M views", "2 weeks ago", "synth", 1, 169, 1.0, "", 14),
    ("Dune Driving School: Don't Do What I Did", 11, "24:36", "388K views", "5 months ago", "dunes", 0, 181, 0.0, "", 150),
    ("A Map of Every Night Train Still Running", 6, "16:40", "1.4M views", "3 weeks ago", "globe", 1, 193, 1.0, "cc", 21),
    ("The Ring Problem, Explained With a Bowl of Marbles", 2, "11:52", "905K views", "1 month ago", "space", 1, 205, 0.0, "cc", 31),
    ("Five Knife Skills That Changed My Cooking", 3, "11:11", "2.7M views", "1 year ago", "plate", 1, 217, 0.0, "", 365),
    ("Building a Lamp From One Sheet of Brass", 5, "28:09", "312K views", "3 days ago", "desk", 2, 229, 0.0, "", 3),
    ("Blue Hour on the Northern Coast, No Talking", 0, "41:30", "640K views", "2 months ago", "ocean", 1, 241, 0.0, "4k", 60),
    ("Night Walk Through the Lantern District", 11, "52:18", "1.6M views", "4 months ago", "city", 0, 253, 0.0, "4k", 120),
    ("Tonight's Top Stories in Eight Minutes", 6, "8:02", "212K views", "3 hours ago", "globe", 2, 265, 0.0, "new", 0),
    ("Clay Court Classics: The Longest Rally", 7, "13:27", "455K views", "2 weeks ago", "pitch", 1, 277, 0.0, "", 14),
    ("Lo-Fi Morning Mix for Slow Coffee", 10, "", "8,412 watching", "Streaming for 3 days", "blobs", 4, 289, 0.0, "live", 3),
    ("We Mapped a Forest From the Air", 2, "17:45", "1.0M views", "4 months ago", "globe", 0, 301, 0.0, "cc", 120),
    ("The Forest at Night Sounds Nothing Like You Think", 0, "22:09", "380K views", "6 weeks ago", "aurora", 2, 313, 0.0, "", 42),
    ("Hard Court Highlights: Game Seven", 7, "10:15", "3.1M views", "2 days ago", "pitch", 2, 325, 0.0, "new", 2),
    ("Building a Pocket Console From Spare Parts", 5, "34:50", "1.3M views", "7 months ago", "desk", 1, 337, 0.0, "", 210),
    ("Why Your Shadows Look Wrong (and the Fix)", 9, "19:03", "88K views", "5 hours ago", "code", 1, 349, 0.0, "new cc", 0),
    ("Sunrise Set From the Lighthouse Roof", 4, "1:12:40", "196K views", "5 weeks ago", "ocean", 0, 361, 0.0, "", 35),
    ("Moonrise Over the Dune Sea in Real Time", 11, "6:12", "540K views", "1 year ago", "dunes", 1, 373, 0.0, "4k", 365),
    ("A Year on a Tidal Island", 0, "58:04", "4.8M views", "2 years ago", "ocean", 3, 385, 0.0, "4k cc", 730),
    ("Dusk Run: Ember Ridge in One Take", 11, "12:48", "76K views", "9 hours ago", "ridges", 3, 397, 0.0, "new", 0),
    ("Violet Hour: A Short Film", 8, "7:31", "233K views", "3 weeks ago", "ridges", 6, 409, 0.0, "", 21),
    ("Teal Dawn Above the Valley", 0, "15:20", "410K views", "10 months ago", "ridges", 2, 421, 0.0, "4k", 300),
    ("Neon Rain: Four Hours of City Ambience", 10, "4:00:00", "2.9M views", "6 months ago", "city", 2, 433, 0.0, "", 180),
    ("Grid Runner — Full Synthwave Set", 4, "56:20", "730K views", "3 months ago", "synth", 2, 445, 0.0, "", 90),
    # More of the first channel, so that its Videos section has a second page.
    ("First Snow on the Pass, Dawn to Dusk", 0, "36:44", "512K views", "1 year ago", "ridges", 1, 457, 0.0, "4k", 380),
    ("Walking the Sea Cliffs in Fog", 0, "27:19", "298K views", "1 year ago", "ocean", 2, 469, 0.0, "", 400),
    ("Three Nights in a Fire Lookout", 0, "44:02", "1.6M views", "1 year ago", "aurora", 1, 481, 0.0, "4k cc", 420),
    ("The Long Lake in Winter Silence", 0, "31:55", "220K views", "2 years ago", "ocean", 0, 493, 0.0, "", 740),
    ("Storm Light Over the Fjord", 0, "18:26", "905K views", "2 years ago", "ridges", 4, 505, 0.0, "4k", 760),
    ("Autumn on the High Moor", 0, "23:40", "174K views", "2 years ago", "dunes", 2, 517, 0.0, "", 800),
    ("A Night Under the Milky Way, Unedited", 0, "1:24:10", "3.3M views", "3 years ago", "space", 2, 529, 0.0, "4k", 1100),
    ("Crossing the Glacier Tongue", 0, "39:08", "660K views", "3 years ago", "ridges", 5, 541, 0.0, "cc", 1150),
    ("Morning Mist in the Birch Forest", 0, "20:33", "142K views", "3 years ago", "aurora", 3, 553, 0.0, "", 1200),
    ("Midnight Sun on the Northern Road", 0, "47:51", "480K views", "4 years ago", "dunes", 1, 565, 0.0, "4k", 1500),
]
OVERVIEW = {
    12: "Four nights above the tree line, waiting for a gap in the cloud. It came at three in the morning.",
}


def vid(i):
    return "ptv%08d" % i


def seconds(length):
    total = 0
    for part in length.split(":"):
        total = total * 60 + int(part)
    return total if length else 0


def video(i, **more):
    title, c, length, views, age, scene, palette, seed, progress, flags, days = VIDEOS[i]
    flags = flags.split()
    row = {"id": vid(i), "title": title, "subtitle": CHANNELS[c][0], "channel_id": cid(c),
           "art_url": "art://%s/%d/%d/384x216" % (scene, palette, seed), "views": views, "published": age}
    if i in OVERVIEW:
        row["overview"] = OVERVIEW[i]
    if "live" in flags:
        row["is_live"] = True
    else:
        row["duration_sec"] = seconds(length)
    if progress >= 1.0:
        row["played"] = True
    elif progress > 0.0:
        row["resume_sec"] = int(seconds(length) * progress)
    badges = [f for f in flags if f != "live"]
    if badges:
        row["badges"] = badges
    row["published_days"] = days
    row.update(more)
    return row


def channel(c, **facts):
    name, handle, subs, _ = CHANNELS[c]
    row = {"id": "channel:" + cid(c), "title": name, "subtitle": subs + " subscribers", "is_folder": True,
           "handle": handle, "subscribers": subs + " subscribers"}
    row.update(facts)
    return row


def notice(title, subtitle=""):
    return {"id": "account:notice", "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}


def name_of(catalog, page=0):
    name = "".join(ch if ch.isalnum() or ch in "-_" else "_" for ch in catalog) if catalog else "root"
    return name + (".p%d" % page if page else "")


def write(group, catalog, items=None, more=False, page=0, error=None, offline=False):
    folder = os.path.join(ROOT, group, "catalog")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name_of(catalog, page) + ".json")
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        if error is not None:
            document = {"error": error}
            if offline:
                document["offline"] = True
            out.write(json.dumps(document, ensure_ascii=False) + "\n")
            return
        out.write("{\n \"items\": [\n")
        out.write(",\n".join("  " + json.dumps(row, ensure_ascii=False) for row in items))
        out.write("\n ],\n \"has_more\": %s\n}\n" % ("true" if more else "false"))


def profile(group, ident, name, guest, signed_in):
    folder = os.path.join(ROOT, group)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "profile.json")
    if os.path.exists(path):
        return  # an existing set keeps its profile
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps({"id": ident, "name": name, "guest": guest, "signed_in": signed_in}) + "\n")


# ---- the channel page -----------------------------------------------------------------------

NORTH = 0
ORBIT = 2
BANNER = "art://ridges/6/77/1280x320"
UPLOADS = [12, 20, 26, 32, 35, 0, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47]


def number(text):
    value, unit = text.split()[0], 1
    if value.endswith("M"):
        value, unit = value[:-1], 1000000
    elif value.endswith("K"):
        value, unit = value[:-1], 1000
    return float(value.replace(",", "")) * unit


def head(c, tab, **facts):
    base = {"videos": CHANNELS[c][3] + " videos", "banner_url": BANNER, "tab": tab}
    base.update(facts)
    return channel(c, **base)


def channel_page(group, subscribed):
    ident = cid(NORTH)
    tabs = {"tabs": "home,videos,shorts,live,playlists", "search": True, "subscribed": subscribed, "notify": "personalized"}
    sorts = {"sorts": "newest,popular,oldest"}
    # Home: the featured video, then the latest uploads.
    write(group, "channel:" + ident, [head(NORTH, "home", **tabs)] + [video(12, art_url="art://aurora/0/145")] + [video(i) for i in [0, 26, 20, 32, 35]])
    write(group, "channel-tab:%s:videos" % ident, [head(NORTH, "videos", sort="newest", **tabs, **sorts)] + [video(i) for i in UPLOADS[:8]], more=True)
    write(group, "channel-tab:%s:videos" % ident, [video(i) for i in UPLOADS[8:]], page=1)
    popular = sorted(UPLOADS, key=lambda i: -number(VIDEOS[i][3]))
    write(group, "channel-tab:%s:videos:popular" % ident, [head(NORTH, "videos", sort="popular", **tabs, **sorts)] + [video(i) for i in popular[:8]])
    write(group, "channel-tab:%s:videos:oldest" % ident, [head(NORTH, "videos", sort="oldest", **tabs, **sorts)] + [video(i) for i in UPLOADS[::-1][:8]])
    write(group, "channel-tab:%s:videos:newest" % ident, [head(NORTH, "videos", sort="newest", **tabs, **sorts)] + [video(i) for i in UPLOADS[:8]])
    shorts = [("Thirty Seconds of Alpenglow", 34, "ridges", 3, 601), ("Fog Rolling Over the Ridge", 41, "ridges", 2, 613),
              ("One Minute at the Tarn", 58, "ocean", 3, 625), ("Ice Cracking on the Lake", 27, "ocean", 0, 637),
              ("Dawn Chorus at the Treeline", 52, "aurora", 1, 649)]
    rows = [head(NORTH, "shorts", **tabs)]
    for n, (title, length, scene, palette, seed) in enumerate(shorts):
        rows.append({"id": "ptvshort%03d" % n, "title": title, "subtitle": CHANNELS[NORTH][0], "channel_id": ident,
                     "art_url": "art://%s/%d/%d/384x216" % (scene, palette, seed), "duration_sec": length, "is_short": True,
                     "views": "%dK views" % (40 + 37 * n), "published": "%d weeks ago" % (n + 1)})
    write(group, "channel-tab:%s:shorts" % ident, rows)
    # Nothing is live: the section is there and empty.
    write(group, "channel-tab:%s:live" % ident, [head(NORTH, "live", **tabs)])
    lists = [("Northern Lights, Start to Finish", "24 videos", "Updated 3 days ago", "aurora", 0, 145),
             ("The Ridge Line Films", "7 videos", "Updated last month", "ridges", 0, 11),
             ("Coast and Tide", "12 videos", "Updated 2 months ago", "ocean", 1, 241),
             ("Nights Without Narration", "31 videos", "Updated last year", "space", 2, 529)]
    rows = [head(NORTH, "playlists", **tabs)]
    for n, (title, count, updated, scene, palette, seed) in enumerate(lists):
        rows.append({"id": "playlist:PLnorthlightatlas%04d" % n, "title": title, "subtitle": "Playlist", "is_folder": True,
                     "art_url": "art://%s/%d/%d/384x216" % (scene, palette, seed), "count": count, "updated": updated, "owner": CHANNELS[NORTH][0]})
    write(group, "channel-tab:%s:playlists" % ident, rows)


channel_page("signed-in", False)
channel_page("guest", False)

# A channel the account subscribes to, with all notifications: fewer sections, no order, no search.
orbit = cid(ORBIT)
write("signed-in", "channel:" + orbit,
      [channel(ORBIT, videos="96 videos", banner_url="art://space/1/88/1280x320", tab="videos", tabs="videos,playlists", search=False,
               subscribed=True, notify="all")] + [video(i) for i in [2, 17, 25]])
write("signed-in", "channel-tab:%s:playlists" % orbit, [channel(ORBIT, tab="playlists", tabs="videos,playlists", search=False)])
# A channel that is gone.
write("signed-in", "channel:UCgonechannel00000000000", error="This channel is not available.")

# The account's answers. A change is named first ("account:apply:<n>") and made when that row is listed.
def acting(group, row, serial, answer=None, error=None):
    write(group, row, [{"id": "account:apply:%d" % serial, "title": "Confirm", "subtitle": "Confirm for YouTube profile: Mara", "is_folder": True, "interface": True}])
    if error:
        write(group, "account:apply:%d" % serial, error=error)
    else:
        write(group, "account:apply:%d" % serial, [notice(answer or "YouTube account updated")])


north = cid(NORTH)
acting("signed-in", "account:confirm:subscribe:" + north, 17)  # 7 is the accounts screen's (an account's removal)
acting("signed-in", "account:confirm:unsubscribe:" + orbit, 8)
acting("signed-in", "notify:confirm:level:personalized:" + orbit, 9)
acting("signed-in", "notify:confirm:level:none:" + orbit, 10, error="YouTube did not confirm this notification level.")

# ---- the actions of a video and of a channel -------------------------------------------------

def folder(ident, title, subtitle=""):
    return {"id": ident, "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}


def video_menu(group, i):
    v = vid(i)
    c = VIDEOS[i][1]
    write(group, "menu:" + v, [
        folder("local:video:1:next:" + v, "Play next", "Saved for the selected profile on this PS5"),
        folder("account:confirm:watch_later_add:" + v, "Add to YouTube Watch later", "YouTube profile: Mara"),
        folder("playlist:pick:add:" + v, "Add to a YouTube playlist", "Choose a playlist, then confirm"),
        {"id": "channel:" + cid(c), "title": "Open channel", "subtitle": CHANNELS[c][0], "is_folder": True, "interface": True},
        folder("description:" + v, "Open description", "Read the full description of this video"),
        folder("details:" + v, "Video details", "Channel, date, views, likes and what the video offers"),
        folder("account:confirm:not_interested:" + v, "Not interested", "YouTube profile: Mara"),
        folder("local:video:1:block:" + v, "Block channel", "Hide this channel in catalogs for the selected profile"),
        folder("local:video:1:watched:" + v, "Mark as watched locally", "Saved for the selected profile on this PS5"),
        folder("link:video:" + v, "Share link", "The video, embed, channel and playlist links, each with a QR code"),
        folder("local:video:1:sbexclude:" + v, "Exclude channel from SponsorBlock", "Never skip segments in this channel's videos"),
    ])
    write(group, "local:video:1:next:" + v, [notice("Plays next", VIDEOS[i][0])])
    acting(group, "account:confirm:watch_later_add:" + v, 20 + i, "Added to Watch later")
    write(group, "playlist:pick:add:" + v, [
        folder("playlist:confirm:add:%s:PLmaraevenings0001" % v, "Evenings", "12 videos"),
        folder("playlist:confirm:add:%s:PLmaralongwalks001" % v, "Long walks", "31 videos"),
        folder("playlist:confirm:add:%s:PLmaratolearn00001" % v, "To learn from", "7 videos"),
    ])
    acting(group, "playlist:confirm:add:%s:PLmaraevenings0001" % v, 60 + i, "Added to Evenings")
    write(group, "local:video:1:watched:" + v, error="This video could not be marked. Try again.")


for i in (7, 20):
    video_menu("signed-in", i)
write("signed-in", "menu:channel:" + north, [
    folder("local:channel:1:pin:" + north, "Pin to the main menu", "Keep this channel beside Home"),
    folder("notify:" + north, "Subscription notifications", "Subscribe with a notification level, or change the level"),
    folder("link:channel:" + north, "Share link", "The channel's link with a QR code"),
    folder("local:channel:1:block:" + north, "Block channel", "Hide this channel in catalogs for the selected profile"),
])
write("signed-in", "local:channel:1:pin:" + north, [notice("Pinned to the main menu", CHANNELS[NORTH][0])])
write("signed-in", "menu:channel:" + orbit, [
    folder("local:channel:1:pin:" + orbit, "Pin to the main menu", "Keep this channel beside Home"),
    folder("account:confirm:unsubscribe:" + orbit, "Unsubscribe on YouTube", "YouTube profile: Mara"),
    folder("link:channel:" + orbit, "Share link", "The channel's link with a QR code"),
])

# ---- subscribed channels -------------------------------------------------------------------

def tile(c):
    facts = {"subscribed": True}
    if c in UNSEEN:
        facts["unseen"] = UNSEEN[c]
    if c in LIVE:
        facts["live"] = True
    return channel(c, **facts)


ACCOUNT_ORDER = list(range(len(CHANNELS)))
NEW_FIRST = [c for c in ACCOUNT_ORDER if c in UNSEEN] + [c for c in ACCOUNT_ORDER if c not in UNSEEN]
BY_NAME = sorted(ACCOUNT_ORDER, key=lambda c: CHANNELS[c][0].lower())


def order_rows(selected):
    labels = ["Account order", "By name", "New content first"]
    rows = []
    for n, label in enumerate(labels):
        row = folder("settings:channel-sort:%d" % n, label, "Selected" if n == selected else "Order of the Subscribed channels list for this profile")
        row["selected"] = n == selected
        row["kind"] = "list"
        rows.append(row)
    return rows


def subscribed(group, order, selected):
    write(group, "library:channels", [tile(c) for c in order[:12]], more=True)
    write(group, "library:channels", [tile(c) for c in order[12:]], page=1)
    write(group, "settings:channel-sort", order_rows(selected))
    for n in range(3):
        write(group, "settings:channel-sort:%d" % n, order_rows(n))
    write(group, "channels:watched", [notice("5 channels marked as watched", "Reload Subscribed channels to see the list without their marks")])


subscribed("signed-in", NEW_FIRST, 2)
LOCAL = [0, 6, 9]


def local_follows(group, channels):
    write(group, "subscriptions", [{"id": "channel:" + cid(c), "title": CHANNELS[c][0], "subtitle": "Followed locally on this PS5", "is_folder": True}
                                   for c in channels])


local_follows("signed-in", LOCAL)
# A Guest who follows nothing, and one who follows three channels on this console.
local_follows("guest", [])
profile("guest-follows", "guest", "", True, False)
local_follows("guest-follows", LOCAL)
write("guest-follows", "menu:channel:" + cid(LOCAL[0]), [
    {"id": "local:channel:1:unfollow:" + cid(LOCAL[0]), "title": "Stop following on this PS5", "subtitle": "The channel leaves this list", "is_folder": True, "interface": True},
    {"id": "link:channel:" + cid(LOCAL[0]), "title": "Share link", "subtitle": "The channel's link with a QR code", "is_folder": True, "interface": True},
])

# ---- the subscriptions feed ------------------------------------------------------------------

FEED = [7, 22, 29, 33, 27, 19, 13, 3, 4, 24, 6, 1, 5, 9, 14, 23, 16, 8, 34, 30, 26, 2, 11, 17]
write("signed-in", "library:subscriptions", [video(i) for i in FEED[:16]], more=True)
write("signed-in", "library:subscriptions", [video(i) for i in FEED[16:]], page=1)

# ---- a signed-in profile with nothing --------------------------------------------------------

profile("signed-in", "mara", "Mara", False, True)
profile("empty", "mara", "Mara", False, True)
write("empty", "library:channels", [])
write("empty", "library:subscriptions", [])
write("empty", "subscriptions", [])
write("empty", "settings:channel-sort", order_rows(0))
quill = cid(12)
write("empty", "channel:" + quill, [channel(12, videos="No videos", tab="videos", tabs="videos", subscribed=False)])

# ---- the account cannot be reached, what the console keeps can ---------------------------------

profile("offline", "mara", "Mara", False, True)
GONE = "The YouTube service cannot be reached."
write("offline", "library:channels", error=GONE, offline=True)
write("offline", "library:subscriptions", error=GONE, offline=True)
write("offline", "settings:channel-sort", order_rows(2))
local_follows("offline", LOCAL)

# ---- long texts: German-length titles, long names, big numbers ---------------------------------

profile("long-texts", "mara", "Mara-Sophie von Hohenstaufen-Wittgenstein", False, True)
LONG_CHANNELS = [
    ("Nordlicht-Atlas: Langsame Filme aus dem Hochgebirge und von der Küste", "@nordlichtatlas.langsamefilme.offiziell", "12.345.678 Abonnenten"),
    ("Glasfarn Spiele — Aufbauspiele, Speedruns und sehr lange Abende", "@glasfarnspiele", "1.080.000 Abonnenten"),
    ("Umlaufbahn", "@umlaufbahn", "3,9 Mio. Abonnenten"),
    ("Die Wochenendküche für Menschen mit genau einer guten Pfanne", "@wochenendkueche", "884.000 Abonnenten"),
    ("Perlmutt Klang", "@perlmuttklang", "412.000 Abonnenten"),
    ("Drehbank & Lichtstrom Werkstattberichte", "@drehbankundlichtstrom", "655.000 Abonnenten"),
    ("Gezeitenschrift", "@gezeitenschrift", "1,7 Mio. Abonnenten"),
    ("Viertellinie — Alle Höhepunkte, nichts vom Geschrei", "@viertellinie", "2,2 Mio. Abonnenten"),
]


def lcid(n):
    return "UC" + ("langertext%02d" % n + "0" * 22)[:22]


def long_channel(n, **facts):
    name, handle, subs = LONG_CHANNELS[n]
    row = {"id": "channel:" + lcid(n), "title": name, "subtitle": subs, "is_folder": True, "handle": handle, "subscribers": subs}
    row.update(facts)
    return row


LONG_VIDEOS = [
    ("Sieben Tage allein auf dem Grat — ein stiller Wanderfilm ohne Erzählstimme und ohne Musik", 0, 2892, "1.234.567 Aufrufe", "vor 3 Wochen", "ridges", 0, 11, 21),
    ("Polarlichtjagd bei -30° (fast hätten wir es verpasst) — die vollständige Nachtaufnahme", 0, 1198, "2,2 Mio. Aufrufe", "vor 8 Monaten", "aurora", 0, 145, 0),
    ("Ich habe an einem Wochenende eine ganze Stadt neu gebaut", 1, 1601, "842.000 Aufrufe", "vor 5 Tagen", "city", 1, 23, 0),
    ("Wie sähe ein Sonnenuntergang auf einer anderen Welt aus?", 2, 847, "3,4 Mio. Aufrufe", "vor 2 Monaten", "space", 0, 35, 0),
    ("Eine Pfanne, sechs Abendessen: der bequeme Wochenplan für alle, die abends nicht mehr denken wollen", 3, 1113, "410.000 Aufrufe", "vor 1 Tag", "plate", 0, 47, 1),
    ("Synthesizer-Session bei Nacht — live aus dem Bootshaus", 4, 0, "2.184 Zuschauer", "Seit gestern live", "stage", 0, 59, 1),
    ("Der Schreibtischaufbau, den ich genau so wieder bauen würde", 5, 725, "267.000 Aufrufe", "vor 1 Woche", "desk", 0, 61, 7),
    ("Warum kommt die Flut zu spät? Eine Erklärung in zehn Minuten", 6, 626, "95.000 Aufrufe", "vor 4 Tagen", "ocean", 1, 73, 4),
    ("Alle Tore eines verrückten letzten Spieltags", 7, 588, "1,9 Mio. Aufrufe", "vor 16 Stunden", "pitch", 0, 85, 0),
    ("Donaudampfschifffahrtsgesellschaftskapitänsmützenabzeichen: ein einziges Wort als Titel", 0, 2490, "640.000 Aufrufe", "vor 2 Monaten", "ocean", 1, 241, 60),
    ("Der Wald klingt nachts ganz anders, als man denkt", 0, 1329, "380.000 Aufrufe", "vor 6 Wochen", "aurora", 2, 313, 42),
    ("Ein Jahr auf einer Gezeiteninsel", 0, 3484, "4,8 Mio. Aufrufe", "vor 2 Jahren", "ocean", 3, 385, 730),
]


def long_video(n, **more):
    title, c, length, views, age, scene, palette, seed, days = LONG_VIDEOS[n]
    row = {"id": "ptvlang%04d" % n, "title": title, "subtitle": LONG_CHANNELS[c][0], "channel_id": lcid(c),
           "art_url": "art://%s/%d/%d/384x216" % (scene, palette, seed), "views": views, "published": age, "published_days": days,
           "overview": "Vier Nächte oberhalb der Baumgrenze, in denen wir auf eine Lücke in den Wolken gewartet haben. Sie kam um drei Uhr morgens, "
                       "und niemand hatte mehr damit gerechnet."}
    if length:
        row["duration_sec"] = length
    else:
        row["is_live"] = True
    row.update(more)
    return row


long_id = lcid(0)
long_head = dict(videos="1.284 Videos", banner_url="art://aurora/1/91/1280x320", tabs="home,videos,shorts,live,playlists", search=True,
                 subscribed=False, notify="personalized")
write("long-texts", "channel:" + long_id, [long_channel(0, tab="home", **long_head)] + [long_video(n) for n in [1, 0, 9, 10, 11]])
write("long-texts", "channel-tab:%s:videos" % long_id,
      [long_channel(0, tab="videos", sorts="newest,popular,oldest", sort="newest", **long_head)] + [long_video(n) for n in [1, 0, 9, 10, 11]])
acting("long-texts", "account:confirm:subscribe:" + long_id, 7,
       error="YouTube hat das Abonnement nicht bestätigt. Bitte versuche es in einigen Minuten noch einmal.")
write("long-texts", "library:channels",
      [long_channel(n, subscribed=True, **({"unseen": 128} if n == 0 else {"live": True} if n == 4 else {"unseen": 2} if n == 3 else {}))
       for n in sorted(range(len(LONG_CHANNELS)), key=lambda n: LONG_CHANNELS[n][0].lower())])
write("long-texts", "settings:channel-sort", order_rows(1))
for n in range(3):
    write("long-texts", "settings:channel-sort:%d" % n, order_rows(n))
write("long-texts", "library:subscriptions", [long_video(n, badges=["new"] if n in (2, 8) else []) for n in [8, 1, 2, 3, 4, 5, 7, 6, 0, 10, 9, 11]])
write("long-texts", "subscriptions", [])
print("fixtures written under", ROOT)
