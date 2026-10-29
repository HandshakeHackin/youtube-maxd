/* Shipping-provider coverage: persistence, profiles, title filters and paging. GPL-3.0-or-later. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
static const evo_provider_t *p;
static const char *videos="{\"state\":\"done\",\"result\":{\"items\":["
 "{\"id\":\"abcdefghijk\",\"title\":\"Spoiler alert\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},"
 "{\"id\":\"bcdefghijkl\",\"title\":\"Cooking\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"},"
 "{\"id\":\"cdefghijklm\",\"title\":\"Third\",\"dearrow_title\":\"A SPOILER\"}]}}";
static void show(const char *id) {assert(!p->list_catalog(id,0,items_cb,NULL));assert(last_ok);}
static void mute(const char *text) {
 show("settings:feed:add");tube_playlist_prompt_t q;char next[160];assert(tube_take_playlist_prompt(&q));
 assert(!tube_submit_playlist_name(q.ticket,text,next,sizeof next));show(next);
}
static void search(void) {assert(!p->search("fixture",0,items_cb,NULL));poll_job(videos);assert(last_ok);}
int main(void) {
 char dir[]="/tmp/maxd-feed-XXXXXX";assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
 p=&evo_provider_youtube;assert(!p->init());assert(!p->set_source("http://127.0.0.1:8765#123456"));
 p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(videos);
 assert(last_count==3);assert(tube_toggle_favorite("abcdefghijk"));assert(tube_toggle_queue("abcdefghijk"));
 mute("spoiler");search();assert(last_count==1 && !strcmp(last_items[0].id,"bcdefghijkl"));
 show("favorites");assert(last_count==1);show("queue");assert(last_count==1);
 show("settings:feed");char stale[160];strcpy(stale,last_items[0].id);
 show("library:profile:alice");p->list_catalog(stale,0,items_cb,NULL);assert(!last_ok);
 search();assert(last_count==3);show("library:profile:guest");search();assert(last_count==1);
 p->shutdown();assert(!p->init());search();assert(last_count==1);
 show("settings:feed");assert(last_count==4);char remove[160];strcpy(remove,last_items[3].id);
 show(remove);search();assert(last_count==3);p->list_catalog(remove,0,items_cb,NULL);assert(!last_ok);
 /* A non-blank literal phrase is required, with a bounded byte length. */
 show("settings:feed:add");tube_playlist_prompt_t q;char next[160];assert(tube_take_playlist_prompt(&q));
 assert(tube_submit_playlist_name(q.ticket,"   ",next,sizeof next)<0);
 assert(tube_submit_playlist_name(q.ticket,"bad\nphrase",next,sizeof next)<0);
 show("settings:feed");char priority[160];strcpy(priority,last_items[0].id);show(priority);
 assert(tube_toggle_subscription("bcdefghijkl"));
 /* Search relevance is retained despite follow prioritization. */
 search();assert(!strcmp(last_items[0].id,"abcdefghijk"));
 p->list_catalog("home-topic:gaming",0,items_cb,NULL);
 while(pending_count && strstr(pending[0].url,"/v1/health"))reply(0,1,200,"{\"api_version\":1,\"features\":[\"search\",\"library\"]}");
 poll_job(videos);assert(last_ok && last_count==3 && !strcmp(last_items[0].id,"bcdefghijkl"));
 /* A prompt cannot be submitted to another profile. */
 show("settings:feed:add");assert(tube_take_playlist_prompt(&q));show("library:profile:alice");
 assert(tube_submit_playlist_name(q.ticket,"Cooking",next,sizeof next)<0);
 p->shutdown();puts("PASS: feed title/DeArrow muting, reversible rules, saved lists, profiles, persistence, local-follow priority and stale actions");
}
