/* YouTube Max'd local experience data. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_EXPERIENCE_STORE_H
#define MAXD_EXPERIENCE_STORE_H
#include "smart_rules.h"
#define MAXD_BOOKMARKS 512
#define MAXD_CUES 4096
typedef struct { char profile[33],video[12],label[121]; double seconds; } MaxdBookmark;
typedef struct { double seconds; char text[321]; } MaxdCue;
static MaxdBookmark g_maxd_marks[MAXD_BOOKMARKS];
static int g_maxd_mark_count;
static struct { MaxdCue *cues; int count,state; unsigned serial,selection; long long deadline; char profile[33],video[12]; } g_maxd_transcript;
/* Accept WebVTT and SubRip; reject non-finite or malformed clocks. */
static inline double maxd_clock(const char *s) {
    const char *first=strchr(s,':');if(!first || first-s<1 || first-s>3)return -1;
    const char *second=strchr(first+1,':');
    if(second && (second-first!=3 || (size_t)(second-s)>6))return -1;
    const char *tail=second?second+1:first+1;
    size_t seconds=strcspn(tail," \t");if(seconds<2 || seconds>6 || strlen(s)>32)return -1;
    for(const char *p=s;*p && *p!=' ' && *p!='\t';++p)
        if((*p<'0'||*p>'9') && *p!=':' && *p!='.')return -1;
    unsigned h=0,m=0;double sec=0;int used=0;
    if(sscanf(s,"%u:%u:%lf%n",&h,&m,&sec,&used)!=3) {
        h=0;if(sscanf(s,"%u:%lf%n",&m,&sec,&used)!=2)return -1;
    }
    if(m>=60 || !isfinite(sec) || sec<0 || sec>=60 || h>167)return -1;
    const char *p=s+used;if(*p && *p!=' ' && *p!='\t')return -1;
    return h*3600.0+m*60.0+sec;
}
static inline void maxd_cue_text(char *out,size_t capacity,const char *s) {
    size_t n=strlen(out);int tag=0;
    if(n && n+1<capacity)out[n++]=' ';
    for(;*s && n+1<capacity;++s) {
        if(*s=='<'){tag=1;continue;}if(*s=='>'){tag=0;continue;}if(tag)continue;
        if(!strncmp(s,"&amp;",5)){out[n++]='&';s+=4;}
        else if(!strncmp(s,"&lt;",4)){out[n++]='<';s+=3;}
        else if(!strncmp(s,"&gt;",4)){out[n++]='>';s+=3;}
        else if(!strncmp(s,"&nbsp;",6)){out[n++]=' ';s+=5;}
        else if((unsigned char)*s>=32)out[n++]=*s;
    }
    /* Do not leave an incomplete UTF-8 suffix at the truncation boundary. */
    if(*s)while(n && ((unsigned char)out[n-1]&0xc0)==0x80)--n;
    if(*s && n && (unsigned char)out[n-1]>=0xc0)--n;
    while(n && out[n-1]==' ')--n;
    out[n]=0;
}
static inline int maxd_parse_cues(char *body,MaxdCue *cues,int capacity) {
    int count=0,active=-1,skip=0;char *line=body;
    while(line && *line) {
        char *next=strchr(line,'\n');if(next)*next++=0;
        size_t len=strlen(line);if(len && line[len-1]=='\r')line[len-1]=0;
        if(!*line){active=-1;skip=0;line=next;continue;}
        if(!strncmp(line,"NOTE",4)||!strncmp(line,"STYLE",5)||!strncmp(line,"REGION",6))skip=1;
        char *arrow=strstr(line," --> ");
        if(arrow && !skip) {
            *arrow=0;for(char *p=line;*p;++p)if(*p==',')*p='.';
            double at=maxd_clock(line);active=-1;
            if(at>=0 && count<capacity) { active=count++;cues[active].seconds=at;cues[active].text[0]=0; }
        } else if(active>=0 && !skip)maxd_cue_text(cues[active].text,sizeof cues[active].text,line);
        line=next;
    }
    int kept=0;for(int i=0;i<count;++i)if(*cues[i].text && (!kept || cues[i].seconds>=cues[kept-1].seconds))cues[kept++]=cues[i];
    return kept;
}
static inline void maxd_experience_write(cJSON *root) {
    cJSON *rows=cJSON_AddArrayToObject(root,"maxd_bookmarks");
    for(int i=0;i<g_maxd_mark_count;++i) {
        MaxdBookmark *b=&g_maxd_marks[i];cJSON *o=cJSON_CreateObject();
        cJSON_AddStringToObject(o,"profile",b->profile);cJSON_AddStringToObject(o,"video",b->video);
        cJSON_AddStringToObject(o,"label",b->label);cJSON_AddNumberToObject(o,"seconds",b->seconds);cJSON_AddItemToArray(rows,o);
    }
}
static inline void maxd_experience_read(const cJSON *root) {
    g_maxd_mark_count=0;const cJSON *o,*rows=cJSON_GetObjectItemCaseSensitive(root,"maxd_bookmarks");
    if(!cJSON_IsArray(rows))return;
    cJSON_ArrayForEach(o,rows) {
        const char *p=maxd_smart_string(o,"profile"),*v=maxd_smart_string(o,"video"),*label=maxd_smart_string(o,"label");
        const cJSON *sec=cJSON_GetObjectItemCaseSensitive(o,"seconds");
        if(g_maxd_mark_count>=MAXD_BOOKMARKS || !maxd_smart_text(p,32,0)||strlen(v)!=11 ||
           strspn(v,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=11 || !maxd_smart_text(label,120,0) ||
           !cJSON_IsNumber(sec)||!isfinite(sec->valuedouble)||sec->valuedouble<0||sec->valuedouble>604800)continue;
        MaxdBookmark *b=&g_maxd_marks[g_maxd_mark_count++];memset(b,0,sizeof *b);
        snprintf(b->profile,sizeof b->profile,"%s",p);snprintf(b->video,sizeof b->video,"%s",v);
        snprintf(b->label,sizeof b->label,"%s",label);b->seconds=sec->valuedouble;
    }
}
#endif
