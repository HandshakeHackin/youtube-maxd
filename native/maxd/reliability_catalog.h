/* Reliability controls, using the provider's existing save and playback paths.
 * SPDX-License-Identifier: GPL-3.0-or-later */
int tube_maxd_channel_leveling(void) {
    if(!g_maxd_playing)return -1;
    const Record *r=record_for(g_active_id,g_active_profile,0);
    const MaxdChannelPref *p=r?maxd_channel_pref(&g_maxd_reliability,g_active_profile,r->item.parent_id,0):NULL;
    return p?p->leveling:-1;
}
const char *tube_maxd_playback_diagnosis(void) {
    if(!g_maxd_failure.count || strcmp(g_maxd_failure.profile,g_library_profile))return "No playback failure has been recorded in this profile's current session.";
    switch(g_maxd_failure.code) {
    case 1:return "A source or resolve request failed. Retry to refresh the stream; the error alone does not identify a network or URL-expiry cause.";
    case 2:return "The video decoder reported an error. Try an available AVC format at up to 1080p30 SDR in Playback health & rescue.";
    case 3:return "The audio decoder reported an error. Try another audio track or resolve the video again.";
    case 4:return "Sustained buffering was observed. Try a lower quality in Playback health & rescue.";
    case 5:return "The resolver returned no supported playback formats. Retry with Auto codec or review this channel's quality ceiling.";
    default:return "No classified playback failure was recorded.";
    }
}
static int maxd_reliability_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent)return 0;
    const char *base=!strcmp(parent,"settings:reliability")?"settings:reliability":!strncmp(parent,"settings:channel-prefs",22)?"settings:channel-prefs":
        !strncmp(parent,"settings:session-recovery",25)?"settings:session-recovery":
        !strncmp(parent,"settings:storage",16)?"settings:storage":
        !strcmp(parent,"settings:playback-diagnostics")?"settings:playback-diagnostics":
        !strcmp(parent,"settings:quick-switch")?"settings:quick-switch":NULL;
    if(!base)return 0;
    evo_provider_item_t out[32];int n=0;const char *action="";
    if(strcmp(parent,base) && !maxd_viewing_owner(parent,base,&action)){tube_set_error("Reopen this page for the current profile.");cb(0,NULL,0,0,ud);return 1;}
    if(page && strcmp(base,"settings:channel-prefs")){cb(1,NULL,0,0,ud);return 1;}
    char id[160],detail[256];
