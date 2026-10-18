#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "maxd/party_server.h"
#include "cJSON.h"
static long long clock_ms=100000;
static char response[32000];
static int http(const char *method,const char *path,const char *token,const char *body,const char *extra) {
    int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(28666);inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    assert(!connect(fd,(struct sockaddr*)&addr,sizeof addr));char request[6000],auth[100]="";if(token)snprintf(auth,sizeof auth,"Authorization: Bearer %s\r\n",token);
    int len=snprintf(request,sizeof request,"%s %s HTTP/1.1\r\nHost: 127.0.0.1:28666\r\nContent-Type: application/json\r\n%s%sContent-Length: %zu\r\n\r\n%s",method,path,auth,extra?extra:"",strlen(body),body);
    assert(send(fd,request,(size_t)len,0)==len);assert(!fcntl(fd,F_SETFL,O_NONBLOCK));size_t used=0;
    for(int i=0;i<1000;++i) {
        maxd_party_tick(clock_ms);ssize_t n=recv(fd,response+used,sizeof response-used-1,0);
        if(n>0)used+=(size_t)n;else if(n==0)break;else assert(errno==EAGAIN || errno==EWOULDBLOCK);
        if(i==999)assert(!"party request timed out");
        usleep(1000);
    }
    close(fd);response[used]=0;int status=0;assert(sscanf(response,"HTTP/1.1 %d",&status)==1);return status;
}
static cJSON *json(void){char *start=strstr(response,"\r\n\r\n");assert(start);cJSON *o=cJSON_Parse(start+4);assert(o);return o;}
int main(void) {
    assert(!maxd_party_start("8.8.8.8",28666,clock_ms));assert(maxd_party_start("127.0.0.1",28666,clock_ms));
    char invite[33],token[33],body[200];snprintf(invite,sizeof invite,"%s",strchr(maxd_party_join(),'#')+1);
    assert(http("GET","/",NULL,"",NULL)==200 && strstr(response,"YouTube Max"));
    assert(http("POST","/state",NULL,"{}",NULL)==401);
    assert(http("POST","/pair",NULL,"{\"join\":\"wrong\"}",NULL)==403);
    snprintf(body,sizeof body,"{\"join\":\"%s\"}",invite);assert(http("POST","/pair",NULL,body,"Origin: http://evil.example\r\n")==403);
    assert(http("POST","/pair",NULL,body,NULL)==200);cJSON *o=json();snprintf(token,sizeof token,"%s",cJSON_GetObjectItem(o,"token")->valuestring);cJSON_Delete(o);
    assert(http("POST","/suggest",token,"{\"id\":\"abcdefghijk\",\"title\":\"A guest suggestion\"}",NULL)==200);
    assert(maxd_party_count()==1 && maxd_party_item(0)->votes==1 && !maxd_party_item(0)->approved);
    clock_ms+=1100;assert(http("POST","/vote",token,"{\"id\":\"abcdefghijk\"}",NULL)==200);assert(maxd_party_item(0)->votes==1);
    assert(http("POST","/suggest",token,"{\"id\":\"bad\"}",NULL)==400);
    assert(http("POST","/state",token,"{}",NULL)==200);o=json();assert(cJSON_GetArraySize(cJSON_GetObjectItem(o,"items"))==1);cJSON_Delete(o);
    assert(maxd_party_approve(0,1) && maxd_party_item(0)->approved);assert(maxd_party_approve(0,0));
    clock_ms+=1100;assert(http("POST","/vote",token,"{\"id\":\"abcdefghijk\"}",NULL)==409);
    assert(http("POST","/state",token,"{}","Origin: null\r\n")==403);
    clock_ms+=600001;snprintf(body,sizeof body,"{\"join\":\"%s\"}",invite);assert(http("POST","/pair",NULL,body,NULL)==403);
    maxd_party_stop();assert(!maxd_party_running() && !maxd_party_count());assert(maxd_party_start("127.0.0.1",28666,clock_ms));
    assert(http("POST","/state",token,"{}",NULL)==401);assert(strcmp(invite,strchr(maxd_party_join(),'#')+1));maxd_party_tick(clock_ms+7200001);assert(!maxd_party_running());
    puts("PASS: real LAN HTTP page, pairing, origin rejection, authentication, suggestions, single votes, host approval, rejection, invite expiry, token revocation and room expiry");
}
