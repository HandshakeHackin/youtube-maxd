/* ProsperoTube interface localization.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Texts are looked up by their English source. A text the catalog does not
 * hold stays as it is, so nothing here can remove or shorten a line. The
 * language is the "Interface language" preference, then the language of the
 * PS5 system settings, then English. See docs/LOCALIZATION.md.
 *
 * The functions the provider, the browser and the kit call are weak
 * references: a program linked without tube_i18n.c shows English. */
#ifndef TUBE_I18N_H
#define TUBE_I18N_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef TUBE_I18N_IMPLEMENTATION
#define TUBE_I18N_HOOK
#else
#define TUBE_I18N_HOOK __attribute__((weak))
#endif

/* Also translate "<label>: <value>" and "<text> (experimental)" part by part.
 * For lines the interface makes itself, never for lines that may hold content. */
#define TUBE_I18N_LABELS 1
/* The line is drawn by the player interface, whose fonts hold fewer letters:
 * translated only in a language those fonts cover in full. */
#define TUBE_I18N_KIT 2

/* 1 = out holds the translation; 0 = no translation, or it does not fit: out is untouched. */
int tube_i18n_translate(const char *source, char *out, size_t capacity, int flags) TUBE_I18N_HOOK;
/* The same in place. The text is left alone when the translation does not fit. */
void tube_i18n_field(char *text, size_t capacity, int flags) TUBE_I18N_HOOK;
/* For the kit's text call: exact texts and counted patterns only. */
int tube_i18n_kit(const char *source, char *out, size_t capacity) TUBE_I18N_HOOK;
/* Changes whenever the language does. */
unsigned tube_i18n_generation(void) TUBE_I18N_HOOK;

/* The "Interface language" preference as provider rows: the row of the
 * preferences list, then its page (System default, English, each catalog). */
#define TUBE_I18N_SETTING "settings:interface-language"
const char *tube_i18n_setting_title(void) TUBE_I18N_HOOK;       /* "Interface language: <name>" */
int tube_i18n_choices(void) TUBE_I18N_HOOK;
const char *tube_i18n_choice_id(int index) TUBE_I18N_HOOK;      /* TUBE_I18N_SETTING ":<tag>" */
const char *tube_i18n_choice_title(int index) TUBE_I18N_HOOK;   /* the language in its own words */
const char *tube_i18n_choice_detail(int index) TUBE_I18N_HOOK;
/* A row of that page was chosen: 1 = stored, 0 = id is the page itself, -1 = not a listed language. */
int tube_i18n_select(const char *id) TUBE_I18N_HOOK;


/* ---- The module itself (not referenced by shared code). ---- */
const char *tube_i18n_language(void);              /* tag in use; "en" for English */
const char *tube_i18n_preference(void);            /* stored tag; "" = follow the system */
int tube_i18n_set_preference(const char *tag);     /* "" = system; 0 on success, -1 for an unknown tag */
int tube_i18n_language_count(void);                /* catalogs compiled in, English not counted */
const char *tube_i18n_language_tag(int index);
const char *tube_i18n_language_name(int index);
int tube_i18n_language_kit(int index);             /* 1 = the player interface fonts cover it */
/* PS5 system language number -> tag ("" when unknown). */
const char *tube_i18n_system_tag(int system_language);
/* Tests and the host preview: the system language number (-1 = ask the console),
 * and where the preference is kept (NULL = the app data folder). */
void tube_i18n_set_system_language(int system_language);
void tube_i18n_set_storage(const char *path);
/* Number of texts asked for that had no translation since the last call with reset. */
unsigned tube_i18n_misses(int reset);

#ifdef __cplusplus
}
#endif
#endif
