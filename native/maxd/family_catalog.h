/* Included after catalog/prompt helpers. GPL-3.0-or-later. */
static int maxd_family_prompt(const char *text,char *parent,size_t size) {
    if(!*g_maxd_family.prompt)return -1;
    int set=!strcmp(g_maxd_family.prompt,"pin"),ok=0;
    if(now_ms()<g_maxd_family.cooldown)return -1;
    if(set) {
        size_t n=strlen(text);unsigned char salt[8];
        if(n>=4 && n<=8 && strspn(text,"0123456789")==n && !tube_random_bytes(salt,sizeof salt))ok=tube_password_make(salt,text,g_maxd_family.pin);
    } else ok=tube_password_check(g_maxd_family.pin,text);
    if(!ok){if(++g_maxd_family.failures>=5){g_maxd_family.cooldown=now_ms()+30000;g_maxd_family.failures=0;}return -1;}
    g_maxd_family.failures=0;g_maxd_family.unlocked=now_ms()+180000;g_maxd_family.prompt[0]=0;
    g_playlist_prompt.active=0;save();snprintf(parent,size,"settings:family");return 0;
}
static int maxd_family_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent || strncmp(parent,"settings:family",15))return 0;
    evo_provider_item_t out[40];int n=0;if(page){cb(1,NULL,0,0,ud);return 1;}
    if(!maxd_family_parent()) {
        if(now_ms()<g_maxd_family.cooldown) {CATALOG_FOLDER("account:notice","Try the parent PIN again in 30 seconds","Too many unsuccessful attempts");}
        else {
            prompt_arm(*g_maxd_family.pin?"Parent PIN":"Create parent PIN (4 to 8 digits)");g_playlist_prompt.maxd_family=1;
            snprintf(g_maxd_family.prompt,sizeof g_maxd_family.prompt,"%s",*g_maxd_family.pin?"unlock":"pin");
            CATALOG_FOLDER("account:notice","Enter the parent PIN","Controls lock again after three minutes or when changing profile");
        }
        cb(1,out,n,0,ud);return 1;
    }
    MaxdFamily *f=maxd_family_for(g_library_profile,1);if(!f){cb(0,NULL,0,0,ud);return 1;}
    const char *action=parent+15;
    if(!strcmp(action,":enabled")){f->enabled=!f->enabled;save();}
    else if(!strcmp(action,":name")) {
        prompt_arm("Local family profile name");g_playlist_prompt.maxd_family=3;
        copy(g_playlist_prompt.prompt.initial,sizeof g_playlist_prompt.prompt.initial,f->name);
        CATALOG_FOLDER("account:notice","Name this local profile","Saved on this PS5; does not change the Google account name");cb(1,out,n,0,ud);return 1;
    }
    else if(!strcmp(action,":search")){f->search=!f->search;save();}
    else if(!strcmp(action,":time")){const int limits[]={0,30,60,90,120,180,240};int next=0;for(int i=0;i<7;++i)if(f->minutes==limits[i])next=(i+1)%7;f->minutes=limits[next];save();}
    else if(!strcmp(action,":approve")) {
        const Record *r=record(g_active_id,0);const char *channel=r?r->item.parent_id:"";
        if(!strncmp(channel,"channel:",8))channel+=8;
        if(!valid_channel(channel))channel=str(g_resolved_facts,"channel_id");
        int duplicate=0;for(int i=0;i<f->count;++i)duplicate|=!strcmp(f->channels[i],channel);
        if(valid_channel(channel) && !duplicate && f->count<MAXD_APPROVED_CHANNELS){copy(f->channels[f->count++],25,channel);save();}
    } else if(!strcmp(action,":channel")) {
        prompt_arm("Approved channel ID (UC followed by 22 characters)");g_playlist_prompt.maxd_family=2;
        CATALOG_FOLDER("account:notice","Enter a channel ID","Copy the channel ID from YouTube; names and @handles are not IDs");cb(1,out,n,0,ud);return 1;
    } else if(!strncmp(action,":remove:",8)) {
        const char *channel=action+8;
        for(int i=0;i<f->count;++i)if(!strcmp(f->channels[i],channel)){memmove(f->channels+i,f->channels+i+1,(size_t)(--f->count-i)*sizeof f->channels[0]);save();break;}
    } else if(!strcmp(action,":lock")) {g_maxd_family.unlocked=0;CATALOG_FOLDER("account:notice","Parental controls locked","Circle returns to browsing");cb(1,out,n,0,ud);return 1;}
    char text[200];snprintf(text,sizeof text,"Local controls for %s",g_library_profile);
    CATALOG_FOLDER("account:notice",text,"Separate from Google child accounts; settings belong to this app profile");
