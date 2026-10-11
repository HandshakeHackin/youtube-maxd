"""Opt-in YouTube Lounge receiver. No console operations or account credentials.

Protocol reference: MediaServiceCore 4d976773, lounge/; BrowserChannel framing
follows Closure's channelrequest.js (UTF-16 code units, not UTF-8 bytes).
Native clients acknowledge commands only after applying them on their UI thread.
"""
import asyncio
import codecs
from collections import deque
from dataclasses import dataclass, field
import json
import math
import re
import secrets
import time
import uuid

import httpx
from pydantic import BaseModel, ConfigDict, Field

from .models import ServiceError, VIDEO_ID

BASE = "https://www.youtube.com/api/lounge"
MAX_FRAME = 262_144
MAX_QUEUE = 128
MAX_RECEIVERS = 4
LEASE_SECONDS = 120


def failure(code="remote_protocol", status=502):
    # Never interpolate HTTP exceptions, URLs, server bodies, or Lounge tokens.
    return ServiceError(code, "YouTube phone connection stopped. Enable it again to reconnect.", status)


def strict_json(value):
    def reject(_):
        raise ValueError("nonfinite JSON")
    return json.loads(value, parse_constant=reject)


class Frames:
    """Incremental application framing, including fragmented multibyte names."""
    def __init__(self):
        self.decoder = codecs.getincrementaldecoder("utf-8")("strict")
        self.buffer = b""
        self.length = None

    def feed(self, data, *, final=False):
        try:
            self.buffer += self.decoder.decode(data, final).encode("utf-16-le")
            if len(self.buffer) > MAX_FRAME * 2 + 32:
                raise ValueError("frame bound")
            result = []
            while True:
                if self.length is None:
                    end = self.buffer.find(b"\n\0")
                    while end >= 0 and end % 2:
                        end = self.buffer.find(b"\n\0", end + 1)
                    if end < 0:
                        if len(self.buffer) > 16:
                            raise ValueError("length bound")
                        break
                    header = self.buffer[:end].decode("utf-16-le")
                    if not re.fullmatch(r"[0-9]{1,6}", header):
                        raise ValueError("frame length")
                    self.length = int(header) * 2
                    if not 0 < self.length <= MAX_FRAME * 2:
                        raise ValueError("frame bound")
                    self.buffer = self.buffer[end + 2:]
                if len(self.buffer) < self.length:
                    break
                value = strict_json(self.buffer[:self.length].decode("utf-16-le"))
                self.buffer = self.buffer[self.length:]
                self.length = None
                if not isinstance(value, list) or len(value) > 512:
                    raise ValueError("message bound")
                result.append(value)
            if final and (self.buffer or self.length is not None):
                raise ValueError("truncated frame")
            return result
        except (ValueError, UnicodeError, RecursionError):
            raise failure() from None


def bounded_text(value, limit, pattern=None):
    if not isinstance(value, str) or len(value) > limit or any(ord(c) < 32 for c in value):
        raise failure()
    if pattern is not None and not re.fullmatch(pattern, value):
        raise failure()
    return value


def number(value, minimum=0, maximum=1_000_000):
    if isinstance(value, bool) or not isinstance(value, (str, int, float)):
        raise failure()
    try:
        result = float(value)
    except (ValueError, OverflowError):
        raise failure() from None
    if not math.isfinite(result) or not minimum <= result <= maximum:
        raise failure()
    return result


