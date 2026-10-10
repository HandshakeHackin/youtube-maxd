"""A decoder's successful exit alone cannot prove the requested playback duration."""
import asyncio

import httpx
import pytest

from test_youtube_probe_accounts import module


@pytest.mark.parametrize('progress', [
    b'', b'out_time_us=2000000\nprogress=continue\n',
    b'out_time_us=N/A\nprogress=end\n',
    b'out_time_us=2000000\nout_time_us=2000000\nprogress=end\n',
    b'out_time_us=1000000\nprogress=end\n',
])
def test_missing_malformed_or_short_final_decode_is_not_success(progress):
    with pytest.raises(RuntimeError): module().decoded_seconds(progress, 2)


def test_final_progress_timestamp_required_after_previous_successful_frames():
    probe = module()
    assert probe.decoded_seconds(b'out_time_us=1000000\nprogress=continue\n'
                                 b'out_time_us=60200000\nprogress=end\n', 60) == 60.2
    with pytest.raises(RuntimeError):
        probe.decoded_seconds(b'out_time_us=60000000\nprogress=continue\n'
                              b'out_time_us=1000000\nprogress=end\n', 60)


def test_delayed_root_expiry_forwards_earlier_refreshes_and_injects_exactly_once():
    async def run():
        transport = module().ExpireOnceTransport()
        await transport.inner.aclose()
        forwarded = []
        def handler(request):
            forwarded.append(str(request.url))
            return httpx.Response(200, content=b'real-playlist')
        transport.inner = httpx.MockTransport(handler)
        transport.fail_url = 'https://r1.googlevideo.com/root.m3u8'
        transport.inject_on_request = 3
        async with httpx.AsyncClient(transport=transport) as client:
            assert (await client.get('https://r1.googlevideo.com/audio.m3u8')).status_code == 200
            statuses = [(await client.get('https://r1.googlevideo.com/root.m3u8')).status_code for _ in range(4)]
        assert statuses == [200, 200, 403, 200]
        assert transport.injections == 1 and transport.matching_requests == 3
        assert len(forwarded) == 4
    asyncio.run(run())
