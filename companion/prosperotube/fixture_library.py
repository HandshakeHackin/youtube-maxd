"""In-memory account fixtures. No credentials, worker, or network connections."""
import copy
import time
from uuid import uuid4

from .models import ServiceError

VIDEOS = ("DEMOAVC0001", "DEMOVP90001", "DEMOHLS0001")
CHANNEL = "UC" + "f" * 22
PRIVATE = {"subscriptions", "history", "library", "playlists", "watch_later",
           "like", "dislike", "unrate", "subscribe", "unsubscribe", "playlist_create",
           "playlist_add", "playlist_remove", "playlist_rename", "playlist_delete",
           "watch_later_add", "watch_later_remove", "watch_progress"}


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
                "watch_later": [], "ratings": {}, "subscriptions": set(), "history": {}}
        self.watches = {}
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
            self.on_profile_removed(owner)
            return {"api_version": 1, "success": True, "profile": owner}
        if action in PRIVATE and owner == "guest":
            raise ServiceError("account_required", "Select an offline account profile.", 401)
        if action in {"continue", "replies"}:
            ref = self.references.get(request.get("cursor"))
            replies = action == "replies"
            if (not ref or ref["owner"] != owner or ref["deadline"] <= self.clock()
                    or (ref["kind"] == "replies") != replies):
                raise ServiceError("cursor_expired", "Reload this page to continue.", 409)
            if ref["result"] is None:
                ref["result"] = self._page(owner, "comments" if replies else ref["kind"], ref["items"])
            return copy.deepcopy(ref["result"])
        account = self.accounts[owner]
        identifier, playlist = request.get("id", ""), request.get("playlist", "")
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

    async def close(self):
        self.closed = True
        self.references.clear()
        self.watches.clear()
