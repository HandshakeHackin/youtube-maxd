"""Device-code authorization; public status never includes credential material."""
import asyncio
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import secrets
import tempfile
import time
from urllib.parse import urlsplit

from .models import ServiceError


def _scalar(value, maximum):
    return isinstance(value, str) and 0 < len(value) <= maximum and not re.search(r"[^\x21-\x7e]", value)


def credentials_valid(value):
    if not isinstance(value, dict) or set(value) != {"access_token", "refresh_token", "expiry_date", "client"}:
        return False
    client = value["client"]
    if (not isinstance(client, dict) or set(client) != {"client_id", "client_secret"}
            or not all(_scalar(client[k], 512) for k in client)
            or not _scalar(value["access_token"], 8192) or not _scalar(value["refresh_token"], 8192)):
        return False
    try:
        expiry = value["expiry_date"]
        if not isinstance(expiry, str) or len(expiry) > 40:
            return False
        date = datetime.fromisoformat(expiry.replace("Z", "+00:00"))
        return date.tzinfo is not None and math.isfinite(date.timestamp())
    except (ValueError, OverflowError, TypeError):
        return False


class OAuthProfiles:
    """Private, bounded, atomic credential file. Corruption never becomes empty state."""
    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        os.chmod(self.directory, 0o700)
        self.path = self.directory / "oauth-profiles.json"
        self.lock_path = self.directory / "oauth-profiles.lock"
        self.snapshot = None
        self.profiles = []
        if self.path.exists() or self.path.is_symlink():
            if self.path.is_symlink() or not self.path.is_file() or self.path.stat().st_size > 196608:
                raise ValueError("The OAuth profile file is invalid.")
            self.snapshot = self.path.read_bytes()
            data = json.loads(self.snapshot)
            if not isinstance(data, dict) or set(data) != {"version", "profiles"} or type(data["version"]) is not int or data["version"] != 1:
                raise ValueError("The OAuth profile file has an unsupported format.")
            profiles = data["profiles"]
            if not isinstance(profiles, list) or len(profiles) > 8:
                raise ValueError("The OAuth profile file is invalid.")
            identities = set()
            for profile in profiles:
                if (not isinstance(profile, dict) or set(profile) != {"id", "name", "cookie", "oauth"}
                        or not isinstance(profile.get("id"), str)
                        or not re.fullmatch(r"oauth_[0-9a-f]{16}", profile.get("id", ""))
                        or profile["id"] in identities or profile["cookie"] != ""
                        or not isinstance(profile["name"], str) or not 1 <= len(profile["name"]) <= 150
                        or not credentials_valid(profile["oauth"])):
                    raise ValueError("The OAuth profile file is invalid.")
                identities.add(profile["id"])
            self.profiles = profiles
            os.chmod(self.path, 0o600)

    def _save(self, profiles):
        data = json.dumps({"version": 1, "profiles": profiles}, ensure_ascii=False).encode("utf-8")
        if len(data) > 196608:
            raise ServiceError("auth_storage", "The account credential store is full.", 507)
        lock_token = secrets.token_hex(16)
        try:
            lock_descriptor = os.open(self.lock_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        except FileExistsError:
            raise ServiceError("auth_storage_busy", "The account credential store is in use.", 409) from None
        temporary = None
        committed = False
        try:
            with os.fdopen(lock_descriptor, "w") as lock:
                lock.write(lock_token)
            current = None
            if self.path.exists():
                if not self.path.is_file():
                    raise ServiceError("auth_storage_changed", "The account credential store changed. Restart the companion.", 409)
                with self.path.open("rb") as existing:
                    current = existing.read(196609)
            if self.path.is_symlink() or current != self.snapshot:
                raise ServiceError("auth_storage_changed", "The account credential store changed. Restart the companion.", 409)
            descriptor, temporary = tempfile.mkstemp(prefix=".oauth-", dir=self.directory)
            with os.fdopen(descriptor, "wb") as stream:
                os.chmod(temporary, 0o600)
                stream.write(data); stream.flush(); os.fsync(stream.fileno())
            os.replace(temporary, self.path)
            committed = True
            if hasattr(os, "O_DIRECTORY"):
                directory_descriptor = os.open(self.directory, os.O_RDONLY | os.O_DIRECTORY)
                try:
                    os.fsync(directory_descriptor)
                finally:
                    os.close(directory_descriptor)
        finally:
            if committed:
                self.snapshot = data
                self.profiles = profiles
            if temporary is not None and os.path.exists(temporary):
                os.unlink(temporary)
            if self.lock_path.read_text() == lock_token:
                self.lock_path.unlink()
        self.snapshot = data
        self.profiles = profiles

    def append(self, credentials, name):
        if not credentials_valid(credentials) or not isinstance(name, str) or not 1 <= len(name) <= 150:
            raise ServiceError("auth_response", "The account credentials are invalid.", 502)
        if len(self.profiles) >= 8:
            raise ServiceError("auth_profiles_full", "At most eight account profiles are supported.", 409)
        profile = {"id": "oauth_" + secrets.token_hex(8), "name": name, "cookie": "", "oauth": credentials}
        self._save([*self.profiles, profile])
        return profile

    def update(self, identity, credentials):
        existing = next((p for p in self.profiles if p["id"] == identity), None)
        if not existing or not credentials_valid(credentials) or credentials["client"] != existing["oauth"]["client"]:
            raise ServiceError("auth_response", "The refreshed account credentials are invalid.", 502)
        profiles = [({**p, "oauth": credentials} if p["id"] == identity else p) for p in self.profiles]
        self._save(profiles)

    def remove(self, identity):
        if not any(p["id"] == identity for p in self.profiles):
            raise ServiceError("unknown_profile", "Select an available profile.", 404)
        remaining = [p for p in self.profiles if p["id"] != identity]
        try:
            self._save(remaining)
        except OSError:
            if self.profiles == remaining:
                return False  # replace committed; directory sync/cleanup unconfirmed
            raise ServiceError("auth_storage", "The account could not be removed from private storage.", 507) from None
        return True


class DeviceAuth:
    def __init__(self, execute, store, register, capacity, *, clock=time.monotonic):
        self.execute, self.store, self.register, self.capacity = execute, store, register, capacity
        self.clock = clock
        self.sessions = {}
        self.active = None
        self.closed = False

    def forget_profile(self, identity):
        for session in self.sessions.values():
            if session.get("profile", {}).get("id") == identity:
                session.pop("profile", None)
                session["state"] = "canceled"

    def _get(self, identity):
        session = self.sessions.get(identity)
        if not session:
            raise ServiceError("auth_session", "Start a new sign-in session.", 404)
        if session["state"] == "pending" and self.clock() >= session["deadline"]:
            session["state"] = "expired"
            session.pop("client", None); session.pop("device_code", None)
            if self.active is session:
                self.active = None
        return session

    def _public(self, session):
        value = {"session": session["id"], "state": session["state"]}
        if session["state"] == "pending":
            value.update(user_code=session["user_code"], verification_url=session["verification_url"],
                         expires_in=max(0, math.ceil(session["deadline"] - self.clock())),
                         poll_after=max(0, math.ceil(session["next_poll"] - self.clock())))
        if session["state"] == "authorized":
            value["profile"] = session["profile"]
        if session["state"] == "failed":
            value["error"] = "YouTube sign-in could not be completed. Start a new session."
        return value

    async def start(self, name="YouTube account"):
        if self.closed:
            raise ServiceError("auth_closed", "The sign-in service has stopped.", 503)
        if self.active and self.active["state"] in ("starting", "pending"):
            if self.active["state"] == "pending":
                self._get(self.active["id"])
            if self.active:
                raise ServiceError("auth_busy", "A sign-in session is already pending.", 409)
        if not self.capacity():
            raise ServiceError("auth_profiles_full", "At most eight account profiles are supported.", 409)
        now = self.clock()
        self.sessions = {k: v for k, v in self.sessions.items() if v["retained_until"] > now}
        if len(self.sessions) >= 16:
            raise ServiceError("auth_busy", "Too many sign-in sessions are retained.", 429)
        if not isinstance(name, str) or not 1 <= len(name.strip()) <= 150 or re.search(r"[\x00-\x1f\x7f]", name):
            raise ServiceError("auth_name", "Enter a valid account name.", 400)
        session = {"id": secrets.token_hex(16), "state": "starting", "polling": False, "task": asyncio.current_task()}
        self.active = session
        try:
            result = await self.execute({"action": "start"})
            if self.active is not session or session["state"] != "starting":
                raise asyncio.CancelledError
            client = result.get("client")
            url = urlsplit(result.get("verification_url", ""))
            if (not isinstance(client, dict) or set(client) != {"client_id", "client_secret"}
                    or not all(_scalar(client[k], 512) for k in client)
                    or not _scalar(result.get("device_code"), 8192)
                    or not isinstance(result.get("user_code"), str)
                    or not re.fullmatch(r"[A-Za-z0-9-]{1,32}", result["user_code"])
                    or url.scheme != "https" or url.netloc not in ("www.google.com", "google.com", "www.youtube.com", "youtube.com")
                    or url.path not in ("/device", "/activate") or url.query or url.fragment
                    or type(result.get("expires_in")) is not int or not 1 <= result["expires_in"] <= 1800
                    or type(result.get("interval")) is not int or not 1 <= result["interval"] <= 60):
                raise ServiceError("auth_response", "The sign-in response is invalid.", 502)
            if self.closed:
                raise ServiceError("auth_closed", "The sign-in service has stopped.", 503)
            now = self.clock()
            session.update(state="pending", name=name.strip(), client=client, device_code=result["device_code"],
                           user_code=result["user_code"], verification_url=result["verification_url"],
                           deadline=now + result["expires_in"], interval=max(5, result["interval"]),
                           next_poll=now + max(5, result["interval"]), retained_until=now + 3600)
            self.sessions[session["id"]] = session
            return self._public(session)
        except asyncio.CancelledError:
            session["state"] = "failed"
            if self.active is session:
                self.active = None
            raise
        except Exception:
            session["state"] = "failed"
            if self.active is session:
                self.active = None
            raise ServiceError("auth_failed", "YouTube sign-in could not be started. Start a new session.", 502) from None
        finally:
            session.pop("task", None)

    async def status(self, identity):
        session = self._get(identity)
        if session["state"] != "pending" or session["polling"] or self.clock() < session["next_poll"]:
            return self._public(session)
        session["polling"] = True
        session["task"] = asyncio.current_task()
        session["next_poll"] = self.clock() + session["interval"]
        try:
            result = await self.execute({"action": "poll", "client": session["client"], "device_code": session["device_code"]})
            if self.active is not session or self._get(identity)["state"] != "pending":
                return self._public(session)  # Canceled/expired in-flight credentials are discarded.
            state = result.get("state")
            if state == "pending" and type(result.get("slow_down")) is bool:
                if result["slow_down"]:
                    session["interval"] = min(60, session["interval"] + 5)
                session["next_poll"] = self.clock() + session["interval"]
            elif state in ("denied", "expired"):
                session["state"] = state; self.active = None
            elif state == "authorized" and credentials_valid(result.get("credentials")):
                if not self.capacity():
                    raise ServiceError("auth_profiles_full", "At most eight account profiles are supported.", 409)
                name = result.get("name")
                if session["name"] != "YouTube account" or not isinstance(name, str) or not 1 <= len(name) <= 150 or re.search(r"[\x00-\x1f\x7f]", name):
                    name = session["name"]
                profile = self.store.append(result["credentials"], name)
                self.register(profile)  # Synchronous commit: cancellation cannot interleave with persistence.
                session.update(state="authorized", profile={"id": profile["id"], "name": profile["name"]})
                self.active = None
            else:
                raise ServiceError("auth_response", "The sign-in response is invalid.", 502)
            return self._public(session)
        except asyncio.CancelledError:
            if session["state"] == "canceled":
                return self._public(session)
            session["state"] = "canceled"
            if self.active is session:
                self.active = None
            raise
        except Exception:
            if session["state"] == "pending":
                session["state"] = "failed"
            if self.active is session:
                self.active = None
            raise ServiceError("auth_failed", "YouTube sign-in could not be completed. Start a new session.", 502) from None
        finally:
            session["polling"] = False
            session.pop("task", None)
            if session["state"] != "pending":
                session.pop("client", None); session.pop("device_code", None)

    def cancel(self, identity):
        session = self._get(identity)
        if session["state"] == "pending":
            session["state"] = "canceled"
            session.pop("client", None); session.pop("device_code", None)
            if self.active is session:
                self.active = None
            task = session.get("task")
            if task and not task.done():
                task.cancel()
        return self._public(session)

    def cancel_task(self, task):
        """Invalidate a canceled job before a late worker response can commit."""
        if self.active and self.active.get("task") is task:
            if self.active["state"] == "pending":
                self.cancel(self.active["id"])
            else:
                self.active["state"] = "canceled"
                self.active = None
                task.cancel()

    def close(self):
        self.closed = True
        if self.active:
            task = self.active.get("task")
            if task and not task.done():
                task.cancel()
            self.active["state"] = "canceled"
            self.active.pop("client", None); self.active.pop("device_code", None)
            self.active = None
