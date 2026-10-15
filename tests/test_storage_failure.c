/* Guest saves preserve the old file and report one actionable notice per failure. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include <sys/stat.h>

static char *contents(const char *path) {
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n >= 0); rewind(f);
    char *body = calloc(1, (size_t)n + 1); assert(body);
    assert(fread(body, 1, (size_t)n, f) == (size_t)n && !fclose(f));
    return body;
}
static void unchanged(const char *path, const char *expected) {
    char *actual = contents(path); assert(!strcmp(actual, expected)); free(actual);
}
static void notice(void) {
    const char *message = tube_take_storage_notice();
    assert(message && strstr(message, "Could not save local changes") && strstr(message, "may be lost"));
    assert(!strcmp(message, error_text) && !tube_take_storage_notice());
}
int main(void) {
    char dir[] = "/tmp/tube-storage-failure-XXXXXX", tmp[512], dst[512], backup[512];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    const evo_provider_t *p = &evo_provider_youtube;
    assert(!p->init() && tube_set_playback_speed(100) && !tube_take_storage_notice());
    snprintf(tmp, sizeof tmp, "%s/youtube.json.tmp", dir);
    snprintf(dst, sizeof dst, "%s/youtube.json", dir);
    snprintf(backup, sizeof backup, "%s/original.json", dir);
    char *before = contents(dst);
    /* The temporary file cannot open: settings keep working in memory, but
       the user is warned and the previous durable state remains intact. */
    assert(!mkdir(tmp, 0700)); error_text[0] = 0;
    assert(tube_set_playback_speed(125) && tube_playback_speed() == 125);
    notice(); unchanged(dst, before);
    assert(tube_set_playback_speed(150) && !tube_take_storage_notice());
    unchanged(dst, before);
    assert(!rmdir(tmp) && tube_set_playback_speed(150) && !tube_take_storage_notice());
    assert(!p->init() && tube_playback_speed() == 150);
    free(before); before = contents(dst);
    /* Opening succeeds but flushing fails (ENOSPC). */
    assert(!symlink("/dev/full", tmp)); error_text[0] = 0;
    assert(tube_set_playback_speed(175)); notice(); unchanged(dst, before);
    assert(!unlink(tmp) && tube_set_playback_speed(150) && !tube_take_storage_notice());
    free(before); before = contents(dst);
    /* A failed final rename also retains all prior bytes. */
    assert(!rename(dst, backup) && !mkdir(dst, 0700)); error_text[0] = 0;
    assert(tube_set_playback_speed(200)); notice(); unchanged(backup, before);
    assert(!rmdir(dst) && !rename(backup, dst));
    assert(tube_set_playback_speed(200) && !tube_take_storage_notice());
    assert(!p->init() && tube_playback_speed() == 200);
    p->shutdown(); free(before);
    assert(!unlink(dst) && !rmdir(dir));
    puts("PASS: Guest storage failure notice, once per episode, old bytes preserved, open/flush/rename failure and successful retry");
}
