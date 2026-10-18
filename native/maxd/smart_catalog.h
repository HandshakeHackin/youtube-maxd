/* Smart playlists use existing catalog requests and local filtered snapshots.
 * Included in tube_provider.c; main thread only. GPL-3.0-or-later. */
#define MAXD_SMART_ITEMS 128
#define MAXD_SMART_CACHES 4
static const char *const maxd_smart_sources[]={"library:home","library:subscriptions","favorites","queue"};
static const char *const maxd_smart_source_names[]={"Home","Subscriptions","Favorites","Play queue"};
static int catalog(const char *,int,evo_provider_items_cb,void *);
typedef struct {
    char group[43],profile[33];uint32_t rule;unsigned scope,serial;
    evo_provider_item_t items[MAXD_SMART_ITEMS];int count,limited;
} MaxdSmartCache;
static MaxdSmartCache g_maxd_smart_cache[MAXD_SMART_CACHES];
typedef struct {
    int used,inflight,cancelled,page,cache;
    unsigned serial;uint32_t rule;
    char profile[33];unsigned scope;
    MaxdSmartRule snapshot;
    long long deadline;
    evo_provider_items_cb callback;void *data;
} MaxdSmartScan;
static MaxdSmartScan g_maxd_smart_scans[4];
static unsigned g_maxd_smart_serial;
static int g_maxd_smart_internal;
static struct { unsigned ticket;uint32_t rule;int field;char profile[33]; } g_maxd_smart_prompt;
static int maxd_smart_group(const char *group) { return group && !strncmp(group,"group:6d617864-",15); }
static void maxd_smart_cancel(void) {
    if(g_maxd_smart_internal)return;
    for(int i=0;i<4;++i) {
        MaxdSmartScan *s=&g_maxd_smart_scans[i];if(!s->used || s->cancelled)continue;
        s->cancelled=1;
        evo_provider_items_cb cb=s->callback;void *data=s->data;s->callback=NULL;
        if(cb)cb(0,NULL,0,0,data);
        if(!s->inflight)memset(s,0,sizeof *s);
    }
}
static void maxd_smart_reset(void) {
    maxd_smart_cancel();memset(g_maxd_smart_cache,0,sizeof g_maxd_smart_cache);
    memset(&g_maxd_smart_store,0,sizeof g_maxd_smart_store);
    memset(&g_maxd_smart_prompt,0,sizeof g_maxd_smart_prompt);
}
static void maxd_smart_tick(void) {
    for(int i=0;i<4;++i) {
        MaxdSmartScan *s=&g_maxd_smart_scans[i];
        if(s->used && !s->cancelled && (now_ms()>s->deadline || strcmp(s->profile,g_library_profile) || s->scope!=g_playlist_scope)) {
            tube_set_error("Smart playlist refresh expired or changed profile. Reopen it to retry.");
            maxd_smart_cancel();return;
        }
    }
}
static void maxd_smart_rows(MaxdSmartCache *cache,int page,evo_provider_items_cb cb,void *data) {
    int start=page*LOCAL_PAGE;
    if(start<0 || start>cache->count) { cb(0,NULL,0,0,data);return; }
    int count=cache->count-start;if(count>LOCAL_PAGE)count=LOCAL_PAGE;
    cb(1,cache->items+start,count,start+count<cache->count,data);
}
static void maxd_smart_scan_next(MaxdSmartScan *scan);
static void maxd_smart_scanned(int ok,const evo_provider_item_t *items,int count,int more,void *data) {
    MaxdSmartScan *s=data;s->inflight=0;
    MaxdSmartRule *rule=maxd_smart_find(&g_maxd_smart_store,s->rule,g_library_profile);
    if(!s->used)return;
    if(s->cancelled || !ok || !rule || strcmp(s->profile,g_library_profile) || s->scope!=g_playlist_scope ||
       memcmp(rule,&s->snapshot,sizeof *rule) || now_ms()>s->deadline || count<0 || count>512 || (count && !items)) {
        evo_provider_items_cb cb=s->callback;void *ud=s->data;memset(s,0,sizeof *s);
        if(cb)cb(0,NULL,0,0,ud);
        return;
    }
    MaxdSmartCache *cache=&g_maxd_smart_cache[s->cache];
    for(int i=0;i<count && cache->count<MAXD_SMART_ITEMS;++i) {
        const evo_provider_item_t *item=&items[i];
        if(item->is_folder || !valid_id(item->id) || blocked_identity(item->parent_id,item->subtitle)>=0)continue;
        int played=item->played;const Record *known=record(item->id,0);if(known && known->item.played)played=1;
        if(!maxd_smart_matches(rule,item->title,item->subtitle,item->duration_sec,played,item->is_live,
                              !strcmp(tube_row_fact_get(item->id,"is_upcoming"),"1")))continue;
        int duplicate=0;for(int j=0;j<cache->count;++j)duplicate|=!strcmp(cache->items[j].id,item->id);
        if(duplicate)continue;
        evo_provider_item_t *out=&cache->items[cache->count];*out=*item;
        copy(out->playback_playlist,sizeof out->playback_playlist,cache->group);
        out->playback_index=cache->count;out->playback_scope=cache->scope;++cache->count;
    }
    if(more && s->page<7 && cache->count<MAXD_SMART_ITEMS) { ++s->page;maxd_smart_scan_next(s);return; }
    cache->limited=more;
    evo_provider_items_cb cb=s->callback;void *ud=s->data;memset(s,0,sizeof *s);
    if(cb)maxd_smart_rows(cache,0,cb,ud);
}
static void maxd_smart_scan_next(MaxdSmartScan *s) {
    const unsigned serial=s->serial;s->inflight=1;++g_maxd_smart_internal;
    int rc=catalog(maxd_smart_sources[s->snapshot.source],s->page,maxd_smart_scanned,s);
    --g_maxd_smart_internal;
    if(rc<0 && s->used && s->serial==serial && s->inflight)maxd_smart_scanned(0,NULL,0,0,s);
}
static int maxd_smart_run(uint32_t id,int page,evo_provider_items_cb cb,void *data) {
    MaxdSmartRule *rule=maxd_smart_find(&g_maxd_smart_store,id,g_library_profile);
    if(!rule) { tube_set_error("Reopen Smart playlists for this profile.");cb(0,NULL,0,0,data);return 0; }
    if(page) {
        MaxdSmartCache *latest=NULL;
        for(int i=0;i<MAXD_SMART_CACHES;++i) {
            MaxdSmartCache *c=&g_maxd_smart_cache[i];
            if(c->rule==id && c->scope==g_playlist_scope && !strcmp(c->profile,g_library_profile)) {
                if(!latest || c->serial>latest->serial)latest=c;
            }
        }
        if(latest) { maxd_smart_rows(latest,page,cb,data);return 0; }
        cb(0,NULL,0,0,data);return 0;
    }
    maxd_smart_cancel();
    int slot=-1;for(int i=0;i<4;++i)if(!g_maxd_smart_scans[i].used){slot=i;break;}
    int cache=-1;
    for(int i=0;i<MAXD_SMART_CACHES;++i) {
        int busy=0;for(int j=0;j<4;++j)busy|=g_maxd_smart_scans[j].used&&g_maxd_smart_scans[j].cache==i;
        if(busy || (*g_active_playlist.playlist && !strcmp(g_active_playlist.playlist,g_maxd_smart_cache[i].group)))continue;
        if(cache<0 || g_maxd_smart_cache[i].serial<g_maxd_smart_cache[cache].serial)cache=i;
    }
    if(slot<0 || cache<0) { cb(0,NULL,0,0,data);return 0; }
    MaxdSmartScan *s=&g_maxd_smart_scans[slot];memset(s,0,sizeof *s);
    s->used=1;s->serial=++g_maxd_smart_serial;s->cache=cache;s->rule=id;s->scope=g_playlist_scope;
    s->snapshot=*rule;s->deadline=now_ms()+60000;s->callback=cb;s->data=data;
    copy(s->profile,sizeof s->profile,g_library_profile);
    MaxdSmartCache *c=&g_maxd_smart_cache[cache];memset(c,0,sizeof *c);c->serial=s->serial;c->rule=id;c->scope=s->scope;
    copy(c->profile,sizeof c->profile,s->profile);
    snprintf(c->group,sizeof c->group,"group:6d617864-%04x-4000-8000-%012x",s->scope&65535,s->serial);
    maxd_smart_scan_next(s);return 0;
}
static int maxd_smart_window(const char *group,int index,int target) {
    if(!maxd_smart_group(group))return -1;
    for(int i=0;i<MAXD_SMART_CACHES;++i) {
        MaxdSmartCache *c=&g_maxd_smart_cache[i];
        if(strcmp(group,c->group) || strcmp(c->profile,g_active_profile) || c->scope!=g_playlist_scope)continue;
        int at=target>=0?target:index;
        if(at<0 || at>=c->count)return 0;
        tube_playlist_window_cancel();g_playlist_window.state=1;g_playlist_window.context=g_active_playlist;
        g_playlist_window.selection=g_selection_serial;g_playlist_window.lookup=target>=0;g_playlist_window.target_index=at;
        g_playlist_window.scan_next=-1;
        copy(g_playlist_window.id,sizeof g_playlist_window.id,g_active_id);
        tube_playlist_window_t *v=&g_playlist_window.value;memset(v,0,sizeof *v);
        v->count=v->playlist_size=c->count;v->current=at;v->scope=c->scope;v->selection=g_selection_serial;
        copy(v->playlist,sizeof v->playlist,group);
        for(int j=0;j<c->count;++j) {
            copy(v->ids[j],sizeof v->ids[j],c->items[j].id);v->indices[j]=j;
            const char *channel=c->items[j].parent_id;
            if(!strncmp(channel,"channel:",8))channel+=8;
            if(valid_channel(channel))copy(v->channels[j],sizeof v->channels[j],channel);
            copy(v->channel_names[j],sizeof v->channel_names[j],c->items[j].subtitle);
        }
        return 1;
    }
    return 0;
}
static int maxd_smart_prompt_submit(unsigned ticket,const char *text,char *parent,size_t capacity) {
    if(!g_maxd_smart_prompt.ticket || ticket!=g_maxd_smart_prompt.ticket ||
       strcmp(g_maxd_smart_prompt.profile,g_library_profile))return -1;
    MaxdSmartRule *r=maxd_smart_find(&g_maxd_smart_store,g_maxd_smart_prompt.rule,g_library_profile);
    int field=g_maxd_smart_prompt.field;
    if(field==0)r=maxd_smart_create(&g_maxd_smart_store,g_library_profile,text);
    else if(!r || !maxd_smart_text(text,field==1?80:128,field!=1))return -1;
    if(!r)return -1;
    if(field==0 && strcmp(g_library_profile,"guest"))r->source=1;
    if(field==1)copy(r->name,sizeof r->name,text);
    if(field==2)copy(r->keyword,sizeof r->keyword,text);
    if(field==3)copy(r->creator,sizeof r->creator,text);
    snprintf(parent,capacity,"settings:smart:%u",r->id);
    memset(&g_maxd_smart_prompt,0,sizeof g_maxd_smart_prompt);g_playlist_prompt.active=0;save();return 0;
}
static uint32_t maxd_smart_id(const char *value,const char **action) {
    if(!value || *value<'1' || *value>'9')return 0;
    uint64_t id=0;
    while(*value>='0'&&*value<='9') { id=id*10+(unsigned)(*value++-'0');if(id>UINT32_MAX)return 0; }
    if(*value && *value!=':')return 0;
    if(action)*action=*value?value+1:"";
    return (uint32_t)id;
}
static void maxd_smart_fact(const char *id,const char *kind,const char *label,const char *value) {
    for(int i=0;i<g_row_fact_used;++i)if(!strcmp(g_row_facts[i].row,id) &&
        (!strcmp(g_row_facts[i].name,"value") || !strcmp(g_row_facts[i].name,"text")))g_row_facts[i].value[0]=0;
    tube_row_fact(id,"kind",kind);tube_row_fact(id,"label",label);tube_row_fact(id,"value",value);
    if(!strcmp(kind,"text"))tube_row_fact(id,"text",value);
}
static int maxd_smart_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent || strncmp(parent,"settings:smart",14))return 0;
    evo_provider_item_t out[32];int n=0;
    if(page) { cb(1,NULL,0,0,ud);return 1; }
    if(!strcmp(parent,"settings:smart")) {
        CATALOG_FOLDER("settings:smart:new","Create smart playlist","Saved on this PS5, for the current profile");
        maxd_smart_fact(out[n-1].id,"text","Create smart playlist","");
        for(int i=0;i<g_maxd_smart_store.count;++i) {
            const MaxdSmartRule *r=&g_maxd_smart_store.rules[i];if(strcmp(r->profile,g_library_profile))continue;
            char id[80];snprintf(id,sizeof id,"settings:smart:%u",r->id);
            CATALOG_FOLDER(id,r->name,"Open, edit rules or delete this local smart playlist");
            maxd_smart_fact(id,"page",r->name,"");
        }
        cb(1,out,n,0,ud);return 1;
    }
    const char *action="";uint32_t id=!strncmp(parent,"settings:smart:",15)?maxd_smart_id(parent+15,&action):0;
    MaxdSmartRule *r=maxd_smart_find(&g_maxd_smart_store,id,g_library_profile);
    int field=!strcmp(parent,"settings:smart:new")?0:!strcmp(action,"name")?1:!strcmp(action,"keyword")?2:!strcmp(action,"creator")?3:-1;
    if(field>=0) {
        if(field && !r){cb(0,NULL,0,0,ud);return 1;}
        prompt_arm(field==0?"Name your smart playlist":field==1?"Playlist name":field==2?"Title contains (blank = any)":"Creator contains (blank = any)");
        g_playlist_prompt.maxd_smart=1;
        g_maxd_smart_prompt.ticket=g_playlist_prompt.prompt.ticket;g_maxd_smart_prompt.rule=id;g_maxd_smart_prompt.field=field;
        copy(g_maxd_smart_prompt.profile,sizeof g_maxd_smart_prompt.profile,g_library_profile);
        if(field)copy(g_playlist_prompt.prompt.initial,sizeof g_playlist_prompt.prompt.initial,field==1?r->name:field==2?r->keyword:r->creator);
        CATALOG_FOLDER("account:notice","Enter your playlist rule","No YouTube account changes are made");cb(1,out,n,0,ud);return 1;
    }
    if(!r){tube_set_error("Reopen Smart playlists for this profile.");cb(0,NULL,0,0,ud);return 1;}
    if(!strcmp(action,"delete")) {
        char row[80];snprintf(row,sizeof row,"settings:smart:%u:delete-confirm",id);
        CATALOG_FOLDER(row,"Delete this smart playlist","Cross confirms local deletion; Circle keeps it");
        maxd_smart_fact(row,"action","Delete this smart playlist","");cb(1,out,n,0,ud);return 1;
    }
    if(!strcmp(action,"delete-confirm")) {
        int at=(int)(r-g_maxd_smart_store.rules);
        memmove(r,r+1,(size_t)(g_maxd_smart_store.count-at-1)*sizeof *r);--g_maxd_smart_store.count;save();
        return maxd_smart_catalog("settings:smart",0,cb,ud);
    }
    int changed=0;
    if(!strcmp(action,"source")){r->source=(r->source+1)%4;changed=1;}
    else if(!strcmp(action,"unwatched")){r->unwatched=!r->unwatched;changed=1;}
    else if(!strcmp(action,"live")){r->hide_live=!r->hide_live;changed=1;}
    else if(!strcmp(action,"duration")) {
        const int bounds[][2]={{0,0},{0,600},{600,1800},{1800,3600},{3600,0}};int next=0;
        for(int i=0;i<5;++i)if(r->minimum==bounds[i][0]&&r->maximum==bounds[i][1])next=(i+1)%5;
        r->minimum=bounds[next][0];r->maximum=bounds[next][1];changed=1;
    } else if(*action){cb(0,NULL,0,0,ud);return 1;}
    if(changed)save();
    char row[80],label[240],value[80];
    snprintf(row,sizeof row,"maxd:smart:run:%u",id);CATALOG_FOLDER(row,"Open playlist","Refresh matching videos from the first eight feed pages (up to 128 matches)");
    maxd_smart_fact(row,"link","Open playlist","");tube_row_fact(row,"screen","grid");tube_row_fact(row,"argument",row);
