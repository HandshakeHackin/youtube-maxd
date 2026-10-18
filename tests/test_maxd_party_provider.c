#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "maxd/party_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
static char result[24000];
static void choose(const char *id){evo_provider_youtube.list_catalog(id,0,items_cb,NULL);assert(last_ok);}
static void post(const char *path,const char *token,const char *body) {
    int fd=socket(AF_INET,SOCK_STREAM,0);assert(fd>=0);struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_port=htons(8766);assert(inet_pton(AF_INET,maxd_party_address(),&address.sin_addr)==1);assert(!connect(fd,(struct sockaddr*)&address,sizeof address));
    char request[2048],auth[100]="";if(token)snprintf(auth,sizeof auth,"Authorization: Bearer %s\r\n",token);
    int len=snprintf(request,sizeof request,"POST %s HTTP/1.1\r\nHost: %s:8766\r\nContent-Type: application/json\r\n%sContent-Length: %zu\r\n\r\n%s",path,maxd_party_address(),auth,strlen(body),body);assert(send(fd,request,(size_t)len,0)==len);fcntl(fd,F_SETFL,O_NONBLOCK);size_t used=0;
    for(int i=0;i<1000;++i){tube_poll();ssize_t n=recv(fd,result+used,sizeof result-used-1,0);if(n>0)used+=(size_t)n;else if(!n)break;else assert(errno==EAGAIN || errno==EWOULDBLOCK);usleep(1000);}
    close(fd);result[used]=0;assert(strstr(result,"HTTP/1.1 200"));
}
int main(void) {
    char dir[]="/tmp/maxd-party-provider-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());SEPARATE_SETTINGS();
    choose("settings:party:start");assert(maxd_party_running());char body[160],token[33];snprintf(body,sizeof body,"{\"join\":\"%s\"}",strchr(maxd_party_join(),'#')+1);post("/pair",NULL,body);
    cJSON *json=cJSON_Parse(strstr(result,"\r\n\r\n")+4);assert(json);snprintf(token,sizeof token,"%s",cJSON_GetObjectItem(json,"token")->valuestring);cJSON_Delete(json);
    post("/suggest",token,"{\"id\":\"abcdefghijk\",\"title\":\"Party fixture\"}");choose("queue");assert(!last_count);
    choose("settings:party:approve:abcdefghijk");choose("queue");assert(last_count==1 && !strcmp(last_items[0].id,"abcdefghijk"));
    usleep(1100000);post("/suggest",token,"{\"id\":\"bcdefghijkl\",\"title\":\"Second suggestion\"}");
    choose("settings:party:approve:bcdefghijkl");choose("queue");assert(last_count==2 && !strcmp(last_items[0].id,"abcdefghijk"));
    snprintf(body,sizeof body,"{\"join\":\"%s\"}",strchr(maxd_party_join(),'#')+1);post("/pair",NULL,body);
    json=cJSON_Parse(strstr(result,"\r\n\r\n")+4);assert(json);snprintf(token,sizeof token,"%s",cJSON_GetObjectItem(json,"token")->valuestring);cJSON_Delete(json);
    post("/vote",token,"{\"id\":\"bcdefghijkl\"}");tube_poll();choose("queue");assert(last_count==2 && !strcmp(last_items[0].id,"bcdefghijkl"));
    assert(tube_toggle_queue("abcdefghijk")==0);assert(!maxd_party_item(0)->rejected); /* host removes an approved entry */
    maxd_party_approve(0,1);tube_poll();choose("queue");assert(last_count==1 && !strcmp(last_items[0].id,"bcdefghijkl")); /* later room revisions must not re-add it */
    choose("library:profile:maxd_family_1");tube_poll();assert(!maxd_party_running());p->shutdown();
    puts("PASS: party host approval writes the local queue, votes reorder pending entries, removal is preserved and profile changes stop the room");
}
