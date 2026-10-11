/* Main-thread application of a staged SRT; never reopens or seeks media. */
#ifndef TUBE_CAPTION_APPLY_H
#define TUBE_CAPTION_APPLY_H
#include "tube_provider.h"
#include "tube_subtitles.h"
#include <string.h>
static inline int tube_caption_apply_ready(void) {
    const char *media = "";
    int state = tube_caption_switch_status(&media);
    if (!state) return 0;
    if (state < 0) { tube_caption_switch_finish(0); return -1; }
    if (tube_caption_switch_secondary()) {
        int delay = prospero_secondary_delay_ms;
        int applied = tube_secondary_subtitle_load(media) > 0;
        prospero_secondary_delay_ms = delay;
        tube_caption_switch_finish(applied);
        return applied ? 1 : -1;
    }
    char old_media[512];
    const char *old_path = tube_caption_media_path();
    size_t length = strlen(old_path);
    if (length >= sizeof old_media) { tube_caption_switch_finish(0); return -1; }
    memcpy(old_media, old_path, length + 1);
    int enabled = prospero_subtitle_enabled, external = prospero_subtitle_use_external;
    int delay = prospero_subtitle_delay_ms, secondary_delay = prospero_secondary_delay_ms;
    double ratio = prospero_subtitle_time_scale;
    int secondary = prospero_secondary_subtitle_requested;
    int cues = prospero_subtitle_load_for_media(media);
    int applied = cues > 0 && prospero_subtitle_select_primary(-1);
    if (!applied) {
        if (*old_media) prospero_subtitle_select_primary(prospero_subtitle_load_for_media(old_media) > 0 ? -1 : -2);
        else prospero_subtitle_clear();
        prospero_subtitle_enabled = enabled; prospero_subtitle_use_external = external;
        if (secondary != PROSPERO_SECONDARY_EXTERNAL_BANK) prospero_secondary_subtitle_select(secondary);
    }
    prospero_subtitle_delay_ms = delay; prospero_subtitle_time_scale = ratio;
    prospero_secondary_delay_ms = secondary_delay;
    if (applied && !strcmp(tube_caption_switch_language(), tube_caption_secondary_language()))
        tube_secondary_subtitle_clear();
    tube_caption_switch_finish(applied);
    return applied ? 1 : -1;
}
#endif
