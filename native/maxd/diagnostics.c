/* User-requested diagnostics only; never reads logs or configuration files.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "maxd/diagnostics.h"
#include "maxd/brand.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
static MaxdDiagnostics current;
static char report[4096];
void maxd_report_sample(const MaxdDiagnostics *s) { if(s)current=*s; }
const char *maxd_report_json(void) { return report; }
static double finite_range(double n,double max) {return isfinite(n) && n>=0 && n<=max?n:0;}
static int bounded(int n,int max) {return n>=0 && n<=max?n:0;}
int maxd_report_capture(const char *path) {
    /* A fixed allowlist, including enum codecs instead of stream metadata. */
    const MaxdDiagnostics *s=&current;
    const char *codecs[]={"unknown","avc","vp9","av1","hevc"};
    cJSON *root=cJSON_CreateObject(); if(!root)return 0;
    cJSON_AddNumberToObject(root,"schema",1);
    cJSON_AddStringToObject(root,"app","YouTube Max'd");
    cJSON_AddStringToObject(root,"version",YM_VERSION);
    cJSON_AddStringToObject(root,"platform","PS5");
    cJSON_AddStringToObject(root,"privacy","No accounts, tokens, IP addresses, video IDs, titles, searches, URLs or raw logs. Review before sharing.");
#define BOOL(key,field) cJSON_AddBoolToObject(root,key,!!s->field)
#define NUM(key,value) cJSON_AddNumberToObject(root,key,(double)(value))
    BOOL("playing",active);BOOL("paused",paused);BOOL("live",live);BOOL("buffering",buffering);
    BOOL("hardware_decoder",hardware);BOOL("captions",captions);BOOL("picture_in_picture",pip);
    BOOL("afterglow",afterglow);BOOL("auto_max",auto_max);
    NUM("width",bounded(s->width,16384));NUM("height",bounded(s->height,16384));
    NUM("fps",finite_range(s->fps,240));NUM("position_seconds",finite_range(s->position,604800));
    NUM("duration_seconds",finite_range(s->duration,604800));
    cJSON_AddStringToObject(root,"codec",codecs[bounded(s->codec,4)]);
    NUM("maximum_height",bounded(s->height_limit,4320));NUM("maximum_fps",bounded(s->fps_limit,240));
    cJSON_AddStringToObject(root,"codec_preference",s->codec_preference>=1 && s->codec_preference<=4?codecs[s->codec_preference]:"auto");
    NUM("decoded_frames",s->frames>9007199254740991ULL?9007199254740991ULL:s->frames);
    NUM("dropped_frames",s->dropped>9007199254740991ULL?9007199254740991ULL:s->dropped);
#undef NUM
#undef BOOL
    char *text=cJSON_Print(root);cJSON_Delete(root);if(!text)return 0;
    if(strlen(text)>=sizeof report){free(text);return 0;}
    int ok=1;
    if(path && *path) {
        char temporary[1024];int n=snprintf(temporary,sizeof temporary,"%s.tmp",path);
        if(n<0 || (size_t)n>=sizeof temporary){free(text);return 0;}
        int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL,0600);
        /* Never follow or truncate an unexpected leftover path. */
        if(fd<0){free(text);return 0;}
        size_t total=strlen(text),written=0;
        while(written<total){ssize_t bytes=write(fd,text+written,total-written);if(bytes<0 && errno==EINTR)continue;if(bytes<=0){ok=0;break;}written+=(size_t)bytes;}
        if(fsync(fd))ok=0;
        if(close(fd))ok=0;
        if(ok && rename(temporary,path))ok=0;
        if(!ok)unlink(temporary);
    }
    if(ok)snprintf(report,sizeof report,"%s",text);
    free(text);return ok;
}
int maxd_report_clear(const char *path) {
    report[0]=0;memset(&current,0,sizeof current);
    if(!path || !*path)return 1;
    int ok=!unlink(path) || errno==ENOENT;
    char temporary[1024];int n=snprintf(temporary,sizeof temporary,"%s.tmp",path);
    if(n<0 || (size_t)n>=sizeof temporary)return 0;
    if(unlink(temporary) && errno!=ENOENT)ok=0;
    return ok;
}
