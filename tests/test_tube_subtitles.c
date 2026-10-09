/* Reuse pinned EVO's host stubs and test the actual engine, not a mock. */
#define main evo_dualsub_main
#include "../third_party/evo/tools/dualsub_host.c"
#undef main
#include "tube_subtitles.h"

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
    prospero_subtitle_clear();
    fprintf(stderr, "%d total checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
