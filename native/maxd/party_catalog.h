/* Main-thread bridge between the LAN room and profile-owned local queue. GPL-3.0-or-later. */
static char g_maxd_party_profile[33];
static unsigned g_maxd_party_seen;
static void maxd_party_sync(void) {
    if(!maxd_party_running())return;
    if(strcmp(g_maxd_party_profile,g_library_profile) || maxd_family_restricted(g_library_profile)) {maxd_party_stop();return;}
    maxd_party_tick(now_ms());
    if(!maxd_party_running() || g_maxd_party_seen==maxd_party_revision())return;
    g_maxd_party_seen=maxd_party_revision();
    int ordered[MAXD_PARTY_ITEMS],count=0;
    for(int i=0;i<maxd_party_count();++i)if(maxd_party_item(i)->approved)ordered[count++]=i;
    for(int i=1;i<count;++i){int value=ordered[i],j=i;while(j>0 && maxd_party_item(ordered[j-1])->votes<maxd_party_item(value)->votes){ordered[j]=ordered[j-1];--j;}ordered[j]=value;}
    for(int i=0;i<count;++i) {
        const MaxdPartyItem *item=maxd_party_item(ordered[i]);if(!strcmp(item->id,g_active_id))continue;
        Record *r=record(item->id,1);if(!r || (item->enqueued && !r->queued))continue;
        if(!*r->item.title || !strcmp(r->item.title,item->id))copy(r->item.title,sizeof r->item.title,*item->title?item->title:"Party suggestion");
        r->queued=1;r->queued_music=0;r->queue_order=++g_queue_order;maxd_party_mark_queued(ordered[i]);
    }
    if(count)save();
}
static int maxd_party_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent || (strncmp(parent,"settings:party",14) && strcmp(parent,"link:maxd-party")))return 0;
    evo_provider_item_t out[72];int n=0;if(page){cb(1,NULL,0,0,ud);return 1;}
    if(maxd_family_restricted(g_library_profile)) {
        CATALOG_FOLDER("account:notice","Party rooms are disabled for restricted profiles","Switch to an adult profile using the parent PIN first");cb(1,out,n,0,ud);return 1;
    }
    if(!strcmp(parent,"settings:party:start")) {
        if(!*g_maxd_party_ip)maxd_party_discover(g_maxd_party_ip,sizeof g_maxd_party_ip);
        if(maxd_party_start(g_maxd_party_ip,8766,now_ms())) {copy(g_maxd_party_profile,sizeof g_maxd_party_profile,g_library_profile);g_maxd_party_seen=0;}
        else {tube_set_error("Could not start the LAN room. Set the PS5's private IPv4 address and retry.");cb(0,NULL,0,0,ud);return 1;}
    } else if(!strcmp(parent,"settings:party:stop"))maxd_party_stop();
    else if(!strcmp(parent,"settings:party:address")) {
        prompt_arm("PS5 LAN IPv4 address (example: 192.168.1.20)");g_playlist_prompt.maxd_party=1;
        copy(g_playlist_prompt.prompt.initial,sizeof g_playlist_prompt.prompt.initial,g_maxd_party_ip);
        CATALOG_FOLDER("account:notice","Enter this PS5's LAN address","Use Settings > Network > View Connection Status");cb(1,out,n,0,ud);return 1;
    } else if(!strncmp(parent,"settings:party:approve:",23) || !strncmp(parent,"settings:party:reject:",22)) {
        int approve=!strncmp(parent,"settings:party:approve:",23);const char *id=parent+(approve?23:22);
        for(int i=0;i<maxd_party_count();++i)if(!strcmp(maxd_party_item(i)->id,id)){maxd_party_approve(i,approve);break;}
        maxd_party_sync();
    } else if(!strncmp(parent,"settings:party:item:",20)) {
        const char *id=parent+20;const MaxdPartyItem *item=NULL;
        for(int i=0;i<maxd_party_count();++i)if(!strcmp(maxd_party_item(i)->id,id))item=maxd_party_item(i);
        if(!item || item->rejected){cb(0,NULL,0,0,ud);return 1;}
        char row[80],hint[200];snprintf(hint,sizeof hint,"Video %s | %u vote(s) | Guest-supplied label",id,item->votes);
        CATALOG_FOLDER("account:notice",*item->title?item->title:id,hint);
        snprintf(row,sizeof row,"settings:party:approve:%s",id);CATALOG_FOLDER(row,"Approve and add to play queue","Votes order upcoming party videos; the current video keeps playing");
        snprintf(row,sizeof row,"settings:party:reject:%s",id);CATALOG_FOLDER(row,"Reject suggestion","This room cannot suggest the same ID again");cb(1,out,n,0,ud);return 1;
    }
    if(!strcmp(parent,"link:maxd-party") && maxd_party_running()) {
        CATALOG_FOLDER("link:maxd-party","Join the party queue",maxd_party_join());copy(out[n-1].overview,sizeof out[n-1].overview,maxd_party_join());
        cb(1,out,n,0,ud);return 1;
    }
    CATALOG_FOLDER(maxd_party_running()?"settings:party:stop":"settings:party:start",maxd_party_running()?"Stop party room":"Start party room","Runs on this PS5; same local network; eight guests maximum");
    if(maxd_party_running()) {
        CATALOG_FOLDER("link:maxd-party","Show invite QR code",maxd_party_join());
        maxd_smart_fact(out[n-1].id,"link","Show invite QR code","");tube_row_fact(out[n-1].id,"argument","link:maxd-party");tube_row_fact(out[n-1].id,"screen","reader");
        CATALOG_FOLDER("account:notice","Invite lasts ten minutes; room lasts two hours","Stop and start a new room to rotate invites and revoke guests");
        for(int i=0;i<maxd_party_count();++i) {
            const MaxdPartyItem *item=maxd_party_item(i);if(item->rejected)continue;
            char id[80],hint[160];snprintf(id,sizeof id,"settings:party:item:%s",item->id);snprintf(hint,sizeof hint,"%u vote(s) | %s | %s",item->votes,item->approved?"Approved":"Needs host approval",item->id);
            CATALOG_FOLDER(id,*item->title?item->title:item->id,hint);maxd_smart_fact(id,"page",*item->title?item->title:item->id,"");
        }
    }
    CATALOG_FOLDER("settings:party:address","PS5 LAN address",*g_maxd_party_ip?g_maxd_party_ip:"Automatic discovery; set manually if needed");maxd_smart_fact(out[n-1].id,"text","PS5 LAN address",g_maxd_party_ip);
    cb(1,out,n,0,ud);return 1;
}
