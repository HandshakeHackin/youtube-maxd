/* Profile-owned preferences. Companion pairing and remote identity are device-owned. */
#define SETTINGS_PROFILES 128
typedef struct {
    int height, codec, captions, auto_captions, sponsor, autoplay, ask_stream, player_chat_mode, sleep_minutes, history_sync, paid_notifications, dearrow_titles, dearrow_thumbnails;
    int speed, last_speed, speed_mode;
    char language[16], audio_language[16];
    tube_sb_policy sb;
} ProfileSettings;
static struct { char profile[33]; ProfileSettings value; } g_profile_settings[SETTINGS_PROFILES];
static int g_profile_settings_count;
static ProfileSettings profile_settings_defaults(void) {
    ProfileSettings p = {0}; p.height = 1080; p.captions = p.history_sync = 1;
    p.speed = p.last_speed = 100; p.speed_mode = 1;
    copy(p.language,sizeof p.language,"en"); tube_sb_defaults(&p.sb); return p;
}
static ProfileSettings profile_settings_capture(void) {
    ProfileSettings p;
    p.speed = g_speed; p.last_speed = g_last_speed; p.speed_mode = g_speed_mode;
    p.height = g_height;
    p.codec = g_codec;
    p.captions = g_captions;
    p.auto_captions = g_auto_captions;
    p.sponsor = g_sponsor;
    p.autoplay = g_autoplay;
    p.ask_stream = g_ask_stream;
    p.player_chat_mode = g_player_chat_mode;
    p.sleep_minutes = g_sleep_minutes;
    p.history_sync = g_history_sync;
    p.paid_notifications = g_paid_notifications;
    p.dearrow_titles = g_dearrow_titles;
    p.dearrow_thumbnails = g_dearrow_thumbnails;
    copy(p.language,sizeof p.language,g_language);
    copy(p.audio_language,sizeof p.audio_language,g_audio_language); p.sb = g_sb; return p;
}
static void profile_settings_apply(const ProfileSettings *p) {
    g_speed = p->speed; g_last_speed = p->last_speed; g_speed_mode = p->speed_mode;
    g_height = p->height;
    g_codec = p->codec;
    g_captions = p->captions;
    g_auto_captions = p->auto_captions;
    g_sponsor = p->sponsor;
    g_autoplay = p->autoplay;
    g_ask_stream = p->ask_stream;
    g_player_chat_mode = p->player_chat_mode;
    g_sleep_minutes = p->sleep_minutes;
    g_history_sync = p->history_sync;
    g_paid_notifications = p->paid_notifications;
    g_dearrow_titles = p->dearrow_titles;
    g_dearrow_thumbnails = p->dearrow_thumbnails;
    copy(g_language,sizeof g_language,p->language);
    copy(g_audio_language,sizeof g_audio_language,p->audio_language); g_sb = p->sb;
    g_sleep_armed = g_sleep_fired = 0;
}
static int profile_settings_slot(const char *profile, int create) {
    for (int i = 0; i < g_profile_settings_count; ++i)
        if (!strcmp(profile,g_profile_settings[i].profile)) return i;
    if (!create || !valid_profile(profile) || g_profile_settings_count == SETTINGS_PROFILES) return -1;
    int i = g_profile_settings_count++;
    copy(g_profile_settings[i].profile,sizeof g_profile_settings[i].profile,profile);
    g_profile_settings[i].value = profile_settings_defaults(); return i;
}
static void profile_settings_store(void) {
    int i = profile_settings_slot(g_library_profile,1);
    if (i >= 0) g_profile_settings[i].value = profile_settings_capture();
}
static int profile_settings_select(const char *profile) {
    int i = profile_settings_slot(profile,1); if (i < 0) return 0;
    if (strcmp(profile,g_library_profile)) tube_queue_retry_cancel();
    profile_settings_store(); copy(g_library_profile,sizeof g_library_profile,profile);
    profile_settings_apply(&g_profile_settings[i].value); return 1;
}
static int settings_integer(const cJSON *root, const char *key, int lo, int hi, int fallback) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(root,key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= lo &&
        v->valuedouble <= hi && floor(v->valuedouble) == v->valuedouble ? (int)v->valuedouble : fallback;
}
static void profile_settings_read(ProfileSettings *p, const cJSON *root) {
    *p = profile_settings_defaults();
    double speed = num(root,"speed_percent"), last = num(root,"last_speed_percent");
    p->speed = valid_speed(speed) ? (int)speed : 100;
    p->last_speed = valid_speed(last) ? (int)last : 100;
    p->speed_mode = settings_integer(root,"speed_memory_mode",0,3,1);
    p->height = settings_integer(root,"max_height",144,2160,1080);
    p->codec = settings_integer(root,"codec",0,2,0);
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"captions"))) p->captions = boolean(root,"captions");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"automatic_captions"))) p->auto_captions = boolean(root,"automatic_captions");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"sponsorblock"))) p->sponsor = boolean(root,"sponsorblock");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"autoplay"))) p->autoplay = boolean(root,"autoplay");
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"ask_stream"))) p->ask_stream = boolean(root,"ask_stream");
    p->player_chat_mode = settings_integer(root,"player_chat_mode",0,2,0);
    p->sleep_minutes = settings_integer(root,"sleep_timer_minutes",0,600,0);
    if (cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(root,"history_sync"))) p->history_sync = boolean(root,"history_sync");
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
            const char *profile = str(row,"profile"); if (!valid_profile(profile)) continue;
            int i = profile_settings_slot(profile,1); if (i < 0 || seen[i]) continue;
            seen[i] = 1; profile_settings_read(&g_profile_settings[i].value,row);
        }
    }
    int i = profile_settings_slot(g_library_profile,0); profile_settings_apply(&g_profile_settings[i].value);
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
        cJSON_AddNumberToObject(row,"codec",p->codec);
        cJSON_AddBoolToObject(row,"captions",p->captions);
        cJSON_AddBoolToObject(row,"automatic_captions",p->auto_captions);
        cJSON_AddBoolToObject(row,"sponsorblock",p->sponsor);
        cJSON_AddBoolToObject(row,"autoplay",p->autoplay);
        cJSON_AddBoolToObject(row,"ask_stream",p->ask_stream);
        cJSON_AddNumberToObject(row,"player_chat_mode",p->player_chat_mode);
        cJSON_AddNumberToObject(row,"sleep_timer_minutes",p->sleep_minutes);
        cJSON_AddBoolToObject(row,"history_sync",p->history_sync);
        cJSON_AddBoolToObject(row,"paid_content_notifications",p->paid_notifications);
        cJSON_AddBoolToObject(row,"dearrow_titles",p->dearrow_titles);
        cJSON_AddBoolToObject(row,"dearrow_thumbnails",p->dearrow_thumbnails);
        cJSON_AddStringToObject(row,"language",p->language);
        cJSON_AddStringToObject(row,"audio_language",p->audio_language);
        tube_sb_save(&p->sb,row); cJSON_AddItemToArray(rows,row);
    }
}
