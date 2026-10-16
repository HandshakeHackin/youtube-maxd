/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "tube_update.h"
#include "self_update_sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static uint64_t now_ms(void *u) { (void)u; struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC,&t)); return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000; }
static int offered;
static pthread_mutex_t fetch_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fetch_gate = PTHREAD_COND_INITIALIZER;
static int hold_fetch, fetch_entered;
static char app[2048], manifest[1024];
static int fetch(void *user, const char *url, char *body, size_t cap, size_t *len, int *code) {
    (void)user;
    pthread_mutex_lock(&fetch_lock);
    if (hold_fetch) {
        fetch_entered = 1;
        pthread_cond_signal(&fetch_gate);
        while (hold_fetch) pthread_cond_wait(&fetch_gate, &fetch_lock);
    }
    pthread_mutex_unlock(&fetch_lock);
    assert(strstr(url, "homebrew.page/api/v1/") || strstr(url,"blackbearreloaded.github.io/ps5-homebrew-catalog/api/v1/"));
    assert(!strstr(url, "api.github.com"));
    if (!offered) { *len = 0; *code = 404; return 0; }
    const char *data = strstr(url,"manifest.json") ? manifest : app;
    if (strstr(url,"manifest.sig")) { assert(cap>=64); memset(body,'S',64); *len=64; }
    else { *len=strlen(data); assert(*len<=cap); memcpy(body,data,*len); }
    *code=200; return 0;
}
static int verify(void *u, const unsigned char k[32], const unsigned char s[64], const void *m, size_t n) {
    (void)u; (void)k; (void)m; (void)n; return s[0]=='S';
}
static int open_helper(void *u, self_update_channel *c) { (void)u; (void)c; return 0; }
static int load_sequence(void *u, uint64_t *s) { (void)u; *s=0; return 1; }
static int save_sequence(void *u, uint64_t s) { (void)u; assert(s==1); return 1; }
static void wait_check(tube_update *u) {
    tube_update_status s;
    for (int n=0; n<500; ++n) {
        tube_update_get(u,&s);
        if (!s.busy) return;
        struct timespec pause = {0,1000000}; nanosleep(&pause,NULL);
    }
    assert(!"checker did not end");
}
int main(void) {
    char path[] = "/tmp/pt-update-adapter-XXXXXX";
    assert(mkdtemp(path));
    self_update_platform platform = {.fetch=fetch,.verify=verify,.open_helper=open_helper,
        .load_sequence=load_sequence,.save_sequence=save_sequence,.now_ms=now_ms};
    tube_update_config config = {.data_dir=path,.installed="01.000.000",.platform=&platform};
    tube_update_config invalid = config; invalid.installed = NULL;
    assert(!tube_update_new(&invalid));
    tube_update *u = tube_update_new(&config); assert(u);
    tube_update_app_use(u);
    tube_update_app_tick(1); wait_check(u); tube_update_app_tick(1);
    assert(!tube_update_app_take_notice()); /* automatic network/unlisted failures stay quiet */
    assert(tube_update_app_action("check")); wait_check(u); tube_update_app_tick(1);
    assert(strstr(tube_update_app_take_notice(),"not listed"));
    assert(!tube_update_download(u) && !tube_update_apply(u));
    tube_update_row rows[12];
    assert(tube_update_app_rows(rows,12)==2);
    assert(!strcmp(rows[0].id,"settings:update:check"));
    assert(tube_update_app_action("notify"));
    tube_update_skip(u,"01.000.010");
    snprintf(app,sizeof app,"{\"schema\":3,\"titleid\":\"PPSA99142\",\"name\":\"ProsperoTube\","
        "\"version\":\"0.1.1\",\"artifact_url\":\"https://github.com/example/app/releases/download/v1/PPSA99142.zip\","
        "\"sha256\":\"%064d\",\"status\":\"available\",\"content_version\":\"01.000.010\","
        "\"format\":\"zip\",\"size\":100,\"release_notes\":\"A signed note\",\"release_notes_truncated\":true}",0);
    self_update_sha256 hash; unsigned char bytes[32]; char digest[65];
    self_update_sha256_start(&hash); self_update_sha256_add(&hash,app,strlen(app));
    self_update_sha256_finish(&hash,bytes); self_update_sha256_hex(bytes,digest);
    snprintf(manifest,sizeof manifest,"{\"schema\":3,\"sequence\":1,\"files\":{\"apps/PPSA99142.json\":\"%s\"}}",digest);
    offered=1;
    tube_update_set_notify(u,1);
    assert(tube_update_app_action("check")); wait_check(u); tube_update_app_tick(1);
    assert(!tube_update_app_take_notice()); /* skipped offer */
    assert(tube_update_app_rows(rows,12)==5);
    assert(!strcmp(tube_update_app_notes(),"A signed note"));
    assert(tube_update_app_action("skip")); tube_update_app_tick(1);
    tube_update_status offer;
    assert(tube_update_app_offer(&offer) && !strcmp(offer.latest, "01.000.010"));
    assert(tube_update_app_offer(&offer)); /* retained until an interface is ready */
    tube_update_app_dismiss_offer();
    assert(!tube_update_app_offer(&offer));
    tube_update_app_tick(1); assert(!tube_update_app_take_notice());
    assert(tube_update_notes_truncated(u));
    const char *retained = tube_update_notes(u);
    pthread_mutex_lock(&fetch_lock); hold_fetch = 1; fetch_entered = 0; pthread_mutex_unlock(&fetch_lock);
    assert(tube_update_app_action("check"));
    pthread_mutex_lock(&fetch_lock);
    while (!fetch_entered) pthread_cond_wait(&fetch_gate, &fetch_lock);
    pthread_mutex_unlock(&fetch_lock);
    assert(!strcmp(retained, "A signed note"));
    for (int n=0; n<10000; ++n) {
        assert(!*tube_update_notes(u));
        assert(!tube_update_notes_truncated(u));
    }
    retained = tube_update_notes(u);
    pthread_mutex_lock(&fetch_lock); hold_fetch = 0; pthread_cond_signal(&fetch_gate); pthread_mutex_unlock(&fetch_lock);
    wait_check(u);
    assert(!*retained); /* the completed worker never writes the reader's snapshot */
    assert(!strcmp(tube_update_notes(u), "A signed note") && tube_update_notes_truncated(u));
    tube_update_app_tick(1);
    assert(!tube_update_app_take_notice());
    assert(tube_update_app_action("install")); wait_check(u); tube_update_app_tick(1);
    assert(strstr(tube_update_app_take_notice(),"payload loader"));
    assert(!tube_update_app_take_restart());
    tube_update_set_notify(u,0); tube_update_skip(u,"01.000.010");
    tube_update_app_shutdown(); tube_update_free(u);
    u=tube_update_new(&config); assert(u);
    tube_update_status s; tube_update_get(u,&s);
    assert(!s.notify && !strcmp(s.skipped,"01.000.010"));
    tube_update_skip(u,"../../bad"); tube_update_get(u,&s); assert(!strcmp(s.skipped,"01.000.010"));
    tube_update_free(u);
    config.development=1; u=tube_update_new(&config); assert(u);
    tube_update_get(u,&s); assert(s.blocked && strstr(s.blocked_text,"development"));
    tube_update_free(u);
    char settings[512]; snprintf(settings,sizeof settings,"%s/update-preferences",path);
    assert(!unlink(settings)); assert(!rmdir(path));
    puts("PASS: catalog adapter manual/startup checks, no unknown install, settings persistence, development refusal");
    return 0;
}
