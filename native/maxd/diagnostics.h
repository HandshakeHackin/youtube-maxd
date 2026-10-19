/* A closed, numeric schema: raw logs, URLs and account data cannot enter it.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_DIAGNOSTICS_H
#define MAXD_DIAGNOSTICS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int active,paused,live,buffering,hardware,captions,pip,afterglow,auto_max;
    int width,height,codec,fps_limit,height_limit,codec_preference;
    double fps,position,duration;
    uint64_t frames,dropped;
} MaxdDiagnostics;
void maxd_report_sample(const MaxdDiagnostics *sample);
int maxd_report_capture(const char *path);
const char *maxd_report_json(void);
int maxd_report_clear(const char *path);
#ifdef __cplusplus
}
#endif
#endif
