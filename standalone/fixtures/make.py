#!/usr/bin/env python3
"""Write the canned upstream answers used by the offline tests.

Hand-made, minimal InnerTube shapes: enough for the pinned youtubei.js parsers
and nothing copied from a live response. Run after changing a fixture here:
    python3 standalone/fixtures/make.py
SPDX-License-Identifier: GPL-3.0-or-later
"""
import hashlib
import json
from pathlib import Path
import struct

HERE = Path(__file__).resolve().parent
CHANNEL = "UC" + "f" * 22
PLAYLIST = "PLfixture0001"
VIDEO, LIVE, GONE, SHORT, DENIED = "fixtureVID1", "fixtureLIV1", "fixtureGONE", "fixtureSHT1", "fixtureDENY"
ZOO = "jNQXAC9IVRw"
MEDIA = "https://rr1---sn-fixture.googlevideo.com/videoplayback?expire=1999999999&c=VISIONOS&sig=FIXTURE&x=a%26b"


def write(name, value):
    path = HERE / name
    if isinstance(value, bytes):
        path.write_bytes(value)
    elif isinstance(value, str):
        path.write_text(value, encoding="utf-8")
    else:
        path.write_text(json.dumps(value, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")


def runs(text, channel=None):
    run = {"text": text}
    if channel:
        run["navigationEndpoint"] = {"browseEndpoint": {"browseId": channel}}
    return {"runs": [run]}


def video(identifier, title, owner="Fixture channel", channel=CHANNEL, length="1:00", art=None):
    return {"videoRenderer": {
        "videoId": identifier,
        "thumbnail": {"thumbnails": [{"url": art or f"https://i.ytimg.com/vi/{identifier}/hq720.jpg", "width": 720, "height": 404}]},
        "title": runs(title), "ownerText": runs(owner, channel), "longBylineText": runs(owner, channel),
        "lengthText": {"simpleText": length}, "navigationEndpoint": {"watchEndpoint": {"videoId": identifier}}}}


def numbered(number):
    return f"fixture{number:04d}"


def search():
    rows = [video(ZOO, "Me at the zoo", "jawed", "UC4QobU6STFB0P71PMvOGN5A", "0:19"),
            video("AAAAAAAAAAA", "Second canned row", "Canned channel", "UC" + "a" * 22, "1:02:03",
                  "https://example.org/not-an-image-host.jpg")]
    rows += [video(numbered(n), f"Canned result {n}") for n in range(2, 30)]
    return {"estimatedResults": "30", "contents": {"twoColumnSearchResultsRenderer": {"primaryContents": {
        "sectionListRenderer": {"contents": [{"itemSectionRenderer": {"contents": rows}}]}}}}}


def channel():
    rows = [{"richItemRenderer": {"content": video(numbered(100 + n), f"Channel video {n}")}} for n in range(3)]
    tab = {"tabRenderer": {
        "title": "Videos", "selected": True,
        "endpoint": {"browseEndpoint": {"browseId": CHANNEL, "params": "EgZ2aWRlb3M", "canonicalBaseUrl": "/@fixture"},
                     "commandMetadata": {"webCommandMetadata": {"url": "/@fixture/videos"}}},
        "content": {"richGridRenderer": {"contents": rows}}}}
    return {"header": {"c4TabbedHeaderRenderer": {"channelId": CHANNEL, "title": "Fixture channel"}},
            "metadata": {"channelMetadataRenderer": {
                "title": "Fixture channel", "description": "", "externalId": CHANNEL,
                "channelUrl": f"https://www.youtube.com/channel/{CHANNEL}", "vanityChannelUrl": "http://www.youtube.com/@fixture",
                "avatar": {"thumbnails": [{"url": "https://yt3.ggpht.com/fixture=s88", "width": 88, "height": 88}]},
                "isFamilySafe": True, "keywords": "", "availableCountryCodes": ["US"], "androidDeepLink": "",
                "androidAppindexingLink": "", "iosAppindexingLink": "", "rssUrl": ""}},
            "contents": {"twoColumnBrowseResultsRenderer": {"tabs": [tab]}}}


def playlist():
    def row(n):
        identifier = numbered(200 + n)
        return {"playlistVideoRenderer": {
            "videoId": identifier, "index": {"simpleText": str(n + 1)},
            "title": {**runs(f"Playlist video {n}"), "accessibility": {"accessibilityData": {"label": f"Playlist video {n}"}}},
            "shortBylineText": runs("Fixture channel", CHANNEL), "lengthSeconds": "60", "lengthText": {"simpleText": "1:00"},
            "thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg", "width": 480, "height": 360}]},
            "isPlayable": True,
            "navigationEndpoint": {"watchEndpoint": {"videoId": identifier, "playlistId": PLAYLIST, "index": n}}}}
    return {"header": {"playlistHeaderRenderer": {"playlistId": PLAYLIST, "title": {"simpleText": "Fixture playlist"}}},
            "contents": {"twoColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"selected": True, "content": {
                "sectionListRenderer": {"contents": [{"itemSectionRenderer": {"contents": [{"playlistVideoListRenderer": {
                    "playlistId": PLAYLIST, "isEditable": False, "contents": [row(n) for n in range(3)]}}]}}]}}}}]}}}


