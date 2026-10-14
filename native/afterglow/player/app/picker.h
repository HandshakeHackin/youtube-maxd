/* Afterglow picker metadata; existing screens still own selection and input.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_AFTERGLOW_PICKER_H
#define TUBE_AFTERGLOW_PICKER_H
#ifdef __cplusplus
extern "C" {
#endif
int tube_afterglow_enabled(void);
void tube_afterglow_options_begin(const char *title);
void tube_afterglow_options_choice(const char *label, int current);
void tube_afterglow_caption_tab(int tab, int first, int total);
static inline int tube_afterglow_picker_active(void) {
#if defined(EVO_AGC_DEVICE)
    return tube_afterglow_enabled();
#else
    return 0;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
