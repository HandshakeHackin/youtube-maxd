#!/usr/bin/env python3
"""Offline test of accounts in `tube_js serve`: device-code sign-in, the
credential file, token refresh, account lists and changes with their
one-attempt guards, history reports, removal and the signed-in playback switch.

    python3 standalone/test/account_test.py [path/to/tube_js]

Google and YouTube are played by canned answers written to a temporary
directory; a request without one fails inside the service. No account, token
or network is involved. SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import os
import re
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import time

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import service_test as base  # noqa: E402

check, same, TOKEN = base.check, base.same, base.TOKEN
ROOT = HERE.parent
CHANNEL, VIDEO, PLAYLIST = base.CHANNEL, base.VIDEO, base.PLAYLIST
OTHER, UPCOMING, AGED, BROKEN = "UC" + "g" * 22, "fixtureUPC1", "fixtureAGE1", "fixtureBRK1"
MINE, THEIRS = "PLmine00000001", "PLtheirs000001"
SPEED = 25   # the sign-in clock runs this much faster, so a five-second poll interval takes 0.2 s
CLIENT = {"client_id": "fixture-client.apps.invalid", "client_secret": "fixture-client-secret"}
SECRETS = ["fixture-client-secret", "device-code-", "access-token-", "refresh-token-"]


def credentials(number, expiry="2099-01-01T00:00:00.000Z"):
    return {"access_token": f"access-token-{number}", "refresh_token": f"refresh-token-{number}", "expiry_date": expiry, "client": CLIENT}


def profile(number, **extra):
    return {"id": f"oauth_{number:016x}", "name": f"Account {number}", "cookie": "", "oauth": credentials(number, **extra)}


def runs(text):
    return {"runs": [{"text": text}]}


def feedback_item(icon, token):
    return {"menuServiceItemRenderer": {"text": {"simpleText": "Localized"}, "icon": {"iconType": icon},
                                        "serviceEndpoint": {"feedbackEndpoint": {"feedbackToken": token}}}}


def video(identifier, title, menu=None, top=None):
    renderer = {"videoId": identifier, "title": runs(title), "lengthText": {"simpleText": "1:00"},
                "thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{identifier}/hq720.jpg", "width": 720, "height": 404}]},
                "ownerText": {"runs": [{"text": "Fixture channel", "navigationEndpoint": {"browseEndpoint": {"browseId": CHANNEL}}}]},
                "navigationEndpoint": {"watchEndpoint": {"videoId": identifier}}}
    if menu or top:
        renderer["menu"] = {"menuRenderer": {"items": menu or [], **({"topLevelButtons": top} if top else {})}}
    return {"videoRenderer": renderer}


def grid(rows):
    return {"contents": {"twoColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"selected": True, "content": {
        "richGridRenderer": {"contents": [{"richItemRenderer": {"content": row}} for row in rows]}}}}]}}}


def sections(rows, side=None):
    return {"contents": {"twoColumnBrowseResultsRenderer": {
        "tabs": [{"tabRenderer": {"selected": True, "content": {"sectionListRenderer": {"contents": [{"itemSectionRenderer": {"contents": rows}}]}}}}],
        **({"secondaryContents": side} if side else {})}}}


def control(icon, token):
    return {"buttonRenderer": {"text": runs("Localized"), "icon": {"iconType": icon}, "navigationEndpoint": {"confirmDialogEndpoint": {"content": {
        "confirmDialogRenderer": {"title": runs("Title"), "dialogMessages": [runs("Question")], "cancelButton": {"buttonRenderer": {"text": {"simpleText": "No"}}},
                                  "confirmButton": {"buttonRenderer": {"text": {"simpleText": "Yes"}, "serviceEndpoint": {
                                      "commandMetadata": {"webCommandMetadata": {"sendPost": True, "apiUrl": "/youtubei/v1/feedback"}},
                                      "feedbackEndpoint": {"feedbackToken": token}}}}}}}}}}


def playlist_page(identifier, title, editable=False):
    def row(n):
        name = f"listed{n:05d}"
        return {"playlistVideoRenderer": {
            "videoId": name, "setVideoId": f"SET{n}", "index": {"simpleText": str(n + 1)},
            "title": {**runs(f"{title} video {n}"), "accessibility": {"accessibilityData": {"label": f"{title} video {n}"}}},
            "shortBylineText": {"runs": [{"text": "Fixture channel", "navigationEndpoint": {"browseEndpoint": {"browseId": CHANNEL}}}]},
            "lengthSeconds": "60", "lengthText": {"simpleText": "1:00"}, "isPlayable": True,
            "thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{name}/hqdefault.jpg", "width": 480, "height": 360}]},
            "navigationEndpoint": {"watchEndpoint": {"videoId": name, "playlistId": identifier, "index": n}}}}

    def order(value, selected=False):
        return {"title": "Localized", "selected": selected, "serviceEndpoint": {"playlistEditEndpoint": {
            "playlistId": identifier, "actions": [{"action": "ACTION_SET_PLAYLIST_VIDEO_ORDER", "playlistVideoOrder": value}]}}}
    listing = {"playlistId": identifier, "isEditable": editable, "contents": [row(n) for n in range(2)]}
    if editable:
        listing["sortFilterMenu"] = {"sortFilterSubMenuRenderer": {"subMenuItems": [order(1, True), order(2), order(3)]}}
    return {"header": {"playlistHeaderRenderer": {"playlistId": identifier, "title": {"simpleText": title}, "isEditable": editable}},
            "contents": {"twoColumnBrowseResultsRenderer": {"tabs": [{"tabRenderer": {"selected": True, "content": {
                "sectionListRenderer": {"contents": [{"itemSectionRenderer": {"contents": [{"playlistVideoListRenderer": listing}]}}]}}}}]}}}


def notifications():
    def notice(n, read, endpoint):
        return {"notificationRenderer": {"notificationId": f"private-notification-{n}", "read": read, "shortMessage": {"simpleText": f"Uploaded: video {n}"},
                                         "sentTimeText": {"simpleText": f"{n} hours ago"},
                                         "thumbnail": {"thumbnails": [{"url": "https://yt3.ggpht.com/avatar", "width": 48, "height": 48}]},
                                         "videoThumbnail": {"thumbnails": [{"url": "https://i.ytimg.com/vi/x/default.jpg", "width": 120, "height": 90}]},
                                         "navigationEndpoint": endpoint}}
    return {"actions": [{"openPopupAction": {"popup": {"multiPageMenuRenderer": {
        "header": {"simpleMenuHeaderRenderer": {"title": {"simpleText": "Notifications"}}},
        "sections": [{"multiPageMenuNotificationSectionRenderer": {"items": [
            notice(1, False, {"watchEndpoint": {"videoId": "notified001"}}), notice(2, True, {"watchEndpoint": {"videoId": "notified002"}}),
            notice(3, False, {"browseEndpoint": {"browseId": "FEcomments"}})]}}]}}}}]}


def tile(identifier, title):
    return {"tileRenderer": {"contentType": "TILE_CONTENT_TYPE_VIDEO", "contentId": identifier, "onSelectCommand": {"watchEndpoint": {"videoId": identifier}},
                             "header": {"tileHeaderRenderer": {"thumbnail": {"thumbnails": [{"url": f"https://i.ytimg.com/vi/{identifier}/default.jpg"}]},
                                                               "thumbnailOverlays": [{"thumbnailOverlayTimeStatusRenderer": {"text": {"simpleText": "2:03"}}}]}},
                             "metadata": {"tileMetadataRenderer": {"title": {"simpleText": title}, "lines": []}}}}


def comments():
    def thread(n):
        return {"commentThreadRenderer": {"commentViewModel": {"commentViewModel": {
            "commentId": f"c{n}", "commentKey": f"k{n}", "toolbarStateKey": f"ts{n}", "toolbarSurfaceKey": f"tf{n}", "commentSurfaceKey": f"cs{n}", "sharedKey": f"sh{n}"}}}}

    def command(action):
        return {"innertubeCommand": {"performCommentActionEndpoint": {"action": action}}}

    def mutations(n):
        return [
            {"entityKey": f"k{n}", "payload": {"commentEntityPayload": {
                "key": f"k{n}", "properties": {"commentId": f"c{n}", "content": {"content": f"Comment {n}"}, "publishedTime": "1 day ago", "replyLevel": 0},
                "author": {"channelId": CHANNEL, "displayName": "@viewer", "avatarThumbnailUrl": "https://yt3.ggpht.com/a", "isCreator": False},
                "toolbar": {"likeCountNotliked": "5", "likeCountLiked": "6", "replyCount": "0"}}}},
            {"entityKey": f"ts{n}", "payload": {"engagementToolbarStateEntityPayload": {
                "key": f"ts{n}", "likeState": "TOOLBAR_LIKE_STATE_INDIFFERENT", "heartState": "TOOLBAR_HEART_STATE_UNHEARTED"}}},
            {"entityKey": f"tf{n}", "payload": {"engagementToolbarSurfaceEntityPayload": {
                "key": f"tf{n}", "likeCommand": command(f"private-like-{n}"), "dislikeCommand": command(f"private-dislike-{n}"),
                "unlikeCommand": command("unlike"), "undislikeCommand": command("undislike"),
                "replyCommand": {"innertubeCommand": {"createCommentReplyDialogEndpoint": {"dialog": {"commentReplyDialogRenderer": {
                    "replyButton": {"buttonRenderer": {"text": {"simpleText": "Reply"}, "serviceEndpoint": {
                        "commandMetadata": {"webCommandMetadata": {"sendPost": True, "apiUrl": "/youtubei/v1/comment/create_comment_reply"}},
                        "createCommentReplyEndpoint": {"createReplyParams": f"private-reply-{n}"}}}},
                    "cancelButton": {"buttonRenderer": {"text": {"simpleText": "Cancel"}}}, "placeholderText": {"simpleText": "Add a reply"},
                    "errorMessage": {"simpleText": "e"}}}}}}}}}]
    return {"onResponseReceivedEndpoints": [
        {"reloadContinuationItemsCommand": {"targetId": "comments-header", "continuationItems": [{"commentsHeaderRenderer": {
            "countText": {"runs": [{"text": "2"}, {"text": " Comments"}]}, "commentsCount": {"simpleText": "2"},
            "createRenderer": {"commentSimpleboxRenderer": {
                "submitButton": {"buttonRenderer": {"text": {"simpleText": "Comment"}, "serviceEndpoint": {
                    "commandMetadata": {"webCommandMetadata": {"sendPost": True, "apiUrl": "/youtubei/v1/comment/create_comment"}},
                    "createCommentEndpoint": {"createCommentParams": "private-create"}}}},
                "cancelButton": {"buttonRenderer": {"text": {"simpleText": "Cancel"}}}, "placeholderText": {"simpleText": "Add"},
                "authorThumbnail": {"thumbnails": []}}}}}]}},
        {"reloadContinuationItemsCommand": {"targetId": "comments-section", "continuationItems": [thread(1), thread(2)]}}],
        "frameworkUpdates": {"entityBatchUpdate": {"mutations": [*mutations(1), *mutations(2)]}}}


def fixtures(extra_index=None, files=None):
    """The Guest fixtures plus what an account needs, in a new directory."""
    directory = Path(tempfile.mkdtemp(prefix="tube_js-account-fixtures-"))
    for path in (ROOT / "fixtures").iterdir():
        if path.is_file() and path.name != "make.py":
            shutil.copyfile(path, directory / path.name)
    index = json.loads((directory / "index.json").read_text())
    player = json.loads((directory / "player.json").read_text())

    def write(name, value):
        (directory / name).write_text(value if isinstance(value, str) else json.dumps(value), encoding="utf-8")
        return name

    def tracked(identifier):
        answer = json.loads(json.dumps(player).replace(VIDEO, identifier))
        answer["playbackTracking"] = {"videostatsPlaybackUrl": {"baseUrl": f"https://s.youtube.com/api/stats/playback?docid={identifier}&ei=fixture"},
                                      "videostatsWatchtimeUrl": {"baseUrl": f"https://s.youtube.com/api/stats/watchtime?docid={identifier}&ei=fixture"}}
        return answer
    signed = "authorization: Bearer "
    toggle = {"toggleButtonRenderer": {"isToggled": False, "defaultText": {"simpleText": "Localized"},
                                       "defaultServiceEndpoint": {"addUpcomingEventReminderEndpoint": {"params": "private-add"}},
                                       "toggledServiceEndpoint": {"removeUpcomingEventReminderEndpoint": {"params": "private-remove"}}}}
    write("tv.html", '<html><script id="base-js" src="/s/_/kabuki_legacy/fixture/base.js" nonce="x"></script></html>')
    write("tv.js", 'var a={clientId:"%s",uq:"%s"};' % (CLIENT["client_id"], CLIENT["client_secret"]))
    account = {
        "GET www.youtube.com/tv": {"file": "tv.html", "type": "text/html"},
        "GET www.youtube.com/s/_/kabuki_legacy/fixture/base.js": {"file": "tv.js", "type": "text/javascript"},
        "POST www.youtube.com/o/oauth2/device/code": {"file": write("device.json", {
            "device_code": "device-code-1", "user_code": "ABCD-EFGH", "verification_url": "https://www.google.com/device", "expires_in": 1800, "interval": 5})},
        "POST www.youtube.com/youtubei/v1/browse": [
            {"header_contains": signed, "body_contains": '"browseId":"default"', "tag": "home", "file": write("a_home.json", grid([
                video("homevideo01", "Recommended one", [feedback_item("NOT_INTERESTED", "private-not-interested"), feedback_item("REMOVE", "private-channel")]),
                video("homevideo02", "Recommended two")]))},
            {"header_contains": signed, "body_contains": '"browseId":"FEsubscriptions"', "tag": "subscriptions", "file": write("a_subscriptions.json", grid([
                video("subscribed1", "From a subscription", [feedback_item("HIDE", "private-hide")]), video("subscribed2", "Another upload")]))},
            {"header_contains": signed, "body_contains": '"browseId":"FEchannels"', "tag": "channels", "file": write("a_channels.json", sections([
                {"channelRenderer": {"channelId": CHANNEL, "title": {"simpleText": "Fixture channel"},
                                     "thumbnail": {"thumbnails": [{"url": "https://yt3.ggpht.com/avatar=s88", "width": 88, "height": 88}]},
                                     "navigationEndpoint": {"browseEndpoint": {"browseId": CHANNEL}}}}]))},
            {"header_contains": signed, "body_contains": '"browseId":"FEhistory"', "tag": "history", "file": write("a_history.json", sections(
                [video("watched0001", "Watched before", top=[{"buttonRenderer": {"icon": {"iconType": "DISMISSAL"}, "serviceEndpoint": {
                    "feedbackEndpoint": {"feedbackToken": "private-remove-history"}}}}]), video("watched0002", "Watched too")],
                {"browseFeedActionsRenderer": {"contents": [control("DELETE", "private-clear"), control("PAUSE", "private-pause")]}}))},
            {"header_contains": signed, "body_contains": '"browseId":"FEplaylist_aggregation"', "tag": "playlists", "file": write("a_playlists.json", grid([
                {"lockupViewModel": {"contentId": MINE, "contentType": "LOCKUP_CONTENT_TYPE_PLAYLIST",
                                     "metadata": {"lockupMetadataViewModel": {"title": {"content": "My playlist"}}}}}]))},
            {"header_contains": signed, "body_contains": '"browseId":"FElibrary"', "tag": "library", "file": write("a_library.json", sections([]))},
            {"header_contains": signed, "body_contains": '"browseId":"VLWL"', "tag": "watch_later", "file": write("a_wl.json", playlist_page("WL", "Watch later", True))},
            {"header_contains": signed, "body_contains": '"browseId":"VLLL"', "tag": "liked", "file": write("a_ll.json", playlist_page("LL", "Liked videos"))},
            {"header_contains": signed, "body_contains": f'"browseId":"VL{MINE}"', "tag": "mine", "file": write("a_mine.json", playlist_page(MINE, "My playlist", True))},
            {"header_contains": signed, "body_contains": '"browseId":"FEmy_videos"', "tag": "my_videos", "file": write("a_my_videos.json", {
                "contents": {"tvBrowseRenderer": {"content": {"tvSurfaceContentRenderer": {"content": {"gridRenderer": {
                    "items": [tile("uploaded001", "My upload")]}}}}}}})},
            *index["POST www.youtube.com/youtubei/v1/browse"]],
        "POST www.youtube.com/youtubei/v1/notification/get_unseen_count": {"header_contains": signed, "file": write("a_notification_count.json", {"unseenCount": 13})},
        "POST www.youtube.com/youtubei/v1/notification/get_notification_menu": {"header_contains": signed, "file": write("a_notifications.json", notifications())},
        "POST www.youtube.com/youtubei/v1/player": [
            {"header_contains": signed, "body_contains": f'"videoId":"{VIDEO}"', "tag": "account player", "file": write("a_player.json", tracked(VIDEO))},
            {"header_contains": signed, "body_contains": f'"videoId":"{BROKEN}"', "tag": "account player", "file": write("a_player_broken.json", tracked(BROKEN))},
            {"body_contains": f'"videoId":"{UPCOMING}"', "file": write("a_upcoming.json", {
                "videoDetails": {"videoId": UPCOMING, "isUpcoming": True}, "playabilityStatus": {"status": "LIVE_STREAM_OFFLINE", "liveStreamability": {
                    "liveStreamabilityRenderer": {"offlineSlate": {"liveStreamOfflineSlateRenderer": {"actionButtons": [toggle]}}}}}})},
            {"header_contains": signed, "body_contains": f'"videoId":"{AGED}"', "tag": "account player", "file": write("a_player_aged.json",
                json.loads(json.dumps(player).replace(VIDEO, AGED)))},
            {"body_contains": f'"videoId":"{AGED}"', "tag": "guest player", "file": write("a_player_login.json", {
                "playabilityStatus": {"status": "LOGIN_REQUIRED", "reason": "Sign in to confirm your age"}, "videoDetails": {"videoId": AGED}})},
            *index["POST www.youtube.com/youtubei/v1/player"]],
        "POST www.youtube.com/youtubei/v1/next": [
            {"header_contains": signed, "body_contains": '"continuation"', "tag": "comments", "file": write("a_comments.json", comments())},
            *index["POST www.youtube.com/youtubei/v1/next"]],
        "GET www.youtube.com/api/stats/playback": [{"url_contains": BROKEN, "status": 500}, {"status": 204}],
        "GET www.youtube.com/api/stats/watchtime": [{"url_contains": "st=77.000", "status": 500}, {"status": 204}],
        "POST www.youtube.com/youtubei/v1/like/like": [{"body_contains": BROKEN, "status": 500, "file": write("empty.json", {})}, {"file": "empty.json"}],
        "POST www.youtube.com/youtubei/v1/like/dislike": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/like/removelike": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/subscription/subscribe": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/subscription/unsubscribe": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/notification/modify_channel_preference": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/notification/add_upcoming_event_reminder": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/playlist/create": {"file": write("a_created.json", {"playlistId": "PLcreated00001"})},
        "POST www.youtube.com/youtubei/v1/playlist/delete": {"file": "empty.json"},
        "POST www.youtube.com/youtubei/v1/browse/edit_playlist": {"file": write("a_edited.json", {"status": "STATUS_SUCCEEDED", "actions": [{"action": "done"}]})},
        "POST www.youtube.com/youtubei/v1/feedback": [
            {"body_contains": "private-channel", "file": write("a_unprocessed.json", {"feedbackResponses": [{"isProcessed": False}]})},
            {"file": write("a_processed.json", {"feedbackResponses": [{"isProcessed": True}]})}],
        "POST www.youtube.com/youtubei/v1/comment/create_comment": [
            {"body_contains": "refuse me", "file": write("a_comment_refused.json", {"actionResult": {"status": "STATUS_FAILED"}})},
            {"file": write("a_comment.json", {"actionResult": {"status": "STATUS_SUCCEEDED"}})}],
        "POST www.youtube.com/youtubei/v1/comment/create_comment_reply": {"file": "a_comment.json"},
        "POST www.youtube.com/youtubei/v1/comment/perform_comment_action": [
            {"body_contains": "private-like-2", "status": 500, "file": "empty.json"}, {"file": "empty.json"}],
    }
    index.update(account)
    for name, value in (files or {}).items():
        write(name, value)
    index.update(extra_index or {})
    write("index.json", index)
    return directory


class Service(base.Service):
    def __init__(self, binary, directory, *options, state=None, stored=None):
        self.state = state or tempfile.mkdtemp(prefix="tube_js-account-")
        if stored is not None:
            path = Path(self.state) / "accounts"
            path.mkdir(parents=True, exist_ok=True)
            (path / "oauth-profiles.json").write_bytes(stored if isinstance(stored, bytes) else json.dumps({"version": 1, "profiles": stored}).encode())
        environment = {**os.environ, "TUBE_JS_FIXTURES": str(directory), "TUBE_JS_AUTH_CLOCK": str(SPEED),
                       "ASAN_OPTIONS": "detect_leaks=1:abort_on_error=0", "UBSAN_OPTIONS": "print_stacktrace=1"}
        self.process = subprocess.Popen([binary, "serve", "--host", "127.0.0.1", "--port", "0", "--state-dir", self.state, "--token", TOKEN, *options],
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=environment, text=True)
        line = self.process.stdout.readline()
        if not line:
            raise SystemExit("the service did not start: " + self.process.stderr.read()[-2000:])
        self.base = json.loads(line)["listening"]
        self.port = int(self.base.rsplit(":", 1)[1])
        self.seen = []   # every answer body, to prove no credential leaves the service

    def call(self, *args, **kwargs):
        answer = super().call(*args, **kwargs)
        self.seen.append(json.dumps(answer[2]) if not isinstance(answer[2], bytes) else answer[2].decode("utf-8", "replace"))
        return answer

    def store(self):
        return Path(self.state) / "accounts" / "oauth-profiles.json"

    def stored(self):
        return json.loads(self.store().read_text())["profiles"]

    def upstream(self, clear=True):
        return super().call("GET", "/v1/_upstream" + ("?clear=1" if clear else ""))[2]["requests"]

    def sent(self, key, clear=True):
        """How many upstream requests matched `key` (a path fragment or a fixture tag) since the last look."""
        return sum(1 for request in self.upstream(clear) if key in request["key"] or key == request["tag"])

    def post(self, **request):
        status, _, body = self.call("POST", "/v1/library", request)
        return status, body

    def error(self, **request):
        status, body = self.post(**request)
        return status, (body or {}).get("error", {}).get("code")

    def finish(self, name):
        code, errors = self.stop()
        diagnostic = re.compile(r"YouTube (?:browse shape: action=(?:home|history|library|playlists|subscriptions|watch_later|playlist) tiles=[0-9]+ mode=(?:tv items=[0-9]+|legacy)|browse shape: action=channels tabs=[0-9]+ channels=[0-9]+ mode=(?:tv|legacy)|operation failed: stage=(?:execute|device|client|playback|other) action=(?:history|library|playlists|watch_later|playlist|home|subscriptions|channels|notifications|notification_count|start|poll|continue|search|search_suggestions|suggestions|suggestions_filter|channel|channel_search|video_details|description|comments|topics|topic_open|topic_videos|other) category=(?:TypeError|ReferenceError|SyntaxError|RangeError|InnertubeError|ParsingError|OAuth2Error|SessionError|Error|(?:LibraryError|ServiceError)(?: code=[a-z][a-z_]{0,39})?))")
        unexpected = "\n".join(line for line in errors.splitlines() if not diagnostic.fullmatch(line))
        same(f"clean shutdown ({name})", (code, unexpected.strip()), (0, ""))
        same(f"no credentials in diagnostics ({name})", [secret for secret in SECRETS if secret in errors], [])
        leaked = [secret for secret in SECRETS if any(secret in body for body in self.seen)]
        same(f"no credential in any answer ({name})", leaked, [])


def wait(seconds):
    time.sleep(seconds / SPEED + 0.03)


def sign_in(binary):
    grant = lambda name, body, status=200: {"body_contains": "grant_type/device", "times": 1, "status": status, "file": name, "_body": body}  # noqa: E731
    sequence = [
        grant("t_pending.json", {"error": "authorization_pending"}, 428),
        grant("t_slow.json", {"error": "slow_down"}, 403),
        grant("t_ok1.json", {"access_token": "access-token-1", "refresh_token": "refresh-token-1", "expires_in": 3600, "token_type": "Bearer"}),
        grant("t_denied.json", {"error": "access_denied"}, 403),
        grant("t_expired.json", {"error": "expired_token"}, 400),
        grant("t_bad.json", {"access_token": "access-token-9", "refresh_token": "refresh-token-9"}),
        grant("t_ok2.json", {"access_token": "access-token-2", "refresh_token": "refresh-token-2", "expires_in": 3600}),
    ]
    files = {entry["file"]: entry.pop("_body") for entry in sequence}
    directory = fixtures({"POST www.youtube.com/o/oauth2/token": sequence}, files)
    s = Service(binary, directory)
    try:
        health = s.call("GET", "/v1/health", token=None)[2]
        check("health offers sign-in", "device_auth" in health["features"] and "library" in health["features"], health["features"])
        same("health: signed-in playback is off", health["account_playback"], False)
        same("no account yet", s.post(action="profiles")[1]["profiles"], [{"id": "guest", "name": "Guest", "account_configured": False}])
        check("no credential file before a sign-in", not s.store().exists())
        same("start: extra field", s.call("POST", "/v1/auth/start", {"name": "A", "more": 1})[0], 422)
        same("start: long name", s.call("POST", "/v1/auth/start", {"name": "x" * 151})[0], 422)
        same("start: empty name", s.call("POST", "/v1/auth/start", {"name": ""})[0], 422)
        same("start: control character", s.call("POST", "/v1/auth/start", {"name": "a\u0001b"})[::2],
             (400, {"api_version": 1, "error": {"code": "auth_name", "message": "Enter a valid account name."}}))
        same("status: bad session shape", s.call("POST", "/v1/auth/status", {"session": "nope"})[0], 422)
        same("status: unknown session", s.call("POST", "/v1/auth/status", {"session": "0" * 32})[::2],
             (404, {"api_version": 1, "error": {"code": "auth_session", "message": "Start a new sign-in session."}}))
        same("nothing was asked upstream so far", s.upstream(), [])

        status, _, started = s.call("POST", "/v1/auth/start", {"name": "  Living room  "})
        session = started.get("session", "")
        same("start", (status, {**started, "session": "x"}), (200, {"session": "x", "state": "pending", "user_code": "ABCD-EFGH",
             "verification_url": "https://www.google.com/device", "expires_in": 1800, "poll_after": 5}))
        check("session shape", len(session) == 32 and all(c in "0123456789abcdef" for c in session), session)
        same("start asked for the client identity and one code", [r["key"] for r in s.upstream()],
             ["GET www.youtube.com/tv", "GET www.youtube.com/s/_/kabuki_legacy/fixture/base.js", "POST www.youtube.com/o/oauth2/device/code"])
        same("one sign-in at a time", s.call("POST", "/v1/auth/start", {})[::2],
             (409, {"api_version": 1, "error": {"code": "auth_busy", "message": "A sign-in session is already pending."}}))
        same("status before the interval polls nothing", (s.call("POST", "/v1/auth/status", {"session": session})[2]["state"], s.sent("oauth2/token")), ("pending", 0))
        wait(5)
        answer = s.call("POST", "/v1/auth/status", {"session": session})[2]
        same("authorization pending", (answer["state"], answer["poll_after"], s.sent("oauth2/token")), ("pending", 5, 1))
        same("a second status inside the interval polls nothing", (s.call("POST", "/v1/auth/status", {"session": session})[2]["state"], s.sent("oauth2/token")), ("pending", 0))
        wait(5)
        answer = s.call("POST", "/v1/auth/status", {"session": session})[2]
        same("slow_down lengthens the interval", (answer["state"], answer["poll_after"], s.sent("oauth2/token")), ("pending", 10, 1))
        wait(5)
        same("the longer interval is kept", (s.call("POST", "/v1/auth/status", {"session": session})[2]["state"], s.sent("oauth2/token")), ("pending", 0))
        wait(5)
        status, _, answer = s.call("POST", "/v1/auth/status", {"session": session})
        first = answer.get("profile", {}).get("id", "")
        same("authorized", (status, answer), (200, {"session": session, "state": "authorized", "profile": {"id": first, "name": "Living room"}}))
        check("profile identity", len(first) == 22 and first.startswith("oauth_"), first)
        same("one token request did it", s.sent("oauth2/token"), 1)
        same("authorized is final and repeatable", (s.call("POST", "/v1/auth/status", {"session": session})[2], s.call("POST", "/v1/auth/cancel", {"session": session})[2]["state"],
             s.sent("oauth2/token")), (answer, "authorized", 0))
        # The credential file.
        mode = lambda path: stat.S_IMODE(os.stat(path).st_mode)  # noqa: E731
        stored = s.stored()
        same("credential file", stored, [{"id": first, "name": "Living room", "cookie": "", "oauth": {
            "access_token": "access-token-1", "refresh_token": "refresh-token-1", "expiry_date": stored[0]["oauth"]["expiry_date"], "client": CLIENT}}])
        same("credential file modes", (oct(mode(s.store())), oct(mode(s.store().parent))), ("0o600", "0o700"))
        check("no temporary file is left", sorted(p.name for p in s.store().parent.iterdir()) == ["oauth-profiles.json"], list(s.store().parent.iterdir()))
        others = [p for p in Path(s.state).rglob("*") if p.is_file() and p != s.store()]
        check("tokens are in no other file", not any(b"-token-" in p.read_bytes() or b"fixture-client-secret" in p.read_bytes() for p in others), others)
        same("the account is a profile at once", s.post(action="profiles")[1]["profiles"],
             [{"id": "guest", "name": "Guest", "account_configured": False}, {"id": first, "name": "Living room", "account_configured": True}])
        same("and its lists answer", (s.post(action="subscriptions", profile=first)[1]["items"][0]["id"], s.sent("subscriptions")), ("subscribed1", 1))

        # The same through jobs, as the app does; a cancelled sign-in frees the slot.
        status, started = s.job({"action": "auth", "auth": {"action": "start", "name": "Second"}})
        second = started["session"]
        same("job start", (status, started["state"], started["user_code"]), (200, "pending", "ABCD-EFGH"))
        same("job without an operation", s.call("POST", "/v1/jobs", {"action": "auth"})[2]["error"]["code"], "invalid_job")
        same("job status without a session", s.call("POST", "/v1/jobs", {"action": "auth", "auth": {"action": "status"}})[2]["error"]["code"], "invalid_job")
        same("job cancel", s.job({"action": "auth", "auth": {"action": "cancel", "session": second}})[1], {"session": second, "state": "canceled"})
        wait(5)
        same("a cancelled session never polls", (s.job({"action": "auth", "auth": {"action": "status", "session": second}})[1]["state"], s.sent("oauth2/token")), ("canceled", 0))
        # A start nobody read: deleting the job withdraws the sign-in.
        created = s.call("POST", "/v1/jobs", {"action": "auth", "auth": {"action": "start"}})[2]
        time.sleep(0.2)
        same("abandoned start job", s.call("DELETE", "/v1/jobs/" + created["job"])[2], {"state": "cancelled"})
        # Denied on the phone.
        status, _, started = s.call("POST", "/v1/auth/start", {})
        same("a new sign-in can start after the abandoned one", (status, started.get("state")), (200, "pending"))
        wait(5)
        same("denied", s.call("POST", "/v1/auth/status", {"session": started["session"]})[2], {"session": started["session"], "state": "denied"})
        # The code expired at Google.
        started = s.call("POST", "/v1/auth/start", {})[2]
        wait(5)
        same("expired upstream", s.call("POST", "/v1/auth/status", {"session": started["session"]})[2], {"session": started["session"], "state": "expired"})
        # A token answer that cannot be used ends the session; it is not asked again.
        started = s.call("POST", "/v1/auth/start", {})[2]
        wait(5)
        s.upstream()
        same("unusable token answer", s.call("POST", "/v1/auth/status", {"session": started["session"]})[::2],
             (502, {"api_version": 1, "error": {"code": "auth_failed", "message": "YouTube sign-in could not be completed. Start a new session."}}))
        wait(5)
        same("failed is final", (s.call("POST", "/v1/auth/status", {"session": started["session"]})[2], s.sent("oauth2/token")),
             ({"session": started["session"], "state": "failed", "error": "YouTube sign-in could not be completed. Start a new session."}, 1))
        same("still one stored account", [p["id"] for p in s.stored()], [first])
        # A second account, signed in through jobs.
        started = s.job({"action": "auth", "auth": {"action": "start", "name": "Second"}})[1]
        wait(5)
        answer = s.job({"action": "auth", "auth": {"action": "status", "session": started["session"]}})[1]
        other = answer.get("profile", {}).get("id", "")
        same("second account", (answer["state"], answer["profile"]["name"], [p["id"] for p in s.stored()]), ("authorized", "Second", [first, other]))
        same("each account answers with its own token", (s.post(action="history", profile=other)[0], s.sent("history")), (200, 1))
    finally:
        s.finish("sign-in")

    # A new start of the app: the accounts are there, the sign-in sessions are not.
    again = Service(binary, directory, state=s.state)
    try:
        same("accounts survive a restart", [p["id"] for p in again.post(action="profiles")[1]["profiles"]], ["guest", first, other])
        same("sign-in sessions do not", again.call("POST", "/v1/auth/status", {"session": session})[2]["error"]["code"], "auth_session")
        same("lists work after the restart", again.post(action="subscriptions", profile=first)[1]["items"][1]["id"], "subscribed2")
    finally:
        again.finish("restart")
    shutil.rmtree(directory)


def sign_in_edges(binary):
    """A code that runs out locally, a start that fails upstream, a full store, an unreadable store."""
    directory = fixtures({"POST www.youtube.com/o/oauth2/device/code": [
        {"times": 1, "file": "short.json"}, {"times": 1, "status": 500, "file": "empty.json"}, {"times": 1, "file": "odd.json"}, {"file": "device.json"}],
        "POST www.youtube.com/o/oauth2/token": []},
        {"short.json": {"device_code": "device-code-2", "user_code": "WXYZ-1234", "verification_url": "https://www.google.com/device", "expires_in": 20, "interval": 5},
         "odd.json": {"device_code": "device-code-3", "user_code": "WXYZ-1234", "verification_url": "https://example.org/device", "expires_in": 600, "interval": 5}})
    s = Service(binary, directory)
    try:
        started = s.call("POST", "/v1/auth/start", {})[2]
        same("short-lived code", (started["expires_in"], started["poll_after"]), (20, 5))
        wait(21)
        s.upstream()
        same("expired locally without asking", (s.call("POST", "/v1/auth/status", {"session": started["session"]})[2], s.sent("oauth2/token")),
             ({"session": started["session"], "state": "expired"}, 0))
        same("start refused upstream", s.call("POST", "/v1/auth/start", {})[::2],
             (502, {"api_version": 1, "error": {"code": "auth_failed", "message": "YouTube sign-in could not be started. Start a new session."}}))
        same("a verification address elsewhere is refused", s.call("POST", "/v1/auth/start", {})[2]["error"]["code"], "auth_failed")
        same("and the next start works", s.call("POST", "/v1/auth/start", {})[2]["state"], "pending")
    finally:
        s.finish("sign-in edges")

    full = Service(binary, directory, stored=[profile(n) for n in range(1, 9)])
    try:
        same("eight accounts load", len(full.post(action="profiles")[1]["profiles"]), 9)
        same("a ninth is refused before anything is asked", (full.call("POST", "/v1/auth/start", {})[::2], full.upstream()),
             ((409, {"api_version": 1, "error": {"code": "auth_profiles_full", "message": "At most eight account profiles are supported."}}), []))
    finally:
        full.finish("full store")

    damaged = b'{"version": 1, "profiles": [{"id": "oauth_0000000000000001", "name": "Cut off", "cook'
    for name, content in (("damaged", damaged), ("unknown version", json.dumps({"version": 2, "profiles": []}).encode()),
                          ("extra field", json.dumps({"version": 1, "profiles": [{**profile(1), "note": "x"}]}).encode())):
        broken = Service(binary, directory, stored=content)
        try:
            same(f"{name} store: Guest only", broken.post(action="profiles")[1]["profiles"], [{"id": "guest", "name": "Guest", "account_configured": False}])
            same(f"{name} store: sign-in says so", broken.call("POST", "/v1/auth/start", {})[::2], (507, {"api_version": 1, "error": {
                "code": "auth_storage", "message": "The account store on this console could not be read and was left untouched."}}))
            same(f"{name} store: Guest still browses", broken.job({"action": "search", "query": "canned"})[0], 200)
            same(f"{name} store: the file is untouched", broken.store().read_bytes(), content)
        finally:
            broken.finish(name + " store")
    shutil.rmtree(directory)



def storage_failure(binary):
    """Phone approval succeeds, but credential persistence fails without hiding why."""
    directory = fixtures({"POST www.youtube.com/o/oauth2/token": [{"file": "authorized.json"}]},
                         {"authorized.json": {"access_token": "access-token-1", "refresh_token": "refresh-token-1", "expires_in": 3600}})
    s = Service(binary, directory)
    try:
        started = s.call("POST", "/v1/auth/start", {})[2]
        # A directory at the temporary-file name makes open fail even as root.
        blocked = Path(str(s.store()) + ".tmp")
        blocked.mkdir(parents=True)
        wait(5)
        message = "The account could not be written to private storage."
        same("authorized credentials report storage failure", s.call("POST", "/v1/auth/status", {"session": started["session"]})[::2],
             (507, {"api_version": 1, "error": {"code": "auth_storage", "message": message}}))
        same("storage failure is retained on status", s.call("POST", "/v1/auth/status", {"session": started["session"]})[2],
             {"session": started["session"], "state": "failed", "error": message})
        same("failed persistence never registers an account", s.post(action="profiles")[1]["profiles"],
             [{"id": "guest", "name": "Guest", "account_configured": False}])
        same("failed session does not redeem twice", s.sent("oauth2/token"), 1)
        check("no credential file after failure", not s.store().exists())
    finally:
        s.finish("credential write failure")
        shutil.rmtree(directory)


def refresh(binary):
    refreshed = {"body_contains": '"grant_type":"refresh_token"', "file": "refreshed.json"}
    directory = fixtures({"POST www.youtube.com/o/oauth2/token": [
        {**refreshed, "body_contains": ['"grant_type":"refresh_token"', "refresh-token-2"], "status": 400, "file": "revoked.json"}, refreshed]},
        {"refreshed.json": {"access_token": "access-token-fresh", "expires_in": 3600}, "revoked.json": {"error": "invalid_grant"}})
    index = json.loads((directory / "index.json").read_text())
    # Only the refreshed token opens the subscriptions list here.
    for entry in index["POST www.youtube.com/youtubei/v1/browse"]:
        if entry.get("tag") == "subscriptions":
            entry["header_contains"] = "authorization: Bearer access-token-fresh"
    (directory / "index.json").write_text(json.dumps(index))
    s = Service(binary, directory, stored=[profile(1, expiry="2020-01-01T00:00:00.000Z"), profile(2, expiry="2020-01-01T00:00:00.000Z")])
    try:
        one, two = profile(1)["id"], profile(2)["id"]
        same("an expired token is refreshed before the first request", s.post(action="subscriptions", profile=one)[1]["items"][0]["id"], "subscribed1")
        log = s.upstream()
        same("refresh, then the list with the new token", [(r["key"].split("/")[-1], r["bearer"]) for r in log], [("token", False), ("browse", True)])
        kept = s.stored()[0]["oauth"]
        same("the new token is stored, the rest kept", (kept["access_token"], kept["refresh_token"], kept["client"], kept["expiry_date"] > "2026"),
             ("access-token-fresh", "refresh-token-1", CLIENT, True))
        same("no second refresh while it lasts", (s.post(action="subscriptions", profile=one)[0], s.sent("oauth2/token")), (200, 0))
        same("a sign-in Google no longer accepts", s.post(action="subscriptions", profile=two), (403, {"api_version": 1, "error": {
            "code": "authentication_required", "message": "Sign in to the YouTube account again."}}))
        same("the refused account keeps its stored record", s.stored()[1]["oauth"]["access_token"], "access-token-2")
    finally:
        s.finish("refresh")
    shutil.rmtree(directory)


def lists_and_changes(binary):
    directory = fixtures()
    one, two = profile(1)["id"], profile(2)["id"]
    s = Service(binary, directory, stored=[profile(1), profile(2)])
    try:
        ids = lambda page: [row["id"] for row in page["items"]]  # noqa: E731
        # ---- lists ----
        status, page = s.library(action="home", profile=one)
        same("home", (status, ids(page)), (200, ["homevideo01", "homevideo02"]))
        cursor = page["items"][0].get("feedback_cursor", "")
        check("home cards carry feedback references, not tokens", cursor and page["items"][0]["feedback_not_interested"] and "private-" not in json.dumps(page), page["items"][0])
        status, page = s.library(action="subscriptions", profile=one)
        same("subscriptions feed", (status, ids(page), page["items"][0].get("feedback_hide")), (200, ["subscribed1", "subscribed2"], True))
        hide = page["items"][0]["feedback_cursor"]
        status, page = s.library(action="channels", profile=one)
        same("subscribed channels", (status, [(row.get("channel_id"), row.get("title")) for row in page["items"]]), (200, [(CHANNEL, "Fixture channel")]))
        status, page = s.library(action="history", profile=one)
        same("history", (status, ids(page), page.get("history_paused")), (200, ["watched0001", "watched0002"], False))
        remove = page["items"][0].get("history_remove_cursor", "")
        check("history rows carry a removal reference", bool(remove) and "private-" not in json.dumps(page), page["items"][0])
        status, page = s.library(action="watch_later", profile=one)
        same("watch later", (status, ids(page), page["items"][1]["playback_context"]), (200, ["listed00000", "listed00001"], {"playlist": "WL", "index": 1}))
        same("liked videos", ids(s.library(action="playlist", id="LL", profile=one)[1]), ["listed00000", "listed00001"])
        status, page = s.library(action="playlists", profile=one)
        same("playlists", (status, [(row["id"], row["title"], row["is_folder"]) for row in page["items"]]), (200, [(f"library:playlist:{MINE}", "My playlist", True)]))
        same("notification count is the account total", s.library(action="notification_count", profile=one),
             (200, {"api_version": 1, "kind": "notification_count", "unseen_count": 13}))
        same("notification count needs an account", s.library(action="notification_count")[1]["error"]["code"], "account_required")
        status, page = s.library(action="notifications", profile=one)
        same("notifications", (status, [(row["id"], row["unread"]) for row in page["items"]]), (200, [("notified001", True), ("notified002", False)]))
        status, page = s.library(action="topics", section="my_videos", profile=one)
        same("my videos", (status, [(row["id"], row["title"]) for row in page["items"]]), (200, [("uploaded001", "My upload")]))
        log = s.upstream()
        # Subscribed channels read the TV Subscriptions page first; this web-shaped answer has no channel tabs.
        same("every account list was asked once, with the account's token", [(r["tag"] or r["key"].split("/")[-1], r["bearer"]) for r in log],
             [(name, True) for name in ("home", "subscriptions", "subscriptions", "channels", "history", "watch_later", "liked", "playlists", "get_unseen_count", "get_notification_menu", "my_videos")])
        same("my videos need an account", s.library(action="topics", section="my_videos")[1]["error"]["code"], "account_required")
        # Public pages of an account profile are read without it; a playlist with it.
        same("search job in an account profile", (s.job({"action": "search", "query": "canned", "profile": one})[0], [r["bearer"] for r in s.upstream()][-1:]), (200, [False]))
        same("playlist job in an account profile", (len(s.job({"action": "catalog", "parent": f"playlist:{MINE}", "profile": one})[1]["items"]),
             [r["tag"] for r in s.upstream() if r["bearer"]]), (2, ["mine"]))
        same("unknown account profile", s.job({"action": "search", "query": "canned", "profile": "oauth_ffffffffffffffff"})[1]["error"]["code"], "unknown_profile")

        # ---- changes ----
        done = {"api_version": 1, "success": True, "playlist_id": ""}
        for action, path in (("like", "like/like"), ("dislike", "like/dislike"), ("unrate", "like/removelike")):
            same(action, (s.post(action=action, id=VIDEO, profile=one), s.sent(path)), ((200, done), 1))
        same("a change YouTube refuses is not a success", s.error(action="like", id=BROKEN, profile=one), (502, "upstream_failure"))
        same("guest cannot rate", s.error(action="like", id=VIDEO), (401, "account_required"))
        same("watch later add", (s.post(action="watch_later_add", id=VIDEO, profile=one), s.sent("edit_playlist")), ((200, {**done, "playlist_id": "WL"}), 1))
        same("watch later remove", (s.post(action="watch_later_remove", id="listed00001", profile=one)[0], s.sent("edit_playlist")), (200, 1))
        same("subscribe with a notification level", (s.post(action="subscribe", id=CHANNEL, notifications="all", profile=one)[1],
             s.sent("subscription/subscribe", False), s.sent("modify_channel_preference")), ({**done, "notifications": "all"}, 1, 1))
        same("notification level alone", (s.post(action="subscribe_notifications", id=CHANNEL, notifications="none", profile=one)[1], s.sent("modify_channel_preference")),
             ({**done, "notifications": "none"}, 1))
        same("unsubscribe", (s.post(action="unsubscribe", id=CHANNEL, profile=one), s.sent("subscription/unsubscribe")), ((200, done), 1))
        same("playlist create", (s.post(action="playlist_create", title="Evening", id=VIDEO, profile=one)[1], s.sent("playlist/create")),
             ({**done, "playlist_id": "PLcreated00001"}, 1))
        same("playlist title bounds", s.error(action="playlist_create", title=" ", profile=one), (400, "invalid_title"))
        same("playlist rename", (s.post(action="playlist_rename", playlist=MINE, title="Night", profile=one)[0], s.sent("edit_playlist")), (200, 1))
        same("playlist add", (s.post(action="playlist_add", playlist=MINE, id=VIDEO, profile=one)[0], s.sent("edit_playlist")), (200, 1))
        same("playlist remove", (s.post(action="playlist_remove", playlist=MINE, id="listed00000", profile=one)[0], s.sent("edit_playlist")), (200, 1))
        same("playlist delete", (s.post(action="playlist_delete", playlist=MINE, profile=one)[0], s.sent("playlist/delete")), (200, 1))
        # Playlist order: read, then one change per confirmation.
        same("playlist order read", s.post(action="playlist_order", playlist=MINE, profile=one)[1],
             {"api_version": 1, "kind": "playlist_order", "playlist_id": MINE, "editable": True, "orders": ["added_newest", "added_oldest", "popular"], "order": "added_newest"})
        s.upstream()
        change = dict(action="playlist_order", playlist=MINE, playlist_order="popular", session="order-1", profile=one)
        same("playlist order change", (s.post(**change)[1], s.sent("edit_playlist")), ({"api_version": 1, "success": True, "playlist_id": MINE, "order": "popular"}, 1))
        same("repeat answers from the record", (s.post(**change)[1]["order"], s.upstream()), ("popular", []))
        same("another change under the same confirmation", (s.error(**{**change, "playlist_order": "added_oldest"}), s.upstream()), ((409, "playlist_unconfirmed"), []))
        same("an order that is not offered sends nothing and frees the confirmation", (s.error(**{**change, "playlist_order": "published_newest", "session": "order-2"}),
             s.sent("edit_playlist")), ((404, "playlist_order_unavailable"), 0))
        same("so it can be used again", s.post(**{**change, "playlist_order": "added_oldest", "session": "order-2"})[0], 200)
        # Saving another channel's playlist.
        save = dict(action="playlist_save", playlist=THEIRS, session="save-1", profile=one)
        same("playlist save", (s.post(**save)[1], s.sent("like/like")), ({"api_version": 1, "success": True, "playlist_id": THEIRS, "saved": True}, 1))
        same("playlist save repeat", (s.post(**save)[0], s.sent("like/like")), (200, 0))
        same("unsave under the same confirmation", s.error(**{**save, "action": "playlist_unsave"}), (409, "playlist_unconfirmed"))
        same("playlist unsave", s.post(**{**save, "action": "playlist_unsave", "session": "save-2"})[1]["saved"], False)
        # History controls and removal.
        s.upstream()
        pause = dict(action="history_pause", session="pause-1", profile=one)
        same("history pause", (s.post(**pause)[1], s.sent("v1/feedback")), ({"api_version": 1, "success": True, "history": "paused"}, 1))
        same("history pause repeat", (s.post(**pause)[1]["history"], s.sent("v1/feedback")), ("paused", 0))
        same("history clear under the same confirmation", s.error(**{**pause, "action": "history_clear"}), (409, "history_unconfirmed"))
        same("history clear", s.post(**{**pause, "action": "history_clear", "session": "clear-1"})[1]["history"], "cleared")
        same("a control YouTube does not offer", s.error(**{**pause, "action": "history_resume", "session": "resume-1"}), (404, "history_control_unavailable"))
        s.upstream()
        same("history remove", (s.post(action="history_remove", id="watched0001", cursor=remove, profile=one)[1], s.sent("v1/feedback")),
             ({"api_version": 1, "success": True, "video_id": "watched0001"}, 1))
        same("history remove with another account's reference", s.error(action="history_remove", id="watched0001", cursor=remove, profile=two), (409, "cursor_expired"))
        # Feedback: one confirmed, one unconfirmed and never repeated.
        choice = dict(action="feedback", id="homevideo01", cursor=cursor, profile=one)
        same("feedback", (s.post(**choice, feedback="not_interested")[1], s.sent("v1/feedback")),
             ({"api_version": 1, "success": True, "video_id": "homevideo01", "feedback": "not_interested"}, 1))
        same("feedback repeat", (s.post(**choice, feedback="not_interested")[0], s.sent("v1/feedback")), (200, 0))
        same("feedback YouTube did not process", (s.error(**choice, feedback="dont_recommend_channel"), s.sent("v1/feedback")), ((502, "feedback_unconfirmed"), 1))
        same("unconfirmed feedback is not sent again", (s.error(**choice, feedback="dont_recommend_channel"), s.sent("v1/feedback")), ((409, "feedback_unconfirmed"), 0))
        same("hide a subscriptions entry", s.post(action="feedback", id="subscribed1", cursor=hide, feedback="hide", profile=one)[1]["feedback"], "hide")
        same("feedback of another account", s.error(**{**choice, "profile": two}, feedback="not_interested"), (409, "cursor_expired"))
        # Stream reminder.
        same("reminder status needs no account", s.post(action="stream_reminder", id=UPCOMING, reminder="status")[1],
             {"api_version": 1, "kind": "stream_reminder", "id": UPCOMING, "upcoming": True, "available": True, "reminder_set": False})
        remind = dict(action="stream_reminder", id=UPCOMING, reminder="set", session="remind-1", profile=one)
        s.upstream()
        same("reminder set", (s.post(**remind)[1], s.sent("add_upcoming_event_reminder")), ({"api_version": 1, "success": True, "id": UPCOMING, "reminder_set": True}, 1))
        same("reminder repeat", (s.post(**remind)[0], s.upstream()), (200, []))
        same("reminder for guest", s.error(**{**remind, "profile": "guest"}), (401, "account_required"))
        # Comments: read with rating and reply references, rate, post, reply.
        status, page = s.library(action="comments", id=VIDEO, profile=one)
        same("comments", (status, [row["content"] for row in page["items"]], page["can_post"], page["comment_count"]), (200, ["Comment 1", "Comment 2"], True, "2"))
        first, second = page["items"]
        check("comment references, no private fields", first["rating_cursor"] and first["respond_cursor"] and "private-" not in json.dumps(page), first)
        s.upstream()
        rate = dict(action="comment_rate", cursor=first["rating_cursor"], rating="like", profile=one)
        same("comment like", (s.post(**rate)[1], s.sent("perform_comment_action")), ({"api_version": 1, "success": True, "rating": "like"}, 1))
        same("comment like repeat", (s.post(**rate)[0], s.sent("perform_comment_action")), (200, 0))
        same("another rating under the same reference", s.error(**{**rate, "rating": "dislike"}), (409, "comment_unconfirmed"))
        failing = dict(action="comment_rate", cursor=second["rating_cursor"], rating="like", profile=one)
        same("a comment rating that fails upstream", (s.error(**failing)[0], s.sent("perform_comment_action")), (502, 1))
        same("is never sent again", (s.error(**failing), s.sent("perform_comment_action")), ((409, "comment_unconfirmed"), 0))
        post = dict(action="comment_post", id=VIDEO, text="Hello there", session="post-1", profile=one)
        same("comment post", (s.post(**post)[1], s.sent("create_comment")), ({"api_version": 1, "success": True, "id": VIDEO, "session": "post-1", "reply": False}, 1))
        same("comment post repeat", (s.post(**post)[0], s.sent("create_comment")), (200, 0))
        same("another text under the same confirmation", s.error(**{**post, "text": "Something else"}), (409, "comment_unconfirmed"))
        same("a comment YouTube refuses", (s.error(**{**post, "text": "refuse me", "session": "post-2"}), s.sent("create_comment")), ((502, "comment_unconfirmed"), 1))
        same("is not posted again", (s.error(**{**post, "text": "refuse me", "session": "post-2"}), s.sent("create_comment")), ((409, "comment_unconfirmed"), 0))
        reply = dict(action="comment_reply", id=VIDEO, cursor=first["respond_cursor"], text="A reply", session="reply-1", profile=one)
        same("comment reply", (s.post(**reply)[1], s.sent("create_comment_reply")), ({"api_version": 1, "success": True, "id": VIDEO, "session": "reply-1", "reply": True}, 1))
        same("guest cannot post", s.error(**{**post, "profile": "guest", "session": "post-3"}), (401, "account_required"))

        # ---- watch-history reports ----
        s.upstream()
        report = dict(action="watch_progress", id=VIDEO, session="watch-1", profile=one)
        same("first report: tracking data, history entry, position", (s.post(**report, position_sec=12, state="playing")[1], [r["key"].split("/")[-1] for r in s.upstream()]),
             ({"api_version": 1, "success": True, "position_sec": 12}, ["player", "playback", "watchtime"]))
        same("the same report again sends nothing", (s.post(**report, position_sec=12, state="playing")[0], s.upstream()), (200, []))
        same("a later position sends one update", (s.post(**report, position_sec=42, state="playing")[1]["position_sec"], [r["key"].split("/")[-1] for r in s.upstream()]),
             (42, ["watchtime"]))
        same("a session belongs to one video", s.error(**{**report, "id": BROKEN}, position_sec=1, state="playing"), (400, "invalid_progress"))
        same("stop report", s.post(**report, position_sec=60, state="stopped")[0], 200)
        same("a stopped session takes nothing more", s.error(**report, position_sec=61, state="playing")[1], "session_closed")
        s.upstream()
        same("reports of Guest are refused before anything is sent", (s.error(**{**report, "profile": "guest"}, position_sec=5), s.upstream()), ((401, "account_required"), []))
        # An update YouTube does not confirm ends that session's reports.
        lost = dict(action="watch_progress", id=VIDEO, session="watch-2", profile=one)
        same("report before the failure", s.post(**lost, position_sec=10, state="playing")[0], 200)
        s.upstream()
        same("an unconfirmed position", (s.error(**lost, position_sec=77, state="playing"), s.sent("watchtime")), ((502, "upstream_failure"), 1))
        same("is not repeated, and nothing later is sent", (s.error(**lost, position_sec=77, state="playing"), s.error(**lost, position_sec=90, state="stopped"), s.upstream()),
             ((409, "history_unconfirmed"), (409, "history_unconfirmed"), []))
        # An unconfirmed history entry (the first call) likewise.
        entry = dict(action="watch_progress", id=BROKEN, session="watch-3", profile=one)
        same("an unconfirmed history entry", s.error(**entry, position_sec=3, state="playing")[1], "upstream_failure")
        s.upstream()
        same("ends the session without another request", (s.error(**entry, position_sec=4, state="playing"), s.upstream()), ((409, "history_unconfirmed"), []))
        # Mark as watched.
        mark = dict(action="mark_watched", id=VIDEO, session="mark-1", profile=one)
        same("mark watched", s.post(**mark)[1], {"api_version": 1, "success": True, "video_id": VIDEO, "position_sec": 635})
        s.upstream()
        same("mark watched repeat", (s.post(**mark)[0], s.upstream()), (200, []))
        same("the same confirmation for another video", s.error(**{**mark, "id": BROKEN}), (409, "history_unconfirmed"))

        # ---- playback of an account profile: resolved as Guest ----
        s.upstream()
        status, play = s.job({"action": "resolve", "playback": {"id": VIDEO, "profile": one}})
        log = s.upstream()
        same("an account profile resolves without its token", (status, play["choices"][0]["container"], [r for r in log if r["bearer"]]), (200, "dash", []))
        same("a video that needs an account stays refused while the switch is off", (s.job({"action": "resolve", "playback": {"id": AGED, "profile": one}})[1]["error"]["code"],
             [r["tag"] for r in s.upstream() if "player" in r["key"]]), ("authentication_required", ["guest player"]))
        same("details of an account profile", s.call("GET", f"/v1/video/{VIDEO}?profile={one}")[0], 200)

        # ---- removal ----
        same("guards hold across an account being added or removed", s.post(**save)[0], 200)
        watch = s.job({"action": "resolve", "playback": {"id": VIDEO, "profile": two}})[1]
        same("media of the other account before removal", s.call("GET", watch["choices"][0]["url"], token=None, raw=True)[0], 200)
        pending = s.call("POST", "/v1/jobs", {"action": "library", "library": {"action": "home", "profile": two}})[2]["job"]
        time.sleep(0.2)
        status, removed = s.job({"action": "library", "library": {"action": "profile_remove", "profile": two}})
        same("remove an account", (status, removed), (200, {"api_version": 1, "success": True, "profile": two}))
        same("its tokens left the file", ([p["id"] for p in s.stored()], "token-2" in s.store().read_text()), ([one], False))
        same("its playback session ended", s.call("GET", watch["choices"][0]["url"], token=None)[2]["error"]["code"], "expired_session")
        same("its finished jobs are gone", s.call("GET", "/v1/jobs/" + pending)[2]["error"]["code"], "expired_job")
        same("it is no profile any more", (s.error(action="home", profile=two), s.error(action="profile_remove", profile=two),
             [p["id"] for p in s.post(action="profiles")[1]["profiles"]]), ((404, "unknown_profile"), (404, "unknown_profile"), ["guest", one]))
        same("resolve for a removed account", s.job({"action": "resolve", "playback": {"id": VIDEO, "profile": two}})[1]["error"]["code"], "unknown_profile")
        s.upstream()
        same("the other account works on", (s.post(action="history", profile=one)[0], s.sent("history")), (200, 1))
        same("confirmed changes are still remembered", (s.post(**save)[0], s.post(**pause)[0], s.upstream()), (200, 200, []))
        same("an open history session does not report again after the rebuild",
             s.error(action="watch_progress", id=VIDEO, session="watch-1", profile=one, position_sec=99, state="playing")[1], "history_unconfirmed")
        same("guest cannot be removed", s.error(action="profile_remove"), (400, "account_required"))
        same("remove the last account", (s.post(action="profile_remove", profile=one)[0], s.stored()), (200, []))
    finally:
        s.finish("lists and changes")

    # Signed-in playback, switched on by the host: the account session answers what Guest could not.
    on = Service(binary, directory, "--account-playback", stored=[profile(1)])
    try:
        same("health says the switch is on", on.call("GET", "/v1/health", token=None)[2]["account_playback"], True)
        status, play = on.job({"action": "resolve", "playback": {"id": AGED, "profile": one}})
        same("the account session resolves it", (status, play.get("choices", [{}])[0].get("container"), [(r["tag"], r["bearer"]) for r in on.upstream() if "v1/player" in r["key"]]),
             (200, "dash", [("guest player", False), ("account player", True)]))
        manifest = on.call("GET", play["choices"][0]["url"], token=None, raw=True)[2].decode()
        check("its manifest names YouTube's servers and no token", "googlevideo.com" in manifest and "access-token" not in manifest, manifest[:300])
        same("Guest gets no such help", on.job({"action": "resolve", "playback": {"id": AGED}})[1]["error"]["code"], "authentication_required")
        same("an ordinary video still resolves without the token", (on.job({"action": "resolve", "playback": {"id": VIDEO, "profile": one}})[0],
             [r for r in on.upstream() if r["bearer"]]), (200, []))
    finally:
        on.finish("signed-in playback on")
    shutil.rmtree(directory)


def tv_lists(binary):
    raw = json.loads((ROOT.parent / "companion/innertube/fixtures/tv-home-shape.json").read_text())
    directory = fixtures(files={name: raw for name in ["a_home.json", "a_history.json", "a_ll.json", "a_wl.json"]})
    s = Service(binary, directory, stored=[profile(1)])
    try:
        for action, extra in [("home", {}), ("history", {}), ("playlist", {"id": "LL"}), ("watch_later", {})]:
            status, page = s.post(action=action, profile=profile(1)["id"], **extra)
            videos = [row for row in page.get("items", []) if not row.get("is_folder")]
            same(f"TV {action} cards survive QuickJS", (status, [(row["id"], row["duration_sec"]) for row in videos]),
                 (200, [("fixtureTV01", 123)]))
            requests = [row for row in s.upstream() if "v1/browse" in row["key"]]
            check(f"TV {action} uses the selected account", bool(requests) and all(row["bearer"] for row in requests))
    finally:
        s.finish("TV account lists")
        shutil.rmtree(directory)


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / "build" / "tube_js")
    only = sys.argv[2:] or ["sign_in", "sign_in_edges", "storage_failure", "refresh", "lists_and_changes", "tv_lists"]
    try:
        for name in only:
            globals()[name](binary)
    except Exception:   # show what was learned before the test itself broke
        for failure in base.failures:
            print("FAIL", failure)
        raise
    for failure in base.failures:
        print("FAIL", failure)
    print(f"account test: {base.passed} checks passed, {len(base.failures)} failed")
    return 1 if base.failures else 0


if __name__ == "__main__":
    sys.exit(main())