def command(name, args, *, queue_state=None):
    """Expose typed allowlisted commands, never arbitrary wire parameters."""
    if not isinstance(args, dict) or len(args) > 32:
        raise failure()
    result = {"action": name}
    if name in ("play", "pause", "next", "previous", "stopVideo", "getNowPlaying"):
        return result
    if name in ("setPlaylist", "updatePlaylist"):
        video = bounded_text(args.get("videoId", ""), 11)
        ids = bounded_text(args.get("videoIds", ""), 119999).split(",") if args.get("videoIds") else []
        if len(ids) > 10000 or any(not VIDEO_ID.fullmatch(v) for v in ids):
            raise failure()
        index = number(args.get("currentIndex", 0), maximum=100_000)
        if not index.is_integer() or (video and not VIDEO_ID.fullmatch(video)):
            raise failure()
        if name == "setPlaylist" and not video:
            if not ids or index >= len(ids):
                raise failure()
            video = ids[int(index)]
        result.update(video_id=video, video_ids=ids, playlist=bounded_text(args.get("listId", ""), 100, r"[A-Za-z0-9_-]*"),
                      index=int(index), position_sec=number(args.get("currentTime", 0)),
                      ctt=bounded_text(args.get("ctt", ""), 512))
        if len(ids) > 128:
            if not video:
                if index >= len(ids): raise failure()
                video = ids[int(index)]
                result["video_id"] = video
            anchor = int(index) if index < len(ids) and ids[int(index)] == video else ids.index(video) if video in ids else -1
            if anchor < 0 or index < anchor or int(index) - anchor + len(ids) - 1 > 100000:
                raise failure()
            base = int(index) - anchor
            start = max(0, min(anchor - 32, len(ids) - 128))
            result["video_ids"] = ids[start:start + 128]
            result["window_start"] = base + start
            if queue_state is not None:
                queue_state.update(ids=ids, base=base, playlist=result["playlist"], ctt=result["ctt"],
                                   window_start=base + start, window_end=base + start + 127)
    elif name == "seekTo":
        result["position_sec"] = number(args.get("newTime"))
    elif name == "setVolume":
        if "volume" not in args and "delta" not in args:
            raise failure()
        if "volume" in args:
            result["volume"] = number(args["volume"], maximum=100)
        if "delta" in args:
            result["delta"] = number(args["delta"], -100, 100)
    elif name in ("remoteConnected", "remoteDisconnected"):
        result.update(device_id=bounded_text(args.get("id", ""), 128), name=bounded_text(args.get("name", ""), 100))
    elif name == "loungeStatus":
        raw = bounded_text(args.get("devices", "[]"), 32_768)
        try:
            devices = strict_json(raw)
        except (ValueError, RecursionError):
            raise failure() from None
        if not isinstance(devices, list) or len(devices) > 32:
            raise failure()
        result["devices"] = []
        for device in devices:
            if not isinstance(device, dict):
                raise failure()
            if device.get("type") == "LOUNGE_SCREEN":
                continue
            result["devices"].append({"device_id": bounded_text(device.get("id", ""), 128),
                                      "name": bounded_text(device.get("name", ""), 100)})
    elif name == "dpadCommand":
        result["key"] = bounded_text(args.get("key", ""), 8, r"UP|DOWN|LEFT|RIGHT|ENTER|BACK")
    elif name == "voiceCommand":
        result["status"] = bounded_text(args.get("status", ""), 12, r"INITIATED|CANCELED")
    elif name == "setSubtitlesTrack":
        result.update(video_id=bounded_text(args.get("videoId", ""), 11, r"(?:[A-Za-z0-9_-]{11})?"),
                      language=bounded_text(args.get("languageCode", ""), 32, r"[A-Za-z0-9_-]*"))
    else:
        return None
    return result


class PlayerState(BaseModel):
    model_config = ConfigDict(extra="forbid")
    video_id: str = Field(default="", pattern=r"^(|[A-Za-z0-9_-]{11})$")
    state: int = Field(default=2, strict=True, ge=-1, le=4)
    position_sec: float = Field(default=0, ge=0, le=1_000_000, allow_inf_nan=False, strict=True)
    duration_sec: float = Field(default=0, ge=0, le=1_000_000, allow_inf_nan=False, strict=True)
    playlist: str = Field(default="", max_length=100, pattern=r"^[A-Za-z0-9_-]*$")
    index: int = Field(default=0, strict=True, ge=0, le=100_000)
    ctt: str = Field(default="", max_length=512, pattern=r"^[^\x00-\x1f]*$")
    volume: int = Field(default=100, strict=True, ge=0, le=100)
    muted: bool = Field(default=False, strict=True)
    has_previous: bool = Field(default=False, strict=True)
    has_next: bool = Field(default=False, strict=True)
    language: str = Field(default="", max_length=32, pattern=r"^[A-Za-z0-9_-]*$")
    vss_id: str = Field(default="", max_length=64, pattern=r"^[A-Za-z0-9_.-]*$")


class RemoteRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    action: str = Field(pattern=r"^(start|pair|poll|update|stop)$")
    device_id: str = Field(default="", pattern=r"^(|[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})$")
    name: str = Field(default="ProsperoTube PS5", min_length=1, max_length=100, pattern=r"^[^\x00-\x1f]+$")
    screen_id: str = Field(default="", max_length=128, pattern=r"^[A-Za-z0-9_-]*$")
    session: str = Field(default="", pattern=r"^(|[0-9a-f]{32})$")
    ack: int = Field(default=0, strict=True, ge=0, le=2_147_483_647)
    update_id: int = Field(default=0, strict=True, ge=0, le=2_147_483_647)
    player: PlayerState | None = None
    voice_enabled: bool = Field(default=False, strict=True)
    profile: str = Field(default="guest", pattern=r"^[A-Za-z0-9_-]{1,32}$")


@dataclass
class Receiver:
    session: str
    device_id: str
    name: str
    screen_id: str = ""
    token: str = field(default="", repr=False)
    sid: str = field(default="", repr=False)
    gsid: str = field(default="", repr=False)
    code: str = field(default="", repr=False)
    aid: int = -1
    rid: int = 1
    ofs: int = 0
    sequence: int = 0
    delivered: int = 0
    ack: int = 0
    update_id: int = -1
    last_player: dict | None = None
    error: str = ""
    queue: deque = field(default_factory=deque, repr=False)
    task: asyncio.Task | None = field(default=None, repr=False)
    lock: asyncio.Lock = field(default_factory=asyncio.Lock, repr=False)
    touched: float = field(default_factory=time.monotonic)
    voice_enabled: bool = False
    full_playlist: dict | None = field(default=None, repr=False)
    profile: str = "guest"
    playlist_anchor: dict | None = field(default=None, repr=False)
    playlist_task: asyncio.Task | None = field(default=None, repr=False)
    playlist_retry: float = 0


