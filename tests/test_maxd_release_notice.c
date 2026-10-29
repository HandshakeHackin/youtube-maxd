#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "tube_update.h"
static long long now = 100;
static long long clock_now(void) { return now; }
static char directory[256];
const char *evo_data_dir(void) { return directory; }
#define MAXD_RELEASE_TEST_TRANSPORT
/* Prototype uses an anonymous-independent type after module declarations. */
#define maxd_release_test_fetch() maxd_release_evaluate(response, "1.4.2")
static const char *response;
#include "maxd_release_notice.h"
static void settle(void) {
    assert(g_release.running);
    int ready = 0;
    for (int i = 0; i < 1000 && !ready; ++i) {
        pthread_mutex_lock(&g_release.mutex);
        ready = g_release.ready;
        pthread_mutex_unlock(&g_release.mutex);
        if (!ready) usleep(1000);
    }
    assert(ready);
    maxd_release_tick(1);
    assert(!g_release.running);
}
int main(void) {
    snprintf(directory, sizeof directory, "/tmp/maxd-release-XXXXXX"); assert(mkdtemp(directory));
    unsigned v[3]; assert(maxd_release_version("v12.34.567",v));
    const char *bad[] = {"1.2", "1.2.3-preview.1", "1.2.3x", "01.2.3", "1.2.3.4", "4294967296.1.1", "", NULL};
    for (int i=0;bad[i];++i) assert(!maxd_release_version(bad[i],v));
    const char *newer = "{\"tag_name\":\"v1.10.0\",\"draft\":false,\"prerelease\":false}";
    assert(maxd_release_evaluate(newer,"1.9.99").newer);
    assert(!maxd_release_evaluate(newer,"2.0.0").newer);
    assert(!maxd_release_evaluate(newer,"1.10.0").newer);
    assert(!maxd_release_evaluate("{}","1.4.2").ok);
    assert(!maxd_release_evaluate("[]","1.4.2").ok);
    assert(!maxd_release_evaluate("{\"tag_name\":\"v9.0.0\",\"draft\":false,\"prerelease\":true}","1.4.2").ok);
    assert(!maxd_release_evaluate("{\"tag_name\":\"v9.0.0\",\"draft\":true,\"prerelease\":false}","1.4.2").ok);
    assert(!maxd_release_evaluate("{\"tag_name\":\"v9.0.0\",\"draft\":false,\"prerelease\":false}junk","1.4.2").ok);
    maxd_release_init(); assert(g_release.enabled);
    tube_update_row rows[3]; assert(maxd_release_rows(rows,2)==2); assert(maxd_release_rows(rows,3)==3);
    assert(!strcmp(rows[2].id,"link:maxd-release"));
    response = newer;
    maxd_release_tick(0); assert(!g_release.running);
    maxd_release_tick(1); assert(g_release.running);
    assert(!maxd_release_start(1)); settle();
    assert(strstr(g_release.notice,"v1.10.0")); g_release.notice[0]=0;
    now += 60; maxd_release_tick(1); assert(!g_release.running);
    now += 21600; maxd_release_tick(1); settle(); assert(!*g_release.notice);
    maxd_release_action("check"); settle(); assert(*g_release.notice);g_release.notice[0]=0;
    assert(maxd_release_action("notify")); assert(!g_release.enabled);
    now += 21600; maxd_release_tick(1); assert(!g_release.running);
    maxd_release_init(); assert(!g_release.enabled); /* survives restart */
    response="{}";maxd_release_action("check"); settle(); assert(strstr(g_release.notice,"Could not check"));
    g_release.notice[0]=0; maxd_release_action("notify"); maxd_release_tick(1); settle(); assert(!*g_release.notice);
    response = newer; maxd_release_action("check");
    maxd_release_action("notify"); settle(); assert(*g_release.notice); /* manual remains explicit */
    g_release.notice[0]=0;
    maxd_release_action("notify"); maxd_release_tick(1); maxd_release_action("notify"); settle(); assert(!*g_release.notice);
    remove(g_release.path); assert(!mkdir(g_release.path,0700));
    int before=g_release.enabled;maxd_release_action("notify");assert(g_release.enabled==before);assert(strstr(g_release.notice,"Could not save"));
    rmdir(g_release.path);maxd_release_shutdown();rmdir(directory);
    puts("PASS: release versions, malformed/preview/draft rejection, asynchronous single-flight checks, notice deduplication, manual checks, opt-out persistence and write failure");
}