def topics():
    """A topic as the TV client lists it: shelves of tiles. The second shelf repeats a video of the first."""
    def tile(n):
        identifier = f"fixture{200 + n:04d}"
        return {"tileRenderer": {"contentType": "TILE_CONTENT_TYPE_VIDEO", "contentId": identifier,
            "onSelectCommand": {"watchEndpoint": {"videoId": identifier}},
            "header": {"tileHeaderRenderer": {"thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg"}]},
                "thumbnailOverlays": [{"thumbnailOverlayTimeStatusRenderer": {"text": {"simpleText": "2:03"}}}]}},
            "metadata": {"tileMetadataRenderer": {"title": {"simpleText": f"Topic video {n}"},
                "lines": [{"lineRenderer": {"items": [{"lineItemRenderer": {"text": {"simpleText": "Fixture channel"}}}]}}]}}}}

    def shelf(title, numbers):
        return {"shelfRenderer": {"title": {"simpleText": title}, "content": {"horizontalListRenderer": {"items": [tile(n) for n in numbers]}}}}
    return {"contents": {"tvBrowseRenderer": {"content": {"tvSurfaceContentRenderer": {"content": {"sectionListRenderer": {
        "contents": [shelf("Recommended", range(0, 3)), shelf("Top music videos", range(2, 6))]}}}}}}}


def track(itag, mime, bitrate, average, extra):
    return {"itag": itag, "url": f"{MEDIA}&itag={itag}", "mimeType": mime, "bitrate": bitrate, "averageBitrate": average,
            "lastModified": "1700000000000000", "contentLength": "1000000", "approxDurationMs": "635200",
            "initRange": {"start": "0", "end": "740"}, "indexRange": {"start": "741", "end": "2000"}, **extra}


def picture(itag, mime, width, height, fps, average, label):
    return track(itag, mime, average * 2, average, {"width": width, "height": height, "fps": fps, "qualityLabel": label, "quality": "hd"})


def sound(itag, mime, average, rate, extra=None):
    return track(itag, mime, average, average, {"audioQuality": "AUDIO_QUALITY_MEDIUM", "audioSampleRate": str(rate),
                                              "audioChannels": 2, "quality": "tiny", **(extra or {})})


def details(identifier, title, live=False):
    return {"videoId": identifier, "title": title, "lengthSeconds": "0" if live else "635", "channelId": CHANNEL,
            "author": "Fixture channel", "shortDescription": "A fixture.\n0:00 Intro\n1:30 Middle part\n5:00 The end\n",
            "thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{identifier}/hqdefault.jpg", "width": 480, "height": 360}]},
            "viewCount": "12345", "isLiveContent": live, **({"isLive": True} if live else {})}


