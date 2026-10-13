/* Host program: `tube_js serve ...` runs the embedded companion until a
 * signal; any other command runs once and prints JSON.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_service.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void on_signal(int number) { (void)number; stopping = 1; }

static int serve(tube_service_options *options, int argc, char **argv) {
  const char *state_dir = NULL;
  for (int i = 0; i < argc; i++) {
    const char *value = i + 1 < argc ? argv[i + 1] : NULL;
    if (!strcmp(argv[i], "--sponsorblock")) { options->sponsorblock = 1; continue; }
    if (!strcmp(argv[i], "--return-youtube-dislike")) { options->return_youtube_dislike = 1; continue; }
    if (!strcmp(argv[i], "--ranged-manifest")) { options->ranged_manifest = 1; continue; }
    if (!strcmp(argv[i], "--account-playback")) { options->account_playback = 1; continue; }
    if (!value) { fprintf(stderr, "tube_js: %s needs a value\n", argv[i]); return 2; }
    if (!strcmp(argv[i], "--host")) {
      if (strcmp(value, "127.0.0.1")) { fprintf(stderr, "tube_js: the service only listens on 127.0.0.1\n"); return 2; }
    } else if (!strcmp(argv[i], "--port")) options->port = (unsigned short)atoi(value);
    else if (!strcmp(argv[i], "--state-dir")) state_dir = value;
    else if (!strcmp(argv[i], "--token")) options->token = value;
    else if (!strcmp(argv[i], "--sponsorblock-server")) options->sponsorblock_server = value;
    else if (!strcmp(argv[i], "--pairing-code")) options->pairing_code = value;
    else if (!strcmp(argv[i], "--lounge-base")) {   /* tests: a local stand-in for YouTube's phone connection service */
      if (strncmp(value, "http://127.0.0.1:", 17)) { fprintf(stderr, "tube_js: --lounge-base must be on http://127.0.0.1\n"); return 2; }
      options->lounge_base = value;
    }
    else { fprintf(stderr, "tube_js: unknown option %s\n", argv[i]); return 2; }
    i++;
  }
  struct sigaction action;
  memset(&action, 0, sizeof action);
  action.sa_handler = on_signal;
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  unsigned short port = 0;
  char token[65];
  tube_service_configure(options);
  if (tube_service_start(state_dir, &port, token)) { fprintf(stderr, "tube_js: %s\n", tube_service_error()); return 1; }
  /* The token goes to a private file for the tools that need it, never to the output. */
  if (!options->token) {
    char path[1024];
    snprintf(path, sizeof path, "%s/token", state_dir);
    FILE *file = fopen(path, "w");
    if (file) { chmod(path, 0600); fprintf(file, "%s\n", token); fclose(file); }
  }
  printf("{\"listening\":\"http://127.0.0.1:%u\"}\n", (unsigned)port);
  fflush(stdout);
  while (!stopping) usleep(100000);
  tube_service_stop();
  return 0;
}

int main(int argc, char **argv) {
  char bundle[1024];
  const char *configured = getenv("TUBE_JS_BUNDLE");
  if (configured) snprintf(bundle, sizeof bundle, "%s", configured);
  else {   /* default: the bundle next to the executable */
    char self[900];
    ssize_t length = readlink("/proc/self/exe", self, sizeof self - 1);
    if (length < 0) length = 0;
    self[length] = 0;
    char *slash = strrchr(self, '/');
    if (slash) *slash = 0;
    snprintf(bundle, sizeof bundle, "%s/tube_js.bundle.js", slash ? self : ".");
  }
  tube_service_options options;
  memset(&options, 0, sizeof options);
  options.bundle_path = bundle;
  options.ca_file = getenv("TUBE_JS_CA_BUNDLE");
  options.allow_player_script = getenv("TUBE_JS_PLAYER_SCRIPT") != NULL;
  if (argc > 1 && !strcmp(argv[1], "serve")) return serve(&options, argc - 2, argv + 2);
  return tube_service_command(&options, argc - 1, argv + 1);
}
