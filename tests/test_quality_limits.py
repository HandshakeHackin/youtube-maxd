"""Resolution and frame-rate limits are applied before transport selection."""
import pytest
from pydantic import ValidationError
from prosperotube.models import ResolveRequest, ServiceError
from prosperotube.sessions import Sessions
from test_companion import info, ID, BASE

@pytest.mark.parametrize('height', [144,240,360,480,720,1080,1440,2160])
@pytest.mark.parametrize('codec', ['avc1.640028','vp09.00.40.08','av01.0.08M.08'])
def test_resolution_and_frame_rate_limits(height, codec):
    source=info()
    video=source['formats'][0]
    audio=source['formats'][1]
    source['formats']=[audio]+[dict(video,format_id=f'{h}-{fps}',height=h,fps=fps,vcodec=codec)
                              for h in [144,240,360,480,720,1080,1440,2160] for fps in [24,29.97,30,59.94,60]]
    for limit in [0,30,60]:
        choices=Sessions().create(source,ResolveRequest(id=ID,max_height=height,max_fps=limit),BASE).choices
        assert choices and max(c.height for c in choices)==height
        assert all(c.height<=height and (not limit or c.fps<=limit) for c in choices)
        assert choices[0].fps==(30 if limit==30 else 60)

@pytest.mark.parametrize('fps', [None,0,-1,'nan',float('inf'),120])
def test_explicit_fps_limit_rejects_unknown_or_excessive_rates(fps):
    source=info();source['formats']=source['formats'][:2];source['formats'][0]['fps']=fps
    with pytest.raises(ServiceError,match='No compatible'):
        Sessions().create(source,ResolveRequest(id=ID,max_fps=30),BASE)
    # Automatic preserves formats that do not advertise a usable rate.
    assert Sessions().create(source,ResolveRequest(id=ID),BASE).choices

@pytest.mark.parametrize('fps', [-1,61,True,30.5,'30'])
def test_invalid_fps_request(fps):
    with pytest.raises(ValidationError): ResolveRequest(id=ID,max_fps=fps)

@pytest.mark.parametrize('live', [False,True])
def test_progressive_and_live_hls_obey_rate_limit(live):
    source=info();video=source['formats'][2]
    source.update(is_live=live,formats=[dict(video,format_id='fast',fps=60),dict(video,format_id='slow',fps=29.97)])
    if live:
        for fmt in source['formats']:fmt.update(protocol='m3u8_native',url='https://r1.googlevideo.com/'+fmt['format_id']+'.m3u8')
    choice=Sessions().create(source,ResolveRequest(id=ID,max_fps=30),BASE).choices[0]
    assert choice.fps==29.97 and '29.97 fps' in choice.label
    assert choice.container==('hls' if live else 'mp4')
