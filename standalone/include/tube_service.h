/* The embedded companion: an HTTP service on 127.0.0.1 that answers the API of
 * companion/prosperotube/app.py from inside the app (QuickJS engine thread +
 * loopback listener thread). The provider is pointed at
 * http://127.0.0.1:<port> with the returned bearer token.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TUBE_SERVICE_H
#define TUBE_SERVICE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Starts the loopback service (127.0.0.1 only) on its own thread(s). state_dir is the app data
 * directory. On success returns 0 and fills port (host order) and a NUL-terminated bearer token
 * (hex, at most 64 chars). Idempotent while running. Never blocks on the network. */
int tube_service_start(const char *state_dir, unsigned short *port, char token[65]);
/* Stops accepting, cancels requests, joins the engine thread; safe to call when not running. */
void tube_service_stop(void);
int tube_service_running(void);
/* Last start failure as a short static string for the UI/log, "" when none. */
const char *tube_service_error(void);

/* ---- Optional settings, read by the next tube_service_start() ----
 * Everything pointed at must stay valid until tube_service_stop(). A zeroed
 * structure (or never calling this) gives the console defaults: the bundle and
 * the root certificates embedded in the executable, a free port between 20000
 * and 39999, a new random token. */
typedef struct tube_service_options {
    const char *bundle_path;            /* service bundle as a file (host builds, development) */
    const void *bundle_data;            /* or in memory; wins over bundle_path */
    size_t bundle_length;
    unsigned short port;                /* 0 = pick a free one */
    const char *token;                  /* 16..64 of [A-Za-z0-9_-]; NULL = 64 random hex digits */
    const char *ca_file;                /* PEM roots; NULL = embedded roots (console) or the system's (host) */
    int sponsorblock;                   /* answer /v1/sponsor from SponsorBlock (hashed-prefix query) */
    const char *sponsorblock_server;    /* optional alternative https origin */
    const char *pairing_code;           /* six digits; NULL = /v1/pair answers "expired" (not needed) */
    int allow_player_script;            /* permit the costly player-script fallback (larger stack and heap) */
    int ranged_manifest;                /* DASH manifests with one byte-range segment per fragment (fast seeks);
                                           0 = the companion's one-segment-per-file shape */
} tube_service_options;
void tube_service_configure(const tube_service_options *options);

/* Host tool: runs one command (see standalone/README.md) on a temporary
 * engine, prints its JSON, returns the exit status. Not for the app. */
int tube_service_command(const tube_service_options *options, int argc, char **argv);

/* What the engine thread is created with. */
#define TUBE_SERVICE_THREAD_STACK ((size_t)4 << 20)
#define TUBE_SERVICE_JS_STACK ((size_t)3 << 20)
#define TUBE_SERVICE_JS_MEMORY ((size_t)128 << 20)

#ifdef __cplusplus
}
#endif
#endif
