/* Reports accept only typed fields, and use an atomic private file. GPL-3.0-or-later. */
#include "maxd/diagnostics.h"
#include "maxd/brand.h"
#include "cJSON.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(void) {
    char dir[]="/tmp/maxd-report-XXXXXX",path[256],tmp[260];assert(mkdtemp(dir));snprintf(path,sizeof path,"%s/bug-report.json",dir);snprintf(tmp,sizeof tmp,"%s.tmp",path);
    MaxdDiagnostics d={0};d.active=1;d.width=3840;d.height=2160;d.codec=4;d.frames=1234;d.dropped=7;d.fps=59.94;d.position=INFINITY;d.duration=NAN;d.height_limit=2160;
    maxd_report_sample(&d);assert(!*maxd_report_json());assert(maxd_report_capture(path));
    struct stat info;assert(!stat(path,&info) && (info.st_mode&0777)==0600);
    cJSON *o=cJSON_Parse(maxd_report_json());assert(o);
    assert(!strcmp(cJSON_GetObjectItem(o,"version")->valuestring,YM_VERSION));
    assert(!strcmp(cJSON_GetObjectItem(o,"codec")->valuestring,"hevc"));
    assert(cJSON_GetObjectItem(o,"position_seconds")->valuedouble==0 && cJSON_GetObjectItem(o,"duration_seconds")->valuedouble==0);
    assert(cJSON_GetObjectItem(o,"width")->valueint==3840 && cJSON_GetObjectItem(o,"dropped_frames")->valueint==7);
    /* The entire schema is a closed allowlist. No identity/string metadata passes through. */
    const char *keys="|schema|app|version|platform|privacy|playing|paused|live|buffering|hardware_decoder|captions|picture_in_picture|afterglow|auto_max|width|height|fps|position_seconds|duration_seconds|codec|maximum_height|maximum_fps|codec_preference|decoded_frames|dropped_frames|";
    cJSON *v;int count=0;cJSON_ArrayForEach(v,o){char needle[100];snprintf(needle,sizeof needle,"|%s|",v->string);assert(strstr(keys,needle));++count;}assert(count==25);cJSON_Delete(o);
    FILE *f=fopen(tmp,"w");assert(f);fputs("leftover",f);fclose(f);assert(!maxd_report_capture(path));assert(maxd_report_clear(path) && access(tmp,F_OK));
    d.width=-1;d.codec=999;maxd_report_sample(&d);assert(maxd_report_capture(path));assert(strstr(maxd_report_json(),"unknown"));
    assert(maxd_report_clear(path) && !*maxd_report_json() && access(path,F_OK));assert(maxd_report_clear(path));assert(!rmdir(dir));
    puts("PASS: report allowlist, enum codecs, numeric validation, exact version, private atomic export, overwrite, clear and leftover rejection");
}
