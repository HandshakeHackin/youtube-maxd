/* ProsperoTube native YouTube provider; original work, no Android dependency.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_provider.h"
#include "tube_i18n.h"
#include "evo_net.h"
#include "evo_data_path.h"
/* Defined by the package build (scripts/build_ps5.sh, on by default) and by
 * the one host test of this mode; every other build keeps the LAN companion. */
#ifdef TUBE_EMBEDDED_SERVICE
#include "tube_service.h"
#include <unistd.h>
#endif
#include "evo_error.h"
#include "cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdint.h>
#include "tube_video_geometry.h"
#include "tube_hls_dvr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "tube_sponsor.h"
#include "tube_remote.h"
#include "tube_search_query.h"
#include "tube_channel_name.h"
#include "tube_random.h"
#include "tube_player_extra.h"
#include "tube_ui_style.h"
#include "tube_password.h"
#include "tube_qr_page.h"
#include "tube_about.h"
#include "tube_backup.h"
#include "tube_update.h"
/* Host tests link the provider without the updater: each use asks for the function first. */
void tube_update_app_init(void) __attribute__((weak));
void tube_update_app_shutdown(void) __attribute__((weak));
void tube_update_app_tick(int service_ready) __attribute__((weak));
void tube_update_app_classic_notice(void) __attribute__((weak));
int tube_afterglow_enabled(void) __attribute__((weak));
int tube_update_app_rows(tube_update_row *rows, int capacity) __attribute__((weak));
int tube_update_app_action(const char *action) __attribute__((weak));
const char *tube_update_app_notes(void) __attribute__((weak));
const char *tube_update_app_notes_title(void) __attribute__((weak));
#include <sys/stat.h>
#ifdef EVO_TARGET_PS5
/* The application sandbox has no working opendir(). */
#include "evo_readdir.h"
typedef evo_dir_t import_dir;
#define import_opendir evo_opendir
#define import_readdir evo_readdir
#define import_closedir evo_closedir
#else
#include <dirent.h>
typedef DIR import_dir;
#define import_opendir opendir
#define import_readdir readdir
#define import_closedir closedir
#endif

#define RECORDS 384
#define REQUESTS 8
#define CHOICES 8
#define SEGMENTS 128
#define LOCAL_PAGE 24
#define CHAT_HISTORY 256
#define LIBRARY_PAGES 1001

typedef struct {
    evo_provider_item_t item;
    char profile[33];
    char dearrow_title[256], dearrow_art[EVO_PROVIDER_MAX_URL];
    int favorite, queued, subscribed, queued_music;
    long long updated, queue_order;
    int64_t live_utc, live_sequence, live_segment_utc, live_duration_us;
    char live_origin[512], live_format[128];
    char history_remove_cursor[65];
    unsigned history_scope;
    long long history_deadline;
    char feedback_cursor[65];
    unsigned feedback_scope, feedback_options; /* 1 = not interested, 2 = channel, 4 = hide */
    long long feedback_deadline;
} Record;

typedef struct { int height, max_fps, codec; } ResolveQuality;
typedef struct { char playlist[101]; int index; unsigned scope; } PlaylistContext;
static unsigned g_playlist_scope = 1;
static PlaylistContext g_resolved_playlist, g_active_playlist;
static struct { char id[12]; PlaylistContext value; } g_selected_playlist;
static struct {
    unsigned serial, selection;
    int state, lookup, target_index, direction, scan_next;
    PlaylistContext context;
    char id[12];
    tube_playlist_window_t value;
} g_playlist_window;
static int g_playlist_eof_wait, g_playlist_next_ready;
static struct { long long deadline; unsigned scope; int focus, play; } g_next_offer;
static int g_manual_direction;
static int g_manual_fallback;
static unsigned g_manual_scope,g_manual_selection;
static int g_playlist_eof_target;
static int g_playlist_shuffle_attempted;
static tube_playlist_window_t g_playlist_shuffle_source;
static evo_provider_item_t g_playlist_next;

typedef struct Request {
    int used, kind, stage, inflight;
    unsigned generation;
    long long deadline, next_poll;
    char job[64], payload[2048];
    evo_provider_items_cb items_cb;
    evo_provider_resolve_cb resolve_cb;
    void *ud;
    evo_stream_choice_t choices[CHOICES];
    int choice_count;
    ResolveQuality quality;
    PlaylistContext playlist;
    int page, music_origin, queue_origin, incognito;
    int search_rows, library_search, aggregate, typed_suggest;
    int wants_health, health_tried, channel_upgrade, channel_page, empty_pages, new_content, startup, codec_fallback;
    unsigned intent, requires;
    char parent[160];
    char profile[33];
    int64_t watch_position;
    int has_chat_position;
    int chat_page;
    int64_t chat_position_ms;
    char auth_source[512], auth_token[128];
    char caption_language[33];
} Request;

static Record g_records[RECORDS];
static struct { char profile[33],channel[25],name[256]; } g_blocked[128];
static int g_blocked_count;
static unsigned g_blocked_revision;
static int g_count;
static int g_store_writable = 1;
static Request g_requests[REQUESTS];
static unsigned g_generation = 1;
static struct { unsigned generation; int unavailable, queue_origin; char id[12], profile[33]; } g_resolve_failure;
static char g_queue_failed[RECORDS][12];
static int g_queue_failed_count;
static char g_source[512], g_token[128], g_pin[7];
/* The built-in service. While it is this run's source, g_source and g_token
 * are its loopback address and token, held in memory only; the address and
 * token of an external companion read from the store are kept aside and
 * written back unchanged. */
static int g_embedded, g_embedded_stopped;
static char g_saved_source[512], g_saved_token[128], g_embedded_error[160];
static char g_caption_path[512], g_active_id[160], g_next_id[160];
static char g_caption_language[33];
static char g_secondary_caption_path[512], g_secondary_caption_language[33];
#define CAPTION_TRACKS 256
static struct { char language[33], name[97], target[33], url[EVO_PROVIDER_MAX_URL]; int automatic, translated; } g_caption_tracks[CAPTION_TRACKS];
static int g_caption_track_count;
static unsigned g_caption_revision, g_caption_serial;
static unsigned g_selection_serial;
static _Atomic unsigned g_live_token;
static unsigned g_live_epoch;
static pthread_mutex_t g_live_mutex = PTHREAD_MUTEX_INITIALIZER;
static tube_live_owner g_live_binding;
static void live_retire(void) { atomic_store(&g_live_token, 0); }
int tube_live_owner_current(unsigned token) { return token && token == atomic_load(&g_live_token); }
static char g_caption_video[12], g_caption_profile[33], g_caption_source[512], g_caption_token[128];
static struct { int phase, sequence; unsigned owner, serial, selection; long long deadline;
    char language[33], media[512]; int secondary; } g_caption_switch;
static void caption_switch_tick(void);
static char g_active_profile[33], g_next_profile[33];
/* Navigation origin, not a content classifier. Resolve snapshots the origin
 * before asynchronous work; queue entries retain the origin of their insertion. */
static int g_browse_music, g_browse_queue, g_resolved_music;
static struct { char id[160], profile[33]; int music, automatic; } g_launch_origin;
static const int g_height = 2160; /* Every new video may use the full supported resolution. */
static int g_max_fps, g_codec, g_auto_captions, g_captions = 1;
static int g_speed = 100;
/* Modes: all videos, per channel, per video, this session only. */
static int g_speed_mode = 1, g_last_speed = 100;
#define SPEED_MEMORIES 1024
static struct { char profile[33], key[25]; int mode, percent; } g_speeds[SPEED_MEMORIES];
static int g_speed_count;
static char g_speed_session_id[160], g_speed_session_profile[33];
static int g_speed_session = 100;
static const int tube_speeds[] = {25,50,75,80,85,90,95,100,105,110,115,120,125,130,135,140,145,150,160,175,180,200,225,250,275,300,325,350,375,400};
/* The long list above, the speed list the profile chose, and any step of the
 * extra-long list: a speed saved under one list stays valid under another. */
static int valid_speed(double speed) {
    for (unsigned i = 0; i < sizeof tube_speeds / sizeof *tube_speeds; ++i)
        if (speed == tube_speeds[i]) return 1;
    if (!(speed >= 1 && speed <= 1000) || floor(speed) != speed) return 0;
    if (tube_extra_speed_valid((int)speed)) return 1;
    for (int i = 0; i < tube_speed_list_count(2); ++i) if (tube_speed_list_at(2,i) == (int)speed) return 1;
    return 0;
}
static void save(void);
static int g_sponsor, g_autoplay, g_finished, g_ask_stream;
static int g_playback_mode;
static struct { int state,same_playlist; unsigned serial,scope,selection; char id[12],next[12],channel[25],channel_name[256]; PlaylistContext next_context; } g_recommendation={.state=-1};
static struct {
    int state,index,target,total; /* 0=request pending, 1=ready, 2=next group window */
    unsigned serial,owner,selection,scope,blocks;
    char id[12],next[12],playlist[101],group[43];
    char next_playlist[101];int next_index; /* returned YouTube playlist occurrence */
} g_phone_next={.state=-1};
static int g_completion_action, g_completion_consumed;
static unsigned g_completion_scope;
static int g_sponsor_completed;
static int g_sponsor_seek_pending;
static double g_sponsor_seek_target;
static int g_queue_respects_mode;
static int queue_mode_allows_advance(void) {
    return g_autoplay || ((g_playback_mode==TUBE_MODE_PAUSE || g_playback_mode==TUBE_MODE_CLOSE) && !g_queue_respects_mode);
}
static int playback_mode_valid(int mode) { return mode>=0 && mode<=6; }
static int playback_mode_continues(int mode) { return mode!=TUBE_MODE_PAUSE && mode!=TUBE_MODE_CLOSE; }
static int g_player_chat_mode;
static int g_sleep_minutes, g_sleep_armed, g_sleep_fired;
static long long g_sleep_activity_ms;
static int g_remote_enabled, g_remote_identity_saved, g_remote_write_failed, g_remote_entropy_failed;
static int g_remote_stop_on_disconnect;
static int g_remote_close_on_disconnect;
static char g_remote_device[37], g_remote_screen[129];
static void remote_configure(void);
static tube_player_chat_row_t g_player_chat_rows[6];
static int g_player_chat_count;
static double g_player_chat_position = -1;
/* SmartTube GeneralData history states. Disabled also stops local history and resume. */
enum { TUBE_HISTORY_AUTO=0, TUBE_HISTORY_ENABLED=1, TUBE_HISTORY_DISABLED=2 };
static int g_history_state;
static int g_search_history = 1;
/* Interface preferences of the selected profile (tube_ui_style.h). The
 * revision changes whenever the browser must restyle itself. */
static TubeUiPrefs g_ui;
static unsigned g_ui_revision = 1;
static uint64_t g_app_exit_armed, g_search_exit_armed;
const struct TubeUiPrefs *tube_ui_prefs(void) { return &g_ui; }
unsigned tube_ui_revision(void) { return g_ui_revision; }
int tube_app_exit_press(uint64_t now_ms) {
    g_search_exit_armed = 0; return tube_ui_press(g_ui.app_exit, now_ms, &g_app_exit_armed);
}
int tube_search_exit_press(uint64_t now_ms) {
    g_app_exit_armed = 0;
    return tube_ui_press(g_ui.search_exit ? TUBE_UI_EXIT_DOUBLE : TUBE_UI_EXIT_SINGLE, now_ms, &g_search_exit_armed);
}
void tube_ui_press_reset(void) { g_app_exit_armed = g_search_exit_armed = 0; }
/* Device-owned, like the other phone and startup preferences. */
static int g_remote_no_history, g_select_account_on_boot;
/* Root sections a profile may open at startup; "" keeps the main menu. */
static const struct { const char *id, *label; } g_boot_sections[] = {
    {"", "Main menu"}, {"library:home", "Home"}, {"library:subscriptions", "Subscriptions"},
    {"library:history", "YouTube history"}, {"library:playlists", "Playlists"}, {"library:watch_later", "Watch later"},
    {"section:gaming", "Gaming"}, {"section:news", "News"}, {"section:music", "Music"},
    {"section:sports", "Sports"}, {"section:live", "Live"}, {"section:movies", "Movies"},
    {"history", "Continue watching & history"}, {"favorites", "Favorites"}, {"queue", "Play queue"},
    {"subscriptions", "Followed channels"}, {"discover", "Discover"},
    {"section:trending", "Trending"}, {"section:shorts", "Shorts"}, {"library:channels", "Subscribed channels"},
    {"library:notifications", "Notifications"}, {"section:my_videos", "My videos"}, {"section:kids", "Kids"}
};
#define BOOT_SECTIONS ((int)(sizeof g_boot_sections / sizeof *g_boot_sections))
static char g_boot_section[24];
/* A password-protected profile that was selected when ProsperoTube closed:
 * browsing starts as Guest and the startup folder asks for its password. */
static char g_startup_lock[33];
/* Until a launch lists anything but the main menu: the startup section is the first list asked for. */
static int g_startup_open;
static int boot_section_index(const char *id) {
    for (int i = 1; id && i < BOOT_SECTIONS; ++i) if (!strcmp(id, g_boot_sections[i].id)) return i;
    return 0;
}
const char *tube_boot_section(void) {
    static char locked[64];
    if (!*g_startup_lock) return g_boot_section;
    snprintf(locked, sizeof locked, "library:profile:%s", g_startup_lock); return locked;
}
const char *tube_boot_section_title(void) {
    return *g_startup_lock ? "Accounts & library" : g_boot_sections[boot_section_index(g_boot_section)].label;
}
int tube_select_account_on_boot(void) { return g_select_account_on_boot; }
/* Main-menu and Discover rows a profile may hide or reorder. The table index is
 * the row's saved identity; settings, player settings and close stay fixed. */
enum { SECTION_ROOT, SECTION_DISCOVER };
static const struct { const char *id, *label, *detail; int group; } g_sections[] = {
    {"discover", "Discover", "Search public videos by topic", SECTION_ROOT},
    {"search-history", "Recent searches", "Repeat or forget queries saved for the selected profile", SECTION_ROOT},
    {"history", "Continue watching & history", "Stored on this PS5", SECTION_ROOT},
    {"favorites", "Favorites", "Your local saved videos", SECTION_ROOT},
    {"queue", "Play queue", "R2 on a video to add or remove", SECTION_ROOT},
    {"subscriptions", "Followed channels", "Local follows, independent of Google accounts", SECTION_ROOT},
    {"blocked-channels", "Blocked channels", "Manage channel blocks for the selected profile", SECTION_ROOT},
    {"library:home", "Home recommendations", "From the companion library service", SECTION_DISCOVER},
    {"library", "Accounts & library", "Profiles, subscriptions, history and playlists", SECTION_DISCOVER},
    {"search:science", "Science", "Explore science videos", SECTION_DISCOVER},
    {"search:technology", "Technology", "Explore technology videos", SECTION_DISCOVER},
    {"section:gaming", "Gaming", "YouTube gaming shelves", SECTION_DISCOVER},
    {"section:news", "News", "YouTube news shelves", SECTION_DISCOVER},
    {"section:music", "Music", "YouTube music shelves", SECTION_DISCOVER},
    {"search:documentary", "Documentaries", "Explore documentaries", SECTION_DISCOVER},
    {"search:education", "Learning", "Explore educational videos", SECTION_DISCOVER},
    {"section:sports", "Sports", "YouTube sports shelves", SECTION_DISCOVER},
    {"section:live", "Live", "YouTube live shelves", SECTION_DISCOVER},
    {"section:movies", "Movies", "YouTube movies and shows", SECTION_DISCOVER},
    {"section:trending", "Trending", "Videos trending on YouTube", SECTION_DISCOVER},
    {"section:shorts", "Shorts", "Short videos from YouTube", SECTION_DISCOVER},
    {"section:kids", "Kids", "YouTube Kids categories", SECTION_DISCOVER}
};
#define SECTIONS ((int)(sizeof g_sections / sizeof *g_sections))
#define SECTION_SLOTS 32
_Static_assert(sizeof g_sections / sizeof *g_sections <= SECTION_SLOTS, "section flags are one unsigned");
static unsigned char g_section_order[SECTION_SLOTS];
static unsigned g_section_hidden;
static int section_index(const char *id) {
    for (int i = 0; id && i < SECTIONS; ++i) if (!strcmp(id, g_sections[i].id)) return i;
    return -1;
}
/* Channels and playlists pinned to the main menu, in pin order for each profile. */
#define PINS 256
#define PROFILE_PINS 24
static struct { char profile[33], id[119], title[EVO_PROVIDER_MAX_TITLE]; } g_pins[PINS];
static int g_pin_count;
static int pin_find(const char *profile, const char *id);
static int followed(const char *channel);
static void unfollow(const char *channel);
/* Folder a section menu was opened on; supplies the title of a new pin. */
static struct { char id[119], title[EVO_PROVIDER_MAX_TITLE], profile[33]; unsigned scope; } g_menu_target;
/* SmartTube keeps search filters for one search session; they are never saved.
 * Mixed channel and playlist results are a saved choice of the profile. */
static const char *const g_search_dates[] = {"any", "today", "week", "month", "year"};
static const char *const g_search_date_labels[] = {"Any time", "Today", "This week", "This month", "This year"};
static const char *const g_search_durations[] = {"any", "short", "medium", "long"};
static const char *const g_search_duration_labels[] = {"Any duration", "Under 3 minutes", "3 to 20 minutes", "Over 20 minutes"};
static const char *const g_search_types[] = {"all", "video", "channel", "playlist", "movie", "shorts"};
static const char *const g_search_type_labels[] = {"All types", "Videos", "Channels", "Playlists", "Movies", "Shorts"};
static const char *const g_search_orders[] = {"relevance", "view_count"};
static const char *const g_search_order_labels[] = {"Relevance", "View count"};
static const char *const g_search_features[] = {"live", "4k", "hdr", "hd", "subtitles", "creative_commons", "360", "vr180", "3d", "location", "purchased"};
static const char *const g_search_feature_labels[] = {"Live", "4K", "HDR", "HD", "Subtitles", "Creative Commons", "360 degrees", "VR180", "3D", "Location", "Purchased"};
#define SEARCH_FEATURES 11
static struct { char profile[33]; int date, duration, type, order; unsigned features; } g_search_options;
static int g_search_cards, g_search_typed;
/* The typed query that Search options, Suggestions and Search again refer to. */
static struct { char query[801], profile[33]; unsigned scope; } g_last_search;
static struct { char text[15][801], profile[33]; unsigned scope; int count; } g_suggestions;
/* Filtered results come from one retained companion feed: later pages continue
 * it by cursor, so they keep the options of the first page. */
static struct { char parent[808], key[32], profile[33]; unsigned scope, serial, unavailable; } g_search_feed;
/* Subscribed channels: 0 = the account's order, 1 = by name, 2 = channels with
 * new content first. Sorting needs the whole list, which is then served from memory. */
static int g_channel_sort;
static const char *const g_channel_order_labels[] = {"account order", "by name", "new content first"};
#define CHANNEL_ROWS 480
typedef struct { char id[33], title[EVO_PROVIDER_MAX_TITLE], subtitle[65], art[EVO_PROVIDER_MAX_ART_URL]; int has_new, order; } ChannelRow;
static ChannelRow g_channel_rows[CHANNEL_ROWS];
static struct { int count, complete; unsigned scope; char profile[33]; } g_channel_list;
/* Channels the last Subscribed channels listing showed as new, for "Mark all as watched". */
static struct { char channel[CHANNEL_ROWS][25], profile[33]; int count; unsigned scope; } g_channel_new;
/* "Mark as watched": YouTube's guide keeps its new-content mark until the
 * channel's uploads are opened, so each profile remembers the channels it has
 * seen (with the newest video listed then, when known). A remembered channel
 * is not shown as new; the entry leaves when YouTube's own mark clears, so the
 * next upload marks the channel again. Bounded; the oldest entry leaves first. */
#define CHANNEL_MARKS 512
static struct { char profile[33], channel[25], video[12]; long long seen; } g_channel_marks[CHANNEL_MARKS];
static int g_channel_mark_count, g_channel_marks_dirty;
/* Channels followed without a video of theirs: a subscriptions import. */
#define FOLLOWS 512
static struct { char profile[33], channel[25], title[97]; } g_follows[FOLLOWS];
static int g_follow_count;
/* One parsed import file waiting for its confirmation. */
static struct { unsigned serial; int count, listed, skipped, truncated, applied, added, existing, full;
    char file[64], format[16], profile[33]; char *body;
    struct { char channel[25], title[97]; } rows[FOLLOWS]; } g_import;
/* The account playlist each profile last added a video to or removed one from. */
#define RECENT_PLAYLISTS 64
static struct { char profile[33], playlist[101], title[EVO_PROVIDER_MAX_TITLE]; } g_recent_playlists[RECENT_PLAYLISTS];
static int g_recent_playlist_count;
static struct { char playlist[101], title[EVO_PROVIDER_MAX_TITLE]; } g_picker_rows[48];
static unsigned g_picker_cursor;
/* "Remember subtitles per channel": captions on or off, chosen while watching. */
#define CAPTION_MEMORIES 512
static struct { char profile[33], channel[25]; int enabled; } g_caption_memories[CAPTION_MEMORIES];
static int g_caption_memory_count;
/* Per-profile switches of the third batch; device-owned auto-backup period. */
static int g_more_subtitles, g_caption_memory, g_dislikes;
static int g_backup_auto = TUBE_BACKUP_AUTO_OFF;
static unsigned g_backup_ticket;
static char g_backup_notice[2][160];
/* Upcoming videos of the current browsing scope: their menu offers a reminder. */
static struct { char id[12]; unsigned scope; } g_upcoming[64];
static unsigned g_upcoming_cursor;
/* The discussion being read: comments are posted to its video. */
static struct { char video[12], profile[33], count[33]; unsigned scope; int can_post; } g_comment_thread;
/* Rows of a details or description page while the dislike relay is asked. */
static struct { evo_provider_item_t rows[12]; int count; char video[12]; } g_detail;
static const char *const g_playlist_orders[] = {"added_newest", "added_oldest", "popular", "published_newest", "published_oldest"};
static const char *const g_playlist_order_labels[] = {"Date added, newest first", "Date added, oldest first", "Most popular",
    "Date published, newest first", "Date published, oldest first"};
static const char *const g_notify_levels[] = {"all", "personalized", "none"};
static const char *const g_notify_labels[] = {"All notifications", "Personalized notifications", "No notifications"};
/* Full description shown by the long-text reader; owned by one browsing scope. */
static struct { char id[40], content[60004], profile[33]; unsigned scope; } g_description;
/* Home cards the profile dismissed in this session; an entry with a channel
 * hides that channel's other Home cards too. A card hidden with "Hide from
 * this list" also leaves the Subscriptions list. */
static struct { char id[12], channel[25], profile[33]; unsigned scope; int hidden; } g_dismissed[64];
static unsigned g_dismissed_cursor;
/* Play from start and Play incognito start their video from the action itself:
 * the host takes it once; reloading the action screen afterwards only offers it. */
static struct { char id[12], profile[33], offered[12]; unsigned scope; int offered_action; } g_menu_launch;
static struct { char rows[2][112], video[12], peers[2][12], profile[33]; unsigned scope, serial; } g_queue_move;
/* One-shot private playback: armed by its video action, bound by the next
 * resolve of that video, retired by any other navigation or scope change. */
static struct { char id[12], profile[33]; unsigned scope; } g_incognito;
static int g_resolved_incognito, g_active_incognito;
/* Play from start applies once per menu visit: reloading its screen after the
 * playback it started must not erase the newly saved position. */
static struct { char id[12]; unsigned scope, selection; } g_restart;
/* Caption, chat and sign-in host tests link the provider without the phone adapter. */
int tube_phone_owns_media(void) __attribute__((weak));
/* SmartTube VideoStateController.updateHistory: incognito, disabled history and
 * opted-out phone playback write neither local history nor account reports. */
static int history_suppressed(void) {
    return g_active_incognito || g_history_state == TUBE_HISTORY_DISABLED ||
        (g_remote_no_history && tube_phone_owns_media && tube_phone_owns_media());
}
static int g_paid_notifications;
static int g_dearrow_titles, g_dearrow_thumbnails;
static char g_paid_text[256];
static int g_paid_shown;
static char g_resolved_id[160], g_resolved_profile[33];
static ResolveQuality g_resolved_quality;
static unsigned g_watch_serial;
static struct { unsigned serial; char id[160], profile[33], session[64];
    int dirty, stopped, failed, status, sent;
    int64_t position; long long last_sent; } g_watch;
static void watch_flush(int terminal_flush);
static void watch_reset(int clear_resolution) {
    memset(&g_watch, 0, sizeof g_watch);
    if (!++g_watch_serial) ++g_watch_serial;
    g_watch.serial = g_watch_serial;
    if (clear_resolution) {
        live_retire();
        if (!++g_playlist_scope) ++g_playlist_scope;
        memset(&g_selected_playlist,0,sizeof g_selected_playlist);
        memset(&g_resolved_playlist,0,sizeof g_resolved_playlist);
        memset(&g_active_playlist,0,sizeof g_active_playlist);
        g_resolved_id[0] = 0; g_resolved_profile[0] = 0; g_resolved_music = 0;
        g_browse_music = g_browse_queue = 0;
        memset(&g_launch_origin, 0, sizeof g_launch_origin);
        memset(&g_incognito, 0, sizeof g_incognito); g_resolved_incognito = 0;
        memset(&g_restart, 0, sizeof g_restart);
    }
}
static char g_language[16] = "en", g_audio_language[16];
static double g_segments[SEGMENTS][2];
static unsigned char g_segment_categories[SEGMENTS], g_segment_seen[SEGMENTS];
static tube_sb_policy g_sb;
static unsigned g_sb_serial, g_sb_active_serial;
static char g_sb_source[512];
static char g_sb_channel[25], g_sb_profile[33], g_sb_prompt_text[128];
static int g_sb_prompt = -1, g_sb_message;
static double g_sb_prompt_start, g_sb_prompt_end, g_sb_last_position = -1, g_sb_finish_target = -1;
static int g_highlight_guard = -1;
static double g_highlight_origin;
static int g_segment_count, g_skip_guard = -1;
static double g_skip_until;
static struct { double from, to; int category, pending; long long shown; } g_sb_skip;
static double g_sb_undo_from = -1, g_sb_undo_until;
static const unsigned g_sb_colors[TUBE_SB_CATEGORIES] = {
    0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xffff00, 0xff9900, 0x2793e6, 0x7300ff, 0xff1684
};
static double g_chapters[128];
static int g_chapter_count;
static void sponsor_invalidate(void) {
    memset(&g_sb_skip, 0, sizeof g_sb_skip); g_sb_undo_from = -1;
    g_paid_text[0] = 0; g_paid_shown = 0;
    if (!++g_sb_serial) ++g_sb_serial;
    g_sb_active_serial = 0; g_sb_source[0] = 0; g_segment_count = 0;
    g_sb_prompt = -1; g_skip_guard = -1; g_sb_finish_target = -1;
    g_highlight_guard = -1;
}
static long long g_last_save, g_queue_order;
static char g_library_profile[33] = "guest", g_library_parent[160];
/* SmartTube "Use separate settings per account". Device-owned; off = one set
 * for every profile. Files written before the switch existed were per profile. */
#define SHARED_SETTINGS "*"
static int g_separate_settings;
static const char *settings_key(void) { return g_separate_settings ? g_library_profile : SHARED_SETTINGS; }
static int settings_current(const char *profile) { return !g_separate_settings || !strcmp(profile, g_library_profile); }
#define PLAYER_PROFILES 64
static struct { char profile[33]; int ok_mode, hide_ms, seek_mode, seek_seconds, time_mode, resize_mode, zoom_percent, aspect_mode, rotation_degrees, mirror_enabled; } g_player_preferences[PLAYER_PROFILES];
static int g_player_preferences_count;
static _Atomic unsigned g_video_requested, g_video_geometry;
static int g_pixel_mode;
static struct { int phase, attempts, subtitles; double position;
    char source[512], token[128], profile[33], id[160];
    evo_stream_choice_t choice; ResolveQuality quality; } g_recovery;
void tube_recovery_finish(void) { g_recovery.phase = 0; }
static int recovery_scope(void) {
    return *g_recovery.id && !strcmp(g_recovery.id, g_active_id) &&
        !strcmp(g_recovery.profile, g_active_profile) && !strcmp(g_recovery.profile, g_library_profile) &&
        !strcmp(g_recovery.source, g_source) && !strcmp(g_recovery.token, g_token);
}
int tube_recovery_active(void) { return g_recovery.phase == 2 && recovery_scope(); }
int tube_recovery_pending(void) { return g_recovery.phase == 1 && recovery_scope(); }
static char g_library_cursors[LIBRARY_PAGES][65];
static struct { char id[160], content[16385], rating[65], respond[65], profile[33]; unsigned rating_options; } g_comments[LOCAL_PAGE + 1];
static struct { char id[160], content[16385]; } g_chat_messages[CHAT_HISTORY];
static unsigned g_account_serial;
static unsigned g_accounts_revision;
unsigned tube_accounts_revision(void) { return g_accounts_revision; }
static void account_removed(void);
enum { AUTH_IDLE, AUTH_STARTING, AUTH_PENDING, AUTH_AUTHORIZED, AUTH_CANCELED,
       AUTH_EXPIRED, AUTH_DENIED, AUTH_FAILED };
static struct { int state; unsigned serial, revision;
    char session[33], code[33], url[64], profile[33], name[601], error[256];
    long long deadline, next_poll; } g_auth;
static void ignored(int ok, int status, const char *body, size_t size, void *ud);
static void auth_cleanup_request(Request *r, const cJSON *root);
static void chat_cleanup_request(Request *r, const cJSON *root);
static void cancel_requests(int preserve_chat);
static void chat_close_remote(const char *source, const char *token, const char *profile, const char *cursor);
static struct { int state, player, replay, seek; unsigned serial; long long next_poll;
    int reader_page, reader_more;
    int64_t position_ms, seek_position_ms, reader_position_ms;
    char parent[160], video[12], mode[8], profile[33], cursor[65], source[512], token[128]; } g_chat;
static struct { int phase, pending; unsigned ticket, chat_serial;
    char text[801], session[65], cursor[65], video[12], profile[33]; } g_message;
static int message_scope(void) {
    return g_message.ticket && g_message.chat_serial == g_chat.serial && g_chat.state == 2 && !g_chat.replay &&
        !strcmp(g_message.profile, g_library_profile) && !strcmp(g_message.profile, g_chat.profile) &&
        !strcmp(g_message.video, g_chat.video) && !strcmp(g_message.cursor, g_chat.cursor) &&
        !strcmp(g_chat.source, g_source) && !strcmp(g_chat.token, g_token);
}
static struct {
    unsigned serial;
    int state; /* 0 confirmation, 1 submitted, 2 confirmed, -1 unconfirmed */
    char action[32], identity[101], profile[33], playlist[101], title[601], history_cursor[65];
    char argument[65], session[65]; /* a reminder, order, notification level or reply reference; the one attempt */
} g_account_intent;
/* The text prompt also asks for a search inside a channel and for profile
 * passwords: lock 1 = give it to select target, 2 = set a new one, 3 = give it
 * to change it, 4 = give it to remove it. comment 1 = a new comment, 2 = a reply.
 * server = the alternative SponsorBlock server of the built-in service. */
static struct { tube_playlist_prompt_t prompt; int pending, active, create, lock, search, comment, server;
    char identity[101], profile[33], pin[119], target[33], cursor[65]; } g_playlist_prompt;
/* First-release scope (GOAL.md): what ships before a console and live-service
 * check carries this suffix on its rows and this line where a row has an overview. */
#define EXPERIMENTAL " (experimental)"
#define EXPERIMENTAL_NOTE "Experimental: tested offline only, not yet on a PS5 or against live YouTube. It may fail or change in a later version."
static void experimental(evo_provider_item_t *item) { snprintf(item->overview, sizeof item->overview, "%s", EXPERIMENTAL_NOTE); }
static const struct { const char *action, *title; int channel; } g_account_actions[] = {
    {"like", "Like video", 0}, {"dislike", "Dislike video", 0}, {"unrate", "Remove video rating", 0},
    {"watch_later_add", "Add to YouTube Watch later", 0},
    {"watch_later_remove", "Remove from YouTube Watch later", 0},
    {"subscribe", "Subscribe on YouTube", 1}, {"unsubscribe", "Unsubscribe on YouTube", 1},
    {"history_remove", "Remove from YouTube history", 0},
    {"mark_watched", "Mark as watched on YouTube", 0},
    {"not_interested", "Not interested" EXPERIMENTAL, 0}, {"dont_recommend_channel", "Don't recommend channel" EXPERIMENTAL, 0},
    {"hide", "Hide from this list" EXPERIMENTAL, 0}
};

static int valid_cursor(const char *value) {
    return value && *value && strlen(value) <= 64 &&
        strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(value);
}

const char *tube_comment_text(const char *id) {
    if (id && *g_description.id && !strcmp(id, g_description.id) && g_description.scope == g_playlist_scope &&
        !strcmp(g_description.profile, g_library_profile)) return g_description.content;
    if (id) for (int i = 0; i < CHAT_HISTORY; ++i)
        if (*g_chat_messages[i].id && !strcmp(id, g_chat_messages[i].id)) return g_chat_messages[i].content;
    if (id) for (int i = 0; i <= LOCAL_PAGE; ++i)
        if (*g_comments[i].id && !strcmp(id, g_comments[i].id)) return g_comments[i].content;
    return "";
}

const char *tube_comment_rating(const char *id) {
    if (id && strcmp(g_library_profile, "guest")) for (int i = 0; i < LOCAL_PAGE; ++i)
        if (!strcmp(g_comments[i].id, id) && !strcmp(g_comments[i].profile, g_library_profile) && g_comments[i].rating_options)
            return g_comments[i].rating;
    return "";
}

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int tube_sleep_minutes(void) { return g_sleep_minutes; }
void tube_sleep_activity(void) {
    g_sleep_activity_ms = now_ms(); g_sleep_armed = g_sleep_minutes > 0; g_sleep_fired = 0;
}
int tube_sleep_set_minutes(int minutes) {
    if (minutes < 0 || minutes > 600 || minutes % 30) return 0;
    g_sleep_minutes = minutes; tube_sleep_activity(); save(); return 1;
}
int tube_sleep_tick(int playback_active) {
    if (!playback_active || !g_sleep_minutes || g_sleep_fired) return 0;
    if (!g_sleep_armed) tube_sleep_activity();
    long long current = now_ms();
    if (current < g_sleep_activity_ms) { tube_sleep_activity(); return 0; }
    if (current - g_sleep_activity_ms < (long long)g_sleep_minutes * 60000) return 0;
    g_sleep_fired = 1; return 1;
}

int tube_chat_due(const char *parent) {
    if (!parent || g_chat.state != 2 || strcmp(parent, g_chat.parent) ||
        strcmp(g_chat.profile, g_library_profile) || !valid_cursor(g_chat.cursor) || now_ms() < g_chat.next_poll) return 0;
    for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) return 0;
    if (g_chat.replay && !g_chat.player && !g_chat.seek) return 0;
    return 1;
}

double tube_chat_reader_position(const char *parent) {
    return parent && !g_chat.player && g_chat.replay && (g_chat.state == 2 || g_chat.state == 3) &&
        !strcmp(parent, g_chat.parent) && !strcmp(g_chat.profile, g_library_profile) &&
        !strcmp(g_chat.source, g_source) && !strcmp(g_chat.token, g_token) ? g_chat.reader_position_ms / 1000.0 : -1;
}
int tube_chat_reader_seek(const char *parent, double position_sec) {
    if (tube_chat_reader_position(parent) < 0 || !isfinite(position_sec) || position_sec < 0 || position_sec > 1000000) return 0;
    for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) return 0;
    g_chat.reader_position_ms = (int64_t)floor(position_sec * 1000 + .000001);
    g_chat.reader_page = 0; g_chat.reader_more = 0;
    g_chat.seek = 1; g_chat.state = 2; g_chat.next_poll = 0;
    return 1;
}
int tube_chat_reader_next(const char *parent) {
    if (tube_chat_reader_position(parent) < 0 || !g_chat.reader_more || g_chat.reader_page >= 1000000) return 0;
    for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) return 0;
    ++g_chat.reader_page; g_chat.reader_more = 0;
    g_chat.seek = 1; g_chat.state = 2; g_chat.next_poll = 0;
    return 1;
}

void tube_chat_stop(void) {
    g_player_chat_position = -1;
    memset(g_player_chat_rows, 0, sizeof g_player_chat_rows); g_player_chat_count = 0;
    memset(g_chat_messages, 0, sizeof g_chat_messages);
    chat_close_remote(g_chat.source, g_chat.token, g_chat.profile, g_chat.cursor);
    unsigned serial = g_chat.serial + 1;
    memset(&g_chat, 0, sizeof g_chat); g_chat.serial = serial ? serial : 1;
    memset(&g_message, 0, sizeof g_message);
    memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
}

static void copy(char *out, size_t cap, const char *src) {
    if (!cap) return;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
        while (n && ((unsigned char)src[n] & 0xc0) == 0x80) --n;
    }
    memcpy(out, src, n); out[n] = 0;
}

static const char *str(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsString(v) && v->valuestring ? v->valuestring : "";
}

static double num(const cJSON *o, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, key);
    return cJSON_IsNumber(v) && isfinite(v->valuedouble) && v->valuedouble >= 0 && v->valuedouble <= 1e12 ? v->valuedouble : 0;
}

static int boolean(const cJSON *o, const char *key) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(o, key));
}

static int valid_id(const char *id) {
    if (!id || strlen(id) != 11) return 0;
    for (int i = 0; i < 11; ++i)
        if (!isalnum((unsigned char)id[i]) && id[i] != '_' && id[i] != '-') return 0;
    return 1;
}

static int valid_profile(const char *id) {
    return id && *id && strlen(id) <= 32 &&
        strspn(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(id);
}

static int player_preference(const char *profile, int create) {
    for (int i = 0; i < g_player_preferences_count; ++i)
        if (!strcmp(profile, g_player_preferences[i].profile)) return i;
    if (!create || (!valid_profile(profile) && strcmp(profile,SHARED_SETTINGS)) || g_player_preferences_count == PLAYER_PROFILES) return -1;
    int i = g_player_preferences_count++;
    copy(g_player_preferences[i].profile,sizeof g_player_preferences[i].profile,profile);
    g_player_preferences[i].ok_mode = 0; g_player_preferences[i].hide_ms = 3000;
    g_player_preferences[i].seek_seconds = 10; g_player_preferences[i].time_mode = 1; return i;
}
int tube_player_ok_mode(void) {
    int i = player_preference(settings_key(),0); return i < 0 ? 0 : g_player_preferences[i].ok_mode;
}
int tube_player_hide_ms(void) {
    int i = player_preference(settings_key(),0); return i < 0 ? 3000 : g_player_preferences[i].hide_ms;
}
int tube_player_set_ok_mode(int mode) {
    if (mode < 0 || mode > 3) return 0;
    int i = player_preference(settings_key(),1); if (i < 0) return 0;
    g_player_preferences[i].ok_mode = mode; save(); return 1;
}
int tube_player_set_hide_seconds(int seconds) {
    if (seconds < 0 || seconds > 15) return 0;
    int i = player_preference(settings_key(),1); if (i < 0) return 0;
    g_player_preferences[i].hide_ms = seconds * 1000; save(); return 1;
}
int tube_player_seek_mode(void) {
    int i = player_preference(settings_key(),0); return i < 0 ? 0 : g_player_preferences[i].seek_mode;
}
int tube_player_set_seek_mode(int mode) {
    if (mode < 0 || mode > 2) return 0;
    int i = player_preference(settings_key(),1); if (i < 0) return 0;
    g_player_preferences[i].seek_mode = mode; save(); return 1;
}
static const int seek_intervals[] = {1,2,3,5,7,10,15,20,30,60};
static int valid_seek_seconds(int seconds) {
    for (unsigned i=0;i<sizeof seek_intervals/sizeof *seek_intervals;++i) if (seek_intervals[i]==seconds) return 1;
    return 0;
}
int tube_player_seek_seconds(void) {
    int i=player_preference(settings_key(),0); return i<0 ? 10 : g_player_preferences[i].seek_seconds;
}
int tube_player_set_seek_seconds(int seconds) {
    if (!valid_seek_seconds(seconds)) return 0;
    int i=player_preference(settings_key(),1); if(i<0)return 0;
    g_player_preferences[i].seek_seconds=seconds; save(); return 1;
}
int tube_player_time_mode(void) {
    int i=player_preference(settings_key(),0);return i<0 ? 1 : g_player_preferences[i].time_mode;
}
int tube_player_set_time_mode(int mode) {
    if(mode<0 || mode>2)return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].time_mode=mode;save();return 1;
}
int tube_player_resize_mode(void) {
    int i=player_preference(settings_key(),0);return i<0?0:g_player_preferences[i].resize_mode;
}
int tube_player_zoom_percent(void) {
    int i=player_preference(settings_key(),0);return i<0?0:g_player_preferences[i].zoom_percent;
}
int tube_player_rotation_degrees(void) {
    int i=player_preference(settings_key(),0);return i<0?0:g_player_preferences[i].rotation_degrees;
}
int tube_player_mirror_enabled(void) {
    int i=player_preference(settings_key(),0);return i<0?0:g_player_preferences[i].mirror_enabled;
}
int tube_player_aspect_mode(void) {
    int i=player_preference(settings_key(),0);return i<0?0:g_player_preferences[i].aspect_mode;
}
int tube_player_pixel_mode(void) {return g_pixel_mode;}
unsigned tube_player_video_geometry(void) {return atomic_load(&g_video_geometry);}
void tube_video_geometry_enable(int enabled) {
    atomic_store(&g_video_geometry,enabled?(atomic_load(&g_video_requested)|1u):0);
}
static void video_geometry_publish(void) {
    unsigned value=((unsigned)tube_player_resize_mode()<<1)|((unsigned)tube_player_zoom_percent()<<4)|
        ((unsigned)tube_player_aspect_mode()<<13)|((unsigned)g_pixel_mode<<17)|
        ((unsigned)(tube_player_rotation_degrees()/90)<<20)|((unsigned)tube_player_mirror_enabled()<<22);
    atomic_store(&g_video_requested,value);
    /* Editing another profile affects its next video, not the current picture. */
    if(!strcmp(g_library_profile,g_active_profile)) {
        unsigned old=atomic_load(&g_video_geometry);
        while((old&1) && !atomic_compare_exchange_weak(&g_video_geometry,&old,value|1u)) {}
    }
}
int tube_player_set_rotation_degrees(int degrees) {
    if(degrees<0 || degrees>270 || degrees%90)return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].rotation_degrees=degrees;video_geometry_publish();save();return 1;
}
int tube_player_set_mirror_enabled(int enabled) {
    if(enabled<0 || enabled>1)return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].mirror_enabled=enabled;video_geometry_publish();save();return 1;
}
int tube_player_set_aspect_mode(int mode) {
    if(mode<0 || mode>10)return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].aspect_mode=mode;video_geometry_publish();save();return 1;
}
int tube_player_set_pixel_mode(int mode) {
    if(mode<0 || mode>5)return 0;
    g_pixel_mode=mode;video_geometry_publish();
    /* Display correction applies even when a different profile owns the video. */
    unsigned old=atomic_load(&g_video_geometry);
    while((old&1) && !atomic_compare_exchange_weak(&g_video_geometry,&old,
          (old&~TUBE_PIXEL_MASK)|((unsigned)mode<<17))) {}
    save();return 1;
}
int tube_player_set_resize_mode(int mode) {
    if(mode<0 || mode>4)return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].resize_mode=mode;g_player_preferences[i].zoom_percent=0;
    video_geometry_publish();save();return 1;
}
int tube_player_set_zoom_percent(int percent) {
    if(!tube_zoom_valid(percent))return 0;
    int i=player_preference(settings_key(),1);if(i<0)return 0;
    g_player_preferences[i].resize_mode=0;g_player_preferences[i].zoom_percent=percent;
    video_geometry_publish();save();return 1;
}
static void player_preferences_load(const cJSON *root) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root,"player_preferences"), *row;
    if (!rows) {
        /* Existing PS5 profiles retain Cross pause and the original 3.5s hide. */
        player_preference("guest",1); player_preference(settings_key(),1);
        cJSON_ArrayForEach(row,cJSON_GetObjectItemCaseSensitive(root,"records"))
            player_preference(str(row,"profile"),1);
        for (int i = 0; i < g_player_preferences_count; ++i) {
            g_player_preferences[i].ok_mode = 1; g_player_preferences[i].hide_ms = 3500;
        }
        return;
    }
    if (!cJSON_IsArray(rows)) return;
    cJSON_ArrayForEach(row,rows) {
        const char *profile = str(row,"profile");
        const cJSON *ok = cJSON_GetObjectItemCaseSensitive(row,"ok_mode"), *hide = cJSON_GetObjectItemCaseSensitive(row,"hide_ms");
        double mode = num(row,"ok_mode"), ms = num(row,"hide_ms");
        if ((!valid_profile(profile) && strcmp(profile,SHARED_SETTINGS)) || !cJSON_IsNumber(ok) || !cJSON_IsNumber(hide) ||
            mode < 0 || mode > 3 || floor(mode) != mode || ms < 0 || ms > 15000 ||
            (ms != 3500 && fmod(ms,1000) != 0) || player_preference(profile,0) >= 0) continue;
        int i = player_preference(profile,1); if (i < 0) break;
        g_player_preferences[i].ok_mode = (int)mode; g_player_preferences[i].hide_ms = (int)ms;
        const cJSON *seek = cJSON_GetObjectItemCaseSensitive(row,"seek_mode");
        double seek_mode = num(row,"seek_mode");
        if (cJSON_IsNumber(seek) && seek_mode >= 0 && seek_mode <= 2 && floor(seek_mode) == seek_mode)
            g_player_preferences[i].seek_mode = (int)seek_mode;
        double degrees=num(row,"rotation_degrees"),mirror=num(row,"mirror_enabled");
        if(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(row,"rotation_degrees")) &&
           degrees>=0 && degrees<=270 && fmod(degrees,90)==0)
            g_player_preferences[i].rotation_degrees=(int)degrees;
        if(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(row,"mirror_enabled")) && (mirror==0 || mirror==1))
            g_player_preferences[i].mirror_enabled=(int)mirror;
        double aspect=num(row,"aspect_mode");
        if(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(row,"aspect_mode")) && aspect>=0 && aspect<=10 && floor(aspect)==aspect)
            g_player_preferences[i].aspect_mode=(int)aspect;
        const cJSON *resize=cJSON_GetObjectItemCaseSensitive(row,"resize_mode");
        double resize_mode=num(row,"resize_mode"),zoom=num(row,"zoom_percent");
        if(cJSON_IsNumber(resize) && resize_mode>=0 && resize_mode<=4 && floor(resize_mode)==resize_mode)
            g_player_preferences[i].resize_mode=(int)resize_mode;
        if(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(row,"zoom_percent")) &&
            zoom>=50 && zoom<=300 && floor(zoom)==zoom && tube_zoom_valid((int)zoom)) {
            g_player_preferences[i].resize_mode=0;g_player_preferences[i].zoom_percent=(int)zoom;
        }
        const cJSON *time=cJSON_GetObjectItemCaseSensitive(row,"time_mode");
        double time_mode=num(row,"time_mode");
        if(cJSON_IsNumber(time) && time_mode>=0 && time_mode<=2 && floor(time_mode)==time_mode)
            g_player_preferences[i].time_mode=(int)time_mode;
        const cJSON *interval=cJSON_GetObjectItemCaseSensitive(row,"seek_seconds");
        double seconds=num(row,"seek_seconds");
        if(cJSON_IsNumber(interval) && seconds>=1 && seconds<=60 && floor(seconds)==seconds && valid_seek_seconds((int)seconds))
            g_player_preferences[i].seek_seconds=(int)seconds;
    }
}

static const char *topic_section(const char *id) {
    static const char *const names[] = {"music", "gaming", "news", "sports", "live", "movies", "trending", "shorts", "my_videos", "kids"};
    if (!id || strncmp(id, "section:", 8)) return NULL;
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i) {
        size_t n = strlen(names[i]);
        if (!strncmp(id + 8, names[i], n) &&
            (!id[8 + n] || (id[8 + n] == ':' && valid_cursor(id + 9 + n)))) return names[i];
    }
    return NULL;
}

static int valid_catalog_id(const char *id) {
    if (id && !strncmp(id,"library:filter:",15) && strlen(id+15)==36 && valid_cursor(id+15))return 1;
    if (topic_section(id)) return 1;
    if (valid_id(id)) return 1;
    const char *value = NULL;
    if (id && !strncmp(id, "channel:UC", 10) && strlen(id) == 32) value = id + 8;
    if (id && !strncmp(id, "playlist:", 9) && strlen(id + 9) >= 10 && strlen(id + 9) <= 100) value = id + 9;
    if (id && !strncmp(id, "library:playlist:", 17) && strlen(id + 17) <= 100 &&
        (strlen(id + 17) >= 10 || !strcmp(id + 17, "WL") || !strcmp(id + 17, "LL"))) value = id + 17;
    return value && strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(value);
}

static int valid_playlist(const char *id) {
    char parent[128]; snprintf(parent, sizeof parent, "library:playlist:%s", id ? id : "");
    return valid_catalog_id(parent);
}

/* Catalog groups share the occurrence handoff ABI, but channel keys must never
 * be sent to YouTube as playlist IDs. */
static int valid_channel_group(const char *id) {
    return id && !strncmp(id,"channel:UC",10) && strlen(id)==32 && valid_catalog_id(id);
}
static int valid_retained_group(const char *id) {
    if(!id || strncmp(id,"group:",6) || strlen(id)!=42)return 0;
    for(int i=0;i<36;++i) {
        char c=id[i+6];
        if(i==8 || i==13 || i==18 || i==23) {if(c!='-')return 0;}
        else if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return 0;
    }
    return 1;
}
static int valid_playback_group(const char *id) {
    return valid_playlist(id) || valid_channel_group(id) || valid_retained_group(id);
}

int tube_select_playlist_context(const char *id, const char *playlist, int index, unsigned scope) {
    memset(&g_selected_playlist,0,sizeof g_selected_playlist);
    if (!valid_id(id)) return 0;
    if (!playlist || !*playlist) return 1;
    if (!scope || scope != g_playlist_scope || !valid_playback_group(playlist) || index < 0 || index > 100000) return 0;
    copy(g_selected_playlist.id,sizeof g_selected_playlist.id,id);
    copy(g_selected_playlist.value.playlist,sizeof g_selected_playlist.value.playlist,playlist);
    g_selected_playlist.value.index=index; g_selected_playlist.value.scope=scope;
    return 1;
}
int tube_playback_playlist_context(char *playlist, size_t capacity, int *index) {
    if (playlist && capacity) playlist[0]=0;
    if (index) *index=-1;
    if (!playlist || !index || !capacity || !recovery_scope() ||
        g_active_playlist.scope != g_playlist_scope || !*g_active_playlist.playlist ||
        strlen(g_active_playlist.playlist) >= capacity) return 0;
    copy(playlist,capacity,g_active_playlist.playlist); *index=g_active_playlist.index; return 1;
}
static void parse_playlist_context(evo_provider_item_t *item, const cJSON *root, const char *parent) {
    const cJSON *context=cJSON_GetObjectItemCaseSensitive(root,"playback_context");
    const char *playlist=str(context,"playlist");
    const cJSON *channel=cJSON_GetObjectItemCaseSensitive(context,"channel");
    const cJSON *retained=cJSON_GetObjectItemCaseSensitive(context,"group");
    char group[101];
    if(retained) {
        if(channel || cJSON_GetObjectItemCaseSensitive(context,"playlist") || !cJSON_IsString(retained))return;
        snprintf(group,sizeof group,"group:%s",retained->valuestring);
        if(!valid_retained_group(group))return;
        playlist=group;
    }
    if (!channel && !retained && !valid_playlist(playlist)) return;
    if (channel) {
        if (!cJSON_IsString(channel) || cJSON_GetObjectItemCaseSensitive(context,"playlist")) return;
        snprintf(group,sizeof group,"channel:%s",channel->valuestring);
        if (!valid_channel_group(group)) return;
        playlist=group;
    }
    const cJSON *index=cJSON_GetObjectItemCaseSensitive(context,"index");
    const char *expected=!strncmp(parent,"library:playlist:",17) ? parent+17 :
        !strncmp(parent,"playlist:",9) ? parent+9 : !strcmp(parent,"library:watch_later") ? "WL" :
        !strncmp(parent,"channel:",8) ? parent : NULL;
    if (!valid_id(item->id) || item->is_folder || !valid_playback_group(playlist) ||
        (expected && strcmp(expected,playlist)) || !cJSON_IsNumber(index) ||
        !isfinite(index->valuedouble) || index->valuedouble < 0 || index->valuedouble > 100000 ||
        floor(index->valuedouble) != index->valuedouble) return;
    copy(item->playback_playlist,sizeof item->playback_playlist,playlist);
    item->playback_index=(int)index->valuedouble; item->playback_scope=g_playlist_scope;
}

static unsigned account_intent(const char *action, const char *identity, const char *playlist, const char *title) {
    memset(&g_account_intent, 0, sizeof g_account_intent);
    if (!++g_account_serial) ++g_account_serial;
    g_account_intent.serial = g_account_serial;
    copy(g_account_intent.action, sizeof g_account_intent.action, action);
    copy(g_account_intent.identity, sizeof g_account_intent.identity, identity);
    copy(g_account_intent.playlist, sizeof g_account_intent.playlist, playlist);
    copy(g_account_intent.title, sizeof g_account_intent.title, title);
    copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
    return g_account_intent.serial;
}

/* Optional password a profile must give before it is selected: salted hashes. */
#define PROFILE_LOCKS 64
static struct { char profile[33], record[TUBE_PASSWORD_RECORD + 1]; } g_locks[PROFILE_LOCKS];
static int g_lock_count;
static struct { char profile[33]; unsigned ticket; } g_unlock;
static char g_prompt_error[96];
static int lock_find(const char *profile) {
    for (int i = 0; i < g_lock_count; ++i) if (!strcmp(g_locks[i].profile, profile)) return i;
    return -1;
}
static void lock_remove(const char *profile) {
    int i = lock_find(profile); if (i < 0) return;
    memmove(&g_locks[i], &g_locks[i + 1], (size_t)(g_lock_count - i - 1) * sizeof g_locks[0]); --g_lock_count;
}
/* 1 = stored; 0 = the list is full, the random source failed or the text is unusable. */
static int lock_put(const char *profile, const char *text) {
    unsigned char salt[8]; int i = lock_find(profile);
    if (!valid_profile(profile) || (i < 0 && g_lock_count == PROFILE_LOCKS) || tube_random_bytes(salt, sizeof salt)) return 0;
    char record[TUBE_PASSWORD_RECORD + 1];
    if (!tube_password_make(salt, text, record)) return 0;
    if (i < 0) i = g_lock_count++;
    copy(g_locks[i].profile, sizeof g_locks[i].profile, profile); memcpy(g_locks[i].record, record, sizeof record);
    return 1;
}
const char *tube_prompt_error(void) { return g_prompt_error; }
/* The alternative SponsorBlock server of the built-in service, "" or "https://host": there is no
 * host to start that service with one, so the address is typed here and goes out with each
 * segments request. Device-wide; the switch that uses it stays the profile's. */
static char g_sb_server[264];
/* The service's rule (standalone/js/extras.js, sponsorOrigin): https, a plain host name, no port,
 * path, query or user. What was typed may leave the scheme out, end in a slash or use capitals. */
static int sponsor_origin(const char *text, char *out, size_t capacity) {
    char host[254]; size_t n = 0;
    if (!text) return 0;
    if (!strncasecmp(text, "https://", 8)) text += 8;
    for (; *text && strcmp(text, "/"); ++text) {
        const char c = *text >= 'A' && *text <= 'Z' ? (char)(*text + 32) : *text;
        if (n + 1 >= sizeof host || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) return 0;
        host[n++] = c;
    }
    host[n] = 0;
    if (!n || host[0] == '.' || host[0] == '-' || host[n - 1] == '.' || host[n - 1] == '-' || strstr(host, "..")) return 0;
    return snprintf(out, capacity, "https://%s", host) < (int)capacity;
}
/* The last search typed inside a channel. */
static struct { char channel[25], query[801], profile[33]; unsigned scope, serial; } g_channel_query;

int tube_take_playlist_prompt(tube_playlist_prompt_t *prompt) {
    if (!prompt || !g_playlist_prompt.pending) return 0;
    *prompt = g_playlist_prompt.prompt; g_playlist_prompt.pending = 0; return 1;
}

static int prompt_submit(unsigned ticket, const char *text, char *parent, size_t capacity) {
    if (!g_playlist_prompt.active || ticket != g_playlist_prompt.prompt.ticket || !text ||
        !parent || capacity < 48 || strcmp(g_playlist_prompt.profile, g_library_profile)) return -1;
    while (*text && isspace((unsigned char)*text)) ++text;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1])) --length;
    if (!length || length > 600) return -1;
    size_t points = 0;
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];
        size_t n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        if (!n || c < 0x20 || c == 0x7f || i + n > length || ++points > 150) return -1;
        for (size_t k = 1; k < n; ++k) if (((unsigned char)text[i+k] & 0xc0) != 0x80) return -1;
        unsigned char next = n > 1 ? (unsigned char)text[i+1] : 0;
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
            (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)) return -1;
        i += n;
    }
    char title[601]; memcpy(title, text, length); title[length] = 0;
    if (g_playlist_prompt.server) { /* a local setting: kept only as a plain https origin, nothing to confirm */
        char origin[sizeof g_sb_server];
        if (!sponsor_origin(title, origin, sizeof origin)) return -1;
        copy(g_sb_server, sizeof g_sb_server, origin); g_playlist_prompt.active = 0; save();
        snprintf(parent, capacity, "settings:sb"); return 0;
    }
    if (g_playlist_prompt.search) { /* a search inside a channel: listed at once, nothing to confirm */
        static unsigned serial;
        if (!tube_search_query_valid(title) || !valid_channel_group(g_playlist_prompt.identity)) return -1;
        memset(&g_channel_query, 0, sizeof g_channel_query);
        if (!++serial) ++serial;
        g_channel_query.serial = serial; g_channel_query.scope = g_playlist_scope;
        copy(g_channel_query.channel, sizeof g_channel_query.channel, g_playlist_prompt.identity + 8);
        copy(g_channel_query.query, sizeof g_channel_query.query, title);
        copy(g_channel_query.profile, sizeof g_channel_query.profile, g_library_profile);
        g_playlist_prompt.active = 0;
        snprintf(parent, capacity, "channel-search:run:%u", serial); return 0;
    }
    if (g_playlist_prompt.lock) { /* the text is compared or hashed here and never kept */
        const int mode = g_playlist_prompt.lock;
        const char *owner = mode == 1 ? g_playlist_prompt.target : g_library_profile;
        const int index = lock_find(owner);
        int ok = mode == 2 ? lock_put(owner, title) : index >= 0 && tube_password_check(g_locks[index].record, title);
        memset(title, 0, sizeof title);
        if (!ok) {
            copy(g_prompt_error, sizeof g_prompt_error, mode == 2 ? "The password could not be saved." : "Wrong password.");
            return -1;
        }
        g_playlist_prompt.active = 0; memset(&g_unlock, 0, sizeof g_unlock);
        if (mode == 1 || mode == 3) { copy(g_unlock.profile, sizeof g_unlock.profile, owner); g_unlock.ticket = ticket; }
        if (mode == 1) snprintf(parent, capacity, "library:profile:%s", owner);
        else if (mode == 3) snprintf(parent, capacity, "settings:lock:new:%u", ticket);
        else {
            if (mode == 4) lock_remove(owner);
            save(); snprintf(parent, capacity, "settings:accounts");
        }
        return 0;
    }
    if (g_playlist_prompt.comment) { /* reviewed on its own screen before the one submission */
        unsigned serial = account_intent(g_playlist_prompt.comment == 2 ? "comment_reply" : "comment_post", g_playlist_prompt.identity, "", title);
        copy(g_account_intent.argument, sizeof g_account_intent.argument, g_playlist_prompt.cursor);
        g_playlist_prompt.active = 0;
        snprintf(parent, capacity, "comment:review:%u", serial); return 0;
    }
    if (*g_playlist_prompt.pin) { /* a local name: applied at once, nothing to confirm */
        int index = pin_find(g_library_profile, g_playlist_prompt.pin);
        if (index < 0) return -1;
        copy(g_pins[index].title, sizeof g_pins[index].title, title); g_playlist_prompt.active = 0; save();
        snprintf(parent, capacity, "settings:pin:renamed"); return 0;
    }
    unsigned serial = account_intent(g_playlist_prompt.create ? "playlist_create" : "playlist_rename",
        g_playlist_prompt.create ? g_playlist_prompt.identity : "",
        g_playlist_prompt.create ? "" : g_playlist_prompt.identity, title);
    g_playlist_prompt.active = 0;
    snprintf(parent, capacity, "playlist:review:%u", serial); return 0;
}
/* A refused text leaves the reason for the host's notice ("" = the playlist name rule). */
int tube_submit_playlist_name(unsigned ticket, const char *text, char *parent, size_t capacity) {
    const int current = g_playlist_prompt.active && ticket == g_playlist_prompt.prompt.ticket;
    copy(g_prompt_error, sizeof g_prompt_error, !current ? "" : g_playlist_prompt.lock ? "Enter a password of 1 to 150 characters." :
        g_playlist_prompt.server ? "Enter an https address without a port or path, such as https://sponsor.example.org." :
        g_playlist_prompt.search ? "Enter 1 to 150 characters to search this channel." :
        g_playlist_prompt.comment ? "Enter a comment of 1 to 150 characters." : "");
    const int result = prompt_submit(ticket, text, parent, capacity);
    if (!result) g_prompt_error[0] = 0;
    return result;
}
/* Title of the folder the host opens after an accepted text. */
const char *tube_prompt_title(const char *parent) {
    return !parent ? "" : !strncmp(parent, "settings:pin:", 13) ? "Pinned section" :
        !strncmp(parent, "channel-search:", 15) ? "Search in channel" :
        !strncmp(parent, "comment:", 8) ? "Confirm comment" :
        !strncmp(parent, "library:profile:", 16) ? "Accounts & library" :
        !strcmp(parent, "settings:sb") ? "SponsorBlock actions" :
        !strncmp(parent, "settings:", 9) ? "Accounts" : "Confirm playlist change";
}

int tube_take_chat_prompt(tube_chat_prompt_t *prompt) {
    if (!prompt || !g_message.pending || g_message.phase != 1 || !message_scope()) return 0;
    memset(prompt, 0, sizeof *prompt); prompt->ticket = g_message.ticket;
    copy(prompt->label, sizeof prompt->label, "YouTube live-chat message");
    g_message.pending = 0; return 1;
}

void tube_cancel_chat_prompt(unsigned ticket) {
    if (ticket == g_message.ticket && g_message.phase == 1) memset(&g_message, 0, sizeof g_message);
}

int tube_submit_chat_message(unsigned ticket, const char *text, char *parent, size_t capacity) {
    if (ticket != g_message.ticket || g_message.phase != 1 || !message_scope() || !text || !parent || capacity < 48) return -1;
    size_t length = strnlen(text, 801), points = 0; int visible = 0;
    if (!length || length > 800) return -1;
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];
        size_t n = c < 0x80 ? 1 : c >= 0xc2 && c <= 0xdf ? 2 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xf0 && c <= 0xf4 ? 4 : 0;
        if (!n || c < 0x20 || c == 0x7f || i + n > length || ++points > 200) return -1;
        for (size_t k = 1; k < n; ++k) if (((unsigned char)text[i+k] & 0xc0) != 0x80) return -1;
        unsigned char next = n > 1 ? (unsigned char)text[i+1] : 0;
        if ((c == 0xe0 && next < 0xa0) || (c == 0xed && next >= 0xa0) ||
            (c == 0xf0 && next < 0x90) || (c == 0xf4 && next >= 0x90)) return -1;
        unsigned cp = c & (n == 1 ? 0x7f : n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07);
        for (size_t k = 1; k < n; ++k) cp = (cp << 6) | ((unsigned char)text[i+k] & 0x3f);
        if (cp != 0x20 && cp != 0x85 && cp != 0xa0 && cp != 0x1680 &&
            !(cp >= 0x2000 && cp <= 0x200a) && cp != 0x2028 && cp != 0x2029 &&
            cp != 0x202f && cp != 0x205f && cp != 0x3000) visible = 1;
        i += n;
    }
    if (!visible) return -1;
    copy(g_message.text, sizeof g_message.text, text); g_message.phase = 2;
    snprintf(parent, capacity, "message:review:%u", ticket); return 0;
}

static Record *record_for(const char *id, const char *profile, int create) {
    if (!valid_id(id) || !valid_profile(profile)) return NULL;
    for (int i = 0; i < g_count; ++i)
        if (!strcmp(g_records[i].item.id, id) && !strcmp(g_records[i].profile, profile)) return &g_records[i];
    if (!create) return NULL;
    evo_provider_item_t metadata = {0};
    for (int i = 0; i < g_count; ++i) if (!strcmp(g_records[i].item.id, id)) {
        metadata = g_records[i].item; metadata.resume_pos_sec = 0; metadata.played = 0; break;
    }
    int slot = g_count;
    if (slot >= RECORDS) {
        slot = -1;
        for (int i = 0; i < g_count; ++i)
            if (!g_records[i].favorite && !g_records[i].queued && !g_records[i].subscribed &&
                (slot < 0 || g_records[i].updated < g_records[slot].updated)) slot = i;
        if (slot < 0) return NULL;
    } else ++g_count;
    memset(&g_records[slot], 0, sizeof(Record));
    g_records[slot].item = metadata;
    copy(g_records[slot].profile, sizeof g_records[slot].profile, profile);
    copy(g_records[slot].item.id, sizeof g_records[slot].item.id, id);
    g_records[slot].item.kind = EVO_MEDIA_VIDEO;
    return &g_records[slot];
}

static Record *record(const char *id, int create) { return record_for(id, g_library_profile, create); }
static int current_record(const Record *r) { return !strcmp(r->profile, g_library_profile); }
static int blocked_channel(const char *channel) {
    if(!channel || !*channel)return -1;
    for(int i=0;i<g_blocked_count;++i)
        if(!strcmp(g_blocked[i].profile,g_library_profile) && !strcmp(g_blocked[i].channel,channel))return i;
    return -1;
}
static int channel_blocks_active(void) {
    for(int i=0;i<g_blocked_count;++i)if(!strcmp(g_blocked[i].profile,g_library_profile))return 1;
    return 0;
}
static int channel_name_blocks_active(void) {
    for(int i=0;i<g_blocked_count;++i)if(!strcmp(g_blocked[i].profile,g_library_profile) &&
        strcmp(g_blocked[i].name,g_blocked[i].channel) && tube_channel_name_equal(g_blocked[i].name,g_blocked[i].name))return 1;
    return 0;
}
static int blocked_identity(const char *channel,const char *name) {
    int index=blocked_channel(channel);if(index>=0)return index;
    for(int i=0;i<g_blocked_count;++i)if(!strcmp(g_blocked[i].profile,g_library_profile) &&
        tube_channel_name_equal(g_blocked[i].name,name))return i;
    return -1;
}
static int change_channel_block(const char *channel,const char *name,int block) {
    char catalog[40];snprintf(catalog,sizeof catalog,"channel:%s",channel);
    if(*channel && (strlen(channel)!=24 || !valid_catalog_id(catalog)))return 0;
    if(!*channel && !tube_channel_name_equal(name,name))return 0;
    int index=blocked_identity(channel,name);
    if(!block) {
        if(index>=0) {memmove(&g_blocked[index],&g_blocked[index+1],(size_t)(g_blocked_count-index-1)*sizeof g_blocked[0]);--g_blocked_count;++g_blocked_revision;}
    } else if(index<0) {
        if(g_blocked_count>=128){tube_set_error("The channel block list is full. Unblock a channel first.");return 0;}
        index=g_blocked_count++;
        copy(g_blocked[index].profile,sizeof g_blocked[index].profile,g_library_profile);
        copy(g_blocked[index].channel,sizeof g_blocked[index].channel,channel);
        copy(g_blocked[index].name,sizeof g_blocked[index].name,*name?name:channel);
        ++g_blocked_revision;
    }
    save();return 1;
}

/* Recent searches are profile-owned, most recent first. The shared bound drops
 * the oldest entry overall; serials name rows without exposing long queries. */
#define SEARCH_HISTORY 50
#define SEARCHES 256
static struct { char profile[33], query[801]; unsigned serial; } g_searches[SEARCHES];
static int g_search_count;
static unsigned g_search_serial;
static int search_same(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}
static void search_history_drop(int index) {
    memmove(&g_searches[index],&g_searches[index+1],(size_t)(g_search_count-index-1)*sizeof g_searches[0]);--g_search_count;
}
/* front=0 loads saved order (first occurrence wins); front=1 records a new search. */
static int search_history_put(const char *profile, const char *query, int front) {
    if (!valid_profile(profile) || !tube_search_query_valid(query) || strlen(query) >= sizeof g_searches[0].query) return 0;
    char text[sizeof g_searches[0].query]; copy(text,sizeof text,query); /* query may alias a moved row */
    int owned = 0, oldest = -1, index = 0;
    unsigned serial = 0; /* a repeated query keeps its row name, so its result pages stay reachable */
    for (int i = 0; i < g_search_count; ++i) if (!strcmp(g_searches[i].profile,profile)) {
        if (search_same(g_searches[i].query,text)) { if (!front) return 0; serial = g_searches[i].serial; search_history_drop(i--); continue; }
        ++owned; oldest = i;
    }
    if (!front) {
        if (owned >= SEARCH_HISTORY || g_search_count == SEARCHES) return 0;
        index = g_search_count++;
    } else {
        if (owned >= SEARCH_HISTORY) search_history_drop(oldest);
        if (g_search_count == SEARCHES) --g_search_count;
        memmove(&g_searches[1],&g_searches[0],(size_t)g_search_count*sizeof g_searches[0]); ++g_search_count;
    }
    copy(g_searches[index].profile,sizeof g_searches[index].profile,profile);
    copy(g_searches[index].query,sizeof g_searches[index].query,text);
    if (!serial) { if (!++g_search_serial) ++g_search_serial; serial = g_search_serial; }
    g_searches[index].serial = serial; return 1;
}
static void search_history_add(const char *query) {
    if (g_search_history && search_history_put(g_library_profile,query,1)) save();
}
static int search_history_count(void) {
    int count = 0;
    for (int i = 0; i < g_search_count; ++i) if (!strcmp(g_searches[i].profile,g_library_profile)) ++count;
    return count;
}
static int search_history_find(const char *serial) {
    char *end = NULL; unsigned long value = isdigit((unsigned char)*serial) ? strtoul(serial,&end,10) : 0;
    if (!value || *end) return -1;
    for (int i = 0; i < g_search_count; ++i)
        if (g_searches[i].serial == value && !strcmp(g_searches[i].profile,g_library_profile)) return i;
    return -1;
}
/* Last playlist occurrence seen for a video in browser pages; never persisted
 * and retired with the browsing scope. Feeds Open playlist and Share link only. */
static struct { char id[12], playlist[101]; unsigned scope; int account; } g_occurrences[64];
static unsigned g_occurrence_cursor;
static int video_occurrence(const char *id) {
    for (int i = 0; i < 64; ++i)
        if (g_occurrences[i].scope && g_occurrences[i].scope == g_playlist_scope && !strcmp(g_occurrences[i].id,id)) return i;
    return -1;
}
static void video_occurrence_note(const char *id, const char *playlist, int account) {
    int i = video_occurrence(id);
    if (i < 0) i = (int)(g_occurrence_cursor++ % 64);
    copy(g_occurrences[i].id,sizeof g_occurrences[i].id,id);
    copy(g_occurrences[i].playlist,sizeof g_occurrences[i].playlist,playlist);
    g_occurrences[i].scope = g_playlist_scope; g_occurrences[i].account = account;
}
/* 1=stored as requested, 0=the exclusion list is full. */
static int sponsor_exclude(const char *channel, int exclude) {
    int found = -1;
    for (int i = 0; i < g_sb.excluded_count; ++i) if (!strcmp(channel, g_sb.excluded[i])) found = i;
    if ((found >= 0) == !!exclude) return 1;
    if (found >= 0) {
        if (found + 1 < g_sb.excluded_count)
            memmove(g_sb.excluded[found], g_sb.excluded[found + 1],
                (size_t)(g_sb.excluded_count - found - 1) * sizeof g_sb.excluded[0]);
        --g_sb.excluded_count;
    } else if (g_sb.excluded_count < TUBE_SB_EXCLUSIONS) {
        memcpy(g_sb.excluded[g_sb.excluded_count++], channel, 25);
    } else { tube_set_error("The excluded channel list is full."); return 0; }
    g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save(); return 1;
}
static void sections_default(unsigned char *order) {
    for (int i = 0; i < SECTION_SLOTS; ++i) order[i] = (unsigned char)i;
}
/* Rows that exist only with content (recent searches, blocked channels) and
 * hidden rows are not on screen. */
static int section_listed(int section) {
    if (g_section_hidden >> section & 1) return 0;
    if (!strcmp(g_sections[section].id, "search-history")) return g_search_history && search_history_count();
    if (!strcmp(g_sections[section].id, "blocked-channels")) return channel_blocks_active();
    return 1;
}
/* Move within the row's own menu, past rows that are not on screen, so that
 * one press changes what the profile sees. */
static int section_move(int section, int direction) {
    int at = -1, moved = 0;
    for (int i = 0; i < SECTIONS; ++i) if (g_section_order[i] == section) at = i;
    for (int i = at + direction; at >= 0 && i >= 0 && i < SECTIONS; i += direction) {
        const int other = g_section_order[i];
        if (g_sections[other].group != g_sections[section].group) continue;
        g_section_order[at] = (unsigned char)other; g_section_order[i] = (unsigned char)section; at = i; moved = 1;
        if (section_listed(other) || !section_listed(section)) break;
    }
    return moved;
}
static int pin_valid(const char *id) {
    return id && strlen(id) < sizeof g_pins[0].id && valid_catalog_id(id) &&
        (!strncmp(id, "channel:", 8) || !strncmp(id, "playlist:", 9) || !strncmp(id, "library:playlist:", 17));
}
static int pin_find(const char *profile, const char *id) {
    for (int i = 0; i < g_pin_count; ++i)
        if (!strcmp(g_pins[i].profile, profile) && !strcmp(g_pins[i].id, id)) return i;
    return -1;
}
/* 1=stored, 0=already pinned, invalid or full (full reports through the error text). */
static int pin_put(const char *profile, const char *id, const char *title) {
    int owned = 0;
    if (!valid_profile(profile) || !pin_valid(id) || pin_find(profile, id) >= 0) return 0;
    for (int i = 0; i < g_pin_count; ++i) if (!strcmp(g_pins[i].profile, profile)) ++owned;
    if (owned >= PROFILE_PINS || g_pin_count == PINS) return 0;
    copy(g_pins[g_pin_count].profile, sizeof g_pins[0].profile, profile);
    copy(g_pins[g_pin_count].id, sizeof g_pins[0].id, id);
    copy(g_pins[g_pin_count].title, sizeof g_pins[0].title, title && *title ? title : strrchr(id, ':') + 1);
    ++g_pin_count; return 1;
}
static int pin_move(int index, int direction) {
    for (int i = index + direction; i >= 0 && i < g_pin_count; i += direction)
        if (!strcmp(g_pins[i].profile, g_pins[index].profile)) {
            char id[sizeof g_pins[0].id], title[sizeof g_pins[0].title];
            memcpy(id, g_pins[i].id, sizeof id); memcpy(title, g_pins[i].title, sizeof title);
            memcpy(g_pins[i].id, g_pins[index].id, sizeof id); memcpy(g_pins[i].title, g_pins[index].title, sizeof title);
            memcpy(g_pins[index].id, id, sizeof id); memcpy(g_pins[index].title, title, sizeof title); return 1;
        }
    return 0;
}
/* Menu a folder row opens with the Touchpad ("" = none). Playlists of the
 * account keep their own menu, which offers the pin through the noted folder;
 * the account's YouTube history opens its pause, resume and clear controls. */
const char *tube_folder_menu(const char *id, const char *title) {
    static char menu[160];
    menu[0] = 0; memset(&g_menu_target, 0, sizeof g_menu_target);
    if (section_index(id) >= 0) snprintf(menu, sizeof menu, "settings:section:%d", section_index(id));
    else if (id && !strcmp(id, "library:history")) snprintf(menu, sizeof menu, "account:history"); /* pause, resume, clear */
    else if (id && !strcmp(id, "library:channels")) snprintf(menu, sizeof menu, "channels:menu"); /* order, mark all as watched */
    else if (pin_valid(id)) {
        copy(g_menu_target.id, sizeof g_menu_target.id, id); copy(g_menu_target.title, sizeof g_menu_target.title, title);
        copy(g_menu_target.profile, sizeof g_menu_target.profile, g_library_profile); g_menu_target.scope = g_playlist_scope;
        if (strncmp(id, "library:playlist:", 17)) snprintf(menu, sizeof menu, "settings:pin:%s", id);
    }
    return menu;
}
/* "verb:id", a bare id (the pin's menu) or "renamed" (the pin just renamed). */
static const char *pin_target(const char *value, int *verb) {
    static const char *const verbs[] = {"add:", "remove:", "up:", "down:", "rename:", "unfollow:"};
    *verb = -1;
    if (!strcmp(value, "renamed"))
        return *g_playlist_prompt.pin && !strcmp(g_playlist_prompt.profile, g_library_profile) ? g_playlist_prompt.pin : NULL;
    for (int i = 0; i < 6; ++i) if (!strncmp(value, verbs[i], strlen(verbs[i]))) { *verb = i; value += strlen(verbs[i]); break; }
    return pin_valid(value) ? value : NULL;
}
/* Repeating an action is harmless: a pinned folder is not pinned twice. */
static int pin_action(const char *value) {
    int verb; const char *id = pin_target(value, &verb);
    if (!id) { tube_set_error("Select a channel or playlist to pin."); return 0; }
    const int index = pin_find(g_library_profile, id);
    if (verb == 5) { /* a channel's own menu also ends its local follow */
        if (!strncmp(id, "channel:", 8) && followed(id + 8)) { unfollow(id + 8); save(); }
    } else if (verb == 0 && index < 0) {
        const int known = !strcmp(g_menu_target.id, id) && g_menu_target.scope == g_playlist_scope &&
            !strcmp(g_menu_target.profile, g_library_profile);
        if (!pin_put(g_library_profile, id, known ? g_menu_target.title : "")) {
            tube_set_error("The pinned section list is full. Unpin a section first."); return 0;
        }
        save();
    } else if (verb == 1 && index >= 0) {
        memmove(&g_pins[index], &g_pins[index + 1], (size_t)(g_pin_count - index - 1) * sizeof g_pins[0]); --g_pin_count; save();
    } else if ((verb == 2 || verb == 3) && index >= 0) {
        if (pin_move(index, verb == 2 ? -1 : 1)) save();
    } else if (verb == 4) {
        if (index < 0) { tube_set_error("Pin this section before renaming it."); return 0; }
        char pin[sizeof g_playlist_prompt.pin]; copy(pin, sizeof pin, id);
        memset(&g_account_intent, 0, sizeof g_account_intent); memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
        if (!++g_account_serial) ++g_account_serial;
        g_playlist_prompt.prompt.ticket = g_account_serial;
        copy(g_playlist_prompt.prompt.label, sizeof g_playlist_prompt.prompt.label, "Pinned section name");
        copy(g_playlist_prompt.prompt.initial, sizeof g_playlist_prompt.prompt.initial, g_pins[index].title);
        copy(g_playlist_prompt.pin, sizeof g_playlist_prompt.pin, pin);
        copy(g_playlist_prompt.profile, sizeof g_playlist_prompt.profile, g_library_profile);
        g_playlist_prompt.pending = g_playlist_prompt.active = 1;
    }
    return 1;
}
static void search_options_reset(void) {
    memset(&g_search_options, 0, sizeof g_search_options);
    copy(g_search_options.profile, sizeof g_search_options.profile, g_library_profile);
}
/* Filters chosen under another profile never apply to this one. */
static int search_filters(void) {
    if (strcmp(g_search_options.profile, g_library_profile)) search_options_reset();
    return g_search_options.date || g_search_options.duration || g_search_options.type ||
        g_search_options.order || g_search_options.features;
}
static void search_summary(char *out, size_t capacity) {
    char text[256] = ""; size_t used = 0;
    const char *parts[4 + SEARCH_FEATURES]; int count = 0;
    search_filters();
    if (g_search_options.date) parts[count++] = g_search_date_labels[g_search_options.date];
    if (g_search_options.duration) parts[count++] = g_search_duration_labels[g_search_options.duration];
    if (g_search_options.type) parts[count++] = g_search_type_labels[g_search_options.type];
    for (int i = 0; i < SEARCH_FEATURES; ++i) if (g_search_options.features >> i & 1) parts[count++] = g_search_feature_labels[i];
    if (g_search_options.order) parts[count++] = g_search_order_labels[g_search_options.order];
    for (int i = 0; i < count && used < sizeof text; ++i)
        used += (size_t)snprintf(text + used, sizeof text - used, "%s%s", i ? ", " : "", parts[i]);
    if (g_search_feed.unavailable == g_playlist_scope && (count || g_search_cards))
        copy(out, capacity, "Options ignored: the companion library worker is off");
    else if (count) copy(out, capacity, text);
    else copy(out, capacity, g_search_cards ? "No filters; channels and playlists included" : "No filters; videos only");
}
static int last_search_current(void) {
    return *g_last_search.query && g_last_search.scope == g_playlist_scope && !strcmp(g_last_search.profile, g_library_profile);
}
/* The last typed search as a folder the host can stack above ("" = none). */
const char *tube_search_folder(void) {
    static char folder[32];
    folder[0] = 0;
    if (last_search_current()) snprintf(folder, sizeof folder, "search-again:%u", g_playlist_scope);
    return folder;
}
static int dismissed(const char *id, const char *channel, int hidden_only) {
    for (int i = 0; i < 64; ++i) if (g_dismissed[i].scope == g_playlist_scope && !strcmp(g_dismissed[i].profile, g_library_profile) &&
        (g_dismissed[i].hidden || !hidden_only) &&
        (!strcmp(g_dismissed[i].id, id) || (*g_dismissed[i].channel && !strcmp(g_dismissed[i].channel, channel)))) return 1;
    return 0;
}
static int home_parent(const char *parent) {
    return !strcmp(parent, "library:home") || !strncmp(parent, "library:filter:", 15);
}
/* Account watch-history controls: 1 = pause, 2 = resume, 3 = clear. */
static int history_control(const char *action) {
    return !strcmp(action, "history_pause") ? 1 : !strcmp(action, "history_resume") ? 2 : !strcmp(action, "history_clear") ? 3 : 0;
}
static unsigned feedback_option(const char *action) {
    return !strcmp(action, "not_interested") ? 1u : !strcmp(action, "dont_recommend_channel") ? 2u : !strcmp(action, "hide") ? 4u : 0u;
}
/* Lists whose cards carry a feedback reference: Home and the account's Subscriptions. */
static int feedback_parent(const char *parent) {
    return home_parent(parent) || !strcmp(parent, "library:subscriptions");
}
static int feedback_current(const Record *r, unsigned option) {
    return r && r->feedback_scope == g_playlist_scope && r->feedback_deadline > now_ms() &&
        valid_cursor(r->feedback_cursor) && (r->feedback_options & option);
}
static int valid_channel(const char *id) {
    char group[40]; snprintf(group, sizeof group, "channel:%.30s", id ? id : "");
    return valid_channel_group(group);
}
/* "1234567" -> "1,234,567 views"; 0 when the text is not a plain count. */
static int count_label(char *out, size_t capacity, const char *digits, const char *unit) {
    size_t length = strlen(digits), used = 0;
    if (!length || length > 20 || strspn(digits, "0123456789") != length || capacity < 64) return 0;
    for (size_t i = 0; i < length; ++i) {
        if (i && !((length - i) % 3)) out[used++] = ',';
        out[used++] = digits[i];
    }
    snprintf(out + used, capacity - used, " %s%s", unit, strcmp(digits, "1") ? "s" : ""); return 1;
}
static int channel_mark_find(const char *channel) {
    for (int i = 0; i < g_channel_mark_count; ++i)
        if (!strcmp(g_channel_marks[i].profile, g_library_profile) && !strcmp(g_channel_marks[i].channel, channel)) return i;
    return -1;
}
static void channel_mark_drop(int index) {
    memmove(&g_channel_marks[index], &g_channel_marks[index + 1], (size_t)(g_channel_mark_count - index - 1) * sizeof g_channel_marks[0]);
    --g_channel_mark_count; g_channel_marks_dirty = 1;
}
static void channel_mark_put(const char *channel, const char *video) {
    if (!valid_channel(channel) || !strcmp(g_library_profile, "guest")) return;
    int i = channel_mark_find(channel);
    if (i < 0) {
        if (g_channel_mark_count == CHANNEL_MARKS) {
            int oldest = 0;
            for (int k = 1; k < g_channel_mark_count; ++k) if (g_channel_marks[k].seen < g_channel_marks[oldest].seen) oldest = k;
            channel_mark_drop(oldest);
        }
        i = g_channel_mark_count++; memset(&g_channel_marks[i], 0, sizeof g_channel_marks[i]);
        copy(g_channel_marks[i].profile, sizeof g_channel_marks[i].profile, g_library_profile);
        copy(g_channel_marks[i].channel, sizeof g_channel_marks[i].channel, channel); g_channel_marks_dirty = 1;
    }
    if (valid_id(video) && strcmp(g_channel_marks[i].video, video)) {
        copy(g_channel_marks[i].video, sizeof g_channel_marks[i].video, video); g_channel_marks_dirty = 1;
    }
    g_channel_marks[i].seen = (long long)time(NULL);
}
/* A listed channel as the selected profile sees it: 1 = new content. YouTube's
 * cleared mark also retires what the profile remembered. */
static int channel_new(const cJSON *row, const char *channel) {
    const cJSON *mark = cJSON_GetObjectItemCaseSensitive(row, "has_new");
    const int seen = channel_mark_find(channel);
    if (!cJSON_IsBool(mark)) return 0;
    if (cJSON_IsFalse(mark)) { if (seen >= 0) channel_mark_drop(seen); return 0; }
    if (seen < 0 && g_channel_new.count < CHANNEL_ROWS) copy(g_channel_new.channel[g_channel_new.count++], 25, channel);
    return seen < 0;
}
static void channel_new_reset(void) {
    memset(&g_channel_new, 0, sizeof g_channel_new); g_channel_new.scope = g_playlist_scope;
    copy(g_channel_new.profile, sizeof g_channel_new.profile, g_library_profile);
}
static int follow_find(const char *profile, const char *channel) {
    for (int i = 0; i < g_follow_count; ++i)
        if (!strcmp(g_follows[i].profile, profile) && !strcmp(g_follows[i].channel, channel)) return i;
    return -1;
}
/* 1 = the selected profile follows the channel, through a video or an import. */
static int followed(const char *channel) {
    if (!channel || !*channel) return 0;
    if (follow_find(g_library_profile, channel) >= 0) return 1;
    for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && g_records[i].subscribed && !strcmp(g_records[i].item.parent_id, channel)) return 1;
    return 0;
}
/* 1 = stored; 0 = invalid, already listed or the list is full. */
static int follow_put(const char *profile, const char *channel, const char *title) {
    if (!valid_profile(profile) || !valid_channel(channel) || follow_find(profile, channel) >= 0 || g_follow_count == FOLLOWS) return 0;
    copy(g_follows[g_follow_count].profile, sizeof g_follows[0].profile, profile);
    copy(g_follows[g_follow_count].channel, sizeof g_follows[0].channel, channel);
    copy(g_follows[g_follow_count].title, sizeof g_follows[0].title, title && *title ? title : channel);
    ++g_follow_count; return 1;
}
static void unfollow(const char *channel) {
    const int i = follow_find(g_library_profile, channel);
    if (i >= 0) { memmove(&g_follows[i], &g_follows[i + 1], (size_t)(g_follow_count - i - 1) * sizeof g_follows[0]); --g_follow_count; }
    for (int k = 0; k < g_count; ++k)
        if (current_record(&g_records[k]) && !strcmp(g_records[k].item.parent_id, channel)) g_records[k].subscribed = 0;
}
static int recent_playlist(void) {
    for (int i = 0; i < g_recent_playlist_count; ++i) if (!strcmp(g_recent_playlists[i].profile, g_library_profile)) return i;
    return -1;
}
/* The playlist of a confirmed add or remove becomes the profile's shortcut. */
static void recent_playlist_note(const char *playlist) {
    char title[EVO_PROVIDER_MAX_TITLE] = "";
    int i = recent_playlist();
    if (!valid_playlist(playlist) || !strcmp(g_library_profile, "guest")) return;
    if (i >= 0 && !strcmp(g_recent_playlists[i].playlist, playlist)) copy(title, sizeof title, g_recent_playlists[i].title);
    for (int k = 0; k < 48; ++k) if (!strcmp(g_picker_rows[k].playlist, playlist)) copy(title, sizeof title, g_picker_rows[k].title);
    if (i < 0) {
        if (g_recent_playlist_count == RECENT_PLAYLISTS) {
            memmove(g_recent_playlists, g_recent_playlists + 1, (RECENT_PLAYLISTS - 1) * sizeof g_recent_playlists[0]); --g_recent_playlist_count;
        }
        i = g_recent_playlist_count++;
    }
    memset(&g_recent_playlists[i], 0, sizeof g_recent_playlists[i]);
    copy(g_recent_playlists[i].profile, sizeof g_recent_playlists[i].profile, g_library_profile);
    copy(g_recent_playlists[i].playlist, sizeof g_recent_playlists[i].playlist, playlist);
    copy(g_recent_playlists[i].title, sizeof g_recent_playlists[i].title, *title ? title : playlist);
}
static int upcoming(const char *id) {
    for (int i = 0; i < 64; ++i) if (g_upcoming[i].scope == g_playlist_scope && !strcmp(g_upcoming[i].id, id)) return 1;
    return 0;
}
static void upcoming_note(const char *id) {
    if (upcoming(id)) return;
    const int slot = (int)(g_upcoming_cursor++ % 64);
    copy(g_upcoming[slot].id, sizeof g_upcoming[slot].id, id); g_upcoming[slot].scope = g_playlist_scope;
}
static int comment_thread_current(void) {
    return *g_comment_thread.video && g_comment_thread.scope == g_playlist_scope && !strcmp(g_comment_thread.profile, g_library_profile);
}
/* Everything the profile kept on this PS5 beyond its library records. */
static void profile_forget(const char *profile) {
    for (int i = g_channel_mark_count - 1; i >= 0; --i) if (!strcmp(g_channel_marks[i].profile, profile)) channel_mark_drop(i);
    for (int i = g_caption_memory_count - 1; i >= 0; --i) if (!strcmp(g_caption_memories[i].profile, profile)) {
        memmove(&g_caption_memories[i], &g_caption_memories[i + 1], (size_t)(g_caption_memory_count - i - 1) * sizeof g_caption_memories[0]);
        --g_caption_memory_count;
    }
    for (int i = g_recent_playlist_count - 1; i >= 0; --i) if (!strcmp(g_recent_playlists[i].profile, profile)) {
        memmove(&g_recent_playlists[i], &g_recent_playlists[i + 1], (size_t)(g_recent_playlist_count - i - 1) * sizeof g_recent_playlists[0]);
        --g_recent_playlist_count;
    }
}
static int caption_memory_find(const char *channel) {
    for (int i = 0; i < g_caption_memory_count; ++i)
        if (!strcmp(g_caption_memories[i].profile, g_library_profile) && !strcmp(g_caption_memories[i].channel, channel)) return i;
    return -1;
}
/* Captions for a video of this channel: what the profile last chose while
 * watching the channel, else its own Captions setting. */
static int captions_wanted(const char *channel) {
    const int i = g_caption_memory && tube_sb_channel_valid(channel) ? caption_memory_find(channel) : -1;
    return i >= 0 ? g_caption_memories[i].enabled : g_captions;
}
int tube_captions_for(const char *id) {
    const Record *r = record(id, 0);
    return captions_wanted(r ? r->item.parent_id : "");
}
int tube_remember_captions(const char *id, int enabled) {
    const Record *r = record(id, 0);
    if (!g_caption_memory || !r || !tube_sb_channel_valid(r->item.parent_id)) return 0;
    int i = caption_memory_find(r->item.parent_id);
    if (i >= 0 && g_caption_memories[i].enabled == !!enabled) return 1;
    if (i < 0) {
        if (g_caption_memory_count == CAPTION_MEMORIES) { /* the oldest entry leaves */
            memmove(g_caption_memories, g_caption_memories + 1, (CAPTION_MEMORIES - 1) * sizeof g_caption_memories[0]); --g_caption_memory_count;
        }
        i = g_caption_memory_count++;
        copy(g_caption_memories[i].profile, sizeof g_caption_memories[i].profile, g_library_profile);
        copy(g_caption_memories[i].channel, sizeof g_caption_memories[i].channel, r->item.parent_id);
    }
    g_caption_memories[i].enabled = !!enabled; save(); return 1;
}
/* Import files are plain names inside <data>/import: no path, no hidden file. */
#define IMPORT_FILES 24
static int import_name_valid(const char *name) {
    const size_t length = name ? strlen(name) : 0;
    return length && length < 60 && name[0] != '.' &&
        strspn(name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._-") == length;
}
static long long import_size(const char *name) {
    char path[640]; struct stat info;
    snprintf(path, sizeof path, "%s/%s", evo_data_path("import"), name);
    return !stat(path, &info) && S_ISREG(info.st_mode) && info.st_size > 0 && info.st_size <= 1024 * 1024 ? (long long)info.st_size : 0;
}
static int import_compare(const void *a, const void *b) { return strcmp(a, b); }
static int import_files(char names[IMPORT_FILES][64]) {
    char directory[512]; int count = 0;
    copy(directory, sizeof directory, evo_data_path("import"));
    evo_mkdir(evo_data_dir()); evo_mkdir(directory); /* the place to copy an export to */
    import_dir *dir = import_opendir(directory); if (!dir) return 0;
    for (struct dirent *entry; count < IMPORT_FILES && (entry = import_readdir(dir)) != NULL;)
        if (import_name_valid(entry->d_name) && import_size(entry->d_name)) copy(names[count++], 64, entry->d_name);
    import_closedir(dir);
    qsort(names, (size_t)count, 64, import_compare); return count;
}
/* The whole file as text; NULL when it is missing, empty, too large or binary. */
static char *import_read(const char *name) {
    char path[640]; snprintf(path, sizeof path, "%s/%s", evo_data_path("import"), name);
    const long long size = import_size(name);
    FILE *file = size ? fopen(path, "rb") : NULL; if (!file) return NULL;
    char *body = calloc(1, (size_t)size + 1);
    if (body && (fread(body, 1, (size_t)size, file) != (size_t)size || memchr(body, 0, (size_t)size))) { free(body); body = NULL; }
    fclose(file); return body;
}
static void import_release(void) { free(g_import.body); g_import.body = NULL; }

/* With the built-in service nothing on screen names a companion. The texts are
 * shared by both modes; here the word is replaced as a row or a message is made. */
static void embedded_wording(char *text, size_t capacity) {
    static const char *const pairs[][2] = {
        {" and companion pairing", ""}, {"The paired companion", "The built-in service"}, {"the paired companion", "the built-in service"},
        {"The companion", "The built-in service"}, {"the companion", "the built-in service"}, {"companion", "service"}, {"Companion", "Service"}};
    if (!g_embedded) return;
    for (size_t i = 0; i < sizeof pairs / sizeof *pairs; ++i) {
        const size_t from = strlen(pairs[i][0]), to = strlen(pairs[i][1]);
        for (char *at; (at = strstr(text, pairs[i][0]));) {
            const size_t tail = strlen(at + from) + 1;
            if ((size_t)(at - text) + to + tail > capacity) return;
            memmove(at + to, at + from, tail); memcpy(at, pairs[i][1], to);
        }
    }
}

void tube_set_error(const char *message) {
    /* Never echo remote URLs, headers, or arbitrary resolver diagnostics. */
    char clean[256]; copy(clean, sizeof clean, message);
    for (size_t i = 0; clean[i]; ++i) if ((unsigned char)clean[i] < 32) clean[i] = ' ';
    if (strstr(clean, "://") || strstr(clean, "Bearer")) copy(clean, sizeof clean, "The companion request failed. Check its configuration and retry.");
    embedded_wording(clean, sizeof clean);
    evo_error_set("%s", clean);
}

/* Keep the failure on its sign-in session: background polls have no screen callback. */
static void auth_request_error(Request *r, const char *message) {
    if (r->kind == 9 && r->intent == g_auth.serial &&
        (g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING)) {
        copy(g_auth.error, sizeof g_auth.error, message);
        for (size_t i=0; g_auth.error[i]; ++i)
            if ((unsigned char)g_auth.error[i] < 32) g_auth.error[i]=' ';
        if (strstr(g_auth.error, "://") || strstr(g_auth.error, "Bearer"))
            copy(g_auth.error, sizeof g_auth.error, "The sign-in request failed. Start a new session.");
        embedded_wording(g_auth.error, sizeof g_auth.error);
    }
    tube_set_error(message);
}

/* Content language and country offered by the companion (SmartTube's lists).
 * The same language list names the preferred caption and audio languages. */
static const struct { const char *code, *name; } g_languages[] = {
    {"ar","Arabic"}, {"az","Azerbaijani"}, {"bg","Bulgarian"}, {"bn","Bengali"}, {"ca","Catalan"}, {"cs","Czech"},
    {"da","Danish"}, {"de","German"}, {"el","Greek"}, {"en","English"}, {"en-NZ","English (New Zealand)"}, {"es","Spanish"},
    {"et","Estonian"}, {"fa","Persian"}, {"fi","Finnish"}, {"fr","French"}, {"gl","Galician"}, {"he","Hebrew"},
    {"hi","Hindi"}, {"hr","Croatian"}, {"hu","Hungarian"}, {"hy","Armenian"}, {"id","Indonesian"}, {"it","Italian"},
    {"ja","Japanese"}, {"ko","Korean"}, {"lt","Lithuanian"}, {"lv","Latvian"}, {"mo","Moldovan"}, {"nb","Norwegian"},
    {"nl","Dutch"}, {"pl","Polish"}, {"pt-BR","Portuguese (Brazil)"}, {"pt-PT","Portuguese (Portugal)"}, {"ro","Romanian"},
    {"ru","Russian"}, {"sk","Slovak"}, {"sl","Slovenian"}, {"sq","Albanian"}, {"sr","Serbian"}, {"sv","Swedish"},
    {"te","Telugu"}, {"th","Thai"}, {"tjl","Tai"}, {"tr","Turkish"}, {"uk","Ukrainian"}, {"vi","Vietnamese"},
    {"zh-CN","Chinese (Simplified)"}, {"zh-TW","Chinese (Traditional)"}
};
#define LANGUAGES ((int)(sizeof g_languages / sizeof *g_languages))
_Static_assert(sizeof g_languages / sizeof *g_languages == 49, "the companion accepts these 49 language tags");
static const struct { const char *code, *name; } g_countries[] = {
    {"US","United States"}, {"GB","United Kingdom"}, {"HR","Croatia"}, {"CA","Canada"}, {"UA","Ukraine"}, {"RU","Russia"},
    {"BY","Belarus"}, {"KZ","Kazakhstan"}, {"AZ","Azerbaijan"}, {"AM","Armenia"}, {"RS","Serbia"}, {"EE","Estonia"},
    {"BG","Bulgaria"}, {"DE","Germany"}, {"RO","Romania"}, {"ES","Spain"}, {"PL","Poland"}, {"FI","Finland"},
    {"LV","Latvia"}, {"FR","France"}, {"HU","Hungary"}, {"NL","Netherlands"}, {"BE","Belgium"}, {"NO","Norway"},
    {"CZ","Czechia"}, {"IT","Italy"}, {"GR","Greece"}, {"DK","Denmark"}, {"LT","Lithuania"}, {"MD","Moldova"},
    {"SK","Slovakia"}, {"SI","Slovenia"}, {"SE","Sweden"}, {"SG","Singapore"}, {"NZ","New Zealand"}, {"NG","Nigeria"},
    {"AU","Australia"}, {"CH","Switzerland"}, {"IL","Israel"}, {"SA","Saudi Arabia"}, {"MA","Morocco"}, {"BD","Bangladesh"},
    {"BR","Brazil"}, {"PT","Portugal"}, {"AR","Argentina"}, {"MX","Mexico"}, {"HK","Hong Kong"}, {"MY","Malaysia"},
    {"JP","Japan"}, {"IN","India"}, {"KE","Kenya"}, {"TR","Turkey"}, {"EG","Egypt"}, {"VN","Vietnam"}, {"CN","China"},
    {"TW","Taiwan"}, {"ID","Indonesia"}, {"KR","South Korea"}, {"PK","Pakistan"}, {"BO","Bolivia"}, {"CL","Chile"},
    {"CO","Colombia"}, {"CR","Costa Rica"}, {"EC","Ecuador"}, {"SV","El Salvador"}, {"GT","Guatemala"}, {"HN","Honduras"},
    {"NI","Nicaragua"}, {"PA","Panama"}, {"PR","Puerto Rico"}, {"PY","Paraguay"}, {"PE","Peru"}, {"DO","Dominican Republic"},
    {"UY","Uruguay"}, {"VE","Venezuela"}, {"IE","Ireland"}, {"PH","Philippines"}, {"IQ","Iraq"}, {"AT","Austria"},
    {"TZ","Tanzania"}, {"NP","Nepal"}, {"ZA","South Africa"}, {"ZW","Zimbabwe"}, {"DZ","Algeria"}, {"TH","Thailand"},
    {"MK","North Macedonia"}, {"TN","Tunisia"}, {"LK","Sri Lanka"}, {"JO","Jordan"}
};
#define COUNTRIES ((int)(sizeof g_countries / sizeof *g_countries))
_Static_assert(sizeof g_countries / sizeof *g_countries == 89, "the companion accepts these 89 country codes");
static int language_index(const char *code) {
    for (int i = 0; code && i < LANGUAGES; ++i) if (!strcmp(code, g_languages[i].code)) return i;
    return -1;
}
static int country_index(const char *code) {
    for (int i = 0; code && i < COUNTRIES; ++i) if (!strcmp(code, g_countries[i].code)) return i;
    return -1;
}
/* Codes the caption and audio lists offered before they used the list above. */
static int legacy_track_language(const char *code) {
    static const char *const codes[] = {"en", "fr", "es", "de", "it", "pt", "ja", "ko", "zh", "ru"};
    for (unsigned i = 0; code && i < sizeof codes / sizeof *codes; ++i) if (!strcmp(code, codes[i])) return 1;
    return 0;
}
static char g_hl[9], g_gl[3];
/* SmartTube's sixteen "Hide unwanted content" switches, by the companion's names. */
static const struct { const char *name, *label; } g_hide_flags[] = {
    {"shorts_all", "Hide Shorts everywhere"}, {"mixes", "Hide mixes"},
    {"watched_watch_later", "Hide watched from Watch later"}, {"watched_home", "Hide watched from Home"},
    {"watched_subscriptions", "Hide watched from Subscriptions"}, {"watched_notifications", "Hide watched from Notifications"},
    {"shorts_subscriptions", "Hide Shorts from Subscriptions"}, {"shorts_search", "Hide Shorts from Search"},
    {"shorts_home", "Hide Shorts from Home"}, {"shorts_channel", "Hide Shorts from Channel"},
    {"shorts_history", "Hide Shorts from History"}, {"shorts_trending", "Hide Shorts from Trending"},
    {"streams_subscriptions", "Hide streams from Subscriptions"}, {"upcoming_subscriptions", "Hide upcoming from Subscriptions"},
    {"upcoming_home", "Hide upcoming from Home"}, {"upcoming_channel", "Hide upcoming from Channel"}
};
#define HIDE_FLAGS 16
_Static_assert(sizeof g_hide_flags / sizeof *g_hide_flags == HIDE_FLAGS, "sixteen hide switches");
#define HIDE_WATCHED_HOME 3
/* Until a switch is changed nothing is sent and the companion keeps its
 * hard-wired rule: watched cards leave the primary Home grid. */
static unsigned g_hide; static int g_hide_set;
static int hide_index(const char *name) {
    for (int i = 0; name && i < HIDE_FLAGS; ++i) if (!strcmp(name, g_hide_flags[i].name)) return i;
    return -1;
}
static unsigned hide_effective(void) { return g_hide_set ? g_hide : 1u << HIDE_WATCHED_HOME; }
static const char *const g_thumbnail_sources[] = {"default", "start", "middle", "end"};
static const char *const g_thumbnail_labels[] = {"Default", "Video start", "Video middle", "Video end"};
static int g_thumbnails;
static int thumbnail_index(const char *name) {
    for (int i = 1; name && i < 4; ++i) if (!strcmp(name, g_thumbnail_sources[i])) return i;
    return 0;
}
static int g_sb_alt, g_section_playlist = 1;
/* What the paired companion advertises in /v1/health. An older companion
 * rejects unknown request fields, so second-batch fields and actions are sent
 * only after its feature list was read for the current source and pairing;
 * until then, and when the read fails, requests keep their earlier form. */
enum { COMPANION_LOCALE = 1, COMPANION_THUMBNAILS = 2, COMPANION_FILTERS = 4, COMPANION_CHANNELS = 8,
       COMPANION_SB_ALT = 16, COMPANION_LIBRARY = 32,
       /* third batch */
       COMPANION_TRANSLATED = 64, COMPANION_DETAILS = 128, COMPANION_IMPORT = 256, COMPANION_KIDS = 512,
       COMPANION_REMINDER = 1024, COMPANION_PLAYLIST_ORDER = 2048, COMPANION_NOTIFICATIONS = 4096,
       COMPANION_COMMENT_POST = 8192, COMPANION_SEARCH_FIRST = 16384, COMPANION_NEW_CONTENT = 32768,
       COMPANION_DISLIKES = 65536,
       /* sign-in by device code; the built-in service lists it once it keeps accounts itself */
       COMPANION_DEVICE_AUTH = 131072,
       /* the phone receiver (POST /v1/remote). Only the built-in service lists it; a companion has
        * always had the route, so the bit is asked about in built-in mode alone (embedded_lacks). */
       COMPANION_REMOTE = 262144,
       /* a segments request may name the alternative SponsorBlock server; the built-in service alone */
       COMPANION_SB_SERVER = 524288 };
static struct { char source[512]; unsigned features; int known; } g_companion;
static int companion_known(void) { return g_companion.known && *g_source && !strcmp(g_companion.source, g_source); }
static int companion_has(unsigned feature) { return companion_known() && (g_companion.features & feature) == feature; }
static void companion_forget(void) { memset(&g_companion, 0, sizeof g_companion); }
/* A built-in service that says it has no sign-in (an older service bundle): the account
 * rows are replaced by one explanation. Before its feature list was read, and whenever
 * the list names sign-in, accounts are offered as with a companion. */
static int embedded_guest_only(void) { return g_embedded && companion_known() && !companion_has(COMPANION_DEVICE_AUTH); }
/* The same rule for every other part: a built-in service that does not list it has no
 * row for it; before the list was read, the service the app carries is assumed. */
static int embedded_lacks(unsigned feature) { return g_embedded && companion_known() && !companion_has(feature); }

int tube_embedded_mode(void) { return g_embedded; }
const char *tube_embedded_address(void) { return g_embedded && !strncmp(g_source, "http://", 7) ? g_source + 7 : ""; }
const char *tube_embedded_error(void) { return g_embedded_error; }
/* Asks the service for its address and token; starting it is idempotent and
 * never waits for the network. 1 = requests may go out. */
static int embedded_start(void) {
#ifdef TUBE_EMBEDDED_SERVICE
    if (!g_embedded || g_embedded_stopped) return 0;
    unsigned short port = 0; char token[65] = "", source[40];
    /* Each is asked only for a profile that turned it on: segments, and the dislike count of an opened details page.
     * Manifests are ranged: since 2026-10-14 YouTube answers 403 to an open-ended or whole-file range. */
    static const tube_service_options options = {.sponsorblock = 1, .return_youtube_dislike = 1, .ranged_manifest = 1};
    if (!tube_service_running()) tube_service_configure(&options);
    int rc = tube_service_start(evo_data_dir(), &port, token);
    token[64] = 0;
    int usable = !rc && port && strlen(token) >= 24;
    for (size_t i = 0; token[i]; ++i) if (!isalnum((unsigned char)token[i]) && token[i] != '-' && token[i] != '_') usable = 0;
    if (!usable) {
        const char *why = rc ? tube_service_error() : "";
        copy(g_embedded_error, sizeof g_embedded_error, why && *why ? why : "the service gave no address");
        g_source[0] = g_token[0] = 0; companion_forget(); return 0;
    }
    snprintf(source, sizeof source, "http://127.0.0.1:%u", (unsigned)port);
    if (strcmp(source, g_source) || strcmp(token, g_token)) companion_forget();
    copy(g_source, sizeof g_source, source); copy(g_token, sizeof g_token, token);
    for (size_t i = 0; i < sizeof token; ++i) token[i] = 0;
    g_pin[0] = 0; g_embedded_error[0] = 0; return 1;
#else
    return 0;
#endif
}
int tube_embedded_start(void) { if (!g_embedded) return 0; g_embedded_stopped = 0; return embedded_start(); }
void tube_embedded_stop(void) {
#ifdef TUBE_EMBEDDED_SERVICE
    if (!g_embedded) return;
    tube_cancel();   /* while the address is still known: pending jobs are told to stop */
    g_embedded_stopped = 1; g_source[0] = g_token[0] = 0; companion_forget();
    tube_service_stop();
#endif
}
static int embedded_ready(void) {
#ifdef TUBE_EMBEDDED_SERVICE
    if (g_embedded_stopped) return 0;
    if (tube_service_running() && *g_source && *g_token) return 1;
    return embedded_start();
#else
    return 0;
#endif
}
/* The mode of this run, decided before the store is read. A file named
 * external-companion in the data folder keeps the LAN companion (development). */
static void embedded_select(void) {
    g_embedded = g_embedded_stopped = 0; g_embedded_error[0] = g_saved_source[0] = g_saved_token[0] = 0;
#ifdef TUBE_EMBEDDED_SERVICE
    g_embedded = access(evo_data_path("external-companion"), F_OK) != 0;
#endif
}
static void embedded_adopt(void) {
    if (!g_embedded) return;
    copy(g_saved_source, sizeof g_saved_source, g_source); copy(g_saved_token, sizeof g_saved_token, g_token);
    g_source[0] = g_token[0] = 0; companion_forget();
    embedded_start();
}
static void companion_read(const cJSON *root) {
    static const struct { const char *name; unsigned bit; } names[] = {
        {"locale", COMPANION_LOCALE}, {"thumbnail_source", COMPANION_THUMBNAILS}, {"content_filters", COMPANION_FILTERS},
        {"channel_pages", COMPANION_CHANNELS}, {"sponsorblock_alt", COMPANION_SB_ALT}, {"library", COMPANION_LIBRARY},
        {"translated_captions", COMPANION_TRANSLATED}, {"video_details", COMPANION_DETAILS}, {"subscriptions_import", COMPANION_IMPORT},
        {"kids", COMPANION_KIDS}, {"stream_reminder", COMPANION_REMINDER}, {"playlist_order", COMPANION_PLAYLIST_ORDER},
        {"subscribe_notifications", COMPANION_NOTIFICATIONS}, {"comment_post", COMPANION_COMMENT_POST},
        {"search_first", COMPANION_SEARCH_FIRST}, {"channel_new_content", COMPANION_NEW_CONTENT}, {"dislikes", COMPANION_DISLIKES},
        {"device_auth", COMPANION_DEVICE_AUTH}, {"remote", COMPANION_REMOTE}, {"sponsorblock_server", COMPANION_SB_SERVER}
    };
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features"), *feature;
    companion_forget();
    if (num(root, "api_version") != 1 || !cJSON_IsArray(features)) return;
    cJSON_ArrayForEach(feature, features) for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
        if (cJSON_IsString(feature) && !strcmp(feature->valuestring, names[i].name)) g_companion.features |= names[i].bit;
    if (boolean(root, "sponsorblock_alt")) g_companion.features |= COMPANION_SB_ALT;
    copy(g_companion.source, sizeof g_companion.source, g_source); g_companion.known = 1;
}
/* A changed locale, filter or thumbnail choice must not continue pages that
 * were opened with the earlier one: lists, cursors and retained rows retire. */
static void catalog_retire(void) {
    live_retire();
    if (!++g_playlist_scope) ++g_playlist_scope;
    memset(&g_selected_playlist, 0, sizeof g_selected_playlist);
    g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors);
    g_search_feed.key[0] = 0; g_channel_list.complete = 0;
}
/* Channel page facts from the companion's first page. */
static const char *const g_channel_tabs[] = {"home", "videos", "shorts", "live", "playlists"};
static const char *const g_channel_tab_labels[] = {"Home", "Videos", "Shorts", "Live", "Playlists"};
static const char *const g_channel_sorts[] = {"newest", "popular", "oldest"};
static const char *const g_channel_sort_labels[] = {"Newest first", "Most popular", "Oldest first"};
static struct { char channel[25], profile[33]; unsigned scope, tabs, sorts; int search, tab, sort; } g_channel_facts;
static int channel_facts_current(const char *channel) {
    return *g_channel_facts.channel && !strcmp(g_channel_facts.channel, channel) && g_channel_facts.scope == g_playlist_scope &&
        !strcmp(g_channel_facts.profile, g_library_profile);
}
/* YouTube's own pause state of the account history, from its last listing. */
static struct { int state; char profile[33]; unsigned scope; } g_history_paused; /* state: 0 unknown, 1 active, 2 paused */

/* Helpers share the provider persistence boundary. */
#include "tube_profile_settings.h"

static int profile_speed_mode(const char *profile) {
    if (settings_current(profile)) return g_speed_mode;
    int i = profile_settings_slot(profile,0); return i < 0 ? 1 : g_profile_settings[i].value.speed_mode;
}
static int profile_speed(const char *profile) {
    if (settings_current(profile)) return g_speed;
    int i = profile_settings_slot(profile,0); return i < 0 ? 100 : g_profile_settings[i].value.speed;
}
int tube_playback_speed(void) { return g_speed; }
int tube_speed_count(void) { return (int)(sizeof tube_speeds / sizeof *tube_speeds); }
int tube_speed_at(int index) { return index >= 0 && index < tube_speed_count() ? tube_speeds[index] : 0; }
int tube_set_playback_speed(int percent) {
    if (!valid_speed(percent)) return 0;
    if (percent != 100) g_last_speed = percent;
    else if (g_speed != 100) g_last_speed = g_speed;
    g_speed = percent; save(); return 1;
}
int tube_last_playback_speed(void) {
    const char *profile = valid_id(g_active_id) && valid_profile(g_active_profile) ? g_active_profile : g_library_profile;
    if (settings_current(profile)) return g_last_speed;
    int i = profile_settings_slot(profile,0); return i < 0 ? 100 : g_profile_settings[i].value.last_speed;
}
static const char *speed_profile(const char *id) {
    return id && !strcmp(id, g_active_id) && valid_profile(g_active_profile) ? g_active_profile : g_library_profile;
}
static const char *speed_key(const char *id, const char *profile) {
    if (!valid_id(id)) return "";
    if (profile_speed_mode(profile) == 2) return id;
    const Record *r = record_for(id, profile, 0);
    /* Channel identity is public video metadata; memory remains profile-keyed. */
    if (!r) for (int i = 0; i < g_count; ++i)
        if (!strcmp(g_records[i].item.id, id)) { r = &g_records[i]; break; }
    return r && tube_sb_channel_valid(r->item.parent_id) ? r->item.parent_id : "";
}
static int speed_lookup(const char *id, const char *profile) {
    const int mode = profile_speed_mode(profile);
    if (mode == 0) return profile_speed(profile);
    if (mode == 3) return 100;
    const char *key = speed_key(id, profile);
    for (int i = 0; *key && i < g_speed_count; ++i)
        if (g_speeds[i].mode == mode && !strcmp(g_speeds[i].profile, profile) && !strcmp(g_speeds[i].key, key))
            return g_speeds[i].percent;
    return 100;
}
int tube_playback_speed_for(const char *id) {
    if (!id || !valid_id(id)) return 100;
    const char *profile = speed_profile(id);
    if (!strcmp(id, g_speed_session_id) && !strcmp(profile, g_speed_session_profile)) return g_speed_session;
    return speed_lookup(id, profile);
}
int tube_remember_playback_speed(const char *id, int percent) {
    if (!valid_id(id) || !valid_speed(percent)) return 0;
    const int previous = tube_playback_speed_for(id);
    const char *profile = speed_profile(id);
    const int mode = profile_speed_mode(profile);
    int preference = profile_settings_slot(settings_current(profile) ? settings_key() : profile,1); if (preference < 0) return 0;
    const char *key = speed_key(id, profile);
    if ((mode == 1 || mode == 2) && *key) {
        int slot = -1;
        for (int i = 0; i < g_speed_count; ++i)
            if (g_speeds[i].mode == mode && !strcmp(g_speeds[i].profile, profile) && !strcmp(g_speeds[i].key, key)) { slot = i; break; }
        /* Normal speed removes an override. The bounded list retains the most
         * recently changed entries and never evicts library/favorite records. */
        if (slot >= 0) {
            memmove(&g_speeds[slot], &g_speeds[slot + 1], (size_t)(g_speed_count - slot - 1) * sizeof g_speeds[0]);
            --g_speed_count;
        }
        if (percent != 100) {
            if (g_speed_count == SPEED_MEMORIES) {
                memmove(g_speeds, g_speeds + 1, (SPEED_MEMORIES - 1) * sizeof g_speeds[0]); --g_speed_count;
            }
            slot = g_speed_count++;
            copy(g_speeds[slot].profile, sizeof g_speeds[slot].profile, profile);
            copy(g_speeds[slot].key, sizeof g_speeds[slot].key, key);
            g_speeds[slot].mode = mode; g_speeds[slot].percent = percent;
        }
    }
    copy(g_speed_session_id, sizeof g_speed_session_id, id);
    copy(g_speed_session_profile, sizeof g_speed_session_profile, profile);
    g_speed_session = percent;
    if (settings_current(profile)) {
        g_speed = previous; return tube_set_playback_speed(percent);
    }
    ProfileSettings *settings = &g_profile_settings[preference].value;
    if (percent != 100) settings->last_speed = percent;
    else if (previous != 100) settings->last_speed = previous;
    settings->speed = percent; save(); return 1;
}

static void speed_load(const cJSON *root) {
    g_speed_count = 0; g_speed_session_id[0] = g_speed_session_profile[0] = 0;
    double last = num(root, "last_speed_percent"), mode = num(root, "speed_memory_mode");
    g_last_speed = valid_speed(last) ? (int)last : (g_speed != 100 ? g_speed : 100);
    const cJSON *setting = cJSON_GetObjectItemCaseSensitive(root, "speed_memory_mode");
    g_speed_mode = setting && cJSON_IsNumber(setting) && mode >= 0 && mode <= 3 && mode == (int)mode ? (int)mode :
        cJSON_HasObjectItem(root, "speed_percent") ? 0 : 1;
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(root, "speed_memories"), *row;
    if (!cJSON_IsArray(rows)) return;
    cJSON_ArrayForEach(row, rows) {
        const char *profile = str(row, "profile"), *key = str(row, "key");
        double kind = num(row, "mode"), percent = num(row, "percent");
        if (!valid_profile(profile) || !valid_speed(percent) || percent == 100 ||
            !((kind == 1 && tube_sb_channel_valid(key)) || (kind == 2 && valid_id(key)))) continue;
        int duplicate = 0;
        for (int i = 0; i < g_speed_count; ++i)
            if (g_speeds[i].mode == kind && !strcmp(g_speeds[i].key, key) && !strcmp(g_speeds[i].profile, profile)) duplicate = 1;
        if (duplicate) continue;
        if (g_speed_count == SPEED_MEMORIES) break;
        int i = g_speed_count++;
        copy(g_speeds[i].profile, sizeof g_speeds[i].profile, profile);
        copy(g_speeds[i].key, sizeof g_speeds[i].key, key);
        g_speeds[i].mode = (int)kind; g_speeds[i].percent = (int)percent;
    }
}


static int g_storage_notice;
static const char *storage_error(void) {
    return g_store_writable ? "Could not save local changes; they may be lost when the app closes. Check free space and access to the data folder." :
        "Local data could not be loaded. The existing file has not been overwritten.";
}
static void storage_failed(void) {
    if (!g_storage_notice) g_storage_notice = 1;
    tube_set_error(storage_error());
}
const char *tube_take_storage_notice(void) {
    if (g_storage_notice != 1) return NULL;
    g_storage_notice = 2;
    return storage_error();
}
static void save(void) {
    g_remote_write_failed = 1;
    if (!g_store_writable) {
        storage_failed(); return;
    }
    cJSON *root = cJSON_CreateObject(), *rows = cJSON_CreateArray();
    if (!root || !rows) { cJSON_Delete(root); cJSON_Delete(rows); storage_failed(); return; }
    profile_settings_write(root);
    cJSON *blocked=cJSON_AddArrayToObject(root,"blocked_channels");
    for(int i=0;i<g_blocked_count;++i) {
        cJSON *row=cJSON_CreateObject();
        cJSON_AddStringToObject(row,"profile",g_blocked[i].profile);
        cJSON_AddStringToObject(row,"channel",g_blocked[i].channel);
        cJSON_AddStringToObject(row,"name",g_blocked[i].name);
        cJSON_AddItemToArray(blocked,row);
    }
    cJSON *searches=cJSON_AddArrayToObject(root,"search_history");
    for(int i=0;searches && i<g_search_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_searches[i].profile);
        cJSON_AddStringToObject(row,"query",g_searches[i].query);
        cJSON_AddItemToArray(searches,row);
    }
    cJSON *pins=cJSON_AddArrayToObject(root,"pinned_sections");
    for(int i=0;pins && i<g_pin_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_pins[i].profile);
        cJSON_AddStringToObject(row,"id",g_pins[i].id);
        cJSON_AddStringToObject(row,"title",g_pins[i].title);
        cJSON_AddItemToArray(pins,row);
    }
    cJSON *locks=cJSON_AddArrayToObject(root,"profile_locks");
    for(int i=0;locks && i<g_lock_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_locks[i].profile);
        cJSON_AddStringToObject(row,"password",g_locks[i].record);
        cJSON_AddItemToArray(locks,row);
    }
    cJSON *follows=cJSON_AddArrayToObject(root,"followed_channels");
    for(int i=0;follows && i<g_follow_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_follows[i].profile);
        cJSON_AddStringToObject(row,"channel",g_follows[i].channel);
        cJSON_AddStringToObject(row,"title",g_follows[i].title);
        cJSON_AddItemToArray(follows,row);
    }
    cJSON *marks=cJSON_AddArrayToObject(root,"seen_channels");
    for(int i=0;marks && i<g_channel_mark_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_channel_marks[i].profile);
        cJSON_AddStringToObject(row,"channel",g_channel_marks[i].channel);
        cJSON_AddStringToObject(row,"video",g_channel_marks[i].video);
        cJSON_AddNumberToObject(row,"seen",(double)g_channel_marks[i].seen);
        cJSON_AddItemToArray(marks,row);
    }
    cJSON *recent=cJSON_AddArrayToObject(root,"recent_playlists");
    for(int i=0;recent && i<g_recent_playlist_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_recent_playlists[i].profile);
        cJSON_AddStringToObject(row,"playlist",g_recent_playlists[i].playlist);
        cJSON_AddStringToObject(row,"title",g_recent_playlists[i].title);
        cJSON_AddItemToArray(recent,row);
    }
    cJSON *caption_memories=cJSON_AddArrayToObject(root,"caption_memories");
    for(int i=0;caption_memories && i<g_caption_memory_count;++i) {
        cJSON *row=cJSON_CreateObject(); if(!row)break;
        cJSON_AddStringToObject(row,"profile",g_caption_memories[i].profile);
        cJSON_AddStringToObject(row,"channel",g_caption_memories[i].channel);
        cJSON_AddBoolToObject(row,"enabled",g_caption_memories[i].enabled);
        cJSON_AddItemToArray(caption_memories,row);
    }
    cJSON_AddNumberToObject(root, "backup_auto_days", g_backup_auto);
    cJSON_AddStringToObject(root, "sponsorblock_server", g_sb_server);
    cJSON_AddBoolToObject(root, "separate_settings", g_separate_settings);
    cJSON_AddNumberToObject(root, "version", 2);
    cJSON_AddStringToObject(root, "source", g_embedded ? g_saved_source : g_source);
    cJSON_AddStringToObject(root, "token", g_embedded ? g_saved_token : g_token);
    cJSON_AddNumberToObject(root, "max_height", g_height);
    cJSON_AddNumberToObject(root, "max_fps", g_max_fps);
    cJSON_AddNumberToObject(root, "speed_percent", g_speed);
    cJSON_AddNumberToObject(root, "last_speed_percent", g_last_speed);
    cJSON_AddNumberToObject(root, "speed_memory_mode", g_speed_mode);
    cJSON *speeds = cJSON_AddArrayToObject(root, "speed_memories");
    for (int i = 0; speeds && i < g_speed_count; ++i) {
        cJSON *row = cJSON_CreateObject(); if (!row) break;
        cJSON_AddStringToObject(row, "profile", g_speeds[i].profile);
        cJSON_AddStringToObject(row, "key", g_speeds[i].key);
        cJSON_AddNumberToObject(row, "mode", g_speeds[i].mode);
        cJSON_AddNumberToObject(row, "percent", g_speeds[i].percent);
        cJSON_AddItemToArray(speeds, row);
    }
    cJSON_AddNumberToObject(root, "codec", g_codec);
    cJSON_AddBoolToObject(root, "captions", g_captions);
    cJSON_AddBoolToObject(root, "automatic_captions", g_auto_captions);
    cJSON_AddStringToObject(root, "language", g_language);
    cJSON_AddStringToObject(root, "audio_language", g_audio_language);
    /* A profile waiting for its password at startup stays the saved choice. */
    cJSON_AddStringToObject(root, "library_profile", *g_startup_lock ? g_startup_lock : g_library_profile);
    cJSON_AddBoolToObject(root, "sponsorblock", g_sponsor);
    cJSON_AddNumberToObject(root, "player_chat_mode", g_player_chat_mode);
    cJSON_AddNumberToObject(root, "sleep_timer_minutes", g_sleep_minutes);
    cJSON *player_preferences = cJSON_CreateArray();
    cJSON_AddItemToObject(root,"player_preferences",player_preferences);
    for (int i = 0; player_preferences && i < g_player_preferences_count; ++i) {
        cJSON *preference = cJSON_CreateObject();
        if (!preference) break;
        cJSON_AddStringToObject(preference,"profile",g_player_preferences[i].profile);
        cJSON_AddNumberToObject(preference,"ok_mode",g_player_preferences[i].ok_mode);
        cJSON_AddNumberToObject(preference,"hide_ms",g_player_preferences[i].hide_ms);
        cJSON_AddNumberToObject(preference,"seek_mode",g_player_preferences[i].seek_mode);
        cJSON_AddNumberToObject(preference,"seek_seconds",g_player_preferences[i].seek_seconds);
        cJSON_AddNumberToObject(preference,"time_mode",g_player_preferences[i].time_mode);
        cJSON_AddNumberToObject(preference,"resize_mode",g_player_preferences[i].resize_mode);
        cJSON_AddNumberToObject(preference,"zoom_percent",g_player_preferences[i].zoom_percent);
        cJSON_AddNumberToObject(preference,"aspect_mode",g_player_preferences[i].aspect_mode);
        cJSON_AddNumberToObject(preference,"rotation_degrees",g_player_preferences[i].rotation_degrees);
        cJSON_AddNumberToObject(preference,"mirror_enabled",g_player_preferences[i].mirror_enabled);
        cJSON_AddItemToArray(player_preferences,preference);
    }
    cJSON_AddNumberToObject(root,"pixel_ratio_mode",g_pixel_mode);
    cJSON_AddBoolToObject(root, "remote_enabled", g_remote_enabled);
    cJSON_AddBoolToObject(root, "remote_stop_on_disconnect", g_remote_stop_on_disconnect);
    cJSON_AddBoolToObject(root, "remote_close_on_disconnect", g_remote_close_on_disconnect);
    cJSON_AddBoolToObject(root, "remote_disable_history", g_remote_no_history);
    cJSON_AddBoolToObject(root, "select_account_on_boot", g_select_account_on_boot);
    cJSON_AddStringToObject(root, "remote_device", g_remote_device);
    cJSON_AddStringToObject(root, "remote_screen", g_remote_screen);
    cJSON_AddBoolToObject(root, "paid_content_notifications", g_paid_notifications);
    cJSON_AddBoolToObject(root, "dearrow_titles", g_dearrow_titles);
    cJSON_AddBoolToObject(root, "dearrow_thumbnails", g_dearrow_thumbnails);
    tube_sb_save(&g_sb, root);
    cJSON_AddBoolToObject(root, "autoplay", g_autoplay);
    cJSON_AddNumberToObject(root,"playback_mode",g_playback_mode);
    cJSON_AddBoolToObject(root,"queue_respects_playback_mode",g_queue_respects_mode);
    cJSON_AddBoolToObject(root, "ask_stream", g_ask_stream);
    cJSON_AddNumberToObject(root, "history_state", g_history_state);
    cJSON_AddBoolToObject(root, "history_sync", g_history_state != TUBE_HISTORY_DISABLED);
    for (int i = 0; i < g_count; ++i) {
        Record *r = &g_records[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id", r->item.id);
        cJSON_AddStringToObject(o, "profile", r->profile);
        cJSON_AddStringToObject(o, "title", r->item.title);
        cJSON_AddStringToObject(o, "dearrow_title", r->dearrow_title);
        cJSON_AddStringToObject(o, "subtitle", r->item.subtitle);
        cJSON_AddStringToObject(o, "overview", r->item.overview);
        cJSON_AddStringToObject(o, "art_url", r->item.art_url);
        cJSON_AddStringToObject(o, "channel_id", r->item.parent_id);
        cJSON_AddNumberToObject(o, "live_utc", (double)r->live_utc);
        cJSON_AddNumberToObject(o, "live_sequence", (double)r->live_sequence);
        cJSON_AddNumberToObject(o, "live_segment_utc", (double)r->live_segment_utc);
        cJSON_AddNumberToObject(o, "live_duration_us", (double)r->live_duration_us);
        cJSON_AddStringToObject(o, "live_origin", r->live_origin);
        cJSON_AddStringToObject(o, "live_format", r->live_format);
        cJSON_AddNumberToObject(o, "position", (double)r->item.resume_pos_sec);
        cJSON_AddNumberToObject(o, "duration", (double)r->item.duration_sec);
        cJSON_AddNumberToObject(o, "updated", (double)r->updated);
        cJSON_AddNumberToObject(o, "queue_order", (double)r->queue_order);
        cJSON_AddBoolToObject(o, "favorite", r->favorite);
        cJSON_AddBoolToObject(o, "queued", r->queued);
        cJSON_AddBoolToObject(o, "queued_music", r->queued && r->queued_music);
        cJSON_AddBoolToObject(o, "subscribed", r->subscribed);
        cJSON_AddBoolToObject(o, "played", r->item.played);
        cJSON_AddBoolToObject(o, "is_live", r->item.is_live);
        cJSON_AddItemToArray(rows, o);
    }
    cJSON_AddItemToObject(root, "records", rows);
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!body) { storage_failed(); return; }
    evo_mkdir(evo_data_dir());
    char tmp[512], dst[512];
    copy(tmp, sizeof tmp, evo_data_path("youtube.json.tmp"));
    copy(dst, sizeof dst, evo_data_path("youtube.json"));
    FILE *f = fopen(tmp, "wb");
    int ok = 0;
    if (f) {
        size_t size = strlen(body);
        ok = fwrite(body, 1, size, f) == size && fflush(f) == 0;
        if (fclose(f)) ok = 0;
        if (ok) ok = rename(tmp, dst) == 0;
    }
    if (ok) { g_last_save = now_ms(); g_remote_identity_saved = !!*g_remote_device; g_remote_write_failed = 0; g_channel_marks_dirty = 0; g_storage_notice = 0; }
    else storage_failed();
    free(body);
}
/* A change made in the player is saved with the selected profile's settings. */
static int extra_saved(const TubePlayerExtra *prefs, void *user) {
    (void)prefs; (void)user; save(); return g_store_writable && !g_remote_write_failed;
}
/* Loudness, Shorts flag, chapters and storyboards of the last resolve; the
 * player receives them when that video starts, not while another one plays. */
static cJSON *g_resolved_facts;

static void remote_configure(void) {
    /* A built-in service without the phone receiver (an older service bundle) is never asked for one. */
    const int offered = !embedded_lacks(COMPANION_REMOTE);
    if (offered && g_remote_enabled && !g_remote_entropy_failed && g_store_writable && *g_source && *g_token && !*g_remote_device) {
        unsigned char bytes[16];
        if (tube_random_bytes(bytes, sizeof bytes)) {
            g_remote_entropy_failed = 1;
            tube_set_error("Phone pairing could not obtain a PS5 random identity. Link a phone retries.");
            tube_remote_shutdown(); return;
        }
        bytes[6] = (bytes[6] & 15) | 64; bytes[8] = (bytes[8] & 63) | 128;
        snprintf(g_remote_device, sizeof g_remote_device,
            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],
            bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
        g_remote_identity_saved = 0; save();
    }
    tube_remote_configure(g_source, g_token, g_library_profile,
        offered && g_remote_enabled && !g_remote_entropy_failed && g_store_writable && g_remote_identity_saved && !g_remote_write_failed,
        g_remote_device, g_remote_screen);
}
unsigned tube_phone_revision(void) { return tube_remote_revision(); }
void tube_phone_sync_scope(void) { remote_configure(); }
int tube_voice_scope(char *source, size_t source_capacity, char *token,
                     size_t token_capacity, char *profile, size_t profile_capacity) {
    if (source && source_capacity) source[0] = 0;
    if (token && token_capacity) token[0] = 0;
    if (profile && profile_capacity) profile[0] = 0;
    if (!source || !token || !profile || g_embedded || !g_store_writable || !*g_source || !*g_token ||
        strlen(g_source) >= source_capacity || strlen(g_token) >= token_capacity ||
        strlen(g_library_profile) >= profile_capacity) return 0;
    copy(source, source_capacity, g_source); copy(token, token_capacity, g_token);
    copy(profile, profile_capacity, g_library_profile); return 1;
}
int tube_phone_stop_on_disconnect(void) { return g_remote_stop_on_disconnect; }
int tube_phone_close_on_disconnect(void) { return g_remote_close_on_disconnect; }

static void account_removed(void) {
    tube_recovery_finish();
    if (!strcmp(g_auth.profile, g_library_profile)) {
        unsigned serial = g_auth.serial + 1, revision = g_auth.revision + 1;
        memset(&g_auth, 0, sizeof g_auth);
        g_auth.serial = serial; g_auth.revision = revision; g_auth.state = AUTH_CANCELED;
    }
    lock_remove(g_library_profile); memset(&g_unlock, 0, sizeof g_unlock);
    profile_forget(g_library_profile);
    profile_settings_select("guest");
    watch_reset(1); tube_extra_video_end();
    g_next_id[0] = 0; g_next_profile[0] = 0;
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    memset(g_comments, 0, sizeof g_comments);
    g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors);
    save();
    ++g_accounts_revision;
}

/* ---- Row facts and the browsing profile (see tube_provider.h). ---- */
#define ROW_FACTS 1024
static struct { char row[EVO_PROVIDER_MAX_ITEM_ID], name[24], value[EVO_PROVIDER_MAX_ART_URL]; } g_row_facts[ROW_FACTS];
static int g_row_fact_used, g_row_fact_next;
static struct { char id[33], name[64]; } g_profile_name;
void tube_row_fact(const char *row, const char *name, const char *value) {
    int slot = -1;
    if (!row || !*row || !name || !*name || !value || !*value || strlen(name) >= sizeof g_row_facts[0].name) return;
    for (int i = 0; i < g_row_fact_used && slot < 0; ++i)
        if (!strcmp(g_row_facts[i].row, row) && !strcmp(g_row_facts[i].name, name)) slot = i;
    if (slot < 0) {
        slot = g_row_fact_used < ROW_FACTS ? g_row_fact_used++ : g_row_fact_next;
        g_row_fact_next = (slot + 1) % ROW_FACTS;
    }
    copy(g_row_facts[slot].row, sizeof g_row_facts[slot].row, row);
    copy(g_row_facts[slot].name, sizeof g_row_facts[slot].name, name);
    copy(g_row_facts[slot].value, sizeof g_row_facts[slot].value, value);
}
/* Read these live rather than caching facts from an earlier sign-in attempt. */
static int auth_row_fact(const char *row, int index, const char **name, const char **value) {
    const int auth_row = row && (!strcmp(row,"auth:info") || !strcmp(row,"auth:info:code") ||
        (g_auth.state == AUTH_AUTHORIZED && !strncmp(row,"library:profile:",16) && !strcmp(row+16,g_auth.profile)));
    if (!auth_row) return -1;
    if (index < 0 || index >= 7) return 0;
    static const char *const keys[]={"state","user_code","verification_url","expires_in","poll_after","error","name"};
    static const char *const states[]={"idle","starting","pending","authorized","canceled","expired","denied","failed"};
    static char expires[24], delay[24];
    const long long now=now_ms();
    snprintf(expires,sizeof expires,"%lld",g_auth.state==AUTH_PENDING && g_auth.deadline>now ? (g_auth.deadline-now+999)/1000 : 0);
    snprintf(delay,sizeof delay,"%lld",g_auth.state==AUTH_PENDING && g_auth.next_poll>now ? (g_auth.next_poll-now+999)/1000 : 0);
    const char *values[]={states[g_auth.state],g_auth.code,g_auth.url,expires,delay,
        g_auth.state==AUTH_FAILED ? g_auth.error : "",g_auth.state==AUTH_AUTHORIZED ? g_auth.name : ""};
    if(name)*name=keys[index];
    if(value)*value=values[index];
    return 1;
}
int tube_row_fact_at(const char *row, int index, const char **name, const char **value) {
    if (row && !strncmp(row, "library:profile:", 16)) {
        if (index < 0) return 0;
        if (index < 4) {
            static const char *const keys[] = {"guest", "signed_in", "selected", "locked"};
            const char *profile = row + 16;
            int configured = g_auth.state == AUTH_AUTHORIZED && !strcmp(profile, g_auth.profile);
            for (int i = 0; i < g_row_fact_used; ++i)
                if (!strcmp(g_row_facts[i].row, row) && !strcmp(g_row_facts[i].name, "account_configured"))
                    configured = !strcmp(g_row_facts[i].value, "1");
            const int flags[] = {!strcmp(profile, "guest"), configured,
                !strcmp(profile, g_library_profile), lock_find(profile) >= 0};
            if (name) *name = keys[index];
            if (value) *value = flags[index] ? "1" : "0";
            return 1;
        }
        index -= 4;
    }
    const int auth=auth_row_fact(row,index,name,value);
    if (auth == 1) return 1;
    if (auth == 0) index -= 7;
    for (int i = 0; row && *row && index >= 0 && i < g_row_fact_used; ++i)
        if (!strcmp(g_row_facts[i].row, row) && !index--) {
            if (name) *name = g_row_facts[i].name;
            if (value) *value = g_row_facts[i].value;
            return 1;
        }
    return 0;
}
const char *tube_row_fact_get(const char *row, const char *name) {
    const char *found = NULL, *value = "";
    for (int i = 0; name && tube_row_fact_at(row, i, &found, &value); ++i) if (!strcmp(found, name)) return value;
    return "";
}
const char *tube_browse_profile(void) { return g_library_profile; }
const char *tube_browse_profile_name(void) { return !strcmp(g_profile_name.id, g_library_profile) ? g_profile_name.name : ""; }
int tube_browse_profile_account(void) { return strcmp(g_library_profile, "guest") != 0; }
/* What a service row says beyond evo_provider_item_t: each value is optional. */
static void item_facts(const char *id, const cJSON *o) {
    char badges[96] = ""; const cJSON *badge;
    tube_row_fact(id, "views", str(o, "views"));
    tube_row_fact(id, "published", str(o, "published"));
    tube_row_fact(id, "channel_art_url", tube_art_url_allowed(str(o, "channel_art_url")) ? str(o, "channel_art_url") : "");
    cJSON_ArrayForEach(badge, cJSON_GetObjectItemCaseSensitive(o, "badges"))
        if (cJSON_IsString(badge) && strlen(badge->valuestring) < 12 && strlen(badges) + strlen(badge->valuestring) + 2 < sizeof badges) {
            if (*badges) strcat(badges, ",");
            strcat(badges, badge->valuestring);
        }
    tube_row_fact(id, "badges", badges);
    if (boolean(o, "is_upcoming")) tube_row_fact(id, "is_upcoming", "1");
    if (boolean(o, "is_short")) tube_row_fact(id, "is_short", "1");
}

static void parse_item(evo_provider_item_t *item, const cJSON *o) {
    memset(item, 0, sizeof *item);
    copy(item->id, sizeof item->id, str(o, "id"));
    copy(item->title, sizeof item->title, str(o, "title"));
    copy(item->subtitle, sizeof item->subtitle, str(o, "subtitle"));
    copy(item->overview, sizeof item->overview, str(o, "overview"));
    copy(item->art_url, sizeof item->art_url, str(o, "art_url"));
    copy(item->parent_id, sizeof item->parent_id, str(o, "channel_id"));
    item->duration_sec = (int64_t)num(o, "duration_sec");
    const cJSON *watched=cJSON_GetObjectItemCaseSensitive(o,"watch_percent");
    if(cJSON_IsNumber(watched) && isfinite(watched->valuedouble) && watched->valuedouble>=0 && watched->valuedouble<=100) {
        item->played=watched->valuedouble==100;
        if(watched->valuedouble>10 && watched->valuedouble<100 && item->duration_sec>0)
            item->resume_pos_sec=(int64_t)((double)item->duration_sec/100.0*watched->valuedouble);
    }
    item->is_live = boolean(o, "is_live");
    item->is_folder = boolean(o, "is_folder");
    item->kind = EVO_MEDIA_VIDEO;
    item_facts(item->id, o);
}

static int init_store(void) {
    memset(&g_resolve_failure,0,sizeof g_resolve_failure); tube_queue_retry_cancel(); g_startup_open = 1;
    tube_remote_shutdown();
    g_profile_settings_count = 0; memset(g_profile_settings,0,sizeof g_profile_settings);
    tube_player_extra_set_save_hook(NULL,NULL); tube_extra_video_end();
    cJSON_Delete(g_resolved_facts); g_resolved_facts = NULL;
    ProfileSettings defaults = profile_settings_defaults(); profile_settings_apply(&defaults);
    tube_player_extra_set_save_hook(extra_saved,NULL);
    g_separate_settings = 0; g_lock_count = 0; memset(&g_unlock,0,sizeof g_unlock); g_prompt_error[0] = 0;
    g_startup_lock[0] = 0; g_follow_count = g_channel_mark_count = g_channel_marks_dirty = g_recent_playlist_count = g_caption_memory_count = 0;
    g_backup_auto = TUBE_BACKUP_AUTO_OFF; g_backup_ticket = 0; memset(g_backup_notice,0,sizeof g_backup_notice);
    g_sb_server[0] = 0;
    import_release(); memset(&g_import,0,sizeof g_import); memset(&g_channel_new,0,sizeof g_channel_new);
    memset(g_picker_rows,0,sizeof g_picker_rows); memset(g_upcoming,0,sizeof g_upcoming);
    memset(&g_comment_thread,0,sizeof g_comment_thread); memset(&g_detail,0,sizeof g_detail);
    companion_forget(); memset(&g_channel_facts,0,sizeof g_channel_facts); memset(&g_channel_query,0,sizeof g_channel_query);
    memset(&g_history_paused,0,sizeof g_history_paused);
    copy(g_library_profile,sizeof g_library_profile,"guest");
    g_source[0] = g_token[0] = 0; g_count = 0; g_queue_order = 0;g_blocked_count=0;
    g_search_count = 0; memset(g_occurrences,0,sizeof g_occurrences); memset(&g_restart,0,sizeof g_restart);
    g_pin_count = 0; memset(&g_menu_target,0,sizeof g_menu_target); memset(&g_menu_launch,0,sizeof g_menu_launch);
    g_queue_move.rows[0][0] = g_queue_move.rows[1][0] = 0;
    memset(&g_search_options,0,sizeof g_search_options); memset(&g_last_search,0,sizeof g_last_search);
    memset(&g_suggestions,0,sizeof g_suggestions); memset(&g_search_feed,0,sizeof g_search_feed);
    memset(&g_channel_list,0,sizeof g_channel_list); memset(&g_description,0,sizeof g_description);
    memset(g_dismissed,0,sizeof g_dismissed); g_search_typed = 0;
    g_caption_path[0] = g_caption_language[0] = 0;
    g_secondary_caption_path[0] = g_secondary_caption_language[0] = 0;
    tube_caption_switch_cancel(); g_caption_track_count = 0; ++g_caption_revision;
    g_remote_enabled = g_remote_identity_saved = g_remote_write_failed = g_remote_entropy_failed = 0; g_remote_device[0] = g_remote_screen[0] = 0;
    g_remote_stop_on_disconnect = 0;
    g_remote_close_on_disconnect = 0;
    g_remote_no_history = g_select_account_on_boot = g_active_incognito = 0;
    tube_sb_defaults(&g_sb);
    sponsor_invalidate(); g_sb_last_position = -1;
    tube_chat_stop();
    tube_auth_cancel();
    unsigned auth_serial = g_auth.serial + 1, auth_revision = g_auth.revision + 1;
    memset(&g_auth, 0, sizeof g_auth);
    g_auth.serial = auth_serial ? auth_serial : 1; g_auth.revision = auth_revision;
    g_paid_notifications = 0; g_dearrow_titles = 0; g_dearrow_thumbnails = 0;
    g_speed = g_last_speed = 100; g_speed_mode = 1; g_speed_count = 0;
    g_speed_session_id[0] = g_speed_session_profile[0] = 0;
    g_browse_music = g_browse_queue = 0;
    g_store_writable = 0;
    g_sleep_minutes = g_sleep_armed = g_sleep_fired = 0;
    g_player_preferences_count = 0; memset(g_player_preferences,0,sizeof g_player_preferences);
    g_pixel_mode=0;atomic_store(&g_video_requested,0);atomic_store(&g_video_geometry,0);
    watch_reset(1);
    g_active_id[0] = 0; g_active_profile[0] = 0; g_next_id[0] = 0; g_next_profile[0] = 0;
    memset(&g_recovery, 0, sizeof g_recovery);
    memset(g_comments, 0, sizeof g_comments);
    memset(&g_account_intent, 0, sizeof g_account_intent);
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    FILE *f = fopen(evo_data_path("youtube.json"), "rb");
    if (!f) { if (errno == ENOENT) { g_store_writable = 1; remote_configure(); return 0; } return -1; }
    if (fseek(f, 0, SEEK_END) || ftell(f) < 0 || ftell(f) > 2 * 1024 * 1024) { fclose(f); return -1; }
    long n = ftell(f); rewind(f);
    char *body = calloc(1, (size_t)n + 1);
    if (!body) { fclose(f); return -1; }
    size_t read = fread(body, 1, (size_t)n, f); fclose(f);
    cJSON *root = read == (size_t)n ? cJSON_ParseWithLength(body, (size_t)n + 1) : NULL;
    free(body);
    double version = num(root, "version");
    if (!root || (version != 1 && version != 2) || !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(root, "records"))) {
        cJSON_Delete(root); return -1;
    }
    g_store_writable = 1;
    copy(g_source, sizeof g_source, str(root, "source"));
    copy(g_token, sizeof g_token, str(root, "token"));
    const char *profile = str(root, "library_profile");
    copy(g_library_profile, sizeof g_library_profile, valid_profile(profile) ? profile : "guest");
    /* A file from before the switch kept one set per profile and still does. */
    const cJSON *separate = cJSON_GetObjectItemCaseSensitive(root, "separate_settings");
    g_separate_settings = cJSON_IsBool(separate) ? cJSON_IsTrue(separate) : 1;
    double pixel=num(root,"pixel_ratio_mode");
    if(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(root,"pixel_ratio_mode")) && pixel>=0 && pixel<=5 && floor(pixel)==pixel)
        g_pixel_mode=(int)pixel;
    player_preferences_load(root);
    /* Legacy max_height preferences do not restrict new video selections. */
    g_max_fps = settings_integer(root,"max_fps",0,60,0);
    double speed = num(root, "speed_percent");
    g_speed = valid_speed(speed) ? (int)speed : 100;
    speed_load(root);
    g_codec = settings_integer(root,"codec",0,4,0);
    g_captions = boolean(root, "captions");
    g_auto_captions = boolean(root, "automatic_captions");
    g_sponsor = boolean(root, "sponsorblock");
    double chat_mode = num(root, "player_chat_mode");
    g_player_chat_mode = chat_mode >= 0 && chat_mode <= 2 && floor(chat_mode) == chat_mode ? (int)chat_mode : 0;
    double sleep_minutes = num(root, "sleep_timer_minutes");
    g_sleep_minutes = sleep_minutes >= 0 && sleep_minutes <= 600 && floor(sleep_minutes) == sleep_minutes &&
        (int)sleep_minutes % 30 == 0 ? (int)sleep_minutes : 0;
    g_sleep_armed = g_sleep_fired = 0;
    const char *remote_device = str(root, "remote_device"), *remote_screen = str(root, "remote_screen");
    g_remote_stop_on_disconnect = boolean(root, "remote_stop_on_disconnect");
    g_remote_close_on_disconnect = boolean(root, "remote_close_on_disconnect");
    if (g_remote_close_on_disconnect) g_remote_stop_on_disconnect = 0;
    g_remote_no_history = boolean(root, "remote_disable_history");
    g_select_account_on_boot = boolean(root, "select_account_on_boot");
    if (tube_remote_identity_valid(remote_device) && strlen(remote_screen) < sizeof g_remote_screen &&
        strspn(remote_screen, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(remote_screen)) {
        copy(g_remote_device, sizeof g_remote_device, remote_device);
        copy(g_remote_screen, sizeof g_remote_screen, remote_screen);
        g_remote_identity_saved = 1; g_remote_enabled = boolean(root, "remote_enabled");
    }
    g_paid_notifications = boolean(root, "paid_content_notifications");
    g_dearrow_titles = boolean(root, "dearrow_titles");
    g_dearrow_thumbnails = boolean(root, "dearrow_thumbnails");
    tube_sb_load(&g_sb, root);
    g_autoplay = boolean(root, "autoplay");
    g_playback_mode=settings_integer(root,"playback_mode",0,6,g_autoplay?TUBE_MODE_ALL:TUBE_MODE_PAUSE);
    if (!playback_mode_valid(g_playback_mode)) g_playback_mode=g_autoplay?TUBE_MODE_ALL:TUBE_MODE_PAUSE;
    g_autoplay=playback_mode_continues(g_playback_mode);
    g_queue_respects_mode=boolean(root,"queue_respects_playback_mode");
    g_ask_stream = boolean(root, "ask_stream");
    g_history_state = history_state_read(root);
    const char *language = str(root, "language"), *audio = str(root, "audio_language");
    const char *valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-";
    if (*language && strlen(language) < sizeof g_language && strspn(language, valid) == strlen(language))
        copy(g_language, sizeof g_language, language);
    if (strlen(audio) < sizeof g_audio_language && strspn(audio, valid) == strlen(audio))
        copy(g_audio_language, sizeof g_audio_language, audio);
    g_count = 0; g_queue_order = 0;
    cJSON *o;
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"blocked_channels")) {
        const char *owner=str(o,"profile"),*channel=str(o,"channel");char catalog[40];
        snprintf(catalog,sizeof catalog,"channel:%s",channel);
        const char *name=str(o,"name");
        if(g_blocked_count>=128 || !valid_profile(owner) ||
           (*channel ? strlen(channel)!=24 || !valid_catalog_id(catalog) : !tube_channel_name_equal(name,name)))continue;
        int duplicate=0;for(int i=0;i<g_blocked_count;++i)
            if(!strcmp(g_blocked[i].profile,owner) && ((*channel && !strcmp(g_blocked[i].channel,channel)) || tube_channel_name_equal(g_blocked[i].name,name)))duplicate=1;
        if(duplicate)continue;
        copy(g_blocked[g_blocked_count].profile,sizeof g_blocked[0].profile,owner);
        copy(g_blocked[g_blocked_count].channel,sizeof g_blocked[0].channel,channel);
        copy(g_blocked[g_blocked_count].name,sizeof g_blocked[0].name,*str(o,"name")?str(o,"name"):channel);++g_blocked_count;
    }
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"search_history"))
        search_history_put(str(o,"profile"),str(o,"query"),0);
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"pinned_sections"))
        pin_put(str(o,"profile"),str(o,"id"),str(o,"title"));
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"profile_locks")) {
        const char *owner=str(o,"profile"),*record=str(o,"password"); /* first valid record of a profile wins */
        if(g_lock_count==PROFILE_LOCKS || !valid_profile(owner) || !tube_password_record_valid(record) || lock_find(owner)>=0)continue;
        copy(g_locks[g_lock_count].profile,sizeof g_locks[0].profile,owner);
        copy(g_locks[g_lock_count++].record,sizeof g_locks[0].record,record);
    }
    /* Lists of the third batch: absent in older files; a malformed row is skipped. */
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"followed_channels"))
        follow_put(str(o,"profile"),str(o,"channel"),str(o,"title"));
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"seen_channels")) {
        const char *owner=str(o,"profile"),*channel=str(o,"channel"),*video=str(o,"video");int duplicate=0;
        for(int i=0;i<g_channel_mark_count;++i)
            if(!strcmp(g_channel_marks[i].profile,owner) && !strcmp(g_channel_marks[i].channel,channel))duplicate=1;
        if(duplicate || g_channel_mark_count==CHANNEL_MARKS || !valid_profile(owner) || !strcmp(owner,"guest") || !valid_channel(channel))continue;
        memset(&g_channel_marks[g_channel_mark_count],0,sizeof g_channel_marks[0]);
        copy(g_channel_marks[g_channel_mark_count].profile,sizeof g_channel_marks[0].profile,owner);
        copy(g_channel_marks[g_channel_mark_count].channel,sizeof g_channel_marks[0].channel,channel);
        if(valid_id(video))copy(g_channel_marks[g_channel_mark_count].video,sizeof g_channel_marks[0].video,video);
        g_channel_marks[g_channel_mark_count++].seen=(long long)num(o,"seen");
    }
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"recent_playlists")) {
        const char *owner=str(o,"profile"),*playlist=str(o,"playlist");int duplicate=0;
        for(int i=0;i<g_recent_playlist_count;++i)if(!strcmp(g_recent_playlists[i].profile,owner))duplicate=1;
        if(duplicate || g_recent_playlist_count==RECENT_PLAYLISTS || !valid_profile(owner) || !strcmp(owner,"guest") || !valid_playlist(playlist))continue;
        memset(&g_recent_playlists[g_recent_playlist_count],0,sizeof g_recent_playlists[0]);
        copy(g_recent_playlists[g_recent_playlist_count].profile,sizeof g_recent_playlists[0].profile,owner);
        copy(g_recent_playlists[g_recent_playlist_count].playlist,sizeof g_recent_playlists[0].playlist,playlist);
        copy(g_recent_playlists[g_recent_playlist_count++].title,sizeof g_recent_playlists[0].title,*str(o,"title")?str(o,"title"):playlist);
    }
    cJSON_ArrayForEach(o,cJSON_GetObjectItemCaseSensitive(root,"caption_memories")) {
        const char *owner=str(o,"profile"),*channel=str(o,"channel");int duplicate=0;
        const cJSON *enabled=cJSON_GetObjectItemCaseSensitive(o,"enabled");
        for(int i=0;i<g_caption_memory_count;++i)
            if(!strcmp(g_caption_memories[i].profile,owner) && !strcmp(g_caption_memories[i].channel,channel))duplicate=1;
        if(duplicate || g_caption_memory_count==CAPTION_MEMORIES || !valid_profile(owner) || !tube_sb_channel_valid(channel) || !cJSON_IsBool(enabled))continue;
        copy(g_caption_memories[g_caption_memory_count].profile,sizeof g_caption_memories[0].profile,owner);
        copy(g_caption_memories[g_caption_memory_count].channel,sizeof g_caption_memories[0].channel,channel);
        g_caption_memories[g_caption_memory_count++].enabled=cJSON_IsTrue(enabled);
    }
    {
        const cJSON *period=cJSON_GetObjectItemCaseSensitive(root,"backup_auto_days");
        if(cJSON_IsNumber(period) && period->valuedouble==(int)period->valuedouble && tube_backup_auto_valid((int)period->valuedouble))
            g_backup_auto=(int)period->valuedouble;
        char origin[sizeof g_sb_server]; /* kept only in the form this version writes */
        if(sponsor_origin(str(root,"sponsorblock_server"),origin,sizeof origin) && !strcmp(origin,str(root,"sponsorblock_server")))
            copy(g_sb_server,sizeof g_sb_server,origin);
    }
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(root, "records")) {
        const char *owner = version == 1 ? "guest" : str(o, "profile");
        if (record_for(str(o, "id"), owner, 0)) continue;
        Record *r = record_for(str(o, "id"), owner, 1); if (!r) continue;
        parse_item(&r->item, o);
        copy(r->dearrow_title, sizeof r->dearrow_title, str(o, "dearrow_title"));
        r->dearrow_art[0] = 0; /* Expiring thumbnail capabilities are never persisted. */
        const cJSON *live_value = cJSON_GetObjectItemCaseSensitive(o,"live_utc");
        double live_utc = cJSON_IsNumber(live_value) ? live_value->valuedouble : 0;
        if (isfinite(live_utc) && live_utc >= 946684800000000.0 && live_utc <= 4102444800000000.0 && floor(live_utc) == live_utc &&
            strlen(str(o,"live_origin")) < sizeof r->live_origin && strlen(str(o,"live_format")) < sizeof r->live_format) {
            const cJSON *seq=cJSON_GetObjectItemCaseSensitive(o,"live_sequence"), *segment=cJSON_GetObjectItemCaseSensitive(o,"live_segment_utc"), *duration=cJSON_GetObjectItemCaseSensitive(o,"live_duration_us");
            if (cJSON_IsNumber(seq) && isfinite(seq->valuedouble) && seq->valuedouble>=0 && seq->valuedouble<=9007199254740991.0 && floor(seq->valuedouble)==seq->valuedouble &&
                cJSON_IsNumber(segment) && isfinite(segment->valuedouble) && segment->valuedouble>=946684800000000.0 && segment->valuedouble<=live_utc && floor(segment->valuedouble)==segment->valuedouble &&
                cJSON_IsNumber(duration) && isfinite(duration->valuedouble) && duration->valuedouble>0 && duration->valuedouble<=60000000 && floor(duration->valuedouble)==duration->valuedouble && live_utc-segment->valuedouble<duration->valuedouble) {
                r->live_utc=(int64_t)live_utc; r->live_sequence=(int64_t)seq->valuedouble;
                r->live_segment_utc=(int64_t)segment->valuedouble; r->live_duration_us=(int64_t)duration->valuedouble;
            }
            copy(r->live_origin,sizeof r->live_origin,str(o,"live_origin"));
            copy(r->live_format,sizeof r->live_format,str(o,"live_format"));
        }
        r->item.resume_pos_sec = (int64_t)num(o, "position");
        r->item.duration_sec = (int64_t)num(o, "duration");
        r->item.played = boolean(o, "played");
        r->updated = (long long)num(o, "updated");
        r->queue_order = (long long)num(o, "queue_order");
        if (r->queue_order > g_queue_order) g_queue_order = r->queue_order;
        r->favorite = boolean(o, "favorite"); r->queued = boolean(o, "queued");
        r->queued_music = r->queued && boolean(o, "queued_music");
        r->subscribed = boolean(o, "subscribed");
    }
    profile_settings_load(root);
    cJSON_Delete(root); if (version == 1) save();
    /* A protected profile is not entered again without its password: browsing
     * starts as Guest and the startup folder asks for it. */
    if (strcmp(g_library_profile, "guest") && lock_find(g_library_profile) >= 0) {
        char locked[33]; copy(locked, sizeof locked, g_library_profile);
        if (profile_settings_select("guest")) copy(g_startup_lock, sizeof g_startup_lock, locked);
    }
    if (g_backup_auto != TUBE_BACKUP_AUTO_OFF) /* a dated copy of the file just read, when one is due */
        tube_backup_auto_run(evo_data_dir(), g_backup_auto, (long long)time(NULL), TUBE_BACKUP_KEEP_DEFAULT, NULL, 0);
    remote_configure(); return 0;
}

static int init(void) {
    tube_ui_defaults(&g_ui); ++g_ui_revision; tube_ui_press_reset();
    embedded_select();
    int rc = init_store();
    embedded_adopt();
    if (tube_update_app_init) tube_update_app_init();   /* first: an interrupted update is settled before anything else */
    return rc;
}

/* Local library and preferences remain usable before pairing or while offline. */
static int configured(void) { return 1; }
static const char *get_source(void) { return g_source; }

static int set_source(const char *value) {
    if (!value || strlen(value) >= sizeof g_source) return -1;
    if (g_embedded) return -1;   /* the built-in service has no address to set */
    char base[512]; copy(base, sizeof base, value);
    char *hash = strchr(base, '#');
    char pin[7] = "";
    if (hash) {
        *hash++ = 0;
        if (strlen(hash) != 6) return -1;
        for (int i = 0; i < 6; ++i) if (!isdigit((unsigned char)hash[i])) return -1;
        copy(pin, sizeof pin, hash);
    }
    char host[256]; int port, tls;
    if (evo_provider_parse_web_source(base, host, sizeof host, &port, &tls, 8765)) return -1;
    if (strchr(base, '@') || strchr(base, '?') || strchr(base, '\r') || strchr(base, '\n')) return -1;
    char normalized[512];
    snprintf(normalized, sizeof normalized, "%s://%s:%d", tls ? "https" : "http", host, port);
    tube_cancel();
    watch_reset(1);
    sponsor_invalidate();
    for (int i = 0; i < g_count; ++i) { g_records[i].dearrow_title[0] = 0; g_records[i].dearrow_art[0] = 0; }
    unsigned auth_serial = g_auth.serial + 1, auth_revision = g_auth.revision + 1;
    memset(&g_auth, 0, sizeof g_auth);
    g_auth.serial = auth_serial ? auth_serial : 1; g_auth.revision = auth_revision;
    memset(&g_account_intent, 0, sizeof g_account_intent);
    memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    memset(g_comments, 0, sizeof g_comments);
    g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors);
    if (strcmp(normalized, g_source)) { profile_settings_select("guest"); search_options_reset(); }
    if (strcmp(normalized, g_source) || *pin) {
        for (int i = 0; i < g_count; ++i) g_records[i].live_utc = 0;
        g_token[0] = 0; companion_forget();
    }
    memset(&g_unlock, 0, sizeof g_unlock); tube_extra_video_end();
    copy(g_source, sizeof g_source, normalized); copy(g_pin, sizeof g_pin, pin);
    tube_queue_retry_cancel();
    ++g_generation; save(); return 0;
}

static void complete(Request *r, int ok) {
    if(r->kind==14 && r->intent==g_phone_next.serial && !ok)g_phone_next.state=-1;
    if(r->kind==13 && r->intent==g_recommendation.serial && !ok) g_recommendation.state=-1;
    if (r->kind == 12 && r->intent == g_playlist_window.serial && !ok)
        g_playlist_window.state = -1;
    if (r->kind == 11 && r->intent == g_message.ticket && g_message.phase == 3) g_message.phase = -1;
    if (r->kind == 10 && !ok) {
        chat_cleanup_request(r, NULL);
        if (r->intent == g_chat.serial) {
            chat_close_remote(g_chat.source, g_chat.token, g_chat.profile, g_chat.cursor);
            g_chat.cursor[0] = 0; g_chat.state = -1;
        }
    }
    if (r->kind == 9 && r->intent == g_auth.serial && !ok &&
        (g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING)) {
        auth_cleanup_request(r, NULL);
        if (!*g_auth.error) copy(g_auth.error,sizeof g_auth.error,"YouTube sign-in could not be completed. Start a new session.");
        tube_auth_cancel(); g_auth.state = AUTH_FAILED; ++g_auth.revision;
    }
    if (r->kind == 8 && r->intent == g_watch.serial && !ok) {
        g_watch.failed = 1; g_watch.dirty = 0; g_watch.status = -1;
    }
    if (r->kind == 6 && r->intent == g_account_intent.serial && g_account_intent.state == 1)
        g_account_intent.state = -1;
    if (r->kind == 18) import_release();
    evo_provider_items_cb items_cb = r->items_cb;
    evo_provider_resolve_cb resolve_cb = r->resolve_cb;
    void *ud = r->ud;
    if (resolve_cb) resolve_cb(ok, ok ? r->choices : NULL, ok ? r->choice_count : 0, ud);
    if (items_cb) items_cb(0, NULL, 0, 0, ud);
    memset(r, 0, sizeof *r);
}

static Request *request_new(int kind, void *ud) {
    for (int i = 0; i < REQUESTS; ++i) if (!g_requests[i].used) {
        Request *r = &g_requests[i]; memset(r, 0, sizeof *r);
        r->used = 1; r->kind = kind; r->ud = ud; r->generation = g_generation;
        r->deadline = now_ms() + 65000; return r;
    }
    tube_set_error("Too many requests are pending. Cancel the current request and retry.");
    return NULL;
}

static int companion_url(const char *url) {
    size_t n = strlen(g_source);
    return n && !strncmp(url, g_source, n) && url[n] == '/' && strlen(url) < EVO_PROVIDER_MAX_URL;
}

/* Artwork the app may fetch. An external companion relays every image, and
 * what it names is taken as before. The built-in service names YouTube's
 * image hosts directly: then only its own loopback address and exactly these
 * hosts over https, default port, pass. */
int tube_art_url_allowed(const char *url) {
    static const char *const hosts[] = {"i.ytimg.com", "i9.ytimg.com", "yt3.ggpht.com", "yt3.googleusercontent.com"};
    if (!url || !*url) return 0;
    if (!g_embedded) return 1;
    if (companion_url(url)) return 1;
    if (strncmp(url, "https://", 8) || strlen(url) >= EVO_PROVIDER_MAX_ART_URL) return 0;
    const char *host = url + 8;
    size_t n = strcspn(host, "/:?#@\\");
    if (host[n] != '/') return 0;   /* no port, no user part, and a path */
    for (const char *c = host + n; *c; ++c) if ((unsigned char)*c <= 32 || (unsigned char)*c >= 127) return 0;
    for (size_t i = 0; i < sizeof hosts / sizeof *hosts; ++i)
        if (strlen(hosts[i]) == n && !strncmp(host, hosts[i], n)) return 1;
    return 0;
}

static void dearrow_item(evo_provider_item_t *item, const Record *cached) {
    if (!cached || item->is_folder) return;
    if (g_dearrow_titles && *cached->dearrow_title) copy(item->title, sizeof item->title, cached->dearrow_title);
    if (g_dearrow_thumbnails && companion_url(cached->dearrow_art)) copy(item->art_url, sizeof item->art_url, cached->dearrow_art);
}

static void response(int ok, int status, const char *body, size_t size, void *ud);
static void folder(evo_provider_item_t *out, const char *id, const char *title, const char *hint);
static int library_job(Request *r, cJSON *operation);
/* "channel:UC..." or "channel-tab:UC...:section[:order]" -> the channel identity ("" = neither). */
static const char *channel_of(const char *parent) {
    static char channel[25];
    channel[0] = 0;
    if (valid_channel_group(parent)) copy(channel, sizeof channel, parent + 8);
    else if (parent && !strncmp(parent, "channel-tab:", 12) && strlen(parent) > 37 && parent[36] == ':') {
        char group[33]; snprintf(group, sizeof group, "channel:%.24s", parent + 12);
        if (valid_channel_group(group)) copy(channel, sizeof channel, group + 8);
    }
    return channel;
}
static int channel_name_index(const char *const *names, int count, const char *name) {
    for (int i = 0; i < count; ++i) if (!strcmp(names[i], name)) return i;
    return -1;
}
/* The first page of a channel section names the sections, orders and search
 * the channel offers. 1 = facts are known for this request's channel. */
static int channel_note(const Request *r, const cJSON *facts) {
    char channel[25]; copy(channel, sizeof channel, channel_of(r->parent));
    if (*channel && !r->page && cJSON_IsObject(facts) && !strcmp(str(facts, "id"), channel)) {
        const cJSON *name;
        memset(&g_channel_facts, 0, sizeof g_channel_facts);
        copy(g_channel_facts.channel, sizeof g_channel_facts.channel, channel);
        copy(g_channel_facts.profile, sizeof g_channel_facts.profile, g_library_profile); g_channel_facts.scope = g_playlist_scope;
        cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(facts, "tabs")) {
            int tab = cJSON_IsString(name) ? channel_name_index(g_channel_tabs, 5, name->valuestring) : -1;
            if (tab >= 0) g_channel_facts.tabs |= 1u << tab;
        }
        cJSON_ArrayForEach(name, cJSON_GetObjectItemCaseSensitive(facts, "sorts")) {
            int sort = cJSON_IsString(name) ? channel_name_index(g_channel_sorts, 3, name->valuestring) : -1;
            if (sort >= 0) g_channel_facts.sorts |= 1u << sort;
        }
        g_channel_facts.search = boolean(facts, "search");
        g_channel_facts.tab = channel_name_index(g_channel_tabs, 5, str(facts, "tab"));
        g_channel_facts.sort = channel_name_index(g_channel_sorts, 3, str(facts, "sort"));
    }
    return *channel && channel_facts_current(channel);
}

static int send_request(Request *r, const char *method, const char *path, const char *payload) {
    char url[2048], auth[160]; const char *headers[2] = {"Content-Type: application/json", auth};
    snprintf(url, sizeof url, "%s%s", g_source, path);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
    int rc = evo_net_request_async(method, url, payload, headers, *g_token ? 2 : 1, response, r);
    r->inflight = rc == 0;
    if (rc) { tube_set_error(g_embedded ? "Could not queue the request. Retry." : "Could not queue the companion request. Retry."); complete(r, 0); }
    return rc;
}

/* Requests by kind: 1 = search or catalog job, 2 = resolve, 3 = library list,
 * 5 = comments, 7 = playlist picker, 15 = description, 16 = suggestions,
 * 17 = one answer shown as rows (details, reminder state, playlist orders,
 * first search result, a channel marked as watched), 18 = a subscriptions import. */
static int request_wants_features(const Request *r) {
    const int listing = r->kind == 1 || r->kind == 3 || r->kind == 7;
    if (r->wants_health) return 1;
    if (r->kind == 2) return *g_hl || g_sb_alt || g_more_subtitles;
    if (listing) return *g_hl || *g_gl || g_thumbnails || (g_hide_set && r->kind != 7);
    if (r->kind == 15 && g_dislikes) return 1;
    return (r->kind == 5 || r->kind == 15 || r->kind == 16) && (*g_hl || *g_gl);
}
/* A field is replaced, never repeated: a request can begin again after pairing. */
static void job_string(cJSON *object, const char *key, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, key); cJSON_AddStringToObject(object, key, value);
}
/* Adds what the selected profile chose and the companion advertises. 0 = ready to send. */
static int request_decorate(Request *r) {
    const int listing = r->kind == 1 || r->kind == 3 || r->kind == 7;
    const int dearrow = (r->kind == 1 || r->kind == 3) && (g_dearrow_titles || g_dearrow_thumbnails);
    const int channel = r->kind == 1 && r->channel_upgrade && companion_has(COMPANION_CHANNELS);
    const int locale = companion_has(COMPANION_LOCALE) && (*g_hl || *g_gl) &&
        (listing || r->kind == 2 || r->kind == 5 || r->kind == 15 || r->kind == 16);
    const int thumbnails = companion_has(COMPANION_THUMBNAILS) && g_thumbnails && listing;
    const int hide = companion_has(COMPANION_FILTERS) && g_hide_set && (r->kind == 1 || r->kind == 3);
    const int translated = r->kind == 2 && g_more_subtitles && companion_has(COMPANION_TRANSLATED);
    const int marks = r->kind == 3 && r->new_content && companion_has(COMPANION_NEW_CONTENT);
    if (!dearrow && !channel && !locale && !thumbnails && !hide && !translated && !marks) return 0;
    cJSON *job = cJSON_Parse(r->payload);
    if (!job) { complete(r, 0); return -1; }
    if (channel) { /* the channel folder becomes the companion's channel page, opened on Videos */
        cJSON *page = cJSON_CreateObject();
        cJSON_AddStringToObject(page, "action", "channel"); cJSON_AddStringToObject(page, "profile", g_library_profile);
        cJSON_AddStringToObject(page, "channel", r->parent + 8);
        cJSON_Delete(job); job = cJSON_CreateObject();
        cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", page);
        r->kind = 3; r->channel_upgrade = 0; r->channel_page = 1;
        copy(g_library_parent, sizeof g_library_parent, r->parent); memset(g_library_cursors, 0, sizeof g_library_cursors);
    }
    cJSON *target = !strcmp(str(job, "action"), "library") ? cJSON_GetObjectItemCaseSensitive(job, "library") :
        r->kind == 2 ? cJSON_GetObjectItemCaseSensitive(job, "playback") : job;
    if (dearrow) {
        cJSON_DeleteItemFromObjectCaseSensitive(job, "dearrow_titles"); cJSON_DeleteItemFromObjectCaseSensitive(job, "dearrow_thumbnails");
        cJSON_AddBoolToObject(job, "dearrow_titles", g_dearrow_titles);
        cJSON_AddBoolToObject(job, "dearrow_thumbnails", g_dearrow_thumbnails);
    }
    if (locale && *g_hl) job_string(target, "hl", g_hl);
    if (locale && *g_gl && r->kind != 2) job_string(target, "gl", g_gl); /* a resolve takes the language only */
    if (thumbnails) job_string(target, "thumbnails", g_thumbnail_sources[g_thumbnails]);
    if (translated) { /* machine translations join the caption list of this resolve */
        cJSON_DeleteItemFromObjectCaseSensitive(target, "translated_captions"); cJSON_AddBoolToObject(target, "translated_captions", 1);
    }
    if (marks && !strcmp(str(target, "action"), "channels")) { /* a cursor continues the list it was opened with */
        cJSON_DeleteItemFromObjectCaseSensitive(target, "new_content"); cJSON_AddBoolToObject(target, "new_content", 1);
    }
    if (hide && strcmp(str(target, "action"), "continue")) { /* a cursor keeps the selection it was opened with */
        cJSON_DeleteItemFromObjectCaseSensitive(target, "hide");
        cJSON *names = cJSON_AddArrayToObject(target, "hide");
        for (int i = 0; names && i < HIDE_FLAGS; ++i)
            if (g_hide >> i & 1) cJSON_AddItemToArray(names, cJSON_CreateString(g_hide_flags[i].name));
    }
    char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
    if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
    copy(r->payload, sizeof r->payload, body); free(body);
    return 0;
}

static int begin(Request *r) {
    if (g_embedded && !embedded_ready()) {   /* never a pairing step: the service is started, or the request fails */
        if (!g_embedded_stopped) {
            char message[240];
            snprintf(message, sizeof message, "The built-in YouTube service could not start: %s. Select the item again to retry.", g_embedded_error);
            auth_request_error(r, message);
        }
        complete(r, 0); return 0;
    }
    if (!*g_source) { auth_request_error(r, "Set the companion LAN address in Options first."); complete(r, 0); return 0; }
    if (!*g_token) {
        if (!*g_pin) { auth_request_error(r, "Pair first: enter the companion address followed by # and its six-digit code in Options."); complete(r, 0); return 0; }
        char payload[64]; snprintf(payload, sizeof payload, "{\"code\":\"%s\"}", g_pin);
        r->stage = 0; return send_request(r, "POST", "/v1/pair", payload);
    }
    if (r->kind == 9 || r->kind == 10) {
        copy(r->auth_source, sizeof r->auth_source, g_source);
        copy(r->auth_token, sizeof r->auth_token, g_token);
    }
    if (r->kind == 10) { r->stage = 4; return send_request(r, "POST", "/v1/library", r->payload); }
    /* One read of the feature list per source, and only for a request that
     * would use it; every profile at its defaults never asks. */
    /* The built-in service is always asked first: what it lists is what is offered. */
    if (!r->health_tried && !companion_known() && (g_embedded || request_wants_features(r))) {
        r->health_tried = 1; r->stage = 5; return send_request(r, "GET", "/v1/health", NULL);
    }
    if (r->requires && !companion_has(r->requires)) {
        auth_request_error(r, g_embedded ? "This action is not part of the built-in YouTube service yet." : companion_known() ? "The paired companion does not offer this action. Update it and enable its library worker." :
            "The companion did not report what it offers. Check that it is running and retry.");
        complete(r, 0); return 0;
    }
    if (r->kind == 18) { /* the export file itself is the request body */
        if (!g_import.body) { complete(r, 0); return 0; }
        r->stage = 7; return send_request(r, "POST", "/v1/subscriptions/import", g_import.body);
    }
    if (request_decorate(r)) return -1;
    r->stage = 1; return send_request(r, "POST", "/v1/jobs", r->payload);
}

static int recommendation_current(void) {
    return recovery_scope() && g_recommendation.scope==g_playlist_scope &&
        g_recommendation.selection==g_selection_serial && !strcmp(g_recommendation.id,g_active_id);
}
static int recommendation_begin(int same_playlist) {
    if(!*g_token || !recovery_scope() || !valid_id(g_active_id)) return 0;
    if(g_manual_direction)g_manual_fallback=1;
    Request *r=request_new(13,NULL);if(!r)return 0;
    if(!++g_recommendation.serial)++g_recommendation.serial;
    g_recommendation.scope=g_playlist_scope;g_recommendation.selection=g_selection_serial;
    copy(g_recommendation.id,sizeof g_recommendation.id,g_active_id);g_recommendation.next[0]=0;
    g_recommendation.channel[0]=g_recommendation.channel_name[0]=0;
    g_recommendation.state=0;r->intent=g_recommendation.serial;
    g_recommendation.same_playlist=same_playlist;memset(&g_recommendation.next_context,0,sizeof g_recommendation.next_context);
    char playlist[101];int index;
    if(tube_playback_playlist_context(playlist,sizeof playlist,&index) && valid_playlist(playlist)) snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"playback_next\",\"profile\":\"%s\",\"id\":\"%s\",\"playlist\":\"%s\",\"index\":%d,\"require_channel\":%s,\"require_name\":%s}}",g_active_profile,g_active_id,playlist,index,channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    else snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"playback_next\",\"profile\":\"%s\",\"id\":\"%s\",\"require_channel\":%s,\"require_name\":%s}}",g_active_profile,g_active_id,channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    begin(r);return 1;
}
static void recommendation_result(Request *r,const cJSON *root) {
    const cJSON *next=cJSON_GetObjectItemCaseSensitive(root,"next_video_id");
    evo_provider_item_t item={0};copy(item.id,sizeof item.id,str(root,"next_video_id"));
    parse_playlist_context(&item,root,"");
    int distinct_context=item.playback_scope && (strcmp(item.playback_playlist,g_active_playlist.playlist) || item.playback_index!=g_active_playlist.index);
    if(num(root,"api_version")!=1 || strcmp(str(root,"video_id"),g_recommendation.id) ||
        (!cJSON_IsNull(next) && (!cJSON_IsString(next) || !valid_id(next->valuestring) ||
            (!strcmp(next->valuestring,g_recommendation.id) && !distinct_context))) ||
        (cJSON_GetObjectItemCaseSensitive(root,"playback_context") && !item.playback_scope)) {complete(r,0);return;}
    g_recommendation.state=cJSON_IsNull(next)?-1:1;
    const cJSON *channel=cJSON_GetObjectItemCaseSensitive(root,"next_channel_id");
    char channel_group[40];snprintf(channel_group,sizeof channel_group,"channel:%s",str(root,"next_channel_id"));
    if(channel && (!cJSON_IsString(channel) || !valid_channel_group(channel_group))) {complete(r,0);return;}
    copy(g_recommendation.channel,sizeof g_recommendation.channel,str(root,"next_channel_id"));
    const char *name=str(root,"next_channel_name");
    if((*name && (strlen(name)>255 || !tube_channel_name_equal(name,name))) || (channel_name_blocks_active() && !*name)) {complete(r,0);return;}
    copy(g_recommendation.channel_name,sizeof g_recommendation.channel_name,name);
    if(channel_blocks_active() && (!*g_recommendation.channel || blocked_identity(g_recommendation.channel,name)>=0))g_recommendation.state=-1;
    if(g_recommendation.same_playlist && (!item.playback_scope || strcmp(item.playback_playlist,g_active_playlist.playlist)))g_recommendation.state=-1;
    if(g_recommendation.state==1)copy(g_recommendation.next,sizeof g_recommendation.next,next->valuestring);
    copy(g_recommendation.next_context.playlist,sizeof g_recommendation.next_context.playlist,item.playback_playlist);
    g_recommendation.next_context.index=item.playback_index;g_recommendation.next_context.scope=item.playback_scope;
    complete(r,1);
}
static int phone_next_current(void) {
    return recovery_scope() && tube_remote_owner_current(g_phone_next.owner) &&
        g_phone_next.selection==g_selection_serial && g_phone_next.scope==g_playlist_scope &&
        g_phone_next.blocks==g_blocked_revision && !strcmp(g_phone_next.id,g_active_id);
}
void tube_phone_recommendation_cancel(void) {
    unsigned serial=g_phone_next.serial+1;
    memset(&g_phone_next,0,sizeof g_phone_next);
    g_phone_next.serial=serial?serial:1;g_phone_next.state=-1;
}
static int phone_next_integer(const cJSON *root,const char *key,int max) {
    const cJSON *value=cJSON_GetObjectItemCaseSensitive(root,key);
    return cJSON_IsNumber(value) && value->valuedouble>=0 && value->valuedouble<=max &&
        value->valuedouble==value->valueint?value->valueint:-1;
}
/* Validate identities even when no block is active; never trust truncated labels. */
static int phone_next_identity(const char *channel,const char *name) {
    char parent[40];snprintf(parent,sizeof parent,"channel:%s",channel);
    return (!*channel || (strlen(channel)==24 && valid_channel_group(parent))) &&
        strlen(name)<=255 && (!*name || tube_channel_name_equal(name,name)) &&
        (!channel_blocks_active() || *channel) && (!channel_name_blocks_active() || *name);
}
static int phone_next_request(void) {
    Request *r=request_new(14,NULL);if(!r){g_phone_next.state=-1;return 0;}
    r->intent=g_phone_next.serial;
    if(*g_phone_next.group)snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"group_window\",\"profile\":\"%s\",\"id\":\"%s\",\"group\":\"%s\",\"index\":0,\"target\":%d,\"direction\":1,\"require_channel\":%s,\"require_name\":%s}}",
        g_active_profile,g_phone_next.id,g_phone_next.group+6,g_phone_next.target,
        channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    else {
        cJSON *job=cJSON_CreateObject(),*lib=cJSON_AddObjectToObject(job,"library");
        cJSON_AddStringToObject(job,"action","library");cJSON_AddStringToObject(lib,"action","playback_next");
        cJSON_AddStringToObject(lib,"profile",g_active_profile);cJSON_AddStringToObject(lib,"id",g_phone_next.id);
        cJSON_AddBoolToObject(lib,"remote_origin",1);
        cJSON_AddBoolToObject(lib,"require_channel",channel_blocks_active());
        cJSON_AddBoolToObject(lib,"require_name",channel_name_blocks_active());
        if(*g_phone_next.playlist) {
            cJSON_AddStringToObject(lib,"playlist",g_phone_next.playlist);
            cJSON_AddNumberToObject(lib,"index",g_phone_next.index);
        }
        char *body=cJSON_PrintUnformatted(job);cJSON_Delete(job);
        if(!body || strlen(body)>=sizeof r->payload){free(body);complete(r,0);return 0;}
        copy(r->payload,sizeof r->payload,body);free(body);
    }
    g_phone_next.state=0;begin(r);return 1;
}
int tube_phone_recommendation(unsigned owner,unsigned selection,const char *video,const char *playlist,int index,tube_phone_next_t *next) {
    if(next)memset(next,0,sizeof *next);
    if(!next || !video || !playlist || !valid_id(video) || (*playlist && !valid_playlist(playlist)) ||
        index<0 || index>100000 || !selection || selection!=g_selection_serial ||
        !tube_remote_owner_current(owner) || !recovery_scope() || strcmp(video,g_active_id) || !*g_token)return -1;
    if(g_phone_next.owner!=owner || g_phone_next.selection!=selection || strcmp(g_phone_next.id,video) ||
        strcmp(g_phone_next.playlist,playlist) || g_phone_next.index!=index) {
        tube_phone_recommendation_cancel();g_phone_next.owner=owner;g_phone_next.selection=selection;
        g_phone_next.scope=g_playlist_scope;g_phone_next.blocks=g_blocked_revision;g_phone_next.index=index;
        copy(g_phone_next.id,sizeof g_phone_next.id,video);copy(g_phone_next.playlist,sizeof g_phone_next.playlist,playlist);
        phone_next_request();
    }
    if(!phone_next_current())return -1;
    if(g_phone_next.state==2)phone_next_request();
    if(g_phone_next.state==1) {
        copy(next->video,sizeof next->video,g_phone_next.next);
        copy(next->playlist,sizeof next->playlist,g_phone_next.next_playlist);next->index=g_phone_next.next_index;
    }
    return g_phone_next.state;
}
static void phone_next_result(Request *r,const cJSON *root) {
    if(phone_next_integer(root,"api_version",1)!=1){complete(r,0);return;}
    if(!*g_phone_next.group) {
        const cJSON *next=cJSON_GetObjectItemCaseSensitive(root,"next_video_id");
        if(strcmp(str(root,"video_id"),g_phone_next.id)){complete(r,0);return;}
        if(cJSON_IsString(next)) {
            const cJSON *channel=cJSON_GetObjectItemCaseSensitive(root,"next_channel_id");
            const cJSON *name=cJSON_GetObjectItemCaseSensitive(root,"next_channel_name");
            /* Catalog group keys never reach the phone; only real playlist occurrences do. */
            evo_provider_item_t item={0};copy(item.id,sizeof item.id,next->valuestring);
            parse_playlist_context(&item,root,"");
            int listed=item.playback_scope && valid_playlist(item.playback_playlist);
            int distinct=listed && (strcmp(item.playback_playlist,g_phone_next.playlist) || item.playback_index!=g_phone_next.index);
            if(!valid_id(next->valuestring) || (!strcmp(next->valuestring,g_phone_next.id) && !distinct) ||
                (cJSON_GetObjectItemCaseSensitive(root,"playback_context") && !item.playback_scope) ||
                (channel && !cJSON_IsString(channel)) || (name && !cJSON_IsString(name)) ||
                !phone_next_identity(str(root,"next_channel_id"),str(root,"next_channel_name"))) {complete(r,0);return;}
            g_phone_next.state=blocked_identity(str(root,"next_channel_id"),str(root,"next_channel_name"))>=0?-1:1;
            if(g_phone_next.state==1) {
                copy(g_phone_next.next,12,next->valuestring);
                if(listed) {
                    copy(g_phone_next.next_playlist,sizeof g_phone_next.next_playlist,item.playback_playlist);
                    g_phone_next.next_index=item.playback_index;
                }
            }
        } else if(cJSON_IsNull(next)) {
            const cJSON *context=cJSON_GetObjectItemCaseSensitive(root,"fallback_context");
            if(!context){g_phone_next.state=-1;complete(r,1);return;}
            const char *group=str(context,"group");
            char key[43];snprintf(key,sizeof key,"group:%s",group);
            int count=phone_next_integer(root,"fallback_count",4095);
            if(strlen(group)!=36 || !valid_retained_group(key) || phone_next_integer(context,"index",0)!=0 || count<1) {complete(r,0);return;}
            copy(g_phone_next.group,sizeof g_phone_next.group,key);
            g_phone_next.total=count+1;g_phone_next.target=1;g_phone_next.state=2;
        } else {complete(r,0);return;}
        complete(r,1);return;
    }
    const cJSON *ids=cJSON_GetObjectItemCaseSensitive(root,"video_ids"),*indices=cJSON_GetObjectItemCaseSensitive(root,"video_indices");
    const cJSON *channels=cJSON_GetObjectItemCaseSensitive(root,"video_channels"),*names=cJSON_GetObjectItemCaseSensitive(root,"video_channel_names");
    int count=cJSON_GetArraySize(ids),selected=phone_next_integer(root,"index",4095),previous=-1,found=0;
    int valid=!strcmp(str(root,"playlist"),g_phone_next.group) && !strcmp(str(root,"anchor_video_id"),g_phone_next.id) &&
        phone_next_integer(root,"anchor_index",0)==0 && phone_next_integer(root,"playlist_size",4096)==g_phone_next.total &&
        selected>=g_phone_next.target && selected<g_phone_next.total && valid_id(str(root,"video_id")) &&
        cJSON_IsArray(ids) && count>0 && count<=128 && cJSON_IsArray(indices) && cJSON_GetArraySize(indices)==count &&
        (!channels || (cJSON_IsArray(channels) && cJSON_GetArraySize(channels)==count)) &&
        (!names || (cJSON_IsArray(names) && cJSON_GetArraySize(names)==count));
    if(selected!=g_phone_next.target)valid=valid && phone_next_integer(root,"requested_index",4095)==g_phone_next.target &&
        phone_next_integer(root,"skipped_unavailable",4095)==selected-g_phone_next.target;
    char chosen[12]={0};
    for(int i=0;valid && i<count;++i) {
        const cJSON *id=cJSON_GetArrayItem(ids,i),*position=cJSON_GetArrayItem(indices,i);
        const cJSON *channel=channels?cJSON_GetArrayItem(channels,i):NULL,*name=names?cJSON_GetArrayItem(names,i):NULL;
        int index=cJSON_IsNumber(position)?position->valueint:-1;
        const char *cid=cJSON_IsString(channel)?channel->valuestring:"",*label=cJSON_IsString(name)?name->valuestring:"";
        valid=cJSON_IsString(id) && valid_id(id->valuestring) && cJSON_IsNumber(position) && position->valuedouble==index &&
            index>previous && index<g_phone_next.total && (index || !strcmp(id->valuestring,g_phone_next.id)) &&
            (i || index==phone_next_integer(root,"window_start",4095)) &&
            (!channel || cJSON_IsString(channel)) && (!name || cJSON_IsString(name)) && phone_next_identity(cid,label);
        if(!valid)break;
        previous=index;
        if(index==selected && !strcmp(id->valuestring,str(root,"video_id")))found=1;
        if(index>=selected && !*chosen && strcmp(id->valuestring,g_phone_next.id) && blocked_identity(cid,label)<0)copy(chosen,12,id->valuestring);
    }
    if(!valid || !found){complete(r,0);return;}
    if(*chosen){copy(g_phone_next.next,12,chosen);g_phone_next.state=1;}
    else if(previous+1<g_phone_next.total){g_phone_next.target=previous+1;g_phone_next.state=2;}
    else g_phone_next.state=-1;
    complete(r,1);
}
static int playlist_window_current(void) {
    return g_playlist_window.selection && g_playlist_window.selection == g_selection_serial &&
        recovery_scope() && g_playlist_window.context.scope == g_playlist_scope &&
        !strcmp(g_playlist_window.id,g_active_id) &&
        !strcmp(g_playlist_window.context.playlist,g_active_playlist.playlist) &&
        g_playlist_window.context.index == g_active_playlist.index;
}
void tube_playlist_window_cancel(void) {
    if (!++g_playlist_window.serial) ++g_playlist_window.serial;
    g_playlist_window.state = -1;
    g_playlist_window.selection = 0;
    memset(&g_playlist_window.value,0,sizeof g_playlist_window.value);
}
int tube_playlist_window_status(tube_playlist_window_t *out) {
    if (!playlist_window_current()) return -1;
    if (g_playlist_window.state == 1 && out) *out=g_playlist_window.value;
    return g_playlist_window.state;
}
static int playlist_window_begin_at(int target,int direction) {
    char playlist[101]; int index;
    if (!tube_playback_playlist_context(playlist,sizeof playlist,&index)) return 0;
    if (!valid_playback_group(playlist)) return 0;
    if (playlist_window_current() && g_playlist_window.state == 0) return 0;
    tube_playlist_window_cancel();
    Request *r=request_new(12,NULL); if (!r) return 0;
    g_playlist_window.selection=g_selection_serial;
    g_playlist_window.context=g_active_playlist;
    g_playlist_window.lookup=target >= 0;
    g_playlist_window.direction=direction;
    g_playlist_window.scan_next=-1;
    g_playlist_window.target_index=target >= 0 ? target : index;
    copy(g_playlist_window.id,sizeof g_playlist_window.id,g_active_id);
    g_playlist_window.state=0;
    r->intent=g_playlist_window.serial;
    if(valid_retained_group(playlist)) snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"group_window\",\"profile\":\"%s\",\"id\":\"%s\",\"group\":\"%s\",\"index\":%d,\"target\":%d,\"direction\":%d,\"require_channel\":%s,\"require_name\":%s}}",
        g_active_profile,g_active_id,playlist+6,index,target>=0?target:index,direction,channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    else if(valid_channel_group(playlist)) snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"channel_window\",\"profile\":\"%s\",\"id\":\"%s\",\"channel\":\"%s\",\"index\":%d,\"target\":%d,\"direction\":%d,\"require_channel\":true,\"require_name\":%s}}",
        g_active_profile,g_active_id,playlist+8,index,target>=0?target:index,direction,channel_name_blocks_active()?"true":"false");
    else if (g_playlist_window.lookup) snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"playback_at\",\"profile\":\"%s\",\"playlist\":\"%s\",\"index\":%d,\"direction\":%d,\"require_channel\":%s,\"require_name\":%s}}",
        g_active_profile,playlist,target,direction,channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    else snprintf(r->payload,sizeof r->payload,
        "{\"action\":\"library\",\"library\":{\"action\":\"playback_queue\",\"profile\":\"%s\",\"id\":\"%s\",\"playlist\":\"%s\",\"index\":%d,\"require_channel\":%s,\"require_name\":%s}}",
        g_active_profile,g_active_id,playlist,index,channel_blocks_active()?"true":"false",channel_name_blocks_active()?"true":"false");
    begin(r); return 1;
}
int tube_playlist_window_begin(void) { return playlist_window_begin_at(-1,0); }
int tube_playlist_window_at(int index) {
    return index >= 0 && index <= 100000 ? playlist_window_begin_at(index,0) : 0;
}
static int playlist_index(const cJSON *value) {
    return cJSON_IsNumber(value) && value->valuedouble >= 0 && value->valuedouble <= 100000 &&
        value->valuedouble == (double)value->valueint ? value->valueint : -1;
}
static void playlist_window_result(Request *r,const cJSON *root) {
    if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"unavailable_only"))) {
        int requested=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"requested_index"));
        int skipped=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"skipped_unavailable"));
        const cJSON *size_value=cJSON_GetObjectItemCaseSensitive(root,"playlist_size");
        int size=cJSON_IsNumber(size_value) && size_value->valuedouble==size_value->valueint?size_value->valueint:0;
        const cJSON *exhausted=cJSON_GetObjectItemCaseSensitive(root,"exhausted");
        const cJSON *infinite=cJSON_GetObjectItemCaseSensitive(root,"is_infinite");
        const cJSON *rebased=cJSON_GetObjectItemCaseSensitive(root,"radio_rebased");
        int next=requested+g_playlist_window.direction*skipped;
        int end=next==-1 || next==size;
        int valid=r->intent==g_playlist_window.serial && playlist_window_current() &&
            (!(valid_channel_group(g_playlist_window.context.playlist) || valid_retained_group(g_playlist_window.context.playlist)) ||
                (!strcmp(str(root,"anchor_video_id"),g_playlist_window.id) &&
                 playlist_index(cJSON_GetObjectItemCaseSensitive(root,"anchor_index"))==g_playlist_window.context.index)) &&
            g_playlist_window.lookup && g_playlist_window.direction && num(root,"api_version")==1 &&
            !strcmp(str(root,"playlist"),g_playlist_window.context.playlist) &&
            strncmp(g_playlist_window.context.playlist,"RD",2) &&
            (!infinite || cJSON_IsFalse(infinite)) && (!rebased || cJSON_IsFalse(rebased)) &&
            requested==g_playlist_window.target_index && skipped>0 && skipped<=4096 &&
            size>requested && size<=100001 && next>=-1 && next<=size &&
            cJSON_IsBool(exhausted) && cJSON_IsTrue(exhausted)==end &&
            (end ? !cJSON_GetObjectItemCaseSensitive(root,"next_index") :
                playlist_index(cJSON_GetObjectItemCaseSensitive(root,"next_index"))==next);
        if(!valid) {complete(r,0);return;}
        g_playlist_window.scan_next=end?-1:next;g_playlist_window.state=2;
        complete(r,1);return;
    }
    tube_playlist_window_t value={0};
    const cJSON *ids=cJSON_GetObjectItemCaseSensitive(root,"video_ids");
    const cJSON *indices=cJSON_GetObjectItemCaseSensitive(root,"video_indices");
    const cJSON *channels=cJSON_GetObjectItemCaseSensitive(root,"video_channels");
    const cJSON *names=cJSON_GetObjectItemCaseSensitive(root,"video_channel_names");
    const int start=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"window_start"));
    const char *selected_id=str(root,"video_id");
    int selected_index=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"index"));
    int skipped=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"skipped_unavailable"));
    int requested=playlist_index(cJSON_GetObjectItemCaseSensitive(root,"requested_index"));
    int matching_index=selected_index==g_playlist_window.target_index ||
        (g_playlist_window.direction && skipped>0 && skipped<4096 &&
         requested==g_playlist_window.target_index && selected_index>=0 &&
         selected_index==requested+g_playlist_window.direction*skipped);
    value.count=cJSON_GetArraySize(ids); value.current=-1;
    int valid=r->intent == g_playlist_window.serial && playlist_window_current() &&
        num(root,"api_version") == 1 && !strcmp(str(root,"playlist"),g_playlist_window.context.playlist) &&
        (!(valid_channel_group(g_playlist_window.context.playlist) || valid_retained_group(g_playlist_window.context.playlist)) ||
            (!strcmp(str(root,"anchor_video_id"),g_playlist_window.id) &&
             playlist_index(cJSON_GetObjectItemCaseSensitive(root,"anchor_index"))==g_playlist_window.context.index)) &&
        valid_id(selected_id) && (g_playlist_window.lookup || !strcmp(selected_id,g_playlist_window.id)) &&
        matching_index &&
        cJSON_IsArray(ids) && value.count > 0 && value.count <= 128 && start >= 0 &&
        (!indices || (cJSON_IsArray(indices) && cJSON_GetArraySize(indices) == value.count)) &&
        (!channels || (cJSON_IsArray(channels) && cJSON_GetArraySize(channels)==value.count)) &&
        (!names || (cJSON_IsArray(names) && cJSON_GetArraySize(names)==value.count));
    for (int i=0; valid && i<value.count; ++i) {
        const cJSON *id=cJSON_GetArrayItem(ids,i);
        int position=indices ? playlist_index(cJSON_GetArrayItem(indices,i)) : start+i;
        valid=cJSON_IsString(id) && valid_id(id->valuestring) && position >= 0 && position <= 100000 &&
            (!i ? position == start : position > value.indices[i-1]);
        if (!valid) break;
        copy(value.ids[i],sizeof value.ids[i],id->valuestring); value.indices[i]=position;
        const cJSON *channel=channels?cJSON_GetArrayItem(channels,i):NULL;
        if(channels) {
            char parent[40];snprintf(parent,sizeof parent,"channel:%s",cJSON_IsString(channel)?channel->valuestring:"");
            if(!cJSON_IsString(channel) || (*channel->valuestring && !valid_channel_group(parent))) {valid=0;break;}
            copy(value.channels[i],sizeof value.channels[i],channel->valuestring);
        }
        if(valid_channel_group(g_playlist_window.context.playlist)) {
            if(*value.channels[i] && strcmp(value.channels[i],g_playlist_window.context.playlist+8)){valid=0;break;}
            copy(value.channels[i],sizeof value.channels[i],g_playlist_window.context.playlist+8);
        }
        if(channel_blocks_active() && !*value.channels[i]) {valid=0;break;}
        if(names) {
            const cJSON *name=cJSON_GetArrayItem(names,i);
            if(!cJSON_IsString(name) || strlen(name->valuestring)>255 ||
               (*name->valuestring && !tube_channel_name_equal(name->valuestring,name->valuestring))) {valid=0;break;}
            copy(value.channel_names[i],sizeof value.channel_names[i],name->valuestring);
        }
        if(channel_name_blocks_active() && !*value.channel_names[i]) {valid=0;break;}
        if (position == selected_index && !strcmp(id->valuestring,selected_id)) value.current=i;
    }
    if (!valid || value.current < 0) { complete(r,0); return; }
    const cJSON *size=cJSON_GetObjectItemCaseSensitive(root,"playlist_size");
    const cJSON *infinite=cJSON_GetObjectItemCaseSensitive(root,"is_infinite");
    const cJSON *rebased=cJSON_GetObjectItemCaseSensitive(root,"radio_rebased");
    if (cJSON_IsNumber(size) && size->valuedouble == (double)size->valueint &&
        size->valueint > value.indices[value.count-1] && size->valueint <= 100001 &&
        (!infinite || cJSON_IsFalse(infinite)) && (!rebased || cJSON_IsFalse(rebased)) &&
        strncmp(g_playlist_window.context.playlist,"RD",2)) value.playlist_size=size->valueint;
    copy(value.playlist,sizeof value.playlist,g_playlist_window.context.playlist);
    value.scope=g_playlist_window.context.scope; value.selection=g_playlist_window.selection;
    g_playlist_window.value=value; g_playlist_window.state=1;
    complete(r,1);
}

static int auth_session_valid(const char *value) {
    return value && strlen(value) == 32 && strspn(value, "0123456789abcdef") == 32;
}

unsigned tube_auth_revision(void) { return g_auth.revision; }

static void auth_cancel_remote(const char *source, const char *token, const char *session) {
    if (!*source || !*token || !auth_session_valid(session)) return;
    char url[640], auth[160], body[80];
    snprintf(url, sizeof url, "%s/v1/auth/cancel", source);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    snprintf(body, sizeof body, "{\"session\":\"%s\"}", session);
    const char *headers[] = {"Content-Type: application/json", auth};
    evo_net_request_async("POST", url, body, headers, 2, ignored, NULL);
}

static void chat_close_remote(const char *source, const char *token, const char *profile, const char *cursor) {
    if (!*source || !*token || !valid_profile(profile) || !valid_cursor(cursor)) return;
    char url[640], auth[160], payload[256];
    snprintf(url, sizeof url, "%s/v1/library", source);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
    snprintf(payload, sizeof payload, "{\"action\":\"chat_close\",\"profile\":\"%s\",\"cursor\":\"%s\"}", profile, cursor);
    const char *headers[] = {"Content-Type: application/json", auth};
    evo_net_request_async("POST", url, payload, headers, 2, ignored, NULL);
}

static void chat_cleanup_request(Request *r, const cJSON *root) {
    const char *job = r->stage == 1 ? str(root, "job") : r->job;
    if (valid_cursor(job) && *r->auth_source && *r->auth_token) {
        char url[640], auth[160];
        snprintf(url, sizeof url, "%s/v1/jobs/%s", r->auth_source, job);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->auth_token);
        const char *headers[] = {auth};
        evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
    }
    const cJSON *result = !strcmp(str(root, "kind"), "live_chat") ? root : cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!strcmp(str(result, "kind"), "live_chat"))
        chat_close_remote(r->auth_source, r->auth_token, r->profile, str(result, "cursor"));
}

static void chat_row_id(char *out, size_t capacity, const char *id) {
    unsigned long long hash = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)id; *p; ++p) { hash ^= *p; hash *= 1099511628211ULL; }
    snprintf(out, capacity, "chat:leaf:%u:%016llx", g_chat.serial, hash);
}

static void chat_result(Request *r, const cJSON *result) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(result, "items");
    const cJSON *delay = cJSON_GetObjectItemCaseSensitive(result, "poll_after_ms");
    const cJSON *ended = cJSON_GetObjectItemCaseSensitive(result, "ended");
    int count = cJSON_GetArraySize(rows);
    const cJSON *replay = cJSON_GetObjectItemCaseSensitive(result, "is_replay");
    const cJSON *position = cJSON_GetObjectItemCaseSensitive(result, "position_ms");
    const cJSON *page = cJSON_GetObjectItemCaseSensitive(result, "chat_page");
    const cJSON *more = cJSON_GetObjectItemCaseSensitive(result, "has_more");
    int page_valid = cJSON_IsFalse(more) && !page && !r->chat_page;
    if (!g_chat.player && cJSON_IsTrue(replay) && cJSON_IsNumber(page) &&
        page->valuedouble == r->chat_page && (cJSON_IsTrue(more) || cJSON_IsFalse(more))) page_valid = 1;
    int replay_valid = cJSON_IsFalse(replay) || (cJSON_IsTrue(replay) &&
        r->has_chat_position && cJSON_IsNumber(position) && isfinite(position->valuedouble) &&
        position->valuedouble == (double)r->chat_position_ms);
    int valid = num(result, "api_version") == 1 && !strcmp(str(result, "kind"), "live_chat") &&
        !strcmp(str(result, "id"), g_chat.video) && !strcmp(str(result, "chat_mode"), g_chat.mode) &&
        replay_valid &&
        page_valid &&
        valid_cursor(str(result, "cursor")) && (cJSON_IsTrue(ended) || cJSON_IsFalse(ended)) &&
        cJSON_IsNumber(delay) && isfinite(delay->valuedouble) && delay->valuedouble >= 0 && delay->valuedouble <= 30000 &&
        floor(delay->valuedouble) == delay->valuedouble && cJSON_IsArray(rows) && count <= CHAT_HISTORY;
    char ids[CHAT_HISTORY][160];
    for (int i = 0; valid && i < count; ++i) {
        const cJSON *row = cJSON_GetArrayItem(rows, i);
        valid = *str(row, "id") && strlen(str(row, "id")) <= 640 &&
            cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "content")) &&
            cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "author"));
        chat_row_id(ids[i], sizeof ids[i], str(row, "id"));
        for (int j = 0; j < i; ++j) if (!strcmp(ids[i], ids[j])) valid = 0;
    }
    if (!valid) {
        chat_cleanup_request(r, result);
        tube_set_error("The chat response was invalid. Reopen the chat to retry."); complete(r, 0); return;
    }
    evo_provider_item_t *items = calloc(CHAT_HISTORY, sizeof *items);
    if (!items) { chat_cleanup_request(r, result); complete(r, 0); return; }
    memset(g_comments, 0, sizeof g_comments);
    memset(g_chat_messages, 0, sizeof g_chat_messages);
    for (int i = 0; i < count; ++i) {
        const cJSON *row = cJSON_GetArrayItem(rows, i);
        const char *type = str(row, "type"), *label = "";
        if (!strcmp(type, "LiveChatPaidMessage")) label = "Super Chat";
        else if (!strcmp(type, "LiveChatPaidSticker")) label = "Super Sticker";
        else if (!strcmp(type, "LiveChatMembershipItem")) label = "Member";
        else if (strstr(type, "Gift")) label = "Membership gift";
        char hint[256];
        snprintf(hint, sizeof hint, "%s%s%s%s%s%s%s", str(row, "published"), *label ? " | " : "", label,
            *str(row, "amount") ? " | " : "", str(row, "amount"),
            boolean(row, "moderator") ? " | Moderator" : "", boolean(row, "verified") ? " | Verified" : "");
        folder(&items[i], ids[i], *str(row, "author") ? str(row, "author") : "Viewer", hint);
        copy(items[i].overview, sizeof items[i].overview, str(row, "content"));
        copy(items[i].art_url, sizeof items[i].art_url, str(row, "art_url"));
        copy(g_chat_messages[i].id, sizeof g_chat_messages[i].id, ids[i]);
        copy(g_chat_messages[i].content, sizeof g_chat_messages[i].content, str(row, "content"));
    }
    copy(g_chat.cursor, sizeof g_chat.cursor, str(result, "cursor"));
    copy(g_chat.source, sizeof g_chat.source, r->auth_source); copy(g_chat.token, sizeof g_chat.token, r->auth_token);
    g_chat.state = cJSON_IsTrue(ended) ? 3 : 2;
    g_chat.replay = cJSON_IsTrue(replay);
    g_chat.reader_more = !g_chat.player && g_chat.replay && cJSON_IsTrue(more);
    g_chat.position_ms = g_chat.replay ? r->chat_position_ms : 0;
    if (!g_chat.player) g_chat.seek = 0;
    g_chat.next_poll = now_ms() + (delay->valuedouble < 1000 ? 1000 : (long long)delay->valuedouble);
    evo_provider_items_cb callback = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r);
    if (callback) callback(1, items, count, 0, data);
    free(items);
}

static void player_chat_text(char *out, size_t capacity, const char *text) {
    size_t length = strlen(text);
    if (length >= capacity) {
        length = capacity - 1;
        while (length && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    /* The upstream fixed-size item may itself have clipped the final codepoint. */
    size_t tail = length;
    while (tail && ((unsigned char)text[tail - 1] & 0xc0) == 0x80) --tail;
    if (tail) {
        --tail;
        unsigned char lead = (unsigned char)text[tail];
        size_t needed = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : lead >= 0xc0 ? 2 : 1;
        if (length - tail < needed) length = tail;
    }
    memcpy(out, text, length); out[length] = 0;
}
static void player_chat_result(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)more;
    if (!g_chat.player || (unsigned)(size_t)ud != g_chat.serial) return;
    if (ok && g_chat.replay && (g_chat.position_ms / 1000.0 > g_player_chat_position + .001 ||
        (g_chat.seek && g_chat.position_ms < g_chat.seek_position_ms))) {
        g_player_chat_count = 0; g_chat.seek = 1; g_chat.next_poll = 0;
        g_chat.seek_position_ms = (int64_t)floor(g_player_chat_position * 1000 + .000001);
        return; /* Keep the returned cursor, but never render a pre-seek view. */
    }
    g_chat.seek = 0;
    memset(g_player_chat_rows, 0, sizeof g_player_chat_rows); g_player_chat_count = 0;
    if (!ok || !items || count < 0) return;
    int first = count > 6 ? count - 6 : 0;
    for (int i = first; i < count; ++i) {
        tube_player_chat_row_t *row = &g_player_chat_rows[g_player_chat_count++];
        player_chat_text(row->author, sizeof row->author, items[i].title);
        player_chat_text(row->message, sizeof row->message, items[i].overview);
    }
}

int tube_player_chat_mode(void) { return g_chat.player ? g_player_chat_mode : 0; }
const char *tube_player_chat_title(void) { return g_chat.replay ? "CHAT REPLAY" : "TOP CHAT"; }
int tube_player_chat_preference(void) { return g_player_chat_mode; }
int tube_player_chat_set_mode(int mode) {
    if (mode < 0 || mode > 2) return 0;
    g_player_chat_mode = mode;
    if (g_chat.player && (!mode || g_chat.state < 0)) tube_chat_stop();
    save();
    return 1;
}
int tube_player_chat_rows(const tube_player_chat_row_t **rows) {
    if (rows) *rows = g_player_chat_rows;
    return g_chat.player ? g_player_chat_count : 0;
}
const char *tube_player_chat_status(void) {
    return g_chat.state < 0 ? "Chat unavailable. Toggle the sidebar to retry." :
           g_chat.state == 3 ? "Chat ended" : g_player_chat_count ? "" :
           g_chat.state == 1 ? "Loading chat" : "Waiting for messages";
}
void tube_player_chat_tick(int visible) {
    tube_player_chat_tick_at(visible, g_player_chat_position >= 0 ? g_player_chat_position : 0);
}
void tube_player_chat_tick_at(int visible, double position_sec) {
    if (!visible || !g_player_chat_mode || !valid_id(g_active_id) ||
        strcmp(g_active_profile, g_library_profile) || !*g_source || !*g_token ||
        !isfinite(position_sec) || position_sec < 0 || position_sec > 1000000) {
        if (g_chat.player) tube_chat_stop();
        return;
    }
    if (g_chat.player && (strcmp(g_chat.video, g_active_id) || strcmp(g_chat.profile, g_active_profile))) tube_chat_stop();
    if (g_chat.player && g_player_chat_position >= 0 &&
        (position_sec < g_player_chat_position - .25 || position_sec > g_player_chat_position + 10)) {
        g_player_chat_count = 0; g_chat.seek = 1; g_chat.next_poll = 0;
        g_chat.seek_position_ms = (int64_t)floor(position_sec * 1000 + .000001);
    }
    g_player_chat_position = position_sec;
    if (!g_chat.player && g_chat.state) return; /* The browser reader owns this chat. */
    if (g_chat.player && (g_chat.state < 0 || (!g_chat.seek &&
        (g_chat.state == 3 || now_ms() < g_chat.next_poll)))) return;
    for (int i = 0; i < REQUESTS; ++i)
        if (g_requests[i].used && g_requests[i].kind != 8) return; /* Foreground work has priority. */
    int reuse = g_chat.player && (g_chat.state == 2 || g_chat.state == 3) && valid_cursor(g_chat.cursor);
    if (!reuse) {
        g_chat.player = 1; g_chat.state = 1;
        copy(g_chat.video, sizeof g_chat.video, g_active_id); copy(g_chat.profile, sizeof g_chat.profile, g_active_profile);
        copy(g_chat.mode, sizeof g_chat.mode, "top");
        snprintf(g_chat.parent, sizeof g_chat.parent, "chat:top:%.11s", g_active_id);
    }
    Request *r = request_new(10, (void *)(size_t)g_chat.serial);
    if (!r) return;
    r->items_cb = player_chat_result; r->intent = g_chat.serial; copy(r->profile, sizeof r->profile, g_active_profile);
    r->has_chat_position = 1; r->chat_position_ms = (int64_t)floor(position_sec * 1000 + .000001);
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"%s\",\"id\":\"%s\",\"chat_mode\":\"top\",\"profile\":\"%s\",\"cursor\":\"%s\",\"chat_history\":false,\"chat_position_sec\":%.3f}",
        reuse ? "chat_poll" : "live_chat", g_active_id, g_active_profile, reuse ? g_chat.cursor : "", r->chat_position_ms / 1000.0);
    begin(r);
}

void tube_auth_cancel(void) {
    if (g_auth.state != AUTH_PENDING && g_auth.state != AUTH_STARTING) return;
    auth_cancel_remote(g_source, g_token, g_auth.session);
    g_auth.state = AUTH_CANCELED;
    g_auth.code[0] = g_auth.url[0] = 0;
    if (!++g_auth.serial) ++g_auth.serial;
    ++g_auth.revision;
}

static void auth_cleanup_request(Request *r, const cJSON *root) {
    char url[640], auth[160];
    const char *job = r->stage == 1 ? str(root, "job") : r->job;
    if (valid_cursor(job) && *r->auth_source && *r->auth_token) {
        snprintf(url, sizeof url, "%s/v1/jobs/%s", r->auth_source, job);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", r->auth_token);
        const char *headers[] = {auth};
        evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
    }
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (!strcmp(str(result, "state"), "pending"))
        auth_cancel_remote(r->auth_source, r->auth_token, str(result, "session"));
}

static int auth_rows(evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t rows[4]; int n = 0;
    if (g_auth.state == AUTH_PENDING) {
        folder(&rows[n++], "auth:info:code", g_auth.code, "Enter this code on your phone or computer");
        folder(&rows[n++], "auth:info:url", g_auth.url, "Open this address to authorize YouTube");
        folder(&rows[n++], "auth:cancel", "Cancel sign-in", "Circle also cancels when leaving sign-in");
        snprintf(rows[0].overview, sizeof rows[0].overview,
            "Open %s on your phone or computer and enter %s. Waiting for authorization; status checks are automatic. The code expires automatically if sign-in is not completed.",
            g_auth.url, g_auth.code);
    } else if (g_auth.state == AUTH_AUTHORIZED) {
        char id[64]; snprintf(id, sizeof id, "library:profile:%s", g_auth.profile);
        folder(&rows[n++], id, "Use authorized YouTube account", g_auth.name);
        folder(&rows[n++], "library:profiles", "All profiles", g_embedded ? "Account saved in the app data on this PS5" : "Account saved privately on the companion");
    } else {
        const char *title = g_auth.state == AUTH_CANCELED ? "Sign-in canceled" :
            g_auth.state == AUTH_EXPIRED ? "Sign-in code expired" :
            g_auth.state == AUTH_DENIED ? "YouTube authorization denied" :
            g_auth.state == AUTH_STARTING ? "Requesting YouTube sign-in code" : "YouTube sign-in could not be completed";
        folder(&rows[n++], "auth:info", title, g_embedded ? "Account credentials stay in the app data on this PS5" : "Account credentials stay on the companion");
        if (g_auth.state == AUTH_FAILED && *g_auth.error) copy(rows[0].overview,sizeof rows[0].overview,g_auth.error);
        if (g_auth.state != AUTH_STARTING)
            folder(&rows[n++], "auth:start", "Start a new sign-in", "Authorize using your phone or computer");
    }
    if (cb) cb(1, rows, n, 0, ud);
    return 0;
}

static void auth_result(Request *r, const cJSON *result) {
    int changed = 1;
    const char *state = str(result, "state"), *session = str(result, "session");
    if (!auth_session_valid(session) || (*g_auth.session && strcmp(g_auth.session, session))) {
        auth_request_error(r, "The sign-in response was invalid."); complete(r, 0); return;
    }
    if (!strcmp(state, "pending")) {
        const char *code = str(result, "user_code"), *url = str(result, "verification_url");
        double expires = num(result, "expires_in"), delay = num(result, "poll_after");
        if (!*code || strlen(code) > 32 || strspn(code, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") != strlen(code) ||
            (strcmp(url, "https://www.google.com/device") && strcmp(url, "https://google.com/device") &&
             strcmp(url, "https://www.youtube.com/activate") && strcmp(url, "https://youtube.com/activate") &&
             strcmp(url, "https://www.google.com/activate") && strcmp(url, "https://google.com/activate") &&
             strcmp(url, "https://www.youtube.com/device") && strcmp(url, "https://youtube.com/device")) ||
            !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(result, "expires_in")) ||
            !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(result, "poll_after")) ||
            cJSON_GetObjectItemCaseSensitive(result, "poll_after")->valuedouble < 0 ||
            cJSON_GetObjectItemCaseSensitive(result, "poll_after")->valuedouble > 60 ||
            expires < 1 || expires > 1800 || floor(expires) != expires || floor(delay) != delay || delay > 60) {
            auth_cancel_remote(r->auth_source, r->auth_token, session);
            auth_request_error(r, "The sign-in response was invalid."); complete(r, 0); return;
        }
        changed = g_auth.state != AUTH_PENDING || strcmp(g_auth.code, code) || strcmp(g_auth.url, url);
        copy(g_auth.session, sizeof g_auth.session, session);
        copy(g_auth.code, sizeof g_auth.code, code); copy(g_auth.url, sizeof g_auth.url, url);
        long long deadline = now_ms() + (long long)(expires * 1000);
        if (g_auth.state != AUTH_PENDING || deadline < g_auth.deadline) g_auth.deadline = deadline;
        g_auth.state = AUTH_PENDING;
        g_auth.next_poll = now_ms() + (long long)((delay < 1 ? 1 : delay) * 1000);
    } else if (!strcmp(state, "authorized")) {
        const cJSON *profile = cJSON_GetObjectItemCaseSensitive(result, "profile");
        const char *id = str(profile, "id"), *name = str(profile, "name");
        if (g_auth.state != AUTH_PENDING || !valid_profile(id) || strlen(id) != 22 || strncmp(id, "oauth_", 6) ||
            strspn(id + 6, "0123456789abcdef") != 16 || !*name || strlen(name) > 600) {
            auth_request_error(r, "The authorized profile response was invalid."); complete(r, 0); return;
        }
        g_auth.state = AUTH_AUTHORIZED;
        copy(g_auth.profile, sizeof g_auth.profile, id); copy(g_auth.name, sizeof g_auth.name, name);
        ++g_accounts_revision; /* Refresh a cached profile list even when leaving approval with Back. */
    } else if (!strcmp(state, "canceled")) g_auth.state = AUTH_CANCELED;
    else if (!strcmp(state, "expired")) g_auth.state = AUTH_EXPIRED;
    else if (!strcmp(state, "denied")) g_auth.state = AUTH_DENIED;
    else if (!strcmp(state, "failed")) {
        auth_request_error(r,*str(result,"error") ? str(result,"error") : "YouTube sign-in could not be completed. Start a new session.");
        g_auth.state = AUTH_FAILED;
    }
    else { auth_request_error(r, "The sign-in response was invalid."); complete(r, 0); return; }
    if (g_auth.state != AUTH_PENDING) g_auth.code[0] = g_auth.url[0] = 0;
    if (changed) ++g_auth.revision;
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r); auth_rows(cb, data);
}

static void watch_flush(int terminal_flush) {
    if (g_history_state == TUBE_HISTORY_DISABLED || !*g_token || !g_watch.dirty || g_watch.failed ||
        !valid_id(g_watch.id) || !valid_profile(g_watch.profile) || !strcmp(g_watch.profile, "guest")) return;
    if (g_watch.position <= 0 && !g_watch.sent) { g_watch.dirty = 0; return; }
    int free_slot = 0;
    for (int i = 0; i < REQUESTS; ++i) {
        if (g_requests[i].used && g_requests[i].kind == 8 && g_requests[i].intent == g_watch.serial && !terminal_flush) return;
        if (!g_requests[i].used) free_slot = 1;
    }
    if (!free_slot || (g_watch.sent && !g_watch.stopped && now_ms() - g_watch.last_sent < 30000)) return;
    Request *r = request_new(8, NULL); if (!r) return;
    r->intent = g_watch.serial; r->watch_position = g_watch.position; r->stage = 3;
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"watch_progress\",\"profile\":\"%s\",\"id\":\"%s\",\"session\":\"%s\",\"position_sec\":%lld,\"state\":\"%s\"}",
        g_watch.profile, g_watch.id, g_watch.session, (long long)g_watch.position, g_watch.stopped ? "stopped" : "playing");
    g_watch.dirty = 0; g_watch.sent = 1; g_watch.last_sent = now_ms(); g_watch.status = 1;
    send_request(r, "POST", "/v1/library", r->payload);
}

static void caption_response(int ok, int status, const char *body, size_t size, void *ud) {
    Request *r = ud; r->inflight = 0;
    if (!r->used) return;
    if (r->generation != g_generation) { complete(r, 0); return; }
    if (ok && status == 200 && size < 2 * 1024 * 1024 && !memchr(body, 0, size)) {
        char path[512]; copy(path, sizeof path, evo_data_path("active.srt"));
        FILE *f = fopen(path, "wb");
        if (f) {
            int success = fwrite(body, 1, size, f) == size;
            if (fclose(f)) success = 0;
            if (success) {
                copy(g_caption_path, sizeof g_caption_path, evo_data_path("active.mp4"));
                copy(g_caption_language, sizeof g_caption_language, r->caption_language);
                if (!strcmp(g_caption_language, g_secondary_caption_language))
                    g_secondary_caption_path[0] = g_secondary_caption_language[0] = 0;
            }
        }
    }
    complete(r, 1); /* A missing caption never prevents ordinary playback. */
}

static void sponsor_response(int ok, int status, const char *body, size_t size, void *ud) {
    unsigned gen = (unsigned)(size_t)ud;
    if (!ok || status != 200 || gen != g_sb_serial || strcmp(g_sb_source, g_source) ||
        strcmp(g_sb_profile, g_library_profile) || size > 65536) return;
    cJSON *root = cJSON_ParseWithLength(body, size);
    if (!root) return;
    g_segment_count = 0;
    cJSON *s;
    cJSON_ArrayForEach(s, cJSON_GetObjectItemCaseSensitive(root, "segments")) {
        double start = num(s, "start"), end = num(s, "end");
        const cJSON *a = cJSON_GetObjectItemCaseSensitive(s, "start"), *b = cJSON_GetObjectItemCaseSensitive(s, "end");
        const char *category = str(s, "category");
        int kind = tube_sb_category(*category ? category : "sponsor");
        if (cJSON_IsNumber(a) && cJSON_IsNumber(b) && isfinite(start) && isfinite(end) &&
            kind >= 0 && start >= 0 && (kind == 8 ? end == start : end > start) &&
            end < 604800 && g_segment_count < SEGMENTS) {
            g_segment_categories[g_segment_count] = (unsigned char)kind;
            g_segment_seen[g_segment_count] = 0;
            g_segments[g_segment_count][0] = start; g_segments[g_segment_count++][1] = end;
        }
    }
    cJSON_Delete(root);
}

static int caption_cached(const char *media, const char *language) {
    int found = 0;
    for (int i = 0; i < g_caption_track_count; ++i)
        if (!strcmp(language, g_caption_tracks[i].language)) found = 1;
    if (!found || !*media) return 0;
    char sidecar[512]; copy(sidecar, sizeof sidecar, media);
    size_t length = strlen(sidecar); if (length < 4) return 0;
    memcpy(sidecar + length - 4, ".srt", 5);
    FILE *file = fopen(sidecar, "rb"); if (!file) return 0;
    int usable = fgetc(file) != EOF; fclose(file); return usable;
}
/* "Name: value\r\n" lines only; anything else leaves the stream without extra headers. */
static void stream_headers(char *out, size_t capacity, const char *value) {
    size_t length = strlen(value);
    out[0] = 0;
    if (!length || length >= capacity || length < 2 || strcmp(value + length - 2, "\r\n")) return;
    for (const char *line = value; *line;) {
        const char *end = strstr(line, "\r\n"), *colon = memchr(line, ':', (size_t)(end - line));
        if (!colon || colon == line) return;
        for (const char *c = line; c < end; ++c) if ((unsigned char)*c < 32 || (unsigned char)*c >= 127) return;
        line = end + 2;
    }
    copy(out, capacity, value);
}

static void resolved(Request *r, const cJSON *root) {
    if (strcmp(str(root, "id"), r->parent)) {
        tube_set_error("The playback response belongs to another video."); complete(r, 0); return;
    }
    r->choice_count = 0;
    cJSON *c;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "choices")) {
        if (r->choice_count >= CHOICES) break;
        if (!companion_url(str(c, "url"))) continue;
        evo_stream_choice_t *out = &r->choices[r->choice_count++];
        copy(out->url, sizeof out->url, str(c, "url"));
        copy(out->label, sizeof out->label, str(c, "label"));
        copy(out->container, sizeof out->container, str(c, "container"));
        copy(out->video_codec, sizeof out->video_codec, str(c, "video_codec"));
        copy(out->audio_codec, sizeof out->audio_codec, str(c, "audio_codec"));
        const char *range = str(c, "video_range");
        copy(out->video_range, sizeof out->video_range,
             !strcmp(range, "HDR10") || !strcmp(range, "HLG") ? range : "SDR");
        out->width = (int)num(c, "width"); out->height = (int)num(c, "height");
        out->bitrate_bps = (int64_t)num(c, "bitrate_bps"); out->is_live = boolean(c, "is_live");
        out->audio_channels = (int)num(c, "audio_channels");
        double fps = num(c,"fps"); out->fps = isfinite(fps) && fps > 0 ? fps : 0;
        /* The built-in service does not relay media: the player fetches from
         * YouTube itself and sends the request headers the service names. */
        if (g_embedded) stream_headers(out->headers, sizeof out->headers, str(c, "headers"));
    }
    if (!r->choice_count) { tube_set_error(g_embedded ? "No supported playback choices were returned." : "The companion returned no supported playback choices."); complete(r, 0); return; }
    if (!tube_recovery_active()) {
        copy(g_paid_text, sizeof g_paid_text, str(root, "paid_content_text")); g_paid_shown = 0;
    }
    copy(g_resolved_id, sizeof g_resolved_id, r->parent);
    copy(g_resolved_profile, sizeof g_resolved_profile, r->profile);
    g_resolved_music = r->music_origin;
    g_resolved_quality = r->quality;
    g_resolved_playlist = r->playlist;
    g_resolved_incognito = r->incognito;
    static const char *const facts[] = {"loudness_db", "is_short", "chapters", "storyboards",
                                      "channel", "channel_id", "views", "published", "channel_art_url"};
    cJSON_Delete(g_resolved_facts); g_resolved_facts = cJSON_CreateObject();
    for (unsigned i = 0; g_resolved_facts && i < sizeof facts / sizeof *facts; ++i) {
        if (!strcmp(facts[i], "channel_art_url") && !tube_art_url_allowed(str(root, facts[i]))) continue;
        cJSON *fact = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(root, facts[i]), 1);
        if (fact && !cJSON_AddItemToObject(g_resolved_facts, facts[i], fact)) cJSON_Delete(fact);
    }
    Record *cached = record(str(root, "id"), 1);
    if (cached) {
        copy(cached->item.title, sizeof cached->item.title, str(root, "title"));
        cached->item.duration_sec = (int64_t)num(root, "duration_sec");
        cached->item.is_live = r->choices[0].is_live;
    }
    const int retain_captions = tube_recovery_active() && !strcmp(r->parent, g_caption_video) &&
        !strcmp(r->profile, g_caption_profile) && !strcmp(g_source, g_caption_source) &&
        !strcmp(g_token, g_caption_token);
    char primary_path[512] = {0}, primary_language[33] = {0};
    char secondary_path[512] = {0}, secondary_language[33] = {0};
    if (retain_captions) {
        copy(primary_path, sizeof primary_path, g_caption_path);
        copy(primary_language, sizeof primary_language, g_caption_language);
        copy(secondary_path, sizeof secondary_path, g_secondary_caption_path);
        copy(secondary_language, sizeof secondary_language, g_secondary_caption_language);
    }
    g_caption_path[0] = g_caption_language[0] = 0; g_segment_count = 0; g_skip_guard = -1;
    g_secondary_caption_path[0] = g_secondary_caption_language[0] = 0;
    tube_caption_switch_cancel(); g_caption_track_count = 0; ++g_caption_revision;
    copy(g_caption_video, sizeof g_caption_video, r->parent);
    copy(g_caption_profile, sizeof g_caption_profile, r->profile);
    copy(g_caption_source, sizeof g_caption_source, g_source); copy(g_caption_token, sizeof g_caption_token, g_token);
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "captions")) {
        const char *language = str(c, "language"), *url = str(c, "url");
        if (g_caption_track_count >= CAPTION_TRACKS || !*language || strlen(language) >= 33 ||
            strspn(language, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(language) ||
            strlen(url) >= EVO_PROVIDER_MAX_URL || !companion_url(url)) continue;
        int duplicate = 0;
        for (int i = 0; i < g_caption_track_count; ++i) if (!strcmp(g_caption_tracks[i].language, language)) duplicate = 1;
        if (duplicate) continue;
        int i = g_caption_track_count++;
        copy(g_caption_tracks[i].language, sizeof g_caption_tracks[i].language, language);
        copy(g_caption_tracks[i].name, sizeof g_caption_tracks[i].name, str(c, "name"));
        copy(g_caption_tracks[i].url, sizeof g_caption_tracks[i].url, url);
        g_caption_tracks[i].automatic = boolean(c, "automatic");
        g_caption_tracks[i].translated = boolean(c, "translated");
        const char *target = str(c, "target"); /* the language a machine translation is in */
        copy(g_caption_tracks[i].target, sizeof g_caption_tracks[i].target,
             g_caption_tracks[i].translated && strlen(target) < 33 &&
             strspn(target, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == strlen(target) ? target : "");
    }
    if (retain_captions && caption_cached(primary_path, primary_language)) {
        copy(g_caption_path, sizeof g_caption_path, primary_path);
        copy(g_caption_language, sizeof g_caption_language, primary_language);
    }
    if (retain_captions && caption_cached(secondary_path, secondary_language)) {
        copy(g_secondary_caption_path, sizeof g_secondary_caption_path, secondary_path);
        copy(g_secondary_caption_language, sizeof g_secondary_caption_language, secondary_language);
    }
    g_sb_prompt = -1; g_sb_last_position = -1; g_sb_message = 0; g_sb_finish_target = -1;
    g_highlight_guard = -1;
    if (!++g_sb_serial) ++g_sb_serial;
    copy(g_sb_source, sizeof g_sb_source, g_source);
    copy(g_sb_channel, sizeof g_sb_channel, cached ? cached->item.parent_id : "");
    copy(g_sb_profile, sizeof g_sb_profile, g_library_profile);
    g_chapter_count = 0;
    cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "chapters")) {
        if (g_chapter_count < 128) g_chapters[g_chapter_count++] = num(c, "start");
    }
    if (g_sponsor && cached && !cached->item.is_live && r->choice_count > 0 &&
        !r->choices[0].is_live && !tube_sb_excluded(&g_sb, g_sb_channel)) {
        char url[1280], auth[160];
        char categories[160] = {0};
        for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) if (g_sb.actions[i] != 3 || g_sb.colors[i]) {
            size_t used = strlen(categories);
            snprintf(categories + used, sizeof categories - used, "%s%s", used ? "," : "", tube_sb_names[i]);
        }
        /* The alternative server is the companion host's choice: ask for it only where it is offered.
         * The built-in service has no host, so the address typed in the settings goes with the request. */
        char alt[300] = "";
        if (g_sb_alt && !g_embedded && companion_has(COMPANION_SB_ALT)) copy(alt, sizeof alt, "&alt=true");
        else if (g_sb_alt && g_embedded && *g_sb_server && companion_has(COMPANION_SB_SERVER))
            snprintf(alt, sizeof alt, "&alt=true&server=https%%3A%%2F%%2F%s", g_sb_server + 8);
        snprintf(url, sizeof url, "%s/v1/sponsor/%s?categories=%s%s", g_source, cached->item.id, categories, alt);
        snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
        const char *headers[] = {auth};
        if (*categories) evo_net_request_async("GET", url, NULL, headers, 1, sponsor_response, (void *)(size_t)g_sb_serial);
    }
    if (captions_wanted(cached ? cached->item.parent_id : "") && !*g_caption_path) {
        cJSON_ArrayForEach(c, cJSON_GetObjectItemCaseSensitive(root, "captions")) {
            /* Every resolve lists the automatic tracks and the machine translations;
             * only "Automatic captions: allowed" lets an automatic one start by itself. */
            if (strcmp(str(c, "language"), g_language) || !companion_url(str(c, "url")) ||
                (boolean(c, "automatic") && !g_auto_captions)) continue;
            r->stage = 3;
            copy(r->caption_language, sizeof r->caption_language, str(c, "language"));
            int rc = evo_net_request_async("GET", str(c, "url"), NULL, NULL, 0, caption_response, r);
            r->inflight = rc == 0;
            if (!rc) return;
            break;
        }
    }
    complete(r, 1);
}

static void resolve_failure(Request *r, const cJSON *error) {
    if (r->kind != 2 || r->stage < 1 || r->generation != g_generation ||
        strcmp(r->profile,g_library_profile) || strcmp(str(error,"code"),"video_unavailable")) return;
    g_resolve_failure.generation = r->generation; g_resolve_failure.unavailable = 1;
    g_resolve_failure.queue_origin = r->queue_origin;
    copy(g_resolve_failure.id,sizeof g_resolve_failure.id,r->parent);
    copy(g_resolve_failure.profile,sizeof g_resolve_failure.profile,r->profile);
}
int tube_resolve_unavailable(const char *id) {
    return id && g_resolve_failure.unavailable && g_resolve_failure.generation == g_generation &&
        !strcmp(id,g_resolve_failure.id) && !strcmp(g_resolve_failure.profile,g_library_profile);
}

/* A companion without the library worker, or the offline fixture, cannot
 * filter: the same request continues as the plain video search, and later
 * searches skip the attempt until an option changes. */
static int search_fallback(Request *r, const char *code) {
    if (!r->library_search || r->page || strcmp(r->parent, g_search_feed.key) ||
        (strcmp(code, "library_unavailable") && strcmp(code, "unknown_operation"))) return 0;
    cJSON *o = cJSON_CreateObject(); if (!o) return 0;
    cJSON_AddStringToObject(o, "action", "search"); cJSON_AddStringToObject(o, "profile", g_library_profile);
    cJSON_AddStringToObject(o, "query", g_search_feed.parent + 7); cJSON_AddNumberToObject(o, "page", 0);
    char *body = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    if (!body || strlen(body) >= sizeof r->payload) { free(body); return 0; }
    copy(r->payload, sizeof r->payload, body); free(body);
    copy(r->parent, sizeof r->parent, g_search_feed.parent);
    g_search_feed.key[0] = 0; g_search_feed.unavailable = g_playlist_scope;
    r->kind = 1; r->library_search = 0; r->job[0] = 0; r->deadline = now_ms() + 65000;
    begin(r); return 1;
}
/* The codec setting is a preference: a video that has no stream in it is
 * asked for once more in whatever it has. The setting itself stays. */
static int codec_fallback(Request *r, const char *code) {
    static const char any[] = "\"codec\":\"auto\"";
    if (r->kind != 2 || r->codec_fallback || !r->quality.codec || strcmp(code, "unsupported_media")) return 0;
    char *field = strstr(r->payload, "\"codec\":\""), *end = field ? strchr(field + 9, '"') : NULL;
    char payload[sizeof r->payload];
    if (!end || snprintf(payload, sizeof payload, "%.*s%s%s", (int)(field - r->payload), r->payload, any, end + 1) >= (int)sizeof payload) return 0;
    copy(r->payload, sizeof r->payload, payload);
    r->codec_fallback = 1; r->job[0] = 0; r->deadline = now_ms() + 65000;
    begin(r); return 1;
}
/* The first page of a typed search leads with its two folders. */
static int search_rows(evo_provider_item_t *items) {
    char summary[160]; search_summary(summary, sizeof summary);
    folder(&items[0], "settings:search", "Search options", summary);
    folder(&items[1], "search-suggest", "Suggestions", "Completions for this search");
    return 2;
}
static void suggestions_result(Request *r, const cJSON *result) {
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(result, "suggestions"), *row;
    if (strcmp(str(result, "kind"), "search_suggestions") || !cJSON_IsArray(rows)) {
        tube_set_error("The suggestion response was invalid."); complete(r, 0); return;
    }
    evo_provider_item_t *items = calloc(15, sizeof *items); int n = 0;
    if (!items) { complete(r, 0); return; }
    memset(&g_suggestions, 0, sizeof g_suggestions);
    copy(g_suggestions.profile, sizeof g_suggestions.profile, g_library_profile); g_suggestions.scope = g_playlist_scope;
    cJSON_ArrayForEach(row, rows) {
        int duplicate = 0;
        if (n >= 15 || !cJSON_IsString(row) || !tube_search_query_valid(row->valuestring) ||
            strlen(row->valuestring) >= sizeof g_suggestions.text[0]) continue;
        for (int i = 0; i < n; ++i) if (search_same(g_suggestions.text[i], row->valuestring)) duplicate = 1;
        if (duplicate) continue;
        char id[64]; snprintf(id, sizeof id, "search-suggest:run:%u:%d", g_playlist_scope, n);
        folder(&items[n], id, row->valuestring, "Cross searches for this");
        if (r->typed_suggest) items[n].tube_interface = 0; /* a completion of what is being typed: YouTube's words, shown as they are */
        copy(g_suggestions.text[n++], sizeof g_suggestions.text[0], row->valuestring);
    }
    g_suggestions.count = n;
    if (!n) folder(&items[n++], "account:notice", "No suggestions", "Circle returns to the search");
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r); if (cb) cb(1, items, n, 0, data);
    free(items);
}
static void request_rows(Request *r, const evo_provider_item_t *items, int n) {
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r); if (cb) cb(1, items, n, 0, data);
}
/* The rows in g_detail are shown at once, or after the dislike relay answered:
 * the profile's switch and the companion's host option both have to allow it.
 * The built-in service has the relay enabled from its start (embedded_start), so
 * there the switch alone decides, and this is the only place that asks: one
 * request per opened page, none while the switch is off. */
static void detail_finish(Request *r) {
    if (g_dislikes && companion_has(COMPANION_DISLIKES) && valid_id(g_detail.video)) {
        char path[64]; snprintf(path, sizeof path, "/v1/dislikes/%s", g_detail.video);
        r->stage = 6; r->deadline = now_ms() + 20000;
        send_request(r, "GET", path, NULL); return;
    }
    request_rows(r, g_detail.rows, g_detail.count);
}
/* Without a usable answer the page simply has no dislike row. */
static void dislikes_result(Request *r, const cJSON *root) {
    const cJSON *dislikes = cJSON_GetObjectItemCaseSensitive(root, "dislikes"); char digits[24], label[96];
    if (root && boolean(root, "enabled") && boolean(root, "available") && !strcmp(str(root, "id"), g_detail.video) &&
        cJSON_IsNumber(dislikes) && dislikes->valuedouble >= 0 && dislikes->valuedouble <= 1e12 &&
        floor(dislikes->valuedouble) == dislikes->valuedouble && g_detail.count < 12) {
        snprintf(digits, sizeof digits, "%.0f", dislikes->valuedouble);
        if (count_label(label, sizeof label, digits, "dislike"))
            folder(&g_detail.rows[g_detail.count++], "account:notice:dislikes", label, "Estimated by Return YouTube Dislike");
    }
    request_rows(r, g_detail.rows, g_detail.count);
}
/* The first row carries the whole text for the reader; the others are its header. */
static void description_result(Request *r, const cJSON *result) {
    const char *id = r->parent + 12, *channel = str(result, "channel_id"), *views = str(result, "views");
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(result, "description");
    char catalog[40], label[96] = ""; snprintf(catalog, sizeof catalog, "channel:%s", channel);
    if (strcmp(str(result, "kind"), "description") || strcmp(str(result, "id"), id) || !cJSON_IsString(text) ||
        strlen(text->valuestring) >= sizeof g_description.content || strlen(views) > 20 || strspn(views, "0123456789") != strlen(views)) {
        tube_set_error("The description response was invalid."); complete(r, 0); return;
    }
    evo_provider_item_t *items = g_detail.rows; int n = 0;
    memset(&g_detail, 0, sizeof g_detail); copy(g_detail.video, sizeof g_detail.video, id);
    memset(&g_description, 0, sizeof g_description);
    snprintf(g_description.id, sizeof g_description.id, "description:text:%u", r->generation);
    copy(g_description.content, sizeof g_description.content, *text->valuestring ? text->valuestring : "This video has no description.");
    copy(g_description.profile, sizeof g_description.profile, g_library_profile); g_description.scope = g_playlist_scope;
    folder(&items[n], g_description.id, *str(result, "title") ? str(result, "title") : "Description",
        boolean(result, "truncated") ? "Description, shortened by the companion; L2 / R2 scroll" : "Description; L2 / R2 scroll");
    copy(items[n].overview, sizeof items[n].overview, g_description.content); ++n;
    if (*str(result, "channel")) {
        if (valid_channel_group(catalog)) folder(&items[n++], catalog, str(result, "channel"), "Channel; Cross opens it");
        else folder(&items[n++], "account:notice:channel", str(result, "channel"), "Channel");
    }
    if (*str(result, "published")) folder(&items[n++], "account:notice:published", str(result, "published"), "Published");
    if (*views) { /* group the digits in threes */
        size_t length = strlen(views), used = 0;
        for (size_t i = 0; i < length; ++i) {
            if (i && !((length - i) % 3)) label[used++] = ',';
            label[used++] = views[i];
        }
        snprintf(label + used, sizeof label - used, " view%s", strcmp(views, "1") ? "s" : "");
        folder(&items[n++], "account:notice:views", label, "Views");
    }
    if (count_label(label, sizeof label, str(result, "like_count"), "like")) folder(&items[n++], "account:notice:likes", label, "Likes");
    g_detail.count = n; detail_finish(r);
}
static int channel_tab_parse(const char *parent, int *tab, int *sort);
static void details_result(Request *r, const cJSON *result) {
    const char *id = r->parent + 8, *subscribers;
    const cJSON *channel = cJSON_GetObjectItemCaseSensitive(result, "channel"), *badge;
    char target[64], label[128], text[160] = ""; evo_provider_item_t *rows = g_detail.rows; int n = 0;
    if (strcmp(str(result, "kind"), "video_details") || strcmp(str(result, "id"), id) || !cJSON_IsObject(channel)) {
        tube_set_error("The details response was invalid."); complete(r, 0); return;
    }
    memset(&g_detail, 0, sizeof g_detail); copy(g_detail.video, sizeof g_detail.video, id);
    cJSON_ArrayForEach(badge, cJSON_GetObjectItemCaseSensitive(result, "badges")) {
        const char *name = !cJSON_IsString(badge) ? NULL : !strcmp(badge->valuestring, "live") ? "Live" :
            !strcmp(badge->valuestring, "upcoming") ? "Upcoming" : !strcmp(badge->valuestring, "4k") ? "4K" : NULL;
        if (name && strlen(text) < 64) snprintf(text + strlen(text), sizeof text - strlen(text), "%s%s", *text ? " | " : "", name);
    }
    const long seconds = (long)num(result, "duration_sec") > 360000 ? 0 : (long)num(result, "duration_sec");
    if (seconds >= 3600) snprintf(text + strlen(text), sizeof text - strlen(text), "%s%ld:%02ld:%02ld", *text ? " | " : "", seconds / 3600, seconds / 60 % 60, seconds % 60);
    else if (seconds > 0) snprintf(text + strlen(text), sizeof text - strlen(text), "%s%ld:%02ld", *text ? " | " : "", seconds / 60, seconds % 60);
    folder(&rows[n], "account:notice:title", *str(result, "title") ? str(result, "title") : id, *text ? text : "Video details");
    copy(rows[n].overview, sizeof rows[n].overview, str(result, "description")); ++n;
    if (*str(channel, "name")) {
        subscribers = str(channel, "subscribers");
        snprintf(target, sizeof target, "channel:%.24s", str(channel, "id"));
        if (count_label(label, sizeof label, subscribers, "subscriber")) subscribers = label;
        folder(&rows[n], valid_channel_group(target) && strlen(str(channel, "id")) == 24 ? target : "account:notice:channel", str(channel, "name"),
            *subscribers ? subscribers : "Channel");
        copy(rows[n].art_url, sizeof rows[n].art_url, str(channel, "art_url")); ++n;
    }
    if (*str(result, "published")) folder(&rows[n++], "account:notice:published", str(result, "published"), "Published");
    if (count_label(label, sizeof label, str(result, "views"), "view")) folder(&rows[n++], "account:notice:views", label, "Views");
    if (count_label(label, sizeof label, str(result, "like_count"), "like")) folder(&rows[n++], "account:notice:likes", label, "Likes");
    snprintf(target, sizeof target, "description:%s", id);
    folder(&rows[n++], target, "Read the description", "The full text; L2 / R2 scroll");
    if (boolean(result, "has_related")) {
        snprintf(target, sizeof target, "library:suggestions:%s", id);
        folder(&rows[n++], target, "Related videos", "Suggestions for this video and the selected profile");
    }
    g_detail.count = n; detail_finish(r);
}
static void reminder_result(Request *r, const cJSON *result) {
    const char *id = r->parent + 9; evo_provider_item_t rows[2]; int n = 0; char target[64];
    const cJSON *coming = cJSON_GetObjectItemCaseSensitive(result, "upcoming"), *available = cJSON_GetObjectItemCaseSensitive(result, "available"),
        *set = cJSON_GetObjectItemCaseSensitive(result, "reminder_set");
    if (strcmp(str(result, "kind"), "stream_reminder") || strcmp(str(result, "id"), id) || !cJSON_IsBool(coming) || !cJSON_IsBool(available) || !cJSON_IsBool(set)) {
        tube_set_error("The reminder response was invalid."); complete(r, 0); return;
    }
    if (!cJSON_IsTrue(coming)) folder(&rows[n++], "account:notice", "This video is not an upcoming stream", "Reminders exist for streams and premieres that have not started");
    else if (!cJSON_IsTrue(available)) folder(&rows[n++], "account:notice", "YouTube offers no reminder for this stream",
        !strcmp(g_library_profile, "guest") ? "Choose an account profile to set a reminder" : "The stream has no Notify me control for this profile");
    else {
        folder(&rows[n++], "account:notice:reminder", cJSON_IsTrue(set) ? "Reminder: set" : "Reminder: not set", "YouTube notifies this account when the stream starts");
        snprintf(target, sizeof target, "reminder:confirm:%s:%s", cJSON_IsTrue(set) ? "unset" : "set", id);
        folder(&rows[n++], target, cJSON_IsTrue(set) ? "Remove stream reminder" : "Set stream reminder", "Asks before changing the account");
    }
    request_rows(r, rows, n);
}
static void playlist_order_result(Request *r, const cJSON *result) {
    const char *playlist = r->parent + 15; evo_provider_item_t rows[5]; int n = 0; unsigned seen = 0;
    const cJSON *orders = cJSON_GetObjectItemCaseSensitive(result, "orders"), *order;
    if (strcmp(str(result, "kind"), "playlist_order") || strcmp(str(result, "playlist_id"), playlist) || !cJSON_IsArray(orders)) {
        tube_set_error("The playlist order response was invalid."); complete(r, 0); return;
    }
    if (boolean(result, "editable")) cJSON_ArrayForEach(order, orders) for (int i = 0; i < 5; ++i) {
        char id[160];
        if (!cJSON_IsString(order) || strcmp(order->valuestring, g_playlist_orders[i]) || (seen >> i & 1)) continue;
        seen |= 1u << i; snprintf(id, sizeof id, "playlist-order:confirm:%s:%.100s", g_playlist_orders[i], playlist);
        folder(&rows[n++], id, g_playlist_order_labels[i], !strcmp(str(result, "order"), g_playlist_orders[i]) ? "Selected" : "Sorts this playlist on YouTube; asks first");
    }
    if (!n) folder(&rows[n++], "account:notice", "This playlist cannot be sorted", "YouTube offers an order only for playlists of this account");
    request_rows(r, rows, n);
}
static void search_first_result(Request *r, const cJSON *result) {
    const cJSON *row = cJSON_GetObjectItemCaseSensitive(result, "item"); evo_provider_item_t item;
    memset(&item, 0, sizeof item);
    if (strcmp(str(result, "kind"), "search_first")) { tube_set_error("The search response was invalid."); complete(r, 0); return; }
    if (cJSON_IsObject(row)) parse_item(&item, row);
    if (!boolean(result, "found") || !cJSON_IsObject(row) || !valid_id(item.id) || item.is_folder || blocked_identity(item.parent_id, item.subtitle) >= 0) {
        folder(&item, "account:notice", "No playable video on the first result page", "Circle returns to the search options");
        request_rows(r, &item, 1); return;
    }
    Record *cached = record(item.id, 1);
    if (cached) { /* what this PS5 knows of the video's progress stays */
        const int64_t position = cached->item.resume_pos_sec; const int played = cached->item.played, known = cached->updated > 0;
        cached->item = item;
        if (known) { cached->item.resume_pos_sec = position; cached->item.played = played; }
        item = cached->item;
        /* The host takes it once, as it does for Play from start. */
        copy(g_menu_launch.id, sizeof g_menu_launch.id, item.id);
        copy(g_menu_launch.profile, sizeof g_menu_launch.profile, g_library_profile); g_menu_launch.scope = g_playlist_scope;
    }
    request_rows(r, &item, 1);
}
static void channel_watched_result(Request *r, const cJSON *result) {
    const cJSON *o; evo_provider_item_t item; const char *channel = r->parent + 16;
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items"))
        if (valid_id(str(o, "id")) && !boolean(o, "is_folder")) { channel_mark_put(channel, str(o, "id")); break; }
    if (g_channel_marks_dirty) save();
    folder(&item, "account:notice", "Channel marked as watched", "Its uploads were opened for the selected profile; the new-content mark is cleared");
    request_rows(r, &item, 1);
}
static void notification_count_result(Request *r, const cJSON *result) {
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(result, "unseen_count");
    if (r->intent != g_playlist_scope || strcmp(r->profile, g_library_profile) ||
        num(result, "api_version") != 1 || strcmp(str(result, "kind"), "notification_count") ||
        !cJSON_IsNumber(count) || !isfinite(count->valuedouble) || count->valuedouble < 0 ||
        count->valuedouble > 2147483647 || floor(count->valuedouble) != count->valuedouble) {
        complete(r, 0); return;
    }
    evo_provider_item_t item; char value[16];
    folder(&item, "library:notification-count", "Notifications", "");
    snprintf(value, sizeof value, "%d", (int)count->valuedouble);
    tube_row_fact(item.id, "unseen_count", value);
    request_rows(r, &item, 1);
}
static void facts_result(Request *r, const cJSON *result) {
    if (!strcmp(r->parent, "library:notification-count")) notification_count_result(r, result);
    else if (!strncmp(r->parent, "details:", 8)) details_result(r, result);
    else if (!strncmp(r->parent, "reminder:", 9)) reminder_result(r, result);
    else if (!strncmp(r->parent, "playlist-order:", 15)) playlist_order_result(r, result);
    else if (!strncmp(r->parent, "search-first:", 13)) search_first_result(r, result);
    else channel_watched_result(r, result);
}
/* The parsed export waits for one confirmation; nothing is followed yet. */
static void import_result(Request *r, const cJSON *root) {
    const cJSON *channels = cJSON_GetObjectItemCaseSensitive(root, "channels"), *o;
    evo_provider_item_t rows[2]; char label[160], detail[200], file[64];
    double skipped = num(root, "skipped");
    import_release();
    if (num(root, "api_version") != 1 || strcmp(str(root, "kind"), "subscriptions_import") || !cJSON_IsArray(channels)) {
        tube_set_error("The import response was invalid."); complete(r, 0); return;
    }
    const unsigned serial = g_import.serial + 1; copy(file, sizeof file, g_import.file);
    memset(&g_import, 0, sizeof g_import); g_import.serial = serial ? serial : 1;
    copy(g_import.file, sizeof g_import.file, file); copy(g_import.profile, sizeof g_import.profile, g_library_profile);
    copy(g_import.format, sizeof g_import.format, str(root, "format"));
    cJSON_ArrayForEach(o, channels) {
        const char *channel = str(o, "channel_id"); int duplicate = 0;
        if (!valid_channel(channel)) { ++skipped; continue; }
        for (int i = 0; i < g_import.listed; ++i) if (!strcmp(g_import.rows[i].channel, channel)) duplicate = 1;
        if (duplicate || g_import.count >= 100000) continue;
        ++g_import.count;
        if (g_import.listed == FOLLOWS) continue; /* more than the follow list can ever hold */
        copy(g_import.rows[g_import.listed].channel, sizeof g_import.rows[0].channel, channel);
        copy(g_import.rows[g_import.listed++].title, sizeof g_import.rows[0].title, str(o, "title"));
    }
    g_import.skipped = skipped > 100000 ? 100000 : (int)skipped; g_import.truncated = boolean(root, "truncated");
    snprintf(label, sizeof label, "%d channel%s found in %s", g_import.count, g_import.count == 1 ? "" : "s", g_import.file);
    snprintf(detail, sizeof detail, "Format: %s; %d entr%s without a channel identity skipped%s", *g_import.format ? g_import.format : "unknown",
        g_import.skipped, g_import.skipped == 1 ? "y" : "ies", g_import.truncated ? "; the file lists more than the companion reads" : "");
    folder(&rows[0], "account:notice:import", label, detail);
    if (g_import.count) {
        char id[64]; snprintf(id, sizeof id, "settings:backup:import:apply:%u", g_import.serial);
        snprintf(label, sizeof label, "Add %d channel%s to Followed channels", g_import.count, g_import.count == 1 ? "" : "s");
        folder(&rows[1], id, label, "For the selected profile on this PS5; channels already followed are left alone");
    }
    request_rows(r, rows, g_import.count ? 2 : 1);
}
static int channel_compare(const void *a, const void *b) {
    const ChannelRow *x = a, *y = b; const unsigned char *s = (const unsigned char *)x->title, *t = (const unsigned char *)y->title;
    if (g_channel_sort == 2) return x->has_new != y->has_new ? y->has_new - x->has_new : x->order - y->order;
    for (; *s && *t; ++s, ++t) if (tolower(*s) != tolower(*t)) return tolower(*s) < tolower(*t) ? -1 : 1;
    return *s ? 1 : *t ? -1 : strcmp(x->id, y->id);
}
static void channel_page(int page, evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t *items = calloc(LOCAL_PAGE, sizeof *items); int n = 0;
    if (!items) { cb(0, NULL, 0, 0, ud); return; }
    for (int i = page * LOCAL_PAGE; i < g_channel_list.count && n < LOCAL_PAGE; ++i, ++n) {
        char marked[EVO_PROVIDER_MAX_SUBTITLE + 8];
        snprintf(marked, sizeof marked, "%s%s%s", g_channel_rows[i].has_new ? "New" : "",
            g_channel_rows[i].has_new && *g_channel_rows[i].subtitle ? " | " : "", g_channel_rows[i].subtitle);
        folder(&items[n], g_channel_rows[i].id, g_channel_rows[i].title, marked);
        copy(items[n].art_url, sizeof items[n].art_url, g_channel_rows[i].art);
        copy(items[n].parent_id, sizeof items[n].parent_id, g_channel_rows[i].id + 8);
    }
    cb(1, items, n, (page + 1) * LOCAL_PAGE < g_channel_list.count, ud); free(items);
}
/* Sorting by name: follow the list to its end, bounded, before showing a page. */
static void channels_result(Request *r, const cJSON *result) {
    const char *cursor = str(result, "cursor"); int more = boolean(result, "has_more"); const cJSON *o;
    if (more && !valid_cursor(cursor)) { tube_set_error("The library pagination response was invalid."); complete(r, 0); return; }
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items")) {
        const char *id = str(o, "id"); int duplicate = 0;
        if (g_channel_list.count >= CHANNEL_ROWS || !valid_channel_group(id) || blocked_channel(id + 8) >= 0) continue;
        for (int i = 0; i < g_channel_list.count; ++i) if (!strcmp(g_channel_rows[i].id, id)) duplicate = 1;
        if (duplicate) continue;
        ChannelRow *row = &g_channel_rows[g_channel_list.count++];
        copy(row->id, sizeof row->id, id); copy(row->title, sizeof row->title, *str(o, "title") ? str(o, "title") : id + 8);
        copy(row->subtitle, sizeof row->subtitle, str(o, "subtitle")); copy(row->art, sizeof row->art, str(o, "art_url"));
        row->has_new = channel_new(o, id + 8); row->order = g_channel_list.count;
    }
    if (more && g_channel_list.count < CHANNEL_ROWS && ++r->aggregate <= 40) {
        snprintf(r->payload, sizeof r->payload, "{\"action\":\"library\",\"library\":{\"action\":\"continue\",\"profile\":\"%s\",\"cursor\":\"%s\"}}",
            g_library_profile, cursor);
        r->job[0] = 0; r->deadline = now_ms() + 65000; begin(r); return;
    }
    qsort(g_channel_rows, (size_t)g_channel_list.count, sizeof g_channel_rows[0], channel_compare);
    g_channel_list.complete = 1;
    if (g_channel_marks_dirty) save();
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r); if (cb) channel_page(0, cb, data);
}

static void response(int ok, int status, const char *body, size_t size, void *ud) {
    Request *r = ud; r->inflight = 0;
    if (!r->used) return;
    if (r->kind == 10 && (r->generation != g_generation || r->intent != g_chat.serial)) {
        cJSON *late = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
        chat_cleanup_request(r, late); cJSON_Delete(late); complete(r, 0); return;
    }
    if (r->kind == 9 && (r->generation != g_generation || r->intent != g_auth.serial)) {
        cJSON *late = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
        auth_cleanup_request(r, late); cJSON_Delete(late); complete(r, 0); return;
    }
    if (r->generation != g_generation) { complete(r, 0); return; }
    if(r->kind==14 && (r->intent!=g_phone_next.serial || !phone_next_current())) {complete(r,0);return;}
    if(r->kind==13 && (r->intent!=g_recommendation.serial || !recommendation_current())) {complete(r,0);return;}
    if (r->kind == 12 && (r->intent != g_playlist_window.serial || !playlist_window_current())) {
        complete(r,0); return;
    }
    if (r->kind == 8 && r->intent != g_watch.serial) { complete(r, 0); return; }
    cJSON *root = size <= 2 * 1024 * 1024 && body ? cJSON_ParseWithLength(body, size) : NULL;
    if (r->stage == 5) { /* the feature list; without it the request keeps its earlier form */
        if (ok && status == 200 && root) companion_read(root);
        cJSON_Delete(root); begin(r); return;
    }
    if (r->stage == 6) { dislikes_result(r, ok && status == 200 ? root : NULL); cJSON_Delete(root); return; }
    if (!ok || status < 200 || status >= 300 || !root) {
        const cJSON *error = root ? cJSON_GetObjectItemCaseSensitive(root, "error") : NULL;
        if (status == 404 || status == 410) resolve_failure(r,error);
        auth_request_error(r, *str(error, "message") ? str(error, "message") : g_embedded ? "The built-in YouTube service did not answer. Retry." : "Could not reach the companion. Check that it is running and the LAN address is correct.");
        /* A refused field or a new pairing means the companion is not the one that was read. */
        if (status == 401 || status == 422) companion_forget();
        if (status == 401) { g_token[0] = 0; save(); }
        cJSON_Delete(root); complete(r, 0); return;
    }
    if (r->kind == 8) {
        int confirmed = boolean(root, "success") && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(root, "position_sec")) &&
            num(root, "position_sec") == r->watch_position;
        if (confirmed && r->intent == g_watch.serial) g_watch.status = 2;
        cJSON_Delete(root); complete(r, confirmed); return;
    }
    if (r->stage == 0) {
        const char *token = str(root, "token");
        if (strlen(token) < 24 || strlen(token) >= sizeof g_token || strchr(token, '\r') || strchr(token, '\n')) {
            cJSON_Delete(root); tube_set_error("The pairing response was invalid."); complete(r, 0); return;
        }
        copy(g_token, sizeof g_token, token); g_pin[0] = 0; save();
        cJSON_Delete(root); begin(r); return;
    }
    if (r->kind == 10 && r->stage == 4) { chat_result(r, root); cJSON_Delete(root); return; }
    if (r->stage == 7) { import_result(r, root); cJSON_Delete(root); return; }
    if (r->stage == 1) {
        const char *job = str(root, "job");
        if (!*job || strlen(job) >= sizeof r->job || strspn(job, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") != strlen(job)) {
            cJSON_Delete(root); auth_request_error(r, "The companion job response was invalid."); complete(r, 0); return;
        }
        copy(r->job, sizeof r->job, job); r->stage = 2; r->next_poll = now_ms() + 250;
        cJSON_Delete(root); return;
    }
    const char *state = str(root, "state");
    if (!strcmp(state, "pending")) {
        r->next_poll = now_ms() + 500; cJSON_Delete(root); return;
    }
    if (strcmp(state, "done")) {
        const cJSON *error = cJSON_GetObjectItemCaseSensitive(root, "error");
        if (!strcmp(state,"failed") && search_fallback(r,str(error,"code"))) { cJSON_Delete(root); return; }
        if (!strcmp(state,"failed") && codec_fallback(r,str(error,"code"))) { cJSON_Delete(root); return; }
        if (!strcmp(state,"failed") && r->kind == 3 && !r->page && !r->startup && r->items_cb && !strncmp(r->parent,"section:",8) &&
            !strcmp(str(error,"code"),"catalog_unavailable")) {
            /* YouTube offers some sections to some profiles only: one row says so.
             * As the startup section the list fails instead, which returns the browser to the main menu. */
            evo_provider_item_t item;
            folder(&item, "account:notice", "This section is not available from YouTube for this profile", str(error, "message"));
            evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
            memset(r, 0, sizeof *r); cb(1, &item, 1, 0, data);
            cJSON_Delete(root); return;
        }
        if (!strcmp(state,"failed")) resolve_failure(r,error);
        auth_request_error(r, *str(error, "message") ? str(error, "message") : "The request was cancelled or could not be completed.");
        cJSON_Delete(root); complete(r, 0); return;
    }
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(root, "result");
    if(r->kind==14) {phone_next_result(r,result);cJSON_Delete(root);return;}
    if(r->kind==13) {recommendation_result(r,result);cJSON_Delete(root);return;}
    if (r->kind == 9) { auth_result(r, result); cJSON_Delete(root); return; }
    if (r->kind == 2) { resolved(r, result); cJSON_Delete(root); return; }
    if (r->kind == 12) { playlist_window_result(r,result); cJSON_Delete(root); return; }
    if (r->kind == 15) { description_result(r,result); cJSON_Delete(root); return; }
    if (r->kind == 16) { suggestions_result(r,result); cJSON_Delete(root); return; }
    if (r->kind == 17) { facts_result(r,result); cJSON_Delete(root); return; }
    if (r->kind == 3 && r->aggregate) { channels_result(r,result); cJSON_Delete(root); return; }
    if (r->kind == 11) {
        if (r->intent != g_message.ticket || !message_scope() || !boolean(result, "success") ||
            num(result, "api_version") != 1 || strcmp(str(result, "id"), g_message.video) ||
            strcmp(str(result, "session"), g_message.session) || !*str(result, "message_id") ||
            strlen(str(result, "message_id")) > 160) {
            cJSON_Delete(root); tube_set_error("The message was not confirmed. Check the chat before sending again."); complete(r, 0); return;
        }
        g_message.phase = 4; g_message.text[0] = 0;
        memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
        evo_provider_item_t item; folder(&item, "account:notice", "Chat message sent", "Circle returns to the chat");
        if (r->items_cb) { r->items_cb(1, &item, 1, 0, r->ud); r->items_cb = NULL; }
        cJSON_Delete(root); complete(r, 1); return;
    }
    if (r->kind == 6) {
        if(r->intent!=g_account_intent.serial || strcmp(g_account_intent.profile,g_library_profile)) {
            cJSON_Delete(root);complete(r,0);return;
        }
        if (!boolean(result, "success")) {
            cJSON_Delete(root); tube_set_error("YouTube did not confirm the account change. Check your account before repeating it."); complete(r, 0); return;
        }
        int removing = !strcmp(g_account_intent.action, "profile_remove");
        if(!strcmp(g_account_intent.action,"mark_watched")) {
            double position=num(result,"position_sec");
            if(num(result,"api_version")!=1 || strcmp(str(result,"video_id"),g_account_intent.identity) ||
               !isfinite(position) || position<=0 || position>1000000) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this video as watched.");complete(r,0);return;
            }
            tube_mark_watched(g_account_intent.identity);
        }
        if(!strcmp(g_account_intent.action,"history_remove")) {
            if(num(result,"api_version")!=1 || strcmp(str(result,"video_id"),g_account_intent.identity)) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm removal of this video.");complete(r,0);return;
            }
            tube_remove_history(g_account_intent.identity);
        }
        if(!strcmp(g_account_intent.action,"playlist_save") || !strcmp(g_account_intent.action,"playlist_unsave")) {
            const cJSON *state=cJSON_GetObjectItemCaseSensitive(result,"saved");
            if(num(result,"api_version")!=1 || strcmp(str(result,"playlist_id"),g_account_intent.playlist) ||
               !cJSON_IsBool(state) || cJSON_IsTrue(state)!=!strcmp(g_account_intent.action,"playlist_save")) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this playlist change. Reload Your playlists to see its state.");complete(r,0);return;
            }
        }
        if(history_control(g_account_intent.action)) {
            const int control=history_control(g_account_intent.action);
            if(num(result,"api_version")!=1 || strcmp(str(result,"history"),control==1?"paused":control==2?"active":"cleared")) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this history change. Check your account before repeating it.");complete(r,0);return;
            }
            if(control<3) {
                g_history_paused.state=control==1?2:1;g_history_paused.scope=g_playlist_scope;
                copy(g_history_paused.profile,sizeof g_history_paused.profile,g_library_profile);
            }
        }
        if(feedback_option(g_account_intent.action)) {
            if(num(result,"api_version")!=1 || strcmp(str(result,"video_id"),g_account_intent.identity) ||
               strcmp(str(result,"feedback"),g_account_intent.action)) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this feedback.");complete(r,0);return;
            }
            /* The card leaves Home and Subscriptions lists at once; YouTube's own lists follow later. */
            Record *card=record(g_account_intent.identity,0);
            int slot=(int)(g_dismissed_cursor++%64);
            memset(&g_dismissed[slot],0,sizeof g_dismissed[slot]);
            copy(g_dismissed[slot].id,sizeof g_dismissed[slot].id,g_account_intent.identity);
            if(card && !strcmp(g_account_intent.action,"dont_recommend_channel") && tube_sb_channel_valid(card->item.parent_id))
                copy(g_dismissed[slot].channel,sizeof g_dismissed[slot].channel,card->item.parent_id);
            copy(g_dismissed[slot].profile,sizeof g_dismissed[slot].profile,g_library_profile);g_dismissed[slot].scope=g_playlist_scope;
            g_dismissed[slot].hidden=!strcmp(g_account_intent.action,"hide");
        }
        const int commenting = !strncmp(g_account_intent.action, "comment_", 8) && strcmp(g_account_intent.action, "comment_rate");
        int level_missed = 0;
        if(!strcmp(g_account_intent.action,"stream_reminder")) {
            const cJSON *state=cJSON_GetObjectItemCaseSensitive(result,"reminder_set");
            if(num(result,"api_version")!=1 || strcmp(str(result,"id"),g_account_intent.identity) || !cJSON_IsBool(state) ||
               cJSON_IsTrue(state)!=!strcmp(g_account_intent.argument,"set")) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this reminder. Reopen the reminder to see its state.");complete(r,0);return;
            }
        }
        if(!strcmp(g_account_intent.action,"playlist_order")) {
            if(num(result,"api_version")!=1 || strcmp(str(result,"playlist_id"),g_account_intent.playlist) ||
               strcmp(str(result,"order"),g_account_intent.argument)) {
                cJSON_Delete(root);tube_set_error("The companion did not confirm this playlist order. Reload the playlist to see it.");complete(r,0);return;
            }
            g_library_parent[0]=0; /* the playlist is listed again in its new order */
        }
        if(!strcmp(g_account_intent.action,"subscribe_notifications") &&
           (num(result,"api_version")!=1 || strcmp(str(result,"notifications"),g_account_intent.argument))) {
            cJSON_Delete(root);tube_set_error("The companion did not confirm this notification level.");complete(r,0);return;
        }
        if(!strcmp(g_account_intent.action,"subscribe") && *g_account_intent.argument)
            level_missed=strcmp(str(result,"notifications"),g_account_intent.argument)!=0; /* subscribed all the same */
        if(commenting) {
            const cJSON *reply=cJSON_GetObjectItemCaseSensitive(result,"reply");
            if(num(result,"api_version")!=1 || strcmp(str(result,"id"),g_account_intent.identity) || !*g_account_intent.session ||
               strcmp(str(result,"session"),g_account_intent.session) || !cJSON_IsBool(reply) ||
               cJSON_IsTrue(reply)!=!strcmp(g_account_intent.action,"comment_reply")) {
                cJSON_Delete(root);tube_set_error("The comment was not confirmed. Reload the comments before writing it again.");complete(r,0);return;
            }
            g_account_intent.title[0]=0;
        }
        if(!strcmp(g_account_intent.action,"playlist_add") || !strcmp(g_account_intent.action,"playlist_remove")) {
            recent_playlist_note(g_account_intent.playlist);save();
        }
        if (!strcmp(g_account_intent.action, "comment_rate") && strcmp(str(result, "rating"), g_account_intent.identity)) {
            cJSON_Delete(root); tube_set_error("The comment rating was not confirmed."); complete(r, 0); return;
        }
        if (removing && strcmp(str(result, "profile"), g_account_intent.profile)) {
            cJSON_Delete(root); tube_set_error("The companion did not confirm removal of this profile."); complete(r, 0); return;
        }
        if (r->intent == g_account_intent.serial) g_account_intent.state = 2;
        evo_provider_item_t item;
        folder(&item, "account:notice", removing ? "Account removed" : commenting ? "Comment posted" :
            level_missed ? "Subscribed; notification level not confirmed" : "YouTube account updated",
            removing ? "Select another profile or continue as Guest" : commenting ? "Reload the comments to see it" :
            level_missed ? "Set the level again from the channel's notification choices; do not subscribe twice" : "The companion confirmed this change");
        if (!strcmp(g_account_intent.action, "playlist_create") && valid_playlist(str(result, "playlist_id"))) {
            snprintf(item.id, sizeof item.id, "library:playlist:%s", str(result, "playlist_id"));
            copy(item.title, sizeof item.title, "Open created playlist");
        }
        snprintf(item.overview, sizeof item.overview, "Operation: %s | Profile: %s. Circle returns to the video browser.",
            g_account_intent.action, g_account_intent.profile);
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        if (removing) account_removed();
        memset(r, 0, sizeof *r); if (cb) cb(1, &item, 1, 0, data);
        cJSON_Delete(root); return;
    }
    evo_provider_item_t *items = calloc(LOCAL_PAGE + 2, sizeof *items);
    if (!items) { cJSON_Delete(root); complete(r, 0); return; }
    int n = 0, lead = 0; cJSON *o;
    if (r->kind == 4) {
        const cJSON *profiles = cJSON_GetObjectItemCaseSensitive(result, "profiles");
        if (!cJSON_IsArray(profiles)) {
            free(items); cJSON_Delete(root); tube_set_error("The companion profile list was invalid."); complete(r, 0); return;
        }
        int selected = !strcmp(g_library_profile, "guest"), guest = 0, waiting = 0;
        cJSON_ArrayForEach(o, profiles) {
            if (n >= LOCAL_PAGE || !valid_profile(str(o, "id"))) continue;
            if (!strcmp(str(o, "id"), "guest")) guest = 1;
            if (!strcmp(str(o, "id"), g_library_profile)) {
                selected = 1;
                copy(g_profile_name.id, sizeof g_profile_name.id, g_library_profile);
                copy(g_profile_name.name, sizeof g_profile_name.name, str(o, "name"));
            }
            if (!strcmp(str(o, "id"), g_startup_lock)) waiting = 1;
            char id[64]; snprintf(id, sizeof id, "library:profile:%s", str(o, "id"));
            folder(&items[n++], id, str(o, "name"), boolean(o, "account_configured") ? g_embedded ? "Account signed in on this PS5" : "Account configured on the companion" : "Browse as guest");
            tube_row_fact(id, "account_configured", boolean(o, "account_configured") ? "1" : "0");
        }
        if (!guest) {
            free(items); cJSON_Delete(root); tube_set_error("The companion profile list was invalid."); complete(r, 0); return;
        }
        if (!selected) account_removed();
        if (*g_startup_lock && !waiting) { /* the profile whose password was awaited left the companion */
            lock_remove(g_startup_lock); profile_forget(g_startup_lock); g_startup_lock[0] = 0; save();
        }
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        memset(r, 0, sizeof *r);
        if (cb) cb(1, items, n, 0, data);
        free(items); cJSON_Delete(root); return;
    }
    if (r->kind == 5 && (strcmp(str(result, "kind"), "comments") ||
        !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(result, "items")))) {
        free(items); cJSON_Delete(root); tube_set_error("The comment response was invalid."); complete(r, 0); return;
    }
    if (r->kind == 3 || r->kind == 5 || r->kind == 7) {
        const char *cursor = str(result, "cursor");
        int more = boolean(result, "has_more");
        if (more && !valid_cursor(cursor)) {
            free(items); cJSON_Delete(root); tube_set_error("The library pagination response was invalid."); complete(r, 0); return;
        }
        if (!strcmp(r->parent, g_library_parent) && r->page + 1 < LIBRARY_PAGES)
            copy(g_library_cursors[r->page + 1], sizeof g_library_cursors[0], more ? cursor : "");
    }
    if (r->kind == 5) {
        memset(g_comments, 0, sizeof g_comments);
        if (!r->page && strncmp(r->parent, "discussion:replies:", 19) && comment_thread_current() &&
            !strcmp(g_comment_thread.video, strrchr(r->parent, ':') + 1)) {
            /* The first page of a video's comments leads with their count and, where the profile may post, the way to write one. */
            char id[64]; snprintf(id, sizeof id, "comment:post:%s", g_comment_thread.video);
            copy(g_comment_thread.count, sizeof g_comment_thread.count, str(result, "comment_count"));
            g_comment_thread.can_post = boolean(result, "can_post") && strcmp(g_library_profile, "guest");
            if (g_comment_thread.can_post) {
                folder(&items[n++], id, "Write a comment" EXPERIMENTAL,
                    *g_comment_thread.count ? g_comment_thread.count : "Enter text, then review it before posting");
                experimental(&items[n - 1]);
            }
            else if (*g_comment_thread.count) folder(&items[n++], "account:notice:comments", g_comment_thread.count, "Comments on this video");
            lead = n;
        }
        cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items")) {
            if (n >= LOCAL_PAGE + lead || !*str(o, "id")) continue;
            char id[160], hint[256]; const int slot = n - lead;
            const char *reply = str(o, "reply_cursor");
            if (*reply && !valid_cursor(reply)) continue;
            if (*reply) snprintf(id, sizeof id, "discussion:replies:%s", reply);
            else snprintf(id, sizeof id, "discussion:leaf:%u:%d", r->generation, n);
            snprintf(hint, sizeof hint, "%.80s | %.32s likes | %.32s replies%s%s%s%s%s%s",
                str(o, "published"), str(o, "likes"), str(o, "replies"),
                boolean(o, "pinned") ? " | Pinned" : "", boolean(o, "owner") ? " | Creator" : "",
                boolean(o, "member") ? " | Member" : "", boolean(o, "hearted") ? " | Creator's heart" : "",
                boolean(o, "liked") ? " | You like it" : "", boolean(o, "disliked") ? " | You dislike it" : "");
            folder(&items[n], id, *str(o, "author") ? str(o, "author") : "Comment", hint);
            copy(items[n].overview, sizeof items[n].overview, str(o, "content"));
            copy(g_comments[slot].id, sizeof g_comments[slot].id, id);
            copy(g_comments[slot].content, sizeof g_comments[slot].content, str(o, "content"));
            const char *rating = str(o, "rating_cursor"), *respond = str(o, "respond_cursor");
            if (valid_cursor(rating)) {
                copy(g_comments[slot].rating, sizeof g_comments[slot].rating, rating);
                copy(g_comments[slot].profile, sizeof g_comments[slot].profile, g_library_profile);
                g_comments[slot].rating_options = (boolean(o, "rating_like") ? 1u : 0u) |
                    (boolean(o, "rating_dislike") ? 2u : 0u) | (boolean(o, "rating_remove") ? 4u : 0u);
            }
            if (valid_cursor(respond) && strcmp(g_library_profile, "guest")) { /* the reference a reply names */
                copy(g_comments[slot].respond, sizeof g_comments[slot].respond, respond);
                copy(g_comments[slot].profile, sizeof g_comments[slot].profile, g_library_profile);
            }
            ++n;
        }
        evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
        int more = boolean(result, "has_more");
        memset(r, 0, sizeof *r);
        if (cb) cb(1, items, n, more, data);
        free(items); cJSON_Delete(root); return;
    }
    const int home = r->kind == 3 && home_parent(r->parent), cards = r->kind == 3 && feedback_parent(r->parent);
    const int notifications = r->kind == 3 && !strcmp(r->parent, "library:notifications");
    const int subscribed = r->kind == 3 && !strcmp(r->parent, "library:channels");
    if (r->kind == 3 && !r->page && !strcmp(r->parent, "library:history")) {
        const cJSON *paused = cJSON_GetObjectItemCaseSensitive(result, "history_paused");
        g_history_paused.state = cJSON_IsBool(paused) ? cJSON_IsTrue(paused) ? 2 : 1 : 0; g_history_paused.scope = g_playlist_scope;
        copy(g_history_paused.profile, sizeof g_history_paused.profile, g_library_profile);
    }
    if (r->search_rows) n = lead = search_rows(items);
    else if (r->channel_page && channel_note(r, cJSON_GetObjectItemCaseSensitive(result, "channel")) && !r->page && valid_channel_group(r->parent)) {
        char id[48]; snprintf(id, sizeof id, "channel-menu:%s", r->parent + 8);
        folder(&items[n++], id, "Channel sections", "Home, Videos, Shorts, Live, Playlists, sort order and search, as this channel offers them");
        lead = 1;
    }
    cJSON_ArrayForEach(o, cJSON_GetObjectItemCaseSensitive(result, "items")) {
        if (n >= LOCAL_PAGE + lead || !valid_catalog_id(str(o, "id"))) continue;
        int duplicate = 0;
        parse_item(&items[n], o);
        if(!items[n].is_folder && blocked_identity(items[n].parent_id,items[n].subtitle)>=0)continue;
        if(items[n].is_folder && valid_channel_group(items[n].id) && blocked_channel(items[n].id+8)>=0)continue;
        if(cards && !items[n].is_folder && dismissed(items[n].id,items[n].parent_id,!home))continue;
        if(subscribed && items[n].is_folder && valid_channel_group(items[n].id) && channel_new(o,items[n].id+8)) {
            char marked[EVO_PROVIDER_MAX_SUBTITLE+8];
            snprintf(marked,sizeof marked,"New%s%s",*items[n].subtitle?" | ":"",items[n].subtitle);
            copy(items[n].subtitle,sizeof items[n].subtitle,marked);
        }
        if(!items[n].is_folder && valid_id(items[n].id) && boolean(o,"is_upcoming"))upcoming_note(items[n].id);
        if(notifications && boolean(o,"unread")) {
            char sent[EVO_PROVIDER_MAX_SUBTITLE+16];
            snprintf(sent,sizeof sent,"Unread%s%s",*items[n].subtitle?" | ":"",items[n].subtitle);
            copy(items[n].subtitle,sizeof items[n].subtitle,sent);
        }
        parse_playlist_context(&items[n], o, r->parent);
        for (int i = 0; i < n; ++i)
            if (!strcmp(items[i].id,items[n].id) &&
                !strcmp(items[i].playback_playlist,items[n].playback_playlist) &&
                items[i].playback_index == items[n].playback_index) duplicate = 1;
        if (duplicate) continue;
        if (notifications) tube_row_fact(items[n].id, "unread", boolean(o, "unread") ? "1" : "0");
        if (r->kind == 7) {
            if (strncmp(items[n].id, "library:playlist:", 17)) continue;
            const char *video = strrchr(r->parent, ':');
            char target[160];
            snprintf(target, sizeof target, "playlist:confirm:%s:%.11s:%.100s",
                !strncmp(r->parent, "playlist:pick:add:", 18) ? "add" : "remove",
                video ? video + 1 : "", items[n].id + 17);
            for (int i = 0; i < n; ++i) if (!strcmp(items[i].id, target)) duplicate = 1;
            if (duplicate) continue;
            { /* the name a later "most recent playlist" shortcut shows */
                const int slot = (int)(g_picker_cursor++ % 48);
                copy(g_picker_rows[slot].playlist, sizeof g_picker_rows[slot].playlist, items[n].id + 17);
                copy(g_picker_rows[slot].title, sizeof g_picker_rows[slot].title, items[n].title);
            }
            copy(items[n].id, sizeof items[n].id, target); items[n].is_folder = 1;
            copy(items[n].subtitle, sizeof items[n].subtitle, "Select this playlist, then confirm the change");
            ++n; continue;
        }
        if(items[n].playback_scope && valid_playlist(items[n].playback_playlist))
            video_occurrence_note(items[n].id,items[n].playback_playlist,!strncmp(r->parent,"library:",8));
        Record *cached = record(items[n].id, 1);
        if (cached) {
            if(r->kind==3 && !strcmp(r->parent,"library:history")) {
                const char *cursor=str(o,"history_remove_cursor");
                copy(cached->history_remove_cursor,sizeof cached->history_remove_cursor,valid_cursor(cursor)?cursor:"");
                cached->history_scope=g_playlist_scope;cached->history_deadline=now_ms()+110000;
            }
            /* A feedback reference belongs to the Home or Subscriptions card last listed;
             * any other listing of the video retires it. The worker keeps it for 30 minutes. */
            const char *feedback=cards?str(o,"feedback_cursor"):"";
            copy(cached->feedback_cursor,sizeof cached->feedback_cursor,valid_cursor(feedback)?feedback:"");
            cached->feedback_options=(boolean(o,"feedback_not_interested")?1u:0u)|(boolean(o,"feedback_channel")?2u:0u)|(boolean(o,"feedback_hide")?4u:0u);
            cached->feedback_scope=g_playlist_scope;cached->feedback_deadline=now_ms()+1740000;
            char known_title[EVO_PROVIDER_MAX_TITLE],known_channel[EVO_PROVIDER_MAX_SUBTITLE],known_parent[25];
            copy(known_title,sizeof known_title,cached->item.title);copy(known_channel,sizeof known_channel,cached->item.subtitle);
            copy(known_parent,sizeof known_parent,cached->item.parent_id);
            const cJSON *watched=cJSON_GetObjectItemCaseSensitive(o,"watch_percent");
            int remote=cJSON_IsNumber(watched) && isfinite(watched->valuedouble) && watched->valuedouble>=0 && watched->valuedouble<=100;
            if(!remote || (watched->valuedouble==100 && cached->updated>0)) {
                items[n].resume_pos_sec = cached->item.resume_pos_sec;
                items[n].played = cached->item.played;
            }
            int64_t local_position=cached->item.resume_pos_sec;
            int local_played=cached->item.played;
            cached->item = items[n];
            if(remote && watched->valuedouble<100 && cached->updated>0) {
                cached->item.resume_pos_sec=local_position;cached->item.played=local_played;
            }
            if(notifications) { /* the row shows the message and its time; the record keeps the video's own names */
                copy(cached->item.subtitle,sizeof cached->item.subtitle,known_channel);
                if(*known_title)copy(cached->item.title,sizeof cached->item.title,known_title);
                if(!*cached->item.parent_id)copy(cached->item.parent_id,sizeof cached->item.parent_id,known_parent);
            }
            /* Occurrences belong to browser selections, not shared video history. */
            cached->item.playback_playlist[0]=0;
            cached->item.playback_index=0; cached->item.playback_scope=0;
            copy(cached->dearrow_title, sizeof cached->dearrow_title, str(o, "dearrow_title"));
            copy(cached->dearrow_art, sizeof cached->dearrow_art,
                 companion_url(str(o, "dearrow_art_url")) ? str(o, "dearrow_art_url") : "");
            dearrow_item(&items[n], cached);
        }
        ++n;
    }
    if (r->channel_page && !r->page && strcmp(g_library_profile, "guest")) {
        /* Opening a subscribed channel's uploads clears YouTube's mark; remember the newest video seen. */
        char channel[25]; int tab = 1, sort = -1, known = 0;
        copy(channel, sizeof channel, channel_of(r->parent));
        if (!valid_channel_group(r->parent)) channel_tab_parse(r->parent, &tab, &sort);
        for (int i = 0; i < g_channel_new.count; ++i) if (!strcmp(g_channel_new.channel[i], channel)) known = 1;
        if (!known || g_channel_new.scope != g_playlist_scope || strcmp(g_channel_new.profile, g_library_profile)) known = channel_mark_find(channel) >= 0;
        for (int i = lead; known && tab == 1 && sort <= 0 && i < n; ++i)
            if (!items[i].is_folder && valid_id(items[i].id)) { channel_mark_put(channel, items[i].id); break; }
    }
    if (g_channel_marks_dirty) save();
    int more = boolean(result, "has_more");
    if (r->kind == 3 && !r->page && !n && !more && !strcmp(r->parent, "library:home") && !strcmp(g_library_profile, "guest")) {
        /* YouTube answers a signed-out Home with a notice alone: say why and lead on. */
        folder(&items[n++], "account:notice", "Home recommendations need a signed-in account", embedded_guest_only() ?
            "YouTube gives them to signed-in accounts only; sign-in arrives in a later version" : "YouTube provides Home recommendations only to signed-in accounts");
        folder(&items[n++], "account:notice", "Search", "Square searches YouTube from any list");
        folder(&items[n++], "discover", "Discover", "Topics and YouTube sections");
        if (!embedded_guest_only()) folder(&items[n++], "library", "Accounts & library", "Sign in to YouTube to get Home recommendations");
    }
    /* Content filters can leave a companion page empty while more follow: read
     * on, a bounded number of times, instead of showing an empty list. */
    if (r->kind == 3 && !r->aggregate && more && g_hide_set && companion_has(COMPANION_FILTERS) &&
        !cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(result, "items")) &&
        !strcmp(r->parent, g_library_parent) && r->empty_pages < 8) {
        cJSON *operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "action", "continue"); cJSON_AddStringToObject(operation, "profile", g_library_profile);
        cJSON_AddStringToObject(operation, "cursor", str(result, "cursor"));
        ++r->empty_pages; r->job[0] = 0; r->deadline = now_ms() + 65000;
        free(items); cJSON_Delete(root); library_job(r, operation); return;
    }
    evo_provider_items_cb cb = r->items_cb; void *data = r->ud;
    memset(r, 0, sizeof *r);
    if (cb) cb(1, items, n, more, data);
    free(items); cJSON_Delete(root);
}

void tube_poll(void) {
    if (tube_update_app_tick) tube_update_app_tick(!g_embedded || embedded_ready());
    if (!(tube_afterglow_enabled ? tube_afterglow_enabled() : g_ui.interface_kind == TUBE_UI_INTERFACE_AFTERGLOW) && tube_update_app_classic_notice) tube_update_app_classic_notice();
    caption_switch_tick();
    if (g_remote_enabled && tube_remote_state() == TUBE_REMOTE_LISTENING &&
        !strcmp(g_remote_device, tube_remote_device()) && *tube_remote_screen() &&
        strcmp(g_remote_screen, tube_remote_screen())) {
        copy(g_remote_screen, sizeof g_remote_screen, tube_remote_screen());
        g_remote_identity_saved = 0; save();
    }
    remote_configure();
    tube_remote_tick(now_ms());
    for (int i = 0; i < REQUESTS; ++i) {
        Request *r = &g_requests[i];
        if (!r->used || r->inflight) continue;
        if (r->generation != g_generation || now_ms() > r->deadline) {
            auth_request_error(r, r->kind == 6 ? "The account change was not confirmed. Check your account before repeating it." :
                "The companion request was cancelled or timed out. Retry."); complete(r, 0); continue;
        }
        if (r->stage == 2 && now_ms() >= r->next_poll) {
            char path[128]; snprintf(path, sizeof path, "/v1/jobs/%s", r->job);
            send_request(r, "GET", path, NULL);
        }
    }
    if (g_auth.state == AUTH_PENDING) {
        if (now_ms() >= g_auth.deadline) {
            tube_auth_cancel(); g_auth.state = AUTH_EXPIRED; ++g_auth.revision;
        } else if (now_ms() >= g_auth.next_poll) {
            int busy = 0;
            for (int i = 0; i < REQUESTS; ++i)
                if (g_requests[i].used && g_requests[i].kind == 9 && g_requests[i].intent == g_auth.serial) busy = 1;
            if (!busy) {
                Request *r = request_new(9, NULL);
                if (r) {
                    r->intent = g_auth.serial;
                    snprintf(r->payload, sizeof r->payload,
                        "{\"action\":\"auth\",\"auth\":{\"action\":\"status\",\"session\":\"%s\"}}", g_auth.session);
                    begin(r);
                }
            }
        }
    }
    watch_flush(0);
}

static void ignored(int ok, int status, const char *body, size_t size, void *ud) {
    (void)ok; (void)status; (void)body; (void)size; (void)ud;
}

static void cancel_requests(int preserve_chat) {
    memset(&g_next_offer,0,sizeof g_next_offer);
    if(!++g_phone_next.serial)++g_phone_next.serial;
    g_phone_next.state=-1;
    if(g_manual_direction)tube_playlist_eof_cancel();
    g_playlist_eof_wait = g_playlist_next_ready = 0;
    tube_playlist_window_cancel();
    memset(&g_selected_playlist,0,sizeof g_selected_playlist);
    if (!preserve_chat) tube_chat_stop();
    tube_auth_cancel();
    ++g_generation; memset(&g_resolve_failure,0,sizeof g_resolve_failure);
    for (int i = 0; i < REQUESTS; ++i) {
        Request *r = &g_requests[i];
        if (!r->used) continue;
        /* Browsing cancellation must not cancel a playback history report. */
        if (r->kind == 8) { r->generation = g_generation; continue; }
        if (*r->job && *g_source && *g_token) {
            char url[1024], auth[160];
            snprintf(url, sizeof url, "%s/v1/jobs/%s", g_source, r->job);
            snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
            const char *headers[] = {auth};
            evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
        }
        if (!r->inflight) complete(r, 0);
    }
}

void tube_cancel(void) { tube_recovery_finish(); cancel_requests(0); }

static void folder(evo_provider_item_t *out, const char *id, const char *title, const char *hint) {
    memset(out, 0, sizeof *out);
    copy(out->id, sizeof out->id, id); copy(out->title, sizeof out->title, title);
    copy(out->subtitle, sizeof out->subtitle, hint); out->is_folder = 1; out->kind = EVO_MEDIA_VIDEO;
    embedded_wording(out->title, sizeof out->title); embedded_wording(out->subtitle, sizeof out->subtitle);
    out->tube_interface = 1;   /* made of interface texts: the browser shows it in the interface language */
}

/* Menus can exceed a 24-item media page. Reject growth before writing past the
 * fixed menu buffer; callbacks receive no partial menu on capacity failure. */
#define CATALOG_FOLDER(id, title, detail) do { \
    if (n >= (int)(sizeof out / sizeof out[0])) { \
        tube_set_error("This menu exceeds its item capacity."); cb(0,NULL,0,0,ud); return 1; \
    } \
    folder(&out[n++],id,title,detail); \
} while (0)
/* 1 = a section of a channel ("channel-tab:UC...:section[:order]"); sort is -1 without an order. */
static int channel_tab_parse(const char *parent, int *tab, int *sort) {
    char name[16];
    if (!parent || strncmp(parent, "channel-tab:", 12) || !*channel_of(parent)) return 0;
    const char *value = parent + 37, *colon = strchr(value, ':');
    const size_t length = colon ? (size_t)(colon - value) : strlen(value);
    if (!length || length >= sizeof name) return 0;
    memcpy(name, value, length); name[length] = 0;
    *tab = channel_name_index(g_channel_tabs, 5, name);
    *sort = colon ? channel_name_index(g_channel_sorts, 3, colon + 1) : -1;
    return *tab >= 0 && (!colon || *sort >= 0);
}
/* Arm the text prompt for one purpose; any earlier prompt or account intent is dropped. */
static void prompt_arm(const char *label) {
    memset(&g_account_intent, 0, sizeof g_account_intent); memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
    if (!++g_account_serial) ++g_account_serial;
    g_playlist_prompt.prompt.ticket = g_account_serial;
    copy(g_playlist_prompt.prompt.label, sizeof g_playlist_prompt.prompt.label, label);
    copy(g_playlist_prompt.profile, sizeof g_playlist_prompt.profile, g_library_profile);
    g_playlist_prompt.pending = g_playlist_prompt.active = 1;
}
/* "settings:ui:<group>" or "settings:ui:<group>:<choice>" -> the interface
 * group, -1 when the id names none. choice is -1 for the group itself and -2
 * for a malformed one. */
static int ui_group_parent(const char *parent, int *choice) {
    char id[64]; *choice = -1;
    /* The classic colour schemes have a list of their own under Theme: choice -3. */
    if (!strcmp(parent, "settings:ui:scheme:classic")) { *choice = -3; return TUBE_UI_GROUP_SCHEME; }
    int group = tube_ui_group_find(parent);
    const char *colon = strrchr(parent, ':');
    if (group >= 0 || !colon || (size_t)(colon - parent) >= sizeof id) return group;
    memcpy(id, parent, (size_t)(colon - parent)); id[colon - parent] = 0;
    if ((group = tube_ui_group_find(id)) < 0) return -1;
    char *end = NULL; long value = isdigit((unsigned char)colon[1]) ? strtol(colon + 1, &end, 10) : -1;
    *choice = value >= 0 && !*end && value < tube_ui_group_count(group) ? (int)value : -2;
    return group;
}
/* "settings:menu:<key>" or "settings:menu:<key>:<verb>" -> the menu key, -1
 * when the id names none. verb is "" for the key itself. */
static int ui_menu_parent(const char *parent, const char **verb) {
    char id[32]; *verb = "";
    if (strncmp(parent, "settings:menu:", 14)) return -1;
    const char *value = parent + 14, *colon = strchr(value, ':');
    const size_t length = colon ? (size_t)(colon - value) : strlen(value);
    if (!length || length >= sizeof id) return -1;
    memcpy(id, value, length); id[length] = 0;
    if (colon) *verb = colon + 1;
    return tube_ui_menu_find(id);
}
/* The video actions menu as the profile arranged it: hidden rows leave, the
 * rest follow its order. Returns the new count. */
static int queue_compact(Record *ordered[RECORDS]) {
    int count = 0;
    for (int i = 0; i < g_count; ++i) if (g_records[i].queue_order > 0 || g_records[i].queued) {
        Record *r = &g_records[i]; int at = count;
        while (at > 0 && ordered[at-1]->queue_order > r->queue_order) { ordered[at] = ordered[at-1]; --at; }
        ordered[at] = r; ++count;
    }
    for (int i = 0; i < count; ++i) ordered[i]->queue_order = i + 1;
    g_queue_order = count;
    return count;
}
static Record *queue_neighbor(Record *target, int direction) {
    Record *neighbor = NULL;
    for (int i = 0; target && i < g_count; ++i) {
        Record *r = &g_records[i];
        if (r == target || !r->queued || !current_record(r) || blocked_identity(r->item.parent_id,r->item.subtitle) >= 0) continue;
        const int earlier = r->queue_order < target->queue_order || (r->queue_order == target->queue_order && r < target);
        if (earlier != (direction < 0)) continue;
        if (!neighbor || (direction < 0 ? r->queue_order > neighbor->queue_order || (r->queue_order == neighbor->queue_order && r > neighbor)
                                      : r->queue_order < neighbor->queue_order || (r->queue_order == neighbor->queue_order && r < neighbor))) neighbor = r;
    }
    return neighbor;
}
static int ui_menu_arrange(evo_provider_item_t *rows, int count) {
    static evo_provider_item_t arranged[64];
    int keys[64], index[64];
    if (count > 64) return count;
    for (int i = 0; i < count; ++i) keys[i] = tube_ui_menu_key(rows[i].id);
    const int kept = tube_ui_menu_arrange(&g_ui, keys, count, index);
    for (int i = 0; i < kept; ++i) arranged[i] = rows[index[i]];
    memcpy(rows, arranged, (size_t)kept * sizeof *rows);
    return kept;
}
/* Player extras by settings screen, in the order of tube_extra_group_screen():
 * captions, sound, controls and keys, player buttons, screen dimming. */
static const char *const g_extra_screens[] = {"settings:extra:captions", "settings:extra:audio", "settings:extra:player",
    "settings:extra:buttons", "settings:extra:dimming"};
#define EXTRA_SCREENS ((int)(sizeof g_extra_screens / sizeof g_extra_screens[0]))
static int extra_screen(int group) { return tube_extra_group_screen(group); }
/* "settings:<group>" or "settings:<group>:<choice>" -> the group, -1 when the
 * id names none. choice is -1 for the group itself and -2 for a malformed one. */
static int extra_group_parent(const char *parent, int *choice) {
    char id[64]; *choice = -1;
    int group = tube_extra_group_find(parent);
    const char *colon = strrchr(parent, ':');
    if (group >= 0 || !colon || colon == parent || (size_t)(colon - parent) >= sizeof id) return group;
    memcpy(id, parent, (size_t)(colon - parent)); id[colon - parent] = 0;
    if ((group = tube_extra_group_find(id)) < 0) return -1;
    char *end = NULL; long value = isdigit((unsigned char)colon[1]) ? strtol(colon + 1, &end, 10) : -1;
    *choice = value >= 0 && !*end && value < tube_extra_group_count(group) ? (int)value : -2;
    return group;
}
#define LINK_ROW(id, title, detail, url) do { \
    CATALOG_FOLDER(id,title,detail); copy(out[n-1].overview,sizeof out[n-1].overview,url); \
} while (0)
static int local_catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t out[64]; int n = 0;
    if (parent && tube_i18n_select && !strncmp(parent, TUBE_I18N_SETTING, sizeof TUBE_I18N_SETTING - 1)) {
        /* Interface language: tube_i18n.c keeps the choice and names the rows. */
        if (tube_i18n_select(parent) < 0) { tube_set_error("Select a listed language."); cb(0,NULL,0,0,ud); return 1; }
        for (int i = page ? tube_i18n_choices() : 0; i < tube_i18n_choices(); ++i)
            CATALOG_FOLDER(tube_i18n_choice_id(i), tube_i18n_choice_title(i), tube_i18n_choice_detail(i));
        cb(1, out, n, 0, ud); return 1;
    }
    if(parent && !strncmp(parent,"link:",5)) {
        /* Addresses to open on another device: a row's overview is its link,
         * which the screen shows as text and as a QR code. */
        char url[TUBE_QR_PAYLOAD_SIZE];const char *value=parent+5;
        if(!page && !strncmp(value,"video:",6) && valid_id(value+6)) {
            const Record *video=record(value+6,0);const int occurrence=video_occurrence(value+6);
            if(!tube_qr_video_url(value+6,0,url,sizeof url))LINK_ROW("link:row:video","Video link",url,url);
            snprintf(url,sizeof url,"https://www.youtube.com/embed/%s",value+6);
            LINK_ROW("link:row:embed","Embed link",url,url);
            if(video && !tube_qr_channel_url(video->item.parent_id,url,sizeof url))LINK_ROW("link:row:channel","Channel link",url,url);
            if(occurrence>=0 && !tube_qr_playlist_url(g_occurrences[occurrence].playlist,url,sizeof url))LINK_ROW("link:row:playlist","Playlist link",url,url);
        } else if(!page && !strncmp(value,"channel:",8) && valid_channel(value+8) && !tube_qr_channel_url(value+8,url,sizeof url)) {
            LINK_ROW("link:row:channel","Channel link",url,url);
        } else if(!page && !strncmp(value,"playlist:",9) && valid_playlist(value+9) && !tube_qr_playlist_url(value+9,url,sizeof url)) {
            LINK_ROW("link:row:playlist","Playlist link",url,url);
        } else if(!page && (!strcmp(value,"about") || !strcmp(value,"feedback"))) {
            const tube_about_link *link;
            for(int i=0;(link=tube_about_link_at(i))!=NULL;++i)if(link->feedback==(value[0]=='f')) {
                char id[64];snprintf(id,sizeof id,"link:row:%s",link->id);
                LINK_ROW(id,link->title,link->detail,link->url);
            }
        }
        else if(!page && !strncmp(value,"row:",4))CATALOG_FOLDER("account:notice","Circle to return","Scan the code or type the address on another device");
        if(!n){tube_set_error("Select a video, channel or playlist to share.");cb(0,NULL,0,0,ud);return 1;}
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && (!strncmp(parent,"reminder:confirm:",17) || !strncmp(parent,"playlist-order:confirm:",23) || !strncmp(parent,"notify:",7))) {
        /* Reminder, playlist order and notification level: a review screen names
         * the change; its one row submits it once. */
        const char *value=strstr(parent,":confirm:"),*action=NULL,*identity="",*playlist="",*title="",*argument="",*prefix="";
        if(page){tube_set_error("Account action screens do not have additional pages.");cb(0,NULL,0,0,ud);return 1;}
        if(!strcmp(g_library_profile,"guest")) {
            CATALOG_FOLDER("account:notice","Choose an account profile first","Discover > Accounts & library > Profile");
            cb(1,out,n,0,ud);return 1;
        }
        if(!value && valid_channel(parent+7)) { /* what the selected account can ask of this channel */
            for(int i=0;i<6;++i) {
                char id[80],label[96];snprintf(id,sizeof id,"notify:confirm:%s:%s:%s",i<3?"subscribe":"level",g_notify_levels[i%3],parent+7);
                snprintf(label,sizeof label,"%s: %s",i<3?"Subscribe":"Notifications",g_notify_labels[i%3]);
                CATALOG_FOLDER(id,label,i<3?"Subscribes on YouTube with this notification level; asks first":
                    "For a channel this account already subscribes to; asks first");
            }
            cb(1,out,n,0,ud);return 1;
        }
        value=value?value+9:"";
        const char *colon=strchr(value,':');
        if(parent[0]=='r') {
            if(colon && valid_id(colon+1) && (!strncmp(value,"set:",4) || !strncmp(value,"unset:",6))) {
                action="stream_reminder";identity=colon+1;argument=value[0]=='s'?"set":"unset";
                title=value[0]=='s'?"Set stream reminder":"Remove stream reminder";
            }
        } else if(parent[0]=='p') {
            for(int i=0;colon && i<5;++i)
                if(strlen(g_playlist_orders[i])==(size_t)(colon-value) && !strncmp(value,g_playlist_orders[i],(size_t)(colon-value)) && valid_playlist(colon+1)) {
                    action="playlist_order";playlist=colon+1;argument=g_playlist_orders[i];title=g_playlist_order_labels[i];prefix="Sort playlist: ";
                }
        } else {
            const int subscribe=!strncmp(value,"subscribe:",10);
            const char *level=subscribe?value+10:!strncmp(value,"level:",6)?value+6:"",*channel=strchr(level,':');
            for(int i=0;channel && i<3;++i)
                if(strlen(g_notify_levels[i])==(size_t)(channel-level) && !strncmp(level,g_notify_levels[i],(size_t)(channel-level)) && valid_channel(channel+1)) {
                    action=subscribe?"subscribe":"subscribe_notifications";identity=channel+1;argument=g_notify_levels[i];
                    title=g_notify_labels[i];prefix=subscribe?"Subscribe: ":"Notifications: ";
                }
        }
        if(!action){tube_set_error("The selected account action was invalid.");cb(0,NULL,0,0,ud);return 1;}
        char label[160],id[80],hint[160];
        snprintf(label,sizeof label,"%s%s",prefix,title);
        memset(&g_playlist_prompt,0,sizeof g_playlist_prompt);
        const unsigned serial=account_intent(action,identity,playlist,"");
        copy(g_account_intent.argument,sizeof g_account_intent.argument,argument);
        snprintf(id,sizeof id,"account:apply:%u",serial);snprintf(hint,sizeof hint,"Confirm for YouTube profile: %s",g_library_profile);
        CATALOG_FOLDER(id,label,hint);
        snprintf(out[0].overview,sizeof out[0].overview,"%s. %s%s. Cross applies this change to YouTube once; Circle returns without submitting it.",
            label,*identity?"Selected: ":"Playlist: ",*identity?identity:playlist);
        copy(g_comments[0].id,sizeof g_comments[0].id,id);copy(g_comments[0].content,sizeof g_comments[0].content,out[0].overview);
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && (!strncmp(parent,"comment:post:",13) || !strncmp(parent,"comment:reply:",14) || !strncmp(parent,"comment:review:",15))) {
        /* A comment or a reply: typed in the text prompt, reviewed, then posted by one confirmation. */
        const int reply=parent[10]=='p',review=parent[10]=='v';
        if(page || !strcmp(g_library_profile,"guest")) {
            tube_set_error("Select an account profile before writing a comment.");cb(0,NULL,0,0,ud);return 1;
        }
        if(review) {
            char expected[80],id[80];snprintf(expected,sizeof expected,"comment:review:%u",g_account_intent.serial);
            const int answering=!strcmp(g_account_intent.action,"comment_reply");
            if(!g_account_intent.serial || strcmp(parent,expected) || strcmp(g_account_intent.profile,g_library_profile) ||
               (!answering && strcmp(g_account_intent.action,"comment_post")) || !*g_account_intent.title) {
                tube_set_error("Reopen the comments to write one for the current profile.");cb(0,NULL,0,0,ud);return 1;
            }
            snprintf(id,sizeof id,"account:apply:%u",g_account_intent.serial);
            CATALOG_FOLDER(id,answering?"Post this reply":"Post this comment","Cross posts once for the selected YouTube profile; Circle cancels");
            copy(g_comments[LOCAL_PAGE].id,sizeof g_comments[LOCAL_PAGE].id,id);
            snprintf(g_comments[LOCAL_PAGE].content,sizeof g_comments[LOCAL_PAGE].content,
                "Profile: %s\nVideo: %s\n%s:\n%s\nCross posts once; Circle returns without posting.",
                g_account_intent.profile,g_account_intent.identity,answering?"Reply":"Comment",g_account_intent.title);
            copy(out[0].overview,sizeof out[0].overview,g_comments[LOCAL_PAGE].content);
        } else {
            char reference[65]="";int known=0;
            if(!reply)known=valid_id(parent+13) && comment_thread_current() && !strcmp(parent+13,g_comment_thread.video) && g_comment_thread.can_post;
            else if(valid_cursor(parent+14) && comment_thread_current())for(int i=0;i<LOCAL_PAGE;++i)
                if(!strcmp(g_comments[i].respond,parent+14) && !strcmp(g_comments[i].profile,g_library_profile)) {
                    known=1;copy(reference,sizeof reference,g_comments[i].respond);
                }
            if(!known){tube_set_error("Reload the comments for the current profile before writing one.");cb(0,NULL,0,0,ud);return 1;}
            prompt_arm(reply?"Reply to this comment":"Comment on this video");
            g_playlist_prompt.comment=reply?2:1;
            copy(g_playlist_prompt.identity,sizeof g_playlist_prompt.identity,g_comment_thread.video);
            copy(g_playlist_prompt.cursor,sizeof g_playlist_prompt.cursor,reference);
            CATALOG_FOLDER("account:notice",reply?"Enter your reply":"Enter your comment","Review it before posting; Circle returns without posting");
        }
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && (!strcmp(parent,"channels:menu") || !strcmp(parent,"channels:watched"))) {
        /* Touchpad on Subscribed channels: its order, and the marks of the list last shown. */
        char label[96];
        if(page || !strcmp(g_library_profile,"guest")) {
            CATALOG_FOLDER("account:notice","Choose an account profile first","Discover > Accounts & library > Profile");
        } else if(parent[9]=='w') {
            int marked=0;
            if(g_channel_new.scope==g_playlist_scope && !strcmp(g_channel_new.profile,g_library_profile))
                for(int i=0;i<g_channel_new.count;++i) {channel_mark_put(g_channel_new.channel[i],"");++marked;}
            g_channel_new.count=0;g_channel_list.complete=0;
            if(g_channel_marks_dirty)save();
            snprintf(label,sizeof label,"%d channel%s marked as watched",marked,marked==1?"":"s");
            CATALOG_FOLDER("account:notice",marked?label:"No channel is marked as new",
                marked?"Reload Subscribed channels to see the list without their marks":"Open Subscribed channels first: the marks come with its list");
        } else {
            snprintf(label,sizeof label,"Order: %s",g_channel_order_labels[g_channel_sort]);
            CATALOG_FOLDER("settings:channel-sort",label,"Account order, by name, or channels with new content first");
            CATALOG_FOLDER("channels:watched","Mark all channels as watched","Clear the new-content marks of the list last shown");
            CATALOG_FOLDER("settings:refresh","Refresh section","Reload the list of subscribed channels");
        }
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && !strncmp(parent,"channel-menu:",13)) {
        char group[48];snprintf(group,sizeof group,"channel:%s",parent+13);
        if(page || !valid_channel_group(group) || !channel_facts_current(parent+13)) {
            tube_set_error("Reopen this channel to list its sections.");cb(0,NULL,0,0,ud);return 1;
        }
        int orders=0;
        for(int i=0;i<3;++i)orders+=g_channel_facts.sorts>>i&1;
        for(int i=0;i<5;++i)if(g_channel_facts.tabs>>i&1) {
            char id[64];snprintf(id,sizeof id,"channel-tab:%s:%s",parent+13,g_channel_tabs[i]);
            CATALOG_FOLDER(id,g_channel_tab_labels[i],i==g_channel_facts.tab?"Section last opened":"Section of this channel");
        }
        for(int i=0;i<3 && orders>1 && g_channel_facts.tab>=0;++i)if(g_channel_facts.sorts>>i&1) {
            char id[80],label[64];snprintf(id,sizeof id,"channel-tab:%s:%s:%s",parent+13,g_channel_tabs[g_channel_facts.tab],g_channel_sorts[i]);
            snprintf(label,sizeof label,"%s: %s",g_channel_tab_labels[g_channel_facts.tab],g_channel_sort_labels[i]);
            CATALOG_FOLDER(id,label,i==g_channel_facts.sort?"Selected":"Order offered for this section");
        }
        if(g_channel_facts.search) {
            char id[48];snprintf(id,sizeof id,"channel-search:%s",parent+13);
            CATALOG_FOLDER(id,"Search in channel","Enter text; results list this channel's videos and playlists");
        }
        if(!n)CATALOG_FOLDER("account:notice","This channel offers no other section","Circle returns to its videos");
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && !strncmp(parent,"channel-search:",15) && strncmp(parent,"channel-search:run:",19)) {
        char group[48];snprintf(group,sizeof group,"channel:%s",parent+15);
        if(page || !valid_channel_group(group) || !channel_facts_current(parent+15) || !g_channel_facts.search) {
            tube_set_error("Reopen this channel before searching in it.");cb(0,NULL,0,0,ud);return 1;
        }
        prompt_arm("Search in this channel");
        copy(g_playlist_prompt.identity,sizeof g_playlist_prompt.identity,group);g_playlist_prompt.search=1;
        CATALOG_FOLDER("account:notice","Enter text to search in this channel","Circle returns to the channel sections");
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && !strncmp(parent,"block:",6)) {
        int index=-1;
        for(int i=0;i<g_blocked_count;++i)if(!strcmp(g_blocked[i].profile,g_library_profile)) {
            char expected[80];snprintf(expected,sizeof expected,"block:%u:%u:%d",g_playlist_scope,g_blocked_revision,i);
            if(!strcmp(parent,expected))index=i;
        }
        if(page || index<0 || !change_channel_block(g_blocked[index].channel,g_blocked[index].name,0)) {
            tube_set_error("Reopen blocked channels for the current profile.");cb(0,NULL,0,0,ud);return 1;
        }
        parent="blocked-channels";
    }
    if(parent && !strcmp(parent,"blocked-channels")) {
        int total=0;
        for(int i=0;i<g_blocked_count;++i)if(!strcmp(g_blocked[i].profile,g_library_profile)) {
            if(total++<page*LOCAL_PAGE || n>=LOCAL_PAGE)continue;
            char id[80];snprintf(id,sizeof id,"block:%u:%u:%d",g_playlist_scope,g_blocked_revision,i);
            CATALOG_FOLDER(id,g_blocked[i].name,"Cross unblocks this channel for the selected profile");
        }
        cb(1,out,n,total>(page+1)*LOCAL_PAGE,ud);return 1;
    }
    if(parent && !strcmp(parent,"description:update")) {
        /* The newest release's notes in the long-text reader. */
        const char *notes=tube_update_app_notes?tube_update_app_notes():"",*title=tube_update_app_notes_title?tube_update_app_notes_title():"";
        memset(&g_description,0,sizeof g_description);
        copy(g_description.id,sizeof g_description.id,"description:text:update");
        copy(g_description.content,sizeof g_description.content,*notes?notes:"This release has no notes.");
        copy(g_description.profile,sizeof g_description.profile,g_library_profile);g_description.scope=g_playlist_scope;
        CATALOG_FOLDER(g_description.id,*title?title:"Release notes","Release notes; L2 / R2 scroll");
        copy(out[n-1].overview,sizeof out[n-1].overview,g_description.content);
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && !strncmp(parent,"description:text:",17)) {
        CATALOG_FOLDER("account:notice","Circle to return","L2 / R2 scroll the description");
        cb(1,out,n,0,ud);return 1;
    }
    if(parent && !strncmp(parent,"search-history",14)) {
        /* Rows are named by profile-checked serials: a stale or foreign row never
         * changes another list, and reloading a finished action cannot repeat it. */
        char clear[48];snprintf(clear,sizeof clear,"search-history:clear:%u",g_playlist_scope);
        int removing=!strcmp(parent,"search-history:remove");
        if(!strncmp(parent,"search-history:forget:",22)) {
            int index=search_history_find(parent+22);
            if(!page && index<0) {
                tube_set_error("Reopen recent searches for the current profile.");cb(0,NULL,0,0,ud);return 1;
            }
            if(!page) {search_history_drop(index);save();}
            removing=1;
        } else if(!strncmp(parent,"search-history:clear:",21)) {
            if(page || strcmp(parent,clear)) {
                tube_set_error("Reopen recent searches for the current profile.");cb(0,NULL,0,0,ud);return 1;
            }
            for(int i=g_search_count-1;i>=0;--i)if(!strcmp(g_searches[i].profile,g_library_profile))search_history_drop(i);
            save();
        } else if(strcmp(parent,"search-history") && !removing) {
            tube_set_error("Reopen recent searches for the current profile.");cb(0,NULL,0,0,ud);return 1;
        }
        if(!g_search_history)CATALOG_FOLDER("account:notice","Search history is off","Turn it on in YouTube preferences for this profile");
        else if(!search_history_count())CATALOG_FOLDER("account:notice","No recent searches","Square searches; queries are saved for the selected profile");
        else {
            if(!removing) {
                CATALOG_FOLDER("search-history:remove","Remove a recent search","Choose one query to forget");
                CATALOG_FOLDER(clear,"Clear recent searches","Forget every query saved for the selected profile");
            }
            for(int i=0;i<g_search_count;++i)if(!strcmp(g_searches[i].profile,g_library_profile)) {
                char id[64];snprintf(id,sizeof id,"search-history:%s:%u",removing?"forget":"run",g_searches[i].serial);
                CATALOG_FOLDER(id,g_searches[i].query,removing?"Cross forgets this query":"Cross searches again");
            }
        }
        int offset=page*LOCAL_PAGE,count=offset<n?n-offset:0;
        if(count>LOCAL_PAGE)count=LOCAL_PAGE;
        cb(1,count?out+offset:NULL,count,offset+count<n,ud);return 1;
    }
    if(parent && !strncmp(parent,"local:queue-move:",17)) {
        int direction = !strcmp(parent,g_queue_move.rows[0]) ? -1 : !strcmp(parent,g_queue_move.rows[1]) ? 1 : 0;
        Record *target = record(g_queue_move.video,0);
        Record *peer = direction ? queue_neighbor(target,direction) : NULL;
        if (page || !direction || g_queue_move.scope != g_playlist_scope || strcmp(g_queue_move.profile,g_library_profile) ||
            !target || !target->queued || blocked_identity(target->item.parent_id,target->item.subtitle) >= 0 ||
            !peer || strcmp(peer->item.id,g_queue_move.peers[direction > 0])) {
            tube_set_error("Reopen video actions for the current profile."); cb(0,NULL,0,0,ud); return 1;
        }
        g_queue_move.rows[0][0] = g_queue_move.rows[1][0] = 0; /* A completed action cannot move twice when reloaded. */
        Record *ordered[RECORDS]; queue_compact(ordered);
        long long order = target->queue_order; target->queue_order = peer->queue_order; peer->queue_order = order;
        tube_playlist_eof_cancel(); tube_queue_retry_cancel(); save();
        CATALOG_FOLDER("account:notice",direction < 0 ? "Moved up in queue" : "Moved down in queue","Circle returns to video actions");
        cb(1,out,n,0,ud); return 1;
    }
    if(parent && !strncmp(parent,"local:video:",12)) {
        const char *id=strrchr(parent,':');id=id?id+1:"";
        const char *actions[]={"next","watched","forget","block","unblock","restart","incognito","share","sbexclude","sbinclude"};
        const char *messages[]={"Video will play next","Marked as watched on this PS5","Removed from local history","Channel blocked","Channel unblocked",
            "","","","SponsorBlock is off for this channel","SponsorBlock is on for this channel"};
        int action=-1;
        for(int i=0;valid_id(id) && i<10;++i) {
            char expected[100];snprintf(expected,sizeof expected,"local:video:%u:%s:%s",g_playlist_scope,actions[i],id);
            if(!strcmp(parent,expected))action=i;
        }
        if(page || action<0 || !record(id,0)) {
            tube_set_error("Reopen video actions for the current profile.");cb(0,NULL,0,0,ud);return 1;
        }
        Record *video=record(id,0);
        if(action==5 || action==6) {
            /* The action starts its video once per menu visit, through the host's
             * next-video poll. Reloading this screen after that playback only
             * offers the playable row: Cross starts it again. */
            int replayed=0;
            const int first=strcmp(g_menu_launch.offered,id) || g_menu_launch.offered_action!=action;
            copy(g_menu_launch.offered,sizeof g_menu_launch.offered,id);g_menu_launch.offered_action=action;
            if(first) {
                copy(g_menu_launch.id,sizeof g_menu_launch.id,id);
                copy(g_menu_launch.profile,sizeof g_menu_launch.profile,g_library_profile);g_menu_launch.scope=g_playlist_scope;
            }
            if(action==5) {
                replayed=!strcmp(g_restart.id,id) && g_restart.scope==g_playlist_scope && g_restart.selection!=g_selection_serial;
                if(!replayed) {
                    video->item.resume_pos_sec=0;save();
                    copy(g_restart.id,sizeof g_restart.id,id);g_restart.scope=g_playlist_scope;g_restart.selection=g_selection_serial;
                }
            } else {
                copy(g_incognito.id,sizeof g_incognito.id,id);
                copy(g_incognito.profile,sizeof g_incognito.profile,g_library_profile);g_incognito.scope=g_playlist_scope;
            }
            out[n]=video->item;dearrow_item(&out[n],video);
            copy(out[n].subtitle,sizeof out[n].subtitle,action==6?"Cross plays without history or account reports":
                replayed?"Cross resumes this video":"Cross plays from the start");
            ++n;cb(1,out,n,0,ud);return 1;
        }
        if(action==7) { /* an id from before the link page: the same links */
            char link[32];snprintf(link,sizeof link,"link:video:%s",id);
            return local_catalog(link,page,cb,ud);
        }
        if(action>=8 && !tube_sb_channel_valid(video->item.parent_id)) {
            tube_set_error("This video has no channel identity to exclude.");cb(0,NULL,0,0,ud);return 1;
        }
        int ok=action==0?tube_queue_play_next(id):action==1?tube_mark_watched(id):action==2?tube_remove_history(id):
            action>=8?sponsor_exclude(video->item.parent_id,action==8):
            change_channel_block(video->item.parent_id,video->item.subtitle,action==3);
        if(action>=3 && !ok){cb(0,NULL,0,0,ud);return 1;}
        CATALOG_FOLDER("account:notice",ok>0?messages[action]:"This video is already playing","Circle returns to video actions");
        cb(1,out,n,0,ud);return 1;
    }
    if (parent && !strncmp(parent, "message:", 8)) {
        if (g_chat.replay) {
            tube_set_error("Chat replay is read-only."); cb(0, NULL, 0, 0, ud); return 1;
        }
        if (page || !strcmp(g_library_profile, "guest") || g_chat.state != 2 || !valid_cursor(g_chat.cursor) ||
            strcmp(g_chat.profile, g_library_profile) || strcmp(g_chat.source, g_source) || strcmp(g_chat.token, g_token)) {
            tube_set_error("Open an active chat with an account profile before composing a message."); cb(0, NULL, 0, 0, ud); return 1;
        }
        if (!strcmp(parent, "message:compose")) {
            memset(&g_message, 0, sizeof g_message);
            if (!++g_account_serial) ++g_account_serial;
            g_message.ticket = g_account_serial; g_message.chat_serial = g_chat.serial;
            g_message.phase = g_message.pending = 1;
            copy(g_message.video, sizeof g_message.video, g_chat.video);
            copy(g_message.profile, sizeof g_message.profile, g_library_profile);
            copy(g_message.cursor, sizeof g_message.cursor, g_chat.cursor);
            snprintf(g_message.session, sizeof g_message.session, "chat-%lld-%u", now_ms(), g_message.ticket);
            CATALOG_FOLDER( "account:notice", "Compose a chat message" EXPERIMENTAL, "Review before sending; Circle cancels"); experimental(&out[n - 1]);
        } else {
            char review[80], send[80];
            snprintf(review, sizeof review, "message:review:%u", g_message.ticket);
            snprintf(send, sizeof send, "message:send:%u", g_message.ticket);
            if (!message_scope() || (strcmp(parent, review) && strcmp(parent, send)) || g_message.phase < 2) {
                tube_set_error("Reopen chat to compose a new message for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            if (!strcmp(parent, send) && g_message.phase == 2) return 0;
            if (g_message.phase == 2) {
                CATALOG_FOLDER( send, "Send this chat message", "Cross sends once; Circle cancels");
                copy(g_comments[LOCAL_PAGE].id, sizeof g_comments[LOCAL_PAGE].id, send);
                snprintf(g_comments[LOCAL_PAGE].content, sizeof g_comments[LOCAL_PAGE].content,
                    "Profile: %s\nVideo: %s\nMessage:\n%s\nCross sends once; Circle returns without sending.",
                    g_message.profile, g_message.video, g_message.text);
                copy(out[0].overview, sizeof out[0].overview, g_comments[LOCAL_PAGE].content);
            } else CATALOG_FOLDER( "account:notice", g_message.phase == 4 ? "Chat message sent" : "Chat message pending",
                          "Check the chat before composing another message");
        }
        cb(1, out, n, 0, ud); return 1;
    }
    if (parent && !strncmp(parent, "comment:", 8)) {
        const char *cursor = !strncmp(parent, "comment:menu:", 13) ? parent + 13 :
            !strncmp(parent, "comment:confirm:", 16) ? strchr(parent + 16, ':') : NULL;
        const int reviewing = !strncmp(parent, "comment:confirm:", 16);
        if (reviewing && cursor) ++cursor;
        int found = -1;
        if (valid_cursor(cursor) && strcmp(g_library_profile, "guest"))
            for (int i = 0; i < LOCAL_PAGE; ++i)
                if (!strcmp(g_comments[i].rating, cursor) && !strcmp(g_comments[i].profile, g_library_profile)) found = i;
        if (page || found < 0) {
            tube_set_error("Reload the comments for the current profile before rating a comment.");
            cb(0, NULL, 0, 0, ud); return 1;
        }
        const char *ratings[] = {"like", "dislike", "none"};
        const char *titles[] = {"Like comment", "Dislike comment", "Remove comment rating"};
        if (!reviewing) {
            for (int i = 0; i < 3; ++i) if (g_comments[found].rating_options & (1u << i)) {
                char id[160]; snprintf(id, sizeof id, "comment:confirm:%s:%s", ratings[i], cursor);
                CATALOG_FOLDER( id, titles[i], "Review the selected comment before confirming");
            }
            if (*g_comments[found].respond) {
                char id[160]; snprintf(id, sizeof id, "comment:reply:%s", g_comments[found].respond);
                CATALOG_FOLDER( id, "Reply to comment" EXPERIMENTAL, "Enter text, then review it before posting"); experimental(&out[n - 1]);
            }
        } else {
            int choice = -1;
            for (int i = 0; i < 3; ++i) {
                char expected[160]; snprintf(expected, sizeof expected, "comment:confirm:%s:%s", ratings[i], cursor);
                if (!strcmp(expected, parent) && (g_comments[found].rating_options & (1u << i))) choice = i;
            }
            if (choice < 0) { tube_set_error("Select an available comment rating."); cb(0, NULL, 0, 0, ud); return 1; }
            char content[16001]; copy(content, sizeof content, g_comments[found].content);
            unsigned serial = account_intent("comment_rate", ratings[choice], cursor, "");
            char id[80]; snprintf(id, sizeof id, "account:apply:%u", serial);
            CATALOG_FOLDER( id, titles[choice], "Cross confirms for the selected YouTube profile; Circle cancels");
            copy(g_comments[LOCAL_PAGE].id, sizeof g_comments[LOCAL_PAGE].id, id);
            snprintf(g_comments[LOCAL_PAGE].content, sizeof g_comments[LOCAL_PAGE].content, "Profile: %s\nComment:\n%s\nCross confirms; Circle returns without submitting it.",
                     g_library_profile, content);
            copy(out[0].overview, sizeof out[0].overview, g_comments[LOCAL_PAGE].content);
        }
        cb(1, out, n, 0, ud); return 1;
    }
    if (parent && !strncmp(parent, "playlist:", 9)) {
        if (!strchr(parent + 9, ':')) return 0; /* ordinary public playlist browsing */
        if (!strncmp(parent, "playlist:pick:", 14)) return 0;
        if (!strcmp(g_library_profile, "guest")) {
            CATALOG_FOLDER( "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
        } else if (!strncmp(parent, "playlist:menu:", 14) && valid_playlist(parent + 14) &&
            strcmp(parent + 14, "WL") && strcmp(parent + 14, "LL")) {
            char id[160]; snprintf(id, sizeof id, "playlist:prompt:rename:%.100s", parent + 14);
            CATALOG_FOLDER( id, "Rename YouTube playlist", "Enter a new name, then confirm");
            snprintf(id, sizeof id, "playlist:confirm:delete:%.100s", parent + 14);
            CATALOG_FOLDER( id, "Delete YouTube playlist", "Requires confirmation for the selected profile");
        } else if (!strncmp(parent, "playlist:prompt:", 16)) {
            int create = !strncmp(parent, "playlist:prompt:create:", 23);
            int rename = !strncmp(parent, "playlist:prompt:rename:", 23);
            const char *identity = (create || rename) ? parent + 23 : "";
            if ((!create && !rename) || (create && *identity && !valid_id(identity)) ||
                (rename && (!valid_playlist(identity) || !strcmp(identity, "WL") || !strcmp(identity, "LL")))) {
                tube_set_error("Select a valid playlist or video."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            if (!++g_account_serial) ++g_account_serial;
            g_playlist_prompt.prompt.ticket = g_account_serial;
            copy(g_playlist_prompt.prompt.label, sizeof g_playlist_prompt.prompt.label,
                create ? "New private YouTube playlist name" : "New YouTube playlist name");
            copy(g_playlist_prompt.identity, sizeof g_playlist_prompt.identity, identity);
            copy(g_playlist_prompt.profile, sizeof g_playlist_prompt.profile, g_library_profile);
            g_playlist_prompt.create = create; g_playlist_prompt.pending = g_playlist_prompt.active = 1;
            CATALOG_FOLDER( "account:notice", "Enter a playlist name", "Circle returns without changing your account");
        } else if (!strncmp(parent, "playlist:confirm:", 17)) {
            const char *value = parent + 17, *identity = NULL, *playlist = NULL, *action = NULL;
            char video[12] = "";
            if (!strncmp(value, "delete:", 7)) { playlist = value + 7; action = "playlist_delete"; }
            else if (!strncmp(value, "save:", 5)) { playlist = value + 5; action = "playlist_save"; }
            else if (!strncmp(value, "unsave:", 7)) { playlist = value + 7; action = "playlist_unsave"; }
            else {
                int add = !strncmp(value, "add:", 4), remove = !strncmp(value, "remove:", 7);
                identity = add ? value + 4 : remove ? value + 7 : "";
                if ((add || remove) && strlen(identity) > 12 && identity[11] == ':') {
                    memcpy(video, identity, 11); playlist = identity + 12;
                    action = add ? "playlist_add" : "playlist_remove";
                }
            }
            const int whole = action && strcmp(action, "playlist_add") && strcmp(action, "playlist_remove");
            if (!action || !valid_playlist(playlist) ||
                (!whole ? !valid_id(video) : (!strcmp(playlist, "WL") || !strcmp(playlist, "LL")))) {
                tube_set_error("The playlist change was invalid."); cb(0, NULL, 0, 0, ud); return 1;
            }
            unsigned serial = account_intent(action, video, playlist, "");
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            CATALOG_FOLDER( id, !strcmp(action, "playlist_delete") ? "Delete this YouTube playlist" :
                !strcmp(action, "playlist_save") ? "Save this playlist to your library" :
                !strcmp(action, "playlist_unsave") ? "Remove this playlist from your library" :
                !strcmp(action, "playlist_add") ? "Add video to this playlist" : "Remove video from this playlist", hint);
            snprintf(out[0].overview, sizeof out[0].overview, "Playlist: %s | Video: %s. Cross applies this change; Circle returns without submitting it.", playlist, *video ? video : "none");
            copy(g_comments[0].id, sizeof g_comments[0].id, out[0].id);
            copy(g_comments[0].content, sizeof g_comments[0].content, out[0].overview);
        } else if (!strncmp(parent, "playlist:review:", 16)) {
            char expected[80]; snprintf(expected, sizeof expected, "playlist:review:%u", g_account_intent.serial);
            if (!g_account_intent.serial || strcmp(parent, expected) || strcmp(g_account_intent.profile, g_library_profile)) {
                tube_set_error("Reopen this playlist action for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            CATALOG_FOLDER( id, !strcmp(g_account_intent.action, "playlist_create") ? "Create private YouTube playlist" : "Rename YouTube playlist", hint);
            copy(g_comments[0].id, sizeof g_comments[0].id, out[0].id);
            snprintf(g_comments[0].content, sizeof g_comments[0].content, "Name: %s\nPlaylist: %s\nVideo: %s\nCross confirms; Circle returns without submitting it.",
                g_account_intent.title, g_account_intent.playlist, g_account_intent.identity);
            copy(out[0].overview, sizeof out[0].overview, g_comments[0].content);
        } else { tube_set_error("The playlist screen was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
        if (!strncmp(parent, "playlist:menu:", 14)) {
            char id[160]; snprintf(id, sizeof id, "settings:pin:library:playlist:%.100s", parent + 14);
            CATALOG_FOLDER( id, pin_find(g_library_profile, id + 13) >= 0 ? "Pinned section actions" : "Pin to main menu",
                "Keep this playlist on the main menu of the selected profile");
            if (strcmp(g_library_profile, "guest")) { /* a saved playlist of another channel can leave the library */
                snprintf(id, sizeof id, "playlist:confirm:unsave:%.100s", parent + 14);
                CATALOG_FOLDER( id, "Remove saved playlist from library", "For a playlist of another channel; requires confirmation");
                snprintf(id, sizeof id, "playlist-order:%.100s", parent + 14);
                CATALOG_FOLDER( id, "Playlist order", "Date added, popularity or date published, as YouTube offers for this playlist");
            }
            if (valid_playlist(parent + 14)) {
                snprintf(id, sizeof id, "link:playlist:%.100s", parent + 14);
                CATALOG_FOLDER( id, "Share link", "The playlist's address, with a QR code");
            }
        }
        cb(1, out, n, 0, ud); return 1;
    }
    if (parent && !strncmp(parent, "account:", 8)) {
        if (!strcmp(parent, "account:remove")) {
            if (!strcmp(g_library_profile, "guest")) {
                tube_set_error("Select an account profile first."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            g_account_intent.serial = ++g_account_serial;
            copy(g_account_intent.action, sizeof g_account_intent.action, "profile_remove");
            copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
            char id[80]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            CATALOG_FOLDER( id, "Remove selected account", "Cross confirms; Circle returns without removing it");
            snprintf(out[0].overview, sizeof out[0].overview,
                "Profile: %s. Remove this account and its saved sign-in from ProsperoTube. Continue as Guest; other accounts remain available.", g_library_profile);
            copy(g_comments[0].id, sizeof g_comments[0].id, id);
            copy(g_comments[0].content, sizeof g_comments[0].content, out[0].overview);
        } else if (!strncmp(parent, "account:menu:", 13) && valid_id(parent + 13)) {
            if (!strcmp(g_library_profile, "guest")) {
                CATALOG_FOLDER( "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
            } else {
                Record *r = record(parent + 13, 0);
                for (size_t i = 0; i < sizeof g_account_actions / sizeof g_account_actions[0]; ++i) {
                    if(!strcmp(g_account_actions[i].action,"history_remove") &&
                       (!r || r->history_scope!=g_playlist_scope || r->history_deadline<=now_ms() || !valid_cursor(r->history_remove_cursor)))continue;
                    if(feedback_option(g_account_actions[i].action) && !feedback_current(r,feedback_option(g_account_actions[i].action)))continue;
                    const char *identity = g_account_actions[i].channel ? r ? r->item.parent_id : "" : parent + 13;
                    if (!*identity) continue;
                    char id[160], hint[160];
                    snprintf(id, sizeof id, "account:confirm:%.32s:%.100s", g_account_actions[i].action, identity);
                    snprintf(hint, sizeof hint, "YouTube profile: %s", g_library_profile);
                    CATALOG_FOLDER( id, g_account_actions[i].title, hint);
                    if (feedback_option(g_account_actions[i].action)) experimental(&out[n - 1]);
                }
                char id[80]; snprintf(id, sizeof id, "playlist:pick:add:%s", parent + 13);
                CATALOG_FOLDER( id, "Add to a YouTube playlist", "Choose a playlist, then confirm");
                snprintf(id, sizeof id, "playlist:pick:remove:%s", parent + 13);
                CATALOG_FOLDER( id, "Remove from a YouTube playlist", "Choose a playlist containing this video");
                snprintf(id, sizeof id, "playlist:prompt:create:%s", parent + 13);
                CATALOG_FOLDER( id, "Create playlist with this video", "A new private playlist on the selected account");
                const int recent = recent_playlist();
                char target[160], label[EVO_PROVIDER_MAX_TITLE + 16];
                if (recent >= 0) { /* the playlist this profile used last */
                    snprintf(target, sizeof target, "playlist:confirm:add:%.11s:%.100s", parent + 13, g_recent_playlists[recent].playlist);
                    snprintf(label, sizeof label, "Add to %s", g_recent_playlists[recent].title);
                    CATALOG_FOLDER( target, label, "The playlist this profile used last; asks first");
                    snprintf(target, sizeof target, "playlist:confirm:remove:%.11s:%.100s", parent + 13, g_recent_playlists[recent].playlist);
                    snprintf(label, sizeof label, "Remove from %s", g_recent_playlists[recent].title);
                    CATALOG_FOLDER( target, label, "The playlist this profile used last; asks first");
                }
                if (upcoming(parent + 13)) {
                    snprintf(target, sizeof target, "reminder:%s", parent + 13);
                    CATALOG_FOLDER( target, "Stream reminder", "See or change the reminder YouTube sends when this stream starts");
                }
                if (r && valid_channel(r->item.parent_id)) {
                    snprintf(target, sizeof target, "notify:%.24s", r->item.parent_id);
                    CATALOG_FOLDER( target, "Subscription notifications", "Subscribe with a notification level, or change the level");
                }
            }
            const char *actions[]={"next","watched","forget"};
            const char *labels[]={"Play next","Mark as watched locally","Remove from local history"};
            for(int i=0;i<3;++i) {
                char id[100];snprintf(id,sizeof id,"local:video:%u:%s:%s",g_playlist_scope,actions[i],parent+13);
                CATALOG_FOLDER(id,labels[i],"Saved for the selected profile on this PS5");
            }
            char related[64];snprintf(related,sizeof related,"library:suggestions:%s",parent+13);
            CATALOG_FOLDER(related,"Related videos","Suggestions for this video and the selected profile");
            Record *video=record(parent+13,0);
            g_queue_move.rows[0][0] = g_queue_move.rows[1][0] = 0;
            if (video && video->queued) {
                if (!++g_queue_move.serial) ++g_queue_move.serial;
                g_queue_move.scope = g_playlist_scope;
                copy(g_queue_move.video,sizeof g_queue_move.video,parent+13);
                copy(g_queue_move.profile,sizeof g_queue_move.profile,g_library_profile);
                for (int i = 0; i < 2; ++i) {
                    Record *peer = queue_neighbor(video,i ? 1 : -1);
                    if (!peer) continue;
                    copy(g_queue_move.peers[i],sizeof g_queue_move.peers[i],peer->item.id);
                    snprintf(g_queue_move.rows[i],sizeof g_queue_move.rows[i],"local:queue-move:%u:%u:%s:%s",g_playlist_scope,g_queue_move.serial,i ? "down" : "up",parent+13);
                    CATALOG_FOLDER(g_queue_move.rows[i],i ? "Move down" : "Move up","Saved for the selected profile on this PS5");
                }
            }
            if(video && ((strlen(video->item.parent_id)==24 && !strncmp(video->item.parent_id,"UC",2)) ||
                         (!*video->item.parent_id && tube_channel_name_equal(video->item.subtitle,video->item.subtitle)))) {
                char id[100];snprintf(id,sizeof id,"local:video:%u:%s:%s",g_playlist_scope,
                    blocked_identity(video->item.parent_id,video->item.subtitle)>=0?"unblock":"block",parent+13);
                CATALOG_FOLDER(id,blocked_identity(video->item.parent_id,video->item.subtitle)>=0?"Unblock channel":"Block channel","Hide this channel in catalogs for the selected profile");
            }
            memset(&g_restart,0,sizeof g_restart);g_menu_launch.offered[0]=0;
            char extra[128];snprintf(extra,sizeof extra,"description:%s",parent+13);
            CATALOG_FOLDER(extra,"Open description","Read the full description of this video");
            snprintf(extra,sizeof extra,"details:%s",parent+13);
            CATALOG_FOLDER(extra,"Video details","Channel, date, views, likes and what the video offers");
            snprintf(extra,sizeof extra,"local:video:%u:restart:%s",g_playlist_scope,parent+13);
            CATALOG_FOLDER(extra,"Play from start","Forget the resume position; keep the watched marker");
            snprintf(extra,sizeof extra,"local:video:%u:incognito:%s",g_playlist_scope,parent+13);
            CATALOG_FOLDER(extra,"Play incognito","No local history, resume position or account report for this playback");
            const int channel=video && tube_sb_channel_valid(video->item.parent_id);
            if(channel) {
                snprintf(extra,sizeof extra,"channel:%.24s",video->item.parent_id);
                CATALOG_FOLDER(extra,"Open channel",*video->item.subtitle?video->item.subtitle:"Browse this channel's videos");
            }
            int occurrence=video_occurrence(parent+13);
            if(occurrence>=0) {
                const char *list=g_occurrences[occurrence].playlist;
                const int account=g_occurrences[occurrence].account && strcmp(g_library_profile,"guest");
                snprintf(extra,sizeof extra,"%s%s",account?"library:playlist:":"playlist:",list);
                if(account || strlen(list)>=10)CATALOG_FOLDER(extra,"Open playlist","Browse the playlist this video was listed in");
            }
            snprintf(extra,sizeof extra,"link:video:%s",parent+13);
            CATALOG_FOLDER(extra,"Share link","The video, embed, channel and playlist links, each with a QR code");
            if(channel) {
                const int excluded=tube_sb_excluded(&g_sb,video->item.parent_id);
                snprintf(extra,sizeof extra,"local:video:%u:%s:%s",g_playlist_scope,excluded?"sbinclude":"sbexclude",parent+13);
                CATALOG_FOLDER(extra,excluded?"Use SponsorBlock on this channel":"Exclude channel from SponsorBlock",
                    excluded?"Skip segments again in this channel's videos":"Never skip segments in this channel's videos");
            }
            CATALOG_FOLDER("settings:refresh","Refresh section","Reload the list this video was shown in");
            n = ui_menu_arrange(out, n);
            if (!n) CATALOG_FOLDER( "account:notice", "Every video action is hidden", "YouTube preferences > Interface > Video actions menu shows them again");
        } else if (!strncmp(parent, "account:confirm:", 16)) {
            const char *value = parent + 16, *colon = strchr(value, ':');
            int found = -1;
            for (size_t i = 0; colon && i < sizeof g_account_actions / sizeof g_account_actions[0]; ++i)
                if ((size_t)(colon - value) == strlen(g_account_actions[i].action) &&
                    !strncmp(value, g_account_actions[i].action, (size_t)(colon - value))) found = (int)i;
            char catalog_id[128]; snprintf(catalog_id, sizeof catalog_id, "channel:%s", colon ? colon + 1 : "");
            if (found < 0 || !strcmp(g_library_profile, "guest") ||
                (g_account_actions[found].channel ? !valid_catalog_id(catalog_id) : !valid_id(colon + 1))) {
                tube_set_error("Select a valid video or channel and account profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            g_account_intent.serial = ++g_account_serial;
            copy(g_account_intent.action, sizeof g_account_intent.action, g_account_actions[found].action);
            copy(g_account_intent.identity, sizeof g_account_intent.identity, colon + 1);
            copy(g_account_intent.profile, sizeof g_account_intent.profile, g_library_profile);
            if(!strcmp(g_account_intent.action,"history_remove")) {
                Record *history=record(colon+1,0);
                if(!history || history->history_scope!=g_playlist_scope || history->history_deadline<=now_ms() || !valid_cursor(history->history_remove_cursor)) {
                    memset(&g_account_intent,0,sizeof g_account_intent);
                    tube_set_error("Reload YouTube history before removing this video.");cb(0,NULL,0,0,ud);return 1;
                }
                copy(g_account_intent.history_cursor,sizeof g_account_intent.history_cursor,history->history_remove_cursor);
            }
            if(feedback_option(g_account_intent.action)) {
                Record *card=record(colon+1,0);
                if(!feedback_current(card,feedback_option(g_account_intent.action))) {
                    memset(&g_account_intent,0,sizeof g_account_intent);
                    tube_set_error("Reload the list before sending feedback for this video.");cb(0,NULL,0,0,ud);return 1;
                }
                copy(g_account_intent.history_cursor,sizeof g_account_intent.history_cursor,card->feedback_cursor);
            }
            char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", g_account_intent.serial);
            snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
            CATALOG_FOLDER( id, g_account_actions[found].title, hint);
            const char *label = colon + 1;
            Record *target = record(colon + 1, 0);
            if (target && *target->item.title) label = target->item.title;
            if (g_account_actions[found].channel) for (int i = 0; i < g_count; ++i)
                if (current_record(&g_records[i]) && !strcmp(g_records[i].item.parent_id, colon + 1) && *g_records[i].item.subtitle) {
                    label = g_records[i].item.subtitle; break;
                }
            snprintf(out[0].overview, sizeof out[0].overview,
                "Selected %s: %.128s (%s). Cross applies this change to YouTube; Circle returns without submitting it.",
                g_account_actions[found].channel ? "channel" : "video", label, colon + 1);
        } else if (!strcmp(parent, "account:history") || !strncmp(parent, "account:history:", 16)) {
            /* SmartTube's History menu: pause or resume recording and clear everything. */
            static const char *const verbs[] = {"pause", "resume", "clear"};
            static const char *const titles[] = {"Pause watch history" EXPERIMENTAL, "Resume watch history" EXPERIMENTAL, "Clear watch history" EXPERIMENTAL};
            static const char *const details[] = {"YouTube stops recording what this account watches",
                "YouTube records what this account watches again", "Removes the whole watch history of this account"};
            const int known = g_history_paused.scope == g_playlist_scope && !strcmp(g_history_paused.profile, g_library_profile) ? g_history_paused.state : 0;
            int verb = -1;
            for (int i = 0; parent[15] && i < 3; ++i) if (!strcmp(parent + 16, verbs[i])) verb = i;
            if (!strcmp(g_library_profile, "guest")) {
                CATALOG_FOLDER( "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
            } else if (!parent[15]) {
                for (int i = 0; i < 3; ++i) {
                    char id[64]; snprintf(id, sizeof id, "account:history:%s", verbs[i]);
                    if ((i == 0 && known == 2) || (i == 1 && known == 1)) continue; /* YouTube offers the other control */
                    CATALOG_FOLDER( id, titles[i], details[i]); experimental(&out[n - 1]);
                }
            } else if (verb < 0) { tube_set_error("The selected account action was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
            else {
                static const char *const actions[] = {"history_pause", "history_resume", "history_clear"};
                unsigned serial = account_intent(actions[verb], "", "", "");
                char id[80], hint[160]; snprintf(id, sizeof id, "account:apply:%u", serial);
                snprintf(hint, sizeof hint, "Confirm for YouTube profile: %s", g_library_profile);
                CATALOG_FOLDER( id, titles[verb], hint);
                snprintf(out[0].overview, sizeof out[0].overview, "%s. Profile: %s. Cross applies this change to YouTube once; Circle returns without submitting it.",
                    details[verb], g_library_profile);
                copy(g_comments[0].id, sizeof g_comments[0].id, id);
                copy(g_comments[0].content, sizeof g_comments[0].content, out[0].overview);
            }
        } else if (!strncmp(parent, "account:apply:", 14)) {
            char expected[80]; snprintf(expected, sizeof expected, "account:apply:%u", g_account_intent.serial);
            if (!g_account_intent.serial || strcmp(parent, expected) || strcmp(g_account_intent.profile, g_library_profile)) {
                tube_set_error("Reopen the account action to confirm it for the current profile."); cb(0, NULL, 0, 0, ud); return 1;
            }
            if (!g_account_intent.state) return 0; /* first explicit confirmation submits one job */
            CATALOG_FOLDER( "account:notice", g_account_intent.state == 2 ? "YouTube account updated" :
                g_account_intent.state == 1 ? "Account change pending" : "Account change not confirmed",
                g_account_intent.state == 1 ? "Wait for the companion response" : "Check your account before repeating an unconfirmed change");
        } else if (!strncmp(parent, "account:notice", 14)) {
            CATALOG_FOLDER( "account:notice", "Circle to return", "No account operation submitted");
        } else { tube_set_error("The selected account action was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
        int offset=page*LOCAL_PAGE,count=offset<n?n-offset:0; /* only the video menu reaches a second page */
        if(count>LOCAL_PAGE)count=LOCAL_PAGE;
        cb(1,count?out+offset:NULL,count,offset+count<n,ud);return 1;
    }
    if (!parent || !*parent || !strcmp(parent, "discover")) {
        /* The profile's order, without the rows it hides; pins follow the main menu. */
        const int group = parent && *parent ? SECTION_DISCOVER : SECTION_ROOT;
        /* The app opens without its service; the first row says why and retries. */
        if (group == SECTION_ROOT && g_embedded && !g_embedded_stopped && *g_embedded_error)
            CATALOG_FOLDER( "settings:service-retry", "YouTube service did not start - select to retry", g_embedded_error);
        for (int k = 0; k < SECTIONS; ++k) {
            const int s = g_section_order[k];
            if (g_sections[s].group != group || !section_listed(s)) continue;
            const char *label = g_sections[s].label, *detail = g_sections[s].detail;
            if (g_embedded && !strcmp(g_sections[s].id, "library:home")) detail = "What YouTube recommends to this profile";
            if (embedded_guest_only() && !strcmp(g_sections[s].id, "library")) { label = "Accounts"; detail = "Sign-in to YouTube arrives in a later version"; }
            CATALOG_FOLDER( g_sections[s].id, label, detail);
        }
        if (group == SECTION_ROOT) {
            for (int i = 0; i < g_pin_count; ++i) if (!strcmp(g_pins[i].profile, g_library_profile))
                CATALOG_FOLDER( g_pins[i].id, g_pins[i].title, !strncmp(g_pins[i].id, "channel:", 8) ? "Pinned channel; Touchpad for actions" : "Pinned playlist; Touchpad for actions");
            CATALOG_FOLDER( "settings", "YouTube preferences", "Quality, codec, captions and SponsorBlock");
            CATALOG_FOLDER( "app:settings", "Player settings", "Display, audio and controller preferences");
            CATALOG_FOLDER( "app:exit", "Close ProsperoTube", "Release playback, then close from the PS5 switcher");
        }
    } else if (!strcmp(parent, "library") || !strncmp(parent, "library:profile:", 16)) {
        if (!strncmp(parent, "library:profile:", 16)) {
            tube_recovery_finish();
            if (!valid_profile(parent + 16)) { tube_set_error("The selected profile was invalid."); cb(0, NULL, 0, 0, ud); return 1; }
            /* A protected profile is selected only after its password was given
             * through the text prompt; the grant serves this one selection. */
            if (strcmp(parent + 16, g_library_profile) && lock_find(parent + 16) >= 0 && strcmp(g_unlock.profile, parent + 16)) {
                prompt_arm("Password of this profile");
                g_playlist_prompt.lock = 1; copy(g_playlist_prompt.target, sizeof g_playlist_prompt.target, parent + 16);
                CATALOG_FOLDER( "account:notice", "Enter the password of this profile", "Circle returns without selecting it");
                cb(1, out, n, 0, ud); return 1;
            }
            memset(&g_unlock, 0, sizeof g_unlock);
            memset(&g_account_intent, 0, sizeof g_account_intent);
            memset(&g_playlist_prompt, 0, sizeof g_playlist_prompt);
            g_startup_lock[0] = 0; /* a profile was chosen: nothing waits for its password any more */
            if (!profile_settings_select(parent + 16)) {
                tube_set_error("The saved profile settings list is full."); cb(0,NULL,0,0,ud); return 1;
            }
            watch_reset(1); tube_extra_video_end();
            sponsor_invalidate();
            g_next_id[0] = 0; g_next_profile[0] = 0;
            memset(g_comments, 0, sizeof g_comments);
            g_library_parent[0] = 0; memset(g_library_cursors, 0, sizeof g_library_cursors); save();
        }
        if (embedded_guest_only()) { /* this service bundle has no accounts: say so, instead of rows that cannot work */
            CATALOG_FOLDER( "account:notice", "Sign-in to YouTube arrives in a later version", "This version browses and plays as Guest, without an account");
            CATALOG_FOLDER( "account:notice", "Your library on this PS5 needs no account", "History, favorites, the play queue and followed channels are in the main menu");
            CATALOG_FOLDER( "library:home", "Home", "What YouTube recommends without an account");
            cb(1, out, n, 0, ud); return 1;
        }
        char label[96]; snprintf(label, sizeof label, "Profile: %s", g_library_profile);
        CATALOG_FOLDER( "library:profiles", label, g_embedded ? "Guest, or an account signed in on this PS5" : "Choose an account configured on the companion");
        CATALOG_FOLDER( "library:home", "Home", "Recommendations for the selected profile");
        CATALOG_FOLDER( "library:subscriptions", "Subscriptions", "Videos from your YouTube subscriptions");
        CATALOG_FOLDER( "library:history", "YouTube history", "History from the selected account");
        CATALOG_FOLDER( "library:watch_later", "Watch later", "Videos saved to your account");
        CATALOG_FOLDER( "library:playlists", "Your playlists", "Playlists from the selected account");
        CATALOG_FOLDER( "library:channels", "Subscribed channels", g_channel_sort == 2 ? "Channels you subscribe to, new content first" :
            g_channel_sort ? "Channels you subscribe to, by name" : "Channels you subscribe to");
        CATALOG_FOLDER( "library:notifications", "Notifications", "Videos your account was notified about");
        CATALOG_FOLDER( "section:my_videos", "My videos", "Uploads of the selected account");
        CATALOG_FOLDER( "playlist:prompt:create:", "Create YouTube playlist", "A private playlist on the selected account");
        CATALOG_FOLDER( "auth:start", "Sign in to YouTube", "Authorize on your phone or computer");
        if (strcmp(g_library_profile, "guest"))
            CATALOG_FOLDER( "account:remove", "Remove selected account", "Review before removing its saved sign-in");
    } else if (!strncmp(parent, "settings", 8)) {
        if (!strncmp(parent, "settings:backup:import:file:", 28)) { /* read by the companion: a request, not a local screen */
            if (!page && import_name_valid(parent + 28) && import_size(parent + 28)) return 0;
            tube_set_error("Select a listed import file."); cb(0,NULL,0,0,ud); return 1;
        }
        int extra_choice; const int extra_group = extra_group_parent(parent, &extra_choice);
        int ui_choice = -1; const char *ui_verb = "";
        const int ui_group = !strncmp(parent, "settings:ui:", 12) ? ui_group_parent(parent, &ui_choice) : -1;
        const int ui_key = ui_menu_parent(parent, &ui_verb);
        if (!strncmp(parent, "settings:backup", 15)) memset(g_backup_notice, 0, sizeof g_backup_notice);
        if (!page) { /* Fetching a later menu page must not repeat its action. */
        if (extra_group >= 0) {
            if (extra_choice == -2) { tube_set_error("Select a listed value for this player preference."); cb(0,NULL,0,0,ud); return 1; }
            if (extra_choice >= 0 && !tube_extra_group_select(extra_group, extra_choice)) { cb(0,NULL,0,0,ud); return 1; }
        } else if (!strncmp(parent, "settings:ui:", 12)) {
            if (ui_group < 0 || ui_choice == -2) { tube_set_error("Select a listed value for this interface preference."); cb(0,NULL,0,0,ud); return 1; }
            if (ui_choice >= 0) {
                tube_ui_group_select(&g_ui, ui_group, ui_choice);
                ++g_ui_revision; tube_ui_press_reset(); save();
            }
        } else if (!strncmp(parent, "settings:menu:", 14)) {
            if (!strcmp(parent, "settings:menu:reset")) { tube_ui_menu_defaults(g_ui.menu_order); g_ui.menu_hidden = 0; save(); }
            else if (ui_key < 0 || (*ui_verb && strcmp(ui_verb, "up") && strcmp(ui_verb, "down") && strcmp(ui_verb, "toggle"))) {
                tube_set_error("Select a row of the video actions menu."); cb(0,NULL,0,0,ud); return 1;
            } else if (*ui_verb) {
                if (!strcmp(ui_verb, "toggle")) g_ui.menu_hidden ^= (uint64_t)1 << ui_key;
                else tube_ui_menu_move(&g_ui, ui_key, !strcmp(ui_verb, "up") ? -1 : 1);
                save();
            }
        } else if (!strncmp(parent, "settings:content-language:", 26) || !strncmp(parent, "settings:content-country:", 25)) {
            const int country = parent[17] == 'c';
            const char *value = parent + (country ? 25 : 26);
            if (strcmp(value, "default") && (country ? country_index(value) : language_index(value)) < 0) {
                tube_set_error("Select a listed language or country."); cb(0,NULL,0,0,ud); return 1;
            }
            if (!strcmp(value, "default")) value = "";
            if (country) copy(g_gl, sizeof g_gl, value); else copy(g_hl, sizeof g_hl, value);
            catalog_retire(); save();
        } else if (!strncmp(parent, "settings:hide:", 14)) {
            const int flag = hide_index(parent + 14);
            if (!strcmp(parent + 14, "reset")) { g_hide = 0; g_hide_set = 0; }
            else if (flag < 0) { tube_set_error("Select a listed content switch."); cb(0,NULL,0,0,ud); return 1; }
            else { g_hide = hide_effective() ^ 1u << flag; g_hide_set = 1; }
            catalog_retire(); save();
        } else if (!strncmp(parent, "settings:thumbnails:", 20)) {
            int source = -1;
            for (int i = 0; i < 4; ++i) if (!strcmp(parent + 20, g_thumbnail_sources[i])) source = i;
            if (source < 0) { tube_set_error("Select a listed thumbnail source."); cb(0,NULL,0,0,ud); return 1; }
            g_thumbnails = source; catalog_retire(); save();
        }
        else if (!strcmp(parent, "settings:sb:alt")) { g_sb_alt = !g_sb_alt; save(); }
        else if (!strcmp(parent, "settings:sb:server") && g_embedded) { /* typed over this screen; an accepted address lists it again */
            prompt_arm("Alternative SponsorBlock server"); g_playlist_prompt.server = 1;
            copy(g_playlist_prompt.prompt.initial, sizeof g_playlist_prompt.prompt.initial, *g_sb_server ? g_sb_server : "https://");
        }
        else if (!strcmp(parent, "settings:more-subtitles")) { g_more_subtitles = !g_more_subtitles; save(); }
        else if (!strcmp(parent, "settings:caption-memory")) { g_caption_memory = !g_caption_memory; save(); }
        else if (!strcmp(parent, "settings:dislikes")) { g_dislikes = !g_dislikes; save(); }
        else if (!strncmp(parent, "settings:backup:import:apply:", 29)) {
            char expected[64]; snprintf(expected, sizeof expected, "settings:backup:import:apply:%u", g_import.serial);
            if (!g_import.serial || strcmp(parent, expected) || strcmp(g_import.profile, g_library_profile)) {
                tube_set_error("Read the import file again for the current profile."); cb(0,NULL,0,0,ud); return 1;
            }
            if (!g_import.applied) { /* one confirmation adds the list once */
                for (int i = 0; i < g_import.listed; ++i) {
                    if (followed(g_import.rows[i].channel)) ++g_import.existing;
                    else if (follow_put(g_library_profile, g_import.rows[i].channel, g_import.rows[i].title)) ++g_import.added;
                    else ++g_import.full;
                }
                g_import.full += g_import.count - g_import.listed; g_import.applied = 1; save();
            }
        }
        else if (!strncmp(parent, "settings:backup:", 16) && strncmp(parent, "settings:backup:import", 22) &&
                 strcmp(parent, "settings:backup:list") && strncmp(parent, "settings:backup:entry:", 22)) {
            const char *verb = parent + 16; char name[TUBE_BACKUP_NAME_SIZE] = "", safety[TUBE_BACKUP_NAME_SIZE] = "";
            const long long now = (long long)time(NULL); int rc = TUBE_BACKUP_OK;
            if (!strcmp(verb, "create")) {
                save(); /* the copy holds what is on screen now */
                rc = g_store_writable && !g_remote_write_failed ? tube_backup_create(evo_data_dir(), now, name, sizeof name) : TUBE_BACKUP_E_IO;
                if (rc == TUBE_BACKUP_OK) {
                    tube_backup_rotate(evo_data_dir(), TUBE_BACKUP_KEEP_DEFAULT);
                    copy(g_backup_notice[0], sizeof g_backup_notice[0], "Backup created"); copy(g_backup_notice[1], sizeof g_backup_notice[1], name);
                }
            } else if (!strncmp(verb, "auto:", 5)) {
                char *end = NULL; const long days = strtol(verb + 5, &end, 10);
                if (!verb[5] || *end || days < -1 || days > 30 || !tube_backup_auto_valid((int)days)) {
                    tube_set_error("Select a listed backup period."); cb(0,NULL,0,0,ud); return 1;
                }
                g_backup_auto = (int)days; save();
            } else if (!strncmp(verb, "restore:", 8) || !strncmp(verb, "delete:", 7)) {
                /* The entry screen issued a ticket for one action: reloading a finished one repeats nothing. */
                const int restore = verb[0] == 'r'; const char *value = verb + (restore ? 8 : 7); char *end = NULL;
                const unsigned long ticket = isdigit((unsigned char)*value) ? strtoul(value, &end, 10) : 0;
                if (!ticket || ticket != g_backup_ticket || !end || *end != ':' || !tube_backup_name_valid(end + 1)) {
                    tube_set_error("Reopen the backup list and select the backup again."); cb(0,NULL,0,0,ud); return 1;
                }
                g_backup_ticket = 0; copy(name, sizeof name, end + 1);
                if (!restore) rc = tube_backup_delete(evo_data_dir(), name);
                else {
                    save(); /* the safety copy holds what is on screen now */
                    rc = g_store_writable && !g_remote_write_failed ? tube_backup_restore(evo_data_dir(), name, now, safety, sizeof safety) : TUBE_BACKUP_E_IO;
                    if (rc == TUBE_BACKUP_OK) {
                        /* The file changed under the provider: read it before anything saves the replaced state over it. */
                        if (init()) { tube_set_error("The restored backup could not be loaded."); cb(0,NULL,0,0,ud); return 1; }
                        ++g_accounts_revision; tube_backup_rotate(evo_data_dir(), TUBE_BACKUP_KEEP_DEFAULT);
                    }
                }
                if (rc == TUBE_BACKUP_OK) {
                    copy(g_backup_notice[0], sizeof g_backup_notice[0], restore ? "Backup restored" : "Backup deleted");
                    if (restore && *safety) snprintf(g_backup_notice[1], sizeof g_backup_notice[1], "The replaced state was kept as %s", safety);
                    else copy(g_backup_notice[1], sizeof g_backup_notice[1], name);
                }
            } else { tube_set_error("Select a listed backup action."); cb(0,NULL,0,0,ud); return 1; }
            if (rc != TUBE_BACKUP_OK) { tube_set_error(tube_backup_error_text(rc)); cb(0,NULL,0,0,ud); return 1; }
        }
        else if (!strcmp(parent, "settings:use-section-playlist")) { g_section_playlist = !g_section_playlist; save(); }
        else if (!strcmp(parent, "settings:separate-settings")) {
            /* Either way the selected profile keeps what is on screen; the other
             * profiles return to their own sets, or all to the shared one. */
            const ProfileSettings current = profile_settings_capture();
            const int from = player_preference(settings_key(),0);
            g_separate_settings = !g_separate_settings;
            const int slot = profile_settings_slot(settings_key(),1), to = player_preference(settings_key(),1);
            if (slot < 0) {
                g_separate_settings = !g_separate_settings;
                tube_set_error("The saved profile settings list is full."); cb(0,NULL,0,0,ud); return 1;
            }
            g_profile_settings[slot].value = current;
            if (to >= 0 && to != from) {
                char owner[33]; copy(owner,sizeof owner,g_player_preferences[to].profile);
                if (from >= 0) g_player_preferences[to] = g_player_preferences[from];
                else {
                    memset(&g_player_preferences[to],0,sizeof g_player_preferences[to]);
                    g_player_preferences[to].hide_ms = 3000; g_player_preferences[to].seek_seconds = 10; g_player_preferences[to].time_mode = 1;
                }
                copy(g_player_preferences[to].profile,sizeof g_player_preferences[to].profile,owner);
            }
            video_geometry_publish(); save();
        }
        else if (!strncmp(parent, "settings:lock:", 14)) {
            const char *verb = parent + 14; const int locked = lock_find(g_library_profile) >= 0;
            char granted[48]; snprintf(granted, sizeof granted, "new:%u", g_unlock.ticket);
            const int renew = g_unlock.ticket && !strcmp(verb, granted) && !strcmp(g_unlock.profile, g_library_profile);
            if (!strcmp(g_library_profile, "guest") || (strcmp(verb, "set") ? !locked : locked) ||
                (strcmp(verb, "set") && strcmp(verb, "change") && strcmp(verb, "remove") && !renew)) {
                tube_set_error("Reopen the account settings of the selected profile."); cb(0,NULL,0,0,ud); return 1;
            }
            memset(&g_unlock, 0, sizeof g_unlock);
            prompt_arm(!strcmp(verb, "set") || renew ? "New password of this profile" : "Current password of this profile");
            g_playlist_prompt.lock = !strcmp(verb, "set") || renew ? 2 : !strcmp(verb, "change") ? 3 : 4;
        }
        else if (!strncmp(parent, "settings:speed-mode:", 20)) {
            const char *modes[] = {"all", "channel", "video", "none"}; int mode = -1;
            for (int i = 0; i < 4; ++i) if (!strcmp(parent + 20, modes[i])) mode = i;
            if (mode < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_speed_mode = mode;
            if (!strcmp(g_speed_session_profile,g_library_profile)) g_speed_session_id[0] = 0;
            save();
        } else if (!strncmp(parent, "settings:speed:", 15)) {
            char *end; long speed = strtol(parent + 15, &end, 10);
            if (!*(parent + 15) || *end || !valid_speed((double)speed)) { cb(0, NULL, 0, 0, ud); return 1; }
            g_speed_mode = 0;
            if (!strcmp(g_speed_session_profile,g_library_profile)) g_speed_session_id[0] = 0;
            tube_set_playback_speed((int)speed);
        } else if (!strncmp(parent, "settings:frame-rate:", 20)) {
            char *end; long fps = strtol(parent + 20, &end, 10);
            if (!*(parent + 20) || *end || (fps != 0 && fps != 30 && fps != 60)) { cb(0,NULL,0,0,ud); return 1; }
            g_max_fps = (int)fps; save();
        } else if (!strcmp(parent, "settings:codec")) { g_codec = (g_codec + 1) % 5; save(); }
        else if (!strcmp(parent, "settings:captions")) { g_captions = !g_captions; save(); }
        else if (!strcmp(parent, "settings:automatic")) { g_auto_captions = !g_auto_captions; save(); }
        else if (!strcmp(parent, "settings:sponsor")) { g_sponsor = !g_sponsor; g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save(); }
        else if (!strcmp(parent, "settings:dearrow:titles")) { g_dearrow_titles = !g_dearrow_titles; save(); }
        else if (!strcmp(parent, "settings:dearrow:thumbnails")) { g_dearrow_thumbnails = !g_dearrow_thumbnails; save(); }
        else if (!strcmp(parent, "settings:sb:paid")) { g_paid_notifications = !g_paid_notifications; save(); }
        else if (!strncmp(parent, "settings:sb:action:", 19)) {
            int category = tube_sb_category(parent + 19);
            if (category < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_sb.actions[category] = (g_sb.actions[category] + 1) % 4; g_sb_prompt = -1; g_sb_finish_target = -1; g_highlight_guard = -1; save();
        } else if (!strncmp(parent, "settings:sb:color:", 18)) {
            int category = tube_sb_category(parent + 18);
            if (category < 0) { cb(0, NULL, 0, 0, ud); return 1; }
            g_sb.colors[category] = !g_sb.colors[category]; save();
        } else if (!strncmp(parent, "settings:sb:minimum:", 20)) {
            const char *value = parent + 20; char *end = NULL; long ms = strtol(value,&end,10);
            if (!isdigit((unsigned char)*value) || *end || ms > 20000 || ms % 500) { cb(0, NULL, 0, 0, ud); return 1; }
            g_sb.minimum = ms / 1000.0; g_sb_finish_target = -1; save();
        } else if (!strcmp(parent, "settings:sb:repeat")) { g_sb.no_repeat = !g_sb.no_repeat; save(); }
        else if (!strncmp(parent, "settings:sb:exclude:", 20)) {
            const char *channel = parent + 20;
            if (!tube_sb_channel_valid(channel) || !sponsor_exclude(channel, !tube_sb_excluded(&g_sb, channel))) { cb(0, NULL, 0, 0, ud); return 1; }
        }
        else if (!strcmp(parent, "settings:autoplay")) {
            tube_set_playback_mode(g_autoplay?TUBE_MODE_PAUSE:TUBE_MODE_ALL);
        }
        else if (!strcmp(parent,"settings:queue-respects-mode")) {
            tube_queue_retry_cancel();
            g_queue_respects_mode=!g_queue_respects_mode; save();
        }
        else if (!strncmp(parent,"settings:playback-mode:",23)) {
            const char *value=parent+23; char *end=NULL; long mode=strtol(value,&end,10);
            if (!*value || *end || mode<0 || mode>6 || !tube_set_playback_mode((int)mode)) {
                cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strcmp(parent, "settings:player-chat")) {
            tube_player_chat_set_mode((g_player_chat_mode + 1) % 3);
        }
        else if (!strncmp(parent, "settings:sleep:", 15)) {
            const char *value = parent + 15; char *end = NULL;
            long minutes = strtol(value, &end, 10);
            if (!*value || *end || minutes < 0 || minutes > 600 || !tube_sleep_set_minutes((int)minutes)) {
                tube_set_error("Select a valid sleep timer duration."); cb(0,NULL,0,0,ud); return 1;
            }
        }
        else if (!strncmp(parent,"settings:player-ok:",19) || !strncmp(parent,"settings:player-hide:",21)) {
            int ok = !strncmp(parent,"settings:player-ok:",19);
            const char *value = parent + (ok ? 19 : 21); char *end = NULL;
            long selection = strtol(value,&end,10);
            if (!*value || *end || selection < 0 || selection > (ok ? 3 : 15) ||
                !(ok ? tube_player_set_ok_mode((int)selection) : tube_player_set_hide_seconds((int)selection))) {
                tube_set_error("Select a valid player preference for this profile."); cb(0,NULL,0,0,ud); return 1;
            }
        }
        else if (!strncmp(parent,"settings:rotate:",16)) {
            const char* value=parent+16;char* end=NULL;long degrees=strtol(value,&end,10);
            if(!*value || *end || degrees<0 || degrees>270 || !tube_player_set_rotation_degrees((int)degrees)) {
                tube_set_error("Select a valid rotation.");cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strcmp(parent,"settings:video-mirror")) {
            if(!tube_player_set_mirror_enabled(!tube_player_mirror_enabled())) {
                tube_set_error("Unable to save mirror setting.");cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strncmp(parent,"settings:aspect:",16) || !strncmp(parent,"settings:pixel:",15)) {
            int aspect=!strncmp(parent,"settings:aspect:",16);
            const char* value=parent+(aspect?16:15);char* end=NULL;long choice=strtol(value,&end,10);
            if(!*value || *end || choice<0 || choice>(aspect?10:5) ||
                !(aspect?tube_player_set_aspect_mode((int)choice):tube_player_set_pixel_mode((int)choice))) {
                tube_set_error("Select a valid aspect or display correction.");cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strncmp(parent,"settings:resize:",16) || !strncmp(parent,"settings:zoom:",14)) {
            int resize=!strncmp(parent,"settings:resize:",16);
            const char* value=parent+(resize?16:14);char* end=NULL;long choice=strtol(value,&end,10);
            if(!*value || *end || choice<0 || choice>300 ||
                !(resize?tube_player_set_resize_mode((int)choice):tube_player_set_zoom_percent((int)choice))) {
                tube_set_error("Select a valid video sizing option.");cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strncmp(parent,"settings:time-display:",22)) {
            const char *value=parent+22;char *end=NULL;long mode=strtol(value,&end,10);
            if(!*value || *end || mode<0 || mode>2 || !tube_player_set_time_mode((int)mode)) {
                tube_set_error("Select a valid time display for this profile.");cb(0,NULL,0,0,ud);return 1;
            }
        }
        else if (!strncmp(parent,"settings:seek-interval:",23)) {
            const char *value=parent+23; char *end=NULL; long seconds=strtol(value,&end,10);
            if(!*value || *end || seconds<1 || seconds>60 || !tube_player_set_seek_seconds((int)seconds)) {
                tube_set_error("Select a valid seek interval for this profile."); cb(0,NULL,0,0,ud); return 1;
            }
        }
        else if (!strncmp(parent,"settings:player-seek:",21)) {
            const char *value = parent + 21; char *end = NULL; long mode = strtol(value,&end,10);
            if (!*value || *end || mode < 0 || mode > 2 || !tube_player_set_seek_mode((int)mode)) {
                tube_set_error("Select a valid seeking mode for this profile."); cb(0,NULL,0,0,ud); return 1;
            }
        }
        else if (!strcmp(parent, "settings:remote:toggle") || !strcmp(parent, "settings:remote:pair")) {
            if (embedded_lacks(COMPANION_REMOTE)) {
                tube_set_error("Linking a phone is not part of this built-in YouTube service.");
            } else if (!g_store_writable || !*g_source || !*g_token) {
                tube_set_error(g_embedded ? "Linking a phone needs the built-in YouTube service running and writable local storage. Check both and retry." :
                    "Pair with the companion and check local storage before linking a phone.");
            } else {
                g_remote_entropy_failed = 0;
                g_remote_enabled = !strcmp(parent, "settings:remote:pair") ? 1 : !g_remote_enabled;
                save(); remote_configure();
                if (!strcmp(parent, "settings:remote:pair")) tube_remote_pair();
            }
        }
        else if (!strcmp(parent, "settings:remote:disconnect")) {
            if (g_remote_close_on_disconnect) g_remote_close_on_disconnect = 0;
            else if (g_remote_stop_on_disconnect) { g_remote_stop_on_disconnect = 0; g_remote_close_on_disconnect = 1; }
            else g_remote_stop_on_disconnect = 1;
            save();
        }
        else if (!strcmp(parent, "settings:remote:history")) { live_retire(); g_remote_no_history = !g_remote_no_history; save(); }
        else if (!strcmp(parent, "settings:remote:reset")) {
            tube_remote_shutdown(); g_remote_enabled = g_remote_entropy_failed = 0;
            g_remote_device[0] = g_remote_screen[0] = 0; g_remote_identity_saved = 0;
            save(); remote_configure();
        }
        else if (!strcmp(parent, "settings:ask-stream")) { g_ask_stream = !g_ask_stream; save(); }
        else if (!strncmp(parent, "settings:history:", 17)) {
            const char *value = parent + 17; char *end = NULL; long state = strtol(value,&end,10);
            if (!isdigit((unsigned char)*value) || *end || state > TUBE_HISTORY_DISABLED) { cb(0,NULL,0,0,ud); return 1; }
            live_retire();
            g_history_state = (int)state;
            if (g_history_state == TUBE_HISTORY_DISABLED) g_watch.dirty = 0;
            save();
        }
        else if (!strcmp(parent, "settings:search-history")) { g_search_history = !g_search_history; save(); }
        else if (!strcmp(parent, "settings:account-on-boot")) { g_select_account_on_boot = !g_select_account_on_boot; save(); }
        else if (!strncmp(parent, "settings:boot-section:", 22)) {
            const char *value = parent + 22; char *end = NULL; long section = strtol(value,&end,10);
            if (!isdigit((unsigned char)*value) || *end || section >= BOOT_SECTIONS) { cb(0,NULL,0,0,ud); return 1; }
            copy(g_boot_section, sizeof g_boot_section, g_boot_sections[section].id); save();
        }
        else if (!strcmp(parent, "settings:search:cards")) { g_search_cards = !g_search_cards; g_search_feed.unavailable = 0; save(); }
        else if (!strncmp(parent, "settings:search:", 16)) {
            const char *value = parent + 16; int feature = -1;
            search_filters();
            for (int i = 0; i < SEARCH_FEATURES; ++i)
                if (!strncmp(value, "feature:", 8) && !strcmp(value + 8, g_search_features[i])) feature = i;
            if (!strcmp(value, "date")) g_search_options.date = (g_search_options.date + 1) % 5;
            else if (!strcmp(value, "duration")) g_search_options.duration = (g_search_options.duration + 1) % 4;
            else if (!strcmp(value, "type")) g_search_options.type = (g_search_options.type + 1) % 6;
            else if (!strcmp(value, "order")) g_search_options.order = !g_search_options.order;
            else if (feature >= 0) g_search_options.features ^= 1u << feature;
            else if (!strcmp(value, "reset")) search_options_reset();
            else { cb(0,NULL,0,0,ud); return 1; }
            g_search_feed.unavailable = 0; /* a changed option asks the companion again */
        }
        else if (!strncmp(parent, "settings:channel-sort:", 22)) {
            if (parent[22] < '0' || parent[22] > '2' || parent[23]) { tube_set_error("Select a listed channel order."); cb(0,NULL,0,0,ud); return 1; }
            g_channel_sort = parent[22] - '0'; g_channel_list.complete = 0; save();
        }
        else if (!strncmp(parent, "settings:section:", 17)) {
            char *end = NULL; long section = isdigit((unsigned char)parent[17]) ? strtol(parent + 17, &end, 10) : -1;
            if (section < 0 || section >= SECTIONS || (*end && strcmp(end, ":up") && strcmp(end, ":down") && strcmp(end, ":toggle"))) {
                tube_set_error("Select a section of the main menu or Discover."); cb(0,NULL,0,0,ud); return 1;
            }
            if (!strcmp(end, ":toggle")) g_section_hidden ^= 1u << section;
            else if (*end) section_move((int)section, !strcmp(end, ":up") ? -1 : 1);
            if (*end) save();
        }
        else if (!strncmp(parent, "settings:pin:", 13)) { if (!pin_action(parent + 13)) { cb(0,NULL,0,0,ud); return 1; } }
        else if (!strncmp(parent, "settings:lang:", 14) || !strncmp(parent, "settings:audio:", 15)) {
            const int audio = parent[9] == 'a'; const char *value = parent + (audio ? 15 : 14);
            if ((*value || !audio) && language_index(value) < 0 && !legacy_track_language(value)) {
                tube_set_error("Select a listed language."); cb(0,NULL,0,0,ud); return 1;
            }
            if (audio) copy(g_audio_language, sizeof g_audio_language, value); else copy(g_language, sizeof g_language, value);
            save();
        }
        else if (!strncmp(parent, "settings:update:", 16)) { if (tube_update_app_action) tube_update_app_action(parent + 16); }
        else if (!strcmp(parent, "settings:clear-history")) {
            live_retire();
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i])) {
                g_records[i].updated = 0; g_records[i].live_utc = 0; g_records[i].item.resume_pos_sec = 0; g_records[i].item.played = 0;
            } save();
        } else if (!strcmp(parent, "settings:clear-queue")) {
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i])) {
                g_records[i].queued = 0; g_records[i].queue_order=0;
            }
            save();
        }
        }
        if (extra_group >= 0) {
            const int selected = tube_extra_group_selected(extra_group), count = tube_extra_group_count(extra_group);
            for (int i = 0; i < count; ++i) {
                char id[80], label[96]; snprintf(id, sizeof id, "%s:%d", tube_extra_group_id(extra_group), i);
                tube_extra_group_label(extra_group, i, label, sizeof label);
                CATALOG_FOLDER( id, label, i == selected ? "Selected" : tube_extra_group_detail(extra_group));
            }
        } else if (!strncmp(parent, "settings:extra:", 15) || !strcmp(parent, "settings:use-section-playlist") ||
                   !strcmp(parent, "settings:more-subtitles") || !strcmp(parent, "settings:caption-memory") || !strcmp(parent, "settings:dislikes")) {
            int screen = !strcmp(parent, "settings:more-subtitles") || !strcmp(parent, "settings:caption-memory") ? 0 : 2;
            for (int i = 0; i < EXTRA_SCREENS; ++i) if (!strcmp(parent, g_extra_screens[i])) screen = i;
            for (int group = 0; group < TUBE_EXTRA_GROUPS; ++group) if (extra_screen(group) == screen) {
                char label[192], value[96] = "other";
                if (tube_extra_group_selected(group) >= 0) tube_extra_group_label(group, tube_extra_group_selected(group), value, sizeof value);
                snprintf(label, sizeof label, "%s: %s", tube_extra_group_title(group), value);
                CATALOG_FOLDER( tube_extra_group_id(group), label, tube_extra_group_detail(group));
            }
            if (screen == 2) { /* its two long lists are screens of their own */
                CATALOG_FOLDER( "settings:extra:buttons", "Player buttons", "Choose the actions the in-player options show");
                CATALOG_FOLDER( "settings:extra:dimming", "Screen dimming and screen off", "Dim the picture while idle; timeout and strength of screen off");
            }
            if (screen == 2) CATALOG_FOLDER( "settings:use-section-playlist", g_section_playlist ? "Use current section as playlist: on" : "Use current section as playlist: off",
                "On: after a video from a section, shelf or channel, continue with the next one listed there");
            /* The built-in service asks Return YouTube Dislike itself, for this switch alone; one that has no relay offers no row. */
            if (screen == 2 && !embedded_lacks(COMPANION_DISLIKES)) CATALOG_FOLDER( "settings:dislikes", g_dislikes ? "Show dislikes: on" : "Show dislikes: off",
                companion_known() && !companion_has(COMPANION_DISLIKES) ? "The companion host has not enabled the Return YouTube Dislike relay" :
                "In descriptions and details; Return YouTube Dislike then learns which videos you open them for");
            if (screen == 0) {
                CATALOG_FOLDER( "settings:more-subtitles", g_more_subtitles ? "Unlock more subtitles: on" : "Unlock more subtitles: off",
                    companion_known() && !companion_has(COMPANION_TRANSLATED) ? "The paired companion does not list translated captions" :
                    "Adds machine translations to the caption list of the next video; opening it takes longer");
                CATALOG_FOLDER( "settings:caption-memory", g_caption_memory ? "Remember subtitles per channel: on" : "Remember subtitles per channel: off",
                    "Captions turned on or off while watching apply to that channel's next videos");
            }
        } else if (!strcmp(parent, "settings:content-language") || !strncmp(parent, "settings:content-language:", 26)) {
            CATALOG_FOLDER( "settings:content-language:default", "Default", !*g_hl ? "Selected" : "Let YouTube choose the language");
            for (int i = 0; i < LANGUAGES; ++i) {
                char id[64]; snprintf(id, sizeof id, "settings:content-language:%s", g_languages[i].code);
                CATALOG_FOLDER( id, g_languages[i].name, !strcmp(g_hl, g_languages[i].code) ? "Selected" : "Language of titles, sections and descriptions from YouTube");
            }
        } else if (!strcmp(parent, "settings:content-country") || !strncmp(parent, "settings:content-country:", 25)) {
            const int total = COUNTRIES + 1, start = page < 8 ? page * LOCAL_PAGE : total;
            for (int i = start; i < total && n < LOCAL_PAGE; ++i) {
                char id[64];
                if (!i) { CATALOG_FOLDER( "settings:content-country:default", "Default", !*g_gl ? "Selected" : "Let YouTube choose the country"); continue; }
                snprintf(id, sizeof id, "settings:content-country:%s", g_countries[i - 1].code);
                CATALOG_FOLDER( id, g_countries[i - 1].name, !strcmp(g_gl, g_countries[i - 1].code) ? "Selected" : "Country of recommendations, Trending and section shelves");
            }
            cb(1, out, n, start + n < total, ud); return 1;
        } else if (!strcmp(parent, "settings:hide") || !strncmp(parent, "settings:hide:", 14)) {
            const unsigned hidden = hide_effective();
            const char *detail = companion_known() && !companion_has(COMPANION_FILTERS) ?
                "The paired companion cannot filter lists: its library worker is off or older" : "Applied by the companion to newly loaded lists of this profile";
            for (int i = 0; i < HIDE_FLAGS; ++i) {
                char id[64], label[96]; snprintf(id, sizeof id, "settings:hide:%s", g_hide_flags[i].name);
                snprintf(label, sizeof label, "%s: %s", g_hide_flags[i].label, hidden >> i & 1 ? "on" : "off");
                CATALOG_FOLDER( id, label, detail);
            }
            CATALOG_FOLDER( "settings:hide:reset", "Restore the default", g_hide_set ? "Changed for this profile; the default hides watched videos from Home only" :
                "Nothing was changed: watched videos leave Home, nothing else is hidden");
        } else if (!strcmp(parent, "settings:thumbnails") || !strncmp(parent, "settings:thumbnails:", 20)) {
            for (int i = 0; i < 4; ++i) {
                char id[64]; snprintf(id, sizeof id, "settings:thumbnails:%s", g_thumbnail_sources[i]);
                CATALOG_FOLDER( id, g_thumbnail_labels[i], g_thumbnails == i ? "Selected" :
                    i ? "A frame of the video instead of its published thumbnail" : "The thumbnail the channel published");
            }
        } else if (!strcmp(parent, "settings:channel-sort") || !strncmp(parent, "settings:channel-sort:", 22)) {
            static const char *const labels[] = {"Account order", "By name", "New content first"};
            for (int i = 0; i < 3; ++i) {
                char id[64]; snprintf(id, sizeof id, "settings:channel-sort:%d", i);
                CATALOG_FOLDER( id, labels[i], g_channel_sort == i ? "Selected" : i == 2 ? "Channels YouTube marks as having new videos lead the list" :
                    "Order of the Subscribed channels list for this profile");
            }
        } else if (!strcmp(parent, "settings:experimental")) {
            CATALOG_FOLDER( "account:notice:experimental:0", "Rows ending in" EXPERIMENTAL " were tested offline only",
                "They were not yet checked on a PS5 or against live YouTube");
            CATALOG_FOLDER( "account:notice:experimental:1", "They may fail or change in a later version",
                "Each acts only when you select it or turn it on; your saved choices are kept");
            CATALOG_FOLDER( "account:notice:experimental:2", "Marked in this version",
                g_embedded ? "Phone pairing, chat and comment posting, account history controls, feedback actions and the profile password" :
                "Phone pairing, voice search, chat and comment posting, account history controls, feedback actions and the profile password");
            for (int i = 0; i < n; ++i) experimental(&out[i]);
        } else if (!strcmp(parent, "settings:about") || !strncmp(parent, "settings:update:", 16)) {
            /* An update row acts in place: About is listed again with its new state. */
            char title[96]; tube_about_title(title, sizeof title);
            CATALOG_FOLDER( "account:notice:version", title, TUBE_ABOUT_TAGLINE "; " TUBE_ABOUT_LICENSE);
            /* Written out, not TUBE_ABOUT_BROUGHT and TUBE_ABOUT_UNOFFICIAL: the texts of a row are found by the catalogs' extraction as literals. */
            CATALOG_FOLDER( "account:notice:author", "Brought to you by BlackBearReloaded", "Not affiliated with or endorsed by YouTube or Google");
            CATALOG_FOLDER( "settings:about:credits", "Credits", "What ProsperoTube builds on; the full notices ship with the package");
            /* Classic has no application modal; retain its update fallback only. */
            if (!(tube_afterglow_enabled ? tube_afterglow_enabled() : g_ui.interface_kind == TUBE_UI_INTERFACE_AFTERGLOW) && tube_update_app_rows) {
                tube_update_row rows[10];
                const int count = tube_update_app_rows(rows, 10);
                for (int i = 0; i < count; ++i) CATALOG_FOLDER(rows[i].id, rows[i].title, rows[i].detail);
            }
        } else if (!strcmp(parent, "settings:about:credits")) {
            const tube_about_credit *credit;
            for (int i = page * LOCAL_PAGE; n < LOCAL_PAGE && (credit = tube_about_credit_at(i)) != NULL; ++i) {
                char id[64]; snprintf(id, sizeof id, "account:notice:credit:%d", i);
                CATALOG_FOLDER( id, credit->name, credit->detail);
            }
            cb(1, out, n, tube_about_credit_at((page + 1) * LOCAL_PAGE) != NULL, ud); return 1;
        } else if (!strcmp(parent, "settings:about:changelog")) {
            /* The installed version's changes; every listed change when that version has none. */
            const char *version = tube_about_change_count(tube_about_version()) ? tube_about_version() : NULL;
            const tube_about_change *change;
            for (int i = page * LOCAL_PAGE; n < LOCAL_PAGE && (change = tube_about_change_at(version, i)) != NULL; ++i) {
                char id[64], label[64]; snprintf(id, sizeof id, "account:notice:change:%d", i);
                snprintf(label, sizeof label, "Version %.40s", change->version);
                CATALOG_FOLDER( id, change->text, label);
                copy(out[n - 1].overview, sizeof out[n - 1].overview, change->text);
            }
            cb(1, out, n, tube_about_change_at(version, (page + 1) * LOCAL_PAGE) != NULL, ud); return 1;
        } else if (!strncmp(parent, "settings:backup:import", 22)) {
            if (!strncmp(parent + 22, ":apply:", 7)) {
                char label[96], detail[160];
                snprintf(label, sizeof label, "%d channel%s added to Followed channels", g_import.added, g_import.added == 1 ? "" : "s");
                snprintf(detail, sizeof detail, "%d already followed; %d not added: the list holds %d channels", g_import.existing, g_import.full, FOLLOWS);
                CATALOG_FOLDER( "account:notice:import", label, detail);
                CATALOG_FOLDER( "subscriptions", "Followed channels", "Open the list on this PS5");
            } else {
                static char names[IMPORT_FILES][64]; const int count = import_files(names);
                for (int i = 0; i < count; ++i) {
                    char id[96], detail[96]; snprintf(id, sizeof id, "settings:backup:import:file:%s", names[i]);
                    snprintf(detail, sizeof detail, "%lld KiB; Cross reads it and shows what it lists", (import_size(names[i]) + 1023) / 1024);
                    CATALOG_FOLDER( id, names[i], detail);
                }
                if (!count) {
                    CATALOG_FOLDER( "account:notice:import", "No import file found", "Copy a NewPipe, PocketTube or GrayJay export into the import folder, then reopen");
                    snprintf(out[0].overview, sizeof out[0].overview, "Import folder: %s. Files of at most 1 MiB with letters, digits, dots, dashes and underscores in their names are listed.",
                        evo_data_path("import"));
                }
            }
        } else if (!strncmp(parent, "settings:backup", 15)) {
            static tube_backup_entry entries[LOCAL_PAGE]; char id[96], label[96], when[32];
            if (!strcmp(parent, "settings:backup:list")) {
                const int count = tube_backup_list(evo_data_dir(), entries, LOCAL_PAGE);
                for (int i = 0; i < count; ++i) {
                    snprintf(id, sizeof id, "settings:backup:entry:%.39s", entries[i].name);
                    if (tube_backup_format_time(entries[i].created, when, sizeof when)) copy(when, sizeof when, entries[i].name);
                    snprintf(label, sizeof label, "%lld KiB%s", (entries[i].bytes + 1023) / 1024, entries[i].safety ? "; the state a restore replaced" : "");
                    CATALOG_FOLDER( id, when, label);
                }
                if (count <= 0) CATALOG_FOLDER( "account:notice", "No backup yet", "Create one from Backup and restore");
            } else if (!strncmp(parent, "settings:backup:entry:", 22)) {
                const char *name = parent + 22; long long created = 0; int safety = 0;
                const int rc = page ? TUBE_BACKUP_E_ARGUMENT : tube_backup_validate(evo_data_dir(), name);
                if (rc != TUBE_BACKUP_OK || tube_backup_name_info(name, &created, &safety) || tube_backup_format_time(created, when, sizeof when)) {
                    tube_set_error(tube_backup_error_text(rc ? rc : TUBE_BACKUP_E_NAME)); cb(0,NULL,0,0,ud); return 1;
                }
                if (!++g_backup_ticket) ++g_backup_ticket;
                snprintf(id, sizeof id, "settings:backup:restore:%u:%s", g_backup_ticket, name);
                CATALOG_FOLDER( id, "Restore this backup", "Replaces the settings and library on this PS5; the current state is kept as a safety copy");
                if (g_embedded) /* nothing is paired here, and the accounts are not in the file (below) */
                    snprintf(out[0].overview, sizeof out[0].overview, "Backup of %s%s. Sign-ins to YouTube are not part of it: the accounts on this PS5 stay as they are. Cross restores; Circle returns without changing anything.",
                        when, safety ? " (the state a restore replaced)" : "");
                else snprintf(out[0].overview, sizeof out[0].overview, "Backup of %s%s. Restoring it also restores the companion pairing it held: pair again if the companion was paired since. Cross restores; Circle returns without changing anything.",
                    when, safety ? " (the state a restore replaced)" : "");
                snprintf(id, sizeof id, "settings:backup:delete:%u:%s", g_backup_ticket, name);
                CATALOG_FOLDER( id, "Delete this backup", "Removes this file only");
            } else {
                static const int periods[] = {TUBE_BACKUP_AUTO_OFF, TUBE_BACKUP_AUTO_DAILY, TUBE_BACKUP_AUTO_WEEKLY, TUBE_BACKUP_AUTO_MONTHLY};
                static const char *const names[] = {"off", "daily", "weekly", "monthly"};
                int period = 0; const int count = tube_backup_list(evo_data_dir(), entries, LOCAL_PAGE);
                for (int i = 0; i < 4; ++i) if (periods[i] == g_backup_auto) period = i;
                if (*g_backup_notice[0]) CATALOG_FOLDER( "account:notice:backup", g_backup_notice[0], g_backup_notice[1]);
                CATALOG_FOLDER( "settings:backup:create", "Create a backup now", g_embedded ?
                    "A dated copy of settings and library in the app data folder; sign-ins to YouTube are not included" :
                    "A dated copy of settings, library and companion pairing in the app data folder");
                if (count > 0 && !tube_backup_format_time(entries[0].created, when, sizeof when)) snprintf(label, sizeof label, "%d kept; newest: %s", count, when);
                else copy(label, sizeof label, "No backup yet");
                CATALOG_FOLDER( "settings:backup:list", "Restore or delete a backup", label);
                snprintf(id, sizeof id, "settings:backup:auto:%d", periods[(period + 1) % 4]);
                snprintf(label, sizeof label, "Automatic backup: %s", names[period]);
                CATALOG_FOLDER( id, label, "Select to cycle; made when ProsperoTube starts, the ten newest are kept; applies to this PS5");
                if (!embedded_lacks(COMPANION_IMPORT)) /* a built-in service that cannot read an export: not offered */
                    CATALOG_FOLDER( "settings:backup:import", "Import subscriptions", "Follow the channels of a NewPipe, PocketTube or GrayJay export");
                if (g_embedded) { /* a backup is youtube.json alone: the service's file of account tokens is never copied, read or replaced */
                    CATALOG_FOLDER( "account:notice:backup-accounts", "Sign-ins to YouTube are not part of a backup",
                        "Restoring a backup neither adds nor removes an account on this PS5");
                    snprintf(out[n - 1].overview, sizeof out[n - 1].overview, "What a backup holds for an account is used only while that sign-in is still on this PS5. An account that signs in again starts as a new profile.");
                }
            }
        } else if (!strncmp(parent, "settings:lock:", 14)) {
            CATALOG_FOLDER( "account:notice", "Enter the password", "Circle returns to the account settings without changing it");
        } else if (!strcmp(parent, "settings:accounts") || !strcmp(parent, "settings:separate-settings")) {
            CATALOG_FOLDER( "settings:separate-settings", g_separate_settings ? "Separate settings per account: on" : "Separate settings per account: off",
                "Off: every profile uses the same preferences. On: each profile keeps its own; applies to this PS5");
            if (!strcmp(g_library_profile, "guest"))
                CATALOG_FOLDER( "account:notice", "Password protection" EXPERIMENTAL, "Select an account profile to protect it with a password");
            else if (lock_find(g_library_profile) >= 0) {
                CATALOG_FOLDER( "settings:lock:change", "Change the password of this profile" EXPERIMENTAL, "Asks for the current password first");
                experimental(&out[n - 1]);
                CATALOG_FOLDER( "settings:lock:remove", "Remove the password of this profile" EXPERIMENTAL, "Asks for the current password first");
            } else CATALOG_FOLDER( "settings:lock:set", "Protect this profile with a password" EXPERIMENTAL,
                "Asked before the profile is selected; this PS5 stores a salted hash, never the text");
            experimental(&out[n - 1]);
        } else if (!strcmp(parent, "settings:speed-mode") || !strncmp(parent, "settings:speed-mode:", 20)) {
            const char *modes[] = {"all", "channel", "video", "none"};
            const char *labels[] = {"All videos", "Each channel", "Each video", "This playback only"};
            for (int i = 0; i < 4; ++i) {
                char id[64], label[96]; snprintf(id, sizeof id, "settings:speed-mode:%s", modes[i]);
                snprintf(label, sizeof label, "%s%s", i == g_speed_mode ? "Selected: " : "", labels[i]);
                CATALOG_FOLDER( id, label, "Choose where a speed selected during playback is remembered");
            }
        } else if (!strcmp(parent, "settings:speed") || !strncmp(parent, "settings:speed:", 15)) {
            const int total = tube_extra_speed_count(); /* the speed list this profile chose */
            int start = page >= 0 && page < (total + LOCAL_PAGE - 1) / LOCAL_PAGE ? page * LOCAL_PAGE : total;
            for (int i = start; i < total && n < LOCAL_PAGE; ++i) {
                char id[64], label[96];
                snprintf(id, sizeof id, "settings:speed:%d", tube_extra_speed_at(i));
                snprintf(label, sizeof label, "%s%.3gx", g_speed == tube_extra_speed_at(i) ? "Selected: " : "", tube_extra_speed_at(i) / 100.0);
                CATALOG_FOLDER( id, label, "Use this speed for all on-demand videos; live streams play at normal speed");
            }
            cb(1, out, n, start + n < total, ud); return 1;
        } else if (!strncmp(parent, "settings:remote", 15)) {
            char label[96]; const char *code = tube_remote_code();
            const int state = tube_remote_state();
            const int linked = tube_remote_connected_count();
            const int failed = g_remote_write_failed || g_remote_entropy_failed ||
                (g_remote_enabled && !g_remote_identity_saved) || state == TUBE_REMOTE_FAILED;
            const char *status = failed ? "stopped" : !g_remote_enabled ? "off" :
                state == TUBE_REMOTE_LISTENING ? (linked ? "connected" : "ready") : "connecting";
            const char *words = g_remote_write_failed ? "local data not saved" : g_remote_entropy_failed ? "random source unavailable" : !g_remote_enabled ? "off" :
                !g_remote_identity_saved ? "storage unavailable" : state == TUBE_REMOTE_FAILED ? "stopped; retry linking" :
                state == TUBE_REMOTE_LISTENING ? (linked ? "connected" : "ready") : "connecting";
            snprintf(label, sizeof label, "Phone connection: %s", words);
            CATALOG_FOLDER( "settings:remote:info", label, "YouTube app TV-code pairing" EXPERIMENTAL); experimental(&out[n - 1]);
            tube_row_fact("settings:remote:info", "state", status);
            tube_row_fact("settings:remote:info", "value", words);
            tube_row_fact("settings:remote:info", "available", embedded_lacks(COMPANION_REMOTE) ? "0" : "1");
            if (*code) {
                char formatted[20] = ""; size_t length = strlen(code), j = 0;
                for (size_t i = 0; i < length; ++i) { if (i && !(i % 3)) formatted[j++] = ' '; formatted[j++] = code[i]; }
                snprintf(label, sizeof label, "TV code: %s", formatted);
                CATALOG_FOLDER( "settings:remote:code", label, "Enter this code in the YouTube app's TV-code pairing screen");
                tube_row_fact("settings:remote:code", "value", code);
            }
            CATALOG_FOLDER( "settings:remote:pair", *code ? "Get a new TV code" : "Link a phone", "Enable connection and request a TV code");
            CATALOG_FOLDER( "settings:remote:toggle", g_remote_enabled ? "Turn phone connection off" : "Turn phone connection on", "Your local library stays available");
            tube_row_fact("settings:remote:toggle", "selected", g_remote_enabled ? "1" : "0");
            CATALOG_FOLDER( "settings:remote:reset", "Reset phone pairing", "Turn off and forget this TV's pairing identity");
            CATALOG_FOLDER( "settings:remote:disconnect", g_remote_close_on_disconnect ? "When phone disconnects: close the app" : g_remote_stop_on_disconnect ? "When phone disconnects: stop playback" : "When phone disconnects: keep playing",
                g_remote_close_on_disconnect ? "Close ProsperoTube when a linked phone disconnects, including while browsing" :
                "Stop applies to phone-started playback; disconnections may happen when the phone sleeps");
            CATALOG_FOLDER( "settings:remote:history", g_remote_no_history ? "History for phone playback: off" : "History for phone playback: on",
                "Off records no local history and sends no account reports for videos started from the phone");
            for (int i = 0; i < linked && i < 32; ++i) {
                char id[64]; snprintf(id, sizeof id, "settings:remote:device:%d", i);
                CATALOG_FOLDER(id, tube_remote_connected_name(i), "");
                out[n - 1].tube_interface = 0; /* A phone's name is user content. */
                tube_row_fact(id, "connected", "1");
            }
        } else if (!strncmp(parent, "settings:dearrow", 16)) {
            CATALOG_FOLDER( "settings:dearrow:titles", g_dearrow_titles ? "DeArrow titles: on" : "DeArrow titles: off", "Community titles; turn off to restore original titles");
            CATALOG_FOLDER( "settings:dearrow:thumbnails", g_dearrow_thumbnails ? "DeArrow thumbnails: on" : "DeArrow thumbnails: off", "Community thumbnails; changes apply to newly loaded videos");
            CATALOG_FOLDER( "settings:dearrow", "DeArrow community attribution", "Ajay Ramachandran and contributors; CC BY-NC-SA 4.0; dearrow.ajay.app");
            CATALOG_FOLDER( "settings:dearrow:about", "About DeArrow: https://dearrow.ajay.app", "Project page; open the address on another device");
            CATALOG_FOLDER( "settings:dearrow:status", "DeArrow status: https://status.sponsor.ajay.app", "Service status page; ProsperoTube does not probe it");
        } else if (!strcmp(parent, "settings:sb:minimum") || !strncmp(parent, "settings:sb:minimum:", 20)) {
            /* SmartTube's list: disabled, then 0.5 to 20 seconds. A value saved by
             * an earlier version stays listed until another one replaces it. */
            const long stored = lround(g_sb.minimum * 1000);
            if (stored > 20000 || stored % 500) {
                char label[96]; snprintf(label, sizeof label, "%g seconds", g_sb.minimum);
                CATALOG_FOLDER( "settings:sb:minimum", label, "Selected; saved by an earlier version");
            }
            for (long ms = 0; ms <= 20000; ms += 500) {
                char id[64], label[96]; snprintf(id, sizeof id, "settings:sb:minimum:%ld", ms);
                if (!ms) copy(label, sizeof label, "Disabled");
                else snprintf(label, sizeof label, "%g second%s", ms / 1000.0, ms == 1000 ? "" : "s");
                CATALOG_FOLDER( id, label, ms == stored ? "Selected" : "Ignore a skip when less than this remains of the segment");
            }
        } else if (!strcmp(parent, "settings:sb:exclusions") || !strncmp(parent, "settings:sb:exclude:", 20)) {
            char channels[RECORDS + TUBE_SB_EXCLUSIONS + 1][25]; int count = 0;
            if (tube_sb_channel_valid(g_sb_channel)) memcpy(channels[count++], g_sb_channel, 25);
            for (int i = 0; i < g_sb.excluded_count; ++i) {
                int duplicate = 0;
                for (int j = 0; j < count; ++j) if (!strcmp(channels[j], g_sb.excluded[i])) duplicate = 1;
                if (!duplicate) memcpy(channels[count++], g_sb.excluded[i], 25);
            }
            for (int i = 0; i < g_count; ++i) if (current_record(&g_records[i]) && g_records[i].subscribed &&
                tube_sb_channel_valid(g_records[i].item.parent_id)) {
                int duplicate = 0;
                for (int j = 0; j < count; ++j) if (!strcmp(channels[j], g_records[i].item.parent_id)) duplicate = 1;
                if (!duplicate) memcpy(channels[count++], g_records[i].item.parent_id, 25);
            }
            for (int i = page * LOCAL_PAGE; i < count && n < LOCAL_PAGE; ++i) {
                char id[80], label[96]; snprintf(id, sizeof id, "settings:sb:exclude:%s", channels[i]);
                const char *name = !strcmp(channels[i], g_sb_channel) ? "Current channel" : channels[i];
                for (int j = 0; j < g_count; ++j)
                    if (!strcmp(g_records[j].item.parent_id, channels[i]) && *g_records[j].item.subtitle) {
                        name = g_records[j].item.subtitle; break;
                    }
                snprintf(label, sizeof label, "%s: %.70s", tube_sb_excluded(&g_sb, channels[i]) ? "Excluded" : "Enabled", name);
                CATALOG_FOLDER( id, label, "Select to toggle; current and followed channels are listed");
            }
            cb(1, out, n, (page + 1) * LOCAL_PAGE < count, ud); return 1;
        } else if (!strcmp(parent, "settings:sb:colors") || !strncmp(parent, "settings:sb:color:", 18)) {
            for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) {
                char id[80], label[128]; snprintf(id, sizeof id, "settings:sb:color:%s", tube_sb_names[i]);
                snprintf(label, sizeof label, "%s marker: %s", tube_sb_labels[i], g_sb.colors[i] ? "on" : "off");
                CATALOG_FOLDER( id, label, "Select to toggle; independent of automatic skip actions");
            }
        } else if (!strncmp(parent, "settings:sb", 11)) {
            for (int i = 0; i < TUBE_SB_CATEGORIES; ++i) {
                static const char *const point_actions[] = {"Jump silently", "Jump with message", "Ask before jumping", "Do nothing"};
                char id[80], label[128]; snprintf(id, sizeof id, "settings:sb:action:%s", tube_sb_names[i]);
                snprintf(label, sizeof label, "%s: %s", tube_sb_labels[i],
                    i == 8 ? point_actions[g_sb.actions[i]] : tube_sb_actions[g_sb.actions[i]]);
                CATALOG_FOLDER( id, label, i == 8 ?
                    "R3 jumps to the next highlight; select to cycle; new categories load on next video" :
                    "Select to cycle the action; fetch new categories on the next video");
            }
            char label[96]; snprintf(label, sizeof label, "Ignore short segments: %g second%s", g_sb.minimum, g_sb.minimum == 1 ? "" : "s");
            CATALOG_FOLDER( "settings:sb:minimum", g_sb.minimum > 0 ? label : "Ignore short segments: disabled", "Choose 0.5 to 20 seconds, or disable");
            CATALOG_FOLDER( "settings:sb:repeat", g_sb.no_repeat ? "Don't skip again: on" : "Don't skip again: off", "Keep skipped segments when rewinding this video");
            CATALOG_FOLDER( "settings:sb:exclusions", "Excluded channels", "Toggle the current channel or a followed channel");
            CATALOG_FOLDER( "settings:sb:colors", "Color markers on timeline", "Choose categories independently of skip actions");
            CATALOG_FOLDER( "settings:sb:paid", g_paid_notifications ? "Paid-content notices: on" : "Paid-content notices: off", "Show YouTube paid-promotion disclosures when playback starts");
            if (!g_embedded) CATALOG_FOLDER( "settings:sb:alt", g_sb_alt ? "Use alternative server: on" : "Use alternative server: off",
                companion_known() && !companion_has(COMPANION_SB_ALT) ? "The paired companion was started without an alternative SponsorBlock server" :
                "For when SponsorBlock refuses to work; the companion host chooses the server");
            else if (!embedded_lacks(COMPANION_SB_SERVER)) { /* no host chooses the server here: its address is typed, the next row */
                CATALOG_FOLDER( "settings:sb:alt", g_sb_alt ? "Use alternative server: on" : "Use alternative server: off",
                    *g_sb_server ? "For when SponsorBlock refuses to work; segments then come from the server below" :
                    "Enter the alternative server below first; until then segments come from SponsorBlock");
                snprintf(label, sizeof label, "Alternative server: %.70s", g_sb_server + (*g_sb_server ? 8 : 0));
                CATALOG_FOLDER( "settings:sb:server", *g_sb_server ? label : "Alternative server: not set",
                    "Enter the https address of a SponsorBlock server; applies to this PS5");
            }
            CATALOG_FOLDER( "settings:sb:about", "About SponsorBlock: https://sponsor.ajay.app", "Project page; open the address on another device");
            CATALOG_FOLDER( "settings:sb:status", "SponsorBlock status: https://status.sponsor.ajay.app", "Service status page; ProsperoTube does not probe it");
        } else if (!strcmp(parent, "settings:search") || !strncmp(parent, "settings:search:", 16)) {
            char label[256], id[64], clipped[121];
            const int filtered = search_filters();
            if (last_search_current()) {
                copy(clipped, sizeof clipped, g_last_search.query);
                snprintf(label, sizeof label, "Search again: %s", clipped);
                snprintf(id, sizeof id, "search-again:%u", g_playlist_scope);
                CATALOG_FOLDER( id, label, "Run the last search with these options");
                CATALOG_FOLDER( "search-suggest", "Suggestions", "Completions for the last search");
            }
            snprintf(label, sizeof label, "Upload date: %s", g_search_date_labels[g_search_options.date]);
            CATALOG_FOLDER( "settings:search:date", label, "Select to cycle; filters last until ProsperoTube closes");
            snprintf(label, sizeof label, "Duration: %s", g_search_duration_labels[g_search_options.duration]);
            CATALOG_FOLDER( "settings:search:duration", label, "Select to cycle");
            snprintf(label, sizeof label, "Type: %s", g_search_type_labels[g_search_options.type]);
            CATALOG_FOLDER( "settings:search:type", label, "Select to cycle; channels and playlists open as folders");
            snprintf(label, sizeof label, "Sort by: %s", g_search_order_labels[g_search_options.order]);
            CATALOG_FOLDER( "settings:search:order", label, "Select to switch");
            for (int i = 0; i < SEARCH_FEATURES; ++i) {
                snprintf(id, sizeof id, "settings:search:feature:%s", g_search_features[i]);
                snprintf(label, sizeof label, "%s: %s", g_search_feature_labels[i], g_search_options.features >> i & 1 ? "required" : "any");
                CATALOG_FOLDER( id, label, "Select to toggle this feature filter");
            }
            CATALOG_FOLDER( "settings:search:cards", g_search_cards ? "Channels and playlists in results: on" : "Channels and playlists in results: off",
                "Saved for this profile; off lists videos only when no filter is set");
            CATALOG_FOLDER( "settings:search:reset", "Reset search filters",
                g_search_feed.unavailable == g_playlist_scope ? "The companion library worker is off: searches ignore these options" :
                filtered ? "Filters are set" : "No filter is set");
            if (last_search_current()) {
                copy(clipped, sizeof clipped, g_last_search.query);
                snprintf(label, sizeof label, "Play the first result: %s", clipped);
                snprintf(id, sizeof id, "search-first:%u", g_playlist_scope);
                CATALOG_FOLDER( id, label, "Run the last search with these options and play its first video");
            }
            tube_ui_group_label(TUBE_UI_GROUP_SEARCH_EXIT, g_ui.search_exit, clipped, sizeof clipped);
            snprintf(label, sizeof label, "%s: %s", g_tube_ui_groups[TUBE_UI_GROUP_SEARCH_EXIT].title, clipped);
            CATALOG_FOLDER( g_tube_ui_groups[TUBE_UI_GROUP_SEARCH_EXIT].id, label, "Circle on typed results; saved for this profile");
        } else if (!strncmp(parent, "settings:ui:", 12)) {
            if (ui_group < 0) { tube_set_error("Select a listed value for this interface preference."); cb(0,NULL,0,0,ud); return 1; }
            const int selected = tube_ui_group_selected(&g_ui, ui_group), count = tube_ui_group_count(ui_group);
            /* Theme lists the four themes and a row for the classic colour schemes; that row's list,
             * and a choice made in it, shows the classic ones alone. */
            const int themed = ui_group == TUBE_UI_GROUP_SCHEME, classics = themed && (ui_choice == -3 || ui_choice >= TUBE_UI_THEMES);
            for (int i = 0; i < count; ++i) {
                char id[80], label[96]; snprintf(id, sizeof id, "%s:%d", g_tube_ui_groups[ui_group].id, i);
                if (themed && (i >= TUBE_UI_THEMES) != classics) continue;
                tube_ui_group_label(ui_group, i, label, sizeof label);
                CATALOG_FOLDER( id, label, i == selected ? "Selected" : g_tube_ui_groups[ui_group].detail);
            }
            if (themed && !classics) {
                char label[192], value[96];
                tube_ui_group_label(ui_group, selected, value, sizeof value);
                if (selected >= TUBE_UI_THEMES) snprintf(label, sizeof label, "%s: %s", "Classic colours", value);
                else snprintf(label, sizeof label, "%s", "Classic colours");
                CATALOG_FOLDER( "settings:ui:scheme:classic", label, "The colour schemes of earlier versions");
            }
        } else if (!strcmp(parent, "settings:ui")) {
            for (int group = 0; group < TUBE_UI_GROUPS; ++group) {
                char label[192], value[96];
                if (group == TUBE_UI_GROUP_SEARCH_EXIT) continue; /* a row of Search options */
                tube_ui_group_label(group, tube_ui_group_selected(&g_ui, group), value, sizeof value);
                snprintf(label, sizeof label, "%s: %s", g_tube_ui_groups[group].title, value);
                CATALOG_FOLDER( g_tube_ui_groups[group].id, label, g_tube_ui_groups[group].detail);
            }
            CATALOG_FOLDER( "settings:menu", "Video actions menu", "Show, hide and order the rows of the video actions menu");
        } else if (!strcmp(parent, "settings:menu") || !strcmp(parent, "settings:menu:reset")) {
            for (int k = 0; k < TUBE_MENU_KEYS; ++k) {
                const int key = g_ui.menu_order[k]; char id[64], label[160];
                snprintf(id, sizeof id, "settings:menu:%s", g_tube_menu_keys[key].id);
                snprintf(label, sizeof label, "%s: %s", g_tube_menu_keys[key].label, g_ui.menu_hidden >> key & 1 ? "hidden" : "shown");
                CATALOG_FOLDER( id, label, "Select to show, hide or move this row");
            }
            CATALOG_FOLDER( "settings:menu:reset", "Restore the default menu", "Every row shown, in the original order");
        } else if (!strncmp(parent, "settings:menu:", 14)) {
            if (ui_key < 0) { tube_set_error("Select a row of the video actions menu."); cb(0,NULL,0,0,ud); return 1; }
            const int hidden = (int)(g_ui.menu_hidden >> ui_key & 1), rank = tube_ui_menu_rank(&g_ui, ui_key);
            char id[64], label[160];
            snprintf(id, sizeof id, "settings:menu:%s", g_tube_menu_keys[ui_key].id);
            snprintf(label, sizeof label, "%s: %s", g_tube_menu_keys[ui_key].label, hidden ? "hidden" : "shown");
            CATALOG_FOLDER( id, label, "A row of the video actions menu, in the browser and in the player");
            snprintf(id, sizeof id, "settings:menu:%s:up", g_tube_menu_keys[ui_key].id);
            if (rank > 0) CATALOG_FOLDER( id, "Move up", "Earlier in the menu");
            snprintf(id, sizeof id, "settings:menu:%s:down", g_tube_menu_keys[ui_key].id);
            if (rank < TUBE_MENU_KEYS - 1) CATALOG_FOLDER( id, "Move down", "Later in the menu");
            snprintf(id, sizeof id, "settings:menu:%s:toggle", g_tube_menu_keys[ui_key].id);
            CATALOG_FOLDER( id, hidden ? "Show row" : "Hide row", hidden ? "Return it to the menu" : "Remove it from the menu; this screen shows it again");
        } else if (!strcmp(parent, "settings:sections")) {
            for (int group = SECTION_ROOT; group <= SECTION_DISCOVER; ++group) for (int k = 0; k < SECTIONS; ++k) {
                const int s = g_section_order[k]; char id[64], label[256];
                if (g_sections[s].group != group) continue;
                snprintf(id, sizeof id, "settings:section:%d", s);
                snprintf(label, sizeof label, "%s: %s", group ? "Discover" : "Main menu", g_sections[s].label);
                CATALOG_FOLDER( id, label, g_section_hidden >> s & 1 ? "Hidden; select to show or move" : "Shown; select to hide or move");
            }
            for (int i = 0; i < g_pin_count; ++i) if (!strcmp(g_pins[i].profile, g_library_profile)) {
                char id[160], label[256]; snprintf(id, sizeof id, "settings:pin:%s", g_pins[i].id);
                snprintf(label, sizeof label, "Pinned: %s", g_pins[i].title);
                CATALOG_FOLDER( id, label, "Select to unpin, rename or move");
            }
        } else if (!strncmp(parent, "settings:section:", 17)) {
            char *end = NULL; long s = isdigit((unsigned char)parent[17]) ? strtol(parent + 17, &end, 10) : -1;
            if (s < 0 || s >= SECTIONS) { cb(0,NULL,0,0,ud); return 1; }
            int first = 1, last = 1, seen = 0; char id[64], label[256];
            for (int k = 0; k < SECTIONS; ++k) if (g_sections[g_section_order[k]].group == g_sections[s].group) {
                if (g_section_order[k] == s) seen = 1; else if (seen) last = 0; else first = 0;
            }
            const int hidden = g_section_hidden >> s & 1;
            snprintf(id, sizeof id, "settings:section:%ld", s);
            snprintf(label, sizeof label, "%s: %s", g_sections[s].label, hidden ? "hidden" : "shown");
            CATALOG_FOLDER( id, label, g_sections[s].group ? "Discover section of the selected profile" : "Main menu section of the selected profile");
            snprintf(id, sizeof id, "settings:section:%ld:up", s);
            if (!first) CATALOG_FOLDER( id, "Move up", "Earlier in its menu");
            snprintf(id, sizeof id, "settings:section:%ld:down", s);
            if (!last) CATALOG_FOLDER( id, "Move down", "Later in its menu");
            snprintf(id, sizeof id, "settings:section:%ld:toggle", s);
            CATALOG_FOLDER( id, hidden ? "Show section" : "Hide section", hidden ? "Return it to its menu" : "Remove it from its menu; YouTube preferences can show it again");
            CATALOG_FOLDER( "settings:sections", "All sections", "Show, hide and order every section and pin");
            CATALOG_FOLDER( "settings:refresh", "Refresh section", "Reload the list this section was shown in");
        } else if (!strncmp(parent, "settings:pin:", 13)) {
            int verb; const char *target = pin_target(parent + 13, &verb);
            if (!target) { tube_set_error("Select a channel or playlist to pin."); cb(0,NULL,0,0,ud); return 1; }
            const int index = pin_find(g_library_profile, target);
            char id[160], label[256];
            if (verb == 4 && g_playlist_prompt.active && !strcmp(g_playlist_prompt.pin, target)) {
                CATALOG_FOLDER( "account:notice", "Enter a section name", "Circle returns without renaming it");
            } else if (index < 0) {
                snprintf(id, sizeof id, "settings:pin:add:%s", target);
                CATALOG_FOLDER( id, "Pin to main menu", "Keep this folder on the main menu of the selected profile");
            } else {
                int first = 1, last = 1;
                for (int i = 0; i < g_pin_count; ++i) if (i != index && !strcmp(g_pins[i].profile, g_library_profile)) { if (i < index) first = 0; else last = 0; }
                snprintf(id, sizeof id, "settings:pin:%s", target); snprintf(label, sizeof label, "Pinned: %s", g_pins[index].title);
                CATALOG_FOLDER( id, label, "On the main menu of the selected profile");
                snprintf(id, sizeof id, "settings:pin:remove:%s", target);
                CATALOG_FOLDER( id, "Unpin from main menu", "Remove this pinned section; nothing else changes");
                snprintf(id, sizeof id, "settings:pin:rename:%s", target);
                CATALOG_FOLDER( id, "Rename pinned section", "Enter the name shown on the main menu");
                snprintf(id, sizeof id, "settings:pin:up:%s", target);
                if (!first) CATALOG_FOLDER( id, "Move up", "Earlier among the pinned sections");
                snprintf(id, sizeof id, "settings:pin:down:%s", target);
                if (!last) CATALOG_FOLDER( id, "Move down", "Later among the pinned sections");
            }
            if (!strncmp(target, "playlist:", 9) && strcmp(g_library_profile, "guest")) {
                snprintf(id, sizeof id, "playlist:confirm:save:%.100s", target + 9);
                CATALOG_FOLDER( id, "Save playlist to your library", "Adds it to Your playlists of the selected account; asks first");
                snprintf(id, sizeof id, "playlist:confirm:unsave:%.100s", target + 9);
                CATALOG_FOLDER( id, "Remove playlist from your library", "For a playlist saved earlier; asks first");
            }
            CATALOG_FOLDER( "settings:refresh", "Refresh section", "Reload the list this folder was shown in");
            if (!strncmp(target, "channel:", 8)) {
                if (strcmp(g_library_profile, "guest")) {
                    snprintf(id, sizeof id, "channel-watched:%s", target + 8);
                    CATALOG_FOLDER( id, "Mark channel as watched", "Clear its new-content mark in Subscribed channels");
                    snprintf(id, sizeof id, "notify:%s", target + 8);
                    CATALOG_FOLDER( id, "Subscription notifications", "Subscribe with a notification level, or change the level");
                }
                if (followed(target + 8)) {
                    snprintf(id, sizeof id, "settings:pin:unfollow:%s", target);
                    CATALOG_FOLDER( id, "Unfollow channel", "Remove it from Followed channels on this PS5");
                }
                snprintf(id, sizeof id, "link:%s", target);
                CATALOG_FOLDER( id, "Share link", "The channel's address, with a QR code");
            } else {
                snprintf(id, sizeof id, "link:playlist:%.100s", strrchr(target, ':') + 1);
                CATALOG_FOLDER( id, "Share link", "The playlist's address, with a QR code");
            }
        } else if (!strcmp(parent,"settings:history") || !strncmp(parent,"settings:history:",17)) {
            const char *labels[]={"Automatic","Enabled","Disabled"};
            const char *details[]={"Record on this PS5; the account's own YouTube setting decides what it keeps",
                "Record on this PS5 and report watched videos to the selected account",
                "No local history or resume positions and no account reports"};
            for(int i=0;i<3;++i) {
                char id[64];snprintf(id,sizeof id,"settings:history:%d",i);
                CATALOG_FOLDER(id,labels[i],g_history_state==i?"Selected":details[i]);
            }
        } else if (!strcmp(parent,"settings:boot-section") || !strncmp(parent,"settings:boot-section:",22)) {
            for(int i=0;i<BOOT_SECTIONS;++i) {
                char id[64];snprintf(id,sizeof id,"settings:boot-section:%d",i);
                CATALOG_FOLDER(id,g_boot_sections[i].label,boot_section_index(g_boot_section)==i?"Selected":"Open this section when ProsperoTube starts; saved for this profile");
            }
        } else if (!strcmp(parent,"settings:frame-rate")) {
            const int rates[]={0,30,60}; const char *labels[]={"Frame rate: automatic","Up to 30 fps","Up to 60 fps"};
            for (int i=0;i<3;++i) {
                char id[64]; snprintf(id,sizeof id,"settings:frame-rate:%d",rates[i]);
                CATALOG_FOLDER(id,labels[i],g_max_fps==rates[i]?"Selected":"Require known frame rate within this limit for the next video");
            }
        } else if (!strcmp(parent,"settings:playback-mode") || !strncmp(parent,"settings:playback-mode:",23)) {
            const int modes[]={2,3,4,5,6,0,1};
            const char *labels[]={"Continuous playback","Repeat one","Shuffle playlist","Playlist only","Reverse playlist","Stop at end","Close at end"};
            for(int i=0;i<7;++i) {
                char id[64];snprintf(id,sizeof id,"settings:playback-mode:%d",modes[i]);
                CATALOG_FOLDER(id,labels[i],g_playback_mode==modes[i]?"Selected":"Saved for this profile");
            }
        } else if (!strcmp(parent,"settings:video-rotate") || !strncmp(parent,"settings:rotate:",16)) {
            for(int degrees=0;degrees<=270;degrees+=90) {
                char id[64],label[64];snprintf(id,sizeof id,"settings:rotate:%d",degrees);
                snprintf(label,sizeof label,"Rotate %d degrees",degrees);
                CATALOG_FOLDER(id,label,tube_player_rotation_degrees()==degrees?"Selected":"Clockwise rotation; saved for this profile");
            }
        } else if (!strcmp(parent,"settings:video-aspect") || !strncmp(parent,"settings:aspect:",16)) {
            for(int i=0;i<11;++i) {
                char id[64];snprintf(id,sizeof id,"settings:aspect:%d",i);
                CATALOG_FOLDER(id,tube_aspect_labels[i],tube_player_aspect_mode()==i?"Selected":"Video aspect ratio; saved for this profile");
            }
        } else if (!strcmp(parent,"settings:pixel-ratio") || !strncmp(parent,"settings:pixel:",15)) {
            for(int i=0;i<6;++i) {
                char id[64];snprintf(id,sizeof id,"settings:pixel:%d",i);
                CATALOG_FOLDER(id,tube_pixel_labels[i],g_pixel_mode==i?"Selected":"Display correction; applies to all profiles");
            }
        } else if (!strcmp(parent,"settings:video-zoom") || !strncmp(parent,"settings:resize:",16) || !strncmp(parent,"settings:zoom:",14)) {
            const int modes[]={0,3,4,1,2};const char* labels[]={"Fit","Fit width","Fit height","Fill","Stretch"};
            for(int i=0;i<5;++i) {
                char id[64];snprintf(id,sizeof id,"settings:resize:%d",modes[i]);
                CATALOG_FOLDER(id,labels[i],!tube_player_zoom_percent() && tube_player_resize_mode()==modes[i]?"Selected":"Saved for this profile; clears percentage zoom");
            }
            for(int zoom=50;zoom<=300;++zoom)if(tube_zoom_valid(zoom)) {
                char id[64],label[64];snprintf(id,sizeof id,"settings:zoom:%d",zoom);snprintf(label,sizeof label,"Zoom %d%%",zoom);
                CATALOG_FOLDER(id,label,tube_player_zoom_percent()==zoom?"Selected":"Centered zoom relative to Fit; saved for this profile");
            }
        } else if (!strcmp(parent,"settings:time-display")) {
            const char *labels[]={"Total duration","Time remaining","Estimated finish time"};
            for(int mode=0;mode<3;++mode) {
                char id[64];snprintf(id,sizeof id,"settings:time-display:%d",mode);
                CATALOG_FOLDER(id,labels[mode],tube_player_time_mode()==mode?"Selected":"Remaining and finish times account for playback speed");
            }
        } else if (!strcmp(parent,"settings:player-ok")) {
            const char *labels[] = {"Show controls", "Show controls and pause", "Pause only", "Toggle playback speed"};
            for (int i = 0; i < 4; ++i) {
                char id[64]; snprintf(id,sizeof id,"settings:player-ok:%d",i);
                CATALOG_FOLDER(id,labels[i],tube_player_ok_mode()==i ? "Selected" : "Cross behavior while controls are hidden; saved for this profile");
            }
        } else if (!strcmp(parent,"settings:seek-interval")) {
            for(unsigned i=0;i<sizeof seek_intervals/sizeof *seek_intervals;++i) {
                int seconds=seek_intervals[i]; char id[64],label[80];
                snprintf(id,sizeof id,"settings:seek-interval:%d",seconds);
                snprintf(label,sizeof label,"Seek interval: %d second%s",seconds,seconds==1?"":"s");
                CATALOG_FOLDER(id,label,tube_player_seek_seconds()==seconds?"Selected":"Saved for this profile");
            }
        } else if (!strcmp(parent,"settings:player-seek")) {
            const char *labels[] = {"Regular seeking", "Seek with confirmation: pause", "Seek with confirmation: keep playing"};
            for (int mode = 0; mode < 3; ++mode) {
                char id[64]; snprintf(id,sizeof id,"settings:player-seek:%d",mode);
                CATALOG_FOLDER(id,labels[mode],tube_player_seek_mode()==mode ? "Selected" : "Cross commits; Circle cancels; previous pause state is retained");
            }
        } else if (!strcmp(parent,"settings:player-hide")) {
            for (int seconds = 0; seconds <= 15; ++seconds) {
                char id[64],label[96]; snprintf(id,sizeof id,"settings:player-hide:%d",seconds);
                if (!seconds) copy(label,sizeof label,"Never hide player controls");
                else snprintf(label,sizeof label,"Hide player controls after %d seconds",seconds);
                CATALOG_FOLDER(id,label,tube_player_hide_ms()==seconds*1000 ? "Selected" : "Saved for this profile");
            }
        } else if (!strcmp(parent, "settings:sleep-timer")) {
            for (int minutes = 0; minutes <= 600; minutes += 30) {
                char id[64], label[96]; snprintf(id,sizeof id,"settings:sleep:%d",minutes);
                if (!minutes) copy(label,sizeof label,"Sleep timer: disabled");
                else snprintf(label,sizeof label,"Sleep after %.1f hours without input",minutes/60.0);
                CATALOG_FOLDER(id,label,minutes==g_sleep_minutes ? "Selected" : "Input restarts the timer; expiry pauses playback");
            }
        } else if (!strcmp(parent, "settings:languages") || !strcmp(parent, "settings:audio-languages") ||
                   !strncmp(parent, "settings:lang:", 14) || !strncmp(parent, "settings:audio:", 15)) {
            /* SmartTube's language list with a reset entry and the selection mark. */
            const int audio = parent[9] == 'a';
            const char *current = audio ? g_audio_language : g_language, *prefix = audio ? "settings:audio:" : "settings:lang:";
            char id[64];
            if (audio) CATALOG_FOLDER( "settings:audio:", "Original audio", !*current ? "Selected" : "Play the track the video was published with");
            else CATALOG_FOLDER( "settings:lang:en", "Default: English", !strcmp(current, "en") ? "Selected" : "Caption language used unless another one is chosen");
            if (*current && language_index(current) < 0) {
                char label[64]; snprintf(label, sizeof label, "Language code %s", current);
                snprintf(id, sizeof id, "%s%s", prefix, current);
                CATALOG_FOLDER( id, label, "Selected; saved by an earlier version");
            }
            for (int i = 0; i < LANGUAGES; ++i) {
                if (!audio && !strcmp(g_languages[i].code, "en")) continue;
                snprintf(id, sizeof id, "%s%s", prefix, g_languages[i].code);
                CATALOG_FOLDER( id, g_languages[i].name, !strcmp(current, g_languages[i].code) ? "Selected" :
                    audio ? "Prefer this audio language when a video offers it" : "Use captions in this language when a video offers them");
            }
        } else {
            char label[96];
            if (g_max_fps) snprintf(label,sizeof label,"Frame rate: up to %d fps",g_max_fps);
            else copy(label,sizeof label,"Frame rate: automatic");
            CATALOG_FOLDER("settings:frame-rate",label,"Save a frame-rate limit for this profile");
            CATALOG_FOLDER("settings:video-rotate","Video rotation","0, 90, 180 or 270 degrees clockwise");
            CATALOG_FOLDER("settings:video-mirror",tube_player_mirror_enabled()?"Mirror video: on":"Mirror video: off","Horizontal mirror before rotation; saved for this profile");
            CATALOG_FOLDER("settings:video-aspect","Video aspect ratio","Original or an aspect override; independent of zoom");
            CATALOG_FOLDER("settings:pixel-ratio","Display pixel ratio","Display correction shared by all profiles");
            CATALOG_FOLDER("settings:video-zoom","Video sizing and zoom","Fit, width, height, fill, stretch and 50% to 300% zoom");
            const char *time_labels[]={"Time display: total duration","Time display: remaining","Time display: estimated finish"};
            CATALOG_FOLDER("settings:time-display",time_labels[tube_player_time_mode()],"Choose the playback time display for this profile");
            snprintf(label, sizeof label, "Speed for all videos: %.3gx", g_speed / 100.0);
            CATALOG_FOLDER( "settings:speed", g_speed_mode == 0 ? label : "Choose a speed for all videos", "Select a speed and enable all-videos memory");
            const char *speed_modes[] = {"all videos", "each channel", "each video", "this playback only"};
            snprintf(label, sizeof label, "Remember speed: %s", speed_modes[g_speed_mode]);
            CATALOG_FOLDER( "settings:speed-mode", label, "Video and channel memories are separate for each profile");
            snprintf(label, sizeof label, "Video codec: %s", g_codec == 1 ? "AVC" : g_codec == 2 ? "VP9" : g_codec == 3 ? "AV1" : g_codec == 4 ? "HEVC" : "Automatic");
            CATALOG_FOLDER( "settings:codec", label, "Video codec");
            CATALOG_FOLDER( "settings:captions", g_captions ? "Captions: on" : "Captions: off", "Apply when opening the next video");
            snprintf(label, sizeof label, "Caption language: %s", g_language);
            CATALOG_FOLDER( "settings:languages", label, "Choose a caption language");
            snprintf(label, sizeof label, "Audio language: %s", *g_audio_language ? g_audio_language : "default");
            CATALOG_FOLDER( "settings:audio-languages", label, "Prefer an alternate audio language when available");
            CATALOG_FOLDER( "settings:automatic", g_auto_captions ? "Automatic captions: allowed" : "Automatic captions: off", "Human captions are preferred");
            CATALOG_FOLDER( "settings:sponsor", g_sponsor ? "SponsorBlock: on" : "SponsorBlock: off", "Also enable --sponsorblock on the companion");
            CATALOG_FOLDER( "settings:dearrow", "DeArrow titles and thumbnails", "Optional community replacements; original metadata is retained");
            CATALOG_FOLDER( "settings:player-chat", g_player_chat_mode == 1 ? "Live chat sidebar: left" :
                g_player_chat_mode == 2 ? "Live chat sidebar: right" : "Live chat sidebar: off",
                "Select to cycle Off / Left / Right; shows chat beside playing video");
            if (g_sleep_minutes) snprintf(label,sizeof label,"Sleep timer: %.1f hours",g_sleep_minutes/60.0);
            else copy(label,sizeof label,"Sleep timer: disabled");
            CATALOG_FOLDER("settings:sleep-timer",label,"Choose inactivity duration; expiry pauses playback");
            const char *ok_labels[] = {"show controls", "show controls and pause", "pause only", "toggle speed"};
            snprintf(label,sizeof label,"Cross when controls are hidden: %s",ok_labels[tube_player_ok_mode()]);
            CATALOG_FOLDER("settings:player-ok",label,"Choose confirm-button behavior for this profile");
            if (tube_player_hide_ms()) snprintf(label,sizeof label,"Hide controls after %.1f seconds",tube_player_hide_ms()/1000.0);
            else copy(label,sizeof label,"Hide controls: never");
            CATALOG_FOLDER("settings:player-hide",label,"Choose controls timeout for this profile");
            const char *seek_labels[] = {"regular", "confirm while paused", "confirm while playing"};
            snprintf(label,sizeof label,"Seeking: %s",seek_labels[tube_player_seek_mode()]);
            char interval_label[80]; snprintf(interval_label,sizeof interval_label,"Seek interval: %d seconds",tube_player_seek_seconds());
            CATALOG_FOLDER("settings:seek-interval",interval_label,"Choose the Left / Right seek interval for this profile");
            CATALOG_FOLDER("settings:player-seek",label,"Choose timeline confirmation behavior for this profile");
            if (!embedded_lacks(COMPANION_REMOTE)) { /* a built-in service without the phone receiver: the row is not offered at all */
                CATALOG_FOLDER( "settings:remote", "Phone pairing" EXPERIMENTAL, "Link the YouTube app with a TV code; off until a phone is linked"); experimental(&out[n - 1]);
            }
            CATALOG_FOLDER( "settings:sb", "SponsorBlock actions", "Category actions, minimum skip, excluded channels and rewind behavior");
            CATALOG_FOLDER("settings:playback-mode","Playback mode","Choose continuation, stop or close behavior at the end of a video");
            CATALOG_FOLDER("settings:queue-respects-mode",g_queue_respects_mode?"Queue respects playback mode: on":"Queue respects playback mode: off","Off continues explicit queued videos in stop and close modes");
            CATALOG_FOLDER( "settings:ask-stream", g_ask_stream ? "Choose stream before playback: on" : "Choose stream before playback: off", "Off plays the preferred quality immediately; on shows available choices");
            const char *history_labels[] = {"History: automatic", "History: enabled", "History: disabled"};
            CATALOG_FOLDER( "settings:history", history_labels[g_history_state],
                g_watch.status < 0 ? "Last report unconfirmed; this playback session will not retry" :
                g_watch.status == 1 ? "History report pending" : g_watch.status == 2 ? "Last history report confirmed" :
                "Local history, resume positions and reports for the selected account");
            CATALOG_FOLDER( "settings:search-history", g_search_history ? "Search history: on" : "Search history: off", "Remember recent searches for this profile");
            CATALOG_FOLDER( "settings:search", "Search options", "Upload date, duration, type, features, sorting and result cards");
            CATALOG_FOLDER( "settings:sections", "Menu sections", "Show, hide and order the main menu, Discover and pinned sections");
            snprintf(label, sizeof label, "Subscribed channels: %s", g_channel_order_labels[g_channel_sort]);
            CATALOG_FOLDER( "settings:channel-sort", label, "Order of the Subscribed channels list for this profile");
            snprintf(label, sizeof label, "Start in: %s", g_boot_sections[boot_section_index(g_boot_section)].label);
            CATALOG_FOLDER( "settings:boot-section", label, "Choose the section this profile opens at startup");
            CATALOG_FOLDER( "settings:account-on-boot", g_select_account_on_boot ? "Choose account at startup: on" : "Choose account at startup: off",
                "Show the profile list each time ProsperoTube starts; applies to all profiles");
            CATALOG_FOLDER( "settings:clear-history", "Clear local history", "Remove resume positions and watched markers for this profile");
            CATALOG_FOLDER( "settings:clear-queue", "Clear play queue", "Remove this profile's queued videos; keep favorites");
            CATALOG_FOLDER( "settings:extra:captions", "Caption style, size and position", "How captions look in the player, translated captions and per-channel memory");
            CATALOG_FOLDER( "settings:extra:audio", "Player volume and audio delay", "Volume up to 300%, auto volume and sound delay");
            CATALOG_FOLDER( "settings:extra:player", "Player controls, keys and Shorts", "Clock, quality info, exit, D-pad, seek preview, speed list, Shorts rules, player buttons, screen dimming and dislikes");
            snprintf(label, sizeof label, "Content language: %s", language_index(g_hl) >= 0 ? g_languages[language_index(g_hl)].name : "default");
            CATALOG_FOLDER( "settings:content-language", label, "Language YouTube uses for titles and sections");
            snprintf(label, sizeof label, "Content country: %s", country_index(g_gl) >= 0 ? g_countries[country_index(g_gl)].name : "default");
            CATALOG_FOLDER( "settings:content-country", label, "Country YouTube uses for recommendations and Trending");
            if (tube_i18n_setting_title) CATALOG_FOLDER( TUBE_I18N_SETTING, tube_i18n_setting_title(), "Language of ProsperoTube's menus and messages");
            CATALOG_FOLDER( "settings:ui", "Interface", "Scale, video grid, card text, color scheme, video actions menu and Circle on the main menu");
            CATALOG_FOLDER( "settings:hide", "Hide content", "Shorts, mixes, watched, live and upcoming videos by section");
            snprintf(label, sizeof label, "Thumbnail source: %s", g_thumbnail_labels[g_thumbnails]);
            CATALOG_FOLDER( "settings:thumbnails", label, "Published thumbnail or a frame from the start, middle or end");
            CATALOG_FOLDER( "settings:experimental", "Experimental features", "What the (experimental) label on a row means");
            experimental(&out[n - 1]);
            CATALOG_FOLDER( "settings:accounts", "Account settings", "Separate settings per account and profile password" EXPERIMENTAL);
            CATALOG_FOLDER( "settings:backup", "Backup and restore", "Local backups, automatic backup and subscriptions import");
            tube_about_title(label, sizeof label);
            CATALOG_FOLDER( "settings:about", label, "Version, author and credits");
        }
    } else if (!strcmp(parent, "subscriptions")) {
        /* Channels followed through one of their videos, most recent first, then the imported ones. */
        static const char *channels[RECORDS + FOLLOWS], *names[RECORDS + FOLLOWS]; int indexes[RECORDS], recorded = 0, total = 0;
        for (int i = 0; i < g_count; ++i) {
            const Record *r = &g_records[i];
            if (r->subscribed && current_record(r) && *r->item.parent_id && blocked_identity(r->item.parent_id,r->item.subtitle)<0) indexes[recorded++] = i;
        }
        for (int i = 0; i < recorded; ++i) for (int j = i + 1; j < recorded; ++j)
            if (g_records[indexes[j]].updated > g_records[indexes[i]].updated) { int tmp = indexes[i]; indexes[i] = indexes[j]; indexes[j] = tmp; }
        for (int i = 0; i < recorded + g_follow_count; ++i) {
            const char *channel = i < recorded ? g_records[indexes[i]].item.parent_id : g_follows[i - recorded].channel; int duplicate = 0;
            if (i >= recorded && (strcmp(g_follows[i - recorded].profile, g_library_profile) || blocked_channel(channel) >= 0)) continue;
            for (int k = 0; k < total; ++k) if (!strcmp(channels[k], channel)) duplicate = 1;
            if (duplicate) continue;
            channels[total] = channel; names[total++] = i < recorded ? g_records[indexes[i]].item.subtitle : g_follows[i - recorded].title;
        }
        for (int i = page * LOCAL_PAGE; i < total && n < LOCAL_PAGE; ++i) {
            char id[160]; snprintf(id, sizeof id, "channel:%.24s", channels[i]);
            CATALOG_FOLDER( id, *names[i] ? names[i] : channels[i], "Followed locally on this PS5");
        }
        cb(1, out, n, (page + 1) * LOCAL_PAGE < total, ud); return 1;
    } else if (!strcmp(parent, "history") || !strcmp(parent, "favorites") || !strcmp(parent, "queue")) {
        int indexes[RECORDS], total = 0;
        for (int i = 0; i < g_count; ++i) {
            Record *r = &g_records[i];
            int include = !strcmp(parent, "history") ? r->updated > 0 : !strcmp(parent, "favorites") ? r->favorite : r->queued;
            if (include && current_record(r) && blocked_identity(r->item.parent_id,r->item.subtitle)<0) indexes[total++] = i;
        }
        for (int i = 0; i < total; ++i) for (int j = i + 1; j < total; ++j) {
            Record *a = &g_records[indexes[i]], *b = &g_records[indexes[j]];
            if ((!strcmp(parent, "queue") && (b->queue_order < a->queue_order ||
                    (b->queue_order == a->queue_order && indexes[j] < indexes[i]))) ||
                (strcmp(parent, "queue") && b->updated > a->updated)) {
                int tmp = indexes[i]; indexes[i] = indexes[j]; indexes[j] = tmp;
            }
        }
        for (int i = page * LOCAL_PAGE; i < total && n < LOCAL_PAGE; ++i) {
            Record *r = &g_records[indexes[i]];
            out[n] = r->item;
            /* Phone playback can create history without ever loading a catalog thumbnail. */
            if (!*out[n].art_url)
                snprintf(out[n].art_url, sizeof out[n].art_url, "https://i.ytimg.com/vi/%s/hqdefault.jpg", r->item.id);
            dearrow_item(&out[n], r); ++n;
        }
        cb(1, out, n, (page + 1) * LOCAL_PAGE < total, ud); return 1;
    } else return 0;
    int offset=page*LOCAL_PAGE;
    int count=offset<n ? n-offset : 0;
    if(count>LOCAL_PAGE)count=LOCAL_PAGE;
    cb(1, count ? out+offset : NULL, count, offset+count<n, ud);return 1;
}

#undef LINK_ROW
#undef CATALOG_FOLDER

/* Takes the operation; a request that cannot be sent completes as failed. */
static int library_job(Request *r, cJSON *operation) {
    cJSON *job = cJSON_CreateObject();
    if (!job || !operation) { cJSON_Delete(job); cJSON_Delete(operation); complete(r, 0); return -1; }
    cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", operation);
    char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
    if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
    copy(r->payload, sizeof r->payload, body); free(body);
    begin(r); return 0;
}
static int account_notice(evo_provider_items_cb cb, void *ud) {
    evo_provider_item_t item;
    folder(&item, "account:notice", "Choose an account profile first", "Discover > Accounts & library > Profile");
    cb(1, &item, 1, 0, ud); return 0;
}
/* Filters or mixed result cards need the companion's library search. Its first
 * page opens a retained feed; later pages continue that feed by cursor. */
static int search_feed_current(const char *parent) {
    return *g_search_feed.key && !strcmp(g_search_feed.parent, parent) && g_search_feed.scope == g_playlist_scope &&
        !strcmp(g_search_feed.profile, g_library_profile);
}
static int library_search(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    if (page && (page >= LIBRARY_PAGES || strcmp(g_library_parent, g_search_feed.key) || !*g_library_cursors[page])) {
        tube_set_error("Search again to continue browsing these results."); cb(0, NULL, 0, 0, ud); return 0;
    }
    if (!page) {
        memset(&g_search_feed, 0, sizeof g_search_feed);
        static unsigned serial; if (!++serial) ++serial;
        g_search_feed.serial = serial; snprintf(g_search_feed.key, sizeof g_search_feed.key, "search-feed:%u", serial);
        copy(g_search_feed.parent, sizeof g_search_feed.parent, parent);
        copy(g_search_feed.profile, sizeof g_search_feed.profile, g_library_profile); g_search_feed.scope = g_playlist_scope;
        copy(g_library_parent, sizeof g_library_parent, g_search_feed.key); memset(g_library_cursors, 0, sizeof g_library_cursors);
    }
    Request *r = request_new(3, ud); if (!r) return -1;
    r->items_cb = cb; r->page = page; r->library_search = 1; r->search_rows = !page;
    copy(r->parent, sizeof r->parent, g_search_feed.key);
    cJSON *operation = cJSON_CreateObject(), *features = page ? NULL : cJSON_CreateArray();
    cJSON_AddStringToObject(operation, "action", page ? "continue" : "search");
    cJSON_AddStringToObject(operation, "profile", g_library_profile);
    if (page) cJSON_AddStringToObject(operation, "cursor", g_library_cursors[page]);
    else {
        cJSON_AddStringToObject(operation, "query", parent + 7);
        cJSON_AddStringToObject(operation, "upload_date", g_search_dates[g_search_options.date]);
        cJSON_AddStringToObject(operation, "duration", g_search_durations[g_search_options.duration]);
        cJSON_AddStringToObject(operation, "type", g_search_types[g_search_options.type]);
        for (int i = 0; i < SEARCH_FEATURES; ++i)
            if (g_search_options.features >> i & 1) cJSON_AddItemToArray(features, cJSON_CreateString(g_search_features[i]));
        cJSON_AddItemToObject(operation, "features", features);
        cJSON_AddStringToObject(operation, "order", g_search_orders[g_search_options.order]);
    }
    return library_job(r, operation);
}

/* "<scope>:<index>", whole or cut short: the name of a listed completion (numbers and colons alone), as
 * against a text that is being typed. */
static int suggestion_row(const char *value) {
    return strspn(value, "0123456789:") == strlen(value);
}

static int catalog(const char *parent, int page, evo_provider_items_cb cb, void *ud) {
    if (!cb || page < 0 || page > 1000) return -1;
    /* LinkPhone polls this read-only snapshot while a cast is resolving. Do not
     * cancel that request or reset its launch context as an ordinary browse does. */
    if (parent && !strcmp(parent, "settings:remote"))
        return local_catalog(parent, page, cb, ud) ? 0 : -1;
    if (parent && !strcmp(parent, "library:notification-count")) {
        if (page || !strcmp(g_library_profile, "guest")) { cb(0, NULL, 0, 0, ud); return 0; }
        Request *r = request_new(17, ud); if (!r) return -1;
        r->items_cb = cb; r->intent = g_playlist_scope;
        copy(r->parent, sizeof r->parent, parent); copy(r->profile, sizeof r->profile, g_library_profile);
        cJSON *operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "action", "notification_count");
        cJSON_AddStringToObject(operation, "profile", g_library_profile);
        return library_job(r, operation);
    }
    if (parent && !strcmp(parent, "settings:service-retry")) {   /* then the main menu again, with or without the row */
        if (g_embedded && !tube_embedded_start()) {
            char message[240];
            snprintf(message, sizeof message, "The built-in YouTube service could not start: %s.", g_embedded_error);
            tube_set_error(message);
        }
        parent = ""; page = 0;
    }
    const int startup = g_startup_open && parent && *parent && !page && !g_select_account_on_boot && !strcmp(parent, tube_boot_section());
    if (parent && *parent) g_startup_open = 0;
    char rerun[816];
    int typed = g_search_typed && parent && !strncmp(parent,"search:",7);
    g_search_typed = 0;
    if (parent && !strncmp(parent,"search-history:run:",19)) {
        int index = search_history_find(parent+19);
        if (index < 0) { tube_set_error("Reopen recent searches for the current profile."); cb(0,NULL,0,0,ud); return 0; }
        snprintf(rerun,sizeof rerun,"search:%s",g_searches[index].query);
        if (!page) search_history_add(rerun+7);
        parent = rerun; typed = 1;
    } else if (parent && ((!strncmp(parent,"search-suggest:run:",19) && suggestion_row(parent+19)) || !strncmp(parent,"search-again:",13))) {
        /* Both rows are named by the browsing scope: a stale or foreign row searches nothing. */
        const int again = parent[7] == 'a';
        char expected[64]; int index = -1;
        for (int i = 0; !again && i < g_suggestions.count; ++i) {
            snprintf(expected,sizeof expected,"search-suggest:run:%u:%d",g_playlist_scope,i);
            if (!strcmp(parent,expected) && g_suggestions.scope == g_playlist_scope && !strcmp(g_suggestions.profile,g_library_profile)) index = i;
        }
        snprintf(expected,sizeof expected,"search-again:%u",g_playlist_scope);
        if (again ? strcmp(parent,expected) || !last_search_current() : index < 0) {
            tube_set_error("Search again for the current profile."); cb(0,NULL,0,0,ud); return 0;
        }
        snprintf(rerun,sizeof rerun,"search:%s",again ? g_last_search.query : g_suggestions.text[index]);
        if (!page) search_history_add(rerun+7);
        parent = rerun; typed = 1;
    }
    if (typed && !page) {
        copy(g_last_search.query,sizeof g_last_search.query,parent+7);
        copy(g_last_search.profile,sizeof g_last_search.profile,g_library_profile); g_last_search.scope = g_playlist_scope;
    }
    const char *section = topic_section(parent);
    if(!parent || (strncmp(parent,"account:menu:",13) && strncmp(parent,"local:video:",12)))
        g_browse_music = section && !strcmp(section, "music");
    g_browse_queue = parent && !strcmp(parent, "queue");
    memset(&g_launch_origin, 0, sizeof g_launch_origin);
    memset(&g_incognito, 0, sizeof g_incognito); /* only its own action screen re-arms it */
    g_menu_launch.id[0] = 0;
    if (parent && !strncmp(parent, "auth:", 5)) {
        if (page || (strcmp(parent, "auth:start") && strcmp(parent, "auth:session") &&
            strcmp(parent, "auth:cancel") && strncmp(parent, "auth:info", 9))) {
            tube_set_error("The sign-in screen was invalid."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!strcmp(parent, "auth:cancel")) { tube_cancel(); return auth_rows(cb, ud); }
        if (strcmp(parent, "auth:start") || g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING)
            return auth_rows(cb, ud);
        tube_cancel();
        unsigned serial = g_auth.serial + 1, revision = g_auth.revision + 1;
        memset(&g_auth, 0, sizeof g_auth); g_auth.serial = serial ? serial : 1;
        g_auth.revision = revision; g_auth.state = AUTH_STARTING;
        Request *r = request_new(9, ud);
        if (!r) { g_auth.state = AUTH_FAILED; ++g_auth.revision; cb(0, NULL, 0, 0, ud); return 0; }
        r->items_cb = cb; r->intent = g_auth.serial;
        copy(r->payload, sizeof r->payload, "{\"action\":\"auth\",\"auth\":{\"action\":\"start\"}}");
        return begin(r);
    }
    /* The video menu itself may exceed one page; its action screens never do. */
    if (parent && ((!strncmp(parent, "account:", 8) && strncmp(parent, "account:menu:", 13)) ||
        (!strncmp(parent, "playlist:", 9) && strchr(parent + 9, ':') && strncmp(parent, "playlist:pick:", 14))) && page != 0) {
        tube_set_error("Account action screens do not have additional pages."); cb(0, NULL, 0, 0, ud); return 0;
    }
    if (parent && !strncmp(parent, "account:apply:", 14) && g_account_intent.state)
        return local_catalog(parent, page, cb, ud) ? 0 : -1;
    if (parent && !strncmp(parent, "chat:", 5)) {
        if (g_chat.player) tube_chat_stop();
        memset(&g_message, 0, sizeof g_message);
        memset(&g_comments[LOCAL_PAGE], 0, sizeof g_comments[LOCAL_PAGE]);
        const char *id = NULL, *mode = "top";
        if (!strncmp(parent, "chat:top:", 9)) id = parent + 9;
        else if (!strncmp(parent, "chat:live:", 10)) { id = parent + 10; mode = "live"; }
        if (page || !valid_id(id)) {
            tube_set_error("Select a valid live video to open chat."); cb(0, NULL, 0, 0, ud); return 0;
        }
        int reuse = (g_chat.state == 2 || g_chat.state == 3) && !strcmp(parent, g_chat.parent) &&
            !strcmp(g_chat.profile, g_library_profile) && valid_cursor(g_chat.cursor);
        int64_t reader_position = !g_chat.player && !strcmp(id, g_chat.video) &&
            !strcmp(g_chat.profile, g_library_profile) ? g_chat.reader_position_ms : 0;
        cancel_requests(reuse);
        if (!reuse) {
            g_chat.state = 1;
            g_chat.reader_page = 0; g_chat.reader_more = 0;
            copy(g_chat.parent, sizeof g_chat.parent, parent); copy(g_chat.video, sizeof g_chat.video, id);
            copy(g_chat.mode, sizeof g_chat.mode, mode); copy(g_chat.profile, sizeof g_chat.profile, g_library_profile);
        }
        g_chat.reader_position_ms = reader_position;
        Request *r = request_new(10, ud); if (!r) { g_chat.state = -1; return -1; }
        r->items_cb = cb; r->intent = g_chat.serial; copy(r->profile, sizeof r->profile, g_library_profile);
        r->has_chat_position = 1; r->chat_position_ms = reader_position;
        r->chat_page = g_chat.reader_page;
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"%s\",\"id\":\"%s\",\"chat_mode\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\",\"chat_history\":true,\"chat_position_sec\":%.3f,\"chat_page\":%d}",
            reuse ? "chat_poll" : "live_chat", id, mode, g_library_profile, reuse ? g_chat.cursor : "", reader_position / 1000.0, r->chat_page);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "message:", 8)) {
        for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 10) {
            tube_set_error("Wait for the chat update before composing a message."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!strncmp(parent, "message:send:", 13) && g_message.phase != 2)
            return local_catalog(parent, page, cb, ud) ? 0 : -1;
        cancel_requests(1);
        if (local_catalog(parent, page, cb, ud)) return 0;
        Request *r = request_new(11, ud); if (!r) { g_message.phase = -1; return -1; }
        r->items_cb = cb; r->intent = g_message.ticket; g_message.phase = 3;
        cJSON *job = cJSON_CreateObject(), *operation = cJSON_CreateObject();
        if (!job || !operation) { cJSON_Delete(job); cJSON_Delete(operation); complete(r, 0); return -1; }
        cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", operation);
        cJSON_AddStringToObject(operation, "action", "chat_send");
        cJSON_AddStringToObject(operation, "profile", g_message.profile);
        cJSON_AddStringToObject(operation, "id", g_message.video);
        cJSON_AddStringToObject(operation, "cursor", g_message.cursor);
        cJSON_AddStringToObject(operation, "session", g_message.session);
        cJSON_AddStringToObject(operation, "text", g_message.text);
        char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body); begin(r); return 0;
    }
    if (!tube_player_menu_calling()) tube_cancel(); /* an in-player listing leaves playback requests alone */
    if (local_catalog(parent, page, cb, ud)) return 0;
    if (parent && (!strcmp(parent, "search-suggest") || !strncmp(parent, "search-suggest:", 15) || !strncmp(parent, "description:", 12))) {
        /* "search-suggest:<text>" completes a text while it is typed: nothing is searched, kept in
         * the history or made the last search by it. */
        const int suggest = parent[0] == 's', typed_text = suggest && parent[14] == ':';
        if (typed_text && !page && !tube_search_query_valid(parent + 15)) {
            tube_set_error("Search again for the current profile."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page || (!suggest && !valid_id(parent + 12))) {
            tube_set_error(suggest ? "Suggestions do not have additional pages." : "Select a video to read its description.");
            cb(0, NULL, 0, 0, ud); return 0;
        }
        Request *r = request_new(suggest ? 16 : 15, ud); if (!r) return -1;
        r->items_cb = cb; r->typed_suggest = typed_text; copy(r->parent, sizeof r->parent, parent);
        cJSON *operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "action", suggest ? "search_suggestions" : "description");
        cJSON_AddStringToObject(operation, "profile", g_library_profile);
        /* Without a search in this session the service lists its default completions. */
        if (typed_text) cJSON_AddStringToObject(operation, "query", parent + 15);
        else if (suggest) cJSON_AddStringToObject(operation, "query", last_search_current() ? g_last_search.query : "");
        else cJSON_AddStringToObject(operation, "id", parent + 12);
        return library_job(r, operation);
    }
    if (parent && !strncmp(parent, "search-preview:", 15)) {
        /* The best matches for a text while it is typed: the first videos of a search for it. It is
         * not a search the person ran: no history, no last search, no options, one page. */
        if (page || !tube_search_query_valid(parent + 15)) {
            tube_set_error(page ? "Search again to continue browsing these results." : "Search again for the current profile."); cb(0, NULL, 0, 0, ud); return 0;
        }
        /* With the library worker the rows are cards (views, age); without it the plain video search. */
        const int cards = companion_known() ? companion_has(COMPANION_LIBRARY) : g_embedded;
        Request *r = request_new(cards ? 3 : 1, ud); if (!r) return -1;
        r->items_cb = cb; copy(r->parent, sizeof r->parent, parent);
        cJSON *operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "action", "search");
        cJSON_AddStringToObject(operation, "profile", g_library_profile);
        cJSON_AddStringToObject(operation, "query", parent + 15);
        if (cards) return library_job(r, operation);
        cJSON_AddNumberToObject(operation, "page", 0);
        char *body = cJSON_PrintUnformatted(operation); cJSON_Delete(operation);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body);
        begin(r); return 0;
    }
    if (parent && (!strncmp(parent, "channel-tab:", 12) || !strncmp(parent, "channel-search:run:", 19) ||
        (page && valid_channel_group(parent) && (!strcmp(parent, g_library_parent) || companion_has(COMPANION_CHANNELS))))) {
        /* A section of a channel page, a search inside the channel, or a later
         * page of the channel's own list: companion lists continued by cursor. */
        const int searching = parent[8] == 's', root = parent[7] == ':';
        int tab = -1, sort = -1; char expected[48];
        snprintf(expected, sizeof expected, "channel-search:run:%u", g_channel_query.serial);
        if (searching ? !g_channel_query.serial || strcmp(parent, expected) || g_channel_query.scope != g_playlist_scope ||
                strcmp(g_channel_query.profile, g_library_profile) : !root && !channel_tab_parse(parent, &tab, &sort)) {
            tube_set_error("Reopen this channel to browse its sections."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload this channel section to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!page) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(3, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; r->channel_page = !searching; copy(r->parent, sizeof r->parent, parent);
        cJSON *operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "action", page ? "continue" : searching ? "channel_search" : "channel");
        cJSON_AddStringToObject(operation, "profile", g_library_profile);
        if (page) cJSON_AddStringToObject(operation, "cursor", g_library_cursors[page]);
        else if (searching) {
            cJSON_AddStringToObject(operation, "channel", g_channel_query.channel);
            cJSON_AddStringToObject(operation, "query", g_channel_query.query);
        } else {
            cJSON_AddStringToObject(operation, "channel", channel_of(parent));
            cJSON_AddStringToObject(operation, "tab", g_channel_tabs[tab]);
            if (sort >= 0) cJSON_AddStringToObject(operation, "channel_sort", g_channel_sorts[sort]);
        }
        if (!page) { r->wants_health = 1; r->requires = COMPANION_CHANNELS; }
        return library_job(r, operation);
    }
    if (parent && (!strncmp(parent, "details:", 8) || !strncmp(parent, "reminder:", 9) || !strncmp(parent, "playlist-order:", 15) ||
        !strncmp(parent, "channel-watched:", 16) || !strncmp(parent, "search-first:", 13))) {
        /* One companion answer shown as rows; each needs a companion that lists the feature. */
        const char *value = strchr(parent, ':') + 1; const int guest = !strcmp(g_library_profile, "guest");
        cJSON *operation = NULL; unsigned requires = 0; char expected[32];
        snprintf(expected, sizeof expected, "search-first:%u", g_playlist_scope);
        if (page) { tube_set_error("This screen does not have additional pages."); cb(0, NULL, 0, 0, ud); return 0; }
        if (guest && (parent[0] == 'p' || parent[0] == 'c')) return account_notice(cb, ud);
        if (parent[0] == 'c' && valid_channel(value)) {
            /* Remembered here first; the companion then opens the uploads, which clears YouTube's own mark. */
            channel_mark_put(value, ""); g_channel_list.complete = 0;
            if (g_channel_marks_dirty) save();
            if (companion_known() && !companion_has(COMPANION_CHANNELS)) {
                evo_provider_item_t item; folder(&item, "account:notice", "Channel marked as watched", "Remembered on this PS5 for the selected profile");
                cb(1, &item, 1, 0, ud); return 0;
            }
        }
        if ((parent[0] == 'd' || parent[0] == 'r') ? !valid_id(value) : parent[0] == 'p' ? !valid_playlist(value) :
            parent[0] == 'c' ? !valid_channel(value) : strcmp(parent, expected) || !last_search_current()) {
            tube_set_error(parent[0] == 's' ? "Search again for the current profile." : "Select a video, playlist or channel first."); cb(0, NULL, 0, 0, ud); return 0;
        }
        operation = cJSON_CreateObject();
        cJSON_AddStringToObject(operation, "profile", g_library_profile);
        if (parent[0] == 'd') { cJSON_AddStringToObject(operation, "action", "video_details"); cJSON_AddStringToObject(operation, "id", value); requires = COMPANION_DETAILS; }
        else if (parent[0] == 'r') {
            cJSON_AddStringToObject(operation, "action", "stream_reminder"); cJSON_AddStringToObject(operation, "id", value);
            cJSON_AddStringToObject(operation, "reminder", "status"); requires = COMPANION_REMINDER;
        } else if (parent[0] == 'p') {
            cJSON_AddStringToObject(operation, "action", "playlist_order"); cJSON_AddStringToObject(operation, "playlist", value); requires = COMPANION_PLAYLIST_ORDER;
        } else if (parent[0] == 'c') {
            cJSON_AddStringToObject(operation, "action", "channel"); cJSON_AddStringToObject(operation, "channel", value);
            cJSON_AddStringToObject(operation, "tab", "videos"); requires = COMPANION_CHANNELS;
        } else { /* the last typed search with its options, answered by its first playable video */
            cJSON *features = cJSON_AddArrayToObject(operation, "features");
            search_filters();
            cJSON_AddStringToObject(operation, "action", "search"); cJSON_AddStringToObject(operation, "query", g_last_search.query);
            cJSON_AddStringToObject(operation, "upload_date", g_search_dates[g_search_options.date]);
            cJSON_AddStringToObject(operation, "duration", g_search_durations[g_search_options.duration]);
            cJSON_AddStringToObject(operation, "type", g_search_types[g_search_options.type]);
            for (int i = 0; features && i < SEARCH_FEATURES; ++i)
                if (g_search_options.features >> i & 1) cJSON_AddItemToArray(features, cJSON_CreateString(g_search_features[i]));
            cJSON_AddStringToObject(operation, "order", g_search_orders[g_search_options.order]);
            cJSON_AddBoolToObject(operation, "first_playable", 1); requires = COMPANION_SEARCH_FIRST;
        }
        Request *r = request_new(17, ud); if (!r) { cJSON_Delete(operation); return -1; }
        r->items_cb = cb; copy(r->parent, sizeof r->parent, parent); r->wants_health = 1; r->requires = requires;
        return library_job(r, operation);
    }
    if (parent && !strncmp(parent, "settings:backup:import:file:", 28)) {
        /* The export file a user copied into the import folder is parsed by the companion. */
        for (int i = 0; i < REQUESTS; ++i) if (g_requests[i].used && g_requests[i].kind == 18) {
            tube_set_error("An import file is still being read. Retry in a moment."); cb(0, NULL, 0, 0, ud); return 0;
        }
        import_release(); g_import.body = import_read(parent + 28);
        if (!g_import.body) {
            tube_set_error("This file cannot be read as an export: it is missing, larger than 1 MiB or not text."); cb(0, NULL, 0, 0, ud); return 0;
        }
        copy(g_import.file, sizeof g_import.file, parent + 28);
        Request *r = request_new(18, ud); if (!r) { import_release(); return -1; }
        r->items_cb = cb; copy(r->parent, sizeof r->parent, parent); r->wants_health = 1; r->requires = COMPANION_IMPORT;
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "home-topic:", 11)) {
        /* A topic as one shelf of Home: the videos of the topic's first rows, which the service
         * takes from the one request that lists the rows. One page; the topic itself has the rest. */
        static const char *const topics[] = {"music", "gaming", "news", "sports", "live"};
        int known = 0;
        for (size_t i = 0; i < sizeof topics / sizeof *topics; ++i) if (!strcmp(parent + 11, topics[i])) known = 1;
        if (!known || page) { tube_set_error("Reload this section to continue browsing."); cb(0, NULL, 0, 0, ud); return 0; }
        Request *r = request_new(3, ud); if (!r) return -1;
        r->items_cb = cb; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"topic_videos\",\"profile\":\"%s\",\"section\":\"%s\"}}",
            g_library_profile, parent + 11);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "section:", 8)) {
        if (!section || page >= LIBRARY_PAGES ||
            (page && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload this section to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!strcmp(section, "my_videos") && !strcmp(g_library_profile, "guest")) return account_notice(cb, ud);
        if (!page) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        const char *key = parent + 8 + strlen(section);
        Request *r = request_new(3, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; r->startup = startup; copy(r->parent, sizeof r->parent, parent);
        if (!page && !strcmp(section, "kids")) { r->wants_health = 1; r->requires = COMPANION_KIDS; } /* an older worker has no Kids client */
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"section\":\"%s\",\"id\":\"%s\",\"cursor\":\"%s\"}}",
            page ? "continue" : *key ? "topic_open" : "topics", g_library_profile, section,
            *key ? key + 1 : "", page ? g_library_cursors[page] : "");
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "account:apply:", 14)) {
        Request *r = request_new(6, ud); if (!r) return -1;
        r->items_cb = cb; r->intent = g_account_intent.serial;
        g_account_intent.state = 1;
        cJSON *job = cJSON_CreateObject(), *operation = cJSON_CreateObject();
        if (!job || !operation) { cJSON_Delete(job); cJSON_Delete(operation); complete(r, 0); return -1; }
        cJSON_AddStringToObject(job, "action", "library"); cJSON_AddItemToObject(job, "library", operation);
        const unsigned feedback = feedback_option(g_account_intent.action);
        cJSON_AddStringToObject(operation, "action", feedback ? "feedback" : g_account_intent.action);
        cJSON_AddStringToObject(operation, "profile", g_account_intent.profile);
        cJSON_AddStringToObject(operation, "id", g_account_intent.identity);
        if(feedback) { /* the reference is spent by this one submission */
            Record *card=record(g_account_intent.identity,0);
            if(!feedback_current(card,feedback) || strcmp(card->feedback_cursor,g_account_intent.history_cursor)) {
                tube_set_error("Reload the list before sending feedback for this video.");
                cJSON_Delete(job);complete(r,0);return 0;
            }
            cJSON_AddStringToObject(operation,"cursor",g_account_intent.history_cursor);
            cJSON_AddStringToObject(operation,"feedback",g_account_intent.action);
            card->feedback_cursor[0]=0;
        }
        cJSON_AddStringToObject(operation, "playlist", g_account_intent.playlist);
        cJSON_AddStringToObject(operation, "title", g_account_intent.title);
        if(!strcmp(g_account_intent.action,"mark_watched")) {
            char session[65];
            snprintf(session,sizeof session,"watched-%llx-%lx-%llx-%u",(unsigned long long)time(NULL),
                     (unsigned long)getpid(),(unsigned long long)now_ms(),g_account_intent.serial);
            cJSON_AddStringToObject(operation,"session",session);
        }
        if(history_control(g_account_intent.action) || !strcmp(g_account_intent.action,"playlist_save") ||
           !strcmp(g_account_intent.action,"playlist_unsave")) {
            /* One confirmation is one attempt on the companion; an older worker has no such action. */
            char session[65];
            snprintf(session,sizeof session,"intent-%llx-%lx-%llx-%u",(unsigned long long)time(NULL),
                     (unsigned long)getpid(),(unsigned long long)now_ms(),g_account_intent.serial);
            cJSON_AddStringToObject(operation,"session",session);
            r->wants_health=1;r->requires=COMPANION_CHANNELS;
        }
        if(!strcmp(g_account_intent.action,"history_remove")) {
            Record *history=record(g_account_intent.identity,0);
            if(!history || history->history_scope!=g_playlist_scope || history->history_deadline<=now_ms() ||
               strcmp(history->history_remove_cursor,g_account_intent.history_cursor) || !valid_cursor(g_account_intent.history_cursor)) {
                tube_set_error("Reload YouTube history before removing this video.");
                cJSON_Delete(job);complete(r,0);return 0;
            }
            cJSON_AddStringToObject(operation,"cursor",g_account_intent.history_cursor);
            history->history_remove_cursor[0]=0;
        }
        if (!strcmp(g_account_intent.action, "comment_rate")) {
            cJSON_AddStringToObject(operation, "cursor", g_account_intent.playlist);
            cJSON_AddStringToObject(operation, "rating", g_account_intent.identity);
        }
        const char *intent = g_account_intent.action;
        const int commenting = !strcmp(intent, "comment_post") || !strcmp(intent, "comment_reply");
        const unsigned third = !strcmp(intent, "stream_reminder") ? COMPANION_REMINDER : !strcmp(intent, "playlist_order") ? COMPANION_PLAYLIST_ORDER :
            !strcmp(intent, "subscribe_notifications") || (!strcmp(intent, "subscribe") && *g_account_intent.argument) ? COMPANION_NOTIFICATIONS :
            commenting ? COMPANION_COMMENT_POST : 0;
        if (third) { /* fields an older companion refuses: sent only where the feature is listed */
            if (!strcmp(intent, "comment_reply") && !valid_cursor(g_account_intent.argument)) {
                tube_set_error("Reload the comments before replying."); cJSON_Delete(job); complete(r, 0); return 0;
            }
            if (third != COMPANION_NOTIFICATIONS) { /* a notification level is idempotent and takes no session */
                snprintf(g_account_intent.session, sizeof g_account_intent.session, "intent-%llx-%lx-%llx-%u", (unsigned long long)time(NULL),
                    (unsigned long)getpid(), (unsigned long long)now_ms(), g_account_intent.serial);
                cJSON_AddStringToObject(operation, "session", g_account_intent.session);
            }
            if (third == COMPANION_REMINDER) cJSON_AddStringToObject(operation, "reminder", g_account_intent.argument);
            if (third == COMPANION_PLAYLIST_ORDER) cJSON_AddStringToObject(operation, "playlist_order", g_account_intent.argument);
            if (third == COMPANION_NOTIFICATIONS) cJSON_AddStringToObject(operation, "notifications", g_account_intent.argument);
            if (commenting) { /* the text travels as text, never as a playlist title */
                cJSON_DeleteItemFromObjectCaseSensitive(operation, "title");
                cJSON_AddStringToObject(operation, "text", g_account_intent.title);
                if (!strcmp(intent, "comment_reply")) cJSON_AddStringToObject(operation, "cursor", g_account_intent.argument);
            }
            r->wants_health = 1; r->requires = third;
        }
        char *body = cJSON_PrintUnformatted(job); cJSON_Delete(job);
        if (!body || strlen(body) >= sizeof r->payload) { free(body); complete(r, 0); return -1; }
        copy(r->payload, sizeof r->payload, body); free(body);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "playlist:pick:", 14)) {
        int add = !strncmp(parent, "playlist:pick:add:", 18), remove = !strncmp(parent, "playlist:pick:remove:", 21);
        const char *video = add ? parent + 18 : remove ? parent + 21 : "";
        if ((!add && !remove) || !valid_id(video) || !strcmp(g_library_profile, "guest")) {
            tube_set_error("Select a video and an account profile first."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload the playlist picker to continue."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (!page) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(7, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload, "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\"}}",
            page ? "continue" : "playlists", g_library_profile, page ? g_library_cursors[page] : "");
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "discussion:", 11)) {
        const char *action = parent + 11, *identity = NULL, *sort = "top";
        int replies = !strncmp(action, "replies:", 8);
        if (replies) identity = action + 8;
        else if (!strncmp(action, "top:", 4)) identity = action + 4;
        else if (!strncmp(action, "newest:", 7)) { identity = action + 7; sort = "newest"; }
        if (!identity || (replies ? !valid_cursor(identity) : !valid_id(identity))) {
            tube_set_error("The selected discussion was invalid."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload the discussion to continue reading."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page == 0) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        if (page == 0 && !replies) {
            memset(&g_comment_thread, 0, sizeof g_comment_thread); copy(g_comment_thread.video, sizeof g_comment_thread.video, identity);
            copy(g_comment_thread.profile, sizeof g_comment_thread.profile, g_library_profile); g_comment_thread.scope = g_playlist_scope;
        }
        Request *r = request_new(5, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"sort\":\"%s\",\"cursor\":\"%s\",\"id\":\"%s\"}}",
            page ? "continue" : replies ? "replies" : "comments", g_library_profile, sort,
            page ? g_library_cursors[page] : replies ? identity : "", replies ? "" : identity);
        begin(r); return 0;
    }
    if (parent && !strncmp(parent, "library:", 8)) {
        const char *action = parent + 8;
        int playlist = !strncmp(action, "playlist:", 9) && valid_catalog_id(parent);
        int suggestions = !strncmp(action, "suggestions:", 12) && valid_id(action+12);
        int filter = !strncmp(action,"filter:",7) && strlen(action+7)==36 && valid_cursor(action+7);
        int channels = !strcmp(action, "channels"), sorted = channels && g_channel_sort;
        if (strcmp(action, "home") && strcmp(action, "subscriptions") && strcmp(action, "history") && !channels && strcmp(action, "notifications") &&
            strcmp(action, "watch_later") && strcmp(action, "playlists") && strcmp(action, "profiles") && !playlist && !suggestions && !filter) {
            tube_set_error("The selected library screen is unavailable."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if ((channels || !strcmp(action, "notifications")) && !strcmp(g_library_profile, "guest")) return account_notice(cb, ud);
        if (sorted && page) { /* later pages of the sorted list are already in memory */
            if (!g_channel_list.complete || g_channel_list.scope != g_playlist_scope || strcmp(g_channel_list.profile, g_library_profile)) {
                tube_set_error("Reload this library screen to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
            }
            channel_page(page, cb, ud); return 0;
        }
        if (page >= LIBRARY_PAGES || (page > 0 && (strcmp(parent, g_library_parent) || !*g_library_cursors[page]))) {
            tube_set_error("Reload this library screen to continue browsing."); cb(0, NULL, 0, 0, ud); return 0;
        }
        if (page == 0) { copy(g_library_parent, sizeof g_library_parent, parent); memset(g_library_cursors, 0, sizeof g_library_cursors); }
        Request *r = request_new(!strcmp(action, "profiles") ? 4 : 3, ud); if (!r) return -1;
        r->items_cb = cb; r->page = page; copy(r->parent, sizeof r->parent, parent);
        if (channels && !page) { channel_new_reset(); r->wants_health = r->new_content = 1; }
        if (sorted) {
            memset(&g_channel_list, 0, sizeof g_channel_list); g_channel_list.scope = g_playlist_scope;
            copy(g_channel_list.profile, sizeof g_channel_list.profile, g_library_profile); r->aggregate = 1;
        }
        snprintf(r->payload, sizeof r->payload,
            "{\"action\":\"library\",\"library\":{\"action\":\"%s\",\"profile\":\"%s\",\"cursor\":\"%s\",\"id\":\"%s\"}}",
            page ? "continue" : playlist ? "playlist" : suggestions ? "suggestions" : filter ? "suggestions_filter" : action, g_library_profile,
            page ? g_library_cursors[page] : "", playlist ? action + 9 : suggestions ? action+12 : filter ? action+7 : "");
        begin(r); return 0;
    }
    if (typed) {
        /* The feature list, once read, says whether the library worker runs:
         * without it the filtered search is not even attempted. */
        if (!page && companion_known() && !companion_has(COMPANION_LIBRARY) && (search_filters() || g_search_cards))
            g_search_feed.unavailable = g_playlist_scope;
        const int feed = page && search_feed_current(parent);
        if (feed || (!page && (search_filters() || g_search_cards) && g_search_feed.unavailable != g_playlist_scope))
            return library_search(parent, page, cb, ud);
        if (!page) g_search_feed.key[0] = 0; /* plain results: later pages go by number */
    }
    Request *r = request_new(1, ud); if (!r) return -1;
    r->items_cb = cb; r->search_rows = typed && !page;
    copy(r->parent,sizeof r->parent,parent);
    if (!page && valid_channel_group(parent)) {
        /* With a companion that has channel pages this becomes its channel
         * action; otherwise the channel stays the plain catalog list. */
        r->channel_upgrade = r->wants_health = 1;
        if (!strcmp(parent, g_library_parent)) g_library_parent[0] = 0;
    }
    cJSON *o = cJSON_CreateObject();
    int search = parent && !strncmp(parent, "search:", 7);
    cJSON_AddStringToObject(o, "action", search ? "search" : "catalog");
    cJSON_AddStringToObject(o, "profile", g_library_profile);
    cJSON_AddStringToObject(o, search ? "query" : "parent", search ? parent + 7 : parent);
    cJSON_AddNumberToObject(o, "page", page);
    char *body = cJSON_PrintUnformatted(o); cJSON_Delete(o);
    if (!body) { memset(r, 0, sizeof *r); return -1; }
    copy(r->payload, sizeof r->payload, body); free(body);
    begin(r); return 0;
}

static int search(const char *query, int page, evo_provider_items_cb cb, void *ud) {
    if (!tube_search_query_valid(query) || !cb) return -1;
    tube_queue_retry_cancel();
    if (!page) search_history_add(query);
    char parent[808]; snprintf(parent, sizeof parent, "search:%s", query);
    g_search_typed = 1; /* a typed query, unlike a Discover topic, leads with its options */
    int result = catalog(parent, page, cb, ud);
    g_search_typed = 0; return result;
}

static int resolve(const char *id, evo_provider_resolve_cb cb, void *ud) {
    if (!valid_id(id) || !cb) return -1;
    int recovering = tube_recovery_active() && !strcmp(id, g_recovery.id);
    int automatic = g_launch_origin.automatic && !strcmp(g_launch_origin.id,id) &&
        !strcmp(g_launch_origin.profile,g_library_profile);
    PlaylistContext playlist={0};
    if (recovering) playlist=g_active_playlist;
    else if (!strcmp(g_selected_playlist.id,id) && g_selected_playlist.value.scope == g_playlist_scope)
        playlist=g_selected_playlist.value;
    memset(&g_selected_playlist,0,sizeof g_selected_playlist);
    int incognito = recovering ? g_active_incognito : !strcmp(g_incognito.id,id) &&
        !strcmp(g_incognito.profile,g_library_profile) && g_incognito.scope == g_playlist_scope;
    memset(&g_incognito,0,sizeof g_incognito); g_menu_launch.id[0] = 0;
    if (!automatic && !recovering) g_queue_failed_count = 0;
    if (recovering) cancel_requests(0); else { g_paid_text[0] = 0; g_paid_shown = 0; tube_cancel(); }
    Request *r = request_new(2, ud); if (!r) return -1;
    r->resolve_cb = cb;
    r->playlist = playlist; r->incognito = incognito;
    copy(r->parent, sizeof r->parent, id); copy(r->profile, sizeof r->profile, g_library_profile);
    const Record *origin_record = record(id, 0);
    r->queue_origin = automatic || (g_browse_queue && origin_record && origin_record->queued);
    r->music_origin = g_browse_music || (g_browse_queue && origin_record && origin_record->queued_music);
    if (!strcmp(g_launch_origin.id, id) && !strcmp(g_launch_origin.profile, g_library_profile))
        r->music_origin = g_launch_origin.music;
    memset(&g_launch_origin, 0, sizeof g_launch_origin);
    r->quality = recovering ? g_recovery.quality : (ResolveQuality){g_height,g_max_fps,g_codec};
    snprintf(r->payload, sizeof r->payload,
        "{\"action\":\"resolve\",\"playback\":{\"id\":\"%s\",\"profile\":\"%s\",\"max_height\":%d,\"max_fps\":%d,\"codec\":\"%s\",\"caption_language\":\"%s\",\"audio_language\":\"%s\",\"automatic_captions\":%s,\"fresh\":%s}}",
        id, g_library_profile, r->quality.height, r->quality.max_fps, r->quality.codec == 1 ? "avc" : r->quality.codec == 2 ? "vp9" : r->quality.codec == 3 ? "av1" : r->quality.codec == 4 ? "hevc" : "auto", g_language, g_audio_language, g_auto_captions ? "true" : "false", recovering ? "true" : "false");
    begin(r); return 0;
}

static void progress(const char *id, int64_t pos, int64_t dur, evo_provider_play_state_t state) {
    if (*g_active_id && strcmp(id, g_active_id)) return;
    if (state == EVO_PROVIDER_PLAY_STOP) tube_extra_video_end(); /* the player forgets the stopped video's facts */
    const char *profile = *g_active_profile ? g_active_profile : g_library_profile;
    const int silent = history_suppressed();
    Record *r = record_for(id, profile, !silent);
    if (r && !r->item.is_live && dur > 0) r->item.duration_sec = dur;
    if (silent) { /* Duration is metadata; position, watched marker and reports are history. */
        if (state == EVO_PROVIDER_PLAY_START) { g_finished = 0; g_skip_guard = -1; g_completion_action=g_completion_consumed=0; g_sponsor_completed=g_sponsor_seek_pending=0; }
        g_watch.dirty = 0; return;
    }
    if (!r) return;
    if (r->item.is_live) {
        /* The fixed DVR origin is not the broadcast's elapsed clock or total
         * duration. Persist only its checked UTC point, never a VOD resume,
         * end marker or fabricated remote watch-report offset. */
        r->item.resume_pos_sec=0;r->updated=(long long)time(NULL);
        g_watch.dirty=0;
        if (state==EVO_PROVIDER_PLAY_STOP) g_watch.stopped=1;
        if (state==EVO_PROVIDER_PLAY_START) { g_finished=0;g_skip_guard=-1;g_completion_action=g_completion_consumed=0;g_sponsor_completed=g_sponsor_seek_pending=0; }
        if (state==EVO_PROVIDER_PLAY_STOP || now_ms()-g_last_save>=10000) save();
        return;
    }
    if (g_finished && state != EVO_PROVIDER_PLAY_START && !strcmp(id, g_active_id)) {
        /* Explicit SponsorBlock completion stops before the old clock reaches
         * the end. Its final stop report must not restore an outro resume. */
        r->item.played = 1; r->item.resume_pos_sec = 0;
    } else if (state != EVO_PROVIDER_PLAY_START || pos > 0) r->item.resume_pos_sec = pos > 0 ? pos : 0;
    /* "Remember position of short videos" off: a video under six minutes starts over. */
    if (r->item.resume_pos_sec > 0 && settings_current(profile) &&
        !tube_extra_resume_allowed((double)(dur > 0 ? dur : r->item.duration_sec))) r->item.resume_pos_sec = 0;
    r->updated = (long long)time(NULL);
    if (dur > 0 && pos >= dur * 0.95 && pos >= dur - 10) { r->item.played = 1; r->item.resume_pos_sec = 0; }
    if (state == EVO_PROVIDER_PLAY_START) { g_finished = 0; g_skip_guard = -1; g_completion_action=g_completion_consumed=0; g_sponsor_completed=g_sponsor_seek_pending=0; }
    if (!strcmp(id, g_watch.id) && !g_watch.failed && !g_watch.stopped && pos >= 0 && pos <= 1000000 &&
        (state == EVO_PROVIDER_PLAY_UPDATE || state == EVO_PROVIDER_PLAY_STOP)) {
        g_watch.position = pos; g_watch.stopped = state == EVO_PROVIDER_PLAY_STOP;
        g_watch.dirty = 1; watch_flush(0);
    }
    if (state == EVO_PROVIDER_PLAY_STOP || now_ms() - g_last_save >= 10000) save();
}

static int live_window_valid(const int64_t window[7]) {
    const int64_t now = now_ms() * 1000;
    return window && window[0] == 1 && window[1] >= 0 && window[2] > window[1] && window[2] - window[1] <= 4096 &&
        window[3] >= 0 && window[4] > window[3] && window[4] - window[3] <= 12LL * 3600 * 1000000 &&
        window[5] >= 946684800000000LL && window[5] <= 4102444800000000LL &&
        window[6] > now && window[6] <= now + 180000000LL;
}
static int live_point_inside(const int64_t window[7], int64_t utc) {
    return utc <= 4102444800000000LL && live_window_valid(window) && utc >= window[5] && utc - window[5] < window[4] - window[3];
}
static void live_format(char out[128]) {
    const evo_stream_choice_t *c = &g_recovery.choice;
    snprintf(out, 128, "%s:%s:%s:%d:%d:%.3f:%d:%s", c->container, c->video_codec, c->audio_codec,
        c->width, c->height, c->fps, c->audio_channels, c->video_range);
}
int tube_live_capture(const char *video, const char *url, tube_live_owner *owner) {
    if (!owner) return 0;
    memset(owner, 0, sizeof *owner);
    if (!video || !url || !valid_id(video) || !companion_url(url) || !recovery_scope() || strcmp(video,g_active_id) ||
        strcmp(url,g_recovery.choice.url) || !g_recovery.choice.is_live || strcmp(g_recovery.choice.container,"hls") ||
        g_recovery.choice.width <= 0 || g_recovery.choice.width > 16384 || g_recovery.choice.height <= 0 || g_recovery.choice.height > 16384 ||
        !isfinite(g_recovery.choice.fps) || g_recovery.choice.fps <= 0 || g_recovery.choice.fps > 240) return 0;
    live_retire();
    copy(owner->profile,sizeof owner->profile,g_active_profile); copy(owner->video,sizeof owner->video,video);
    copy(owner->url,sizeof owner->url,url); copy(owner->origin,sizeof owner->origin,g_embedded ? "embedded" : g_source);
    live_format(owner->format);
    const Record *r = record_for(video,owner->profile,0);
    if (!history_suppressed() && r && !strcmp(r->live_origin,owner->origin) && !strcmp(r->live_format,owner->format)) {
        owner->saved_utc=r->live_utc;owner->saved_sequence=r->live_sequence;
        owner->saved_segment_utc=r->live_segment_utc;owner->saved_duration_us=r->live_duration_us;
    }
    if (!++g_live_epoch) ++g_live_epoch;
    owner->token = g_live_epoch;
    pthread_mutex_lock(&g_live_mutex);
    g_live_binding = *owner;
    atomic_store(&g_live_token, owner->token);
    pthread_mutex_unlock(&g_live_mutex);
    return 1;
}
int64_t tube_live_restore(const tube_live_owner *owner, const int64_t window[7], const int64_t identity[3]) {
    int64_t result = 0;
    if (!owner || !identity) return 0;
    pthread_mutex_lock(&g_live_mutex);
    if (tube_live_owner_current(owner->token) && owner->saved_utc == g_live_binding.saved_utc &&
        owner->saved_sequence==g_live_binding.saved_sequence && owner->saved_segment_utc==g_live_binding.saved_segment_utc && owner->saved_duration_us==g_live_binding.saved_duration_us &&
        identity[0]==owner->saved_sequence && identity[1]==owner->saved_segment_utc && identity[2]==owner->saved_duration_us &&
        !strcmp(owner->profile,g_live_binding.profile) && !strcmp(owner->video,g_live_binding.video) &&
        !strcmp(owner->url,g_live_binding.url) && !strcmp(owner->origin,g_live_binding.origin) &&
        !strcmp(owner->format,g_live_binding.format) && live_point_inside(window,owner->saved_utc)) result = owner->saved_utc;
    pthread_mutex_unlock(&g_live_mutex);
    return result;
}
int tube_live_remember(const tube_live_owner *owner, const int64_t window[7], int64_t utc, const int64_t identity[3]) {
    char format[128];
    if (!owner || !tube_live_owner_current(owner->token) || !recovery_scope() || history_suppressed() ||
        strcmp(owner->video,g_active_id) || strcmp(owner->profile,g_active_profile) ||
        strcmp(owner->url,g_recovery.choice.url) || strcmp(owner->origin,g_embedded ? "embedded" : g_source) || !live_point_inside(window,utc) ||
        !identity || identity[0]<window[1] || identity[0]>=window[2] || identity[0]>9007199254740991LL ||
        identity[1]<window[5] || identity[1]>utc || identity[2]<=0 || identity[2]>60000000 ||
        utc-identity[1]>=identity[2] || identity[1]-window[5]+identity[2]>window[4]-window[3]) return 0;
    live_format(format);
    if (strcmp(owner->format,format)) return 0;
    Record *r = record_for(owner->video,owner->profile,1);
    if (!r) return 0;
    r->live_utc=utc;r->live_sequence=identity[0];r->live_segment_utc=identity[1];r->live_duration_us=identity[2];copy(r->live_origin,sizeof r->live_origin,owner->origin); copy(r->live_format,sizeof r->live_format,format);
    r->item.is_live = 1; r->item.resume_pos_sec = 0; r->updated = (long long)time(NULL);
    if (now_ms() - g_last_save >= 10000) save();
    return 1;
}

static void chosen(const char *id, const evo_stream_choice_t *choice) {
    live_retire();
    memset(&g_sb_skip, 0, sizeof g_sb_skip); g_sb_undo_from = -1;
    int recovering = tube_recovery_active() && !strcmp(id, g_recovery.id);
    if (!recovering) { tube_sleep_activity(); tube_playlist_eof_cancel(); }
    if (!recovering && !++g_selection_serial) ++g_selection_serial;
    (void)choice; copy(g_active_id, sizeof g_active_id, id); g_finished = 0; g_skip_guard = -1;
    g_completion_action=g_completion_consumed=0;
    g_sponsor_completed=g_sponsor_seek_pending=0;
    g_sb_active_serial = !strcmp(id, g_resolved_id) ? g_sb_serial : 0;
    g_sb_finish_target = -1;
    char profile[33]; copy(profile, sizeof profile, !strcmp(id, g_resolved_id) ? g_resolved_profile : "");
    copy(g_active_profile, sizeof g_active_profile, valid_profile(profile) ? profile : g_library_profile);
    if (g_resolved_facts && !strcmp(id, g_resolved_id) && !strcmp(g_active_profile, g_resolved_profile)) tube_extra_video_begin(g_resolved_facts);
    else tube_extra_video_end();
    if (!recovering) {
        memset(&g_active_playlist,0,sizeof g_active_playlist);
        if (choice && !strcmp(id,g_resolved_id) && !strcmp(g_active_profile,g_resolved_profile) &&
            g_resolved_playlist.scope == g_playlist_scope) g_active_playlist=g_resolved_playlist;
        /* "Use current section as playlist" off: a YouTube playlist still
         * continues; a section, shelf or channel list does not. */
        if (!g_section_playlist && !valid_playlist(g_active_playlist.playlist)) memset(&g_active_playlist,0,sizeof g_active_playlist);
    }
    if (!recovering || strcmp(g_speed_session_id, id) || strcmp(g_speed_session_profile, g_active_profile)) {
        g_speed_session = !strcmp(id, g_resolved_id) && !strcmp(g_active_profile, g_resolved_profile) && g_resolved_music
            ? 100 : speed_lookup(id, g_active_profile);
        copy(g_speed_session_id, sizeof g_speed_session_id, id);
        copy(g_speed_session_profile, sizeof g_speed_session_profile, g_active_profile);
    }
    if (!recovering) memset(&g_recovery, 0, sizeof g_recovery);
    if (choice) {
        g_recovery.choice = *choice;
        if (!recovering) g_recovery.quality = !strcmp(id,g_resolved_id) && !strcmp(g_active_profile,g_resolved_profile)
            ? g_resolved_quality : (ResolveQuality){g_height,g_max_fps,g_codec};
        copy(g_recovery.id, sizeof g_recovery.id, id);
        copy(g_recovery.profile, sizeof g_recovery.profile, g_active_profile);
        copy(g_recovery.source, sizeof g_recovery.source, g_source);
        copy(g_recovery.token, sizeof g_recovery.token, g_token);
    }
    if (g_watch.stopped && g_watch.dirty) watch_flush(1);
    watch_reset(0);
    if (!recovering) g_active_incognito = g_resolved_incognito && !strcmp(id,g_resolved_id) && !strcmp(g_active_profile,g_resolved_profile);
    g_resolved_incognito = 0;
    if (valid_id(id) && valid_profile(profile) && strcmp(profile, "guest") && !g_active_incognito) {
        copy(g_watch.id, sizeof g_watch.id, id); copy(g_watch.profile, sizeof g_watch.profile, profile);
        snprintf(g_watch.session, sizeof g_watch.session, "%llx-%lx-%u", (unsigned long long)time(NULL), (unsigned long)getpid(), g_watch.serial);
    }
}

int tube_toggle_favorite(const char *id) {
    Record *r = record(id, 1); if (!r) return -1;
    r->favorite = !r->favorite; save(); return r->favorite;
}

int tube_toggle_queue(const char *id) {
    Record *r = record(id, 1); if (!r) return -1;
    r->queued = !r->queued;
    if (r->queued) { r->queue_order = ++g_queue_order; r->queued_music = g_browse_music; }
    else {r->queued_music = 0;r->queue_order=0;}
    save(); return r->queued;
}
int tube_queue_play_next(const char *id) {
    Record *target=record(id,0);if(!target || !valid_id(id))return -1;
    Record *active=recovery_scope()?record_for(g_active_id,g_active_profile,0):NULL;
    if(target==active)return 0;
    /* Compact retained order before insertion to keep persisted numeric ranks
     * bounded. Preserve history and the relative order of all other entries. */
    Record *ordered[RECORDS],*pending[RECORDS];int count=queue_compact(ordered),queued=0;
    for(int i=0;i<count;++i) {
        Record *r=ordered[i];
        if(current_record(r) && r->queued && r!=target && r!=active)pending[queued++]=r;
    }
    if(!target->queued)target->queued_music=g_browse_music;
    target->queued=1;target->queue_order=++g_queue_order;
    for(int i=0;i<queued;++i)pending[i]->queue_order=++g_queue_order;
    tube_playlist_eof_cancel();tube_queue_retry_cancel();save();return 1;
}
int tube_mark_watched(const char *id) {
    Record *r=record(id,0);if(!r || !valid_id(id))return -1;
    r->item.played=1;r->item.resume_pos_sec=0;r->updated=time(NULL);save();return 1;
}
int tube_remove_history(const char *id) {
    if (id && !strcmp(id,g_active_id)) live_retire();
    Record *r=record(id,0);if(!r || !valid_id(id))return -1;
    r->item.played=0;r->item.resume_pos_sec=0;r->live_utc=0;r->updated=0;save();return 1;
}

int tube_toggle_subscription(const char *id) {
    Record *r = record(id, 1); if (!r || strncmp(r->item.parent_id, "UC", 2) || strlen(r->item.parent_id) != 24) return -1;
    const int follow = !followed(r->item.parent_id); /* through a video or an import */
    if (!follow) unfollow(r->item.parent_id);
    else for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && !strcmp(g_records[i].item.parent_id, r->item.parent_id)) g_records[i].subscribed = 1;
    save(); return follow;
}

const char *tube_caption_media_path(void) { return g_caption_path; }
int tube_phone_video(const char *url, char video[12]) {
    if (!url || !video || !valid_id(g_active_id) || strcmp(g_active_profile, g_library_profile) ||
        strcmp(g_recovery.source, g_source) || strcmp(g_recovery.token, g_token) ||
        strcmp(g_recovery.profile, g_library_profile) || strcmp(g_recovery.id, g_active_id) ||
        strcmp(g_recovery.choice.url, url)) return 0;
    copy(video, 12, g_active_id); return 1;
}
const char *tube_phone_caption_language(void) { return *g_caption_path ? g_caption_language : ""; }
unsigned tube_phone_selection(void) { return g_selection_serial; }
static int caption_scope(void) {
    return recovery_scope() && !strcmp(g_caption_video, g_active_id) &&
        !strcmp(g_caption_profile, g_library_profile) && !strcmp(g_caption_source, g_source) &&
        !strcmp(g_caption_token, g_token);
}
int tube_caption_track_count(void) { return caption_scope() ? g_caption_track_count : 0; }
const char *tube_caption_track_language(int i) { return i >= 0 && i < tube_caption_track_count() ? g_caption_tracks[i].language : ""; }
const char *tube_caption_track_name(int i) { return i >= 0 && i < tube_caption_track_count() ? g_caption_tracks[i].name : ""; }
int tube_caption_track_automatic(int i) { return i >= 0 && i < tube_caption_track_count() && g_caption_tracks[i].automatic; }
int tube_caption_track_translated(int i) { return i >= 0 && i < tube_caption_track_count() && g_caption_tracks[i].translated; }
const char *tube_caption_track_target(int i) { return tube_caption_track_translated(i) ? g_caption_tracks[i].target : ""; }
int tube_caption_track_translation_for(const char *tag) {
    if (!tag || !*tag) return -1;
    const size_t base = strcspn(tag, "-_");
    int loose = -1;
    for (int i = 0; i < tube_caption_track_count(); ++i) {
        const char *target = tube_caption_track_target(i);
        if (!*target) continue;
        if (!strcasecmp(target, tag)) return i;
        /* "pt-BR" takes "pt", "de" takes "de-DE": the same language first named. */
        if (loose < 0 && strcspn(target, "-_") == base && !strncasecmp(target, tag, base)) loose = i;
    }
    return loose;
}
unsigned tube_caption_revision(void) { return g_caption_revision; }
unsigned tube_caption_selection(void) { return caption_scope() ? g_selection_serial : 0; }
void tube_caption_switch_cancel(void) {
    if (!++g_caption_serial) ++g_caption_serial;
    memset(&g_caption_switch, 0, sizeof g_caption_switch);
}
static int caption_switch_current(void) {
    return caption_scope() && g_caption_switch.selection == g_selection_serial && (!g_caption_switch.owner ||
        tube_remote_command_current(g_caption_switch.owner, g_caption_switch.sequence));
}
static void caption_switch_response(int ok, int status, const char *body, size_t size, void *ud) {
    unsigned serial = (unsigned)(size_t)ud;
    if (serial != g_caption_serial || g_caption_switch.phase != 1 || g_caption_switch.serial != serial) return;
    g_caption_switch.phase = 3;
    if (!caption_switch_current() || !ok || status != 200 || !body || size >= 2 * 1024 * 1024) return;
    if (size && !body[size - 1]) --size;
    if (!size || memchr(body, 0, size)) return;
    char path[512], temporary[512];
    copy(path, sizeof path, g_caption_switch.media);
    size_t length = strlen(path); if (length < 4 || length > sizeof path - 9) return;
    memcpy(path + length - 4, ".srt", 5);
    memcpy(temporary, path, length); memcpy(temporary + length, ".tmp", 5);
    FILE *file = fopen(temporary, "wb"); if (!file) return;
    int success = fwrite(body, 1, size, file) == size;
    if (fclose(file)) success = 0;
    if (success && rename(temporary, path) == 0) g_caption_switch.phase = 2;
    else remove(temporary);
}
static int caption_switch_begin(const char *language, unsigned owner, int sequence, int secondary) {
    if (!language || !caption_scope() || g_caption_switch.phase ||
        (owner ? !tube_remote_command_current(owner, sequence) : sequence != 0)) return 0;
    int index = -1;
    for (int i = 0; i < g_caption_track_count; ++i) if (!strcmp(language, g_caption_tracks[i].language)) { index = i; break; }
    if (index < 0) return 0;
    if (secondary && !strcmp(language, tube_phone_caption_language())) return 0;
    char first[512]; copy(first, sizeof first, evo_data_path(secondary ? "caption-secondary-a.mp4" : "caption-a.mp4"));
    tube_caption_switch_cancel();
    g_caption_switch.secondary = secondary;
    copy(g_caption_switch.media, sizeof g_caption_switch.media,
        !strcmp(secondary ? g_secondary_caption_path : g_caption_path, first) ?
        evo_data_path(secondary ? "caption-secondary-b.mp4" : "caption-b.mp4") : first);
    if (strlen(g_caption_switch.media) > sizeof g_caption_switch.media - 9) { tube_caption_switch_cancel(); return 0; }
    copy(g_caption_switch.language, sizeof g_caption_switch.language, language);
    g_caption_switch.owner = owner; g_caption_switch.sequence = sequence; g_caption_switch.selection = g_selection_serial;
    g_caption_switch.serial = g_caption_serial; g_caption_switch.deadline = now_ms() + 15000; g_caption_switch.phase = 1;
    if (evo_net_request_async("GET", g_caption_tracks[index].url, NULL, NULL, 0,
        caption_switch_response, (void *)(size_t)g_caption_serial)) { tube_caption_switch_cancel(); return 0; }
    return 2;
}
int tube_caption_switch_begin(const char *language, unsigned owner, int sequence) {
    return caption_switch_begin(language, owner, sequence, 0);
}
int tube_caption_secondary_begin(const char *language) { return caption_switch_begin(language, 0, 0, 1); }
int tube_caption_switch_secondary(void) { return g_caption_switch.phase && g_caption_switch.secondary; }
const char *tube_caption_switch_language(void) { return g_caption_switch.phase ? g_caption_switch.language : ""; }
const char *tube_caption_secondary_language(void) { return caption_scope() ? g_secondary_caption_language : ""; }
const char *tube_caption_secondary_media_path(void) { return caption_scope() ? g_secondary_caption_path : ""; }
void tube_caption_secondary_clear(void) {
    if (g_caption_switch.secondary) tube_caption_switch_cancel();
    g_secondary_caption_path[0] = g_secondary_caption_language[0] = 0; ++g_caption_revision;
}
static void caption_switch_tick(void) {
    if (g_caption_switch.phase && !caption_switch_current()) tube_caption_switch_cancel();
    else if (g_caption_switch.phase == 1 && now_ms() > g_caption_switch.deadline) {
        g_caption_switch.phase = 3;
        if (!++g_caption_serial) ++g_caption_serial; /* discard a late HTTP completion */
    }
}
int tube_caption_switch_status(const char **media_path) {
    if (media_path) *media_path = "";
    caption_switch_tick();
    if (g_caption_switch.phase == 2) {
        if (media_path) *media_path = g_caption_switch.media;
        return 1;
    }
    return g_caption_switch.phase == 1 ? 0 : -1;
}
void tube_caption_switch_finish(int applied) {
    if (applied && g_caption_switch.phase == 2 && caption_switch_current()) {
        if (g_caption_switch.secondary) {
            copy(g_secondary_caption_path, sizeof g_secondary_caption_path, g_caption_switch.media);
            copy(g_secondary_caption_language, sizeof g_secondary_caption_language, g_caption_switch.language);
        } else {
            copy(g_caption_path, sizeof g_caption_path, g_caption_switch.media);
            copy(g_caption_language, sizeof g_caption_language, g_caption_switch.language);
            if (!strcmp(g_caption_language, g_secondary_caption_language)) tube_caption_secondary_clear();
            tube_remember_captions(g_caption_video, 1); /* a chosen track turns captions on for this channel */
        }
        ++g_caption_revision;
    }
    tube_caption_switch_cancel();
}
int tube_caption_switch_local(void) { return g_caption_switch.phase && !g_caption_switch.owner; }
const char *tube_take_paid_content_notice(void) {
    if (!g_paid_notifications || g_paid_shown || !*g_paid_text || g_finished ||
        !*g_active_id || !*g_sb_source || strcmp(g_sb_source, g_source) ||
        strcmp(g_active_id, g_resolved_id) || strcmp(g_active_profile, g_resolved_profile) ||
        strcmp(g_active_profile, g_library_profile) || g_sb_active_serial != g_sb_serial) return NULL;
    g_paid_shown = 1;
    return g_paid_text;
}
int tube_autoplay_enabled(void) { return g_autoplay; }
int tube_playback_mode(void) { return g_playback_mode; }
int tube_queue_respects_playback_mode(void) { return g_queue_respects_mode; }
int tube_set_playback_mode(int mode) {
    if (!playback_mode_valid(mode)) return 0;
    tube_playlist_eof_cancel(); tube_queue_retry_cancel();
    g_playback_mode=mode; g_autoplay=playback_mode_continues(mode); save(); return 1;
}

static int sponsor_scope(void) {
    return g_sponsor && *g_sb_source && !strcmp(g_sb_source, g_source) && !strcmp(g_sb_profile, g_library_profile) &&
        !tube_sb_excluded(&g_sb, g_sb_channel);
}
static int sponsor_playing_scope(void) {
    return sponsor_scope() && !g_finished && g_sb_active_serial == g_sb_serial &&
        !strcmp(g_active_id, g_resolved_id) && !strcmp(g_active_profile, g_sb_profile) &&
        !g_recovery.choice.is_live;
}
static void highlight_observe(double position) {
    if (g_highlight_guard >= 0 && (g_highlight_guard >= g_segment_count ||
        position >= g_segments[g_highlight_guard][0] - .25 || position < g_highlight_origin - .5 ||
        position > g_highlight_origin + .5))
        g_highlight_guard = -1;
}
void tube_highlight_seek_failed(double target) {
    if (g_highlight_guard >= 0 && g_highlight_guard < g_segment_count &&
        target == g_segments[g_highlight_guard][0]) {
        g_highlight_guard = -1; g_sb_message = 0;
    }
}
void tube_highlight_retry_idle(void) {
    if (sponsor_playing_scope()) { g_highlight_guard = -1; g_sb_message = 0; }
}
double tube_highlight_target(double position, double duration) {
    g_sb_message = 0;
    if (!sponsor_playing_scope() || !isfinite(position) || position < 0 ||
        !isfinite(duration) || duration <= 0 || duration > 1000000 || position >= duration)
        return -1;
    highlight_observe(position);
    if (tube_sponsor_prompt(position)) return -1; // Keep an existing review intact.
    int action = g_sb.actions[8], index = -1;
    if (action == 3) return -1;
    for (int i = 0; i < g_segment_count; ++i)
        if (g_segment_categories[i] == 8 && g_segments[i][0] > position + .25 &&
            g_segments[i][0] < duration && (index < 0 || g_segments[i][0] < g_segments[index][0])) index = i;
    if (index < 0 || index == g_highlight_guard) return -1;
    double target = g_segments[index][0];
    g_sb_finish_target = -1; // A point cannot authorize end-of-video completion.
    if (action == 2) {
        g_sb_prompt = index; g_sb_prompt_start = position; g_sb_prompt_end = target;
        snprintf(g_sb_prompt_text, sizeof g_sb_prompt_text, "Jump to highlight? Cross: jump   Circle: keep playing");
        return -1;
    }
    // Explicit intent ignores automatic minimum/repeat policy. Guard only the
    // same request while its seek clock has not moved; observing movement or
    // arrival releases it. A refused paused request is released explicitly.
    g_highlight_guard = index; g_highlight_origin = position;
    g_segment_seen[index] = 1; g_sb_message = action == 1;
    return target;
}
int tube_sponsor_markers(double duration, tube_sponsor_marker_t *markers, int capacity) {
    if (!markers || capacity <= 0 || !isfinite(duration) || duration <= 0 || !sponsor_playing_scope()) return 0;
    int count = 0;
    for (int i = 0; i < g_segment_count && count < capacity; ++i) {
        int category = g_segment_categories[i];
        if (!g_sb.colors[category] || g_segments[i][0] > duration ||
            (category != 8 && g_segments[i][0] == duration)) continue;
        markers[count++] = (tube_sponsor_marker_t){g_segments[i][0] / duration,
            fmin(g_segments[i][1], duration) / duration, g_sb_colors[category], category == 8};
    }
    return count;
}
const char *tube_sponsor_prompt(double position) {
    if (isfinite(position)) highlight_observe(position);
    if (!sponsor_scope() || !isfinite(position) || g_sb_prompt < 0 ||
        (g_segment_categories[g_sb_prompt] == 8 && !sponsor_playing_scope()) ||
        position < g_sb_prompt_start - .5 || position >= g_sb_prompt_end - .25) {
        g_sb_prompt = -1; return NULL;
    }
    return g_sb_prompt_text;
}
double tube_sponsor_confirm(double position, int accept) {
    if (!tube_sponsor_prompt(position)) return -1;
    int index = g_sb_prompt; double end = g_sb_prompt_end; g_sb_prompt = -1;
    if (g_segment_categories[index] == 8) {
        g_segment_seen[index] = accept ? 1 : 2;
        if (accept) { g_highlight_guard = index; g_highlight_origin = position; }
        g_sb_finish_target = -1;
        return accept ? end : -1;
    }
    g_segment_seen[index] = accept ? 1 : 2;
    g_skip_guard = index; g_skip_until = end;
    g_sb_finish_target = accept ? end : -1;
    return accept ? end : -1;
}
int tube_sponsor_show_message(void) { return g_sb_message; }
void tube_sponsor_skip_started(double from, double to) {
    memset(&g_sb_skip, 0, sizeof g_sb_skip);
    if (!sponsor_playing_scope() || !g_sb_message || !isfinite(from) || !isfinite(to) || from < 0 ||
        to <= from || g_skip_guard < 0 || g_skip_guard >= g_segment_count || to != g_skip_until) return;
    g_sb_skip.from = from; g_sb_skip.to = to;
    g_sb_skip.category = g_segment_categories[g_skip_guard];
    if (tube_sponsor_completed()) g_sb_skip.shown=now_ms();
    else g_sb_skip.pending = 1;
}
void tube_sponsor_skip_observe(double position, int busy) {
    if (!g_sb_skip.pending && !g_sb_skip.shown) return;
    const long long now = now_ms();
    if (!sponsor_playing_scope() || !isfinite(position)) {
        memset(&g_sb_skip, 0, sizeof g_sb_skip); return;
    }
    if (g_sb_skip.pending == 2) {
        if (busy) return;
        if (fabs(position - g_sb_skip.from) <= fmin(1.0, (g_sb_skip.to - g_sb_skip.from) * .25)) {
            g_sb_undo_from = g_sb_skip.from; g_sb_undo_until = g_sb_skip.to;
            g_sb_last_position = position;
            memset(&g_sb_skip, 0, sizeof g_sb_skip); return;
        }
        g_sb_skip.pending = 0; // A refused asynchronous rewind can be retried while the notice stands.
    }
    if (g_sb_skip.shown &&
        (now < g_sb_skip.shown || now - g_sb_skip.shown >= TUBE_SPONSOR_UNDO_MS)) {
        memset(&g_sb_skip, 0, sizeof g_sb_skip); return;
    }
    if (g_sb_skip.pending && !busy) {
        if (position < g_sb_skip.to - .1 || position > g_sb_skip.to + 2.0) { memset(&g_sb_skip, 0, sizeof g_sb_skip); return; }
        g_sb_skip.pending = 0; g_sb_skip.shown = now;
    }
}
int tube_sponsor_last_skip(tube_sponsor_skip_t *skip) {
    const long long age = now_ms() - g_sb_skip.shown;
    if (!skip || !g_sb_skip.shown || g_sb_skip.pending == 2 || !sponsor_playing_scope() || age < 0 || age >= TUBE_SPONSOR_UNDO_MS) return 0;
    *skip = (tube_sponsor_skip_t){g_sb_skip.from, g_sb_skip.to, g_sb_colors[g_sb_skip.category],
        (unsigned)age, tube_sb_labels[g_sb_skip.category]};
    return 1;
}
void tube_sponsor_undo_started(double target) {
    tube_sponsor_skip_t skip;
    if (!tube_sponsor_last_skip(&skip) || target != skip.from) return;
    g_sb_skip.pending = 2;
}
unsigned tube_sponsor_prompt_rgb(double position) {
    return tube_sponsor_prompt(position) ? g_sb_colors[g_segment_categories[g_sb_prompt]] : 0;
}
static int sponsor_match(double position, int index, double rate) {
    return g_segment_categories[index] != 8 && position >= g_segments[index][0] &&
        position <= fmin(g_segments[index][0] + 2.0 * rate, g_segments[index][1]);
}
double tube_skip_target_at_rate(double position, double rate) {
    if (!sponsor_scope() || !isfinite(position) || position < 0 || !isfinite(rate) || rate <= 0) {
        g_sb_prompt = -1; g_sb_finish_target = -1; return -1;
    }
    if (g_sb_undo_from >= 0) {
        if (position >= g_sb_undo_from - .5 && position < g_sb_undo_until + .25) return -1;
        g_sb_undo_from = -1;
    }
    if (g_sb_last_position >= 0 && position < g_sb_last_position - .5) {
        g_skip_guard = -1; g_sb_prompt = -1; g_sb_finish_target = -1;
        if (!g_sb.no_repeat) memset(g_segment_seen, 0, sizeof g_segment_seen);
    }
    g_sb_last_position = position;
    if (tube_sponsor_prompt(position)) return -1;
    if (g_highlight_guard >= 0) return -1;
    if (g_skip_guard >= 0 && position < g_skip_until + .25) return -1;
    int selected[SEGMENTS], count = 0, last = -1;
    for (int i = 0; i < g_segment_count; ++i) {
        int action = g_sb.actions[g_segment_categories[i]];
        if (action == 3 || g_segment_seen[i] == 2 || (g_sb.no_repeat && g_segment_seen[i])) continue;
        if (!count) {
            if (i == g_skip_guard || !sponsor_match(position, i, rate)) continue;
            selected[count++] = i; last = i;
            if (action == 2) break; /* Reference dialogs apply to one segment. */
        } else if (action < 2 && sponsor_match(g_segments[last][1] + 3.0, i, rate)) {
            selected[count++] = i; last = i;
        }
    }
    if (!count) return -1;
    double end = g_segments[last][1];
    int action = g_sb.actions[g_segment_categories[last]];
    if (g_sb.no_repeat) for (int i = 0; i < count; ++i) g_segment_seen[selected[i]] = 1;
    const Record *media = record_for(g_resolved_id, g_sb_profile, 0);
    double bounded_end = media && media->item.duration_sec > 0 ? fmin(end, (double)media->item.duration_sec) : end;
    if (bounded_end - position < g_sb.minimum) return -1;
    if (action == 2) {
        g_sb_prompt = last; g_sb_prompt_start = g_segments[last][0]; g_sb_prompt_end = end;
        snprintf(g_sb_prompt_text, sizeof g_sb_prompt_text, "Skip %s? Cross: skip   Circle: keep", tube_sb_labels[g_segment_categories[last]]);
        return -1;
    }
    for (int i = 0; i < count; ++i) g_segment_seen[selected[i]] = 1;
    g_sb_message = action == 1;
    g_skip_guard = selected[0]; g_skip_until = end; g_sb_finish_target = end;
    return end;
}
/* The current PS5 engine plays at 1x; future rate controls must pass the actual rate. */
double tube_skip_target(double position) { return tube_skip_target_at_rate(position, 1.0); }

double tube_chapter_target(double position, int direction) {
    if (!isfinite(position) || !g_chapter_count) return -1;
    double target = -1;
    for (int i = 0; i < g_chapter_count; ++i) {
        double start = g_chapters[i];
        if (direction > 0 && start > position + 1 && (target < 0 || start < target)) target = start;
        if (direction < 0 && start < position - 3 && start > target) target = start;
    }
    return target;
}

int tube_playback_at_end(double position, double duration) {
    if (!isfinite(position) || position < 0 || !isfinite(duration)) return 0;
    const Record *r = record_for(g_active_id, g_active_profile, 0);
    if (duration <= 0 && r) duration = (double)r->item.duration_sec;
    /* EVO's EOF flag means input was exhausted, including while several
     * seconds remain buffered. Do not mark watched or advance the queue then.
     * Unknown-duration streams require a separate presentation-drain signal. */
    return duration > 0 && position >= duration - fmin(0.1, duration * 0.01);
}

int tube_queue_retry_pending(void) {
    Record *r = record_for(g_next_id,g_next_profile,0);
    return queue_mode_allows_advance() && r && r->queued && blocked_identity(r->item.parent_id,r->item.subtitle)<0 && !strcmp(g_next_profile,g_library_profile);
}
void tube_queue_retry_cancel(void) {
    g_next_id[0] = g_next_profile[0] = 0; g_queue_failed_count = 0;
    memset(&g_resolve_failure,0,sizeof g_resolve_failure);
    memset(&g_launch_origin,0,sizeof g_launch_origin);
}
static int queue_failed(const char *id) {
    for (int i = 0; i < g_queue_failed_count; ++i) if (!strcmp(id,g_queue_failed[i])) return 1;
    return 0;
}
static int queue_next(void) {
    Record *next = NULL;
    for (int i = 0; i < g_count; ++i)
        if (current_record(&g_records[i]) && g_records[i].queued && blocked_identity(g_records[i].item.parent_id,g_records[i].item.subtitle)<0 && !queue_failed(g_records[i].item.id) &&
            (!next || g_records[i].queue_order < next->queue_order)) next = &g_records[i];
    if (!next) return 0;
    copy(g_next_id,sizeof g_next_id,next->item.id);
    copy(g_next_profile,sizeof g_next_profile,next->profile); return 1;
}
int tube_queue_skip_unavailable(const char *id) {
    if (!queue_mode_allows_advance() || !tube_resolve_unavailable(id) || !g_resolve_failure.queue_origin) return 0;
    Record *failed = record_for(id,g_library_profile,0);
    if (!failed || !failed->queued || queue_failed(id) || g_queue_failed_count == RECORDS) return 0;
    copy(g_queue_failed[g_queue_failed_count++],sizeof g_queue_failed[0],id);
    memset(&g_resolve_failure,0,sizeof g_resolve_failure);
    return queue_next(); /* Unavailable is not watched: retain queue/resume/history. */
}

void tube_playlist_eof_cancel(void) {
    g_next_id[0]=0;g_next_profile[0]=0;
    memset(&g_next_offer,0,sizeof g_next_offer);
    g_manual_direction=0;
    if(!++g_recommendation.serial)++g_recommendation.serial;
    g_recommendation.state=-1;
    g_completion_action=0; g_completion_consumed=0;
    g_playlist_eof_wait = g_playlist_next_ready = 0;
    g_playlist_shuffle_attempted=0;
    tube_playlist_window_cancel();
}
int tube_playlist_eof_pending(void) { return g_playlist_eof_wait || g_playlist_next_ready ||
    (g_recommendation.state>=0 && recommendation_current()); }
static int playlist_random_index(int current,int count) {
    if (count<=30 || current<0 || current>=count) return -1;
    uint32_t bound=(uint32_t)count-1, threshold=(uint32_t)(-bound)%bound, value;
    for(int attempt=0;attempt<8;++attempt) {
        if(tube_random_bytes(&value,sizeof value)) return -1;
        if(value<threshold) continue;
        int target=(int)(value%bound); return target>=current?target+1:target;
    }
    return -1;
}
static int playlist_eof_advance(void) {
    if (!g_playlist_eof_wait) return 0;
    const int mode=g_manual_direction? (g_manual_direction<0?TUBE_MODE_REVERSE:TUBE_MODE_ALL):g_playback_mode;
    if ((!g_autoplay && !g_manual_direction) || !playlist_window_current() ||
        g_playlist_window.lookup != (g_playlist_eof_wait >= 2) ||
        (g_playlist_eof_wait >= 2 && g_playlist_window.target_index != g_playlist_eof_target)) {
        tube_playlist_eof_cancel(); return 0;
    }
    int state=tube_playlist_window_status(NULL);
    if (!state) return 0;
    int phase=g_playlist_eof_wait, lookup=phase>=2;
    g_playlist_eof_wait=0;
    if(state==2 && phase==2) {
        int target=g_playlist_window.scan_next,direction=g_playlist_window.direction;
        if(target>=0) {
            g_playlist_eof_target=target;
            if(playlist_window_begin_at(target,direction)) g_playlist_eof_wait=2;
        } else if(direction>0 && (mode==TUBE_MODE_ALL || mode==TUBE_MODE_SHUFFLE || mode==TUBE_MODE_LIST)) recommendation_begin(mode==TUBE_MODE_LIST);
        return 0;
    }
    if(state != 1 && phase==3) {
        g_playlist_window.value=g_playlist_shuffle_source;
        g_playlist_window.lookup=0; g_playlist_window.target_index=g_active_playlist.index;
        g_playlist_window.state=1; g_playlist_eof_wait=1;
        return playlist_eof_advance();
    }
    if (state != 1) {
        if(valid_retained_group(g_active_playlist.playlist) && (mode==TUBE_MODE_ALL || mode==TUBE_MODE_SHUFFLE))recommendation_begin(0);
        return 0;
    }
    const tube_playlist_window_t *window=&g_playlist_window.value;
    if(!lookup && mode==TUBE_MODE_SHUFFLE && !g_playlist_shuffle_attempted && valid_playlist(g_active_playlist.playlist)) {
        g_playlist_shuffle_attempted=1;
        int target=playlist_random_index(g_active_playlist.index,window->playlist_size);
        if(target>=0) {
            g_playlist_shuffle_source=*window; g_playlist_eof_target=target;
            if(tube_playlist_window_at(target)) g_playlist_eof_wait=3;
            return 0;
        }
    }
    int direction=mode==TUBE_MODE_REVERSE?-1:1;
    int at=window->current+(lookup ? 0 : direction);
    int blocked=0;
    while(at>=0 && at<window->count) {
        const Record *cached=record_for(window->ids[at],g_active_profile,0);
        const char *channel=*window->channels[at]?window->channels[at]:cached?cached->item.parent_id:"";
        if(channel_blocks_active() && !*channel)return 0;
        const char *name=*window->channel_names[at]?window->channel_names[at]:cached?cached->item.subtitle:"";
        if(channel_name_blocks_active() && !*name)return 0;
        if(blocked_identity(channel,name)<0)break;
        ++blocked;at+=direction;
    }
    if (at < 0 || at >= window->count) {
        if(direction>0 && !strncmp(g_active_playlist.playlist,"RD",2)) {
            recommendation_begin(mode==TUBE_MODE_LIST);return 0;
        }
        int target=blocked?window->indices[direction>0?window->count-1:0]+direction:g_active_playlist.index+direction;
        if(direction>0 && window->playlist_size && target >= window->playlist_size) {
            if(mode==TUBE_MODE_ALL || mode==TUBE_MODE_SHUFFLE || mode==TUBE_MODE_LIST)recommendation_begin(mode==TUBE_MODE_LIST);
            return 0;
        }
        if ((!lookup || blocked) && target>=0 && target<=100000) {
            g_playlist_eof_target=target;
            if(playlist_window_begin_at(target,direction)) g_playlist_eof_wait=2;
        }
        return 0;
    }
    const Record *cached=record_for(window->ids[at],g_active_profile,0);
    memset(&g_playlist_next,0,sizeof g_playlist_next);
    if (cached) { g_playlist_next=cached->item; dearrow_item(&g_playlist_next,cached); }
    if(*window->channels[at])copy(g_playlist_next.parent_id,sizeof g_playlist_next.parent_id,window->channels[at]);
    if(*window->channel_names[at])copy(g_playlist_next.subtitle,sizeof g_playlist_next.subtitle,window->channel_names[at]);
    copy(g_playlist_next.id,sizeof g_playlist_next.id,window->ids[at]);
    if (!*g_playlist_next.title) copy(g_playlist_next.title,sizeof g_playlist_next.title,window->ids[at]);
    copy(g_playlist_next.playback_playlist,sizeof g_playlist_next.playback_playlist,window->playlist);
    g_playlist_next.playback_index=window->indices[at]; g_playlist_next.playback_scope=window->scope;
    g_playlist_next.resume_pos_sec=0; g_playlist_next.kind=EVO_MEDIA_VIDEO;
    g_playlist_next_ready=1; return 1;
}
static int manual_queue(int direction) {
    const Record *current=record_for(g_active_id,g_active_profile,0);
    Record *next=NULL;
    if(direction<0 && (!current || !current->queue_order))return 0;
    for(int i=0;i<g_count;++i) {
        Record *r=&g_records[i];
        if(!current_record(r) || blocked_identity(r->item.parent_id,r->item.subtitle)>=0 || (direction>0?!r->queued:!r->queue_order) || queue_failed(r->item.id) || !strcmp(r->item.id,g_active_id))continue;
        if(direction<0 ? r->queue_order>=current->queue_order :
            (current && current->queued && r->queue_order<=current->queue_order))continue;
        if(!next || (direction<0?r->queue_order>next->queue_order:r->queue_order<next->queue_order))next=r;
    }
    if(!next)return 0;
    copy(g_next_id,sizeof g_next_id,next->item.id);copy(g_next_profile,sizeof g_next_profile,next->profile);return 1;
}
int tube_manual_navigation_begin(int direction) {
    if((direction!=1 && direction!=-1) || !recovery_scope() || tube_recovery_pending())return 0;
    tube_playlist_eof_cancel();tube_queue_retry_cancel();
    g_manual_direction=direction;g_manual_fallback=0;g_manual_scope=g_playlist_scope;g_manual_selection=g_selection_serial;
    if(direction>0 && manual_queue(1))return 1;
    if(*g_active_playlist.playlist && (direction>0 || g_active_playlist.index>0) && tube_playlist_window_begin()) {
        g_playlist_eof_wait=1;return 1;
    }
    if(direction>0)g_manual_fallback=1;
    if(direction<0?manual_queue(-1):recommendation_begin(0))return 1;
    tube_playlist_eof_cancel();return 0;
}
int tube_manual_navigation_status(void) {
    if(!g_manual_direction)return -1;
    if(!recovery_scope() || g_manual_scope!=g_playlist_scope || g_manual_selection!=g_selection_serial) {
        tube_playlist_eof_cancel();return -1;
    }
    if(*g_next_id || g_playlist_next_ready || (g_recommendation.state==1 && recommendation_current()))return 1;
    if(playlist_eof_advance())return 1;
    if(tube_playlist_eof_pending())return 0;
    if(g_manual_direction<0 && manual_queue(-1))return 1;
    if(g_manual_direction>0 && !g_manual_fallback) {
        g_manual_fallback=1;if(recommendation_begin(0))return 0;
    }
    tube_playlist_eof_cancel();return -1;
}
int tube_manual_navigation_pending(void) {return g_manual_direction!=0;}
static void manual_departure(void) {
    /* Consuming a pending queue entry is independent of marking it watched.
     * Keep its order so explicit Previous can return to the skipped entry. */
    if(g_manual_direction>0) {
        Record *current=record_for(g_active_id,g_active_profile,0);
        if(current && current->queued) {current->queued=0;save();}
    }
    g_manual_direction=0;
}
static int finish_and_next(int playlist) {
    if(g_manual_direction)return 0;
    if (g_finished) {
        if (*g_next_id || g_playlist_next_ready) return 1;
        if(g_recommendation.state>=0 && recommendation_current())return g_recommendation.state==1;
        return playlist ? playlist_eof_advance() : 0;
    }
    if (!*g_active_id) return 0;
    g_finished = 1;
    Record *r = record_for(g_active_id, g_active_profile, 0);
    /* "Loop Shorts": a Short repeats like Repeat one until another video is chosen. */
    const int looping = !strcmp(g_active_profile, g_library_profile) && tube_extra_loop_current();
    if (r) {
        if(g_playback_mode!=TUBE_MODE_ONE && !looping) r->queued = 0;
        if (!history_suppressed()) { r->item.played = 1; r->item.resume_pos_sec = 0; }
        save();
    }
    if (strcmp(g_active_profile, g_library_profile)) return 0;
    g_completion_scope=g_playlist_scope;
    if (g_playback_mode==TUBE_MODE_ONE || looping) {
        g_completion_action=g_recovery.choice.is_live?TUBE_COMPLETION_PAUSE:TUBE_COMPLETION_REPEAT;
        return 0;
    }
    if (!g_autoplay) {
        if (queue_mode_allows_advance() && queue_next()) return 1;
        return g_playback_mode==TUBE_MODE_CLOSE;
    }
    if(playlist && g_playback_mode==TUBE_MODE_REVERSE && *g_active_playlist.playlist &&
       g_active_playlist.scope==g_playlist_scope) {
        if(g_active_playlist.index>0 && tube_playlist_window_begin()) g_playlist_eof_wait=1;
        return 0;
    }
    if (queue_next()) return 1;
    if(g_playback_mode==TUBE_MODE_LIST && (valid_channel_group(g_active_playlist.playlist) || valid_retained_group(g_active_playlist.playlist)))return 0;
    if (playlist && tube_playlist_window_begin()) g_playlist_eof_wait=1;
    else if(playlist && !*g_active_playlist.playlist && g_playback_mode!=TUBE_MODE_LIST) recommendation_begin(0);
    return 0;
}

int tube_sponsor_finish(double target, double duration) {
    /* An acknowledged optional skip may reach (or overrun) the known end.
     * Finish deliberately instead of asking the demuxer to seek past its last
     * decodable frame. This is separate from natural presentation drain. */
    if (!sponsor_scope() || g_finished || tube_sponsor_completed() || g_sb_active_serial != g_sb_serial ||
        strcmp(g_active_id, g_resolved_id) || strcmp(g_active_profile, g_sb_profile) ||
        g_recovery.choice.is_live || !isfinite(target) || !isfinite(duration) ||
        duration <= 0 || duration > 1000000 || target < duration ||
        target != g_sb_finish_target || target != g_skip_until || g_skip_guard < 0) return 0;
    g_sb_finish_target = -1;
    g_sponsor_completed=1;
    g_completion_scope=g_playlist_scope;
    return 1; /* Acknowledge the skip; the player's completion path owns mode actions. */
}

int tube_sponsor_completed(void) {
    return g_sponsor_completed && g_completion_scope==g_playlist_scope &&
        !strcmp(g_active_profile,g_library_profile);
}
void tube_completion_seek_started(double target) {
    if(tube_sponsor_completed() && isfinite(target) && target>=0) {
        g_sponsor_seek_pending=1;g_sponsor_seek_target=target;
    }
}
void tube_completion_seek_observe(double position,int busy) {
    if(!g_sponsor_seek_pending || busy) return;
    g_sponsor_seek_pending=0;
    if(!tube_sponsor_completed()) return;
    if(isfinite(position) && fabs(position-g_sponsor_seek_target)<=1.0) {
        g_sponsor_completed=g_sponsor_seek_pending=0;
        tube_playlist_eof_cancel();g_finished=0;g_skip_guard=-1;
    } else {
        g_completion_consumed=0;g_completion_action=TUBE_COMPLETION_PAUSE;
    }
}

int tube_finish_and_next(double position, double duration) {
    return tube_playback_at_end(position, duration) ? finish_and_next(1) : 0;
}

int tube_finish_drained_and_next(double position) {
    /* Called with natural EOF + retirement proof, or an acknowledged end skip.
     * Unknown/inaccurate metadata duration must not prevent a clean completion. */
    if (!isfinite(position) || position<0) return 0;
    int next=finish_and_next(1);
    if (!next && g_finished && !g_completion_consumed && !g_completion_action &&
        !tube_playlist_eof_pending() && g_completion_scope==g_playlist_scope &&
        !strcmp(g_active_profile,g_library_profile)) g_completion_action=TUBE_COMPLETION_PAUSE;
    return next;
}

int tube_take_completion_action(void) {
    int action=g_completion_action; g_completion_action=0;
    if (!g_finished || g_completion_consumed || g_completion_scope!=g_playlist_scope ||
        strcmp(g_active_profile,g_library_profile)) return 0;
    if(action) g_completion_consumed=1;
    return action;
}
void tube_completion_playback_state(int ended,int busy) {
    if (!ended && !busy && g_finished && g_completion_consumed) {
        tube_playlist_eof_cancel(); g_finished=0; g_skip_guard=-1;
    }
}

static int next_item(evo_provider_item_t *item, int consume) {
    if(consume && *g_menu_launch.id) { /* Play from start or Play incognito, taken once */
        Record *r=g_menu_launch.scope==g_playlist_scope && !strcmp(g_menu_launch.profile,g_library_profile)?record(g_menu_launch.id,0):NULL;
        g_menu_launch.id[0]=0;
        if(r && item) {*item=r->item;dearrow_item(item,r);return 1;}
    }
    if(g_manual_direction && (!consume || tube_manual_navigation_status()!=1))return 0;
    if(g_recommendation.state==1) {
        int current=recommendation_current();if(consume)g_recommendation.state=-1;
        if(!current || !item)return 0;
        if(channel_name_blocks_active() && !*g_recommendation.channel_name)return 0;
        if(channel_blocks_active() && (!*g_recommendation.channel || blocked_identity(g_recommendation.channel,g_recommendation.channel_name)>=0))return 0;
        const Record *cached=record_for(g_recommendation.next,g_active_profile,0);
        if(cached)*item=cached->item;else memset(item,0,sizeof *item);
        copy(item->id,sizeof item->id,g_recommendation.next);
        if(*g_recommendation.channel)copy(item->parent_id,sizeof item->parent_id,g_recommendation.channel);
        if(*g_recommendation.channel_name)copy(item->subtitle,sizeof item->subtitle,g_recommendation.channel_name);
        if(!*item->title)copy(item->title,sizeof item->title,item->id);
        item->kind=EVO_MEDIA_VIDEO;item->resume_pos_sec=0;
        copy(item->playback_playlist,sizeof item->playback_playlist,g_recommendation.next_context.playlist);
        item->playback_index=g_recommendation.next_context.index;item->playback_scope=g_recommendation.next_context.scope;
        if(consume)manual_departure();
        return 1;
    }
    if (g_playlist_next_ready) {
        if(consume)g_playlist_next_ready=0;
        if (!item || (channel_blocks_active() && !*g_playlist_next.parent_id) || (channel_name_blocks_active() && !*g_playlist_next.subtitle) || blocked_identity(g_playlist_next.parent_id,g_playlist_next.subtitle)>=0 || (!g_autoplay && !g_manual_direction) || !playlist_window_current()) return 0;
        *item=g_playlist_next;if(consume)manual_departure();return 1;
    }
    if (!*g_next_id || strcmp(g_next_profile, g_library_profile)) { if(consume)g_next_id[0] = 0; return 0; }
    Record *r = record_for(g_next_id, g_next_profile, 0);
    if(consume) {g_next_id[0] = 0; g_next_profile[0] = 0;}
    if (!r || blocked_identity(r->item.parent_id,r->item.subtitle)>=0 || (!r->queued && g_manual_direction>=0) || !item) return 0;
    if(consume) {
        copy(g_launch_origin.id, sizeof g_launch_origin.id, r->item.id);
        copy(g_launch_origin.profile, sizeof g_launch_origin.profile, r->profile);
        g_launch_origin.music = r->queued_music; g_launch_origin.automatic = !g_manual_direction;
        manual_departure();
    }
    *item = r->item; dearrow_item(item, r); return 1;
}

int tube_take_next(evo_provider_item_t *item) {
    memset(&g_next_offer,0,sizeof g_next_offer);
    return next_item(item,1);
}
int tube_next_offer(tube_next_offer_t *offer) {
    if (!offer || !g_next_offer.deadline || g_next_offer.scope!=g_playlist_scope ||
        strcmp(g_active_profile,g_library_profile) || !g_finished || !next_item(&offer->item,0)) return 0;
    long long left=g_next_offer.deadline-now_ms();
    offer->left_ms=left>0?(left>TUBE_NEXT_OFFER_MS?TUBE_NEXT_OFFER_MS:(int)left):0;
    offer->focus=g_next_offer.focus;return 1;
}
int tube_next_offer_hold(void) {
    if (g_next_offer.scope!=g_playlist_scope) memset(&g_next_offer,0,sizeof g_next_offer);
    if (!g_next_offer.deadline) {
        evo_provider_item_t item;
        if (!g_finished || strcmp(g_active_profile,g_library_profile) || !next_item(&item,0)) return 0;
        g_next_offer.deadline=now_ms()+TUBE_NEXT_OFFER_MS;g_next_offer.scope=g_playlist_scope;
    }
    tube_next_offer_t offer;
    return tube_next_offer(&offer) && !g_next_offer.play && offer.left_ms>0;
}
int tube_next_offer_input(int direction,int confirm,int cancel) {
    tube_next_offer_t offer;
    if (!tube_next_offer(&offer)) return 0;
    if (cancel || (confirm && g_next_offer.focus)) {
        tube_playlist_eof_cancel();g_completion_action=TUBE_COMPLETION_PAUSE;g_completion_consumed=0;
    } else if (confirm) g_next_offer.play=1;
    else if (direction) g_next_offer.focus=direction>0?1:0;
    return 1;
}

int tube_choose_stream_enabled(void) { return g_ask_stream; }

int tube_queue_recovery(double position, int live, int subtitles) {
    if (g_recovery.phase || g_recovery.attempts >= 3 || !recovery_scope() ||
        !isfinite(position) || position < 0 || position > 1000000 ||
        !!live != !!g_recovery.choice.is_live || !companion_url(g_recovery.choice.url)) return 0;
    g_recovery.position = live ? 0 : position;
    g_recovery.subtitles = !!subtitles;
    ++g_recovery.attempts; g_recovery.phase = 1;
    return 1;
}

int tube_take_recovery(evo_provider_item_t *item, double *position, int *subtitles) {
    if (g_recovery.phase != 1) return 0;
    if (!item || !position || !subtitles || !recovery_scope()) { tube_recovery_finish(); return 0; }
    Record *r = record_for(g_recovery.id, g_recovery.profile, 0);
    if (!r) { tube_recovery_finish(); return 0; }
    *item = r->item; dearrow_item(item, r); item->is_live = g_recovery.choice.is_live;
    *position = g_recovery.position; *subtitles = g_recovery.subtitles;
    g_recovery.phase = 2; return 1;
}

int tube_recovery_choice(const evo_stream_choice_t *choices, int count) {
    if (!tube_recovery_active() || !choices || count < 1 || count > CHOICES) return -1;
    const evo_stream_choice_t *old = &g_recovery.choice;
    int found = -1;
    for (int i = 0; i < count; ++i) {
        const evo_stream_choice_t *c = &choices[i];
        if (!companion_url(c->url) || !isfinite(c->fps) || c->fps != old->fps ||
            c->width != old->width || c->height != old->height ||
            !!c->is_live != !!old->is_live || c->audio_channels != old->audio_channels ||
            strcmp(c->container, old->container) || strcmp(c->video_codec, old->video_codec) ||
            strcmp(c->audio_codec, old->audio_codec) || strcmp(c->video_range, old->video_range)) continue;
        if (found >= 0) return -1; /* ambiguous replacement requires a new user selection */
        found = i;
    }
    return found;
}

static void tube_shutdown(void) { live_retire(); if (tube_update_app_shutdown) tube_update_app_shutdown(); tube_embedded_stop(); tube_video_geometry_enable(0); tube_caption_switch_cancel(); g_caption_track_count = 0; ++g_caption_revision; tube_remote_shutdown(); tube_cancel(); tube_queue_retry_cancel(); save();
    tube_player_extra_set_save_hook(NULL,NULL); tube_extra_video_end(); cJSON_Delete(g_resolved_facts); g_resolved_facts = NULL; }
const evo_provider_t evo_provider_youtube = {
    .id = "youtube", .name = "ProsperoTube", .icon = "icon_emby.png",
    .caps = EVO_PROVIDER_CAP_CATALOG | EVO_PROVIDER_CAP_SEARCH | EVO_PROVIDER_CAP_RESOLVE |
            EVO_PROVIDER_CAP_PROGRESS | EVO_PROVIDER_CAP_CONFIG | EVO_PROVIDER_CAP_PICK,
    .api_version = EVO_PROVIDER_API_VERSION,
    .init = init, .shutdown = tube_shutdown, .is_configured = configured,
    .list_catalog = catalog, .search = search, .resolve = resolve,
    .report_progress = progress, .stream_chosen = chosen,
    .get_source = get_source, .set_source = set_source, .ui_embedded = "youtube"
};

/* ---- In-player actions (see the end of tube_provider.h). The video actions
 * are the browser's own catalog pages, listed for the playing video while it
 * keeps playing: nothing here names an action the menu does not offer. ---- */
#define PLAYER_MENU_ROWS 320
#define PLAYER_MENU_TEXT 104          /* bytes of one description line */
static struct {
    unsigned serial;
    int state, page, base, count, root, calling, snapshot;
    char parent[EVO_PROVIDER_MAX_ITEM_ID], title[96], message[160];
    tube_player_menu_row_t rows[PLAYER_MENU_ROWS];
    /* The browser's own list state, put back when the menu closes. */
    char library_parent[sizeof g_library_parent], (*cursors)[65];
    int browse_music, browse_queue;
} g_player_menu;
static struct { evo_provider_item_t item; char profile[33]; unsigned scope; int ready; } g_player_launch;
static struct { char parent[EVO_PROVIDER_MAX_ITEM_ID], title[EVO_PROVIDER_MAX_TITLE], profile[33]; unsigned scope; int kind; } g_player_browse;
static struct { char id[160]; int attempts, relaxed; } g_player_fix;
static evo_provider_item_t g_player_root[64];
static int g_player_root_count, g_player_root_more;

int tube_player_menu_calling(void) { return g_player_menu.calling; }
static const char *player_video(void) {
    return valid_id(g_active_id) && recovery_scope() ? g_active_id : "";
}
static void player_root_items(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    (void)ud;
    g_player_root_count = 0; g_player_root_more = ok && more;
    for (int i = 0; ok && items && i < count && i < 64; ++i) g_player_root[g_player_root_count++] = items[i];
}
/* Rows of the actions menu that mean nothing while their video is playing. */
static int player_row_hidden(const char *id) {
    if (!strcmp(id, "settings:refresh") || !strncmp(id, "playlist:prompt:", 16)) return 1;
    return !strncmp(id, "local:video:", 12) && (strstr(id, ":next:") || strstr(id, ":restart:") || strstr(id, ":incognito:"));
}
static int player_row_matches(int action, const char *id) {
    switch (action) {
    case TUBE_PLAYER_LIKE: return !strncmp(id, "account:confirm:like:", 21);
    case TUBE_PLAYER_DISLIKE: return !strncmp(id, "account:confirm:dislike:", 24);
    case TUBE_PLAYER_SUBSCRIBE: return !strncmp(id, "account:confirm:subscribe:", 26);
    case TUBE_PLAYER_PLAYLIST_ADD: return !strncmp(id, "playlist:pick:add:", 18);
    case TUBE_PLAYER_SUGGESTIONS: return !strncmp(id, "library:suggestions:", 20);
    case TUBE_PLAYER_DESCRIPTION: return !strncmp(id, "description:", 12);
    case TUBE_PLAYER_SHARE: return !strncmp(id, "link:video:", 11);
    case TUBE_PLAYER_SPONSOR: return !strncmp(id, "local:video:", 12) && (strstr(id, ":sbexclude:") || strstr(id, ":sbinclude:"));
    case TUBE_PLAYER_CHANNEL: return !strncmp(id, "channel:", 8);
    case TUBE_PLAYER_PLAYLIST: return !strncmp(id, "library:playlist:", 17) || (!strncmp(id, "playlist:", 9) && !strchr(id + 9, ':'));
    default: return 0;
    }
}
/* The row of the actions menu behind one direct action; NULL when the menu does not offer it. */
static const evo_provider_item_t *player_action_row(int action) {
    static evo_provider_item_t fixed;
    const char *video = player_video();
    if (!*video || action < 0 || action >= TUBE_PLAYER_ACTION_COUNT) return NULL;
    if (action == TUBE_PLAYER_ACTIONS || action == TUBE_PLAYER_QUEUE) {
        char parent[64]; snprintf(parent, sizeof parent, "account:menu:%s", video);
        if (action == TUBE_PLAYER_ACTIONS) folder(&fixed, parent, "Video actions", "Rate, save, share and more for this video");
        else folder(&fixed, "queue", "Playback queue", "Videos waiting to play; Cross plays one now");
        return &fixed;
    }
    char parent[64]; snprintf(parent, sizeof parent, "account:menu:%s", video);
    /* The menu spans more than one page for an account profile. */
    for (int page = 0; page < 4; ++page) {
        g_player_root_count = g_player_root_more = 0;
        if (!local_catalog(parent, page, player_root_items, NULL)) return NULL;
        for (int i = 0; i < g_player_root_count; ++i)
            if (player_row_matches(action, g_player_root[i].id)) return &g_player_root[i];
        if (!g_player_root_more) break;
    }
    return NULL;
}
int tube_player_action(int action, char *label, size_t label_size, char *detail, size_t detail_size) {
    const evo_provider_item_t *row = player_action_row(action);
    if (label && label_size) copy(label, label_size, row ? row->title : "");
    if (detail && detail_size) copy(detail, detail_size, row ? row->subtitle : "");
    return row != NULL;
}

static void player_menu_items(int ok, const evo_provider_item_t *items, int count, int more, void *ud);
static void player_menu_stale(void) {
    for (int i = 0; i < REQUESTS; ++i) {
        Request *r = &g_requests[i];
        if (!r->used || r->items_cb != player_menu_items) continue;
        /* Retired like a cancelled browser request: its reply is dropped. */
        r->generation = g_generation - 1;
        if (*r->job && *g_source && *g_token) {
            char url[1024], auth[160];
            snprintf(url, sizeof url, "%s/v1/jobs/%s", g_source, r->job);
            snprintf(auth, sizeof auth, "Authorization: Bearer %s", g_token);
            const char *headers[] = {auth};
            evo_net_request_async("DELETE", url, NULL, headers, 1, ignored, NULL);
        }
        if (!r->inflight) complete(r, 0);
    }
}
static void player_menu_snapshot(void) {
    if (g_player_menu.snapshot) return;
    g_player_menu.cursors = malloc(sizeof g_library_cursors);
    if (g_player_menu.cursors) memcpy(g_player_menu.cursors, g_library_cursors, sizeof g_library_cursors);
    copy(g_player_menu.library_parent, sizeof g_player_menu.library_parent, g_library_parent);
    g_player_menu.browse_music = g_browse_music; g_player_menu.browse_queue = g_browse_queue;
    g_player_menu.snapshot = 1;
}
/* One line of at most PLAYER_MENU_TEXT - 1 bytes, broken at a space and never inside a character. */
static const char *player_text_line(const char *text, char *out) {
    size_t length = 0, space = 0;
    while (text[length] && text[length] != '\n' && length < PLAYER_MENU_TEXT - 1) {
        if (text[length] == ' ') space = length;
        ++length;
    }
    if (text[length] && text[length] != '\n') {
        if (space) length = space;
        else while (length > 1 && ((unsigned char)text[length] & 0xc0) == 0x80) --length;
    }
    memcpy(out, text, length); out[length] = 0;
    for (size_t i = 0; i < length; ++i) if ((unsigned char)out[i] < 32) out[i] = ' ';
    text += length;
    while (*text == ' ') ++text;
    if (*text == '\n') ++text;
    return text;
}
static tube_player_menu_row_t *player_menu_add(const char *id, const char *title, const char *detail, int is_folder) {
    if (g_player_menu.count >= PLAYER_MENU_ROWS) return NULL;
    tube_player_menu_row_t *row = &g_player_menu.rows[g_player_menu.count++];
    memset(row, 0, sizeof *row);
    copy(row->id, sizeof row->id, id); copy(row->title, sizeof row->title, title);
    copy(row->detail, sizeof row->detail, detail); row->folder = is_folder;
    return row;
}
static void player_menu_items(int ok, const evo_provider_item_t *items, int count, int more, void *ud) {
    if (!g_player_menu.serial || (unsigned)(uintptr_t)ud != g_player_menu.serial) return;
    if (!ok) {
        g_player_menu.state = -1; return;   /* the reason is the provider's last error */
    }
    g_player_menu.count = g_player_menu.base;
    const char *text = NULL;
    for (int i = 0; items && i < count; ++i) {
        if (player_row_hidden(items[i].id)) continue;
        if (!strncmp(items[i].id, "description:text:", 17)) {
            /* The reader's text follows its header rows as lines of this list. */
            text = tube_comment_text(items[i].id);
            if (!text) text = items[i].overview;
            player_menu_add("account:notice", items[i].title, "Description", 1);
            continue;
        }
        /* A link is listed as its address; the codes are on the browser's page. */
        const int link = !strncmp(items[i].id, "link:row:", 9) && *items[i].overview;
        tube_player_menu_row_t *row = link ? player_menu_add(items[i].id, items[i].overview, items[i].title, 1) :
            player_menu_add(items[i].id, items[i].title, items[i].subtitle, items[i].is_folder);
        if (!row) break;
        copy(row->playlist, sizeof row->playlist, items[i].playback_playlist);
        row->index = items[i].playback_index; row->scope = items[i].playback_scope;
        if (!row->folder) {
            copy(row->art_url, sizeof row->art_url, items[i].art_url);
            copy(row->badges, sizeof row->badges, tube_row_fact_get(items[i].id, "badges"));
            row->duration_sec = items[i].duration_sec;
            row->is_live = items[i].is_live;
        }
    }
    while (text && *text && g_player_menu.count < PLAYER_MENU_ROWS - 1) {
        char line[PLAYER_MENU_TEXT];
        text = player_text_line(text, line);
        player_menu_add("account:notice", line, "", 1);
    }
    if (more && g_player_menu.page + 1 < LIBRARY_PAGES) player_menu_add("more:", "More results", "Load the next page of this list", 1);
    if (!strncmp(g_player_menu.parent, "link:", 5) && g_player_menu.count)
        player_menu_add(g_player_menu.parent, "Show QR codes", "Stops this video and opens these links with their QR codes", 1);
    if (!g_player_menu.count) player_menu_add("account:notice", "Nothing is listed here", "Circle returns", 1);
    g_player_menu.state = 1;
}
static int player_menu_list(const char *parent, int page, const char *title, int root) {
    player_menu_stale();
    if (!++g_player_menu.serial) ++g_player_menu.serial;
    if (!page) {
        g_player_menu.count = 0;
        copy(g_player_menu.parent, sizeof g_player_menu.parent, parent);
        copy(g_player_menu.title, sizeof g_player_menu.title, title);
        g_player_menu.root = root;
    } else if (g_player_menu.count && !strcmp(g_player_menu.rows[g_player_menu.count - 1].id, "more:")) --g_player_menu.count;
    g_player_menu.base = g_player_menu.count; g_player_menu.page = page;
    g_player_menu.state = 0; g_player_menu.message[0] = 0;
    player_menu_snapshot();
    char target[EVO_PROVIDER_MAX_ITEM_ID]; copy(target, sizeof target, g_player_menu.parent);
    g_player_menu.calling = 1;
    int result = catalog(target, page, player_menu_items, (void *)(uintptr_t)g_player_menu.serial);
    /* The actions menu is local and short: its further pages join at once. */
    while (!strncmp(target, "account:menu:", 13) && g_player_menu.state == 1 && g_player_menu.count &&
           !strcmp(g_player_menu.rows[g_player_menu.count - 1].id, "more:")) {
        g_player_menu.base = --g_player_menu.count; g_player_menu.page = ++page; g_player_menu.state = 0;
        result = catalog(target, page, player_menu_items, (void *)(uintptr_t)g_player_menu.serial);
    }
    g_player_menu.calling = 0;
    if (result < 0 && !g_player_menu.state) {
        copy(g_player_menu.message, sizeof g_player_menu.message, "The companion is not ready for this action.");
        g_player_menu.state = -1;
    }
    return TUBE_MENU_OPENED;
}
static int player_browse(int kind, const char *parent, const char *title) {
    memset(&g_player_browse, 0, sizeof g_player_browse);
    g_player_browse.kind = kind; g_player_browse.scope = g_playlist_scope;
    copy(g_player_browse.parent, sizeof g_player_browse.parent, parent);
    copy(g_player_browse.title, sizeof g_player_browse.title, title);
    copy(g_player_browse.profile, sizeof g_player_browse.profile, g_library_profile);
    return TUBE_MENU_LEAVE;
}
int tube_player_menu_begin(int action) {
    const evo_provider_item_t *row = player_action_row(action);
    if (!row) return TUBE_MENU_NOTHING;
    if (action == TUBE_PLAYER_CHANNEL) return player_browse(1, row->id, *row->subtitle ? row->subtitle : "Channel");
    char id[EVO_PROVIDER_MAX_ITEM_ID], title[96];
    copy(id, sizeof id, row->id); copy(title, sizeof title, row->title);
    return player_menu_list(id, 0, title, action == TUBE_PLAYER_ACTIONS);
}
int tube_player_menu_open(int index) {
    if (index < 0 || index >= g_player_menu.count || g_player_menu.state != 1 || !*player_video()) return TUBE_MENU_NOTHING;
    const tube_player_menu_row_t *row = &g_player_menu.rows[index];
    if (!strcmp(row->id, "more:")) return player_menu_list(g_player_menu.parent, g_player_menu.page + 1, g_player_menu.title, g_player_menu.root);
    if (!strncmp(row->id, "account:notice", 14) || !strncmp(row->id, "settings:", 9) || !strncmp(row->id, "link:row:", 9)) return TUBE_MENU_NOTHING;
    /* The QR codes are drawn by the browser's link page: the row of a links list leaves for it. */
    if (!strncmp(row->id, "link:", 5) && !strcmp(row->id, g_player_menu.parent)) return player_browse(1, row->id, "Share link");
    if (!strncmp(row->id, "channel:", 8)) return player_browse(1, row->id, row->folder && strcmp(row->title, "Open channel") ? row->title : *row->detail ? row->detail : "Channel");
    if (!row->folder) {
        if (!valid_id(row->id)) return TUBE_MENU_NOTHING;
        const Record *known = record(row->id, 0);
        memset(&g_player_launch, 0, sizeof g_player_launch);
        if (known) { g_player_launch.item = known->item; dearrow_item(&g_player_launch.item, known); }
        copy(g_player_launch.item.id, sizeof g_player_launch.item.id, row->id);
        if (!*g_player_launch.item.title) copy(g_player_launch.item.title, sizeof g_player_launch.item.title, row->title);
        g_player_launch.item.kind = EVO_MEDIA_VIDEO; g_player_launch.item.is_folder = 0;
        copy(g_player_launch.item.playback_playlist, sizeof g_player_launch.item.playback_playlist, row->playlist);
        g_player_launch.item.playback_index = row->index; g_player_launch.item.playback_scope = row->scope;
        copy(g_player_launch.profile, sizeof g_player_launch.profile, g_library_profile);
        g_player_launch.scope = g_playlist_scope; g_player_launch.ready = 1;
        return TUBE_MENU_LEAVE;
    }
    char id[EVO_PROVIDER_MAX_ITEM_ID], title[96];
    copy(id, sizeof id, row->id); copy(title, sizeof title, row->title);
    return player_menu_list(id, 0, title, 0);
}
int tube_player_menu_status(void) { return g_player_menu.serial ? g_player_menu.state : -1; }
int tube_player_menu_count(void) { return g_player_menu.state == 1 ? g_player_menu.count : 0; }
const tube_player_menu_row_t *tube_player_menu_row(int index) {
    return index >= 0 && index < tube_player_menu_count() ? &g_player_menu.rows[index] : NULL;
}
const char *tube_player_menu_title(void) { return g_player_menu.title; }
const char *tube_player_menu_message(void) { return g_player_menu.message; }
int tube_player_menu_root(void) { return g_player_menu.root; }
void tube_player_menu_close(void) {
    player_menu_stale();
    if (g_player_menu.snapshot) {
        if (g_player_menu.cursors) memcpy(g_library_cursors, g_player_menu.cursors, sizeof g_library_cursors);
        free(g_player_menu.cursors); g_player_menu.cursors = NULL;
        copy(g_library_parent, sizeof g_library_parent, g_player_menu.library_parent);
        g_browse_music = g_player_menu.browse_music; g_browse_queue = g_player_menu.browse_queue;
        g_player_menu.snapshot = 0;
    }
    g_player_menu.serial = 0; g_player_menu.state = 0; g_player_menu.count = 0; g_player_menu.root = 0;
    g_player_menu.parent[0] = g_player_menu.title[0] = g_player_menu.message[0] = 0;
}
int tube_player_take_launch(evo_provider_item_t *item) {
    const int ready = g_player_launch.ready && g_player_launch.scope == g_playlist_scope &&
        !strcmp(g_player_launch.profile, g_library_profile);
    g_player_launch.ready = 0;
    if (!ready || !item) return 0;
    *item = g_player_launch.item; return 1;
}
int tube_player_launch_pending(void) { return g_player_launch.ready; }
void tube_player_request_search(void) { player_browse(2, "", ""); }
int tube_player_take_browse(char *parent, size_t parent_size, char *title, size_t title_size) {
    const int kind = g_player_browse.scope == g_playlist_scope && !strcmp(g_player_browse.profile, g_library_profile)
        ? g_player_browse.kind : 0;
    if (parent && parent_size) copy(parent, parent_size, kind ? g_player_browse.parent : "");
    if (title && title_size) copy(title, title_size, kind ? g_player_browse.title : "");
    memset(&g_player_browse, 0, sizeof g_player_browse);
    return kind;
}

/* Resolution is fixed; frame-rate and codec preferences belong to the profile. */
void tube_player_quality(int *height, int *fps, int *codec) {
    if (height) *height = g_height;
    if (fps) *fps = g_max_fps;
    if (codec) *codec = g_codec;
}
int tube_player_set_quality(int height, int fps, int codec) {
    if (height < 0) height = g_height;
    if (fps < 0) fps = g_max_fps;
    if (codec < 0) codec = g_codec;
    if ((height != 144 && height != 240 && height != 360 && height != 480 && height != 720 && height != 1080 &&
         height != 1440 && height != 2160) || (fps != 0 && fps != 30 && fps != 60) || codec > 4) return 0;
    if (fps != g_max_fps || codec != g_codec) { g_max_fps = fps; g_codec = codec; save(); }
    return 1;
}
int tube_player_remember_height(int height) {
    /* A per-video choice must never lower the maximum for subsequent videos. */
    return height > 0;
}
static int player_reopen(double position, int live, int subtitles, ResolveQuality quality) {
    g_recovery.position = live ? 0 : position; g_recovery.subtitles = !!subtitles;
    g_recovery.quality = quality; g_recovery.phase = 1;
    copy(g_player_fix.id, sizeof g_player_fix.id, g_recovery.id);
    g_player_fix.attempts = g_recovery.attempts; g_player_fix.relaxed = 1;
    return 1;
}
int tube_player_reload(double position, int live, int subtitles) {
    if (g_recovery.phase || !recovery_scope() || !isfinite(position) || position < 0 || position > 1000000 ||
        !!live != !!g_recovery.choice.is_live || !companion_url(g_recovery.choice.url)) return 0;
    return player_reopen(position, live, subtitles, (ResolveQuality){g_height, g_max_fps, g_codec});
}
static int player_codec(const char *name) {
    return !strncmp(name, "avc", 3) || !strncmp(name, "h264", 4) ? TUBE_CODEC_AVC : !strncmp(name, "vp9", 3) || !strncmp(name, "vp09", 4) ? TUBE_CODEC_VP9 :
        !strncmp(name, "av1", 3) || !strncmp(name, "av01", 4) ? TUBE_CODEC_AV1 : !strncmp(name, "hev", 3) || !strncmp(name, "hvc", 3) || !strncmp(name, "h265", 4) ? TUBE_CODEC_HEVC : TUBE_CODEC_AUTO;
}
int tube_error_fix_begin(int error, double position, int live, int subtitles) {
    if (g_recovery.phase || !recovery_scope()) return TUBE_FIX_NONE;
    const evo_stream_choice_t *playing = &g_recovery.choice;
    int height = playing->height > 0 ? playing->height : g_recovery.quality.height;
    int fps = isfinite(playing->fps) && playing->fps > 0 ? (int)(playing->fps + 0.5) : g_recovery.quality.max_fps;
    int codec = player_codec(playing->video_codec);
    const tube_fix_input input = {tube_extra_error_fixer(), error, live, subtitles, height, fps, codec, g_recovery.attempts};
    const int action = tube_error_fix_decide(&input);
    if (!(action & TUBE_FIX_ACTION)) return TUBE_FIX_NONE;
    const int captions = subtitles && !(action & TUBE_FIX_SUBTITLES_OFF);
    /* Arms the existing bounded recovery: three attempts for one video. */
    if (!tube_queue_recovery(position, live, captions)) return TUBE_FIX_NONE;
    fps = g_recovery.quality.max_fps; codec = g_recovery.quality.codec;
    if (tube_error_fix_quality(action, &height, &fps, &codec))
        player_reopen(g_recovery.position, live, captions, (ResolveQuality){height, fps, codec});
    return action;
}
int tube_error_fix_relaxed(void) {
    return g_player_fix.relaxed && tube_recovery_active() && g_player_fix.attempts == g_recovery.attempts &&
        !strcmp(g_player_fix.id, g_recovery.id);
}
int tube_error_fix_choice(const evo_stream_choice_t *choices, int count) {
    const int relaxed = tube_error_fix_relaxed();
    g_player_fix.relaxed = 0;
    if (!relaxed || !choices || count < 1 || count > CHOICES) return -1;
    /* The companion lists the best stream inside the requested limits first. */
    for (int i = 0; i < count; ++i)
        if (companion_url(choices[i].url) && !!choices[i].is_live == !!g_recovery.choice.is_live) return i;
    return -1;
}
/* Sign-in in progress: the code or QR on screen must stay readable, so the screen never dims. */
int tube_player_dim_blocked(void) { return g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING; }
