"""HEVC selection, transport identity, exclusions and automatic codec ordering."""
import xml.etree.ElementTree as ET
import pytest
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.sessions import Sessions, codec_name
from prosperotube.live_hls import apply_master
from test_companion import info, ID, BASE


@pytest.mark.parametrize('raw', ['hvc1.1.6.L93.B0', 'hev1.1.6.L93.B0', 'hevc', 'h265'])
@pytest.mark.parametrize('container', ['mp4'])
def test_hevc_dash_retains_codec_container_and_private_refresh_binding(raw, container):
    source = info()
    source['formats'][0].update(vcodec=raw, ext=container)
    sessions = Sessions()
    result = sessions.create(source, ResolveRequest(id=ID, codec='hevc'), BASE)
    assert len(result.choices) == 1 and result.choices[0].video_codec == 'hevc'
    assert result.choices[0].audio_codec == 'aac' and result.choices[0].container == 'dash'
    session = next(iter(sessions.sessions.values()))
    manifest = ET.fromstring(next(iter(session.manifests.values())))
    video = manifest.find('.//{*}AdaptationSet[@contentType="video"]')
    assert video.attrib['codecs'] == raw and video.attrib['mimeType'] == 'video/' + container
    assert next(track for key, track in session.tracks.items() if key.endswith('v')).binding is not None
    assert 'signature' not in result.model_dump_json()


@pytest.mark.parametrize('change', [{'has_drm': True}, {'height': 2160}, {'protocol': 'sabr'}])
def test_hevc_does_not_bypass_existing_media_restrictions(change):
    source = info()
    source['formats'][0].update(vcodec='hvc1.1.6.L93.B0', **change)
    with pytest.raises(ServiceError, match='No compatible'):
        Sessions().create(source, ResolveRequest(id=ID, codec='hevc', max_height=1080), BASE)


@pytest.mark.parametrize('range_name', ['HDR10', 'HLG'])
def test_hevc_offers_supported_hdr_range(range_name):
    source = info()
    source['formats'][0].update(vcodec='hvc1.1.6.L93.B0', dynamic_range=range_name)
    choice = Sessions().create(source, ResolveRequest(id=ID, codec='hevc'), BASE).choices[0]
    assert choice.video_range == range_name and range_name in choice.label


def test_auto_prefers_existing_codecs_at_equal_quality_and_explicit_hevc_is_honored():
    source = info()
    video = source['formats'][0]
    source['formats'] += [dict(video, format_id='vp9', vcodec='vp09.00.40.08'),
                          dict(video, format_id='hevc', vcodec='hvc1.1.6.L93.B0'),
                          dict(video, format_id='av1', vcodec='av01.0.08M.08')]
    choices = Sessions().create(source, ResolveRequest(id=ID), BASE).choices
    assert [c.video_codec for c in choices[:4]] == ['h264', 'vp9', 'hevc', 'av1']
    assert {c.video_codec for c in Sessions().create(source, ResolveRequest(id=ID, codec='hevc'), BASE).choices} == {'hevc'}
    assert not codec_name('hvc1-malformed')


def test_hevc_progressive_and_split_live_hls_mapping():
    source = info()
    source['formats'][2]['vcodec'] = 'hev1.1.6.L93.B0'
    choice = Sessions().create(source, ResolveRequest(id=ID, codec='hevc'), BASE).choices[0]
    assert choice.container == 'mp4' and choice.video_codec == 'hevc'
    master = 'https://r1.googlevideo.com/master.m3u8'
    video = dict(source['formats'][0], vcodec='hvc1.1.6.L93.B0', protocol='m3u8_native',
                 url='https://r1.googlevideo.com/v.m3u8', manifest_url=master)
    audio = dict(source['formats'][1], protocol='m3u8_native',
                 url='https://r1.googlevideo.com/a.m3u8', manifest_url=master)
    source.update(is_live=True, formats=[video, audio])
    apply_master(source, master, '#EXTM3U\n#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="a",URI="a.m3u8"\n'
                 '#EXT-X-STREAM-INF:BANDWIDTH=1000000,AUDIO="a",CODECS="hvc1.1.6.L93.B0,mp4a.40.2"\nv.m3u8\n')
    choice = Sessions().create(source, ResolveRequest(id=ID, codec='hevc'), BASE).choices[0]
    assert choice.container == 'hls' and choice.video_codec == 'hevc' and choice.audio_codec == 'aac'


@pytest.mark.parametrize('raw', ['hvc1-malformed', 'hev1-malformed', 'dvhe.05.06'])
def test_hevc_does_not_advertise_unknown_or_dolby_vision_codec(raw):
    assert not codec_name(raw)
