import hashlib
import time
import httpx
import pytest
from fastapi.testclient import TestClient
from prosperotube.app import create_app
from prosperotube.dearrow import branding
from test_companion import AUTH, TOKEN, ID, FakeExtractor, info


def entry(title="Community title"):
    return {"titles":[{"title":title,"votes":2,"original":False}],
            "thumbnails":[{"timestamp":10.5,"votes":1,"original":False}]}


def test_winner_trust_original_and_bounded_text():
    assert branding(entry()) == ("Community title",10.5)
    data=entry(); data["titles"][0]["votes"]=-1
    data["titles"].append({"title":"must not bypass winner","votes":5,"original":False})
    assert branding(data)[0] == ""
    data["titles"][0]["locked"]=True; assert branding(data)[0] == "Community title"
    data["titles"][0]["original"]=True; assert branding(data)[0] == "must not bypass winner"
    data["titles"].pop(); assert branding(data)[0] == ""
    data["thumbnails"][0]["timestamp"]=float("nan"); assert branding(data)[1] is None
    data=entry(">日本語 "*200); assert len(branding(data)[0].encode()) <=191 and ">" not in branding(data)[0]


@pytest.mark.parametrize("titles,thumbs",[(False,False),(True,False),(False,True),(True,True)])
def test_shipping_search_preferences_originals_cache_and_thumbnail_fallback(titles,thumbs):
    requests=[]
    def upstream(request):
        requests.append(request)
        assert "authorization" not in request.headers and "cookie" not in request.headers
        if request.url.host=="sponsor.ajay.app":
            assert request.url.path.endswith(hashlib.sha256(ID.encode()).hexdigest()[:4])
            return httpx.Response(200,json={ID:entry(),"other-video":entry("wrong")})
        if request.url.host=="dearrow-thumb.ajay.app": return httpx.Response(204)
        assert request.url.host=="i.ytimg.com"
        return httpx.Response(200,content=b"\xff\xd8\xfffixture",headers={"content-type":"image/jpeg"})
    extractor=FakeExtractor(info())
    app=create_app(extractor=extractor,token=TOKEN,http_client=httpx.AsyncClient(transport=httpx.MockTransport(upstream)))
    with TestClient(app) as client:
        params={"q":"fixture","dearrow_titles":titles,"dearrow_thumbnails":thumbs}
        result=client.get("/v1/search",headers=AUTH,params=params)
        assert result.status_code==200,result.text
        row=result.json()["items"][0]
        assert row["title"]==extractor.result["title"] and "dearrow_title" not in extractor.result
        assert row.get("dearrow_title","")==("Community title" if titles else "")
        assert bool(row.get("dearrow_art_url"))==thumbs
        count=len(requests)
        client.get("/v1/search",headers=AUTH,params=params)
        assert len(requests)==count
        if thumbs:
            image=client.get(row["dearrow_art_url"])
            assert image.status_code==200 and image.content.startswith(b"\xff\xd8\xff")
            assert requests[-2].url.params["videoID"]==ID and requests[-2].url.params["time"]=="10.5"
        elif not titles: assert not requests
        assert client.get("/art/dearrow/"+"a"*32).status_code==404


@pytest.mark.parametrize("failure",[404,503,"bad_json","redirect","untrusted","wrong_id"])
def test_optional_failures_keep_catalog(failure):
    def upstream(request):
        if failure=="bad_json": return httpx.Response(200,content=b"broken")
        if failure=="redirect": return httpx.Response(302,headers={"location":"http://127.0.0.1/private"})
        if failure=="untrusted":
            data=entry();data["titles"][0]["votes"]=-1;data["thumbnails"][0]["votes"]=-1
            return httpx.Response(200,json={ID:data})
        if failure=="wrong_id": return httpx.Response(200,json={"wrong-id":entry()})
        return httpx.Response(failure)
    with TestClient(create_app(extractor=FakeExtractor(),token=TOKEN,http_client=httpx.AsyncClient(transport=httpx.MockTransport(upstream)))) as client:
        row=client.get("/v1/search",headers=AUTH,params={"q":"fixture","dearrow_titles":True,"dearrow_thumbnails":True}).json()["items"][0]
        assert row["title"]==info()["title"] and not row.get("dearrow_title") and not row.get("dearrow_art_url")


def test_job_path_and_expired_capability_original_fallback():
    requests=[]
    def upstream(request):
        requests.append(request)
        if request.url.host=="sponsor.ajay.app": return httpx.Response(200,json={ID:entry()})
        return httpx.Response(200,content=b"\x89PNG\r\n\x1a\nfixture")
    app=create_app(extractor=FakeExtractor(),token=TOKEN,http_client=httpx.AsyncClient(transport=httpx.MockTransport(upstream)))
    with TestClient(app) as client:
        job=client.post("/v1/jobs",headers=AUTH,json={"action":"search","query":"fixture","dearrow_titles":True,"dearrow_thumbnails":True}).json()["job"]
        for _ in range(100):
            result=client.get("/v1/jobs/"+job,headers=AUTH).json()
            if result["state"]!="pending":break
            time.sleep(.01)
        row=result["result"]["items"][0];assert row["dearrow_title"]=="Community title"
        token=row["dearrow_art_url"].rsplit("/",1)[1]
        record=app.state.dearrow.images[token];app.state.dearrow.images[token]=(0,*record[1:])
        assert client.get(row["dearrow_art_url"]).status_code==200
        assert requests[-1].url.host=="i.ytimg.com"


def test_nonvideo_library_rows_never_query_branding():
    import asyncio
    from prosperotube.dearrow import DeArrow
    async def case():
        async with httpx.AsyncClient(transport=httpx.MockTransport(lambda r: pytest.fail("nonvideo lookup"))) as client:
            d=DeArrow(client)
            for kind in ("comments","profiles","live_chat"):
                source={"kind":kind,"items":[{"id":ID,"title":"unchanged"}]}
                assert await d.decorate(source,True,True,"http://local")==source
    asyncio.run(case())
