/* Bounded local preferences and malformed saved state. GPL-3.0-or-later. */
#include "maxd/feed_controls.h"
#include <assert.h>
static MaxdFeedStore store,restored;
int main(void) {
 MaxdFeedPrefs *p=maxd_feed_for(&store,"guest",1);assert(p);
 assert(!maxd_feed_for(&store,"bad/profile",1));assert(!maxd_feed_add(p,"   "));assert(!maxd_feed_add(p,"bad\nline"));
 assert(maxd_feed_add(p,"Spoilers"));assert(maxd_feed_add(p,"SPOILERS") && p->count==1);
 assert(maxd_feed_hides(p,"A spoilers video",NULL));assert(maxd_feed_hides(p,"Original","SPOILERS today"));
 assert(!maxd_feed_hides(p,"Unrelated",NULL));assert(maxd_feed_add(p,"Обзор"));assert(maxd_feed_hides(p,"Обзор игры",NULL));
 assert(!maxd_feed_add(p,"\xc0\xaf"));char large[100];memset(large,'a',sizeof large);large[99]=0;assert(!maxd_feed_add(p,large));
 for(int i=p->count;i<MAXD_FEED_WORDS;++i){char phrase[32];snprintf(phrase,sizeof phrase,"phrase %d",i);assert(maxd_feed_add(p,phrase));}
 assert(!maxd_feed_add(p,"one too many"));assert(maxd_feed_add(p,"spoilers"));p->followed_first=1;
 cJSON *root=cJSON_CreateObject();maxd_feed_write(&store,root);maxd_feed_read(&restored,root);cJSON_Delete(root);
 p=maxd_feed_for(&restored,"guest",0);assert(p && p->count==16 && p->followed_first);
 root=cJSON_Parse("{\"maxd_feed_controls\":[{\"profile\":\"guest\",\"muted\":[\"good\",2,\"good\"]},{\"profile\":\"guest\",\"followed_first\":true},{\"profile\":\"bad/profile\"}]}");
 maxd_feed_read(&restored,root);cJSON_Delete(root);assert(restored.count==1 && restored.rows[0].count==1 && !restored.rows[0].followed_first);
 root=cJSON_Parse("{\"maxd_feed_controls\":{\"row\":{\"profile\":\"guest\"}}}");maxd_feed_read(&restored,root);cJSON_Delete(root);assert(!restored.count);
 puts("PASS: feed phrase bounds, duplicates, UTF-8, malformed persistence, profile keys and first-row ownership");
}
