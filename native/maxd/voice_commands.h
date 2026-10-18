/* Deterministic, local voice commands. GPL-3.0-or-later. */
#ifndef MAXD_VOICE_COMMANDS_H
#define MAXD_VOICE_COMMANDS_H
#include <string.h>
#include <stdio.h>
#include <ctype.h>
enum {MAXD_CMD_NONE,MAXD_CMD_PAUSE,MAXD_CMD_RESUME,MAXD_CMD_SEEK,MAXD_CMD_CAPTIONS,MAXD_CMD_QUALITY,MAXD_CMD_QUEUE};
typedef struct {int kind,value;} MaxdVoiceCommand;
static inline MaxdVoiceCommand maxd_voice_command(const char *text) {
    MaxdVoiceCommand c={MAXD_CMD_NONE,0};char s[129];size_t n=0;
    if(!text || strlen(text)>128)return c;
    while(*text && isspace((unsigned char)*text))++text;
    for(;*text && n<128;++text) {
        unsigned char ch=(unsigned char)*text;
        if(ch>='A'&&ch<='Z')ch+=32;
        if(isspace(ch)){if(n && s[n-1]!=' ')s[n++]=' ';}else s[n++]=(char)ch;
    }
    while(n && (s[n-1]==' '||s[n-1]=='.'||s[n-1]=='!'||s[n-1]=='?'))--n;
    s[n]=0;
    if(!strcmp(s,"pause")||!strcmp(s,"pause video"))c.kind=MAXD_CMD_PAUSE;
    else if(!strcmp(s,"play")||!strcmp(s,"resume")||!strcmp(s,"resume video"))c.kind=MAXD_CMD_RESUME;
    else if(!strcmp(s,"add to queue")||!strcmp(s,"queue this video"))c.kind=MAXD_CMD_QUEUE;
    else if(!strcmp(s,"captions on")||!strcmp(s,"subtitles on")){c.kind=MAXD_CMD_CAPTIONS;c.value=1;}
    else if(!strcmp(s,"captions off")||!strcmp(s,"subtitles off")){c.kind=MAXD_CMD_CAPTIONS;c.value=0;}
    else if(!strcmp(s,"forward")||!strcmp(s,"skip forward")){c.kind=MAXD_CMD_SEEK;c.value=10;}
    else if(!strcmp(s,"rewind")||!strcmp(s,"go back")){c.kind=MAXD_CMD_SEEK;c.value=-10;}
    else {
        const char *number=NULL;int direction=0;
        if(!strncmp(s,"forward ",8)){number=s+8;direction=1;}
        if(!strncmp(s,"back ",5)){number=s+5;direction=-1;}
        if(number) {
            size_t digits=strspn(number,"0123456789");
            if(digits>=1 && digits<=3 && !strcmp(number+digits," seconds")) {
                int sec=0;for(size_t i=0;i<digits;++i)sec=sec*10+number[i]-'0';
                if(sec>=1 && sec<=300){c.kind=MAXD_CMD_SEEK;c.value=sec*direction;}
            }
        }
        const int heights[]={144,240,360,480,720,1080,1440,2160};
        for(unsigned i=0;i<sizeof heights/sizeof *heights;++i) {
            char a[32],b[32];snprintf(a,sizeof a,"quality %dp",heights[i]);snprintf(b,sizeof b,"quality %d",heights[i]);
            if(!strcmp(s,a)||!strcmp(s,b)){c.kind=MAXD_CMD_QUALITY;c.value=heights[i];}
        }
        if(!strcmp(s,"quality 4k")){c.kind=MAXD_CMD_QUALITY;c.value=2160;}
    }
    return c;
}
#endif
