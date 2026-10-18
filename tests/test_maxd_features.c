/* Rule persistence and measured adaptation policies. GPL-3.0-or-later. */
#include "maxd/smart_rules.h"
#include "maxd/auto_max.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    MaxdSmartStore store={0},loaded;
    MaxdSmartRule *r=maxd_smart_create(&store,"guest","Gaming tonight");assert(r);
    strcpy(r->keyword,"Gaming");strcpy(r->creator,"Studio");r->minimum=600;r->maximum=1800;
    assert(maxd_smart_matches(r,"New GAMING release","Studio games",600,0,0,0));
    assert(maxd_smart_matches(r,"gaming","studio",1800,0,0,0));
    assert(!maxd_smart_matches(r,"gaming","studio",599,0,0,0));
    assert(!maxd_smart_matches(r,"gaming","studio",1801,0,0,0));
    assert(!maxd_smart_matches(r,"gaming","studio",900,1,0,0));
    assert(!maxd_smart_matches(r,"gaming","studio",900,0,1,0));
    assert(!maxd_smart_matches(r,"gaming","studio",900,0,0,1));
    assert(!maxd_smart_matches(r,"other","studio",900,0,0,0));
    assert(!maxd_smart_matches(r,"gaming","other",900,0,0,0));
    cJSON *root=cJSON_CreateObject();maxd_smart_write(&store,root);maxd_smart_read(&loaded,root);
    assert(loaded.count==1 && !memcmp(&store.rules[0],&loaded.rules[0],sizeof *r));cJSON_Delete(root);
    assert(!maxd_smart_find(&loaded,r->id,"alice"));
    assert(maxd_smart_create(&loaded,"alice","Gaming tonight"));
    for(int i=1;i<MAXD_SMART_PER_PROFILE;++i)assert(maxd_smart_create(&store,"guest","Rule"));
    assert(!maxd_smart_create(&store,"guest","Overflow"));
    root=cJSON_Parse("{\"maxd_smart_playlists\":[{\"id\":-1,\"name\":\"bad\",\"profile\":\"guest\"},{\"id\":1,\"name\":\"ok\",\"profile\":\"guest\"},{\"id\":1,\"name\":\"dup\",\"profile\":\"alice\"}]}");
    maxd_smart_read(&loaded,root);assert(loaded.count==1 && loaded.serial==1);cJSON_Delete(root);
    assert(!maxd_smart_text("\xc0\xaf",128,0) && !maxd_smart_text("bad\nrule",128,0));
    MaxdAutoMax s;maxd_auto_reset(&s);uint64_t now=0,frames=0,dropped=0;
    assert(!maxd_auto_tick(&s,now,frames,dropped,0,0));
    for(int i=0;i<10;++i){now+=1000;frames+=30;assert(!maxd_auto_tick(&s,now,frames,dropped,0,1));}
    for(int i=0;i<2;++i){now+=1000;assert(!maxd_auto_tick(&s,now,frames,dropped,0,1));}
    now+=1000;assert(maxd_auto_tick(&s,now,frames,dropped,0,1)==MAXD_AUTO_BUFFER);
    maxd_auto_changed(&s);assert(s.changes==1);
    for(int i=0;i<35;++i){now+=1000;frames+=30;dropped+=10;assert(!maxd_auto_tick(&s,now,frames,dropped,0,0));}
    maxd_auto_reset(&s);now=0;frames=dropped=0;maxd_auto_tick(&s,0,0,0,0,0);
    for(int i=0;i<17;++i){now+=1000;frames+=30;dropped+=3;assert(!maxd_auto_tick(&s,now,frames,dropped,0,0));}
    now+=1000;frames+=30;dropped+=3;assert(maxd_auto_tick(&s,now,frames,dropped,0,0)==MAXD_AUTO_DROP);
    assert(!maxd_auto_tick(&s,now+1000,frames+30,dropped+30,1,1));
    maxd_auto_reset(&s);now=0;frames=0;maxd_auto_tick(&s,0,0,0,0,0);
    for(int i=0;i<99;++i){now+=1000;frames+=30;assert(!maxd_auto_tick(&s,now,frames,0,0,0));}
    assert(maxd_auto_tick(&s,100000,3000,0,0,0)==MAXD_AUTO_UP);
    assert(!maxd_auto_tick(&s,1,1,0,0,0)); /* seek/restart and backwards clock */
    MaxdAutoFormat f[]={{2160,60,3,20000000},{2160,60,2,18000000},{1440,60,2,9000000},{1080,30,1,4000000},{720,30,1,2000000}};
    assert(maxd_auto_pick(f,5,0,MAXD_AUTO_DROP,2160,0,0,0,0)==1); /* avoid software AV1 first */
    assert(maxd_auto_pick(f,5,1,MAXD_AUTO_BUFFER,2160,0,0,0,0)==2);
    assert(maxd_auto_pick(f,5,3,MAXD_AUTO_UP,1440,0,0,0,0)==2);
    assert(maxd_auto_pick(f,5,3,MAXD_AUTO_UP,1080,0,0,0,0)==-1);
    assert(maxd_auto_pick(f,5,3,MAXD_AUTO_UP,2160,30,1,0,0)==-1);
    assert(maxd_auto_pick(f,5,1,MAXD_AUTO_DROP,2160,0,0,maxd_auto_cost(f[1]),0)==2);
    puts("PASS: smart rule bounds, filtering, profiles, JSON; Auto Max warmup, windows, cooldown, pause/reset, recovery, codec/FPS/quality ceilings");
}