class Lounge:
    def __init__(self, client=None, *, playlist_loader=None):
        self.http = client or httpx.AsyncClient(timeout=httpx.Timeout(65, connect=5), follow_redirects=False)
        self.owns_http = client is None
        self.receivers = {}
        self.lock = asyncio.Lock()
        self.closed = False
        self.playlist_loader = playlist_loader
        self.playlist_tasks = set()

    def status(self, receiver):
        return {"api_version": 1, "session": receiver.session, "device_id": receiver.device_id,
                "screen_id": receiver.screen_id, "name": receiver.name,
                "state": "failed" if receiver.error else "listening", "error": receiver.error}

    def params(self, receiver, *, rpc=False):
        # Native capture/search support and local ASR must both be available.
        params = {"device": "LOUNGE_SCREEN", "theme": "cl", "capabilities": "dsp,dpa" + (",mic" if receiver.voice_enabled else ""),
                  "mdxVersion": "2", "VER": "8", "CVER": "1", "v": "2", "t": "1", "app": "lb-v4",
                  "name": receiver.name, "id": receiver.device_id, "loungeIdToken": receiver.token,
                  "zx": secrets.token_hex(8), "RID": "rpc" if rpc else str(receiver.rid)}
        if receiver.sid:
            params.update(SID=receiver.sid, gsessionid=receiver.gsid, AID=str(receiver.aid))
        if rpc:
            params.update(CI="0", TYPE="xmlhttp")
        else:
            receiver.rid += 1
        return params

    async def response(self, method, path, *, params=None, data=None):
        async with self.http.stream(method, BASE + path, params=params, data=data) as response:
            if response.status_code != 200:
                raise failure("remote_upstream")
            body = bytearray()
            async for chunk in response.aiter_bytes(4096):
                body.extend(chunk)
                if len(body) > MAX_FRAME:
                    raise failure()
            return bytes(body)

    def accept(self, receiver, frames):
        for frame in frames:
            for message in frame:
                if (not isinstance(message, list) or len(message) != 2 or type(message[0]) is not int
                        or not 0 <= message[0] <= 2_147_483_647 or not isinstance(message[1], list)
                        or not 1 <= len(message[1]) <= 8):
                    raise failure()
                index, payload = message
                if index <= receiver.aid:
                    continue
                if index != receiver.aid + 1:
                    raise failure("remote_gap")
                name = bounded_text(payload[0], 64)
                if name in ("c", "S"):
                    if len(payload) < 2:
                        raise failure()
                    identity = bounded_text(payload[1], 256, r"[A-Za-z0-9_-]+")
                    if name == "c":
                        receiver.sid = identity
                    else:
                        receiver.gsid = identity
                elif name in ("stop", "close"):
                    raise failure("remote_disconnected")
                elif name != "noop":
                    queue_state = {}
                    typed = command(name, payload[1] if len(payload) > 1 else {}, queue_state=queue_state)
                    if typed is not None:
                        if len(receiver.queue) >= MAX_QUEUE:
                            raise failure("remote_overflow")
                        receiver.sequence += 1
                        receiver.queue.append({"sequence": receiver.sequence, **typed})
                        if name in ("setPlaylist", "updatePlaylist"):
                            self.retire_playlist(receiver)
                            receiver.full_playlist = queue_state or None
                            if not queue_state:
                                ids = typed['video_ids'] or ([typed['video_id']] if typed['video_id'] else [])
                                anchor = typed['index'] if typed['index'] < len(ids) and ids[typed['index']] == typed['video_id'] else ids.index(typed['video_id']) if typed['video_id'] in ids else -1
                                if anchor >= 0 and typed['index'] >= anchor:
                                    receiver.playlist_anchor = dict(typed, sequence=receiver.sequence,
                                        video_ids=ids, window_start=typed['index']-anchor)
                        elif name == "stopVideo":
                            self.retire_playlist(receiver)
                            receiver.full_playlist = None
                receiver.aid = index

    async def bind(self, receiver):
        body = await self.response("POST", "/bc/bind", params=self.params(receiver), data={"count": "0"})
        self.accept(receiver, Frames().feed(body, final=True))
        if not receiver.sid or not receiver.gsid:
            raise failure()

    def fail(self, receiver, code):
        self.retire_playlist(receiver)
        receiver.error = code
        receiver.full_playlist = None
        receiver.queue.clear()  # Never apply stale commands after a failed channel.
        receiver.code = ""
        if receiver.task is not None and receiver.task is not asyncio.current_task():
            receiver.task.cancel()

    async def listen(self, receiver):
        try:
            while True:
                # Native must renew the lease by polling, even while the phone is idle.
                if time.monotonic() - receiver.touched > LEASE_SECONDS:
                    raise failure("remote_expired")
                parser = Frames()
                async with self.http.stream("GET", BASE + "/bc/bind", params=self.params(receiver, rpc=True)) as response:
                    if response.status_code != 200:
                        raise failure("remote_upstream")
                    async for chunk in response.aiter_bytes():
                        self.accept(receiver, parser.feed(chunk))
                        if time.monotonic() - receiver.touched > LEASE_SECONDS:
                            raise failure("remote_expired")
                    self.accept(receiver, parser.feed(b"", final=True))
                # A normal long-poll completion opens a fresh channel. Avoid spinning
                # on empty upstream responses; failures require an explicit restart.
                await asyncio.sleep(0.25)
        except asyncio.CancelledError:
            raise
        except ServiceError as exc:
            self.fail(receiver, exc.code)
        except (httpx.HTTPError, ValueError, UnicodeError):
            self.fail(receiver, "remote_upstream")

    async def stop(self, receiver):
        playlist_task = receiver.playlist_task
        self.retire_playlist(receiver)
        if playlist_task is not None:
            await asyncio.gather(playlist_task, return_exceptions=True)
        receiver.error = "remote_stopped"
        if receiver.task is not None:
            receiver.task.cancel()
            await asyncio.gather(receiver.task, return_exceptions=True)
        async with receiver.lock:
            self.receivers.pop(receiver.session, None)
            receiver.queue.clear()
            try:
                if receiver.token and receiver.sid:
                    params = self.params(receiver)
                    params["TYPE"] = "terminate"
                    # Closing is best effort, bounded, and never retries an
                    # ambiguous request. Local resources close even offline.
                    try:
                        await asyncio.wait_for(self.response("GET", "/bc/bind", params=params), timeout=2)
                    except (ServiceError, httpx.HTTPError, TimeoutError):
                        pass
            finally:
                receiver.token = receiver.sid = receiver.gsid = receiver.code = ""
                receiver.full_playlist = None

    async def start(self, payload):
        async with self.lock:
            if self.closed:
                raise failure("remote_closed", 503)
            for receiver in list(self.receivers.values()):
                if time.monotonic() - receiver.touched > LEASE_SECONDS or receiver.error:
                    await self.stop(receiver)
                elif payload.device_id and receiver.device_id == payload.device_id:
                    if receiver.name != payload.name or receiver.profile != payload.profile or receiver.voice_enabled != payload.voice_enabled or (payload.screen_id and receiver.screen_id != payload.screen_id):
                        raise failure("remote_identity_conflict", 409)
                    receiver.touched = time.monotonic()
                    return self.status(receiver)
            if len(self.receivers) >= MAX_RECEIVERS:
                raise failure("remote_capacity", 429)
            receiver = Receiver(secrets.token_hex(16), payload.device_id or str(uuid.uuid4()), payload.name, payload.screen_id, voice_enabled=payload.voice_enabled, profile=payload.profile)
            try:
                if not receiver.screen_id:
                    raw = await self.response("GET", "/pairing/generate_screen_id")
                    receiver.screen_id = bounded_text(raw.decode().strip(), 128, r"[A-Za-z0-9_-]+")
                body = await self.response("POST", "/pairing/get_lounge_token_batch", data={"screen_ids": receiver.screen_id})
                info = strict_json(body)
                screens = info.get("screens") if isinstance(info, dict) else None
                if not isinstance(screens, list) or len(screens) > 16:
                    raise failure()
                matching = [entry for entry in screens if isinstance(entry, dict) and entry.get("screenId") == receiver.screen_id]
                if len(matching) != 1:
                    raise failure()
                receiver.token = bounded_text(matching[0].get("loungeToken"), 4096, r"[A-Za-z0-9_.=-]+")
                await self.bind(receiver)
            except ServiceError:
                await self.stop(receiver)
                raise
            except asyncio.CancelledError:
                await self.stop(receiver)
                raise
            except (httpx.HTTPError, ValueError, UnicodeError, RecursionError):
                await self.stop(receiver)
                raise failure("remote_upstream") from None
            receiver.touched = time.monotonic()
            self.receivers[receiver.session] = receiver
            receiver.task = asyncio.create_task(self.listen(receiver))
            return self.status(receiver)

    async def execute(self, payload):
        if payload.action == "start":
            return await self.start(payload)
        receiver = self.receivers.get(payload.session)
        if receiver is None:
            raise failure("remote_session", 404)
        if payload.action == "stop":
            await self.stop(receiver)
            return {"api_version": 1, "state": "stopped"}
        async with receiver.lock:
            if self.receivers.get(payload.session) is not receiver:
                raise failure("remote_session", 404)
            if receiver.error or time.monotonic() - receiver.touched > LEASE_SECONDS:
                raise failure(receiver.error or "remote_expired", 409)
            receiver.touched = time.monotonic()
            if payload.action == "poll":
                if not receiver.ack <= payload.ack <= receiver.delivered:
                    raise failure("remote_ack", 409)
                receiver.ack = payload.ack
                while receiver.queue and receiver.queue[0]["sequence"] <= payload.ack:
                    receiver.queue.popleft()
                commands = list(receiver.queue)[:32]
                if commands:
                    receiver.delivered = max(receiver.delivered, commands[-1]["sequence"])
                return {**self.status(receiver), "commands": commands, "ack": receiver.ack}
            try:
                if payload.action == "pair":
                    body = await self.response("POST", "/pairing/get_pairing_code", params={"ctx": "pair"},
                                               data={"lounge_token": receiver.token, "screen_id": receiver.screen_id,
                                                     "screen_name": receiver.name, "access_type": "permanent",
                                                     "app": "lb-v4", "device_id": receiver.device_id, "qr": "1"})
                    info = strict_json(body)
                    if not isinstance(info, dict):
                        raise failure()
                    receiver.code = bounded_text(info.get("code"), 12, r"(?:[0-9]{9}|[0-9]{12})")
                    if receiver.error:
                        raise failure(receiver.error)
                    return {**self.status(receiver), "pairing_code": receiver.code}
                if payload.player is None or payload.update_id <= 0:
                    raise failure("remote_state", 400)
                player = payload.player.model_dump()
                if payload.update_id == receiver.update_id and player == receiver.last_player:
                    return self.status(receiver)
                if payload.update_id <= receiver.update_id:
                    raise failure("remote_state_order", 409)
                packets = self.state_packets(player)
                form = {"count": str(len(packets)), "ofs": str(receiver.ofs)}
                for index, (name, args) in enumerate(packets):
                    form[f"req{index}__sc"] = name
                    form.update({f"req{index}_{key}": str(value) for key, value in args.items()})
                await self.response("POST", "/bc/bind", params=self.params(receiver), data=form)
                # If the back channel failed while this request was in flight, do
                # not publish success to the client or revive the failed receiver.
                if receiver.error:
                    raise failure(receiver.error)
                receiver.ofs += len(packets)
                receiver.update_id, receiver.last_player = payload.update_id, player
                self.refresh_playlist_window(receiver, player)
                self.schedule_playlist(receiver, player)
                return self.status(receiver)
            except ServiceError as exc:
                if exc.status >= 500:
                    self.fail(receiver, exc.code)
                raise
            except asyncio.CancelledError:
                self.fail(receiver, "remote_unconfirmed")
                raise
            except (httpx.HTTPError, ValueError, UnicodeError, RecursionError):
                self.fail(receiver, "remote_upstream")
                raise failure("remote_upstream") from None

    @staticmethod
    def retire_playlist(receiver):
        receiver.playlist_anchor = None
        receiver.playlist_retry = 0
        if receiver.playlist_task is not None:
            receiver.playlist_task.cancel()
            receiver.playlist_task = None

    def schedule_playlist(self, receiver, player):
        anchor = receiver.playlist_anchor
        if (self.playlist_loader is None or not anchor or receiver.full_playlist or receiver.playlist_task or
            time.monotonic() < receiver.playlist_retry or receiver.ack < anchor['sequence'] or
            not re.fullmatch(r'(WL|LL|[A-Za-z0-9_-]{10,100})', anchor['playlist']) or
            any(c['action'] in ('setPlaylist', 'updatePlaylist') for c in receiver.queue)):
            return
        offset = player['index'] - anchor['window_start']
        if (player['playlist'] != anchor['playlist'] or player['ctt'] != anchor['ctt'] or
            not 0 <= offset < len(anchor['video_ids']) or anchor['video_ids'][offset] != player['video_id']):
            return
        if anchor.get('expanded'):
            margin = min(16, max(1, len(anchor['video_ids'])//4))
            near_left = anchor['window_start'] > 0 and offset < margin and not player['playlist'].startswith('RD')
            if anchor.get('checked_index') == player['index'] or (not near_left and offset < len(anchor['video_ids'])-margin):
                return
        receiver.playlist_task = asyncio.create_task(self.load_playlist(receiver, anchor, player))
        self.playlist_tasks.add(receiver.playlist_task)
        receiver.playlist_task.add_done_callback(self.playlist_tasks.discard)

    async def load_playlist(self, receiver, anchor, player):
        task = asyncio.current_task()
        try:
            result = await self.playlist_loader(dict(action='remote_playlist', profile=receiver.profile,
                id=player['video_id'], playlist=player['playlist'], index=player['index']))
            ids, start = result.get('video_ids'), result.get('window_start')
            if (result.get('api_version') != 1 or result.get('playlist') != player['playlist'] or
                result.get('index') != player['index'] or result.get('video_id') != player['video_id'] or
                not isinstance(ids, list) or not 1 <= len(ids) <= 128 or
                any(not isinstance(v, str) or not VIDEO_ID.fullmatch(v) for v in ids) or
                type(start) is not int or not 0 <= start <= 100001-len(ids) or
                not 0 <= player['index']-start < len(ids) or ids[player['index']-start] != player['video_id']):
                raise failure('remote_playlist_unavailable')
            async with receiver.lock:
                current = receiver.last_player
                if (self.receivers.get(receiver.session) is not receiver or receiver.error or
                    receiver.playlist_anchor is not anchor or not current or
                    any(current[k] != player[k] for k in ('video_id','playlist','index','ctt')) or
                    any(c['action'] in ('setPlaylist','updatePlaylist') for c in receiver.queue)):
                    return
                if ids == anchor['video_ids'] and start == anchor['window_start']:
                    anchor['expanded'] = True
                    anchor['checked_index'] = player['index']
                    return
                if result.get('radio_rebased') is True and player['playlist'].startswith('RD') and anchor['window_start'] <= start <= player['index']:
                    prefix = anchor['video_ids'][:start-anchor['window_start']][-32:]
                    start -= len(prefix)
                    combined = prefix + ids
                    crop = max(0, min(player['index']-start-32, len(combined)-128))
                    start += crop
                    ids = combined[crop:crop+128]
                if len(receiver.queue) >= MAX_QUEUE or receiver.sequence >= 2_147_483_647:
                    return
                receiver.sequence += 1
                update = dict(sequence=receiver.sequence, action='updatePlaylist', video_id=current['video_id'],
                    playlist=current['playlist'], ctt=current['ctt'], index=current['index'],
                    position_sec=current['position_sec'], video_ids=ids, window_start=start)
                receiver.queue.append(update)
                receiver.playlist_anchor = dict(update, expanded=True, checked_index=player['index'])
        except (ServiceError, ValueError, TypeError, AttributeError):
            pass  # Preserve the supplied queue when optional expansion is unavailable.
        finally:
            if receiver.playlist_task is task:
                receiver.playlist_task = None
                receiver.playlist_retry = time.monotonic() + 10

    @staticmethod
    def refresh_playlist_window(receiver, player):
        saved = receiver.full_playlist
        if not saved or len(receiver.queue) >= MAX_QUEUE or receiver.sequence >= 2_147_483_647:
            return
        if any(c["action"] in ("setPlaylist", "updatePlaylist") for c in receiver.queue):
            return
        index = player["index"]
        local = index - saved["base"]
        ids = saved["ids"]
        if (not player["video_id"] or not (player["has_previous"] or player["has_next"]) or
            player["playlist"] != saved["playlist"] or player["ctt"] != saved["ctt"] or
            not 0 <= local < len(ids) or ids[local] != player["video_id"]):
            return
        if (saved["window_start"] == saved["base"] or index >= saved["window_start"] + 16) and (
            saved["window_end"] == saved["base"] + len(ids) - 1 or index <= saved["window_end"] - 16):
            return
        start = max(0, min(local - 32, len(ids) - 128))
        if saved["window_start"] == saved["base"] + start:
            return
        receiver.sequence += 1
        receiver.queue.append({"sequence": receiver.sequence, "action": "updatePlaylist",
                               "video_id": player["video_id"], "video_ids": ids[start:start + 128],
                               "playlist": saved["playlist"], "ctt": saved["ctt"], "index": index,
                               "window_start": saved["base"] + start, "position_sec": player["position_sec"]})
        saved["window_start"], saved["window_end"] = saved["base"] + start, saved["base"] + start + 127

    @staticmethod
    def state_packets(player):
        clock = {"currentTime": player["position_sec"], "duration": player["duration_sec"],
                 "loadedTime": "0", "seekableStartTime": "0", "seekableEndTime": player["duration_sec"]}
        return [("nowPlaying", {**clock, "videoId": player["video_id"], "state": player["state"],
                                "listId": player["playlist"], "currentIndex": player["index"], "ctt": player["ctt"]}),
                ("onStateChange", {**clock, "state": player["state"]}),
                ("onVolumeChanged", {"volume": player["volume"], "muted": str(player["muted"]).lower()}),
                ("onHasPreviousNextChanged", {"hasPrevious": str(player["has_previous"]).lower(),
                                             "hasNext": str(player["has_next"]).lower()}),
                ("onSubtitlesTrackChanged", {"videoId": player["video_id"], "vss_id": player["vss_id"],
                                            "languageCode": player["language"], "sourceLanguageCode": player["language"]})]

    async def close(self):
        async with self.lock:
            self.closed = True
            for receiver in list(self.receivers.values()):
                await self.stop(receiver)
            tasks = list(self.playlist_tasks)
            for task in tasks:
                task.cancel()
            await asyncio.gather(*tasks, return_exceptions=True)
            if self.owns_http:
                await self.http.aclose()
