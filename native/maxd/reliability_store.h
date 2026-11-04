/* Channel overrides and interruption checkpoints. No account credentials.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_RELIABILITY_STORE_H
#define MAXD_RELIABILITY_STORE_H
#include "smart_rules.h"
#define MAXD_CHANNEL_PREFS 256
#define MAXD_CHECKPOINTS 128
typedef struct { char profile[33],channel[25]; int height,speed,captions,leveling; } MaxdChannelPref;
typedef struct { char profile[33],video[12]; int64_t position,duration; int active,pending; } MaxdCheckpoint;
typedef struct { MaxdChannelPref channels[MAXD_CHANNEL_PREFS]; int count; MaxdCheckpoint sessions[MAXD_CHECKPOINTS]; int sessions_count; } MaxdReliabilityStore;
static inline int maxd_reliability_id(const char *s,size_t length) {
    if(!s || strlen(s)!=length)return 0;
    for(size_t i=0;i<length;++i){unsigned c=(unsigned char)s[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;}
    return 1;
}
static inline MaxdChannelPref *maxd_channel_pref(MaxdReliabilityStore *s,const char *profile,const char *channel,int create) {
    if(!profile || !channel)return NULL;
    for(int i=0;i<s->count;++i)if(!strcmp(s->channels[i].profile,profile)&&!strcmp(s->channels[i].channel,channel))return &s->channels[i];
    if(!create || s->count>=MAXD_CHANNEL_PREFS || !maxd_smart_text(profile,32,0) || !maxd_reliability_id(channel,24) || strncmp(channel,"UC",2))return NULL;
    MaxdChannelPref *p=&s->channels[s->count++];memset(p,0,sizeof *p);p->captions=p->leveling=-1;
    snprintf(p->profile,sizeof p->profile,"%s",profile);snprintf(p->channel,sizeof p->channel,"%s",channel);return p;
}
static inline MaxdCheckpoint *maxd_checkpoint(MaxdReliabilityStore *s,const char *profile,int create) {
    for(int i=0;i<s->sessions_count;++i)if(!strcmp(s->sessions[i].profile,profile))return &s->sessions[i];
    if(!create || s->sessions_count>=MAXD_CHECKPOINTS || !maxd_smart_text(profile,32,0))return NULL;
    MaxdCheckpoint *p=&s->sessions[s->sessions_count++];memset(p,0,sizeof *p);snprintf(p->profile,sizeof p->profile,"%s",profile);return p;
}
static inline void maxd_reliability_forget(MaxdReliabilityStore *s,const char *profile) {
    for(int i=s->count-1;i>=0;--i)if(!strcmp(s->channels[i].profile,profile)){memmove(s->channels+i,s->channels+i+1,(size_t)(s->count-i-1)*sizeof s->channels[0]);--s->count;}
    for(int i=s->sessions_count-1;i>=0;--i)if(!strcmp(s->sessions[i].profile,profile)){memmove(s->sessions+i,s->sessions+i+1,(size_t)(s->sessions_count-i-1)*sizeof s->sessions[0]);--s->sessions_count;}
}
static inline void maxd_reliability_write(const MaxdReliabilityStore *s,cJSON *root) {
    cJSON *channels=cJSON_AddArrayToObject(root,"maxd_channel_preferences"),*sessions=cJSON_AddArrayToObject(root,"maxd_checkpoints");
    for(int i=0;i<s->count;++i){const MaxdChannelPref *p=&s->channels[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddStringToObject(o,"profile",p->profile);cJSON_AddStringToObject(o,"channel",p->channel);
        cJSON_AddNumberToObject(o,"height",p->height);cJSON_AddNumberToObject(o,"speed",p->speed);cJSON_AddNumberToObject(o,"captions",p->captions);cJSON_AddNumberToObject(o,"leveling",p->leveling);cJSON_AddItemToArray(channels,o);}
    for(int i=0;i<s->sessions_count;++i){const MaxdCheckpoint *p=&s->sessions[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddStringToObject(o,"profile",p->profile);cJSON_AddStringToObject(o,"video",p->video);cJSON_AddNumberToObject(o,"position",(double)p->position);
        cJSON_AddNumberToObject(o,"duration",(double)p->duration);cJSON_AddBoolToObject(o,"active",p->active);cJSON_AddItemToArray(sessions,o);}
}
static inline int maxd_override(const cJSON *o,const char *key) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsNumber(v) && (v->valuedouble==0 || v->valuedouble==1)?v->valueint:-1;
}
static inline void maxd_reliability_read(MaxdReliabilityStore *s,const cJSON *root) {
    memset(s,0,sizeof *s);const cJSON *o,*rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_channel_preferences");
    if(cJSON_IsArray(rows))cJSON_ArrayForEach(o,rows){const char *owner=maxd_smart_string(o,"profile"),*channel=maxd_smart_string(o,"channel");
        if(maxd_channel_pref(s,owner,channel,0))continue;
        MaxdChannelPref *p=maxd_channel_pref(s,owner,channel,1);if(!p)continue;
        int height=(int)maxd_smart_integer(o,"height",2160,0);const int heights[]={144,240,360,480,720,1080,1440,2160};
        for(int j=0;j<8;++j)if(height==heights[j])p->height=height;
        p->speed=(int)maxd_smart_integer(o,"speed",400,0);if(p->speed && p->speed!=50 && p->speed!=75 && p->speed!=100 && p->speed!=125 && p->speed!=150 && p->speed!=200)p->speed=0;
        p->captions=maxd_override(o,"captions");p->leveling=maxd_override(o,"leveling");}
    rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_checkpoints");
    if(cJSON_IsArray(rows))cJSON_ArrayForEach(o,rows){const char *owner=maxd_smart_string(o,"profile"),*video=maxd_smart_string(o,"video");
        if(!maxd_reliability_id(video,11) || maxd_checkpoint(s,owner,0))continue;
        int64_t position=maxd_smart_integer(o,"position",1000000,-1),duration=maxd_smart_integer(o,"duration",1000000,-1);
        if(position<0 || duration<1 || position>=duration)continue;
        MaxdCheckpoint *p=maxd_checkpoint(s,owner,1);if(!p)continue;
        snprintf(p->video,sizeof p->video,"%s",video);p->position=position;p->duration=duration;p->active=p->pending=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"active"));}
}
#endif
