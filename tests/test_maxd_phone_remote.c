/* Exercise the real HTTP listener and its privacy-only report path. GPL-3.0-or-later. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "maxd/phone_remote.h"
#include "maxd/diagnostics.h"
#include "cJSON.h"
static int64_t clock_ms=100000;
static char response[120000];
static MaxdRemoteState state;
static int applied,captured;
static MaxdRemoteCommand command;
static void snapshot(void *context,MaxdRemoteState *out){(void)context;*out=state;}
static int apply(void *context,const MaxdRemoteCommand *c){(void)context;command=*c;++applied;return 1;}
static int capture(void *context){(void)context;++captured;return maxd_report_capture(NULL);}
static const char *report(void *context){(void)context;return maxd_report_json();}
static MaxdRemoteControls controls={NULL,snapshot,apply,capture,report};
static int http(const char *method,const char *path,const char *token,const char *body,const char *extra) {
    int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(28667);inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    assert(!connect(fd,(struct sockaddr*)&addr,sizeof addr));char request[6000],auth[100]="";if(token)snprintf(auth,sizeof auth,"Authorization: Bearer %s\r\n",token);
    int len=snprintf(request,sizeof request,"%s %s HTTP/1.1\r\nHost: 127.0.0.1:28667\r\nContent-Type: application/json\r\n%s%sContent-Length: %zu\r\n\r\n%s",method,path,auth,extra?extra:"",strlen(body),body);
    assert(send(fd,request,(size_t)len,0)==len);assert(!fcntl(fd,F_SETFL,O_NONBLOCK));size_t used=0;
    for(int i=0;i<1000;++i) {
        maxd_remote_tick(clock_ms,&controls);ssize_t n=recv(fd,response+used,sizeof response-used-1,0);
        if(n>0)used+=(size_t)n;else if(!n)break;else assert(errno==EAGAIN || errno==EWOULDBLOCK);
        assert(i!=999);usleep(1000);
    }
    close(fd);response[used]=0;int status=0;assert(sscanf(response,"HTTP/1.1 %d",&status)==1);return status;
}
static cJSON *json(void){char *start=strstr(response,"\r\n\r\n");assert(start);cJSON *o=cJSON_Parse(start+4);assert(o);return o;}
static void send_command(const char *token,const char *body,int status) {clock_ms+=200;assert(http("POST","/command",token,body,NULL)==status);}
int main(void) {
    assert(!maxd_remote_start("0.0.0.0",28667,clock_ms));assert(!maxd_remote_start("8.8.8.8",28667,clock_ms));
    assert(!maxd_remote_start("127.0.0.1",28667,INT64_MAX));assert(maxd_remote_start("127.0.0.1",28667,clock_ms));
    char invite[33],token[33],body[200];snprintf(invite,sizeof invite,"%s",strchr(maxd_remote_join(),'#')+1);
    assert(http("GET","/",NULL,"",NULL)==200 && strstr(response,"Phone remote"));
    assert(strstr(response,"frame-ancestors 'none'") && strstr(response,"Cache-Control: no-store"));
    assert(http("POST","/state",NULL,"{}",NULL)==401);
    assert(http("POST","/report",NULL,"{}",NULL)==401);
    assert(http("POST","/pair",NULL,"{\"join\":\"wrong\"}",NULL)==403);
    snprintf(body,sizeof body,"{\"join\":\"%s\"}",invite);
    assert(http("POST","/pair",NULL,body,"Origin: http://evil.example\r\n")==403);
    assert(http("POST","/pair",NULL,body,"Origin: http://127.0.0.1:28667\r\nOrigin: null\r\n")==403);
    assert(http("POST","/pair",NULL,body,"Transfer-Encoding: chunked\r\n")==403);
    assert(http("POST","/pair",NULL,body,"Content-Length: 1\r\n")==400);
    assert(http("POST","/pair",NULL,body,NULL)==200);cJSON *o=json();snprintf(token,sizeof token,"%s",cJSON_GetObjectItem(o,"token")->valuestring);cJSON_Delete(o);
    assert(maxd_remote_paired());assert(http("POST","/pair",NULL,body,NULL)==403);
    state.active=1;state.selection=42;state.position=100;state.duration=500;
    send_command(token,"{\"sequence\":1,\"action\":\"seek\",\"selection\":41,\"seconds\":120}",400);assert(!applied);
    send_command(token,"{\"sequence\":2,\"action\":\"seek\",\"selection\":42,\"seconds\":120}",200);assert(applied==1 && command.seconds==120);
    send_command(token,"{\"sequence\":2,\"action\":\"seek\",\"selection\":42,\"seconds\":120}",200);assert(applied==1); /* retry is acknowledged once */
    send_command(token,"{\"sequence\":4,\"action\":\"pause\",\"selection\":42}",409);assert(applied==1);
    send_command(token,"{\"sequence\":3,\"action\":\"search\",\"text\":\"pause\"}",200);assert(applied==2 && command.kind==MAXD_REMOTE_SEARCH && !strcmp(command.text,"pause"));
    send_command(token,"{\"sequence\":4,\"action\":\"add\",\"text\":\"https://evil.test\"}",400);assert(applied==2);
    send_command(token,"{\"sequence\":5,\"action\":\"add\",\"text\":\"abcdefghijk\"}",200);assert(applied==3 && command.kind==MAXD_REMOTE_ADD);
    state.live=1;send_command(token,"{\"sequence\":6,\"action\":\"seek\",\"selection\":42,\"seconds\":30}",400);assert(applied==3);
    state.busy=1;send_command(token,"{\"sequence\":7,\"action\":\"key\",\"text\":\"ENTER\"}",200);assert(strstr(response,"false") && applied==3);state.busy=0;
    send_command(token,"{\"sequence\":8,\"action\":\"capture\"}",200);assert(captured==1);
    assert(http("POST","/report",token,"{}",NULL)==200 && strstr(response,"No accounts"));
    assert(http("POST","/state",token,"{\"x\":1,\"x\":2}",NULL)==400);
    assert(http("POST","/state",token,"{}","Host: attacker.test\r\n")==403);
    clock_ms+=300001;assert(http("POST","/pair",NULL,body,NULL)==403);assert(http("POST","/state",token,"{}",NULL)==200);
    maxd_remote_stop();assert(!maxd_remote_paired());assert(maxd_remote_start("127.0.0.1",28667,clock_ms));
    assert(http("POST","/state",token,"{}",NULL)==401);assert(strcmp(invite,strchr(maxd_remote_join(),'#')+1));
    clock_ms+=300000;snprintf(body,sizeof body,"{\"join\":\"%s\"}",strchr(maxd_remote_join(),'#')+1);assert(http("POST","/pair",NULL,body,NULL)==403);
    maxd_remote_tick(clock_ms+7200001,&controls);assert(!maxd_remote_running());
    puts("PASS: phone HTTP pairing, one-use QR, auth, framing/origin checks, expiry/revocation, command scope/deduplication, search, queue and protected report download");
}
