/* YouTube Max'd controller effects. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MAXD_FEEDBACK_H
#define MAXD_FEEDBACK_H
#include "lightbar.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Main-thread API. The pad is borrowed from EVO; this module never opens it. */
void ym_feedback_state(YmLightState state, int foreground, uint64_t now);
void ym_feedback_shutdown(void);
int tube_maxd_ui_light_state(void);
#ifdef __cplusplus
}
#endif
#endif
