/* Profile-owned preferences. Companion pairing and remote identity are device-owned.
 * With "separate settings per account" off, every profile uses the one row
 * named SHARED_SETTINGS, a name no companion profile can carry. */
#define SETTINGS_PROFILES 128
typedef struct {
    int height, max_fps, codec, captions, auto_captions, sponsor, autoplay, ask_stream, player_chat_mode, sleep_minutes, history_state, search_history, paid_notifications, dearrow_titles, dearrow_thumbnails;
    int speed, last_speed, speed_mode;
    int playback_mode;
    int queue_respects_mode;
    char language[16], audio_language[16], boot_section[24];
    int search_cards, channel_sort;
    unsigned char section_order[SECTION_SLOTS]; unsigned section_hidden;
    tube_sb_policy sb;
    TubePlayerExtra extra;
    char hl[9], gl[3];
    unsigned hide; int hide_set, thumbnails, sb_alt, section_playlist;
    int more_subtitles, caption_memory, dislikes;
    TubeUiPrefs ui;
} ProfileSettings;
static struct { char profile[33]; ProfileSettings value; } g_profile_settings[SETTINGS_PROFILES];
static int g_profile_settings_count;
static ProfileSettings profile_settings_defaults(void) {
    ProfileSettings p = {0}; p.height = 1080; p.captions = p.search_history = p.section_playlist = 1;
    tube_player_extra_defaults(&p.extra); tube_ui_defaults(&p.ui);
    p.speed = p.last_speed = 100; p.speed_mode = 1;
    sections_default(p.section_order);
    copy(p.language,sizeof p.language,"en"); tube_sb_defaults(&p.sb); return p;
}
static ProfileSettings profile_settings_capture(void) {
    ProfileSettings p;
    p.extra = *tube_player_extra_current();
    p.ui = g_ui; tube_ui_validate(&p.ui);
    copy(p.hl,sizeof p.hl,g_hl); copy(p.gl,sizeof p.gl,g_gl);
    p.hide = g_hide; p.hide_set = g_hide_set; p.thumbnails = g_thumbnails; p.sb_alt = g_sb_alt;
    p.section_playlist = g_section_playlist;
    p.more_subtitles = g_more_subtitles; p.caption_memory = g_caption_memory; p.dislikes = g_dislikes;
    p.speed = g_speed; p.last_speed = g_last_speed; p.speed_mode = g_speed_mode;
    p.height = g_height;
    p.max_fps = g_max_fps;
    p.codec = g_codec;
    p.captions = g_captions;
    p.auto_captions = g_auto_captions;
    p.sponsor = g_sponsor;
    p.autoplay = g_autoplay;
    p.playback_mode=g_playback_mode;
    p.queue_respects_mode=g_queue_respects_mode;
    p.ask_stream = g_ask_stream;
    p.player_chat_mode = g_player_chat_mode;
    p.sleep_minutes = g_sleep_minutes;
    p.history_state = g_history_state;
    p.search_history = g_search_history;
    copy(p.boot_section,sizeof p.boot_section,g_boot_section);
    p.search_cards = g_search_cards; p.channel_sort = g_channel_sort;
    memcpy(p.section_order,g_section_order,sizeof p.section_order); p.section_hidden = g_section_hidden;
    p.paid_notifications = g_paid_notifications;
    p.dearrow_titles = g_dearrow_titles;
    p.dearrow_thumbnails = g_dearrow_thumbnails;
    copy(p.language,sizeof p.language,g_language);
    copy(p.audio_language,sizeof p.audio_language,g_audio_language); p.sb = g_sb; return p;
}
static void profile_settings_apply(const ProfileSettings *p) {
    g_speed = p->speed; g_last_speed = p->last_speed; g_speed_mode = p->speed_mode;
    g_height = p->height;
    g_max_fps = p->max_fps;
    g_codec = p->codec;
    g_captions = p->captions;
    g_auto_captions = p->auto_captions;
    g_sponsor = p->sponsor;
    g_autoplay = p->autoplay;
    g_playback_mode=p->playback_mode; g_autoplay=playback_mode_continues(g_playback_mode);
    g_queue_respects_mode=p->queue_respects_mode;
    g_ask_stream = p->ask_stream;
    g_player_chat_mode = p->player_chat_mode;
    g_sleep_minutes = p->sleep_minutes;
    g_history_state = p->history_state;
    g_search_history = p->search_history;
    copy(g_boot_section,sizeof g_boot_section,p->boot_section);
    g_search_cards = p->search_cards; g_channel_sort = p->channel_sort;
    memcpy(g_section_order,p->section_order,sizeof g_section_order); g_section_hidden = p->section_hidden;
    g_paid_notifications = p->paid_notifications;
    g_dearrow_titles = p->dearrow_titles;
    g_dearrow_thumbnails = p->dearrow_thumbnails;
    copy(g_language,sizeof g_language,p->language);
    copy(g_audio_language,sizeof g_audio_language,p->audio_language); g_sb = p->sb;
    tube_player_extra_select(&p->extra);
    copy(g_hl,sizeof g_hl,p->hl); copy(g_gl,sizeof g_gl,p->gl);
    g_hide = p->hide; g_hide_set = p->hide_set; g_thumbnails = p->thumbnails; g_sb_alt = p->sb_alt;
    g_section_playlist = p->section_playlist;
    g_more_subtitles = p->more_subtitles; g_caption_memory = p->caption_memory; g_dislikes = p->dislikes;
    g_ui = p->ui; tube_ui_validate(&g_ui); ++g_ui_revision; tube_ui_press_reset();
    g_sleep_armed = g_sleep_fired = 0;
    video_geometry_publish();
}
static int profile_settings_slot(const char *profile, int create) {
    for (int i = 0; i < g_profile_settings_count; ++i)
        if (!strcmp(profile,g_profile_settings[i].profile)) return i;
    if (!create || (!valid_profile(profile) && strcmp(profile,SHARED_SETTINGS)) || g_profile_settings_count == SETTINGS_PROFILES) return -1;
    int i = g_profile_settings_count++;
    copy(g_profile_settings[i].profile,sizeof g_profile_settings[i].profile,profile);
    g_profile_settings[i].value = profile_settings_defaults(); return i;
}
static void profile_settings_store(void) {
    int i = profile_settings_slot(settings_key(),1);
    if (i >= 0) g_profile_settings[i].value = profile_settings_capture();
}
static int profile_settings_select(const char *profile) {
    int i = profile_settings_slot(g_separate_settings ? profile : SHARED_SETTINGS,1); if (i < 0) return 0;
    if (strcmp(profile,g_library_profile)) tube_queue_retry_cancel();
    profile_settings_store(); copy(g_library_profile,sizeof g_library_profile,profile);
    profile_settings_apply(&g_profile_settings[i].value); return 1;
}
static int settings_integer(const cJSON *root, const char *key, int lo, int hi, int fallback) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root,key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= lo &&
        v->valuedouble <= hi && floor(v->valuedouble) == v->valuedouble ? (int)v->valuedouble : fallback;
}
/* history_state supersedes the history_sync boolean; a saved "off" becomes Disabled. */
static int history_state_read(const cJSON *root) {
    return settings_integer(root,"history_state",TUBE_HISTORY_AUTO,TUBE_HISTORY_DISABLED,
        cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(root,"history_sync")) ? TUBE_HISTORY_DISABLED : TUBE_HISTORY_AUTO);
}
/* Saved order first, then any section it does not name, in the built-in order.
 * Unknown, repeated and malformed rows are skipped one by one. */
