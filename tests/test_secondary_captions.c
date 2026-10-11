/* Actual generated EVO engine: independent external SRTs and atomic failure. */
#define main evo_dualsub_main
#include "../third_party/evo/tools/dualsub_host.c"
#undef main
#include <math.h>

int main(int argc, char **argv) {
    if (evo_dualsub_main(argc, argv)) return 1;
    const char *dir = argc > 1 ? argv[1] : ".";
    char primary[512], secondary[512], sidecar[512];
    snprintf(primary, sizeof primary, "%s/primary.mp4", dir);
    snprintf(secondary, sizeof secondary, "%s/secondary.mp4", dir);
    snprintf(sidecar, sizeof sidecar, "%s/secondary.srt", dir);
    char primary_srt[512]; snprintf(primary_srt,sizeof primary_srt,"%s/primary.srt",dir);
    write_srt(primary_srt);
    play_fmt=avformat_alloc_context(); CHECK(play_fmt != NULL,"playback context allocated");
    snprintf(current_media_path,sizeof current_media_path,"%s",primary);
    CHECK(prospero_subtitle_load_for_media(primary)==3,"primary cues loaded");
    CHECK(prospero_subtitle_select_primary(-1),"primary SRT selected");
    prospero_subtitle_delay_ms=250; prospero_subtitle_time_scale=1.25;
    audio_clock_seconds=42.125; player_paused=1;
    FILE *f=fopen(sidecar,"wb"); CHECK(f != NULL,"secondary file opened");
    if(f){fputs("1\n00:00:01,000 --> 00:00:03,000\nBonjour\n\n",f);fclose(f);}
    CHECK(tube_secondary_subtitle_load(secondary)==1,"independent secondary parsed");
    CHECK(prospero_secondary_subtitle_active(),"two external subtitle banks active");
    char text[768];
    CHECK(prospero_secondary_subtitle_text_at(2,text,sizeof text) && !strcmp(text,"Bonjour"),"second line rendered");
    const ProsperoSubtitleCue *cue=prospero_subtitle_active_cue(2);
    CHECK(cue && !strcmp(cue->text,"SRT one"),"primary remains unchanged");
    CHECK(prospero_subtitle_delay_ms==250 && prospero_subtitle_time_scale==1.25,"primary timing retained");
    CHECK(audio_clock_seconds==42.125 && player_paused==1,"playback clock/pause untouched");
    prospero_secondary_nudge_delay(2000);
    CHECK(!prospero_secondary_subtitle_text_at(2,text,sizeof text),"independent secondary delay");
    CHECK(prospero_secondary_subtitle_text_at(4,text,sizeof text),"delayed cue uses media clock");
    f=fopen(sidecar,"wb"); if(f){fputs("1\n00:00:01,000 --> 00:00:03,000\nHola\n\n",f);fclose(f);}
    for(int i=0;i<20;++i) CHECK(tube_secondary_subtitle_load(secondary)==1,"successful replacements retire previous bank");
    CHECK(prospero_secondary_subtitle_text_at(2,text,sizeof text) && !strcmp(text,"Hola"),"successful replacement publishes new cues");
    CHECK(prospero_secondary_delay_ms==0,"new bank starts with its own timing");
    prospero_secondary_nudge_delay(2000);
    CHECK(!tube_secondary_subtitle_load("/missing/secondary.mp4"),"missing replacement rejected");
    f=fopen(sidecar,"wb");if(f){fputs("invalid\n",f);fclose(f);}
    CHECK(!tube_secondary_subtitle_load(secondary),"invalid replacement rejected");
    CHECK(prospero_secondary_subtitle_text_at(4,text,sizeof text) && !strcmp(text,"Hola"),"failed replacement preserves old line");
    CHECK(prospero_secondary_delay_ms==2000,"failed replacement preserves delay");
    CHECK(prospero_subtitle_select_primary(-1) && prospero_secondary_subtitle_active(),"primary reselection preserves independent secondary");
    CHECK(!prospero_secondary_subtitle_text_at(NAN,text,sizeof text),"nonfinite clock rejected");
    CHECK(!prospero_secondary_subtitle_text_at(50,text,sizeof text),"forward seek outside cues produces no stale text");
    CHECK(prospero_secondary_subtitle_text_at(4,text,sizeof text),"rewind returns the appropriate cue");
    strcpy(current_media_path,"other.mp4");
    CHECK(!prospero_secondary_subtitle_active(),"different media cannot inherit second line");
    prospero_embedded_subtitle_close();
    CHECK(!prospero_secondary_subtitle_active(),"media teardown clears second bank");
    prospero_subtitle_clear(); avformat_free_context(play_fmt);play_fmt=NULL;
    remove(sidecar);remove(primary_srt);
    fprintf(stderr,"%d total checks passed, %d failed\n",g_pass,g_fail);
    return g_fail ? 1 : 0;
}
