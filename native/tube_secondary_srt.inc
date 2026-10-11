/* Independent YouTube secondary SRT bank. Included in EVO's subtitle engine,
 * reusing its bounded parser and cue cleaning without touching primary cues. */
#include "tube_provider.h"
static ProsperoSubtitleCue *tube_secondary_cues;
static int tube_secondary_count;
static char tube_secondary_owner[512];
static unsigned tube_secondary_selection;

void tube_secondary_subtitle_clear(void) {
    free(tube_secondary_cues); tube_secondary_cues = NULL;
    tube_secondary_count = 0; tube_secondary_owner[0] = 0;
    tube_secondary_selection = 0;
    if (prospero_secondary_use_external == 2) {
        prospero_secondary_use_external = 0;
        prospero_secondary_subtitle_requested = PROSPERO_SECONDARY_NONE;
    }
}

static int tube_secondary_current(void) {
    return tube_secondary_cues && tube_secondary_count > 0 &&
        (tube_secondary_selection ? tube_secondary_selection == tube_caption_selection() :
         !strcmp(tube_secondary_owner, current_media_path));
}

int tube_secondary_subtitle_load(const char *media) {
    char path[512], stored_path[512]; int count = 0;
    const unsigned selection = tube_caption_selection();
    if (!play_fmt || (!selection && (!current_media_path[0] || strlen(current_media_path) >= sizeof tube_secondary_owner)) ||
        !prospero_subtitle_sidecar_path(media, path, sizeof path, ".srt")) return 0;
    ProsperoSubtitleCue *staged = calloc(PROSPERO_SUBTITLE_MAX_CUES, sizeof *staged);
    if (!staged) return 0;
    if (!tube_subtitle_read_file(path, staged, &count, stored_path, sizeof stored_path)) {
        free(staged); return 0;
    }
    /* Publish only after successful parsing; failed changes retain both lines. */
    free(tube_secondary_cues); tube_secondary_cues = staged; tube_secondary_count = count;
    tube_secondary_selection = selection;
    if (selection) tube_secondary_owner[0] = 0;
    else snprintf(tube_secondary_owner, sizeof tube_secondary_owner, "%s", current_media_path);
    prospero_sub_slot_close(SUB_SECONDARY);
    prospero_secondary_use_external = 2;
    prospero_secondary_subtitle_requested = PROSPERO_SECONDARY_EXTERNAL_BANK;
    prospero_secondary_delay_ms = 0;
    prospero_subtitle_enabled = 1;
    return count;
}

static int tube_secondary_text_at(double clock, char *output, size_t size) {
    double position = clock - (double)prospero_secondary_delay_ms / 1000.0;
    if (!tube_secondary_current() || !isfinite(position)) return 0;
    if (position < 0) position = 0;
    for (int i = 0; i < tube_secondary_count; ++i) {
        const ProsperoSubtitleCue *cue = &tube_secondary_cues[i];
        if (position >= cue->start_seconds && position <= cue->end_seconds) {
            snprintf(output, size, "%s", cue->text); return *output != 0;
        }
    }
    return 0;
}
