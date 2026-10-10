#define main existing_provider_regression
#include "test_native.c"
#undef main
static const char *page="{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Original title\",\"art_url\":\"https://i.ytimg.com/original.jpg\",\"dearrow_title\":\"Community title\",\"dearrow_art_url\":\"http://127.0.0.1:8765/art/dearrow/test\"}],\"has_more\":false}}";
int main(void) {
 char temporary[]="/tmp/tube-dearrow-XXXXXX";assert(mkdtemp(temporary)&&setenv("EVO_DATA_DIR_OVERRIDE",temporary,1)==0);
 const evo_provider_t*p=&evo_provider_youtube;assert(p->init()==0);
 assert(p->set_source("http://127.0.0.1:8765#123456")==0);
 p->search("fixture",0,items_cb,NULL);reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job(page);
 assert(!strcmp(last_items[0].title,"Original title")&&!strcmp(last_items[0].art_url,"https://i.ytimg.com/original.jpg"));
 assert(tube_toggle_favorite("abcdefghijk")==1);
 p->list_catalog("settings:dearrow:titles",0,items_cb,NULL);assert(last_ok&&last_count==3);
 p->list_catalog("favorites",0,items_cb,NULL);assert(!strcmp(last_items[0].title,"Community title"));
 p->list_catalog("settings:dearrow:thumbnails",0,items_cb,NULL);
 p->search("fixture",0,items_cb,NULL);assert(strstr(pending[0].body,"\"dearrow_titles\":true")&&strstr(pending[0].body,"\"dearrow_thumbnails\":true"));poll_job(page);
 assert(!strcmp(last_items[0].title,"Community title")&&strstr(last_items[0].art_url,"/art/dearrow/"));
 p->report_progress("abcdefghijk",1,100,EVO_PROVIDER_PLAY_STOP);
 cJSON*saved=read_store();cJSON*row=cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(saved,"records"),0);
 assert(!strcmp(cJSON_GetObjectItemCaseSensitive(row,"title")->valuestring,"Original title"));
 assert(!cJSON_GetObjectItemCaseSensitive(row,"dearrow_art_url"));cJSON_Delete(saved);
 evo_stream_choice_t choice={0};snprintf(choice.url,sizeof choice.url,"http://127.0.0.1:8765/media/test/a.mpd");
 p->stream_chosen("abcdefghijk",&choice);assert(tube_queue_recovery(2,0,0));
 evo_provider_item_t resumed;double position;int subtitles;
 assert(tube_take_recovery(&resumed,&position,&subtitles)&&!strcmp(resumed.title,"Community title"));tube_recovery_finish();
 assert(tube_toggle_queue("abcdefghijk")==1);p->list_catalog("settings:autoplay",0,items_cb,NULL);
 p->report_progress("12345678901",1,10,EVO_PROVIDER_PLAY_STOP);p->stream_chosen("12345678901",&choice);
 assert(tube_finish_drained_and_next(10)&&tube_take_next(&resumed)&&!strcmp(resumed.title,"Community title"));
 assert(p->init()==0);p->list_catalog("favorites",0,items_cb,NULL);
 assert(!strcmp(last_items[0].title,"Community title")&&!strcmp(last_items[0].art_url,"https://i.ytimg.com/original.jpg"));
 p->list_catalog("settings:dearrow:titles",0,items_cb,NULL);p->list_catalog("favorites",0,items_cb,NULL);
 assert(!strcmp(last_items[0].title,"Original title"));
 p->list_catalog("settings:dearrow:titles",0,items_cb,NULL);
 assert(p->set_source("http://127.0.0.1:9000#123456")==0);p->list_catalog("favorites",0,items_cb,NULL);
 assert(!strcmp(last_items[0].title,"Original title")&&!strcmp(last_items[0].art_url,"https://i.ytimg.com/original.jpg"));
 puts("PASS: shipping DeArrow independent settings, persisted originals, local display, reload and source isolation");return 0;
}
