/* Optional support: the real provider routes to the same phone QR in both UIs. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_qr_page.h"

static int actual_afterglow;
int tube_afterglow_enabled(void) { return actual_afterglow; }

int main(void) {
    char dir[]="/tmp/maxd-support-XXXXXX",payload[TUBE_QR_PAYLOAD_SIZE];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    const evo_provider_t *p=&evo_provider_youtube;
    assert(!p->init());
    for(actual_afterglow=0;actual_afterglow<=1;++actual_afterglow) {
        p->list_catalog("settings:about",0,items_cb,NULL);
        assert(last_ok && !pending_count);
        int support=-1;
        for(int i=0;i<last_count;++i)if(!strcmp(last_items[i].id,"link:maxd-support"))support=i;
        assert(support>=0 && strstr(last_items[support].title,"YouTube Max") && strstr(last_items[support].subtitle,"Optional"));
        assert(!strcmp(tube_row_fact_get("link:maxd-support","kind"),"link"));
        assert(!strcmp(tube_row_fact_get("link:maxd-support","screen"),"reader"));
        assert(!strcmp(tube_row_fact_get("link:maxd-support","argument"),"link:maxd-support"));
        p->list_catalog("link:maxd-support",0,items_cb,NULL);
        assert(last_ok && last_count==1 && !last_more && !pending_count);
        assert(!strcmp(last_items[0].overview,"https://ko-fi.com/handshakehackin"));
        assert(strstr(last_items[0].subtitle,"Optional") && strstr(last_items[0].subtitle,"HandshakeHackin"));
        assert(tube_qr_row_payload("link:maxd-support",last_items[0].id,last_items[0].title,
                                   last_items[0].overview,payload,sizeof payload));
        tube_qr_code code;
        assert(!strcmp(payload,"https://ko-fi.com/handshakehackin"));
        assert(!tube_qr_encode(payload,strlen(payload),TUBE_QR_LEVEL_M,&code) && code.version<=5);
        /* Selecting the displayed address stays inside the phone-link screen. */
        p->list_catalog(last_items[0].id,0,items_cb,NULL);
        assert(last_ok && last_count==1 && !strcmp(last_items[0].id,"account:notice") && !pending_count);
        p->list_catalog("link:maxd-release",0,items_cb,NULL);
        assert(last_ok && last_count==1 && !pending_count);
        assert(!strcmp(last_items[0].overview,"https://github.com/HandshakeHackin/youtube-maxd/releases/latest"));
        assert(tube_qr_row_payload("link:maxd-release",last_items[0].id,last_items[0].title,
                                   last_items[0].overview,payload,sizeof payload));
        assert(!strcmp(payload,"https://github.com/HandshakeHackin/youtube-maxd/releases/latest"));
        assert(!tube_qr_encode(payload,strlen(payload),TUBE_QR_LEVEL_M,&code));
        p->list_catalog("link:maxd-support",1,items_cb,NULL);
        assert(!last_ok && !pending_count);
        p->list_catalog("link:maxd-support:other",0,items_cb,NULL);
        assert(!last_ok && !pending_count);
    }
    p->shutdown();
    unlink(evo_data_path("youtube.json"));
    assert(!rmdir(dir));
    puts("PASS: Classic/Afterglow support routing, optional wording, exact Ko-fi QR and no outbound payment requests");
}
