/* Shipping provider/catalog/fetch scope, staging, failures and late callbacks. */
#define main native_provider_regression_main
#include "test_native.c"
#undef main
#include <sys/stat.h>
#include <time.h>
static int64_t caption_clock = 100000;
int tube_test_clock_gettime(clockid_t clock, struct timespec *value) {
    assert(clock == CLOCK_MONOTONIC);
    value->tv_sec = caption_clock / 1000; value->tv_nsec = caption_clock % 1000 * 1000000; return 0;
}
static const char *cue = "1\n00:00:00,000 --> 00:00:02,000\nCaption cue\n\n";
static void resolved_tracks(void) {
    evo_provider_youtube.resolve("abcdefghijk",resolve_cb,NULL);
    reply(0,1,200,"{\"job\":\"fixture_job\",\"state\":\"pending\"}");
    caption_clock += 500; tube_poll();
    reply(0,1,200,"{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"choices\":[{\"url\":\"http://127.0.0.1:8765/media/session/video\"}],\"captions\":[{\"language\":\"en\",\"name\":\"English\",\"url\":\"http://127.0.0.1:8765/media/session/en.srt\"},{\"language\":\"fr\",\"name\":\"French\",\"automatic\":true,\"url\":\"http://127.0.0.1:8765/media/session/fr.srt\"},{\"language\":\"de\",\"url\":\"http://127.0.0.1:8765/media/session/de.srt\"},{\"language\":\"fr\",\"url\":\"http://127.0.0.1:8765/media/session/duplicate.srt\"},{\"language\":\"x\",\"url\":\"http://evil.test/track.srt\"},{\"language\":\"../bad\",\"url\":\"http://127.0.0.1:8765/media/session/bad.srt\"}]}}");
    evo_stream_choice_t choice = {0}; strcpy(choice.url,"http://127.0.0.1:8765/media/session/video");
    evo_provider_youtube.stream_chosen("abcdefghijk",&choice);
}
int main(void) {
    char dir[]="/tmp/prosperotube-caption-switch-XXXXXX";
    assert(mkdtemp(dir)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
    cJSON *seed=cJSON_Parse("{\"version\":2,\"source\":\"http://127.0.0.1:8765\",\"token\":\"synthetic-token\",\"captions\":false,\"records\":[]}");
    assert(seed); write_store(seed); cJSON_Delete(seed); assert(!evo_provider_youtube.init());
    assert(!tube_caption_track_count() && !tube_caption_switch_begin("fr",0,0));
    resolved_tracks(); assert(last_ok && tube_caption_track_count()==3);
    assert(tube_caption_selection()==tube_phone_selection() && tube_caption_selection()!=0);
    assert(!strcmp(tube_caption_track_name(1),"French") && tube_caption_track_automatic(1));
    assert(!*tube_caption_track_language(-1) && !*tube_caption_track_language(3));
    assert(!tube_caption_switch_begin("missing",0,0) && !tube_caption_switch_begin("fr",123,1));
    unsigned revision=tube_caption_revision();
    assert(tube_caption_switch_begin("fr",0,0)==2 && tube_caption_switch_local());
    assert(!tube_caption_switch_begin("de",0,0));
    const char *media=""; assert(!tube_caption_switch_status(&media) && !*media);
    assert(!*tube_phone_caption_language()); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1 && strstr(media,"caption-a.mp4"));
    assert(!*tube_phone_caption_language()); tube_caption_switch_finish(1);
    assert(!strcmp(tube_phone_caption_language(),"fr") && tube_caption_revision()!=revision);
    char original[512]; strcpy(original,tube_caption_media_path());
    assert(tube_caption_switch_begin("en",0,0)==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1 && strstr(media,"caption-b.mp4"));
    tube_caption_switch_finish(0); assert(!strcmp(tube_caption_media_path(),original));
    assert(tube_caption_switch_begin("de",0,0)==2); reply(0,0,503,"");
    assert(tube_caption_switch_status(&media)==-1); tube_caption_switch_finish(0);
    assert(!strcmp(tube_phone_caption_language(),"fr"));
    /* Failed file creation must preserve the current file and identity. */
    char blocked[512]; strcpy(blocked,evo_data_path("caption-b.srt.tmp")); assert(!mkdir(blocked,0700));
    assert(tube_caption_switch_begin("de",0,0)==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==-1); tube_caption_switch_finish(0); assert(!rmdir(blocked));
    /* A timed-out callback cannot overwrite the next explicit attempt. */
    assert(tube_caption_switch_begin("de",0,0)==2); Pending expired=pending[0]; pending_count=0;
    caption_clock += 16000; tube_poll(); assert(tube_caption_switch_status(&media)==-1); tube_caption_switch_finish(0);
    assert(tube_caption_switch_begin("en",0,0)==2);
    expired.cb(1,200,cue,strlen(cue),expired.ud); assert(tube_caption_switch_status(&media)==0);
    reply(0,1,200,cue); assert(tube_caption_switch_status(&media)==1); tube_caption_switch_finish(1);
    assert(!strcmp(tube_phone_caption_language(),"en"));
    /* Independent staging cannot overwrite primary/active secondary files. */
    assert(!tube_caption_secondary_begin("en"));
    assert(tube_caption_secondary_begin("fr")==2 && tube_caption_switch_secondary());
    reply(0,1,200,cue); assert(tube_caption_switch_status(&media)==1);
    assert(strstr(media,"caption-secondary-a.mp4")); tube_caption_switch_finish(1);
    assert(!strcmp(tube_caption_secondary_language(),"fr") && !strcmp(tube_phone_caption_language(),"en"));
    assert(strstr(tube_caption_secondary_media_path(),"caption-secondary-a.mp4"));
    /* Fresh resolution of the same scoped recovery retains selected sidecars. */
    char saved_primary[512], saved_secondary[512];
    strcpy(saved_primary,tube_caption_media_path());strcpy(saved_secondary,tube_caption_secondary_media_path());
    unsigned recovery_selection=tube_caption_selection();
    assert(tube_queue_recovery(42.25,0,1));
    evo_provider_item_t recovery_item;double recovery_position;int recovery_enabled;
    assert(tube_take_recovery(&recovery_item,&recovery_position,&recovery_enabled));
    assert(recovery_position==42.25 && recovery_enabled);
    resolved_tracks();
    assert(tube_caption_selection()==recovery_selection);
    assert(!strcmp(tube_phone_caption_language(),"en") && !strcmp(tube_caption_secondary_language(),"fr"));
    assert(!strcmp(tube_caption_media_path(),saved_primary) && !strcmp(tube_caption_secondary_media_path(),saved_secondary));
    tube_recovery_finish();
    /* A disappeared sidecar cannot be advertised after fresh recovery. */
    FILE *empty=fopen(evo_data_path("caption-secondary-a.srt"),"wb"); assert(empty); assert(!fclose(empty));
    assert(tube_queue_recovery(43.5,0,1));
    assert(tube_take_recovery(&recovery_item,&recovery_position,&recovery_enabled));
    resolved_tracks();
    assert(!*tube_caption_secondary_language() && !*tube_caption_secondary_media_path());
    assert(!strcmp(tube_phone_caption_language(),"en")); tube_recovery_finish();
    assert(tube_caption_secondary_begin("fr")==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1); tube_caption_switch_finish(1);
    assert(tube_caption_secondary_begin("de")==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1 && strstr(media,"caption-secondary-"));
    tube_caption_switch_finish(0); assert(!strcmp(tube_caption_secondary_language(),"fr"));
    assert(tube_caption_switch_begin("fr",0,0)==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1); tube_caption_switch_finish(1);
    assert(!*tube_caption_secondary_language());
    assert(tube_caption_switch_begin("en",0,0)==2); reply(0,1,200,cue);
    assert(tube_caption_switch_status(&media)==1); tube_caption_switch_finish(1);
    /* Same-ID manual playback is a new selection; old callbacks cannot apply. */
    assert(tube_caption_switch_begin("de",0,0)==2); Pending reopened=pending[0]; pending_count=0;
    unsigned selection=tube_phone_selection();
    evo_stream_choice_t same={0}; strcpy(same.url,"http://127.0.0.1:8765/media/session/video");
    evo_provider_youtube.stream_chosen("abcdefghijk",&same); assert(tube_phone_selection()!=selection);
    reopened.cb(1,200,cue,strlen(cue),reopened.ud); assert(tube_caption_switch_status(&media)==-1);
    assert(!strcmp(tube_phone_caption_language(),"en"));
    /* Profile retirement discards late data and never leaks a catalog. */
    assert(tube_caption_switch_begin("de",0,0)==2); Pending retired=pending[0]; pending_count=0;
    evo_provider_youtube.list_catalog("library:profile:other",0,items_cb,NULL);
    assert(!tube_caption_track_count() && tube_caption_switch_status(&media)==-1);
    assert(!tube_caption_selection());
    retired.cb(1,200,cue,strlen(cue),retired.ud);
    assert(tube_caption_switch_status(&media)==-1);
    evo_provider_youtube.list_catalog("library:profile:guest",0,items_cb,NULL);
    resolved_tracks(); assert(!*tube_phone_caption_language() && tube_caption_track_count()==3);
    evo_provider_youtube.shutdown(); assert(!tube_caption_track_count());
    assert(!unlink(evo_data_path("youtube.json"))); assert(!unlink(evo_data_path("caption-a.srt")));
    assert(!unlink(evo_data_path("caption-b.srt")));
    assert(!unlink(evo_data_path("caption-secondary-a.srt")));
    assert(!unlink(evo_data_path("caption-secondary-b.srt"))); assert(!rmdir(dir));
    puts("Caption catalogs, staged switching, failure preservation, timeout and scope retirement passed"); return 0;
}
