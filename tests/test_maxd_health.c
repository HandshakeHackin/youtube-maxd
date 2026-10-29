/* Real provider rescue path and measured-health policy. GPL-3.0-or-later. */
#define MAXD_STREAM_HELPERS_ONLY
#include "test_stream_choice.c"
#include "maxd/playback_health.h"
static uint64_t tick,frames;
static void sample(int waiting,int paused) {tick+=1000;if(!waiting)frames+=30;tube_maxd_auto_sample(tick,frames,0,paused,waiting,42.75,0,1);}
int main(void) {
 char dir[]="/tmp/maxd-health-XXXXXX";assert(mkdtemp(dir));assert(!setenv("EVO_DATA_DIR_OVERRIDE",dir,1));
 const evo_provider_t *p=&evo_provider_youtube;assert(!p->init());
 assert(!p->set_source("http://127.0.0.1:8765#123456"));p->search("fixture",0,items_cb,NULL);
 reply(0,1,200,"{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");poll_job("{\"state\":\"done\",\"result\":{\"items\":[]}}");
 resolve_video("en-US.4",NULL);p->stream_chosen("abcdefghijk",&listed[0]);
 /* Pick a dubbed audio track before asking for a picture rescue. */
 assert(tube_player_audio_select(1,10.25,0,1)==1);
 evo_provider_item_t audio_item;double audio_pos;int audio_captions;
 assert(tube_take_recovery(&audio_item,&audio_pos,&audio_captions));
 resolve_video("de.3","\"audio_track\":\"de.3\"");
 int audio_choice=tube_recovery_choice(listed,listed_count);assert(audio_choice==0);
 p->stream_chosen("abcdefghijk",&listed[audio_choice]);tube_recovery_finish();
 unsigned owner=tube_phone_selection();assert(owner);char text[256];assert(tube_maxd_health_summary(text,sizeof text));
 assert(!tube_maxd_auto_enabled());for(int i=0;i<10;++i)sample(0,0);
 assert(tube_maxd_health_summary(text,sizeof text) && strstr(text,"stable"));
 for(int i=0;i<3;++i)sample(1,0);
 assert(tube_maxd_health_summary(text,sizeof text) && strstr(text,"buffering"));
 assert(!tube_recovery_pending()); /* Health measurement does not silently enable Auto Max. */
 assert(!tube_maxd_health_repair(owner+1,1,42.75,0,1));assert(!tube_maxd_health_repair(owner,1,NAN,0,1));
 assert(!tube_maxd_health_repair(owner,1,42.75,1,1));
 assert(tube_maxd_health_repair(owner,1,42.75,0,1));assert(tube_recovery_pending());assert(strstr(tube_maxd_health_action(),"AVC"));
 evo_provider_item_t item;double pos;int captions;assert(tube_take_recovery(&item,&pos,&captions));assert(pos==42.75 && captions);
 resolve_video("de.3","\"audio_track\":\"de.3\"");int chosen=tube_recovery_choice(listed,listed_count);assert(chosen==2); /* AVC 720p30, not AVC 1080p60 */
 p->stream_chosen("abcdefghijk",&listed[chosen]);tube_recovery_finish();assert(tube_player_quality_pinned()>=0);
 assert(tube_maxd_health_repair(owner,0,50.5,0,0));assert(tube_take_recovery(&item,&pos,&captions) && pos==50.5 && !captions);
 resolve_video("de.3","\"audio_track\":\"de.3\"");chosen=tube_recovery_choice(listed,listed_count);assert(chosen==3);
 p->stream_chosen("abcdefghijk",&listed[chosen]);tube_recovery_finish();assert(!tube_maxd_health_repair(owner,0,51,0,1));
 p->list_catalog("library:profile:alice",0,items_cb,NULL);assert(!tube_maxd_health_summary(text,sizeof text));
 assert(!tube_maxd_health_repair(owner,1,42.75,0,1));
 MaxdPlaybackHealth s={0};for(int i=0;i<10;++i)maxd_health_sample(&s,1000*i,30*i,4*i,0,0);
 assert(s.state==MAXD_HEALTH_DROPPING);maxd_health_sample(&s,10000,300,40,1,1);assert(s.state==MAXD_HEALTH_LEARNING);
 maxd_health_sample(&s,9000,1,0,0,0);assert(s.state==MAXD_HEALTH_LEARNING);
 p->shutdown();puts("PASS: observed health with Auto Max off, stale/profile/live guards, AVC compatibility constraints, resume/captions, manual pin, lower limit and pause/reset sampling");
}
