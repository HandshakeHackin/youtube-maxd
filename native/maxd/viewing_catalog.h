/* Everyday viewing controls in the existing provider. UI thread only.
 * SPDX-License-Identifier: GPL-3.0-or-later */
static int maxd_viewing_owner(const char *parent,const char *base,const char **action) {
    size_t length=strlen(base);if(strncmp(parent,base,length) || parent[length]!=':')return 0;
    const char *owner=parent+length+1,*end=strchr(owner,':');
    if(!end || (size_t)(end-owner)!=strlen(g_library_profile) || strncmp(owner,g_library_profile,(size_t)(end-owner)))return 0;
    *action=end+1;return 1;
}
static int maxd_viewing_catalog(const char *parent,int page,evo_provider_items_cb cb,void *ud) {
    if(!parent)return 0;
    const int presets=!strncmp(parent,"settings:presets",strlen("settings:presets"));
    const int access=!strncmp(parent,"settings:accessibility",strlen("settings:accessibility"));
    const int rest=!strncmp(parent,"settings:rest",strlen("settings:rest"));
    if(!presets && !access && !rest)return 0;
    evo_provider_item_t out[24];int n=0;const char *action="";
    const char *base=presets?"settings:presets":access?"settings:accessibility":"settings:rest";
    if(page) {cb(1,NULL,0,0,ud);return 1;}
    if(strcmp(parent,base) && !maxd_viewing_owner(parent,base,&action)) {
        tube_set_error("Reopen these controls for the current profile.");cb(0,NULL,0,0,ud);return 1;
    }
    char id[120],detail[180];
#define MAXD_VIEW_ROW(key,title,help,kind) do { \
    snprintf(id,sizeof id,"%s:%s:%s",base,g_library_profile,key); \
    CATALOG_FOLDER(id,title,help);maxd_smart_fact(id,kind,title,""); \
} while(0)
    if(presets) {
        static const char *names[]={"Movies","Gaming","Listening"};
        if(!*action) {
            for(int slot=0;slot<3;++slot) {
                char key[16];snprintf(key,sizeof key,"%d",slot);
                MaxdPlaybackPreset *p=maxd_preset_find(&g_maxd_presets,g_library_profile,slot,0);
                snprintf(detail,sizeof detail,p?"Saved: up to %dp, %s fps; %.2gx speed":"Default: up to %dp, %s fps; %.2gx speed",
                    p?p->height:slot==0?2160:slot==1?1080:720,p?(p->fps==60?"60":p->fps==30?"30":"any"):slot==1?"60":"30",p?p->speed/100.0:1.0);
                MAXD_VIEW_ROW(key,names[slot],detail,"page");
            }
        } else {
            int slot=action[0]-'0';const char *operation=action+1;
            if(slot<0 || slot>2 || (*operation && *operation!=':')) {cb(0,NULL,0,0,ud);return 1;}
            if(*operation)++operation;
            if(!strcmp(operation,"save")) {
                MaxdPlaybackPreset *p=maxd_preset_find(&g_maxd_presets,g_library_profile,slot,1);
                if(!p){tube_set_error("Preset storage is full.");cb(0,NULL,0,0,ud);return 1;}
                p->height=g_height;p->fps=g_max_fps;p->codec=g_codec;p->speed=g_speed;
                p->captions=g_captions;p->automatic=g_auto_captions;p->adaptive=g_auto_max;
                copy(p->language,sizeof p->language,g_language);copy(p->audio,sizeof p->audio,g_audio_language);
                save();
            } else if(!strcmp(operation,"apply")) {
                MaxdPlaybackPreset *p=maxd_preset_find(&g_maxd_presets,g_library_profile,slot,0);
                if(p && (!valid_speed(p->speed) || language_index(p->language)<0 || (*p->audio && language_index(p->audio)<0))) {
                    tube_set_error("This saved preset is invalid. Save it again.");cb(0,NULL,0,0,ud);return 1;
                }
                g_height=p?p->height:slot==0?2160:slot==1?1080:720;g_max_fps=p?p->fps:slot==1?60:30;
                g_codec=p?p->codec:0;g_speed=p?p->speed:100;g_last_speed=g_speed;
                if(p) {g_captions=p->captions;g_auto_captions=p->automatic;g_auto_max=p->adaptive;
                    copy(g_language,sizeof g_language,p->language);copy(g_audio_language,sizeof g_audio_language,p->audio);}
                save();
            } else if(!strcmp(operation,"reset")) {
                MaxdPlaybackPreset *p=maxd_preset_find(&g_maxd_presets,g_library_profile,slot,0);
                if(p){int at=(int)(p-g_maxd_presets.rows);memmove(p,p+1,(size_t)(--g_maxd_presets.count-at)*sizeof *p);save();}
            } else if(*operation) {cb(0,NULL,0,0,ud);return 1;}
            char key[24];
            snprintf(key,sizeof key,"%d:apply",slot);MAXD_VIEW_ROW(key,"Use this preset","Applies to the next resolved video; current playback stays as it is","action");
            snprintf(key,sizeof key,"%d:save",slot);MAXD_VIEW_ROW(key,"Save current preferences","Save quality, FPS, codec, speed, audio/caption languages and Auto Max to this slot","action");
            snprintf(key,sizeof key,"%d:reset",slot);MAXD_VIEW_ROW(key,"Reset this slot","Restore the built-in Movies, Gaming or Listening defaults","action");
        }
    } else if(access) {
        if(!strcmp(action,"scale")) {g_ui.ui_scale=g_ui.ui_scale<115?115:g_ui.ui_scale<120?120:100;++g_ui_revision;save();}
        else if(!strcmp(action,"contrast")) {g_ui.scheme=g_ui.scheme==3?0:3;++g_ui_revision;save();}
        else if(!strcmp(action,"motion")) {g_ui.reduced_motion=!g_ui.reduced_motion;++g_ui_revision;save();}
        else if(!strcmp(action,"caption-background")) {
            int current=tube_extra_subtitle_style_index();
            if(!tube_extra_set_subtitle_style(current==1?2:current==2?3:1)) {cb(0,NULL,0,0,ud);return 1;}
        } else if(*action) {cb(0,NULL,0,0,ud);return 1;}
        snprintf(detail,sizeof detail,"%d%%; cycles 100, 115, 120%%",g_ui.ui_scale);MAXD_VIEW_ROW("scale","Larger interface text",detail,"action");
        MAXD_VIEW_ROW("contrast","High contrast",g_ui.scheme==3?"On: yellow accent on black":"Off: select to use the high-contrast palette","action");
        MAXD_VIEW_ROW("motion","Reduced motion",g_ui.reduced_motion?"On: decorative travel is reduced":"Off: select to reduce browser motion","action");
        int style=tube_extra_subtitle_style_index();
        snprintf(detail,sizeof detail,"%s; select to cycle white captions",style==1?"Transparent":style==2?"Translucent black":style==3?"Opaque black":"Current player style");
        MAXD_VIEW_ROW("caption-background","Caption background",detail,"action");
        CATALOG_FOLDER("settings:subtitle-size","Caption text size","Choose the player caption size");maxd_smart_fact("settings:subtitle-size","page","Caption text size","");
    } else {
        if(!strcmp(action,"end")) {
            if(!tube_sleep_schedule(0,1)){tube_set_error("Start a video before choosing end-of-video sleep.");cb(0,NULL,0,0,ud);return 1;}
        } else if(!strcmp(action,"off")) tube_sleep_schedule(0,0);
        else if(*action) {
            const int minutes[]={15,30,45,60,90,120};int chosen=0;
            for(int i=0;i<6;++i){char key[16];snprintf(key,sizeof key,"%d",minutes[i]);if(!strcmp(action,key))chosen=minutes[i];}
            if(!chosen || !tube_sleep_schedule(chosen,0)){cb(0,NULL,0,0,ud);return 1;}
        }
        if(tube_sleep_after_video())copy(detail,sizeof detail,"Pauses when this video ends");
        else if(tube_sleep_scheduled_minutes())snprintf(detail,sizeof detail,"%d min remaining",tube_sleep_scheduled_minutes());
        else if(tube_sleep_minutes())snprintf(detail,sizeof detail,"Inactivity timer: %d minutes",tube_sleep_minutes());
        else copy(detail,sizeof detail,"Off");
        CATALOG_FOLDER("account:notice","Sleep timer status",detail);
        MAXD_VIEW_ROW("off","Turn sleep timer off","Cancels the session timer and inactivity timer","action");
        MAXD_VIEW_ROW("end","When this video ends","Pause once instead of repeat, autoplay or the next queued video","action");
        const int minutes[]={15,30,45,60,90,120};
        for(int i=0;i<6;++i){char key[16],title[64];snprintf(key,sizeof key,"%d",minutes[i]);snprintf(title,sizeof title,"Pause in %d minutes",minutes[i]);MAXD_VIEW_ROW(key,title,"Elapsed time; input does not restart the timer","action");}
        CATALOG_FOLDER("settings:sleep-timer","Inactivity timer","Existing input-reset timer; choosing it cancels the session timer");
    }
#undef MAXD_VIEW_ROW
    if(g_storage_notice){tube_set_error("Preferences changed for this session, but could not be saved. Check free space.");cb(0,NULL,0,0,ud);return 1;}
    cb(1,out,n,0,ud);return 1;
}
