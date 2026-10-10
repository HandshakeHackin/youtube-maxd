/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SPONSOR_H
#define TUBE_SPONSOR_H
/* Shared persisted policy; rendering remains in the PS5 player. */
#define TUBE_SB_CATEGORIES 8
#define TUBE_SB_EXCLUSIONS 128
static const char *const tube_sb_names[] = {
    "sponsor", "intro", "outro", "interaction", "selfpromo", "music_offtopic", "preview", "filler"
};
static const char *const tube_sb_labels[] = {
    "Sponsor", "Intro", "Outro", "Interaction reminder", "Self promotion", "Non-music section", "Preview / recap", "Filler"
};
static const char *const tube_sb_actions[] = {"Skip silently", "Skip with message", "Ask before skipping", "Do nothing"};
typedef struct {
    int actions[TUBE_SB_CATEGORIES], no_repeat, excluded_count;
    double minimum;
    char excluded[TUBE_SB_EXCLUSIONS][25];
} tube_sb_policy;
static int tube_sb_category(const char *name) {
    for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) if (!strcmp(name, tube_sb_names[i])) return i;
    return -1;
}
static int tube_sb_channel_valid(const char *id) {
    return strlen(id) == 24 && !strncmp(id, "UC", 2) &&
        strspn(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == 24;
}
static int tube_sb_excluded(const tube_sb_policy *policy, const char *channel) {
    for (int i = 0; i < policy->excluded_count; ++i)
        if (!strcmp(channel, policy->excluded[i])) return 1;
    return 0;
}
static void tube_sb_defaults(tube_sb_policy *policy) {
    memset(policy, 0, sizeof *policy);
    for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) policy->actions[i] = i == 7 ? 3 : 1;
    policy->minimum = 5.0;
}
static void tube_sb_load(tube_sb_policy *policy, const cJSON *root) {
    tube_sb_defaults(policy);
    const cJSON *settings = cJSON_GetObjectItemCaseSensitive(root, "sponsor_settings");
    const cJSON *actions = cJSON_GetObjectItemCaseSensitive(settings, "actions");
    for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(actions, tube_sb_names[i]);
        if (cJSON_IsNumber(value) && value->valuedouble >= 0 && value->valuedouble <= 3 &&
            value->valuedouble == floor(value->valuedouble)) policy->actions[i] = value->valueint;
    }
    const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(settings, "minimum_seconds");
    if (cJSON_IsNumber(minimum) && isfinite(minimum->valuedouble) &&
        minimum->valuedouble >= 0 && minimum->valuedouble <= 60) policy->minimum = minimum->valuedouble;
    policy->no_repeat = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(settings, "no_repeat"));
    const cJSON *channel;
    cJSON_ArrayForEach(channel, cJSON_GetObjectItemCaseSensitive(settings, "excluded_channels")) {
        if (cJSON_IsString(channel) && tube_sb_channel_valid(channel->valuestring) &&
            !tube_sb_excluded(policy, channel->valuestring) && policy->excluded_count < TUBE_SB_EXCLUSIONS)
            memcpy(policy->excluded[policy->excluded_count++], channel->valuestring, 25);
    }
}
static void tube_sb_save(const tube_sb_policy *policy, cJSON *root) {
    cJSON *settings = cJSON_AddObjectToObject(root, "sponsor_settings");
    cJSON *actions = cJSON_AddObjectToObject(settings, "actions");
    for (int i = 0; i < TUBE_SB_CATEGORIES; ++i)
        cJSON_AddNumberToObject(actions, tube_sb_names[i], policy->actions[i]);
    cJSON_AddNumberToObject(settings, "minimum_seconds", policy->minimum);
    cJSON_AddBoolToObject(settings, "no_repeat", policy->no_repeat);
    cJSON *channels = cJSON_AddArrayToObject(settings, "excluded_channels");
    for (int i = 0; i < policy->excluded_count; ++i)
        cJSON_AddItemToArray(channels, cJSON_CreateString(policy->excluded[i]));
}
#endif
