/* Local smart-playlist rules and bounded JSON persistence. GPL-3.0-or-later. */
#ifndef MAXD_SMART_RULES_H
#define MAXD_SMART_RULES_H
#include "cJSON.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tube_search_query.h"
#define MAXD_SMART_RULES 64
#define MAXD_SMART_PER_PROFILE 16
typedef struct {
    uint32_t id;
    char profile[33], name[81], keyword[129], creator[129];
    int source, minimum, maximum, unwatched, hide_live, session_minutes;
} MaxdSmartRule;
typedef struct { MaxdSmartRule rules[MAXD_SMART_RULES]; int count; uint32_t serial; } MaxdSmartStore;
static inline int maxd_smart_text(const char *value, size_t limit, int empty) {
    if (!value || strlen(value)>limit || (!empty && !*value)) return 0;
    for (const unsigned char *p=(const unsigned char *)value;*p;++p) if (*p<32 || *p==127) return 0;
    return !*value ? empty : tube_search_query_valid(value);
}
static inline int maxd_smart_contains(const char *text,const char *part) {
    if (!*part) return 1;
    for (;*text;++text) {
        const unsigned char *a=(const unsigned char *)text,*b=(const unsigned char *)part;
        while (*a && *b) {
            unsigned ac=*a,bc=*b;
            if(ac>='A'&&ac<='Z')ac+=32;
            if(bc>='A'&&bc<='Z')bc+=32;
            if(ac!=bc)break;
            ++a;++b;
        }
        if (!*b) return 1;
    }
    return 0;
}
static inline int maxd_smart_matches(const MaxdSmartRule *r,const char *title,const char *creator,
                                    int64_t duration,int played,int live,int upcoming) {
    if (((r->unwatched || r->session_minutes) && played) || upcoming || (r->hide_live && live)) return 0;
    if ((r->minimum || r->maximum || r->session_minutes) && (duration<=0 || live)) return 0;
    return duration>=r->minimum && (!r->maximum || duration<=r->maximum) &&
        maxd_smart_contains(title,r->keyword) && maxd_smart_contains(creator,r->creator);
}
static inline MaxdSmartRule *maxd_smart_find(MaxdSmartStore *s,uint32_t id,const char *profile) {
    for(int i=0;i<s->count;++i)if(s->rules[i].id==id&&!strcmp(s->rules[i].profile,profile))return &s->rules[i];
    return NULL;
}
static inline int maxd_smart_count(const MaxdSmartStore *s,const char *profile) {
    int n=0;for(int i=0;i<s->count;++i)n+=!strcmp(s->rules[i].profile,profile);return n;
}
static inline MaxdSmartRule *maxd_smart_create(MaxdSmartStore *s,const char *profile,const char *name) {
    if(s->count>=MAXD_SMART_RULES || maxd_smart_count(s,profile)>=MAXD_SMART_PER_PROFILE ||
       s->serial==UINT32_MAX || !maxd_smart_text(name,80,0) || !maxd_smart_text(profile,32,0))return NULL;
    MaxdSmartRule *r=&s->rules[s->count++];memset(r,0,sizeof *r);r->id=++s->serial;
    snprintf(r->name,sizeof r->name,"%s",name);snprintf(r->profile,sizeof r->profile,"%s",profile);
    r->unwatched=r->hide_live=1;return r;
}
static inline void maxd_smart_write(const MaxdSmartStore *s,cJSON *root) {
    cJSON *rows=cJSON_AddArrayToObject(root,"maxd_smart_playlists");
    cJSON_AddNumberToObject(root,"maxd_smart_serial",s->serial);
    for(int i=0;i<s->count;++i) {
        const MaxdSmartRule *r=&s->rules[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddNumberToObject(o,"id",r->id);cJSON_AddStringToObject(o,"profile",r->profile);
        cJSON_AddStringToObject(o,"name",r->name);cJSON_AddStringToObject(o,"keyword",r->keyword);cJSON_AddStringToObject(o,"creator",r->creator);
        cJSON_AddNumberToObject(o,"session_minutes",r->session_minutes);cJSON_AddNumberToObject(o,"source",r->source);cJSON_AddNumberToObject(o,"minimum",r->minimum);cJSON_AddNumberToObject(o,"maximum",r->maximum);
        cJSON_AddBoolToObject(o,"unwatched",r->unwatched);cJSON_AddBoolToObject(o,"hide_live",r->hide_live);cJSON_AddItemToArray(rows,o);
    }
}
static inline const char *maxd_smart_string(const cJSON *o,const char *key) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";
}
static inline int64_t maxd_smart_integer(const cJSON *o,const char *key,int64_t max,int64_t fallback) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    return cJSON_IsNumber(v)&&isfinite(v->valuedouble)&&v->valuedouble>=0&&v->valuedouble<=max&&floor(v->valuedouble)==v->valuedouble?(int64_t)v->valuedouble:fallback;
}
static inline void maxd_smart_read(MaxdSmartStore *s,const cJSON *root) {
    memset(s,0,sizeof *s);s->serial=(uint32_t)maxd_smart_integer(root,"maxd_smart_serial",UINT32_MAX,0);
    const cJSON *rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_smart_playlists"),*o;
    if(!cJSON_IsArray(rows))return;
    cJSON_ArrayForEach(o,rows) {
        const char *profile=maxd_smart_string(o,"profile"),*name=maxd_smart_string(o,"name");
        const char *keyword=maxd_smart_string(o,"keyword"),*creator=maxd_smart_string(o,"creator");
        uint32_t id=(uint32_t)maxd_smart_integer(o,"id",UINT32_MAX,0);
        if(!id || s->count>=MAXD_SMART_RULES || maxd_smart_count(s,profile)>=MAXD_SMART_PER_PROFILE ||
           !maxd_smart_text(profile,32,0)||!maxd_smart_text(name,80,0)||!maxd_smart_text(keyword,128,1)||!maxd_smart_text(creator,128,1))continue;
        int duplicate=0;for(int i=0;i<s->count;++i)duplicate|=s->rules[i].id==id;if(duplicate)continue;
        MaxdSmartRule *r=&s->rules[s->count++];memset(r,0,sizeof *r);r->id=id;
        if(id>s->serial)s->serial=id;
        snprintf(r->profile,sizeof r->profile,"%s",profile);snprintf(r->name,sizeof r->name,"%s",name);
        snprintf(r->keyword,sizeof r->keyword,"%s",keyword);snprintf(r->creator,sizeof r->creator,"%s",creator);
        r->session_minutes=(int)maxd_smart_integer(o,"session_minutes",120,0);
        r->source=(int)maxd_smart_integer(o,"source",3,0);r->minimum=(int)maxd_smart_integer(o,"minimum",86400,0);r->maximum=(int)maxd_smart_integer(o,"maximum",86400,0);
        if(r->maximum && r->maximum<r->minimum)r->minimum=r->maximum=0;
        r->unwatched=!cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(o,"unwatched"));r->hide_live=!cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(o,"hide_live"));
    }
}
#endif
