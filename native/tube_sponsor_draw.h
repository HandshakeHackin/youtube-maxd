/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SPONSOR_DRAW_H
#define TUBE_SPONSOR_DRAW_H
#include "tube_provider.h"
#include <math.h>
/* Keep both renderers inside the track, including point markers at its end. */
static inline int tube_sponsor_geometry(const tube_sponsor_marker_t *marker, double width,
                                       double *left, double *size) {
    if (!marker || !left || !size || !isfinite(width) || width <= 0 ||
        !isfinite(marker->start) || !isfinite(marker->end) || marker->start < 0 ||
        marker->end > 1 || marker->start > marker->end ||
        (!marker->point && marker->start == marker->end)) return 0;
    *size = fmin(width, fmax(marker->point ? 4.0 : 1.0, (marker->end - marker->start) * width));
    *left = fmin(width - *size, marker->start * width);
    return 1;
}
#endif
