/* Ephemeral Lounge queue; never changes the provider's saved local queue.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_phone.h"
#include <math.h>
#include <string.h>
#include <time.h>
typedef struct {
    unsigned owner;
    unsigned selection;
    int count, cursor, base;
    int indices[128];
    char videos[128][12], video[12], playlist[101], ctt[513];
} PhoneQueue;
static PhoneQueue current, proposed;
static PhoneQueue refill;
static int64_t refill_deadline, refill_not_before;
static double refill_position, refill_duration;
static tube_remote_command pending;
static unsigned serial, ticket, caption_selection;
static int phase; /* 1 ready, 2 resolving/starting, 3 caption, 4 unavailable delay */
static int64_t retry_deadline;
static int interruptible_launch;
static double resume;
static tube_remote_player last;
static unsigned last_owner;
static char remote_video[12];
static unsigned remote_selection;
static void clear_queue(void) {
    memset(&refill, 0, sizeof refill); refill_deadline = refill_not_before = 0;
    refill_position = refill_duration = 0;
    memset(&current, 0, sizeof current); memset(&proposed, 0, sizeof proposed);
    memset(&pending, 0, sizeof pending); phase = 0; ticket = 0; retry_deadline = 0; interruptible_launch = 0;
    memset(&last, 0, sizeof last); last_owner = 0;
}
void tube_phone_clear(void) {
    clear_queue(); remote_video[0] = 0; remote_selection = 0;
}
void tube_phone_cancel(void) {
    if (phase && pending.sequence) {
        if (phase == 4) tube_remote_retire_launch(pending.owner,pending.sequence);
        else tube_remote_finish(pending.owner, pending.sequence, 0);
    }
    clear_queue();
}
static int launch_valid(void) {
    return phase && tube_remote_owner_current(pending.owner) &&
        (!pending.sequence || tube_remote_command_current(pending.owner, pending.sequence));
}
static void prune(void);
int tube_phone_launch_current(unsigned value) { prune(); return value && value == ticket && launch_valid(); }
static void prune(void) {
    if (refill.owner && (!tube_remote_owner_current(refill.owner) || tube_remote_launch_interrupted(refill.owner,0))) clear_queue();
    if (interruptible_launch && phase && phase != 3 && launch_valid() && tube_remote_launch_interrupted(pending.owner,pending.sequence)) clear_queue();
    if ((phase && !launch_valid()) || (current.owner && !tube_remote_owner_current(current.owner))) clear_queue();
}
int tube_phone_launch_pending(void) { prune(); return phase == 1; }
int tube_phone_retry_waiting(void) { prune(); return phase == 4 || refill.owner; }
static int find(const PhoneQueue *q, const char *id) {
    for (int i = 0; i < q->count; ++i) if (!strcmp(q->videos[i], id)) return i;
    return -1;
}
static PhoneQueue playlist(const tube_remote_command *c, const char *playing) {
    PhoneQueue q; memset(&q, 0, sizeof q); q.owner = c->owner;
    q.count = c->count; memcpy(q.videos, c->videos, sizeof q.videos);
    memcpy(q.video, playing, 12); memcpy(q.playlist, c->playlist, sizeof q.playlist); memcpy(q.ctt, c->ctt, sizeof q.ctt);
    q.cursor = find(&q, playing);
    /* Wire index may be global while the sender supplies a playlist window. */
    int anchor = *c->video ? find(&q, c->video) : q.cursor;
    if (c->has_window) anchor = c->index - c->window_start;
    if (c->has_indices) {
        anchor = -1;
        for (int i = 0; i < q.count; ++i) if (c->indices[i] == c->index) anchor = i;
    }
    const char *anchor_id = *c->video ? c->video : playing;
    if (!c->has_window && c->index < q.count && !strcmp(q.videos[c->index], anchor_id)) anchor = c->index;
    if (!strcmp(playing, anchor_id)) q.cursor = anchor;
    q.base = c->has_window ? c->window_start : anchor < 0 ? c->index : c->index >= anchor ? c->index - anchor : 0;
    for (int i = 0; i < q.count; ++i) q.indices[i] = c->has_indices ? c->indices[i] : q.base + i;
    return q;
}
static void begin(const tube_remote_command *c, const PhoneQueue *q, double position) {
    pending = *c; proposed = *q; proposed.selection = 0; resume = position; phase = 1; retry_deadline = 0; interruptible_launch = 0;
    if (!++serial) ++serial;
    ticket = serial;
}
unsigned tube_phone_take(char video[12], double *position) {
    prune(); if (phase != 1 || !video || !position) return 0;
    memcpy(video, proposed.video, 12); *position = resume; phase = 2; return ticket;
}
static int wait_window(const PhoneQueue *anchor, int64_t now_ms, int delay, double position, double duration) {
    if (anchor->cursor < 0 || anchor->cursor >= anchor->count || !*anchor->playlist ||
        !tube_remote_owner_current(anchor->owner) || now_ms < 0 || now_ms > INT64_MAX - 30000) return 0;
    refill = *anchor;
    refill_not_before = now_ms + delay; refill_deadline = now_ms + 30000;
    refill_position = position; refill_duration = duration;
    return 1;
}
int tube_phone_launch_unavailable(unsigned value, const char *video, int64_t now_ms) {
    if (phase != 2 || !video || !tube_phone_launch_current(value) || strcmp(video,proposed.video) ||
        now_ms < 0 || now_ms > INT64_MAX - 30000) return 0;
    if (proposed.cursor < 0 || proposed.cursor + 1 >= proposed.count) {
        unsigned owner = pending.owner; int sequence = pending.sequence;
        PhoneQueue anchor = proposed;
        clear_queue(); remote_video[0] = 0; remote_selection = 0;
        if (sequence) tube_remote_retire_launch(owner,sequence);
        anchor.selection = 0;
        if (wait_window(&anchor,now_ms,5000,0,0)) return 3;
        return 2;
    }
    ++proposed.cursor; memcpy(proposed.video,proposed.videos[proposed.cursor],12);
    proposed.selection = 0; resume = 0; phase = 4; interruptible_launch = 1; retry_deadline = now_ms + 5000;
    if (!++serial) ++serial;
    ticket = serial; /* The failed attempt cannot bind or acknowledge this one. */
    return 1;
}
int tube_phone_launch_finish(unsigned value, int success) {
    if (phase != 2 || !tube_phone_launch_current(value)) { prune(); return 0; }
    unsigned owner = pending.owner; int sequence = pending.sequence;
    if (success && !proposed.selection) success = 0;
    if (success) current = proposed;
    phase = 0; ticket = 0; interruptible_launch = 0;
    if (sequence) return tube_remote_finish(owner, sequence, success);
    return !!success;
}
int tube_phone_launch_bind(unsigned value, unsigned selection) {
    if (!selection || phase != 2 || !tube_phone_launch_current(value)) return 0;
    proposed.selection = selection; return 1;
}
static int advance(const tube_remote_command *c, int direction) {
    int cursor = current.cursor + direction;
    if (current.owner != c->owner || current.cursor < 0 || cursor < 0 || cursor >= current.count || current.indices[cursor] > 100000) return 0;
    PhoneQueue q = current; q.cursor = cursor; memcpy(q.video, q.videos[cursor], 12);
    begin(c, &q, 0); return 1;
}
static int64_t monotonic_ms(void) {
    struct timespec now;
    return clock_gettime(CLOCK_MONOTONIC,&now) == 0 ?
        (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000 : -1;
}
int tube_phone_finished(void) { return tube_phone_finished_at(monotonic_ms()); }
int tube_phone_finished_at(int64_t now_ms) {
    prune();
    if (phase || !*remote_video || strcmp(last.video, remote_video) || last.selection != remote_selection) return 0;
    if (!current.owner) { remote_video[0] = 0; return 1; }
    tube_remote_command c; memset(&c, 0, sizeof c); c.owner = current.owner;
    if (tube_remote_launch_interrupted(c.owner,0)) {
        clear_queue(); remote_video[0] = 0; remote_selection = 0;
        return 1;
    }
    if (advance(&c, 1)) {
        /* Automatic EOF has no command sequence. Keep it interruptible through
         * resolution and native binding, just like unavailable-video retries. */
        interruptible_launch = 1;
    } else {
        PhoneQueue anchor = current;
        double position = last.position, duration = last.duration;
        clear_queue(); remote_video[0] = 0; remote_selection = 0;
        wait_window(&anchor,now_ms,0,position,duration);
    }
    return 1; /* Remote EOF never removes an item from the saved local queue. */
}
int tube_phone_owns_media(void) {
    prune(); return *remote_video && !strcmp(last.video, remote_video) && last.selection == remote_selection;
}
void tube_phone_tick(const tube_phone_controls *ctl) {
    tube_phone_tick_at(ctl,monotonic_ms());
}
void tube_phone_tick_at(const tube_phone_controls *ctl, int64_t now_ms) {
    if (!ctl || !ctl->snapshot) return;
    prune();
    tube_remote_player p; memset(&p, 0, sizeof p); p.volume = 100; p.state = -1;
    int active = ctl->snapshot(ctl->context, &p);
    if (refill.owner && (now_ms >= refill_deadline || (active &&
        (!refill.selection || p.selection != refill.selection || strcmp(p.video,refill.video) || p.state != 0)))) clear_queue();
    if (!active) {
        if (last_owner && tube_remote_owner_current(last_owner) && *last.video) {
            int volume = p.volume, muted = p.muted;
            p = last; p.state = 2; p.previous = p.next = 0;
            p.volume = volume; p.muted = muted;
        } else { p.video[0] = 0; p.position = p.duration = 0; p.state = -1; }
    }
    if (!phase && current.owner && active && strcmp(p.video, current.video)) memset(&current, 0, sizeof current);
    if (current.owner && active && current.selection && current.selection != p.selection) memset(&current, 0, sizeof current);
    if (*remote_video && active && (strcmp(p.video, remote_video) || p.selection != remote_selection)) remote_video[0] = 0;
    if (current.owner && active && !strcmp(p.video, current.video)) {
        current.selection = p.selection;
        memcpy(remote_video, p.video, sizeof remote_video); remote_selection = p.selection;
        memcpy(p.playlist, current.playlist, sizeof p.playlist); memcpy(p.ctt, current.ctt, sizeof p.ctt);
        p.index = current.cursor < 0 ? current.base : current.indices[current.cursor];
        if (p.index > 100000) p.index = 100000;
        p.previous = current.cursor > 0; p.next = current.cursor >= 0 && current.cursor + 1 < current.count;
    }
    if (refill.owner) {
        /* Ended occurrence, never a fictitious playing selection. This scoped
         * anchor lets the service return the next bounded playlist window. */
        int volume = p.volume, muted = p.muted;
        memset(&p, 0, sizeof p); p.volume = volume; p.muted = muted; p.state = 0;
        memcpy(p.video, refill.video, sizeof p.video);
        memcpy(p.playlist, refill.playlist, sizeof p.playlist);
        memcpy(p.ctt, refill.ctt, sizeof p.ctt);
        p.index = refill.indices[refill.cursor];
        p.position = refill_position; p.duration = refill_duration;
    }
    last = p;
    last_owner = tube_remote_current_owner();
    tube_remote_report(&p);
    if (phase == 4 && now_ms >= retry_deadline) { phase = 1; retry_deadline = 0; }
    if (phase == 3) {
        int result = active && p.selection == caption_selection && !strcmp(p.video, pending.video) && ctl->captions_status ? ctl->captions_status(ctl->context) : -1;
        if (!result) return;
        unsigned owner = pending.owner; int sequence = pending.sequence;
        phase = 0; tube_remote_finish(owner, sequence, result > 0); return;
    }
    if (phase) return;
    tube_remote_command c; if (!tube_remote_take(&c)) return;
    if (ctl->activity && c.action != TUBE_REMOTE_QUERY && c.action != TUBE_REMOTE_QUEUE && c.action != TUBE_REMOTE_CONNECTED &&
        c.action != TUBE_REMOTE_DISCONNECTED && c.action != TUBE_REMOTE_DEVICES) ctl->activity(ctl->context);
    int applied = 0;
    switch (c.action) {
    case TUBE_REMOTE_PLAYLIST: {
        PhoneQueue q = playlist(&c, c.video); begin(&c, &q, c.position);
        if (ctl->browser) ctl->browser(ctl->context);
        return;
    }
    case TUBE_REMOTE_QUEUE:
        if (refill.owner == c.owner && !strcmp(c.playlist,refill.playlist) &&
            !strcmp(c.ctt,refill.ctt) && !strcmp(c.video,refill.video) &&
            c.index == refill.indices[refill.cursor]) {
            PhoneQueue q = playlist(&c,refill.video);
            if (q.cursor >= 0 && q.cursor + 1 < q.count &&
                q.indices[q.cursor + 1] > c.index) {
                ++q.cursor; memcpy(q.video,q.videos[q.cursor],12);
                int64_t delay = refill_not_before;
                memset(&refill,0,sizeof refill);
                begin(&c,&q,0); interruptible_launch = 1;
                if (now_ms < delay) { phase = 4; retry_deadline = delay; }
                if (ctl->browser) ctl->browser(ctl->context);
                return;
            }
            /* A matching window without a successor is a terminal response. */
            clear_queue();
        }
        if (active && current.owner == c.owner && current.selection == p.selection &&
            !strcmp(current.playlist,c.playlist) && !strcmp(current.ctt,c.ctt)) {
            current = playlist(&c, p.video); current.selection = p.selection;
        }
        applied = 1; break;
    case TUBE_REMOTE_NEXT: case TUBE_REMOTE_PREVIOUS:
        if (active && !strcmp(p.video, current.video) && advance(&c, c.action == TUBE_REMOTE_NEXT ? 1 : -1)) {
            if (ctl->browser) ctl->browser(ctl->context);
            return;
        }
        /* At a queue boundary, report no next item without reopening playback. */
        applied = 1; break;
    case TUBE_REMOTE_PLAY: case TUBE_REMOTE_PAUSE:
        applied = !active || (ctl->pause && ctl->pause(ctl->context, c.action == TUBE_REMOTE_PAUSE)); break;
    case TUBE_REMOTE_SEEK:
        applied = active && ctl->seek && ctl->seek(ctl->context, c.position); break;
    case TUBE_REMOTE_STOP:
        applied = !active || (ctl->stop && ctl->stop(ctl->context));
        if (applied) { memset(&current, 0, sizeof current); remote_video[0] = 0; }
        break;
    case TUBE_REMOTE_VOLUME: {
        double v = c.has_volume ? c.volume : p.volume + c.delta;
        if (v < 0) v = 0;
        if (v > 100) v = 100;
        applied = ctl->volume && ctl->volume(ctl->context, (int)lround(v)); break;
    }
    case TUBE_REMOTE_DPAD: applied = ctl->key && ctl->key(ctl->context, c.key); break;
    case TUBE_REMOTE_VOICE:
        applied = ctl->voice && ctl->voice(ctl->context, !strcmp(c.voice, "INITIATED"), c.owner); break;
    case TUBE_REMOTE_CAPTIONS:
        if (active && (!*c.video || !strcmp(c.video, p.video)) && ctl->captions)
            applied = ctl->captions(ctl->context, c.language, c.owner, c.sequence);
        if (applied == 2) {
            pending = c; memcpy(pending.video, p.video, sizeof pending.video); caption_selection = p.selection; phase = 3; ticket = 0; return;
        }
        applied = applied == 1; break;
    case TUBE_REMOTE_DISCONNECTED:
        applied = 1;
        if (ctl->close_on_disconnect) {
            applied = ctl->close_app && ctl->close_app(ctl->context);
            if (applied) { memset(&current, 0, sizeof current); remote_video[0] = 0; }
        } else if (ctl->stop_on_disconnect && active && current.owner == c.owner && !strcmp(p.video, current.video)) {
            applied = ctl->stop && ctl->stop(ctl->context);
            if (applied) { memset(&current, 0, sizeof current); remote_video[0] = 0; }
        }
        break;
    case TUBE_REMOTE_QUERY: case TUBE_REMOTE_CONNECTED: case TUBE_REMOTE_DEVICES:
        applied = 1; break;
    default: break;
    }
    tube_remote_finish(c.owner, c.sequence, applied);
}