#define MAXD_RELIABLE_ROW(key,title,help,kind) do { \
    snprintf(id,sizeof id,"%s:%s:%s",base,g_library_profile,key);CATALOG_FOLDER(id,title,help);maxd_smart_fact(id,kind,title,""); \
} while(0)
    if(!strcmp(base,"settings:reliability")) {
        const char *ids[]={"settings:channel-prefs","settings:session-recovery","settings:playback-diagnostics","settings:quick-switch","settings:storage","settings:backup"};
        const char *names[]={"Channel preferences","Interrupted session","Playback diagnostics","Quick Switch","Storage and cache","Offline backup and restore"};
        for(int i=0;i<6;++i){CATALOG_FOLDER(ids[i],names[i],"Open these controls");maxd_smart_fact(ids[i],"page",names[i],"");}
    } else if(!strcmp(base,"settings:quick-switch")) {
        const char *ids[]={"continue-watching","library:subscriptions","search-history","queue"};
        const char *names[]={"Continue Watching","Subscriptions","Search","Play queue"};
        for(int i=0;i<4;++i){CATALOG_FOLDER(ids[i],names[i],i==2?"Square starts a new search; recent queries remain available":"Jump to this page");maxd_smart_fact(ids[i],"link",names[i],"");tube_row_fact(ids[i],"screen",i==2?"search":"grid");tube_row_fact(ids[i],"argument",i==2?"":ids[i]);}
    } else if(!strcmp(base,"settings:playback-diagnostics")) {
        CATALOG_FOLDER("account:notice","Last observed playback failure",tube_maxd_playback_diagnosis());
        CATALOG_FOLDER("account:notice:diagnostics","Recovery options","During playback, press R2 and select Playback health & rescue to review lower-quality and compatible AVC alternatives. Alternatives depend on the formats already resolved.");
        CATALOG_FOLDER("settings:codec","Default codec","Select Auto or another supported codec for future resolves");
    } else if(!strcmp(base,"settings:session-recovery")) {
        MaxdCheckpoint *p=maxd_checkpoint(&g_maxd_reliability,g_library_profile,0);
        if(!p || !p->pending){CATALOG_FOLDER("account:notice","No interrupted session","Normal exits keep your queue and positions without an interruption offer");}
        else if(!strcmp(action,"dismiss")){p->pending=p->active=0;save();CATALOG_FOLDER("queue","Open play queue","The queue and saved video history were kept");}
        else if(!strcmp(action,"resume")) {
            Record *r=record(p->video,0);
            if(!r || r->item.is_live || blocked_identity(r->item.parent_id,r->item.subtitle)>=0){tube_set_error("This saved video is no longer available in this profile.");cb(0,NULL,0,0,ud);return 1;}
            r->item.resume_pos_sec=p->position;r->item.played=0;out[n++]=r->item;out[0].is_folder=0;out[0].playback_playlist[0]=0;out[0].playback_scope=0;
        } else if(*action){cb(0,NULL,0,0,ud);return 1;}
        else {
            snprintf(detail,sizeof detail,"Saved position %lld:%02lld; queue order is retained. Playback starts only if you select Resume.",(long long)p->position/60,(long long)p->position%60);
            MAXD_RELIABLE_ROW("resume","Resume interrupted video",detail,"link");
            tube_row_fact(id,"screen","grid");tube_row_fact(id,"argument",id);
            CATALOG_FOLDER("queue","Review remaining play queue","Review or reorder saved entries before resuming");maxd_smart_fact("queue","link","Review remaining play queue","");tube_row_fact("queue","screen","grid");tube_row_fact("queue","argument","queue");
            MAXD_RELIABLE_ROW("dismiss","Dismiss interruption offer","Keep the queue, bookmarks and saved history","action");
        }
    } else if(!strcmp(base,"settings:storage")) {
        unsigned long long bytes=0;int pictures=0;
        int available=tube_maxd_thumbnail_cache?tube_maxd_thumbnail_cache(&bytes,&pictures,0):0;
        if(!strcmp(action,"thumbnails")) {
            if(!available || !tube_maxd_thumbnail_cache(&bytes,&pictures,1)){tube_set_error("The Afterglow thumbnail cache is not active. Open Afterglow first.");cb(0,NULL,0,0,ud);return 1;}
            bytes=0;pictures=0;
        } else if(!strcmp(action,"captions")) {
            int busy=g_maxd_playing || (tube_phone_owns_media && tube_phone_owns_media());for(int i=0;i<REQUESTS;++i)busy|=g_requests[i].used;
            if(busy){tube_set_error("Stop playback and wait for background requests to finish before clearing temporary captions.");cb(0,NULL,0,0,ud);return 1;}
            char path[640];snprintf(path,sizeof path,"%s/active.srt",evo_data_dir());
            struct stat info;if(!stat(path,&info) && (!S_ISREG(info.st_mode) || remove(path))){tube_set_error("The temporary caption file could not be removed.");cb(0,NULL,0,0,ud);return 1;}
            g_caption_path[0]=g_caption_language[0]=0;
        } else if(*action){cb(0,NULL,0,0,ud);return 1;}
        if(available)snprintf(detail,sizeof detail,"%.1f MiB, %d ready pictures; reloaded on demand",bytes/1048576.0,pictures);else copy(detail,sizeof detail,"Afterglow is not active; its RAM cache is unavailable");
        MAXD_RELIABLE_ROW("thumbnails","Clear Afterglow thumbnail RAM",detail,"action");
        char path[640];struct stat info;snprintf(path,sizeof path,"%s/active.srt",evo_data_dir());
        long long caption_bytes=!stat(path,&info)&&S_ISREG(info.st_mode)?(long long)info.st_size:0;
        snprintf(detail,sizeof detail,"%lld KiB; available only when playback and requests are idle",(caption_bytes+1023)/1024);
        MAXD_RELIABLE_ROW("captions","Clear temporary caption file",detail,"action");
        CATALOG_FOLDER("account:notice","Saved data is kept","These controls do not remove profiles, credentials, bookmarks, queue records, backups or the voice model. Thumbnail RAM is separate from disk usage.");
    } else {
        if(!*action) {
            char channels[256][25];int count=0;
            for(int i=0;i<g_maxd_reliability.count && count<256;++i)if(!strcmp(g_maxd_reliability.channels[i].profile,g_library_profile))copy(channels[count++],25,g_maxd_reliability.channels[i].channel);
            for(int i=0;i<g_count && count<256;++i)if(current_record(&g_records[i])&&valid_channel(g_records[i].item.parent_id)) {
                int duplicate=0;for(int j=0;j<count;++j)duplicate|=!strcmp(channels[j],g_records[i].item.parent_id);
                if(!duplicate)copy(channels[count++],25,g_records[i].item.parent_id);
            }
            for(int i=0;i<g_follow_count && count<256;++i)if(!strcmp(g_follows[i].profile,g_library_profile)) {
                int duplicate=0;for(int j=0;j<count;++j)duplicate|=!strcmp(channels[j],g_follows[i].channel);
                if(!duplicate)copy(channels[count++],25,g_follows[i].channel);
            }
            for(int i=page*LOCAL_PAGE;i<count&&n<LOCAL_PAGE;++i){const char *name=channels[i];
                for(int j=0;j<g_count;++j)if(current_record(&g_records[j])&&!strcmp(g_records[j].item.parent_id,channels[i])&&*g_records[j].item.subtitle)name=g_records[j].item.subtitle;
                for(int j=0;j<g_follow_count;++j)if(!strcmp(g_follows[j].profile,g_library_profile)&&!strcmp(g_follows[j].channel,channels[i]))name=g_follows[j].title;
                MAXD_RELIABLE_ROW(channels[i],name,"Channel-specific preferences for future videos","page");}
            if(!count)CATALOG_FOLDER("account:notice","Browse or follow a channel first","Known channels appear here for this profile");
            cb(1,out,n,(page+1)*LOCAL_PAGE<count,ud);return 1;
        }
        char channel[25];if(strlen(action)<24){cb(0,NULL,0,0,ud);return 1;}memcpy(channel,action,24);channel[24]=0;
        if(!valid_channel(channel) || (action[24] && action[24]!=':')){cb(0,NULL,0,0,ud);return 1;}
        const char *operation=action+24;if(*operation)++operation;
        if(*operation && strcmp(operation,"quality") && strcmp(operation,"speed") && strcmp(operation,"captions") && strcmp(operation,"leveling") && strcmp(operation,"reset")){cb(0,NULL,0,0,ud);return 1;}
        int editing=*operation && strcmp(operation,"reset");
        MaxdChannelPref *p=maxd_channel_pref(&g_maxd_reliability,g_library_profile,channel,editing);
        if(editing && !p){tube_set_error("Channel preference storage is full.");cb(0,NULL,0,0,ud);return 1;}
        if(!strcmp(operation,"quality")){const int values[]={0,360,720,1080,1440,2160};int next=0;for(int i=0;i<6;++i)if(p->height==values[i])next=(i+1)%6;p->height=values[next];save();}
        else if(!strcmp(operation,"speed")){const int values[]={0,50,75,100,125,150,200};int next=0;for(int i=0;i<7;++i)if(p->speed==values[i])next=(i+1)%7;p->speed=values[next];save();}
        else if(!strcmp(operation,"captions")){p->captions=p->captions==1?-1:p->captions+1;save();}
        else if(!strcmp(operation,"leveling")){p->leveling=p->leveling==1?-1:p->leveling+1;save();}
        else if(!strcmp(operation,"reset") && p){int at=(int)(p-g_maxd_reliability.channels);memmove(p,p+1,(size_t)(--g_maxd_reliability.count-at)*sizeof *p);p=NULL;save();}
        else if(*operation && strcmp(operation,"reset")){cb(0,NULL,0,0,ud);return 1;}
        char key[64];
#define MAXD_CHANNEL_ROW(verb,title,help) do {snprintf(key,sizeof key,"%s:%s",channel,verb);MAXD_RELIABLE_ROW(key,title,help,"action");}while(0)
        if(p && p->height)snprintf(detail,sizeof detail,"Up to %dp; still respects the profile ceiling",p->height);else copy(detail,sizeof detail,"Inherit profile quality ceiling");
        MAXD_CHANNEL_ROW("quality","Quality ceiling",detail);
        if(p && p->speed)snprintf(detail,sizeof detail,"%.2gx for future videos",p->speed/100.0);else copy(detail,sizeof detail,"Use existing profile/video/channel speed memory");
        MAXD_CHANNEL_ROW("speed","Playback speed",detail);
        MAXD_CHANNEL_ROW("captions","Captions",!p||p->captions<0?"Inherit existing caption preferences":p->captions?"On for this channel":"Off for this channel");
        MAXD_CHANNEL_ROW("leveling","Volume leveling",!p||p->leveling<0?"Inherit profile; requires loudness metadata":p->leveling?"On when loudness metadata is available":"Off for this channel");
        MAXD_CHANNEL_ROW("reset","Reset channel overrides","Keep the existing profile settings and legacy caption/speed memories");
#undef MAXD_CHANNEL_ROW
    }
#undef MAXD_RELIABLE_ROW
    cb(1,out,n,0,ud);return 1;
}
