#ifndef TUBE_SUBTITLES_H
#define TUBE_SUBTITLES_H

#include "evo_subtitle.h"

/* YouTube's caption preference determines which sidecar was downloaded.
 * Loading cues alone does not select them in EVO's subtitle engine. */
static inline int tube_subtitle_activate(int loaded_cues) {
    prospero_subtitle_enabled = 0;
    prospero_subtitle_use_external = 0;
    prospero_secondary_subtitle_select(PROSPERO_SECONDARY_NONE);
    return loaded_cues > 0 && prospero_subtitle_select_primary(-1);
}

#endif
