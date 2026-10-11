#define main existing_provider_regression
#include "test_native.c"
#undef main
static void page(const evo_provider_t *p,double percent) {
    p->search("progress",0,items_cb,NULL);
    char body[512];snprintf(body,sizeof body,"{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"duration_sec\":100,\"watch_percent\":%g}]}}",percent);
    poll_job(body);assert(last_ok && last_count==1);
}
int main(void) {
    char dir[]="/tmp/tube-progress-XXXXXX";assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *root=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\",\"records\":[]}");
    write_store(root);cJSON_Delete(root);const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
    page(p,50);assert(last_items[0].resume_pos_sec==50 && !last_items[0].played);
    page(p,10);assert(!last_items[0].resume_pos_sec && !last_items[0].played);
    page(p,100);assert(!last_items[0].resume_pos_sec && last_items[0].played);
    p->report_progress("abcdefghijk",37,100,EVO_PROVIDER_PLAY_STOP);
    page(p,75);assert(last_items[0].resume_pos_sec==75 && !last_items[0].played);
    page(p,100);assert(last_items[0].resume_pos_sec==37);
    page(p,-1);assert(last_items[0].resume_pos_sec==37);
    page(p,0);assert(last_items[0].resume_pos_sec==0);
    page(p,101);assert(last_items[0].resume_pos_sec==37);
    p->shutdown();unlink(evo_data_path("youtube.json"));assert(!rmdir(dir));
    puts("PASS: catalog progress thresholds, remote/local precedence and local history preservation");
}
