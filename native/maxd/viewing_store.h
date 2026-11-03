/* Saved playback slots belong to the actual profile, even with shared settings.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_VIEWING_STORE_H
#define MAXD_VIEWING_STORE_H
#include "smart_rules.h"
#define MAXD_PRESET_ROWS 384
typedef struct {
    char profile[33], language[16], audio[16];
    int slot, height, fps, codec, speed, captions, automatic, adaptive;
} MaxdPlaybackPreset;
typedef struct { MaxdPlaybackPreset rows[MAXD_PRESET_ROWS]; int count; } MaxdPresetStore;
static inline MaxdPlaybackPreset *maxd_preset_find(MaxdPresetStore *s,const char *profile,int slot,int create) {
    for(int i=0;i<s->count;++i)if(s->rows[i].slot==slot&&!strcmp(s->rows[i].profile,profile))return &s->rows[i];
    if(!create || slot<0 || slot>2 || !maxd_smart_text(profile,32,0) || s->count>=MAXD_PRESET_ROWS)return NULL;
    MaxdPlaybackPreset *p=&s->rows[s->count++];memset(p,0,sizeof *p);
    snprintf(p->profile,sizeof p->profile,"%s",profile);p->slot=slot;return p;
}
static inline void maxd_presets_forget(MaxdPresetStore *s,const char *profile) {
    for(int i=s->count-1;i>=0;--i)if(!strcmp(s->rows[i].profile,profile)) {
        memmove(s->rows+i,s->rows+i+1,(size_t)(s->count-i-1)*sizeof s->rows[0]);--s->count;
    }
}
static inline void maxd_presets_write(const MaxdPresetStore *s,cJSON *root) {
    cJSON *rows=cJSON_AddArrayToObject(root,"maxd_playback_presets");
    for(int i=0;i<s->count;++i) {
        const MaxdPlaybackPreset *p=&s->rows[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddStringToObject(o,"profile",p->profile);cJSON_AddStringToObject(o,"language",p->language);cJSON_AddStringToObject(o,"audio",p->audio);
#define MAXD_PRESET_WRITE(field) cJSON_AddNumberToObject(o,#field,p->field)
        MAXD_PRESET_WRITE(slot);MAXD_PRESET_WRITE(height);MAXD_PRESET_WRITE(fps);MAXD_PRESET_WRITE(codec);MAXD_PRESET_WRITE(speed);
        MAXD_PRESET_WRITE(captions);MAXD_PRESET_WRITE(automatic);MAXD_PRESET_WRITE(adaptive);
#undef MAXD_PRESET_WRITE
        cJSON_AddItemToArray(rows,o);
    }
}
static inline void maxd_presets_read(MaxdPresetStore *s,const cJSON *root) {
    memset(s,0,sizeof *s);const cJSON *o,*rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_playback_presets");
    if(!cJSON_IsArray(rows))return;
    cJSON_ArrayForEach(o,rows) {
        const char *owner=maxd_smart_string(o,"profile"),*language=maxd_smart_string(o,"language"),*audio=maxd_smart_string(o,"audio");
        int slot=(int)maxd_smart_integer(o,"slot",2,-1);
        int height=(int)maxd_smart_integer(o,"height",2160,-1),fps=(int)maxd_smart_integer(o,"fps",60,-1);
        int codec=(int)maxd_smart_integer(o,"codec",4,-1),speed=(int)maxd_smart_integer(o,"speed",1000,-1);
        const int heights[]={144,240,360,480,720,1080,1440,2160};int valid_height=0;
        for(int i=0;i<8;++i)valid_height|=height==heights[i];
        if(slot<0 || !valid_height || (fps!=0&&fps!=30&&fps!=60) || codec<0 || speed<1 ||
           !maxd_smart_text(owner,32,0) || !maxd_smart_text(language,15,0) || !maxd_smart_text(audio,15,1) ||
           maxd_preset_find(s,owner,slot,0))continue;
        MaxdPlaybackPreset *p=maxd_preset_find(s,owner,slot,1);if(!p)continue;
        p->height=height;p->fps=fps;p->codec=codec;p->speed=speed;
        p->captions=(int)maxd_smart_integer(o,"captions",1,1);
        p->automatic=(int)maxd_smart_integer(o,"automatic",1,0);p->adaptive=(int)maxd_smart_integer(o,"adaptive",1,0);
        snprintf(p->language,sizeof p->language,"%s",language);snprintf(p->audio,sizeof p->audio,"%s",audio);
    }
}
#endif
