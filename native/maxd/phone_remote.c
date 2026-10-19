/* Bounded LAN HTTP controller, separate from party guest access.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "maxd/phone_remote.h"
#include "maxd/phone_remote_page.h"
#include "maxd/smart_rules.h"
#include "tube_random.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define REMOTE_CONNECTIONS 4
#define REMOTE_REQUEST 4096
#define REMOTE_RESPONSE 100000
#define REMOTE_SESSION_MS 7200000
#define REMOTE_INVITE_MS 300000
/* One controlling phone, one-use QR, short request deadlines. */
typedef struct {int fd;char request[REMOTE_REQUEST+1],response[REMOTE_RESPONSE];size_t used,sent,length;int64_t deadline;} RemoteConnection;
static struct {
    int fd,running,paired,pair_attempts;char origin[80],join[160],invite[33],token[33];
    int64_t now,deadline,invite_deadline,pair_window,last_command;
    unsigned sequence;int last_applied;
    RemoteConnection connections[REMOTE_CONNECTIONS];
} remote;
static int remote_token(char out[33]) {
    unsigned char bytes[16];const char *hex="0123456789abcdef";
    if(tube_random_bytes(bytes,sizeof bytes))return 0;
    for(int i=0;i<16;++i){out[i*2]=hex[bytes[i]>>4];out[i*2+1]=hex[bytes[i]&15];}out[32]=0;return 1;
}
static int remote_equal(const char *a,const char *b) {
    if(strlen(a)!=32 || strlen(b)!=32)return 0;
    unsigned diff=0;for(int i=0;i<32;++i)diff|=(unsigned char)a[i]^(unsigned char)b[i];return !diff;
}
static int remote_id(const char *id) {return id && strlen(id)==11 && strspn(id,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==11;}
static void remote_close(RemoteConnection *c) {if(c->fd>=0)close(c->fd);memset(c,0,sizeof *c);c->fd=-1;}
void maxd_remote_stop(void) {
    if(remote.running){close(remote.fd);for(int i=0;i<REMOTE_CONNECTIONS;++i)remote_close(&remote.connections[i]);}
    memset(&remote,0,sizeof remote);remote.fd=-1;
}
int maxd_remote_start(const char *address,unsigned port,int64_t now) {
    struct in_addr ip;if(!address || inet_pton(AF_INET,address,&ip)!=1 || !port || port>65535 || now<0 || now>INT64_MAX-REMOTE_SESSION_MS)return 0;
    uint32_t host=ntohl(ip.s_addr);
    if(!((host>>24)==10 || (host>>20)==0xac1 || (host>>16)==0xc0a8 || (host>>24)==127))return 0;
    maxd_remote_stop();if(!remote_token(remote.invite))return 0;
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 0;
    if(fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);return 0;}
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
    struct sockaddr_in local;memset(&local,0,sizeof local);local.sin_family=AF_INET;local.sin_port=htons((uint16_t)port);local.sin_addr=ip;
    if(bind(fd,(struct sockaddr*)&local,sizeof local) || listen(fd,4)){close(fd);return 0;}
    remote.fd=fd;remote.running=1;remote.now=now;remote.deadline=now+REMOTE_SESSION_MS;remote.invite_deadline=now+REMOTE_INVITE_MS;
    snprintf(remote.origin,sizeof remote.origin,"http://%s:%u",address,port);
    snprintf(remote.join,sizeof remote.join,"%s/#%s",remote.origin,remote.invite);
    for(int i=0;i<REMOTE_CONNECTIONS;++i)remote.connections[i].fd=-1;
    return 1;
}
int maxd_remote_running(void) {return remote.running;}
int maxd_remote_paired(void) {return remote.running && remote.paired;}
const char *maxd_remote_join(void) {return remote.join;}
static const char *remote_string(cJSON *o,const char *key) {cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsString(v)?v->valuestring:"";}
static int remote_unsigned(cJSON *o,const char *key,unsigned *out) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<1 || v->valuedouble>2147483647 || floor(v->valuedouble)!=v->valuedouble)return 0;
    *out=(unsigned)v->valuedouble;return 1;
}
static void remote_response(RemoteConnection *c,int status,const char *type,const char *body) {
    int length=snprintf(c->response,sizeof c->response,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'\r\n\r\n%s",status,status==200?"OK":"Error",type,strlen(body),body);
    if(length<0 || (size_t)length>=sizeof c->response){remote_close(c);return;}c->length=(size_t)length;c->deadline=remote.now+5000;
}
static void remote_error(RemoteConnection *c,int status,const char *message) {char json[200];snprintf(json,sizeof json,"{\"error\":\"%s\"}",message);remote_response(c,status,"application/json",json);}
static int remote_header(char *headers,const char *key,char *out,size_t size) {
    *out=0;size_t len=strlen(key);int found=0;
    for(char *p=strstr(headers,"\r\n");p && p[2];p=strstr(p+2,"\r\n")) {
        p+=2;if(!strncasecmp(p,key,len) && p[len]==':') {
            if(found++)return -1;
            const char *value=p+len+1;while(*value==' ')++value;const char *end=strstr(value,"\r\n");
            if(!end || (size_t)(end-value)>=size)return -1;
            memcpy(out,value,(size_t)(end-value));out[end-value]=0;
        }
        p-=2;
    }
    return found;
}

static void remote_reply(RemoteConnection *c,cJSON *o) {
    char *body=cJSON_PrintUnformatted(o);cJSON_Delete(o);
    if(body){remote_response(c,200,"application/json",body);free(body);}else remote_error(c,503,"Out of memory");
}
static int remote_unique(cJSON *o) {
    cJSON *a,*b;cJSON_ArrayForEach(a,o)for(b=a->next;b;b=b->next)if(a->string && b->string && !strcmp(a->string,b->string))return 0;
    return 1;
}
static void remote_dispatch(RemoteConnection *c,char *headers,const char *body,const MaxdRemoteControls *controls) {
    char method[12],path[128],version[16],host[80],origin[80],auth[80],type[80],encoding[80];
    if(sscanf(headers,"%11s %127s %15s",method,path,version)!=3 || strcmp(version,"HTTP/1.1")){remote_error(c,400,"Invalid HTTP request");return;}
    int origins=remote_header(headers,"Origin",origin,sizeof origin);
    if(remote_header(headers,"Host",host,sizeof host)!=1 || strcmp(host,remote.origin+7) ||
       remote_header(headers,"Transfer-Encoding",encoding,sizeof encoding)!=0 || origins<0 || (origins && strcmp(origin,remote.origin))) {
        remote_error(c,403,"Origin not allowed");return;
    }
    if(!strcmp(method,"GET") && !strcmp(path,"/")){remote_response(c,200,"text/html; charset=utf-8",maxd_phone_remote_page);return;}
    if(strcmp(method,"POST") || remote_header(headers,"Content-Type",type,sizeof type)!=1 || strcmp(type,"application/json")) {
        remote_error(c,415,"Use the remote page's JSON requests");return;
    }
    cJSON *json=cJSON_ParseWithOpts(body,NULL,1);
    if(!cJSON_IsObject(json) || !remote_unique(json)){cJSON_Delete(json);remote_error(c,400,"Invalid JSON");return;}
    if(!strcmp(path,"/pair")) {
        if(remote.now-remote.pair_window>=10000){remote.pair_window=remote.now;remote.pair_attempts=0;}
        if(++remote.pair_attempts>12)remote_error(c,429,"Wait ten seconds before pairing again");
        else if(remote.paired || remote.now>=remote.invite_deadline || !remote_equal(remote_string(json,"join"),remote.invite))
            remote_error(c,403,"QR used or expired. Restart Phone remote on the PS5");
        else if(!remote_token(remote.token))remote_error(c,503,"Could not create session");
        else {remote.paired=1;remote.invite[0]=0;cJSON *r=cJSON_CreateObject();cJSON_AddStringToObject(r,"token",remote.token);remote_reply(c,r);}
        cJSON_Delete(json);return;
    }
    if(!remote.paired || remote_header(headers,"Authorization",auth,sizeof auth)!=1 || strncmp(auth,"Bearer ",7) || !remote_equal(auth+7,remote.token)) {
        cJSON_Delete(json);remote_error(c,401,"Session ended. Pair with a new PS5 QR code");return;
    }
    if(!controls || !controls->snapshot || !controls->apply){cJSON_Delete(json);remote_error(c,503,"Console is busy");return;}
    MaxdRemoteState state;memset(&state,0,sizeof state);controls->snapshot(controls->context,&state);
    if(!strcmp(path,"/state")) {
        cJSON *r=cJSON_CreateObject();
        cJSON_AddBoolToObject(r,"active",state.active);cJSON_AddBoolToObject(r,"paused",state.paused);
        cJSON_AddBoolToObject(r,"live",state.live);cJSON_AddBoolToObject(r,"busy",state.busy);
        cJSON_AddNumberToObject(r,"selection",state.selection);
        cJSON_AddNumberToObject(r,"position",isfinite(state.position)?state.position:0);
        cJSON_AddNumberToObject(r,"duration",isfinite(state.duration)?state.duration:0);
        cJSON_AddNumberToObject(r,"sequence",remote.sequence);
        cJSON_AddBoolToObject(r,"report_ready",controls->report && *controls->report(controls->context));
        cJSON *items=cJSON_AddArrayToObject(r,"queue");
        for(int i=0;i<state.count && i<MAXD_REMOTE_QUEUE;++i) {
            cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"id",state.queue[i].id);
            cJSON_AddStringToObject(item,"title",state.queue[i].title);cJSON_AddItemToArray(items,item);
        }
        remote_reply(c,r);
    } else if(!strcmp(path,"/report")) {
        const char *report=controls->report?controls->report(controls->context):"";
        if(*report)remote_response(c,200,"application/json",report);else remote_error(c,404,"Capture a report first");
    } else if(!strcmp(path,"/command")) {
        unsigned sequence;
        if(!remote_unsigned(json,"sequence",&sequence))remote_error(c,400,"Invalid command sequence");
        else if(sequence==remote.sequence) {
            remote_response(c,200,"application/json",remote.last_applied?"{\"ok\":true}":"{\"ok\":false}");
        } else if(sequence!=remote.sequence+1)remote_error(c,409,"Refresh remote state before retrying");
        else if(remote.now-remote.last_command<150)remote_error(c,429,"Slow down and retry");
        else {
            const char *action=remote_string(json,"action");MaxdRemoteCommand command;memset(&command,0,sizeof command);
            const char *actions[]={"pause","resume","seek","search","key","add","remove","next"};
            for(int i=0;i<8;++i)if(!strcmp(action,actions[i]))command.kind=i+1;
            int valid=command.kind!=0,applied=0;
            if(command.kind<=MAXD_REMOTE_SEEK)valid=valid && remote_unsigned(json,"selection",&command.selection) && state.active && command.selection==state.selection;
            if(command.kind==MAXD_REMOTE_SEEK) {
                cJSON *v=cJSON_GetObjectItemCaseSensitive(json,"seconds");
                valid=valid && cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble>=0 && v->valuedouble<=604800 && !state.live;
                if(valid)command.seconds=v->valuedouble;
            }
            const char *text=remote_string(json,"text");
            if(command.kind==MAXD_REMOTE_SEARCH)valid=maxd_smart_text(text,800,0);
            if(command.kind==MAXD_REMOTE_KEY)valid=!strcmp(text,"UP") || !strcmp(text,"DOWN") || !strcmp(text,"LEFT") || !strcmp(text,"RIGHT") || !strcmp(text,"ENTER") || !strcmp(text,"BACK");
            if(command.kind>=MAXD_REMOTE_ADD)valid=remote_id(text);
            if(!strcmp(action,"capture")) {valid=1;if(!state.busy && controls->capture)applied=controls->capture(controls->context);}
            else if(valid && !state.busy) {snprintf(command.text,sizeof command.text,"%s",text);applied=controls->apply(controls->context,&command);}
            remote.sequence=sequence;remote.last_command=remote.now;remote.last_applied=applied;
            if(!valid)remote_error(c,400,"Invalid command or stale playback selection");
            else remote_response(c,200,"application/json",applied?"{\"ok\":true}":"{\"ok\":false}");
        }
    } else remote_error(c,404,"Unknown remote operation");
    cJSON_Delete(json);
}
void maxd_remote_tick(int64_t now,const MaxdRemoteControls *controls) {
    if(!remote.running)return;
    if(now<remote.now){maxd_remote_stop();return;}
    remote.now=now;if(now>=remote.deadline || now<0){maxd_remote_stop();return;}
    for(int i=0;i<REMOTE_CONNECTIONS;++i) {
        RemoteConnection *c=&remote.connections[i];
        if(c->fd<0) {int fd=accept(remote.fd,NULL,NULL);if(fd<0)continue;if(fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);continue;}
#ifdef SO_NOSIGPIPE
            int quiet=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&quiet,sizeof quiet);
#endif
            c->fd=fd;c->deadline=now+5000;}
        if(now>c->deadline){remote_close(c);continue;}
        if(c->length) {
#ifdef MSG_NOSIGNAL
            int flags=MSG_NOSIGNAL;
#else
            int flags=0;
#endif
            ssize_t n=send(c->fd,c->response+c->sent,c->length-c->sent,flags);
            if(n>0)c->sent+=(size_t)n;
            if(c->sent==c->length || (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK))remote_close(c);
            continue;
        }
        ssize_t n=recv(c->fd,c->request+c->used,REMOTE_REQUEST-c->used,0);
        if(n==0 || (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK)){remote_close(c);continue;}
        if(n<0)continue;
        c->used+=(size_t)n;c->request[c->used]=0;
        if(memchr(c->request,0,c->used)){remote_error(c,400,"Invalid request encoding");continue;}
        char *end=strstr(c->request,"\r\n\r\n");
        if(!end){if(c->used==REMOTE_REQUEST)remote_error(c,413,"Request too large");continue;}
        char saved=end[2];end[2]=0;char value[24];size_t length=0;
        int lengths=remote_header(c->request,"Content-Length",value,sizeof value);
        if(lengths<0){end[2]=saved;remote_error(c,400,"Duplicate or invalid length");continue;}
        if(remote_header(c->request,"Content-Length",value,sizeof value)==1) {
            char *tail=NULL;unsigned long v=strtoul(value,&tail,10);
            if(!*value || strspn(value,"0123456789")!=strlen(value) || *tail || v>2048){end[2]=saved;remote_error(c,413,"Body too large");continue;}length=(size_t)v;
        }
        end[2]=saved;size_t header_size=(size_t)(end-c->request)+4;
        if(header_size+length>REMOTE_REQUEST){remote_error(c,413,"Request too large");continue;}
        if(c->used<header_size+length)continue;
        if(c->used!=header_size+length){remote_error(c,400,"Pipelining is unsupported");continue;}
        end[2]=0;remote_dispatch(c,c->request,c->request+header_size,controls);
    }
}
