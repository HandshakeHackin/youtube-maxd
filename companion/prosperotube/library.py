"""Bounded InnerTube worker; account credentials remain on the companion."""
import asyncio
import hashlib
import http.cookiejar
import json
import os
from pathlib import Path
import re
import secrets
import time

from .models import ServiceError


def load_profiles(config):
    if config is None:
        return []
    path = Path(config)
    if path.stat().st_size > 65536:
        raise ValueError("The profile configuration exceeds 64 KiB.")
    profiles = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(profiles, list) or len(profiles) > 8:
        raise ValueError("Configure at most eight profiles.")
    output, identities = [], {"guest"}
    for profile in profiles:
        if not isinstance(profile, dict):
            raise ValueError("Each profile must be an object.")
        identity = profile.get("id", "")
        if not isinstance(identity, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", identity) or identity in identities:
            raise ValueError("Profile identities must be unique short identifiers.")
        identities.add(identity)
        index = profile.get("account_index", 0)
        if type(index) is not int or not 0 <= index <= 7:
            raise ValueError("Account indexes must be between zero and seven.")
        channel = profile.get("channel", "")
        if not isinstance(channel, str) or (channel and not re.fullmatch(r"[A-Za-z0-9_-]{1,100}", channel)):
            raise ValueError("The profile channel identifier is invalid.")
        cookie_path = Path(profile["cookie_file"])
        if not cookie_path.is_absolute():
            cookie_path = path.parent / cookie_path
        if cookie_path.stat().st_size > 65536:
            raise ValueError("Cookie files must be at most 64 KiB.")
        contents = cookie_path.read_text(encoding="utf-8")
        if contents.startswith("# Netscape HTTP Cookie File") or contents.startswith("# HTTP Cookie File"):
            jar = http.cookiejar.MozillaCookieJar(str(cookie_path))
            jar.load(ignore_discard=True, ignore_expires=False)
            cookie = "; ".join(f"{item.name}={item.value}" for item in jar
                               if item.domain.lstrip(".") == "youtube.com" and not item.is_expired())
        else:
            cookie = contents.strip()
        if not cookie or len(cookie) > 65536 or "\r" in cookie or "\n" in cookie:
            raise ValueError("Provide a valid YouTube cookie file for each account profile.")
        value = {"id": identity, "name": str(profile.get("name", identity))[:192],
                 "cookie": cookie, "account_index": index}
        if channel:
            value["channel"] = channel
        output.append(value)
    return output


class LibraryService:
    def __init__(self, node="node", *, profile_config=None, worker=None, timeout=30, oauth_store=None, proxy=None):
        self.node = node
        self.proxy = proxy
        self.session_intents = {}
        self.worker = Path(worker) if worker else Path(__file__).resolve().parents[1] / "innertube" / "worker.bundle.mjs"
        self.profiles = load_profiles(profile_config)
        self.oauth_store = oauth_store
        if oauth_store:
            self.profiles.extend(oauth_store.profiles)
        if len(self.profiles) > 8 or len({p["id"] for p in self.profiles}) != len(self.profiles):
            raise ValueError("Configure at most eight distinct account profiles.")
        self.profile_revision = 0
        self.worker_revision = -1
        self.timeout = timeout
        self.process = None
        self.lock = asyncio.Lock()
        self.pending = 0
        self.closed = False
        self.watch_states = {}
        self.comment_states = {}
        self.chat_sends = {}
        self.watched_intents = {}
        self.feedback_states = {}
        self.account_intents = {}
        self.on_profile_removed = lambda identity: None

    def has_profile(self, identity):
        return identity == "guest" or any(p["id"] == identity for p in self.profiles)

    async def _remove_profile(self, request):
        identity = request.get("profile", "guest")
        profile = next((p for p in self.profiles if p["id"] == identity), None)
        if identity == "guest":
            raise ServiceError("account_required", "Select an account profile first.", 400)
        if profile is None:
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        if "oauth" not in profile or self.oauth_store is None:
            raise ServiceError("profile_managed", "Remove this externally configured account from the companion profile configuration.", 409)
        # Commit private storage first. Pre-commit failure preserves the profile,
        # worker, caches and capabilities. No upstream account mutation occurs.
        confirmed = self.oauth_store.remove(identity)
        self.profiles = [p for p in self.profiles if p["id"] != identity]
        self.profile_revision += 1
        self.watch_states = {k: v for k, v in self.watch_states.items() if k[0] != identity}
        self.comment_states = {k: v for k, v in self.comment_states.items() if k[0] != identity}
        self.chat_sends = {k: v for k, v in self.chat_sends.items() if k[0] != identity}
        self.watched_intents = {k: v for k, v in self.watched_intents.items() if k[0] != identity}
        self.feedback_states = {k: v for k, v in self.feedback_states.items() if k[0] != identity}
        self.account_intents = {k: v for k, v in self.account_intents.items() if k[0] != identity}
        self.session_intents = {k: v for k, v in self.session_intents.items() if k[0] != identity}
        self.on_profile_removed(identity)
        await self._stop()  # retire all cached Node credentials/continuations
        if not confirmed:
            raise ServiceError("auth_storage_unconfirmed", "The account was removed, but disk synchronization was not confirmed. Restart the companion before adding accounts.", 507)
        return {"api_version": 1, "success": True, "profile": identity}

    def add_oauth_profile(self, profile):
        from .device_auth import credentials_valid
        if (len(self.profiles) >= 8 or any(p["id"] == profile["id"] for p in self.profiles)
                or not credentials_valid(profile.get("oauth")) or profile.get("cookie") != ""):
            raise ServiceError("auth_profiles_full", "The new account profile could not be registered.", 409)
        self.profiles.append(profile)
        self.profile_revision += 1

    async def _stop(self):
        # Tracking objects live in Node. Losing them must not silently recreate
        # an already attempted history mutation in a queued request.
        for state in self.watch_states.values():
            state["blocked"] = True
        process, self.process = self.process, None
        if process is not None:
            if process.returncode is None:
                try:
                    process.kill()
                except ProcessLookupError:
                    pass
            await process.wait()

    async def _exchange(self, payload):
        identity = secrets.token_hex(16)
        body = json.dumps({"id": identity, **payload}, ensure_ascii=False).encode("utf-8") + b"\n"
        self.process.stdin.write(body)
        await self.process.stdin.drain()
        response = await self.process.stdout.readline()
        if not response or len(response) > 1048576:
            raise ServiceError("library_transport", "The library worker returned an invalid response.")
        value = json.loads(response)
        if not isinstance(value, dict) or value.get("id") != identity:
            raise ServiceError("library_transport", "The library worker returned an invalid response identity.")
        updates = value.get("credential_updates", [])
        if not isinstance(updates, list) or len(updates) > 8 or (updates and self.oauth_store is None):
            raise ServiceError("library_transport", "The library worker returned invalid credential updates.")
        for update in updates:
            if not isinstance(update, dict) or set(update) != {"profile", "credentials"}:
                raise ServiceError("library_transport", "The library worker returned invalid credential updates.")
            self.oauth_store.update(update["profile"], update["credentials"])
            for profile in self.profiles:
                if profile["id"] == update["profile"]:
                    profile["oauth"] = update["credentials"]
        if "error" in value:
            error = value["error"]
            # Only the original worker's bounded public errors cross this boundary.
            code = error.get("code", "library_failure")
            status = error.get("status", 502)
            if not isinstance(code, str) or not re.fullmatch(r"[a-z_]{1,64}", code):
                code = "library_failure"
            if type(status) is not int or not 400 <= status <= 599:
                status = 502
            message = error.get("message", "The library operation failed.")
            if not isinstance(message, str) or len(message) > 256:
                message = "The library operation failed."
            raise ServiceError(code, message, status)
        result = value.get("result")
        if not isinstance(result, dict):
            raise ServiceError("library_transport", "The library worker returned invalid operation data.")
        return result

    async def _start(self):
        if self.process is not None and self.process.returncode is None and self.worker_revision == self.profile_revision:
            return
        if self.process is not None:
            await self._stop()
        self.process = await asyncio.create_subprocess_exec(
            str(self.node), str(self.worker), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
            limit=1048577,
            # The proxy address may hold a password: environment, never arguments.
            env={**os.environ, "PROSPEROTUBE_PROXY": self.proxy} if self.proxy else None)
        result = await self._exchange({"operation": "init", "profiles": self.profiles})
        if result.get("ready") is not True:
            raise ServiceError("library_transport", "The library worker did not initialize.")
        self.worker_revision = self.profile_revision

    def _watch_state(self, request, *, create=False):
        if request.get("action") != "watch_progress":
            return None
        owner, session, identifier = request.get("profile", "guest"), request.get("session", ""), request.get("id", "")
        if (not isinstance(owner, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", owner)
                or not isinstance(session, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", session)
                or not isinstance(identifier, str) or not re.fullmatch(r"[A-Za-z0-9_-]{11}", identifier)):
            raise ServiceError("invalid_progress", "The playback report is invalid.", 400)
        if owner == "guest":
            raise ServiceError("account_required", "Select an account profile first.", 401)
        if not any(profile["id"] == owner for profile in self.profiles):
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        now = time.monotonic()
        self.watch_states = {k: v for k, v in self.watch_states.items() if v["deadline"] > now}
        key = (owner, session)
        state = self.watch_states.get(key)
        if state and state["id"] != identifier:
            raise ServiceError("invalid_progress", "The playback session belongs to another video.", 400)
        if state and state["blocked"]:
            raise ServiceError("history_unconfirmed", "The history change was not confirmed; this session will not repeat it.", 409)
        if state is None and create:
            if len(self.watch_states) >= 64:
                raise ServiceError("history_busy", "Too many playback sessions are retained.", 429)
            state = {"id": identifier, "blocked": False, "deadline": now + 1800}
            self.watch_states[key] = state
        return state

    def _comment_state(self, request, *, create=False):
        if request.get("action") != "comment_rate":
            return None
        owner, cursor, rating = request.get("profile", "guest"), request.get("cursor", ""), request.get("rating")
        if (not isinstance(owner, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", owner)
                or not isinstance(cursor, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", cursor)
                or rating not in ("like", "dislike", "none")):
            raise ServiceError("invalid_rating", "Select an available comment rating.", 400)
        if owner == "guest":
            raise ServiceError("account_required", "Select an account profile first.", 401)
        if not self.has_profile(owner):
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        now = time.monotonic()
        self.comment_states = {k: v for k, v in self.comment_states.items() if v["deadline"] > now}
        state = self.comment_states.get((owner, cursor))
        if state and (state["rating"] != rating or state["result"] is None):
            raise ServiceError("comment_unconfirmed", "Reload the comments; this rating intent cannot be repeated.", 409)
        if state is None and create:
            if len(self.comment_states) >= 256:
                raise ServiceError("comment_busy", "Too many comment rating intents are retained.", 429)
            state = {"rating": rating, "result": None, "deadline": now + 1800}
            self.comment_states[(owner, cursor)] = state
        return state

    def _chat_send_state(self, request, *, create=False):
        if request.get('action') != 'chat_send':
            return None
        owner, intent, cursor = request.get('profile', 'guest'), request.get('session', ''), request.get('cursor', '')
        text, video = request.get('text'), request.get('id')
        if (not isinstance(intent, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', intent)
                or not isinstance(cursor, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', cursor)
                or not isinstance(video, str) or not re.fullmatch(r'[A-Za-z0-9_-]{11}', video)
                or not isinstance(text, str) or not text.strip() or len(text) > 200
                or re.search(r'[\x00-\x1f\x7f]', text)):
            raise ServiceError('invalid_message', 'Enter a message of 1 to 200 characters.', 400)
        if owner == 'guest':
            raise ServiceError('account_required', 'Select an account profile first.', 401)
        if not self.has_profile(owner):
            raise ServiceError('unknown_profile', 'Select an available profile.', 404)
        now = time.monotonic()
        self.chat_sends = {k: v for k, v in self.chat_sends.items() if v['deadline'] > now}
        identity = (cursor, video, hashlib.sha256(text.encode('utf-8')).hexdigest())
        state = self.chat_sends.get((owner, intent))
        if state and (state['identity'] != identity or state['result'] is None):
            raise ServiceError('chat_unconfirmed', 'Check the chat; this message intent cannot be repeated.', 409)
        if state is None and create:
            if len(self.chat_sends) >= 256:
                raise ServiceError('chat_busy', 'Too many message intents are retained.', 429)
            state = {'identity': identity, 'result': None, 'deadline': now + 1800}
            self.chat_sends[(owner, intent)] = state
        return state

    def _watched_intent(self, request, *, create=False):
        if request.get('action') != 'mark_watched':
            return None
        owner, session, video = request.get('profile', 'guest'), request.get('session', ''), request.get('id', '')
        if (not isinstance(session, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', session)
                or not isinstance(video, str) or not re.fullmatch(r'[A-Za-z0-9_-]{11}', video)):
            raise ServiceError('invalid_progress', 'A video and confirmation session are required.', 400)
        if owner == 'guest':
            raise ServiceError('account_required', 'Select an account profile first.', 401)
        if not self.has_profile(owner):
            raise ServiceError('unknown_profile', 'Select an available profile.', 404)
        now = time.monotonic()
        self.watched_intents = {k: v for k, v in self.watched_intents.items() if v['deadline'] > now}
        state = self.watched_intents.get((owner, session))
        if state and (state['id'] != video or state['result'] is None):
            raise ServiceError('history_unconfirmed', 'Check account history before repeating this change.', 409)
        if state is None and create:
            if len(self.watched_intents) >= 256:
                raise ServiceError('history_busy', 'Too many history changes are retained.', 429)
            state = {'id': video, 'result': None, 'deadline': now + 1800}
            self.watched_intents[(owner, session)] = state
        return state

    def _feedback_state(self, request, *, create=False):
        if request.get('action') != 'feedback':
            return None
        owner, cursor = request.get('profile', 'guest'), request.get('cursor', '')
        video, choice = request.get('id', ''), request.get('feedback')
        if (not isinstance(owner, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', owner)
                or not isinstance(cursor, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', cursor)
                or not isinstance(video, str) or not re.fullmatch(r'[A-Za-z0-9_-]{11}', video)
                or choice not in ('not_interested', 'dont_recommend_channel', 'hide')):
            raise ServiceError('invalid_feedback', 'Select an available feedback choice.', 400)
        if owner == 'guest':
            raise ServiceError('account_required', 'Select an account profile first.', 401)
        if not self.has_profile(owner):
            raise ServiceError('unknown_profile', 'Select an available profile.', 404)
        now = time.monotonic()
        self.feedback_states = {k: v for k, v in self.feedback_states.items() if v['deadline'] > now}
        state = self.feedback_states.get((owner, cursor, choice))
        if state and (state['id'] != video or state['result'] is None):
            raise ServiceError('feedback_unconfirmed', 'Check the list before repeating this feedback.', 409)
        if state is None and create:
            if len(self.feedback_states) >= 256:
                raise ServiceError('feedback_busy', 'Too many feedback intents are retained.', 429)
            state = {'id': video, 'result': None, 'deadline': now + 1800}
            self.feedback_states[(owner, cursor, choice)] = state
        return state

    ACCOUNT_RESULTS = {'playlist_save': ('saved', True), 'playlist_unsave': ('saved', False),
                       'history_pause': ('history', 'paused'), 'history_resume': ('history', 'active'),
                       'history_clear': ('history', 'cleared')}

    def _account_intent(self, request, *, create=False):
        """Playlist library membership and history controls: one attempt per confirmation session."""
        action = request.get('action')
        if action not in self.ACCOUNT_RESULTS:
            return None
        owner, session, playlist = request.get('profile', 'guest'), request.get('session', ''), request.get('playlist', '')
        saving = action.startswith('playlist_')
        if (not isinstance(owner, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', owner)
                or not isinstance(session, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', session)
                or (saving and (not isinstance(playlist, str) or playlist in ('WL', 'LL')
                                or not re.fullmatch(r'[A-Za-z0-9_-]{10,100}', playlist)))):
            raise ServiceError('invalid_request', 'The account change needs a confirmation session and a valid target.', 400)
        if owner == 'guest':
            raise ServiceError('account_required', 'Select an account profile first.', 401)
        if not self.has_profile(owner):
            raise ServiceError('unknown_profile', 'Select an available profile.', 404)
        now = time.monotonic()
        self.account_intents = {k: v for k, v in self.account_intents.items() if v['deadline'] > now}
        identity = (action, playlist if saving else '')
        state = self.account_intents.get((owner, session))
        if state and (state['identity'] != identity or state['result'] is None):
            raise ServiceError('playlist_unconfirmed' if saving else 'history_unconfirmed',
                               'Check the account before repeating this change.', 409)
        if state is None and create:
            if len(self.account_intents) >= 256:
                raise ServiceError('account_busy', 'Too many account changes are retained.', 429)
            state = {'identity': identity, 'result': None, 'deadline': now + 1800}
            self.account_intents[(owner, session)] = state
        return state

    # Third-batch changes: action -> (fields that identify the change, unconfirmed code,
    # result fields kept). Reads of the same actions (`status`, empty order) are not intents.
    SESSION_INTENTS = {
        'stream_reminder': (('id', 'reminder'), 'reminder_unconfirmed', ('api_version', 'success', 'id', 'reminder_set')),
        'playlist_order': (('playlist', 'playlist_order'), 'playlist_unconfirmed', ('api_version', 'success', 'playlist_id', 'order')),
        'comment_post': (('id', 'text'), 'comment_unconfirmed', ('api_version', 'success', 'id', 'session', 'reply')),
        'comment_reply': (('id', 'cursor', 'text'), 'comment_unconfirmed', ('api_version', 'success', 'id', 'session', 'reply')),
    }
    # The worker sent nothing for these errors, so the confirmation may be used again.
    NOTHING_SENT = {'history_control_unavailable', 'reminder_unavailable', 'playlist_order_unavailable'}

    def _session_intent(self, request, *, create=False):
        """Reminders, playlist order and comment posts: one attempt per confirmation session."""
        action = request.get('action')
        if (action not in self.SESSION_INTENTS or (action == 'stream_reminder' and request.get('reminder') == 'status')
                or (action == 'playlist_order' and not request.get('playlist_order'))):
            return None
        fields, code, _ = self.SESSION_INTENTS[action]
        owner, session = request.get('profile', 'guest'), request.get('session', '')
        values = [request.get(field) for field in fields]
        if (not isinstance(owner, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,32}', owner)
                or not isinstance(session, str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', session)
                or not all(isinstance(value, str) and value for value in values)):
            raise ServiceError('invalid_request', 'The change needs a confirmation session and a valid target.', 400)
        if owner == 'guest':
            raise ServiceError('account_required', 'Select an account profile first.', 401)
        if not self.has_profile(owner):
            raise ServiceError('unknown_profile', 'Select an available profile.', 404)
        now = time.monotonic()
        self.session_intents = {k: v for k, v in self.session_intents.items() if v['deadline'] > now}
        identity = (action, hashlib.sha256('\x00'.join(values).encode('utf-8')).hexdigest())
        state = self.session_intents.get((owner, session))
        if state and (state['identity'] != identity or state['result'] is None):
            raise ServiceError(code, 'Check the account before repeating this change.', 409)
        if state is None and create:
            if len(self.session_intents) >= 256:
                raise ServiceError('account_busy', 'Too many account changes are retained.', 429)
            state = {'identity': identity, 'result': None, 'deadline': now + 1800}
            self.session_intents[(owner, session)] = state
        return state

    def _confirm_session_intent(self, request, result):
        action = request['action']
        _, code, keys = self.SESSION_INTENTS[action]
        expected = ({'id': request['id'], 'reminder_set': request['reminder'] == 'set'} if action == 'stream_reminder' else
                    {'playlist_id': request['playlist'], 'order': request['playlist_order']} if action == 'playlist_order' else
                    {'id': request['id'], 'session': request['session'], 'reply': action == 'comment_reply'})
        if (result.get('api_version') != 1 or result.get('success') is not True
                or any(result.get(key) != value or type(result.get(key)) is not type(value) for key, value in expected.items())):
            raise ServiceError(code, 'YouTube did not confirm the change.', 502)
        return {key: result[key] for key in keys}

    async def execute(self, request, *, operation="execute"):
        if self.closed:
            raise ServiceError("library_closed", "The library service has stopped.", 503)
        if operation not in ("execute", "device", "playback") or not isinstance(request, dict) or len(json.dumps(request).encode("utf-8")) > (16384 if operation == "device" else 8192):
            raise ServiceError("library_request_limit", "The library operation exceeds its request limit.", 400)
        if self.pending >= 16:
            raise ServiceError("library_busy", "The library is busy. Retry shortly.", 429)
        self.pending += 1
        owned = False
        try:
            async with asyncio.timeout(self.timeout):
                async with self.lock:
                    if self.closed:
                        raise ServiceError("library_closed", "The library service has stopped.", 503)
                    owned = True
                    if operation == "execute" and request.get("action") == "profile_remove":
                        return await self._remove_profile(request)
                    chat_state = self._chat_send_state(request) if operation == 'execute' else None
                    if chat_state is not None:
                        return dict(chat_state['result'])
                    comment_state = self._comment_state(request) if operation == "execute" else None
                    if comment_state is not None:
                        return dict(comment_state["result"])
                    self._watch_state(request)
                    watched_intent = self._watched_intent(request) if operation == 'execute' else None
                    if watched_intent is not None:
                        return dict(watched_intent['result'])
                    feedback_state = self._feedback_state(request) if operation == 'execute' else None
                    if feedback_state is not None:
                        return dict(feedback_state['result'])
                    account_intent = self._account_intent(request) if operation == 'execute' else None
                    if account_intent is not None:
                        return dict(account_intent['result'])
                    session_intent = self._session_intent(request) if operation == 'execute' else None
                    if session_intent is not None:
                        return dict(session_intent['result'])
                    await self._start()
                    session_intent = self._session_intent(request, create=True) if operation == 'execute' else None
                    account_intent = self._account_intent(request, create=True) if operation == 'execute' else None
                    feedback_state = self._feedback_state(request, create=True) if operation == 'execute' else None
                    watched_intent = self._watched_intent(request, create=True) if operation == 'execute' else None
                    chat_state = self._chat_send_state(request, create=True) if operation == 'execute' else None
                    comment_state = self._comment_state(request, create=True) if operation == "execute" else None
                    watch_state = self._watch_state(request, create=True)
                    if watch_state is not None:
                        watch_state["blocked"] = True
                    result = await self._exchange({"operation": operation, "request": request})
                    if session_intent is not None:
                        session_intent['result'] = self._confirm_session_intent(request, result)
                        result = dict(session_intent['result'])
                    if account_intent is not None:
                        field, expected = self.ACCOUNT_RESULTS[request['action']]
                        saving = field == 'saved'
                        if (result.get('api_version') != 1 or result.get('success') is not True
                                or result.get(field) != expected or type(result.get(field)) is not type(expected)
                                or (saving and result.get('playlist_id') != request['playlist'])):
                            raise ServiceError('playlist_unconfirmed' if saving else 'history_unconfirmed',
                                               'YouTube did not confirm the account change.', 502)
                        keys = ('api_version', 'success', 'playlist_id', 'saved') if saving else ('api_version', 'success', 'history')
                        account_intent['result'] = {key: result[key] for key in keys}
                        result = dict(account_intent['result'])
                    if feedback_state is not None:
                        if (result.get('api_version') != 1 or result.get('success') is not True
                                or result.get('video_id') != request['id'] or result.get('feedback') != request['feedback']):
                            raise ServiceError('feedback_unconfirmed', 'YouTube did not confirm this feedback.', 502)
                        feedback_state['result'] = {key: result[key] for key in ('api_version', 'success', 'video_id', 'feedback')}
                        result = dict(feedback_state['result'])
                    if watched_intent is not None:
                        position = result.get('position_sec')
                        if (result.get('api_version') != 1 or result.get('success') is not True
                                or result.get('video_id') != request['id'] or type(position) not in (int, float)
                                or not 0 < position <= 1000000):
                            raise ServiceError('history_unconfirmed', 'YouTube did not confirm this video as watched.', 502)
                        watched_intent['result'] = dict(result)
                    if chat_state is not None:
                        message = result.get('message_id')
                        if (result.get('api_version') != 1 or result.get('success') is not True or result.get('id') != request['id']
                                or result.get('session') != request['session'] or not isinstance(message, str)
                                or not 1 <= len(message) <= 160 or re.search(r'[\x00-\x1f\x7f]', message)):
                            raise ServiceError('chat_unconfirmed', 'YouTube did not confirm this message.', 502)
                        chat_state['result'] = {key: result[key] for key in ('api_version', 'success', 'id', 'session', 'message_id')}
                        result = dict(chat_state['result'])
                    if comment_state is not None:
                        if result.get("success") is not True or result.get("rating") != request["rating"]:
                            raise ServiceError("comment_unconfirmed", "YouTube did not accept the comment rating.", 502)
                        comment_state["result"] = dict(result)
                    if watch_state is not None:
                        if (result.get("success") is not True or type(result.get("position_sec")) not in (int, float)
                                or result.get("position_sec") != request.get("position_sec")):
                            raise ServiceError("history_unconfirmed", "The history change was not confirmed; this session will not repeat it.", 502)
                        watch_state["blocked"] = False
                        watch_state["deadline"] = time.monotonic() + 1800
                    return result
        except asyncio.CancelledError:
            if owned:
                await self._stop()
            raise
        except TimeoutError:
            if owned:
                await self._stop()
            raise ServiceError("library_timeout", "The operation timed out. Check the account before repeating a change.", 504) from None
        except ServiceError as error:
            if owned and error.code == "library_transport":
                await self._stop()
            if owned and error.code in self.NOTHING_SENT:
                # The worker sent nothing, so this confirmation may be used again.
                key = (request.get("profile", "guest"), request.get("session", ""))
                for intents in (self.account_intents, self.session_intents):
                    if (intents.get(key) or {}).get("result", 0) is None:
                        del intents[key]
            raise
        except (OSError, ValueError, ConnectionError):
            if owned:
                await self._stop()
            raise ServiceError("library_transport", "The library worker is unavailable. Check its installation.", 503) from None
        finally:
            self.pending -= 1

    async def close(self):
        self.closed = True
        async with self.lock:
            await self._stop()