static void sections_read(ProfileSettings *p, const cJSON *root) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root,"sections"), *row;
    unsigned seen = 0; int n = 0;
    if (cJSON_IsArray(rows)) cJSON_ArrayForEach(row,rows) {
        int s = section_index(str(row,"id")); if (s < 0 || (seen >> s & 1)) continue;
        seen |= 1u << s; p->section_order[n++] = (unsigned char)s;
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row,"hidden"))) p->section_hidden |= 1u << s;
    }
    for (int s = 0; s < SECTIONS; ++s) if (!(seen >> s & 1)) p->section_order[n++] = (unsigned char)s;
}
/* Interface preferences: absent in older files, default field by field when
 * corrupt. The menu reads like the sections: saved order first, then any row it
 * does not name, in the built-in order. */
static void ui_prefs_read(TubeUiPrefs *ui, const cJSON *root) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root,"video_menu"), *row;
    uint64_t seen = 0; int n = 0;
    tube_ui_defaults(ui);
    ui->ui_scale = settings_integer(root,"ui_scale",40,140,100);
    ui->grid_scale = settings_integer(root,"video_grid_scale",50,150,100);
    ui->title_lines = settings_integer(root,"card_title_lines",1,4,1);
    ui->title_scroll = boolean(root,"card_title_scroll");
    ui->scroll_speed = settings_integer(root,"card_scroll_speed",10,80,10);
    ui->subtitle_lines = boolean(root,"card_multiline_subtitle") ? 2 : 1;
    ui->square = boolean(root,"card_square_corners");
    for (int i = 0; i < TUBE_UI_SCHEMES; ++i) if (!strcmp(str(root,"color_scheme"),g_tube_schemes[i].id)) ui->scheme = i;
    ui->app_exit = settings_integer(root,"app_exit",TUBE_UI_EXIT_NONE,TUBE_UI_EXIT_SINGLE,TUBE_UI_EXIT_NONE);
    ui->search_exit = boolean(root,"search_exit_double");
    if (cJSON_IsArray(rows)) cJSON_ArrayForEach(row,rows) {
        const int key = tube_ui_menu_find(str(row,"id")); if (key < 0 || (seen >> key & 1)) continue;
        seen |= (uint64_t)1 << key; ui->menu_order[n++] = (unsigned char)key;
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(row,"hidden"))) ui->menu_hidden |= (uint64_t)1 << key;
    }
    for (int key = 0; key < TUBE_MENU_KEYS; ++key) if (!(seen >> key & 1)) ui->menu_order[n++] = (unsigned char)key;
    tube_ui_validate(ui);
}
static void ui_prefs_write(cJSON *row, const TubeUiPrefs *ui) {
    cJSON_AddNumberToObject(row,"ui_scale",ui->ui_scale);
    cJSON_AddNumberToObject(row,"video_grid_scale",ui->grid_scale);
    cJSON_AddNumberToObject(row,"card_title_lines",ui->title_lines);
    cJSON_AddBoolToObject(row,"card_title_scroll",ui->title_scroll);
    cJSON_AddNumberToObject(row,"card_scroll_speed",ui->scroll_speed);
    cJSON_AddBoolToObject(row,"card_multiline_subtitle",ui->subtitle_lines > 1);
    cJSON_AddBoolToObject(row,"card_square_corners",ui->square);
    cJSON_AddStringToObject(row,"color_scheme",g_tube_schemes[ui->scheme].id);
    cJSON_AddNumberToObject(row,"app_exit",ui->app_exit);
    cJSON_AddBoolToObject(row,"search_exit_double",ui->search_exit);
    TubeUiPrefs plain; tube_ui_defaults(&plain);   /* absent = the built-in menu */
    if (!ui->menu_hidden && !memcmp(ui->menu_order,plain.menu_order,sizeof plain.menu_order)) return;
    cJSON *menu = cJSON_AddArrayToObject(row,"video_menu");
    for (int k = 0; menu && k < TUBE_MENU_KEYS; ++k) {
        cJSON *entry = cJSON_CreateObject(); if (!entry) break;
        cJSON_AddStringToObject(entry,"id",g_tube_menu_keys[ui->menu_order[k]].id);
        cJSON_AddBoolToObject(entry,"hidden",ui->menu_hidden >> ui->menu_order[k] & 1);
        cJSON_AddItemToArray(menu,entry);
    }
}
static void profile_settings_read(ProfileSettings *p, const cJSON *root) {
    *p = profile_settings_defaults();
    double speed = num(root,"speed_percent"), last = num(root,"last_speed_percent");
    p->extra = tube_player_extra_read(root);
    p->speed = valid_speed(speed) ? (int)speed : 100;
    p->last_speed = valid_speed(last) ? (int)last : 100;
    p->speed_mode = settings_integer(root,"speed_memory_mode",0,3,1);
    p->height = settings_integer(root,"max_height",144,2160,1080);
    p->max_fps = settings_integer(root,"max_fps",0,60,0);
    p->codec = settings_integer(root,"codec",0,4,0);
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"captions"))) p->captions = boolean(root,"captions");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"automatic_captions"))) p->auto_captions = boolean(root,"automatic_captions");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"sponsorblock"))) p->sponsor = boolean(root,"sponsorblock");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"autoplay"))) p->autoplay = boolean(root,"autoplay");
    p->playback_mode=settings_integer(root,"playback_mode",0,6,p->autoplay?TUBE_MODE_ALL:TUBE_MODE_PAUSE);
    if (!playback_mode_valid(p->playback_mode)) p->playback_mode=p->autoplay?TUBE_MODE_ALL:TUBE_MODE_PAUSE;
    p->autoplay=playback_mode_continues(p->playback_mode);
    p->queue_respects_mode=boolean(root,"queue_respects_playback_mode");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"ask_stream"))) p->ask_stream = boolean(root,"ask_stream");
    p->player_chat_mode = settings_integer(root,"player_chat_mode",0,2,0);
    p->sleep_minutes = settings_integer(root,"sleep_timer_minutes",0,600,0);
    p->history_state = history_state_read(root);
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"search_history"))) p->search_history = boolean(root,"search_history");
    copy(p->boot_section,sizeof p->boot_section,g_boot_sections[boot_section_index(str(root,"boot_section"))].id);
    p->search_cards = boolean(root,"search_channels_playlists");
    p->channel_sort = settings_integer(root,"channel_sort",0,2,0);
    sections_read(p,root);
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"paid_content_notifications"))) p->paid_notifications = boolean(root,"paid_content_notifications");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"dearrow_titles"))) p->dearrow_titles = boolean(root,"dearrow_titles");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"dearrow_thumbnails"))) p->dearrow_thumbnails = boolean(root,"dearrow_thumbnails");
    if (p->sleep_minutes % 30) p->sleep_minutes = 0;
    const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    const char *language = str(root,"language"), *audio = str(root,"audio_language");
    if (*language && strlen(language) < sizeof p->language && strspn(language,valid) == strlen(language))
        copy(p->language,sizeof p->language,language);
    if (strlen(audio) < sizeof p->audio_language && strspn(audio,valid) == strlen(audio))
        copy(p->audio_language,sizeof p->audio_language,audio);
    tube_sb_load(&p->sb,root);
    /* Fields of the second batch: absent in older files, default when corrupt. */
    const char *hl = str(root,"content_language"), *gl = str(root,"content_country");
    if (language_index(hl) >= 0) copy(p->hl,sizeof p->hl,hl);
    if (country_index(gl) >= 0) copy(p->gl,sizeof p->gl,gl);
    const cJSON *hidden = cJSON_GetObjectItemCaseSensitive(root,"hide"), *name;
    if (cJSON_IsArray(hidden)) {
        p->hide_set = 1;
        cJSON_ArrayForEach(name,hidden) {
            int flag = cJSON_IsString(name) ? hide_index(name->valuestring) : -1;
            if (flag >= 0) p->hide |= 1u << flag;
        }
    }
    p->thumbnails = thumbnail_index(str(root,"thumbnail_source"));
    p->sb_alt = boolean(root,"sponsorblock_alt_server");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"section_playlist"))) p->section_playlist = boolean(root,"section_playlist");
    /* Fields of the third batch. */
    p->more_subtitles = boolean(root,"more_subtitles"); p->caption_memory = boolean(root,"caption_memory");
    p->dislikes = boolean(root,"show_dislikes");
    ui_prefs_read(&p->ui,root);
}
static void profile_settings_load(const cJSON *root) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root,"profile_settings"), *row;
    ProfileSettings legacy = profile_settings_capture();
    /* Reserve guest and selected profile so account/source removal always has a safe destination. */
    profile_settings_slot("guest",1); profile_settings_slot(g_library_profile,1);
    if (!cJSON_IsArray(rows)) {
        for (int i = 0; i < g_count; ++i) profile_settings_slot(g_records[i].profile,1);
        for (int i = 0; i < g_player_preferences_count; ++i) profile_settings_slot(g_player_preferences[i].profile,1);
        for (int i = 0; i < g_profile_settings_count; ++i) g_profile_settings[i].value = legacy;
    } else {
        /* First valid occurrence wins; malformed settings fall back field-by-field. */
        unsigned char seen[SETTINGS_PROFILES] = {0};
        cJSON_ArrayForEach(row,rows) {
            const char *profile = str(row,"profile"); if (!valid_profile(profile) && strcmp(profile,SHARED_SETTINGS)) continue;
            int i = profile_settings_slot(profile,1); if (i < 0 || seen[i]) continue;
            seen[i] = 1; profile_settings_read(&g_profile_settings[i].value,row);
        }
    }
    int i = profile_settings_slot(settings_key(),1);
    if (i >= 0) profile_settings_apply(&g_profile_settings[i].value);
}
static void profile_settings_write(cJSON *root) {
    profile_settings_store(); cJSON *rows = cJSON_AddArrayToObject(root,"profile_settings");
    for (int i = 0; rows && i < g_profile_settings_count; ++i) {
        const ProfileSettings *p = &g_profile_settings[i].value;
        cJSON *row = cJSON_CreateObject(); if (!row) break;
        cJSON_AddStringToObject(row,"profile",g_profile_settings[i].profile);
        cJSON_AddNumberToObject(row,"speed_percent",p->speed);
        cJSON_AddNumberToObject(row,"last_speed_percent",p->last_speed);
        cJSON_AddNumberToObject(row,"speed_memory_mode",p->speed_mode);
        cJSON_AddNumberToObject(row,"max_height",p->height);
        cJSON_AddNumberToObject(row,"max_fps",p->max_fps);
        cJSON_AddNumberToObject(row,"codec",p->codec);
        cJSON_AddBoolToObject(row,"captions",p->captions);
        cJSON_AddBoolToObject(row,"automatic_captions",p->auto_captions);
        cJSON_AddBoolToObject(row,"sponsorblock",p->sponsor);
        cJSON_AddBoolToObject(row,"autoplay",p->autoplay);
        cJSON_AddNumberToObject(row,"playback_mode",p->playback_mode);
        cJSON_AddBoolToObject(row,"queue_respects_playback_mode",p->queue_respects_mode);
        cJSON_AddBoolToObject(row,"ask_stream",p->ask_stream);
        cJSON_AddNumberToObject(row,"player_chat_mode",p->player_chat_mode);
        cJSON_AddNumberToObject(row,"sleep_timer_minutes",p->sleep_minutes);
        cJSON_AddNumberToObject(row,"history_state",p->history_state);
        cJSON_AddBoolToObject(row,"history_sync",p->history_state!=TUBE_HISTORY_DISABLED);
        cJSON_AddBoolToObject(row,"search_history",p->search_history);
        cJSON_AddStringToObject(row,"boot_section",p->boot_section);
        cJSON_AddBoolToObject(row,"search_channels_playlists",p->search_cards);
        cJSON_AddNumberToObject(row,"channel_sort",p->channel_sort);
        cJSON *sections = cJSON_AddArrayToObject(row,"sections");
        for (int k = 0; sections && k < SECTIONS; ++k) {
            cJSON *section = cJSON_CreateObject(); if (!section) break;
            cJSON_AddStringToObject(section,"id",g_sections[p->section_order[k]].id);
            cJSON_AddBoolToObject(section,"hidden",p->section_hidden >> p->section_order[k] & 1);
            cJSON_AddItemToArray(sections,section);
        }
        cJSON_AddBoolToObject(row,"paid_content_notifications",p->paid_notifications);
        cJSON_AddBoolToObject(row,"dearrow_titles",p->dearrow_titles);
        cJSON_AddBoolToObject(row,"dearrow_thumbnails",p->dearrow_thumbnails);
        cJSON_AddStringToObject(row,"language",p->language);
        cJSON_AddStringToObject(row,"audio_language",p->audio_language);
        tube_sb_save(&p->sb,row);
        tube_player_extra_write(row,&p->extra);
        cJSON_AddStringToObject(row,"content_language",p->hl);
        cJSON_AddStringToObject(row,"content_country",p->gl);
        if (p->hide_set) { /* absent = nothing was changed: the companion keeps its own default */
            cJSON *hidden = cJSON_AddArrayToObject(row,"hide");
            for (int k = 0; hidden && k < HIDE_FLAGS; ++k)
                if (p->hide >> k & 1) cJSON_AddItemToArray(hidden,cJSON_CreateString(g_hide_flags[k].name));
        }
        cJSON_AddStringToObject(row,"thumbnail_source",g_thumbnail_sources[p->thumbnails]);
        cJSON_AddBoolToObject(row,"sponsorblock_alt_server",p->sb_alt);
        cJSON_AddBoolToObject(row,"section_playlist",p->section_playlist);
        cJSON_AddBoolToObject(row,"more_subtitles",p->more_subtitles);
        cJSON_AddBoolToObject(row,"caption_memory",p->caption_memory);
        cJSON_AddBoolToObject(row,"show_dislikes",p->dislikes);
        ui_prefs_write(row,&p->ui);
        cJSON_AddItemToArray(rows,row);
    }
}