#define MAXD_FAMILY_ROW(id,title,hint,value) do {CATALOG_FOLDER(id,title,hint);maxd_smart_fact(id,"cycle",title,value);}while(0)
    MAXD_FAMILY_ROW("settings:family:enabled","Family restrictions",f->enabled?"Enabled: only approved channels can play":"Off for this profile",f->enabled?"On":"Off");
    MAXD_FAMILY_ROW("settings:family:search","Allow search","Playback remains limited to approved channels",f->search?"On":"Off");
    if(f->minutes)snprintf(text,sizeof text,"%d minutes per UTC day; used %.0f minutes",f->minutes,f->spent/60.0);
    if(!f->minutes)snprintf(text,sizeof text,"Unlimited; used %.0f minutes",f->spent/60.0);
    MAXD_FAMILY_ROW("settings:family:time","Daily viewing limit",text,text);
#undef MAXD_FAMILY_ROW
    CATALOG_FOLDER("settings:family:name","Profile name",*f->name?f->name:g_library_profile);maxd_smart_fact(out[n-1].id,"text","Profile name",f->name);
    CATALOG_FOLDER("settings:family:channel","Add approved channel ID","Up to 32 exact channel IDs; an empty list blocks all playback");maxd_smart_fact(out[n-1].id,"text","Add approved channel ID","");
    CATALOG_FOLDER("settings:family:approve","Approve current video's channel","Play an example before enabling restrictions, or enter a channel ID");
    for(int i=0;i<f->count;++i){char id[80];snprintf(id,sizeof id,"settings:family:remove:%s",f->channels[i]);CATALOG_FOLDER(id,f->channels[i],"Cross removes this approved channel");}
    CATALOG_FOLDER("settings:family:lock","Lock parental controls now","Automatically locks after three minutes");cb(1,out,n,0,ud);return 1;
}
static int maxd_family_gate(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!maxd_family_restricted(g_library_profile) || maxd_family_parent() || !parent || !strncmp(parent,"settings:family",15))return 0;
    if(!strncmp(parent,"settings",8) || !strncmp(parent,"app:",4) || !strncmp(parent,"account:",8) ||
       !strncmp(parent,"library:profile:",16) || !strcmp(parent,"library:profiles"))
        return maxd_family_catalog("settings:family",page,cb,ud);
    if(!maxd_family_search(g_library_profile) && (!strncmp(parent,"search:",7)||!strncmp(parent,"channel-search:",15))) {
        tube_set_error("Search is disabled by local family controls.");cb(0,NULL,0,0,ud);return 1;
    }
    return 0;
}
int tube_maxd_family_tick(long long now,int playing) {
    MaxdFamily *f=maxd_family_for(g_active_profile,0);long long delta=g_maxd_family.last_tick?now-g_maxd_family.last_tick:0;g_maxd_family.last_tick=now;
    if(!f || !f->enabled)return 0;
    int64_t day=(int64_t)time(NULL)/86400;if(day>f->day){f->day=day;f->spent=0;}
    if(playing && delta>0 && delta<10000)f->spent+=(double)delta/1000;
    if(playing && now-g_maxd_family.last_saved>=10000){g_maxd_family.last_saved=now;save();}
    return f->minutes && f->spent>=f->minutes*60;
}
