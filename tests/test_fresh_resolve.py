"""Both native jobs and direct requests must honor the selected fresh resolver."""
import time
from fastapi.testclient import TestClient
from prosperotube.app import create_app
from test_companion import AUTH, TOKEN, ID, FakeExtractor


def test_fresh_resolution_reaches_both_shipping_routes():
    extractor = FakeExtractor()
    with TestClient(create_app(extractor=extractor, token=TOKEN)) as client:
        for fresh in (False, True):
            result = client.post('/v1/resolve', headers=AUTH, json={'id': ID, 'fresh': fresh})
            assert result.status_code == 200, result.text
            assert extractor.calls[-1][1] == {'fresh': fresh}
            job = client.post('/v1/jobs', headers=AUTH, json={
                'action': 'resolve', 'playback': {'id': ID, 'fresh': fresh}}).json()['job']
            for _ in range(100):
                result = client.get('/v1/jobs/' + job, headers=AUTH).json()
                if result['state'] != 'pending':
                    break
                time.sleep(.01)
            assert result['state'] == 'done', result
            assert extractor.calls[-1][1] == {'fresh': fresh}
        assert client.post('/v1/resolve', headers=AUTH, json={'id': ID, 'fresh': 'yes'}).status_code == 422