#define MAXD_SMART_ROW(key,title,detail,kind,shown) do { \
    snprintf(row,sizeof row,"settings:smart:%u:%s",id,key);CATALOG_FOLDER(row,title,detail);maxd_smart_fact(row,kind,title,shown); \
} while(0)
    MAXD_SMART_ROW("name","Name",r->name,"text",r->name);
    MAXD_SMART_ROW("source","Source feed",maxd_smart_source_names[r->source],"cycle",maxd_smart_source_names[r->source]);
    MAXD_SMART_ROW("keyword","Title contains",*r->keyword?r->keyword:"Any title","text",r->keyword);
    MAXD_SMART_ROW("creator","Creator contains",*r->creator?r->creator:"Any creator","text",r->creator);
    if(!r->minimum&&!r->maximum)copy(value,sizeof value,"Any duration");
    else if(!r->maximum)snprintf(value,sizeof value,"At least %d minutes",r->minimum/60);
    else snprintf(value,sizeof value,"%d to %d minutes",r->minimum/60,r->maximum/60);
    MAXD_SMART_ROW("duration","Duration",value,"cycle",value);
    snprintf(label,sizeof label,"Unwatched only: %s",r->unwatched?"on":"off");MAXD_SMART_ROW("unwatched",label,"Account and local watched markers","cycle",r->unwatched?"On":"Off");
    tube_row_fact(row,"label","Unwatched only");
    snprintf(label,sizeof label,"Hide live videos: %s",r->hide_live?"on":"off");MAXD_SMART_ROW("live",label,"Upcoming videos are always excluded","cycle",r->hide_live?"On":"Off");
    tube_row_fact(row,"label","Hide live videos");
    MAXD_SMART_ROW("delete","Delete playlist","Review before deleting local rules","page","");
#undef MAXD_SMART_ROW
    cb(1,out,n,0,ud);return 1;
}
