"""In-memory account fixtures. No credentials, worker, or network connections."""
import copy
import hashlib
import re
import time
from uuid import uuid4

from .models import ServiceError

VIDEOS = ("DEMOAVC0001", "DEMOVP90001", "DEMOHLS0001")
CHANNEL = "UC" + "f" * 22
PRIVATE = {"subscriptions", "history", "library", "playlists", "watch_later",
           "like", "dislike", "unrate", "subscribe", "unsubscribe", "playlist_create",
           "playlist_add", "playlist_remove", "playlist_rename", "playlist_delete",
           "watch_later_add", "watch_later_remove", "watch_progress", "comment_rate", "chat_send"}


class FixtureLibrary:
    def __init__(self, extractor, *, clock=time.monotonic):
        self.extractor, self.clock = extractor, clock
        self.closed = False
        self.references = {}
        self.profiles = {"guest": "Guest", "fixture_one": "Offline account one",
                         "fixture_two": "Offline account two"}
        self.accounts = {}
        for owner in self.profiles:
            self.accounts[owner] = {
                "playlists": {f"PLfixture{i:02d}": {"title": f"Offline playlist {i + 1}",
                              "videos": [VIDEOS[i % 3]], "privacy": "PRIVATE"} for i in range(25)},
                "watch_later": [], "ratings": {}, "subscriptions": set(), "history": {}, "comment_ratings": {}}
        self.watches = {}
        self.chats = {}
        self.chat_sends = {}
        self.on_profile_removed = lambda identity: None

    def has_profile(self, identity):
        return identity in self.profiles

    def _reference(self, owner, kind, items):
        now = self.clock()
        self.references = {k: v for k, v in self.references.items() if v["deadline"] > now}
        if len(self.references) >= 256:
            self.references.pop(next(iter(self.references)))
        cursor = str(uuid4())
        self.references[cursor] = {"owner": owner, "kind": kind,
                                   "items": copy.deepcopy(items), "deadline": now + 120,
                                   "result": None}
        return cursor

    def _page(self, owner, kind, items):
        cursor = self._reference(owner, kind, items[24:]) if len(items) > 24 else ""
        return {"api_version": 1, "kind": kind, "items": copy.deepcopy(items[:24]),
                "cursor": cursor, "has_more": bool(cursor)}

    def _video(self, identifier):
        from .catalog import item
        row = item(self.extractor.info(identifier)).model_dump()
        row["art_url"] = self.extractor.base + "/fixtures/thumb.png"
        return row

    @staticmethod
    def _title(value):
        title = value.strip()
        if not title or len(title) > 150 or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in title):
            raise ServiceError("invalid_title", "Enter a playlist title of 1 to 150 characters.", 400)
        return title

    async def execute(self, request):
        if self.closed:
            raise ServiceError("library_closed", "The library service has stopped.", 503)
        action, owner = request.get("action"), request.get("profile", "guest")
        if action == "profiles":
            return {"api_version": 1, "profiles": [{"id": k, "name": v,
                    "account_configured": k != "guest"} for k, v in self.profiles.items()]}
        if owner not in self.accounts:
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        if action == "profile_remove":
            if owner == "guest":
                raise ServiceError("account_required", "Select an account profile first.", 400)
            del self.accounts[owner]
            del self.profiles[owner]
            self.references = {k: v for k, v in self.references.items() if v["owner"] != owner}
            self.watches = {k: v for k, v in self.watches.items() if k[0] != owner}
            self.chats = {k: v for k, v in self.chats.items() if v["owner"] != owner}
            self.chat_sends = {k: v for k, v in self.chat_sends.items() if k[0] != owner}
            self.on_profile_removed(owner)
            return {"api_version": 1, "success": True, "profile": owner}
        if action in PRIVATE and owner == "guest":
            raise ServiceError("account_required", "Select an offline account profile.", 401)
        if action == "chat_send":
            return self._send_chat(request, owner)
        if action in {"live_chat", "chat_poll", "chat_close"}:
            return self._chat(request, owner)
        if action == "comment_rate":
            ref = self.references.get(request.get("cursor"))
            if not ref or ref["owner"] != owner or ref["kind"] != "rating" or ref["deadline"] <= self.clock():
                raise ServiceError("cursor_expired", "Reload the comments before rating this comment.", 409)
            rating = request.get("rating")
            if rating not in ("like", "dislike", "none"):
                raise ServiceError("invalid_rating", "Select an available comment rating.", 400)
            if ref["result"] is not None:
                if ref["result"]["rating"] != rating:
                    raise ServiceError("comment_unconfirmed", "Reload the comments; this rating intent cannot be repeated.", 409)
                return copy.deepcopy(ref["result"])
            if rating not in ref["items"][1]:
                raise ServiceError("invalid_rating", "Select an available comment rating.", 400)
            self.accounts[owner]["comment_ratings"][ref["items"][0]] = rating
            ref["result"] = {"api_version": 1, "success": True, "rating": rating}
            return copy.deepcopy(ref["result"])
        if action in {"topics", "topic_open"}:
            section = request.get("section")
            if section not in {"music", "gaming", "news", "sports", "live", "movies", "trending", "shorts", "my_videos"}:
                raise ServiceError("invalid_catalog", "Choose an available section.", 400)
            if section == "my_videos" and owner == "guest":
                raise ServiceError("account_required", "Select an offline account profile.", 401)
            if action == "topics":
                key = self._reference(owner, "topic:" + section, [self._video(i) for i in VIDEOS])
                return self._page(owner, "catalog", [{"id": f"section:{section}:{key}",
                    "title": "Generated " + section + " row", "is_folder": True}])
            ref = self.references.get(request.get("id"))
            if not ref or ref["owner"] != owner or ref["kind"] != "topic:" + section or ref["deadline"] <= self.clock():
                raise ServiceError("cursor_expired", "Reload this section to open the row.", 409)
            return self._page(owner, "catalog", ref["items"])
        if action in {"continue", "replies"}:
            ref = self.references.get(request.get("cursor"))
            replies = action == "replies"
            if (not ref or ref["owner"] != owner or ref["deadline"] <= self.clock()
                    or (ref["kind"] == "replies") != replies or ref["kind"] in {"rating", "chat"}
                    or ref["kind"].startswith("topic:")):
                raise ServiceError("cursor_expired", "Reload this page to continue.", 409)
            if ref["result"] is None:
                ref["result"] = self._page(owner, "comments" if replies else ref["kind"], ref["items"])
            return copy.deepcopy(ref["result"])
        if action == "search_suggestions":
            query = str(request.get("query", "")).strip()[:160]
            return {"api_version": 1, "kind": "search_suggestions", "query": query,
                    "suggestions": [f"{query} offline suggestion {i + 1}".strip() for i in range(5)]}
        account = self.accounts[owner]
        identifier, playlist = request.get("id", ""), request.get("playlist", "")
        if action == "description":
            if identifier not in VIDEOS:
                raise ServiceError("fixture_only", "Select a generated fixture video.", 404)
            row = self._video(identifier)
            text = "Generated description.\nLiteral <b>markup</b> & Unicode: caf\u00e9 \u65e5\u672c\u8a9e.\n" + "Full description scroll test. " * 200
            return {"api_version": 1, "kind": "description", "id": identifier, "title": row["title"],
                    "channel": row["subtitle"], "channel_id": row["channel_id"], "published": "2024-01-01",
                    "views": "1234", "like_count": "56", "description": text[:20000], "truncated": False}
        if action in {"comments", "like", "dislike", "unrate", "playlist_add", "playlist_remove", "watch_later_add", "watch_later_remove", "watch_progress"} or (action == "playlist_create" and identifier):
            if identifier not in VIDEOS:
                raise ServiceError("fixture_only", "Select a generated fixture video.", 404)
        if action in {"subscribe", "unsubscribe"} and identifier != CHANNEL:
            raise ServiceError("fixture_only", "Select the generated fixture channel.", 404)
        if action in {"playlist_add", "playlist_remove", "playlist_rename", "playlist_delete"}:
            if playlist not in account["playlists"]:
                raise ServiceError("playlist_missing", "Select an existing offline playlist.", 404)
        if action == "watch_progress":
            import math
            import re
            session, position, state = request.get("session", ""), request.get("position_sec", 0), request.get("state", "playing")
            if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", session) or not math.isfinite(position) or not 0 <= position <= 1_000_000 or state not in {"playing", "stopped"}:
                raise ServiceError("invalid_progress", "The playback report is invalid.", 400)
            self.watches = {k: v for k, v in self.watches.items() if v["deadline"] > self.clock()}
            key = (owner, session)
            previous = self.watches.get(key)
            if previous and previous["id"] != identifier:
                raise ServiceError("invalid_progress", "The playback session belongs to another video.", 400)
            if previous and previous["state"] == "stopped" and (previous["position"] != position or state != "stopped"):
                raise ServiceError("session_closed", "This playback session has ended.", 409)
            if not previous and len(self.watches) >= 64:
                raise ServiceError("history_busy", "Too many playback sessions are retained.", 429)
            self.watches[key] = {"id": identifier, "position": position, "state": state, "deadline": self.clock() + 1800}
            account["history"][identifier] = position
            return {"api_version": 1, "success": True, "position_sec": position}
        if action == "comments":
            rows = [{"id": f"fixture-comment-{i}", "author": f"Offline author {i + 1}",
                     "content": f"Generated comment {i + 1}.\n" + ("Full text scroll test. " * 100 if i == 0 else "Literal <b>markup</b> & Unicode: café 日本語."),
                     "published": "Offline fixture", "likes": str(i), "replies": "3" if i == 0 else "0",
                     "pinned": i == 0, "owner": i == 0, "art_url": "", "reply_cursor": ""} for i in range(25)]
            replies = [{**rows[1], "id": f"fixture-reply-{i}", "content": f"Generated reply {i + 1}.", "replies": "0"} for i in range(3)]
            rows[0]["reply_cursor"] = self._reference(owner, "replies", replies)
            for row in [*rows, *replies]:
                current = account["comment_ratings"].get(row["id"], "none")
                options = [r for r in ("like", "dislike", "none") if r != current]
                row.update(rating_like=owner != "guest" and "like" in options,
                           rating_dislike=owner != "guest" and "dislike" in options,
                           rating_remove=owner != "guest" and "none" in options,
                           rating_cursor=self._reference(owner, "rating", [row["id"], options]) if owner != "guest" else "")
            # The reply reference takes its snapshot after rating capabilities exist.
            self.references[rows[0]["reply_cursor"]]["items"] = copy.deepcopy(replies)
            if request.get("sort", "top") == "newest":
                rows.reverse()
            return self._page(owner, "comments", rows)
        if action in {"home", "history", "subscriptions"}:
            ids = (list(reversed(account["history"])) if action == "history" else
                   VIDEOS if action != "subscriptions" or CHANNEL in account["subscriptions"] else ())
            rows = [self._video(i) for i in ids]
            if action == "history":
                for row in rows:
                    row["resume_pos_sec"] = int(account["history"][row["id"]])
            return self._page(owner, "catalog", rows)
        if action in {"library", "playlists"}:
            rows = [{"id": "library:playlist:" + k, "title": v["title"],
                     "subtitle": "Offline private playlist", "art_url": "", "is_folder": True}
                    for k, v in account["playlists"].items()]
            return self._page(owner, "catalog", rows)
        if action in {"playlist", "watch_later"}:
            if action == "watch_later" or identifier == "WL":
                ids = account["watch_later"]
            elif identifier == "LL":
                ids = [k for k, v in account["ratings"].items() if v == "like"]
            else:
                selected = account["playlists"].get(identifier)
                if selected is None:
                    raise ServiceError("playlist_missing", "Select an existing offline playlist.", 404)
                ids = selected["videos"]
            return self._page(owner, "catalog", [self._video(i) for i in ids])
        created = ""
        if action == "playlist_create":
            if len(account["playlists"]) >= 128:
                raise ServiceError("fixture_limit", "Restart fixture mode to reset its playlists.", 409)
            created = "PLfixture_" + uuid4().hex
            account["playlists"][created] = {"title": self._title(request.get("title", "")),
                                            "videos": [identifier] if identifier else [], "privacy": "PRIVATE"}
        elif action == "playlist_rename":
            account["playlists"][playlist]["title"] = self._title(request.get("title", ""))
        elif action == "playlist_delete":
            del account["playlists"][playlist]
        elif action in {"playlist_add", "playlist_remove", "watch_later_add", "watch_later_remove"}:
            ids = account["watch_later"] if action.startswith("watch_later") else account["playlists"][playlist]["videos"]
            if action.endswith("add") and identifier not in ids:
                ids.append(identifier)
            elif action.endswith("remove") and identifier in ids:
                ids.remove(identifier)
        elif action in {"like", "dislike"}:
            account["ratings"][identifier] = action
        elif action == "unrate":
            account["ratings"].pop(identifier, None)
        elif action == "subscribe":
            account["subscriptions"].add(identifier)
        elif action == "unsubscribe":
            account["subscriptions"].discard(identifier)
        else:
            raise ServiceError("unknown_operation", "This library operation is unavailable.", 400)
        return {"api_version": 1, "success": True, "playlist_id": created}

    def _send_chat(self, request, owner):
        now = self.clock()
        intent, cursor = request.get("session"), request.get("cursor")
        text, video = request.get("text"), request.get("id")
        if (not isinstance(intent, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", intent)
                or not isinstance(cursor, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", cursor)
                or not isinstance(video, str) or not re.fullmatch(r"[A-Za-z0-9_-]{11}", video)
                or not isinstance(text, str) or not text.strip() or len(text) > 200
                or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in text)):
            raise ServiceError("invalid_message", "Enter a message of 1 to 200 characters.", 400)
        self.chat_sends = {k: v for k, v in self.chat_sends.items() if v["deadline"] > now}
        identity = (cursor, video, hashlib.sha256(text.encode("utf-8")).hexdigest())
        key = (owner, intent)
        state = self.chat_sends.get(key)
        if state:
            if state["identity"] != identity or state["result"] is None:
                raise ServiceError("chat_unconfirmed", "This message intent cannot be repeated.", 409)
            return copy.deepcopy(state["result"])
        if video != VIDEOS[2]:
            raise ServiceError("chat_unavailable", "Select the HLS fixture for offline chat.", 404)
        if len(self.chat_sends) >= 256:
            raise ServiceError("chat_busy", "Too many message intents are retained.", 429)
        state = {"identity": identity, "result": None, "deadline": now + 1800}
        self.chat_sends[key] = state
        ref = self.references.get(cursor)
        context = self.chats.get(ref["items"]) if ref and ref["kind"] == "chat" else None
        if (not context or ref["owner"] != owner or context["owner"] != owner
                or ref["deadline"] <= now or context["deadline"] <= now
                or context["id"] != video):
            raise ServiceError("cursor_expired", "Reopen this chat to continue.", 409)
        message = {"id": "fixture-sent-" + str(uuid4()), "author": self.profiles[owner],
                   "content": text, "published": "Offline fixture", "type": "LiveChatTextMessage",
                   "amount": "", "moderator": False, "verified": False, "art_url": ""}
        context["sent"] = [*context["sent"], message][-256:]
        # Include the confirmed fixture message in the next poll, including a
        # poll before the normal two-second refresh interval. Do not alter a
        # previously cached continuation response.
        context["result"]["items"] = [*context["result"]["items"], message][-context["limit"]:]
        state["result"] = {"api_version": 1, "success": True, "id": video,
                           "session": intent, "message_id": message["id"]}
        return copy.deepcopy(state["result"])

    def _chat(self, request, owner):
        now = self.clock()
        self.chats = {k: v for k, v in self.chats.items() if v["deadline"] > now}
        action = request["action"]
        previous = None
        if action == "live_chat":
            if request.get("id") != VIDEOS[2]:
                raise ServiceError("chat_unavailable", "Select the HLS fixture for offline chat.", 404)
            mode = request.get("chat_mode", "top")
            if mode not in {"top", "live"}:
                raise ServiceError("invalid_sort", "Select top or live chat.", 400)
            if len(self.chats) >= 16:
                raise ServiceError("chat_busy", "Close another chat before opening this one.", 429)
            identity = str(uuid4())
            context = {"owner": owner, "id": VIDEOS[2], "mode": mode, "step": 0, "next_poll": now,
                       "deadline": now + 1800, "sent": [], "limit": 256 if request.get("chat_history") is True else 24}
            self.chats[identity] = context
        else:
            ref = self.references.get(request.get("cursor"))
            identity = ref["items"] if ref and ref["kind"] == "chat" else None
            context = self.chats.get(identity)
            if not context or ref["owner"] != owner or ref["deadline"] <= now:
                raise ServiceError("cursor_expired", "Reopen this chat to continue.", 409)
            if action == "chat_close":
                del self.chats[identity]
                self.references = {k: v for k, v in self.references.items() if v["kind"] != "chat" or v["items"] != identity}
                return {"api_version": 1, "success": True}
            if ref["result"] is not None:
                return copy.deepcopy(ref["result"])
            previous = ref
            if now < context["next_poll"]:
                return {**copy.deepcopy(context["result"]), "cursor": request["cursor"],
                        "poll_after_ms": max(0, int((context["next_poll"] - now) * 1000))}
            context["step"] += 1
        start = context["step"]
        rows = [{"id": f"fixture-chat-{i}", "author": f"Offline viewer {i + 1}",
                 "content": f"Generated {context['mode']} chat message {i + 1}.\nLiteral <b>markup</b> & café 日本語.\n" +
                            ("Full message scroll test. " * 100 if i % 24 == 0 else ""),
                 "published": "Offline fixture", "type": "LiveChatPaidMessage" if i % 24 == 1 else "LiveChatTextMessage",
                 "amount": "$5.00" if i % 24 == 1 else "", "moderator": i % 24 == 2, "verified": i % 24 == 3,
                 "art_url": ""} for i in range(max(0, start + 24 - context["limit"]), start + 24)]
        cursor = self._reference(owner, "chat", identity)
        self.references[cursor]["deadline"] = now + 1800
        result = {"api_version": 1, "kind": "live_chat", "id": context["id"], "chat_mode": context["mode"],
                  "items": [*rows, *copy.deepcopy(context["sent"])][-context["limit"]:], "cursor": cursor, "has_more": False, "ended": False, "is_replay": False,
                  "poll_after_ms": 2000}
        context.update(next_poll=now + 2, deadline=now + 1800, result=result)
        if previous is not None:
            previous["result"] = copy.deepcopy(result)
        return copy.deepcopy(result)

    async def close(self):
        self.closed = True
        self.references.clear()
        self.watches.clear()
        self.chats.clear()
        self.chat_sends.clear()
