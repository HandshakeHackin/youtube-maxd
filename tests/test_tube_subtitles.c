/* Reuse pinned EVO's host stubs and test the actual engine, not a mock. */
#define main evo_dualsub_main
#include "../third_party/evo/tools/dualsub_host.c"
#undef main
#include "tube_subtitles.h"
#include "tube_caption_apply.h"
static int switch_state, switch_applied, switch_secondary;
static char switch_media[512], active_media[512], secondary_media[512];
int tube_caption_switch_status(const char **media) { *media = switch_media; return switch_state; }
void tube_caption_switch_finish(int applied) { switch_applied = applied; switch_state = -1; if (applied && !switch_secondary) strcpy(active_media,switch_media); }
const char *tube_caption_media_path(void) { return active_media; }
int tube_caption_switch_secondary(void) { return switch_secondary; }
const char *tube_caption_switch_language(void) { return "fr"; }
const char *tube_caption_secondary_language(void) { return ""; }
const char *tube_caption_secondary_media_path(void) { return secondary_media; }

int main(int argc, char **argv) {
    if (evo_dualsub_main(argc, argv)) return 1;
    char media[512], sidecar[512];
    const char *dir = argc > 1 ? argv[1] : ".";
    snprintf(media, sizeof media, "%s/tube.mkv", dir);
    snprintf(sidecar, sizeof sidecar, "%s/tube.srt", dir);
    write_srt(sidecar);
    prospero_subtitle_enabled = 0;
    prospero_subtitle_use_external = 0;
    prospero_subtitle_delay_ms = 5000;
    prospero_subtitle_time_scale = 2.0;
    int cues = prospero_subtitle_load_for_media(media);
    CHECK(cues == 3, "YouTube sidecar loaded");
    CHECK(tube_subtitle_activate(cues), "caption preference activates downloaded cues");
    CHECK(prospero_subtitle_enabled && prospero_subtitle_use_external,
          "loaded sidecar becomes the enabled primary track");
    CHECK(prospero_subtitle_delay_ms == 0 && prospero_subtitle_time_scale == 1.0,
          "previous video's timing does not leak");
    const ProsperoSubtitleCue *cue =
        prospero_subtitle_active_cue(prospero_subtitle_position(2.0));
    CHECK(cue && !strcmp(cue->text, "SRT one"), "caption available at media time");
    CHECK(!prospero_subtitle_active_cue(prospero_subtitle_position(0.0)),
          "no caption before first cue");
    CHECK(!prospero_subtitle_active_cue(prospero_subtitle_position(12.0)),
          "no caption after last cue");
    CHECK(!tube_subtitle_activate(0), "no caption track stays off");
    CHECK(!prospero_subtitle_enabled && !prospero_subtitle_use_external,
          "disabled captions do not inherit previous enabled state");
    remove(sidecar);
    CHECK(prospero_subtitle_load_for_media(media) == 0, "missing sidecar handled");
    CHECK(!tube_subtitle_activate(0), "missing captions stay off");
    CHECK(tube_subtitle_activate(3) == 0 && !prospero_subtitle_enabled,
          "failed selection cannot enable an empty track");
    /* The real parser/application bridge switches without changing playback clocks. */
    write_srt(sidecar); strcpy(active_media,media);
    CHECK(tube_subtitle_activate(prospero_subtitle_load_for_media(media)), "original track active before language switch");
    char next_sidecar[512]; snprintf(switch_media,sizeof switch_media,"%s/switch.mp4",dir);
    snprintf(next_sidecar,sizeof next_sidecar,"%s/switch.srt",dir);
    FILE *translated=fopen(next_sidecar,"wb"); CHECK(translated!=NULL,"translated sidecar opened");
    if (translated) { fputs("1\n00:00:01,000 --> 00:00:03,000\nBonjour\n\n",translated); fclose(translated); }
    audio_clock_seconds=42.125; player_paused=1;
    prospero_subtitle_delay_ms=250; prospero_subtitle_time_scale=1.25;
    switch_state=0; CHECK(!tube_caption_apply_ready(),"pending transfer does not apply a track");
    switch_state=1; CHECK(tube_caption_apply_ready()==1 && switch_applied,"valid translated track applied");
    cue=prospero_subtitle_active_cue(2.0); CHECK(cue && !strcmp(cue->text,"Bonjour"),"translated cue rendered by real engine");
    CHECK(audio_clock_seconds==42.125 && player_paused==1,"language switch preserves pause and media clock");
    CHECK(prospero_subtitle_delay_ms==250 && prospero_subtitle_time_scale==1.25,"language switch preserves timing adjustments");
    char good_media[512]; strcpy(good_media,active_media);
    snprintf(switch_media,sizeof switch_media,"%s/missing.mp4",dir); switch_state=1;
    CHECK(tube_caption_apply_ready()==-1 && !switch_applied,"invalid new file is not committed");
    cue=prospero_subtitle_active_cue(2.0); CHECK(cue && !strcmp(cue->text,"Bonjour"),"failed switch restores actual old cues");
    CHECK(!strcmp(active_media,good_media) && prospero_subtitle_enabled && prospero_subtitle_use_external,"failed switch retains old enabled track");
    CHECK(audio_clock_seconds==42.125 && player_paused==1 && prospero_subtitle_delay_ms==250 && prospero_subtitle_time_scale==1.25,"failed switch preserves playback and timing");
    /* Exercise the actual apply bridge for a second independent caption line. */
    play_fmt=avformat_alloc_context(); CHECK(play_fmt!=NULL,"secondary playback context allocated");
    snprintf(current_media_path,sizeof current_media_path,"%s",good_media);
    strcpy(switch_media,media); switch_secondary=1; switch_state=1;
    prospero_secondary_delay_ms=100;
    CHECK(tube_caption_apply_ready()==1 && switch_applied,"second language applied by actual bridge");
    char second_text[768];
    CHECK(prospero_secondary_subtitle_text_at(2,second_text,sizeof second_text) && !strcmp(second_text,"SRT one"),"second line comes from independent cues");
    cue=prospero_subtitle_active_cue(2);CHECK(cue && !strcmp(cue->text,"Bonjour"),"primary translation survives secondary application");
    CHECK(prospero_secondary_delay_ms==100 && prospero_subtitle_delay_ms==250 && prospero_subtitle_time_scale==1.25,"both timing adjustments preserved");
    strcpy(switch_media,"/missing/second.mp4"); switch_state=1;
    CHECK(tube_caption_apply_ready()==-1 && !switch_applied,"failed second-language change rejected");
    CHECK(prospero_secondary_subtitle_text_at(2,second_text,sizeof second_text) && !strcmp(second_text,"SRT one"),"failed second-language change retains old cues");
    switch_secondary=0;strcpy(switch_media,good_media);switch_state=1;
    CHECK(tube_caption_apply_ready()==1 && prospero_secondary_subtitle_active(),"primary application retains separate second bank");
    CHECK(audio_clock_seconds==42.125 && player_paused==1,"dual caption operations preserve clock and pause");
    strcpy(secondary_media,media);
    prospero_embedded_subtitle_close();
    CHECK(!prospero_secondary_subtitle_active(),"decoder reopen retires old secondary bank");
    CHECK(tube_subtitle_reopen(prospero_subtitle_load_for_media(good_media))==1,"startup bridge reloads both selected languages");
    CHECK(prospero_secondary_subtitle_text_at(2,second_text,sizeof second_text) && !strcmp(second_text,"SRT one"),"secondary returns after decoder reopen");
    cue=prospero_subtitle_active_cue(2);CHECK(cue && !strcmp(cue->text,"Bonjour"),"primary returns after decoder reopen");
    strcpy(secondary_media,"/missing/second.mp4");
    CHECK(tube_subtitle_reopen(prospero_subtitle_load_for_media(good_media))==-1,"failed secondary restoration is reported");
    cue=prospero_subtitle_active_cue(2);CHECK(cue && !strcmp(cue->text,"Bonjour"),"failed secondary restoration retains primary cues");
    prospero_embedded_subtitle_close();avformat_free_context(play_fmt);play_fmt=NULL;
    remove(sidecar); remove(next_sidecar);
    prospero_subtitle_clear();
    fprintf(stderr, "%d total checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