def player():
    captions = {"playerCaptionsTracklistRenderer": {
        "captionTracks": [
            {"baseUrl": f"https://www.youtube.com/api/timedtext?v={VIDEO}&lang=en&signature=FIXTURE", "name": {"simpleText": "English"},
             "vssId": ".en", "languageCode": "en", "isTranslatable": True},
            {"baseUrl": f"https://www.youtube.com/api/timedtext?v={VIDEO}&lang=de&kind=asr&signature=FIXTURE",
             "name": {"simpleText": "German (auto-generated)"}, "vssId": "a.de", "languageCode": "de", "kind": "asr", "isTranslatable": True}],
        "translationLanguages": [{"languageCode": "fr", "languageName": {"simpleText": "French"}},
                                 {"languageCode": "es", "languageName": {"simpleText": "Spanish"}}]}}
    boards = (f"https://i.ytimg.com/sb/{VIDEO}/storyboard3_L$L/$N.jpg?sqp=fixture"
              "|48#27#100#10#10#0#default#sigh0|80#45#128#10#10#5000#M$M#sigh1|160#90#128#5#5#5000#M$M#sigh2")
    return {"playabilityStatus": {"status": "OK"}, "videoDetails": details(VIDEO, "Fixture video"),
            "streamingData": {"expiresInSeconds": "21540", "formats": [], "adaptiveFormats": [
                picture(299, 'video/mp4; codecs="avc1.64002a"', 1920, 1080, 60, 3248000, "1080p60"),
                picture(303, 'video/webm; codecs="vp9"', 1920, 1080, 60, 2127000, "1080p60"),
                picture(399, 'video/mp4; codecs="av01.0.09M.08"', 1920, 1080, 60, 1568000, "1080p60"),
                picture(136, 'video/mp4; codecs="avc1.4d401f"', 1280, 720, 30, 1100000, "720p"),
                picture(315, 'video/webm; codecs="vp9"', 3840, 2160, 60, 17174000, "2160p60"),
                picture(337, 'video/webm; codecs="vp09.02.51.10"', 3840, 2160, 30, 18500000, "2160p30 HDR"),
                sound(140, 'audio/mp4; codecs="mp4a.40.2"', 129500, 44100),
                sound(140, 'audio/mp4; codecs="mp4a.40.2"', 129600, 44100, {"isDrc": True}),
                sound(251, 'audio/webm; codecs="opus"', 135000, 48000)]},
            "captions": captions, "storyboards": {"playerStoryboardSpecRenderer": {"spec": boards}},
            "playerConfig": {"audioConfig": {"loudnessDb": -3.5}}}


def short_player():
    """A portrait video of 23 seconds with a paid-promotion notice."""
    answer = {"playabilityStatus": {"status": "OK"}, "videoDetails": {**details(SHORT, "Fixture short"), "lengthSeconds": "23"},
              "streamingData": {"expiresInSeconds": "21540", "adaptiveFormats": [
                  picture(299, 'video/mp4; codecs="avc1.64002a"', 1080, 1920, 30, 2000000, "1920p"),
                  sound(140, 'audio/mp4; codecs="mp4a.40.2"', 129500, 44100)]},
              "paidContentOverlay": {"paidContentOverlayRenderer": {"text": {"runs": [{"text": "Includes  paid\n"}, {"text": "promotion"}]}}}}
    return answer


def watch_next():
    """The watch page's structured chapters (the uploader's)."""
    chapters = [("Opening", 0), ("Main part", 120000), ("Credits", 500000)]
    return {"contents": {"twoColumnWatchNextResults": {"results": {"results": {"contents": []}},
                                                       "secondaryResults": {"secondaryResults": {"results": []}}}},
            "playerOverlays": {"playerOverlayRenderer": {"decoratedPlayerBarRenderer": {"decoratedPlayerBarRenderer": {"playerBar": {
        "multiMarkersPlayerBarRenderer": {"markersMap": [{"key": "DESCRIPTION_CHAPTERS", "value": {"chapters": [
            {"chapterRenderer": {"title": {"simpleText": title}, "timeRangeStartMillis": start, "thumbnail": {"thumbnails": []}}}
            for title, start in chapters]}}]}}}}}}}


def live_player():
    return {"playabilityStatus": {"status": "OK"}, "videoDetails": details(LIVE, "Fixture live stream", live=True),
            "streamingData": {"expiresInSeconds": "21540", "adaptiveFormats": [],
                              "hlsManifestUrl": f"https://manifest.googlevideo.com/api/manifest/hls_variant/id/{LIVE}/file/index.m3u8"}}


