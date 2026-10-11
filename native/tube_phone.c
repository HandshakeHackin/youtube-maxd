/* Ephemeral Lounge queue; never changes the provider's saved local queue.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_phone.h"
#include <math.h>
#include <string.h>
typedef struct {
    unsigned owner;
    unsigned selection;
    int count, cursor, base;
    char videos[128][12], video[12], playlist[101], ctt[513];
} PhoneQueue;
static PhoneQueue current, proposed;
static tube_remote_command pending;
static unsigned serial, ticket, caption_selection;
static int phase;
static double resume;
static tube_remote_player last;
static unsigned last_owner;
static char remote_video[12];
static unsigned remote_selection;
static void clear_queue(void) {
    memset(&current, 0, sizeof current); memset(&proposed, 0, sizeof proposed);
    memset(&pending, 0, sizeof pending); phase = 0; ticket = 0;
    memset(&last, 0, sizeof last); last_owner = 0;
}
void tube_phone_clear(void) {
    clear_queue(); remote_video[0] = 0; remote_selection = 0;
}
void tube_phone_cancel(void) {
    if (phase && pending.sequence) tube_remote_finish(pending.owner, pending.sequence, 0);
    clear_queue();
}
static int launch_valid(void) {
    return phase && tube_remote_owner_current(pending.owner) &&
        (!pending.sequence || tube_remote_command_current(pending.owner, pending.sequence));
}
int tube_phone_launch_current(unsigned value) { return value && value == ticket && launch_valid(); }
static void prune(void) {
    if ((phase && !launch_valid()) || (current.owner && !tube_remote_owner_current(current.owner))) clear_queue();
}
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
    const char *anchor_id = *c->video ? c->video : playing;
    if (c->index < q.count && !strcmp(q.videos[c->index], anchor_id)) anchor = c->index;
    if (!strcmp(playing, anchor_id)) q.cursor = anchor;
    q.base = anchor < 0 ? c->index : c->index >= anchor ? c->index - anchor : 0;
    return q;
}
static void begin(const tube_remote_command *c, const PhoneQueue *q, double position) {
    pending = *c; proposed = *q; resume = position; phase = 1;
    if (!++serial) ++serial;
    ticket = serial;
}
unsigned tube_phone_take(char video[12], double *position) {
    prune(); if (phase != 1 || !video || !position) return 0;
    memcpy(video, proposed.video, 12); *position = resume; phase = 2; return ticket;
}
int tube_phone_launch_finish(unsigned value, int success) {
    if (phase != 2 || !tube_phone_launch_current(value)) { prune(); return 0; }
    unsigned owner = pending.owner; int sequence = pending.sequence;
    if (success) { current = proposed; current.selection = 0; }
    phase = 0; ticket = 0;
    if (sequence) return tube_remote_finish(owner, sequence, success);
    return !!success;
}
static int advance(const tube_remote_command *c, int direction) {
    int cursor = current.cursor + direction;
    if (current.owner != c->owner || current.cursor < 0 || cursor < 0 || cursor >= current.count || current.base + cursor > 100000) return 0;
    PhoneQueue q = current; q.cursor = cursor; memcpy(q.video, q.videos[cursor], 12);
    begin(c, &q, 0); return 1;
}
int tube_phone_finished(void) {
    prune();
    if (phase || !*remote_video || strcmp(last.video, remote_video) || last.selection != remote_selection) return 0;
    if (!current.owner) { remote_video[0] = 0; return 1; }
    tube_remote_command c; memset(&c, 0, sizeof c); c.owner = current.owner;
    if (!advance(&c, 1)) { memset(&current, 0, sizeof current); remote_video[0] = 0; }
    return 1; /* Remote EOF never removes an item from the saved local queue. */
}
int tube_phone_owns_media(void) {
    prune(); return *remote_video && !strcmp(last.video, remote_video) && last.selection == remote_selection;
}
void tube_phone_tick(const tube_phone_controls *ctl) {
    if (!ctl || !ctl->snapshot) return;
    prune();
    tube_remote_player p; memset(&p, 0, sizeof p); p.volume = 100; p.state = -1;
    int active = ctl->snapshot(ctl->context, &p);
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
        p.index = current.base + (current.cursor < 0 ? 0 : current.cursor);
        if (p.index > 100000) p.index = 100000;
        p.previous = current.cursor > 0; p.next = current.cursor >= 0 && current.cursor + 1 < current.count;
    }
    last = p;
    last_owner = tube_remote_current_owner();
    tube_remote_report(&p);
    if (phase == 3) {
        int result = active && p.selection == caption_selection && !strcmp(p.video, pending.video) && ctl->captions_status ? ctl->captions_status(ctl->context) : -1;
        if (!result) return;
        unsigned owner = pending.owner; int sequence = pending.sequence;
        phase = 0; tube_remote_finish(owner, sequence, result > 0); return;
    }
    if (phase) return;
    tube_remote_command c; if (!tube_remote_take(&c)) return;
    int applied = 0;
    switch (c.action) {
    case TUBE_REMOTE_PLAYLIST: {
        PhoneQueue q = playlist(&c, c.video); begin(&c, &q, c.position);
        if (ctl->browser) ctl->browser(ctl->context);
        return;
    }
    case TUBE_REMOTE_QUEUE:
        if (active) { current = playlist(&c, p.video); current.selection = p.selection; }
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
    case TUBE_REMOTE_CAPTIONS:
        if (active && (!*c.video || !strcmp(c.video, p.video)) && ctl->captions)
            applied = ctl->captions(ctl->context, c.language, c.owner, c.sequence);
        if (applied == 2) {
            pending = c; memcpy(pending.video, p.video, sizeof pending.video); caption_selection = p.selection; phase = 3; ticket = 0; return;
        }
        applied = applied == 1; break;
    case TUBE_REMOTE_DISCONNECTED:
        applied = 1;
        if (ctl->stop_on_disconnect && active && current.owner == c.owner && !strcmp(p.video, current.video)) {
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
