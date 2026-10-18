/* Bounded nonblocking HTTP; ticked only by the app thread. No account API here.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "maxd/party_server.h"
#include "maxd/party_page.h"
#include "tube_random.h"
#include "maxd/smart_rules.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define PARTY_GUESTS 8
#define PARTY_CONNECTIONS 8
#define PARTY_REQUEST 4096
#define PARTY_RESPONSE 24000
typedef struct {int fd;char request[PARTY_REQUEST+1],response[PARTY_RESPONSE];size_t used,sent,length;long long deadline;} PartyConnection;
static struct {
    int fd,running;char address[32],origin[80],join[160],invite[33];unsigned revision,port;
    long long now,deadline,invite_deadline,pair_window;int pair_attempts;
    struct {char token[33];long long last;unsigned mutations;} guests[PARTY_GUESTS];int guests_count;
    MaxdPartyItem items[MAXD_PARTY_ITEMS];uint64_t voters[MAXD_PARTY_ITEMS];int count;
    PartyConnection connections[PARTY_CONNECTIONS];
} party;
static int party_token(char out[33]) {unsigned char bytes[16];const char *hex="0123456789abcdef";if(tube_random_bytes(bytes,sizeof bytes))return 0;for(int i=0;i<16;++i){out[i*2]=hex[bytes[i]>>4];out[i*2+1]=hex[bytes[i]&15];}out[32]=0;return 1;}
static int party_equal(const char *a,const char *b) {if(strlen(a)!=32 || strlen(b)!=32)return 0;unsigned diff=0;for(int i=0;i<32;++i)diff|=(unsigned char)a[i]^(unsigned char)b[i];return !diff;}
static int party_id(const char *id) {return id && strlen(id)==11 && strspn(id,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==11;}
static void party_close(PartyConnection *c) {if(c->fd>=0)close(c->fd);memset(c,0,sizeof *c);c->fd=-1;}
void maxd_party_stop(void) {if(party.running){close(party.fd);for(int i=0;i<PARTY_CONNECTIONS;++i)party_close(&party.connections[i]);}memset(&party,0,sizeof party);party.fd=-1;}
int maxd_party_discover(char *address,unsigned size) {
    if(!address || size<16)return 0;
    address[0]=0;int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return 0;
    struct sockaddr_in route;memset(&route,0,sizeof route);route.sin_family=AF_INET;route.sin_port=htons(9);inet_pton(AF_INET,"192.0.2.1",&route.sin_addr);
    int ok=0;if(!connect(fd,(struct sockaddr*)&route,sizeof route)){socklen_t len=sizeof route;if(!getsockname(fd,(struct sockaddr*)&route,&len) && inet_ntop(AF_INET,&route.sin_addr,address,size))ok=strcmp(address,"0.0.0.0")!=0;}
    close(fd);return ok;
}
int maxd_party_start(const char *address,unsigned port,long long now) {
    struct in_addr ip;if(!address || inet_pton(AF_INET,address,&ip)!=1 || !port || port>65535 || now<0)return 0;
    uint32_t host=ntohl(ip.s_addr);
    if(!((host>>24)==10 || (host>>20)==0xac1 || (host>>16)==0xc0a8 || (host>>24)==127))return 0;
    maxd_party_stop();if(!party_token(party.invite))return 0;
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return 0;
    if(fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);return 0;}
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);
    struct sockaddr_in local;memset(&local,0,sizeof local);local.sin_family=AF_INET;local.sin_port=htons((uint16_t)port);local.sin_addr=ip;
    if(bind(fd,(struct sockaddr*)&local,sizeof local) || listen(fd,8)){close(fd);return 0;}
    party.fd=fd;party.running=1;party.now=now;party.deadline=now+7200000;party.invite_deadline=now+600000;party.port=port;party.revision=1;
    snprintf(party.address,sizeof party.address,"%s",address);snprintf(party.origin,sizeof party.origin,"http://%s:%u",address,port);
    snprintf(party.join,sizeof party.join,"%s/#%s",party.origin,party.invite);
    for(int i=0;i<PARTY_CONNECTIONS;++i)party.connections[i].fd=-1;
    return 1;
}
int maxd_party_running(void) {return party.running;}
const char *maxd_party_address(void) {return party.address;}
const char *maxd_party_join(void) {return party.join;}
unsigned maxd_party_revision(void) {return party.revision;}
int maxd_party_count(void) {return party.count;}
const MaxdPartyItem *maxd_party_item(int i) {return i>=0 && i<party.count?&party.items[i]:NULL;}
int maxd_party_approve(int i,int approve) {if(!party.running || i<0 || i>=party.count)return 0;party.items[i].approved=!!approve;party.items[i].rejected=!approve;++party.revision;return 1;}
void maxd_party_mark_queued(int i) {if(i>=0 && i<party.count)party.items[i].enqueued=1;}
static const char *party_string(cJSON *o,const char *key) {cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsString(v)?v->valuestring:"";}
static void party_response(PartyConnection *c,int status,const char *type,const char *body) {
    int length=snprintf(c->response,sizeof c->response,"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'; form-action 'self'\r\n\r\n%s",status,status==200?"OK":"Error",type,strlen(body),body);
    if(length<0 || (size_t)length>=sizeof c->response){party_close(c);return;}c->length=(size_t)length;c->deadline=party.now+5000;
}
static void party_error(PartyConnection *c,int status,const char *message) {char json[160];snprintf(json,sizeof json,"{\"error\":\"%s\"}",message);party_response(c,status,"application/json",json);}
static int party_header(char *headers,const char *key,char *out,size_t size) {
    *out=0;size_t len=strlen(key);int found=0;
    for(char *p=strstr(headers,"\r\n");p && p[2];p=strstr(p+2,"\r\n")) {
        p+=2;if(!strncasecmp(p,key,len) && p[len]==':') {
            if(found++)return 0;
            const char *value=p+len+1;while(*value==' ')++value;const char *end=strstr(value,"\r\n");
            if(!end || (size_t)(end-value)>=size)return 0;
            memcpy(out,value,(size_t)(end-value));out[end-value]=0;
        }
        p-=2;
    }
    return found==1;
}
static void party_dispatch(PartyConnection *c,char *headers,const char *body) {
    char method[12],path[128],version[16],host[80],origin[80],auth[80],type[80],encoding[80];
    if(sscanf(headers,"%11s %127s %15s",method,path,version)!=3 || strcmp(version,"HTTP/1.1")){party_error(c,400,"Invalid HTTP request");return;}
    const char *expected=party.origin+7;
    if(!party_header(headers,"Host",host,sizeof host) || strcmp(host,expected) || party_header(headers,"Transfer-Encoding",encoding,sizeof encoding) ||
       (party_header(headers,"Origin",origin,sizeof origin) && strcmp(origin,party.origin))){party_error(c,403,"Origin not allowed");return;}
    if(!strcmp(method,"GET") && !strcmp(path,"/")){party_response(c,200,"text/html; charset=utf-8",maxd_party_page);return;}
    if(strcmp(method,"POST") || !party_header(headers,"Content-Type",type,sizeof type) || strcmp(type,"application/json")){party_error(c,415,"Use JSON requests from the party page");return;}
    cJSON *json=cJSON_ParseWithOpts(body,NULL,1);if(!cJSON_IsObject(json)){cJSON_Delete(json);party_error(c,400,"Invalid JSON");return;}
    if(!strcmp(path,"/pair")) {
        if(party.now-party.pair_window>=10000){party.pair_window=party.now;party.pair_attempts=0;}
        if(++party.pair_attempts>16 || party.guests_count>=PARTY_GUESTS){party_error(c,429,"Room full or too many pairing attempts");}
        else if(party.now>party.invite_deadline || !party_equal(party_string(json,"join"),party.invite))party_error(c,403,"Invite expired; ask the PS5 host for a new room");
        else {
            int i=party.guests_count;if(!party_token(party.guests[i].token))party_error(c,503,"Could not create guest session");
            else {++party.guests_count;char reply[100];snprintf(reply,sizeof reply,"{\"token\":\"%s\"}",party.guests[i].token);party_response(c,200,"application/json",reply);}
        }
        cJSON_Delete(json);return;
    }
    int guest=-1;
    if(party_header(headers,"Authorization",auth,sizeof auth) && !strncmp(auth,"Bearer ",7))for(int i=0;i<party.guests_count;++i)if(party_equal(auth+7,party.guests[i].token)){guest=i;break;}
    if(guest<0){cJSON_Delete(json);party_error(c,401,"Guest session ended; join the room again");return;}
    if(!strcmp(path,"/state")) {
        cJSON *result=cJSON_CreateObject(),*items=cJSON_AddArrayToObject(result,"items");
        for(int i=0;i<party.count;++i)if(!party.items[i].rejected){MaxdPartyItem *r=&party.items[i];cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"id",r->id);cJSON_AddStringToObject(o,"title",r->title);cJSON_AddNumberToObject(o,"votes",r->votes);cJSON_AddBoolToObject(o,"approved",r->approved);cJSON_AddBoolToObject(o,"voted",!!(party.voters[i]&(1ULL<<guest)));cJSON_AddItemToArray(items,o);}
        char *reply=cJSON_PrintUnformatted(result);cJSON_Delete(result);if(reply){party_response(c,200,"application/json",reply);free(reply);}else party_error(c,503,"Out of memory");
    } else if(!strcmp(path,"/suggest") || !strcmp(path,"/vote")) {
        const char *id=party_string(json,"id"),*title=party_string(json,"title");
        if(!party_id(id) || !maxd_smart_text(title,120,1)){party_error(c,400,"Invalid video ID or label");}
        else if(party.now-party.guests[guest].last<1000 && party.guests[guest].mutations)party_error(c,429,"Wait a second before changing the queue");
        else {
            party.guests[guest].last=party.now;++party.guests[guest].mutations;
            int at=-1;for(int i=0;i<party.count;++i)if(!strcmp(party.items[i].id,id)){at=i;break;}
            if(at<0 && !strcmp(path,"/suggest") && party.count<MAXD_PARTY_ITEMS){at=party.count++;snprintf(party.items[at].id,12,"%s",id);snprintf(party.items[at].title,121,"%s",title);}
            if(at<0 || party.items[at].rejected)party_error(c,409,"Video unavailable or room queue full");
            else {uint64_t mask=1ULL<<guest;if(!(party.voters[at]&mask)){party.voters[at]|=mask;++party.items[at].votes;++party.revision;}party_response(c,200,"application/json","{\"ok\":true}");}
        }
    } else party_error(c,404,"Unknown party operation");
    cJSON_Delete(json);
}
void maxd_party_tick(long long now) {
    if(!party.running)return;
    party.now=now;if(now>=party.deadline || now<0){maxd_party_stop();return;}
    for(int i=0;i<PARTY_CONNECTIONS;++i) {
        PartyConnection *c=&party.connections[i];
        if(c->fd<0) {int fd=accept(party.fd,NULL,NULL);if(fd<0)continue;if(fcntl(fd,F_SETFL,O_NONBLOCK)<0){close(fd);continue;}
#ifdef SO_NOSIGPIPE
            int quiet=1;setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&quiet,sizeof quiet);
#endif
            c->fd=fd;c->deadline=now+5000;}
        if(now>c->deadline){party_close(c);continue;}
        if(c->length) {
#ifdef MSG_NOSIGNAL
            int flags=MSG_NOSIGNAL;
#else
            int flags=0;
#endif
            ssize_t n=send(c->fd,c->response+c->sent,c->length-c->sent,flags);
            if(n>0)c->sent+=(size_t)n;
            if(c->sent==c->length || (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK))party_close(c);
            continue;
        }
        ssize_t n=recv(c->fd,c->request+c->used,PARTY_REQUEST-c->used,0);
        if(n==0 || (n<0 && errno!=EAGAIN && errno!=EWOULDBLOCK)){party_close(c);continue;}
        if(n<0)continue;
        c->used+=(size_t)n;c->request[c->used]=0;
        if(memchr(c->request,0,c->used)){party_error(c,400,"Invalid request encoding");continue;}
        char *end=strstr(c->request,"\r\n\r\n");
        if(!end){if(c->used==PARTY_REQUEST)party_error(c,413,"Request too large");continue;}
        char saved=end[2];end[2]=0;char value[24];size_t length=0;
        if(party_header(c->request,"Content-Length",value,sizeof value)) {
            char *tail=NULL;unsigned long v=strtoul(value,&tail,10);
            if(!*value || *tail || v>1024){end[2]=saved;party_error(c,413,"Body too large");continue;}length=(size_t)v;
        }
        end[2]=saved;size_t header_size=(size_t)(end-c->request)+4;
        if(header_size+length>PARTY_REQUEST){party_error(c,413,"Request too large");continue;}
        if(c->used<header_size+length)continue;
        if(c->used!=header_size+length){party_error(c,400,"Pipelining is unsupported");continue;}
        end[2]=0;party_dispatch(c,c->request,c->request+header_size);
    }
}
