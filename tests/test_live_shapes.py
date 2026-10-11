"""Shapes distilled from the Guest live check of 2026-10-11; addresses are synthetic. No network use."""
import json

from prosperotube.models import ResolveRequest
from prosperotube.sessions import Sessions
from prosperotube.worker import compact_captions
from test_companion import ID, info

BASE = "http://127.0.0.1:8765"
TIMEDTEXT = "https://www.youtube.com/api/timedtext?v=" + ID


def test_caption_listing_of_a_dubbed_video_stays_within_the_result_limit():
    # 177 languages x 21 audio tracks x 7 formats, each entry with request headers.
    formats = ("json3", "srv1", "srv2", "srv3", "ttml", "srt", "vtt")
    headers = {"User-Agent": "fixture " * 60, "Cookie": "host-only-cookie"}
    source = info()
    source["automatic_captions"] = {
        f"x{language:03}": [{"ext": ext, "url": f"{TIMEDTEXT}&lang=x{language:03}&variant={track}&fmt={ext}",
                             "name": f"Language {language}", "http_headers": headers}
                            for track in range(21) for ext in formats]
        for language in range(177)}
    source["subtitles"] = {"en": [{"ext": "json3", "url": TIMEDTEXT + "&fmt=json3"},
                                  {"ext": "vtt", "url": TIMEDTEXT + "&fmt=vtt"},
                                  {"ext": "vtt", "url": TIMEDTEXT + "&fmt=vtt&second=1"}],
                           "de": [{"ext": "srt", "url": TIMEDTEXT + "&lang=de"}], "broken": "not a list"}
    source["requested_formats"] = [dict(entry) for entry in source["formats"]]
    assert len(json.dumps(source)) > 8 * 1024 * 1024
    compact_captions(source)
    assert len(json.dumps(source)) < 512 * 1024
    assert "requested_formats" not in source
    assert source["subtitles"] == {"en": [{"ext": "vtt", "url": TIMEDTEXT + "&fmt=vtt"}]}
    assert len(source["automatic_captions"]) == 177
    first = source["automatic_captions"]["x000"]
    assert len(first) == 1 and first[0]["url"].endswith("lang=x000&variant=0&fmt=vtt") and first[0]["name"] == "Language 0"
    # The session lists the same tracks as before the listing was compacted.
    playback = Sessions().create(source, ResolveRequest(id=ID, automatic_captions=True), BASE)
    assert len(playback.captions) == 178 and playback.captions[0].language == "en"
    plain = {"id": ID}
    compact_captions(plain)
    assert plain == {"id": ID}


def test_translations_into_the_requested_language_are_listed_first_and_kept():
    source = info()
    languages = [f"{a}{b}" for a in "abcdefghijklmnopqrstuvwxyz" for b in "abcdefgh"]  # 208 targets, "fr" is not among them
    source["automatic_captions"] = {f"{target}-en": [{"ext": "vtt", "url": f"{TIMEDTEXT}&lang=en&tlang={target}"}]
                                    for target in languages + ["zu", "fr"]}
    playback = Sessions().create(source, ResolveRequest(id=ID, translated_captions=True, caption_language="zu"), BASE)
    listed = [track.language for track in playback.captions]
    assert listed[0] == "zu-en" and listed[1] == "aa-en" and "en" in listed
    assert sum(track.translated for track in playback.captions) == 128
    # A human-made track in the requested language still comes before its translations.
    source["subtitles"]["zu"] = [{"ext": "vtt", "url": TIMEDTEXT + "&lang=zu"}]
    playback = Sessions().create(source, ResolveRequest(id=ID, translated_captions=True, caption_language="zu"), BASE)
    assert [track.language for track in playback.captions][:2] == ["zu", "zu-en"]
