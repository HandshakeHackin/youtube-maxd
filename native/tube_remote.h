/* PS5 main-thread YouTube Lounge bridge. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_REMOTE_H
#define TUBE_REMOTE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { TUBE_REMOTE_OFF, TUBE_REMOTE_STARTING, TUBE_REMOTE_LISTENING, TUBE_REMOTE_FAILED };
enum { TUBE_REMOTE_PLAY = 1, TUBE_REMOTE_PAUSE, TUBE_REMOTE_NEXT, TUBE_REMOTE_PREVIOUS,
    TUBE_REMOTE_STOP, TUBE_REMOTE_QUERY, TUBE_REMOTE_PLAYLIST, TUBE_REMOTE_QUEUE,
    TUBE_REMOTE_SEEK, TUBE_REMOTE_VOLUME, TUBE_REMOTE_CONNECTED, TUBE_REMOTE_DISCONNECTED,
    TUBE_REMOTE_DEVICES, TUBE_REMOTE_DPAD, TUBE_REMOTE_VOICE, TUBE_REMOTE_CAPTIONS };
typedef struct {
    unsigned owner;
    int sequence, action, index, count, has_volume, has_delta, devices, has_window, window_start;
    int has_indices, indices[128];
    double position, volume, delta;
    char video[12], videos[128][12], playlist[101], ctt[513];
    char device[513], name[401], key[9], voice[13], language[33];
} tube_remote_command;
typedef struct {
    char video[12], playlist[101], ctt[513], language[33], vss_id[65];
    unsigned selection; /* Native-only selection identity; never serialized to Lounge. */
    int state, index, volume, muted, previous, next;
    double position, duration;
} tube_remote_player;
/* Pass only the provider's validated LAN source/token and selected profile.
 * Changing any scope invalidates pending commands. No background UI callbacks. */
void tube_remote_configure(const char *source, const char *token, const char *profile,
                           int enabled, const char *device, const char *screen);
void tube_remote_retry(void);
/* Capability changes retire the old receiver and rebind the same identity. */
void tube_remote_voice_available(int available);
void tube_remote_pair(void);
void tube_remote_tick(int64_t now_ms);
void tube_remote_report(const tube_remote_player *player);
int tube_remote_take(tube_remote_command *command);
int tube_remote_finish(unsigned owner, int sequence, int applied);
/* A processed unavailable/cancelled launch consumes its command without claiming
 * playback success. Media feedback and native selection binding remain separate. */
int tube_remote_retire_launch(unsigned owner, int sequence);
/* Retire only the outstanding launch if a later stop/replacement/disconnect exists.
 * Intermediate commands remain queued and execute in their original order. */
int tube_remote_launch_interrupted(unsigned owner, int sequence);
int tube_remote_owner_current(unsigned owner);
unsigned tube_remote_current_owner(void);
int tube_remote_command_current(unsigned owner, int sequence);
int tube_remote_state(void);
const char *tube_remote_code(void);
const char *tube_remote_device(void);
const char *tube_remote_screen(void);
unsigned tube_remote_revision(void);
int tube_remote_connected_count(void);
const char *tube_remote_connected_name(int index);
int tube_remote_identity_valid(const char *device);
void tube_remote_shutdown(void);
#ifdef __cplusplus
}
#endif
#endif
