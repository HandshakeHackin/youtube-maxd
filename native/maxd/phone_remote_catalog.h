/* Main-thread, profile-owned remote and report settings. GPL-3.0-or-later. */
static char g_maxd_remote_profile[33],g_maxd_remote_source[512],g_maxd_remote_ip[32];
void tube_maxd_remote_address(const char *address) {copy(g_maxd_remote_ip,sizeof g_maxd_remote_ip,address);}
int tube_maxd_remote_scope(void) {
    if(maxd_remote_running() && (strcmp(g_maxd_remote_profile,g_library_profile) ||
       strcmp(g_maxd_remote_source,g_source) || maxd_family_restricted(g_library_profile))) {
        maxd_remote_stop();maxd_report_clear(evo_data_path("bug-report.json"));
    }
    return maxd_remote_running();
}
void tube_maxd_remote_queue_state(MaxdRemoteState *state) {
    if(!state || !tube_maxd_remote_scope())return;
    Record *ordered[RECORDS];int count=queue_compact(ordered);state->count=0;
    for(int i=0;i<count && state->count<MAXD_REMOTE_QUEUE;++i) {
        Record *r=ordered[i];if(!current_record(r) || !r->queued)continue;
        MaxdRemoteItem *item=&state->queue[state->count++];copy(item->id,sizeof item->id,r->item.id);
        copy(item->title,sizeof item->title,*r->item.title?r->item.title:r->item.id);
    }
}
int tube_maxd_remote_queue(int action,const char *id) {
    if(!tube_maxd_remote_scope() || !valid_id(id))return 0;
    Record *r=record(id,0);
    if(action==MAXD_REMOTE_REMOVE) {
        if(r && r->queued){r->queued=0;r->queued_music=0;r->queue_order=0;tube_playlist_eof_cancel();tube_queue_retry_cancel();save();}
        return 1;
    }
    if(action!=MAXD_REMOTE_ADD && action!=MAXD_REMOTE_NEXT)return 0;
    if(!r || !r->queued) {
        int count=0;for(int i=0;i<g_count;++i)if(current_record(&g_records[i]) && g_records[i].queued)++count;
        if(count>=MAXD_REMOTE_QUEUE)return 0;
        r=record(id,1);if(!r)return 0;
        r->queued=1;r->queued_music=0;r->queue_order=++g_queue_order;
        if(!*r->item.title)copy(r->item.title,sizeof r->item.title,id);
        save();
    }
    return action==MAXD_REMOTE_NEXT?tube_queue_play_next(id)>0:1;
}
static int maxd_remote_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent || (strncmp(parent,"settings:phone-remote",21) && strncmp(parent,"settings:bug-report",19) && strcmp(parent,"link:maxd-remote")))return 0;
    evo_provider_item_t out[32];int n=0;if(page){cb(1,NULL,0,0,ud);return 1;}
    tube_maxd_remote_scope();
    if(maxd_family_restricted(g_library_profile)) {
        CATALOG_FOLDER("account:notice","Use an adult profile for phone control and reports","Switch profiles using the parent PIN first");cb(1,out,n,0,ud);return 1;
    }
    if(!strncmp(parent,"settings:bug-report",19)) {
        if(!strcmp(parent,"settings:bug-report:capture")) {
            if(!maxd_report_capture(evo_data_path("bug-report.json"))){tube_set_error("Could not save the report. Check app data storage.");cb(0,NULL,0,0,ud);return 1;}
        } else if(!strcmp(parent,"settings:bug-report:clear")) {
            if(!maxd_report_clear(evo_data_path("bug-report.json"))){tube_set_error("Could not delete the saved report.");cb(0,NULL,0,0,ud);return 1;}
        } else if(!strcmp(parent,"settings:bug-report:preview")) {
            cJSON *json=cJSON_Parse(maxd_report_json()),*field;
            if(!json)CATALOG_FOLDER("account:notice","No report captured","Capture a report first.");
            else cJSON_ArrayForEach(field,json) {
                char id[64];snprintf(id,sizeof id,"link:row:maxd-report:%d",n);
                char *value=cJSON_PrintUnformatted(field);if(!value)continue;
                CATALOG_FOLDER(id,field->string,value);copy(out[n-1].overview,sizeof out[n-1].overview,value);free(value);
            }
            cJSON_Delete(json);cb(1,out,n,0,ud);return 1;
        }
        CATALOG_FOLDER("settings:bug-report:capture","Capture bug report","Overwrites /data/youtube-maxd/bug-report.json with technical details only");
        CATALOG_FOLDER("settings:bug-report:preview","Review captured report",*maxd_report_json()?"Ready; capture again to refresh":"No report captured in this session");
        maxd_smart_fact(out[n-1].id,"link","Review captured report","");tube_row_fact(out[n-1].id,"screen","reader");tube_row_fact(out[n-1].id,"argument","settings:bug-report:preview");
        CATALOG_FOLDER("settings:phone-remote","Download on your phone","Enable Phone remote, scan its QR, then Capture & preview report");
        CATALOG_FOLDER("settings:bug-report:clear","Delete captured report","Nothing is uploaded automatically");
        CATALOG_FOLDER("account:notice","Privacy: technical fields only","No accounts, tokens, IP addresses, video IDs, titles, searches, URLs or raw logs");
        cb(1,out,n,0,ud);return 1;
    }
    if(!strcmp(parent,"settings:phone-remote:start")) {
        if(!*g_maxd_remote_ip)maxd_party_discover(g_maxd_remote_ip,sizeof g_maxd_remote_ip);
        if(!maxd_remote_start(g_maxd_remote_ip,8767,now_ms())) {tube_set_error("Could not start Phone remote. Set this PS5's private IPv4 address and retry.");cb(0,NULL,0,0,ud);return 1;}
        copy(g_maxd_remote_profile,sizeof g_maxd_remote_profile,g_library_profile);copy(g_maxd_remote_source,sizeof g_maxd_remote_source,g_source);
    } else if(!strcmp(parent,"settings:phone-remote:stop"))maxd_remote_stop();
    else if(!strcmp(parent,"settings:phone-remote:address")) {
        prompt_arm("PS5 LAN IPv4 address (example: 192.168.1.20)");g_playlist_prompt.maxd_party=2;
        copy(g_playlist_prompt.prompt.initial,sizeof g_playlist_prompt.prompt.initial,g_maxd_remote_ip);
        CATALOG_FOLDER("account:notice","Enter this PS5's LAN address","Use Settings > Network > View Connection Status");cb(1,out,n,0,ud);return 1;
    }
    if(!strcmp(parent,"link:maxd-remote")) {
        if(maxd_remote_running() && !maxd_remote_paired()) {
            CATALOG_FOLDER("link:maxd-remote","Pair your phone",maxd_remote_join());copy(out[n-1].overview,sizeof out[n-1].overview,maxd_remote_join());
        } else CATALOG_FOLDER("account:notice","Restart Phone remote for a new QR","The QR is single-use and expires in five minutes");
        cb(1,out,n,0,ud);return 1;
    }
    CATALOG_FOLDER(maxd_remote_running()?"settings:phone-remote:stop":"settings:phone-remote:start",maxd_remote_running()?"Stop Phone remote":"Start Phone remote","Off by default; one trusted phone; same local network; no PC needed");
    if(maxd_remote_running()) {
        CATALOG_FOLDER("link:maxd-remote",maxd_remote_paired()?"Phone paired":"Show pairing QR code","One-use QR: five minutes; paired session: two hours; restart to revoke");
        maxd_smart_fact(out[n-1].id,"link","Pair your phone","");tube_row_fact(out[n-1].id,"argument","link:maxd-remote");tube_row_fact(out[n-1].id,"screen","reader");
    }
    CATALOG_FOLDER("settings:phone-remote:address","PS5 LAN address",*g_maxd_remote_ip?g_maxd_remote_ip:"Automatic discovery; set manually if needed");maxd_smart_fact(out[n-1].id,"text","PS5 LAN address",g_maxd_remote_ip);
    CATALOG_FOLDER("settings:bug-report","Bug-report export","Capture, review and download a report from your paired phone");
    CATALOG_FOLDER("account:notice","Use your trusted home network","Local HTTP; control ends on profile/source change, app exit, or after two hours");
    cb(1,out,n,0,ud);return 1;
}
