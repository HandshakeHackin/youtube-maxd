/* Local, profile-owned feed preferences. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_FEED_CONTROLS_H
#define MAXD_FEED_CONTROLS_H
#include "smart_rules.h"
#define MAXD_FEED_PROFILES 128
#define MAXD_FEED_WORDS 16
typedef struct {
    char profile[33], muted[MAXD_FEED_WORDS][97];
    int count, followed_first;
} MaxdFeedPrefs;
typedef struct { MaxdFeedPrefs rows[MAXD_FEED_PROFILES]; int count; unsigned revision; } MaxdFeedStore;
static inline MaxdFeedPrefs *maxd_feed_for(MaxdFeedStore *s,const char *profile,int create) {
    for(int i=0;i<s->count;++i)if(!strcmp(s->rows[i].profile,profile))return &s->rows[i];
    if(!create || s->count>=MAXD_FEED_PROFILES || !maxd_smart_text(profile,32,0) ||
        strspn(profile,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=strlen(profile))return NULL;
    MaxdFeedPrefs *p=&s->rows[s->count++];memset(p,0,sizeof *p);
    snprintf(p->profile,sizeof p->profile,"%s",profile);return p;
}
/* Literal phrases, not regular expressions. ASCII case is ignored; UTF-8
 * phrases otherwise match exactly. No network feedback or account writes. */
static inline int maxd_feed_add(MaxdFeedPrefs *p,const char *text) {
    if(!p || !maxd_smart_text(text,96,0))return 0;
    int visible=0;for(const unsigned char *c=(const unsigned char *)text;*c;++c)visible|=*c!=' ';
    if(!visible)return 0;
    for(int i=0;i<p->count;++i)if(strlen(text)==strlen(p->muted[i]) && maxd_smart_contains(p->muted[i],text))return 1;
    if(p->count>=MAXD_FEED_WORDS)return 0;
    snprintf(p->muted[p->count++],97,"%s",text);return 1;
}
static inline int maxd_feed_hides(const MaxdFeedPrefs *p,const char *title,const char *alternate) {
    for(int i=0;p && i<p->count;++i)
        if(maxd_smart_contains(title,p->muted[i]) || (alternate && *alternate && maxd_smart_contains(alternate,p->muted[i])))return 1;
    return 0;
}
static inline void maxd_feed_write(const MaxdFeedStore *s,cJSON *root) {
    cJSON *rows=cJSON_AddArrayToObject(root,"maxd_feed_controls");
    for(int i=0;i<s->count;++i) {
        const MaxdFeedPrefs *p=&s->rows[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddStringToObject(o,"profile",p->profile);cJSON_AddBoolToObject(o,"followed_first",p->followed_first);
        cJSON *words=cJSON_AddArrayToObject(o,"muted");
        for(int j=0;j<p->count;++j)cJSON_AddItemToArray(words,cJSON_CreateString(p->muted[j]));
        cJSON_AddItemToArray(rows,o);
    }
}
static inline void maxd_feed_read(MaxdFeedStore *s,const cJSON *root) {
    memset(s,0,sizeof *s);const cJSON *o,*v;
    const cJSON *rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_feed_controls");
    if(!cJSON_IsArray(rows))return;
    cJSON_ArrayForEach(o,rows) {
        const cJSON *key=cJSON_GetObjectItemCaseSensitive(o,"profile");
        if(!cJSON_IsString(key) || maxd_feed_for(s,key->valuestring,0))continue;
        MaxdFeedPrefs *p=maxd_feed_for(s,key->valuestring,1);if(!p)continue;
        p->followed_first=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o,"followed_first"));
        const cJSON *words=cJSON_GetObjectItemCaseSensitive(o,"muted");
        if(cJSON_IsArray(words))cJSON_ArrayForEach(v,words)if(cJSON_IsString(v))maxd_feed_add(p,v->valuestring);
    }
}
#endif
