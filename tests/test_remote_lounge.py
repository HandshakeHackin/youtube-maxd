"""Actual Lounge wire shapes with synthetic HTTP, no account or phone required."""
import asyncio
import json
from urllib.parse import parse_qs

import httpx
import pytest

from prosperotube.app import create_app
from prosperotube.models import ServiceError
from prosperotube.remote_lounge import Frames, Lounge, MAX_QUEUE, PlayerState, Receiver, RemoteRequest, command

DEVICE = "01234567-89ab-cdef-0123-456789abcdef"
VIDEO = "abcdefghijk"
AUTH = {"Authorization": "Bearer remote-test"}


def wire(messages):
    text = json.dumps(messages, ensure_ascii=False, separators=(",", ":"))
    return (str(len(text.encode("utf-16-le")) // 2) + "\n" + text).encode()


def test_frames_fragmented_unicode_multiple_messages_and_eof():
    messages = [[0, ["noop"]], [1, ["remoteConnected", {"id": "phone", "name": "日本語 😀"}]]]
    data = wire(messages) + wire([[2, ["play"]]])
    for step in (1, 2, 7, len(data)):
        parser, actual = Frames(), []
        for index in range(0, len(data), step):
            actual += parser.feed(data[index:index + step])
        actual += parser.feed(b"", final=True)
        assert actual == [messages, [[2, ["play"]]]]


@pytest.mark.parametrize("data", [b"-1\n", b"0000000\n", b"262145\n", b"2\n[", b"4\nnull", b"3\nNaN", b"1\n\xff", b"2\n{}"])
def test_malformed_frames_fail_without_upstream_details(data):
    with pytest.raises(ServiceError) as error:
        Frames().feed(data, final=True)
    assert error.value.code == "remote_protocol"
    assert data.decode(errors="replace") not in error.value.message


class WaitingStream(httpx.AsyncByteStream):
    def __init__(self, initial=b""):
        self.initial = initial
        self.entered = asyncio.Event()
        self.release = asyncio.Event()
        self.closed = False

    async def __aiter__(self):
        if self.initial:
            yield self.initial
        self.entered.set()
        await self.release.wait()

    async def aclose(self):
        self.closed = True


class WireServer:
    def __init__(self):
        self.calls = []
        self.streams = []
        self.initial = [[0, ["c", "private-SID", "", 8]], [1, ["S", "private-gsession"]],
                        [2, ["getNowPlaying"]]]
        self.incoming = b""
        self.fail_pair = False
        self.fail_update = False

    async def handle(self, request):
        assert request.url.host == "www.youtube.com" and request.url.scheme == "https"
        path = request.url.path.removeprefix("/api/lounge")
        form = {key: values[0] for key, values in parse_qs(request.content.decode(), keep_blank_values=True).items()}
        self.calls.append((request.method, path, dict(request.url.params), form))
        if path == "/pairing/generate_screen_id":
            assert request.method == "GET"
            return httpx.Response(200, text="screen-123")
        if path == "/pairing/get_lounge_token_batch":
            assert form == {"screen_ids": "screen-123"}
            return httpx.Response(200, json={"screens": [{"screenId": "screen-123", "loungeToken": "private-token"}]})
        if path == "/pairing/get_pairing_code":
            assert dict(request.url.params) == {"ctx": "pair"}
            assert form == {"lounge_token": "private-token", "screen_id": "screen-123", "screen_name": "ProsperoTube PS5",
                            "access_type": "permanent", "app": "lb-v4", "device_id": DEVICE, "qr": "1"}
            return httpx.Response(503, text="private-token") if self.fail_pair else httpx.Response(200, json={"code": "123456789012"})
        assert path == "/bc/bind"
        query = request.url.params
        assert query["id"] == DEVICE and query["loungeIdToken"] == "private-token"
        assert query["capabilities"] == "dsp,dpa"
        if query.get("TYPE") == "terminate":
            assert request.method == "GET" and query["SID"] == "private-SID"
            return httpx.Response(200, content=b"")
        if request.method == "GET":
            assert query["RID"] == "rpc" and query["SID"] == "private-SID" and query["gsessionid"] == "private-gsession"
            assert query["CI"] == "0" and query["TYPE"] == "xmlhttp"
            stream = WaitingStream(self.incoming)
            self.streams.append(stream)
            return httpx.Response(200, stream=stream)
        if form == {"count": "0"}:
            assert "SID" not in query and query["RID"].isdigit()
            return httpx.Response(200, content=wire(self.initial))
        assert query["SID"] == "private-SID" and query["AID"].isdigit()
        if self.fail_update:
            raise httpx.ReadError("private-token private-SID", request=request)
        return httpx.Response(200, content=b"[1,2,3]")


def service(server):
    return Lounge(httpx.AsyncClient(transport=httpx.MockTransport(server.handle)))


async def cleanup(lounge):
    await lounge.close()
    await lounge.http.aclose()


async def start(lounge):
    return await lounge.execute(RemoteRequest(action="start", device_id=DEVICE))


def test_registration_pairing_acknowledgment_retry_state_feedback_and_stop():
    async def run():
        server = WireServer()
        server.incoming = wire([[3, ["setPlaylist", {"videoId": VIDEO, "videoIds": VIDEO + ",zyxwvutsrqp",
                                                   "currentTime": "13.25", "currentIndex": "0", "listId": "RDabcdefghijk", "ctt": "opaque"}]],
                                [4, ["pause"]], [5, ["seekTo", {"newTime": "27.5"}]], [6, ["noop"]]])
        lounge = service(server)
        try:
            status = await start(lounge)
            session = status["session"]
            await asyncio.sleep(0)
            await server.streams[0].entered.wait()
            assert await start(lounge) == status  # Same native device never opens a second receiver.
            assert len([call for call in server.calls if call[1] == "/pairing/get_lounge_token_batch"]) == 1
            paired = await lounge.execute(RemoteRequest(action="pair", session=session))
            assert paired["pairing_code"] == "123456789012"
            request = RemoteRequest(action="poll", session=session)
            poll = await lounge.execute(request)
            assert [item["action"] for item in poll["commands"]] == ["getNowPlaying", "setPlaylist", "pause", "seekTo"]
            assert poll["commands"][1]["position_sec"] == 13.25
            assert poll["commands"][1]["video_ids"] == [VIDEO, "zyxwvutsrqp"]
            assert await lounge.execute(request) == poll  # Poll loss cannot consume commands.
            with pytest.raises(ServiceError) as error:
                await lounge.execute(RemoteRequest(action="poll", session=session, ack=99))
            assert error.value.code == "remote_ack"
            empty = await lounge.execute(RemoteRequest(action="poll", session=session, ack=4))
            assert empty["commands"] == [] and empty["ack"] == 4
            state = PlayerState(video_id=VIDEO, position_sec=27.5, duration_sec=300, state=2, playlist="RDabcdefghijk", ctt="opaque",
                                volume=40, muted=True, has_next=True, language="en", vss_id="a.en")
            update = RemoteRequest(action="update", session=session, update_id=1, player=state)
            await lounge.execute(update)
            sends = [call for call in server.calls if call[3].get("count") == "5"]
            assert len(sends) == 1
            form = sends[0][3]
            assert form["ofs"] == "0" and form["req0__sc"] == "nowPlaying"
            assert form["req0_currentTime"] == "27.5" and form["req0_ctt"] == "opaque"
            assert form["req2_muted"] == "true" and form["req3_hasNext"] == "true" and form["req4_vss_id"] == "a.en"
            before = len(server.calls)
            await lounge.execute(update)
            assert len(server.calls) == before
            with pytest.raises(ServiceError) as error:
                await lounge.execute(update.model_copy(update={"player": PlayerState(video_id=VIDEO, position_sec=28)}))
            assert error.value.code == "remote_state_order"
            await lounge.execute(update.model_copy(update={"update_id": 2}))
            assert server.calls[-1][3]["ofs"] == "5"
        finally:
            await cleanup(lounge)
        assert not lounge.receivers and server.streams[0].closed
    asyncio.run(run())


@pytest.mark.parametrize("name,args,expected", [
    ("setPlaylist", {"videoIds": VIDEO + ",zyxwvutsrqp", "currentIndex": "1"}, {"video_id": "zyxwvutsrqp", "index": 1}),
    ("updatePlaylist", {"videoIds": VIDEO}, {"video_ids": [VIDEO]}),
    ("setVolume", {"volume": "75", "delta": "-5"}, {"volume": 75, "delta": -5}),
    ("dpadCommand", {"key": "ENTER"}, {"key": "ENTER"}),
    ("voiceCommand", {"status": "INITIATED"}, {"status": "INITIATED"}),
    ("setSubtitlesTrack", {"videoId": VIDEO, "languageCode": "pt-BR"}, {"language": "pt-BR"}),
    ("loungeStatus", {"devices": json.dumps([{"type": "LOUNGE_SCREEN", "id": "tv"}, {"type": "REMOTE_CONTROL", "id": "phone", "name": "Phone", "token": "secret"}])},
     {"devices": [{"device_id": "phone", "name": "Phone"}]}),
])
def test_phone_commands_have_typed_allowlisted_parameters(name, args, expected):
    result = command(name, args)
    assert result["action"] == name
    for key, value in expected.items():
        assert result[key] == value
    assert "secret" not in json.dumps(result)


@pytest.mark.parametrize("name,args", [("setPlaylist", {}), ("setPlaylist", {"videoId": "file:///bad"}),
    ("setPlaylist", {"videoId": VIDEO, "currentTime": "NaN"}), ("setPlaylist", {"videoId": VIDEO, "currentIndex": "0.5"}),
    ("setPlaylist", {"videoId": VIDEO, "videoIds": ",".join([VIDEO] * 129)}),
    ("seekTo", {"newTime": "-1"}), ("seekTo", {"newTime": True}), ("setVolume", {}),
    ("setVolume", {"volume": "101"}), ("dpadCommand", {"key": "exec"}), ("voiceCommand", {"status": "EXEC"}),
    ("loungeStatus", {"devices": "{}"}), ("setSubtitlesTrack", {"languageCode": "\n"})])
def test_invalid_wire_commands_fail_closed(name, args):
    with pytest.raises(ServiceError):
        command(name, args)


def test_duplicate_wire_ids_ack_limits_queue_bound_and_no_silent_gap():
    lounge = Lounge.__new__(Lounge)  # Pure command handling needs no client/event loop.
    receiver = Receiver("session", DEVICE, "TV")
    lounge.accept(receiver, [[[0, ["play"]], [0, ["pause"]], [1, ["futureExtension", {}]]]])
    assert receiver.aid == 1 and list(receiver.queue) == [{"sequence": 1, "action": "play"}]
    with pytest.raises(ServiceError) as error:
        lounge.accept(receiver, [[[3, ["pause"]]]])
    assert error.value.code == "remote_gap" and receiver.aid == 1
    lounge.accept(receiver, [[[index, ["pause"]] for index in range(2, MAX_QUEUE + 1)]])
    assert len(receiver.queue) == MAX_QUEUE
    with pytest.raises(ServiceError) as error:
        lounge.accept(receiver, [[[MAX_QUEUE + 1, ["pause"]]]])
    assert error.value.code == "remote_overflow" and receiver.aid == MAX_QUEUE


@pytest.mark.parametrize("failure", ["pair", "update", "incoming"])
def test_upstream_failure_does_not_leak_secrets_or_replay_uncertain_commands(failure):
    async def run():
        server, lounge = WireServer(), None
        if failure == "incoming":
            server.incoming = wire([[3, ["seekTo", {"newTime": "nan"}]]])
        lounge = service(server)
        try:
            status = await start(lounge)
            session = status["session"]
            await asyncio.sleep(0)
            if failure != "incoming":
                server.fail_pair = failure == "pair"
                server.fail_update = failure == "update"
                payload = RemoteRequest(action=failure, session=session, update_id=1, player=PlayerState(video_id=VIDEO))
                with pytest.raises(ServiceError) as error:
                    await lounge.execute(payload)
                assert "private" not in str(error.value.message)
            else:
                await lounge.receivers[session].task
            receiver = lounge.receivers[session]
            assert receiver.error and not receiver.queue
            before = len(server.calls)
            with pytest.raises(ServiceError):
                await lounge.execute(RemoteRequest(action="update", session=session, update_id=1, player=PlayerState()))
            assert len(server.calls) == before
            assert "private" not in json.dumps(lounge.status(receiver)) and "private" not in repr(receiver)
            await lounge.execute(RemoteRequest(action="stop", session=session))
            assert receiver.token == receiver.sid == receiver.gsid == ""
        finally:
            await cleanup(lounge)
    asyncio.run(run())


def test_api_opt_in_auth_schema_lifespan_and_no_arbitrary_origins():
    async def run():
        server = WireServer()
        lounge = service(server)
        app = create_app(token="remote-test", remote=lounge)
        async with app.router.lifespan_context(app):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(app), base_url="http://test") as client:
                assert not server.calls
                assert (await client.post("/v1/remote", json={"action": "start"})).status_code == 401
                assert not server.calls
                for extra in ({"origin": "http://localhost"}, {"device_id": "bad"}, {"ack": True}, {"name": "bad\n"}, {"player": {"position_sec": -1}}, {"player": {"position_sec": True}}):
                    assert (await client.post("/v1/remote", headers=AUTH, json={"action": "start", **extra})).status_code == 422
                assert not server.calls
                status = (await client.post("/v1/remote", headers=AUTH, json={"action": "start", "device_id": DEVICE})).json()
                session = status["session"]
                assert "private" not in json.dumps(status)
                assert (await client.post("/v1/remote", headers=AUTH, json={"action": "poll", "session": "f" * 32})).status_code == 404
                assert (await client.post("/v1/remote", headers=AUTH, json={"action": "poll", "session": session})).status_code == 200
                await asyncio.sleep(0)
        assert lounge.closed and not lounge.receivers and server.streams[0].closed
        await lounge.http.aclose()
        lazy = create_app(token="remote-test")
        async with lazy.router.lifespan_context(lazy):
            async with httpx.AsyncClient(transport=httpx.ASGITransport(lazy), base_url="http://test") as client:
                assert (await client.post("/v1/remote", headers=AUTH, json={"action": "poll"})).status_code == 404
                assert lazy.state.remote is None
    asyncio.run(run())


def test_identity_reuse_conflict_expiry_and_closed_service():
    async def run():
        server, lounge = WireServer(), None
        lounge = service(server)
        try:
            status = await lounge.execute(RemoteRequest(action="start", device_id=DEVICE, screen_id="screen-123"))
            assert not any(call[1] == "/pairing/generate_screen_id" for call in server.calls)
            await asyncio.sleep(0)
            with pytest.raises(ServiceError) as error:
                await lounge.execute(RemoteRequest(action="start", device_id=DEVICE, name="Other name"))
            assert error.value.code == "remote_identity_conflict"
            receiver = lounge.receivers[status["session"]]
            receiver.touched -= 121
            with pytest.raises(ServiceError) as error:
                await lounge.execute(RemoteRequest(action="poll", session=status["session"]))
            assert error.value.code == "remote_expired"
            fresh = await start(lounge)
            assert fresh["session"] != status["session"] and status["session"] not in lounge.receivers
            assert server.streams[0].closed and not receiver.token
            with pytest.raises(ServiceError):
                await lounge.execute(RemoteRequest(action="poll", session=status["session"]))
        finally:
            await cleanup(lounge)
        with pytest.raises(ServiceError) as error:
            await start(lounge)
        assert error.value.code == "remote_closed"
    asyncio.run(run())


def test_capacity_and_failed_registration_do_not_orphan_receivers():
    async def run():
        server = WireServer()
        server.initial = [[0, ["c", "private-SID"]]]  # Missing global session.
        lounge = service(server)
        try:
            with pytest.raises(ServiceError):
                await start(lounge)
            assert not lounge.receivers and not server.streams
            assert server.calls[-1][2]["TYPE"] == "terminate"
            for index in range(4):
                lounge.receivers[str(index)] = Receiver(str(index), str(index), "TV")
            before = len(server.calls)
            with pytest.raises(ServiceError) as error:
                await start(lounge)
            assert error.value.code == "remote_capacity" and len(server.calls) == before
        finally:
            await cleanup(lounge)
    asyncio.run(run())


@pytest.mark.parametrize("case", ["cancel", "channel_failure"])
def test_inflight_state_report_cannot_revive_failed_receiver(case):
    async def run():
        server = WireServer()
        entered, release = asyncio.Event(), asyncio.Event()
        async def handler(request):
            if request.method == "POST" and b"req0__sc" in request.content:
                entered.set()
                await release.wait()
            return await server.handle(request)
        lounge = Lounge(httpx.AsyncClient(transport=httpx.MockTransport(handler)))
        try:
            status = await start(lounge)
            receiver = lounge.receivers[status["session"]]
            await asyncio.sleep(0)
            task = asyncio.create_task(lounge.execute(RemoteRequest(action="update", session=status["session"],
                                                                    update_id=1, player=PlayerState(video_id=VIDEO))))
            await entered.wait()
            if case == "cancel":
                task.cancel()
                with pytest.raises(asyncio.CancelledError):
                    await task
                assert receiver.error == "remote_unconfirmed"
            else:
                lounge.fail(receiver, "remote_protocol")
                release.set()
                with pytest.raises(ServiceError):
                    await task
                assert receiver.error == "remote_protocol"
            assert not receiver.queue and receiver.update_id == -1 and receiver.ofs == 0
            await asyncio.sleep(0)
            assert receiver.task.done() and server.streams[0].closed
            with pytest.raises(ServiceError):
                await lounge.execute(RemoteRequest(action="update", session=status["session"], update_id=1, player=PlayerState()))
        finally:
            await cleanup(lounge)
    asyncio.run(run())


def test_queue_overflow_stream_failure_closes_without_acknowledging_lost_command():
    async def run():
        server = WireServer()
        server.incoming = wire([[index, ["play"]] for index in range(3, 3 + MAX_QUEUE)])
        lounge = service(server)
        try:
            status = await start(lounge)
            receiver = lounge.receivers[status["session"]]
            await receiver.task
            assert receiver.error == "remote_overflow" and not receiver.queue and receiver.ack == 0
            assert receiver.aid == MAX_QUEUE + 1  # The rejected command's wire ID is not accepted.
            with pytest.raises(ServiceError):
                await lounge.execute(RemoteRequest(action="poll", session=status["session"]))
        finally:
            await cleanup(lounge)
    asyncio.run(run())
