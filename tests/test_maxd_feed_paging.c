/* Muted library pages retain cursors and have a bounded read-ahead. GPL-3.0-or-later. */
#define MAXD_LIBRARY_HELPERS_ONLY
#include "test_library_paging.c"
static void mute_title(void) {
 p->list_catalog("settings:feed:add",0,items_cb,NULL);tube_playlist_prompt_t q;char next_id[160];
 assert(tube_take_playlist_prompt(&q));assert(!tube_submit_playlist_name(q.ticket,"Video",next_id,sizeof next_id));
}
int main(void) {
 (void)next;(void)refused;
 char dir[]="/tmp/maxd-feed-pages-XXXXXX";assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
 p=&evo_provider_youtube;assert(!p->init());assert(!p->set_source("http://127.0.0.1:8765#123456"));
 p->list_catalog("library:profile:fixture",0,items_cb,NULL);p->list_catalog("library:subscriptions",0,items_cb,NULL);
 for(int i=0;i<4 && pending_count==1 && !direct();++i) {
  if(strstr(pending[0].url,"/v1/pair"))reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");else settle();
 }
 poll_job(PAGE("abcdefghij1","false",""));mute_title();
 p->list_catalog("library:home",0,items_cb,NULL);settle();assert(direct());int calls=item_calls;
 poll_job(PAGE("abcdefghij1","true","afterMuted"));assert(item_calls==calls && direct() && !strcmp(field("cursor"),"afterMuted"));
 poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghij2\",\"title\":\"Cooking\"}],\"has_more\":false}}");
 assert(item_calls==calls+1 && last_count==1 && !strcmp(last_items[0].title,"Cooking") && !last_more);
 first("library:home",PAGE("abcdefghij1","false",""));
 assert(last_count==1 && !strcmp(last_items[0].id,"settings:feed") && !strstr(last_items[0].title,"signed-in"));
 p->list_catalog("library:home",0,items_cb,NULL);settle();calls=item_calls;
 for(int i=0;i<9;++i)poll_job(PAGE("abcdefghij1","true","boundedCursor"));
 assert(item_calls==calls+1 && !pending_count && !last_count && last_more);
 /* Watch later is a saved list and is not hidden by a title phrase. */
 first("library:watch_later",PAGE("abcdefghij1","false",""));assert(last_count==1 && !strcmp(last_items[0].id,"abcdefghij1"));
 p->shutdown();puts("PASS: title-muted Home pagination, no false sign-in notice, bounded empty-page traversal and saved Watch later");
}