MASTER = """#EXTM3U
#EXT-X-INDEPENDENT-SEGMENTS
#EXT-X-MEDIA:URI="https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/234/playlist/index.m3u8",TYPE=AUDIO,GROUP-ID="234",NAME="Default",DEFAULT=YES,AUTOSELECT=YES
#EXT-X-STREAM-INF:BANDWIDTH=2500000,CODECS="avc1.4D401F,mp4a.40.2",RESOLUTION=1280x720,FRAME-RATE=30,AUDIO="234",CLOSED-CAPTIONS=NONE
https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/232/playlist/index.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=5000000,CODECS="avc1.640028,mp4a.40.2",RESOLUTION=1920x1080,FRAME-RATE=60,AUDIO="234",CLOSED-CAPTIONS=NONE
https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/270/playlist/index.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=4000000,CODECS="vp09.00.40.08,mp4a.40.2",RESOLUTION=1920x1080,FRAME-RATE=60,AUDIO="234",CLOSED-CAPTIONS=NONE
https://manifest.googlevideo.com/api/manifest/hls_playlist/itag/614/playlist/index.m3u8
"""
VTT = """WEBVTT
Kind: captions
Language: en

00:00:01.200 --> 00:00:03.360 align:start position:0%
All right, so here <c>we are</c>

00:00:03.360 --> 00:00:05.000
in front of the &amp; fixture
"""


def prefix(identifier):
    return hashlib.sha256(identifier.encode()).hexdigest()[:4]


