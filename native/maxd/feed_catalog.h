/* Included beside other native catalogs. SPDX-License-Identifier: GPL-3.0-or-later */
static void maxd_feed_changed(void) {
    ++g_maxd_feed.revision;catalog_retire();save();
}
static int maxd_feed_prompt(const char *text,char *parent,size_t size) {
    if(!maxd_feed_add(maxd_feed_for(&g_maxd_feed,g_library_profile,1),text))return -1;
    g_playlist_prompt.active=0;maxd_feed_changed();snprintf(parent,size,"settings:feed");return 0;
}
static int maxd_feed_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent || (strcmp(parent,"settings:feed") && strncmp(parent,"settings:feed:",14)))return 0;
    evo_provider_item_t out[32];int n=0;
    MaxdFeedPrefs *p=maxd_feed_for(&g_maxd_feed,g_library_profile,1);
    if(!p || page){cb(p!=NULL,NULL,0,0,ud);return 1;}
    if(!strcmp(parent,"settings:feed:add")) {
        prompt_arm("Mute a word or phrase in video titles");g_playlist_prompt.maxd_feed=1;
        CATALOG_FOLDER("account:notice","Enter a title phrase","Local to this profile; up to 16 phrases, 96 UTF-8 bytes each");
        cb(1,out,n,0,ud);return 1;
    }
    if(strcmp(parent,"settings:feed")) {
        int matched=0;
        char id[128];
        snprintf(id,sizeof id,"settings:feed:priority:%u:%u",g_playlist_scope,g_maxd_feed.revision);
        if(!strcmp(parent,id)){p->followed_first=!p->followed_first;matched=1;}
        for(int i=0;!matched && i<p->count;++i) {
            snprintf(id,sizeof id,"settings:feed:remove:%u:%u:%d",g_playlist_scope,g_maxd_feed.revision,i);
            if(strcmp(parent,id))continue;
            memmove(p->muted[i],p->muted[i+1],(size_t)(p->count-i-1)*sizeof p->muted[0]);
            memset(p->muted[--p->count],0,sizeof p->muted[0]);matched=1;
        }
        if(!matched){tube_set_error("Reopen Feed controls for this profile before changing a rule.");cb(0,NULL,0,0,ud);return 1;}
        maxd_feed_changed();
    }
    char id[128];snprintf(id,sizeof id,"settings:feed:priority:%u:%u",g_playlist_scope,g_maxd_feed.revision);
    CATALOG_FOLDER(id,p->followed_first?"Followed channels first: on":"Followed channels first: off",
        "Prioritize local follows within each Home/Discover page; remote subscriptions and search order stay unchanged");
    CATALOG_FOLDER("settings:feed:add","Mute a title phrase","Hide matching original or DeArrow titles in browsing feeds; saved history, favorites and queues stay available");
    CATALOG_FOLDER("blocked-channels","Blocked channels","Manage this profile's existing channel blocks; use a video's actions menu to block its channel");
    for(int i=0;i<p->count;++i) {
        snprintf(id,sizeof id,"settings:feed:remove:%u:%u:%d",g_playlist_scope,g_maxd_feed.revision,i);
        CATALOG_FOLDER(id,p->muted[i],"Cross removes this phrase; reopen a feed to apply changes");
    }
    cb(1,out,n,0,ud);return 1;
}