def main():
    write("config.json", {"responseContext": {"globalConfigGroup": {}}})
    write("search.json", search())
    write("channel.json", channel())
    write("playlist.json", playlist())
    write("topics.json", topics())
    # Completions as YouTube's suggestion address answers them: a call around the list, one of them twice.
    write("suggest.txt", 'window.google.ac.h(["canned",[["canned heat",0,[512]],["canned music for a quiet room",0,[512]],'
                         '["canned heat",0],["canned laughter",0]],{"k":1,"q":"fixture"}])')
    write("player.json", player())
    write("player_live.json", live_player())
    write("player_short.json", short_player())
    # The same video on a media host that refuses every read (links YouTube will not serve).
    denied = json.loads(json.dumps(player()).replace("rr1---sn-fixture", "rr2---sn-fixture").replace(VIDEO, DENIED))
    write("player_denied.json", denied)
    write("next.json", watch_next())
    write("next_empty.json", {})
    write("player_gone.json", {"playabilityStatus": {"status": "ERROR", "reason": "This video is unavailable"}})
    write("live.m3u8", MASTER)
    write("captions.vtt", VTT)
    write("sheet.jpg", bytes.fromhex("ffd8ffe000104a46494600010100000100010000ffd9"))
    # A segment index (ISO BMFF `sidx`, version 0): three fragments of 4, 6 and 5 seconds.
    references = [(1000, 4000), (2000, 6000), (1500, 5000)]
    body = struct.pack(">4sIIIIIHH", b"sidx", 0, 1, 1000, 0, 0, 0, len(references))
    body += b"".join(struct.pack(">III", size, duration, 0x90000000) for size, duration in references)
    write("sidx.bin", struct.pack(">I", len(body) + 4) + body)
    write("sponsor.json", [
        {"videoID": "anotherVID1", "segments": [{"category": "sponsor", "actionType": "skip", "segment": [1, 2]}]},
        {"videoID": VIDEO, "segments": [
            {"category": "sponsor", "actionType": "skip", "segment": [12.5, 40]},
            {"category": "selfpromo", "actionType": "skip", "segment": [100, 90]},
            {"category": "intro", "actionType": "skip", "segment": [0, 5]},
            {"category": "poi_highlight", "actionType": "poi", "segment": [60, 60]}]}])
    # The same video on an alternative SponsorBlock server, which lists another segment.
    write("sponsor_alt.json", [{"videoID": VIDEO, "segments": [{"category": "sponsor", "actionType": "skip", "segment": [3, 9]}]}])
    # Return YouTube Dislike: a good answer, one for another video, one whose count is not written as an integer, one too large.
    write("dislikes.json", {"id": VIDEO, "likes": 120, "dislikes": 7, "viewCount": 9, "rating": 4.5})
    write("dislikes_other.json", {"id": "zzzzzzzzzzz", "likes": 1, "dislikes": 1})
    write("dislikes_float.json", '{"id": "%s", "likes": 5.0, "dislikes": 1}\n' % SHORT)
    write("dislikes_large.json", "[" + "1," * 40000 + "1]")
    write("branding.json", {ZOO: {"titles": [{"title": "The first > video", "original": False, "votes": 3, "locked": False}],
                                  "thumbnails": [{"timestamp": 7.5, "original": False, "votes": 0, "locked": True}]}})
    write("index.json", {
        "GET www.youtube.com/sw.js_data": {"status": 404},
        "POST www.youtube.com/youtubei/v1/config": {"file": "config.json"},
        "POST www.youtube.com/youtubei/v1/search": {"file": "search.json"},
        "POST www.youtube.com/youtubei/v1/browse": [
            {"body_contains": f'"browseId":"VL{PLAYLIST}"', "file": "playlist.json"},
            {"body_contains": f'"browseId":"{CHANNEL}"', "file": "channel.json"},
            {"body_contains": '"browseId":"FEtopics_music"', "file": "topics.json"}],
        "POST www.youtube.com/youtubei/v1/player": [
            {"body_contains": f'"videoId":"{VIDEO}"', "file": "player.json"},
            {"body_contains": f'"videoId":"{LIVE}"', "file": "player_live.json"},
            {"body_contains": f'"videoId":"{SHORT}"', "file": "player_short.json"},
            {"body_contains": f'"videoId":"{DENIED}"', "file": "player_denied.json"},
            {"body_contains": f'"videoId":"{GONE}"', "file": "player_gone.json"}],
        "POST www.youtube.com/youtubei/v1/next": [
            {"body_contains": f'"videoId":"{VIDEO}"', "file": "next.json"}, {"file": "next_empty.json"}],
        f"GET manifest.googlevideo.com/api/manifest/hls_variant/id/{LIVE}/file/index.m3u8": {"file": "live.m3u8", "type": "application/vnd.apple.mpegurl"},
        "GET rr1---sn-fixture.googlevideo.com/videoplayback": {"file": "sidx.bin", "status": 206, "type": "video/mp4"},
        "GET rr2---sn-fixture.googlevideo.com/videoplayback": {"status": 403, "type": "text/plain"},
        "GET www.youtube.com/api/timedtext":{"file": "captions.vtt", "type": "text/vtt"},
        "GET suggestqueries-clients6.youtube.com/complete/search": {"url_contains": "q=canned", "file": "suggest.txt", "type": "text/javascript"},
        f"GET i.ytimg.com/sb/{VIDEO}/storyboard3_L1/M0.jpg": {"file": "sheet.jpg", "type": "image/jpeg"},
        f"GET sponsor.ajay.app/api/skipSegments/{prefix(VIDEO)}": {"file": "sponsor.json"},
        f"GET sb.example.org/api/skipSegments/{prefix(VIDEO)}": {"file": "sponsor_alt.json", "tag": "sponsor-alt"},
        "GET returnyoutubedislikeapi.com/votes": [
            {"url_contains": f"videoId={VIDEO}", "file": "dislikes.json", "tag": "dislikes"},
            {"url_contains": f"videoId={LIVE}", "file": "dislikes_other.json", "tag": "dislikes"},
            {"url_contains": f"videoId={SHORT}", "file": "dislikes_float.json", "tag": "dislikes"},
            {"url_contains": f"videoId={DENIED}", "file": "dislikes_large.json", "tag": "dislikes"},
            {"url_contains": f"videoId={GONE}", "status": 500, "file": "captions.vtt", "type": "text/plain", "tag": "dislikes"},
            {"url_contains": f"videoId={ZOO}", "file": "captions.vtt", "type": "text/plain", "tag": "dislikes"}],
        f"GET sponsor.ajay.app/api/branding/{prefix(ZOO)}": {"file": "branding.json"},
        "GET dearrow-thumb.ajay.app/api/v1/getThumbnail": {"file": "sheet.jpg", "type": "image/jpeg"}})


if __name__ == "__main__":
    main()
