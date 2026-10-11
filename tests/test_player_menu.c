/* In-player video actions on the shipping provider: the actions menu and its
 * lists for the playing video, launches and browser targets, the browser's
 * paging state, quality presets, reload and the error fixer. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_player_extra.h"
static const evo_provider_t *p;
static evo_stream_choice_t offered[4], playing;
static int offered_count;
static void streams_cb(int ok, const evo_stream_choice_t *choices, int count, void *ud) {
    (void)ud; last_ok = ok; offered_count = ok ? count : 0;
    for (int i = 0; ok && i < count && i < 4; ++i) offered[i] = choices[i];
}
static int row_of(const char *title) {
    for (int i = 0; i < tube_player_menu_count(); ++i) if (!strcmp(tube_player_menu_row(i)->title, title)) return i;
    return -1;
}
static const char *listed = "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":["
    "{\"id\":\"abcdefghijk\",\"title\":\"First\",\"subtitle\":\"Fixture channel\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"duration_sec\":100,\"playback_context\":{\"playlist\":\"PLfixture01\",\"index\":0}},"
    "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"duration_sec\":50}],\"has_more\":false}}";
/* What the host does with a video: resolve it, then start the offered stream. */
static void resolve_to(const char *height, const char *fps, const char *codec, const char *expect) {
    assert(!p->resolve("abcdefghijk", streams_cb, NULL));
    if (expect) assert(strstr(pending[0].body, expect));
    char body[640];
    snprintf(body, sizeof body, "{\"state\":\"done\",\"result\":{\"id\":\"abcdefghijk\",\"title\":\"First\",\"duration_sec\":100,\"choices\":["
        "{\"url\":\"http://127.0.0.1:8765/media/capability/p%s\",\"container\":\"dash\",\"width\":1280,\"height\":%s,\"fps\":%s,"
        "\"video_codec\":\"%s\",\"audio_codec\":\"aac\",\"audio_channels\":2}]}}", height, height, fps, codec);
    poll_job(body); assert(last_ok && offered_count == 1);
}
static void start(void) { playing = offered[0]; p->stream_chosen("abcdefghijk", &playing); }
int main(void) {
    char dir[] = "/tmp/tube-player-menu-XXXXXX", label[160], detail[160], parent[160], title[192];
    evo_provider_item_t item; double position; int subtitles;
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    p = &evo_provider_youtube; assert(!p->init());
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    p->list_catalog("playlist:PLfixture01", 0, items_cb, NULL);
    reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}"); poll_job(listed);
    assert(last_ok && last_count == 2);
    /* The browser was on a list with a further page when playback started. */
    p->list_catalog("library:suggestions:bcdefghijkl", 0, items_cb, NULL);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"cdefghijklm\",\"title\":\"Browser row\"}],\"has_more\":true,\"cursor\":\"browser-next\"}}");
    assert(last_ok && last_more);

    /* Nothing is offered without a playing video. */
    assert(!tube_player_action(TUBE_PLAYER_ACTIONS, label, sizeof label, detail, sizeof detail) && !*label);
    assert(tube_player_menu_begin(TUBE_PLAYER_ACTIONS) == TUBE_MENU_NOTHING && tube_player_menu_status() == -1);
    assert(!tube_player_reload(10, 0, 1) && !tube_error_fix_begin(TUBE_ERROR_BUFFERING, 10, 0, 1));
    resolve_to("1080", "60", "vp9", "\"max_height\":1080"); start();

    /* Direct actions are the rows the actions menu offers for this video and profile. */
    assert(tube_player_action(TUBE_PLAYER_ACTIONS, label, sizeof label, detail, sizeof detail) && !strcmp(label, "Video actions"));
    assert(!tube_player_action(TUBE_PLAYER_LIKE, label, sizeof label, NULL, 0) && !tube_player_action(TUBE_PLAYER_SUBSCRIBE, NULL, 0, NULL, 0));
    assert(!tube_player_action(TUBE_PLAYER_PLAYLIST_ADD, NULL, 0, NULL, 0) && !tube_player_action(TUBE_PLAYER_ACTION_COUNT, NULL, 0, NULL, 0));
    assert(tube_player_action(TUBE_PLAYER_CHANNEL, label, sizeof label, detail, sizeof detail) && !strcmp(label, "Open channel") && !strcmp(detail, "Fixture channel"));
    assert(tube_player_action(TUBE_PLAYER_SHARE, label, sizeof label, NULL, 0) && !strcmp(label, "Share link"));
    assert(tube_player_action(TUBE_PLAYER_DESCRIPTION, label, sizeof label, NULL, 0) && !strcmp(label, "Open description"));
    assert(tube_player_action(TUBE_PLAYER_SUGGESTIONS, label, sizeof label, NULL, 0) && !strcmp(label, "Related videos"));
    assert(tube_player_action(TUBE_PLAYER_SPONSOR, label, sizeof label, NULL, 0) && !strcmp(label, "Exclude channel from SponsorBlock"));
    assert(tube_player_action(TUBE_PLAYER_PLAYLIST, label, sizeof label, NULL, 0) && !strcmp(label, "Open playlist"));
    assert(tube_player_action(TUBE_PLAYER_QUEUE, label, sizeof label, NULL, 0) && !strcmp(label, "Playback queue"));

    /* The menu itself, without the rows that mean nothing while their video plays. */
    assert(tube_player_menu_begin(TUBE_PLAYER_ACTIONS) == TUBE_MENU_OPENED && tube_player_menu_status() == 1 && tube_player_menu_root());
    assert(!pending_count && !strcmp(tube_player_menu_title(), "Video actions"));
    assert(row_of("Play next") < 0 && row_of("Play from start") < 0 && row_of("Play incognito") < 0 && row_of("Refresh section") < 0);
    assert(row_of("Mark as watched locally") >= 0 && row_of("Related videos") >= 0 && row_of("Choose an account profile first") >= 0);
    assert(tube_player_menu_open(row_of("Choose an account profile first")) == TUBE_MENU_NOTHING && tube_player_menu_root());
    assert(tube_player_menu_open(-1) == TUBE_MENU_NOTHING && tube_player_menu_open(tube_player_menu_count()) == TUBE_MENU_NOTHING);
    assert(!tube_player_menu_row(tube_player_menu_count()));
    /* A row opens its page in place: the links are inert rows that show their address. */
    assert(tube_player_menu_open(row_of("Share link")) == TUBE_MENU_OPENED && tube_player_menu_status() == 1 && !tube_player_menu_root());
    assert(tube_player_menu_count() == 5 && !strcmp(tube_player_menu_row(0)->title, "https://youtu.be/abcdefghijk"));
    assert(!strcmp(tube_player_menu_row(0)->detail, "Video link") && !strcmp(tube_player_menu_title(), "Share link"));
    assert(!strcmp(tube_player_menu_row(1)->title, "https://www.youtube.com/embed/abcdefghijk") && !strcmp(tube_player_menu_row(1)->detail, "Embed link"));
    assert(!strcmp(tube_player_menu_row(2)->title, "https://www.youtube.com/channel/UCaaaaaaaaaaaaaaaaaaaaaa"));
    assert(!strcmp(tube_player_menu_row(3)->title, "https://www.youtube.com/playlist?list=PLfixture01"));
    for (int i = 0; i < 4; ++i) assert(tube_player_menu_open(i) == TUBE_MENU_NOTHING && tube_player_menu_count() == 5);
    /* The QR codes are on the browser's link page: the last row leaves for it. */
    assert(!strcmp(tube_player_menu_row(4)->title, "Show QR codes") && tube_player_menu_open(4) == TUBE_MENU_LEAVE);
    assert(tube_player_take_browse(parent, sizeof parent, title, sizeof title) == 1);
    assert(!strcmp(parent, "link:video:abcdefghijk") && !strcmp(title, "Share link"));
    /* The direct action opens the same list. */
    assert(tube_player_menu_begin(TUBE_PLAYER_SHARE) == TUBE_MENU_OPENED && tube_player_menu_status() == 1 && !tube_player_menu_root());
    assert(tube_player_menu_count() == 5 && !strcmp(tube_player_menu_row(4)->id, "link:video:abcdefghijk"));
    /* SponsorBlock for this channel acts at once and the row names the way back. */
    assert(tube_player_menu_begin(TUBE_PLAYER_SPONSOR) == TUBE_MENU_OPENED && tube_player_menu_status() == 1);
    assert(!strcmp(tube_player_menu_row(0)->title, "SponsorBlock is off for this channel"));
    assert(tube_player_action(TUBE_PLAYER_SPONSOR, label, sizeof label, NULL, 0) && !strcmp(label, "Use SponsorBlock on this channel"));
    /* The queue is listed with its videos. */
    assert(tube_toggle_queue("bcdefghijkl") == 1);
    assert(tube_player_menu_begin(TUBE_PLAYER_QUEUE) == TUBE_MENU_OPENED && tube_player_menu_status() == 1 && tube_player_menu_count() == 1);
    assert(!strcmp(tube_player_menu_row(0)->id, "bcdefghijkl") && !tube_player_menu_row(0)->folder);

    /* Related videos come from the companion while the video keeps playing:
     * the request is not preceded by the browser's cancellation. */
    assert(tube_recovery_choice(&playing, 1) == -1 && tube_queue_recovery(5, 0, 1) && tube_recovery_pending());
    assert(tube_player_menu_begin(TUBE_PLAYER_SUGGESTIONS) == TUBE_MENU_OPENED && tube_player_menu_status() == 0 && !tube_player_menu_count());
    assert(tube_recovery_pending());                    /* tube_cancel() would have ended it */
    tube_recovery_finish();
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"suggestions\"") && strstr(pending[0].body, "\"id\":\"abcdefghijk\""));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":["
        "{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"subtitle\":\"Other channel\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}},"
        "{\"id\":\"library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"From this channel\",\"is_folder\":true}],"
        "\"has_more\":true,\"cursor\":\"related-next\"}}");
    assert(tube_player_menu_status() == 1 && tube_player_menu_count() == 3 && !strcmp(tube_player_menu_title(), "Related videos"));
    const tube_player_menu_row_t *row = tube_player_menu_row(0);
    assert(!row->folder && !strcmp(row->id, "bcdefghijkl") && !strcmp(row->detail, "Other channel") && row->scope);
    assert(!strcmp(row->playlist, "group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") && tube_player_menu_row(1)->folder);
    assert(!strcmp(tube_player_menu_row(2)->title, "More results"));
    /* The next page joins the list in place of its row. */
    assert(tube_player_menu_open(2) == TUBE_MENU_OPENED && tube_player_menu_status() == 0);
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"continue\"") && strstr(pending[0].body, "related-next"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"defghijklmn\",\"title\":\"Third\"}],\"has_more\":false}}");
    assert(tube_player_menu_status() == 1 && tube_player_menu_count() == 3 && !strcmp(tube_player_menu_row(2)->id, "defghijklmn"));
    assert(!strcmp(tube_player_menu_row(0)->id, "bcdefghijkl"));
    /* A chip opens its own list; a newer list retires the request of an older one. */
    assert(tube_player_menu_open(1) == TUBE_MENU_OPENED && tube_player_menu_status() == 0 && pending_count == 1);
    assert(strstr(pending[0].body, "\"action\":\"suggestions_filter\""));
    assert(tube_player_menu_begin(TUBE_PLAYER_DESCRIPTION) == TUBE_MENU_OPENED && tube_player_menu_status() == 0 && pending_count == 2);
    reply(0, 1, 200, "{\"job\":\"stale_job\",\"state\":\"pending\"}");
    assert(tube_player_menu_status() == 0);             /* the stale answer changes nothing */
    for (int i = 0; i < pending_count; ) {             /* its job is given up on the companion */
        if (!strcmp(pending[i].method, "DELETE")) reply(i, 1, 200, "{}"); else ++i;
    }
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"description\""));
    poll_job("{\"state\":\"done\",\"result\":{\"api_version\":1,\"kind\":\"description\",\"id\":\"abcdefghijk\",\"title\":\"First\",\"channel\":\"Fixture channel\","
        "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\",\"published\":\"\",\"views\":\"12\",\"truncated\":false,\"description\":\""
        "First line\\nA second line that is long enough to be broken at a space because it does not fit into one row of the list, "
        "which holds a little over one hundred bytes.\\n\\n\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9"
        "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9"
        "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\"}}");
    assert(tube_player_menu_status() == 1 && !strcmp(tube_player_menu_row(0)->title, "First") && !strcmp(tube_player_menu_row(0)->detail, "Description"));
    int first = row_of("First line");
    assert(first > 0 && !strncmp(tube_player_menu_row(first + 1)->title, "A second line", 13));
    assert(!strncmp(tube_player_menu_row(first + 2)->title, "which holds", 11) || strstr(tube_player_menu_row(first + 2)->title, "hundred bytes."));
    int accented = 0;
    for (int i = first; i < tube_player_menu_count(); ++i) {
        const char *line = tube_player_menu_row(i)->title;
        assert(strlen(line) < 104 && tube_player_menu_open(i) == TUBE_MENU_NOTHING);
        for (const char *c = line; *c; ++c) accented += (unsigned char)*c == 0xa9;
        /* No line starts or ends inside a character. */
        assert(((unsigned char)line[0] & 0xc0) != 0x80 && (!*line || (unsigned char)line[strlen(line) - 1] != 0xc3));
    }
    assert(accented == 60);

    /* Choosing a video ends playback and hands it to the host with its group. */
    assert(tube_player_menu_begin(TUBE_PLAYER_SUGGESTIONS) == TUBE_MENU_OPENED);
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"bcdefghijkl\",\"title\":\"Second\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":0}}],\"has_more\":false}}");
    assert(tube_player_menu_status() == 1 && !tube_player_launch_pending());
    assert(tube_player_menu_open(0) == TUBE_MENU_LEAVE && tube_player_launch_pending());
    tube_player_menu_close();
    assert(tube_player_menu_status() == -1 && !tube_player_menu_count() && !tube_player_menu_root());
    memset(&item, 0, sizeof item);
    assert(tube_player_take_launch(&item) == 1 && !strcmp(item.id, "bcdefghijkl") && !strcmp(item.title, "Second") && item.playback_scope);
    assert(!strcmp(item.playback_playlist, "group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee") && !item.is_folder);
    assert(!tube_player_take_launch(&item) && !tube_player_launch_pending());
    /* The browser's list continues where it was. */
    p->list_catalog("library:suggestions:bcdefghijkl", 1, items_cb, NULL);
    assert(pending_count == 1 && strstr(pending[0].body, "\"action\":\"continue\"") && strstr(pending[0].body, "browser-next"));
    poll_job("{\"state\":\"done\",\"result\":{\"items\":[],\"has_more\":false}}"); assert(last_ok);
    /* The catalog call just made ended playback requests as the browser does; play again. */
    resolve_to("1080", "60", "vp9", NULL); start();
    /* Open channel and Search leave for the browser with their target. */
    assert(tube_player_menu_begin(TUBE_PLAYER_CHANNEL) == TUBE_MENU_LEAVE);
    assert(tube_player_take_browse(parent, sizeof parent, title, sizeof title) == 1);
    assert(!strcmp(parent, "channel:UCaaaaaaaaaaaaaaaaaaaaaa") && !strcmp(title, "Fixture channel"));
    assert(!tube_player_take_browse(parent, sizeof parent, title, sizeof title) && !*parent);
    tube_player_request_search();
    assert(tube_player_take_browse(parent, sizeof parent, title, sizeof title) == 2 && !*parent);
    /* A launch chosen under another profile is never started. */
    assert(tube_player_menu_begin(TUBE_PLAYER_QUEUE) == TUBE_MENU_OPENED && tube_player_menu_open(0) == TUBE_MENU_LEAVE);
    tube_player_menu_close();
    p->list_catalog("library:profile:another", 0, items_cb, NULL);
    assert(!tube_player_take_launch(&item));
    /* An account profile's menu spans two pages of the browser: the player lists it
     * whole, and the direct actions find the rows of its second page. */
    p->list_catalog("playlist:PLfixture01", 0, items_cb, NULL); poll_job(listed); assert(last_ok && last_count == 2);
    p->list_catalog("account:menu:abcdefghijk", 0, items_cb, NULL);
    assert(last_ok && last_count == 24 && last_more && !strcmp(last_items[23].title, "Share link"));
    p->list_catalog("account:menu:abcdefghijk", 1, items_cb, NULL);
    assert(last_ok && last_count == 2 && !last_more && strstr(last_items[0].id, ":sbinclude:") && !strcmp(last_items[1].id, "settings:refresh"));
    resolve_to("1080", "60", "vp9", NULL); start();
    assert(tube_player_menu_begin(TUBE_PLAYER_ACTIONS) == TUBE_MENU_OPENED && tube_player_menu_status() == 1 && tube_player_menu_root());
    assert(!pending_count && tube_player_menu_count() == 21 && row_of("More results") < 0 && row_of("Like video") == 0);
    assert(row_of("Play next") < 0 && row_of("Create playlist with this video") < 0 && row_of("Refresh section") < 0);
    assert(row_of("Open playlist") == 18 && row_of("Share link") == 19 && row_of("Use SponsorBlock on this channel") == 20);
    assert(tube_player_action(TUBE_PLAYER_SPONSOR, label, sizeof label, NULL, 0) && !strcmp(label, "Use SponsorBlock on this channel"));
    assert(tube_player_action(TUBE_PLAYER_SHARE, label, sizeof label, NULL, 0) && !strcmp(label, "Share link"));
    assert(tube_player_action(TUBE_PLAYER_LIKE, label, sizeof label, NULL, 0) && !strcmp(label, "Like video"));
    assert(tube_player_menu_begin(TUBE_PLAYER_SHARE) == TUBE_MENU_OPENED && !strcmp(tube_player_menu_row(0)->title, "https://youtu.be/abcdefghijk"));
    tube_player_menu_close();
    p->list_catalog("library:profile:guest", 0, items_cb, NULL);

    /* Quality limits behind the presets. */
    int height, fps, codec;
    tube_player_quality(&height, &fps, &codec); assert(height == 1080 && !fps && !codec);
    assert(!tube_player_set_quality(721, 0, 0) && !tube_player_set_quality(720, 24, 0) && !tube_player_set_quality(720, 0, 5));
    assert(tube_player_set_quality(720, 60, TUBE_CODEC_VP9) && tube_player_set_quality(-1, -1, -1));
    tube_player_quality(&height, &fps, &codec); assert(height == 720 && fps == 60 && codec == TUBE_CODEC_VP9);
    assert(tube_preset_find(height, fps, codec) > 0);
    assert(tube_player_remember_height(1012) && !tube_player_remember_height(0));
    tube_player_quality(&height, &fps, &codec); assert(height == 1080 && fps == 60);
    assert(tube_player_remember_height(4000)); tube_player_quality(&height, NULL, NULL); assert(height == 2160);
    assert(tube_player_set_quality(720, 30, TUBE_CODEC_AVC));
    p->shutdown(); assert(!p->init());                 /* saved for the profile */
    tube_player_quality(&height, &fps, &codec); assert(height == 720 && fps == 30 && codec == TUBE_CODEC_AVC);

    /* Apply now: the playing video reopens at its position with the new limits,
     * and its stream may differ from the one that was playing. */
    assert(tube_player_set_quality(1080, 0, 0));
    resolve_to("1080", "60", "vp9", "\"max_height\":1080"); start();
    assert(tube_player_set_quality(720, 30, TUBE_CODEC_AVC));
    assert(!tube_player_reload(NAN, 0, 1) && !tube_player_reload(42.5, 1, 1));
    assert(tube_player_reload(42.5, 0, 1) && tube_recovery_pending() && !tube_player_reload(42.5, 0, 1));
    assert(!tube_error_fix_relaxed());                  /* not before the host took it */
    assert(tube_take_recovery(&item, &position, &subtitles) && position == 42.5 && subtitles == 1 && tube_error_fix_relaxed());
    resolve_to("720", "30", "avc", "\"max_height\":720,\"max_fps\":30,\"codec\":\"avc\"");
    assert(tube_recovery_choice(offered, 1) == -1 && tube_error_fix_choice(offered, 1) == 0);
    assert(!tube_error_fix_relaxed() && tube_error_fix_choice(offered, 1) == -1);     /* taken once */
    start(); tube_recovery_finish();

    /* Error fixer. A failed video decoder first asks for AVC at up to 1080p30... */
    assert(tube_player_set_quality(2160, 0, 0));
    resolve_to("2160", "60", "vp9", "\"max_height\":2160"); start();
    assert(tube_error_fix_begin(TUBE_ERROR_VIDEO_DECODER, 10, 0, 1) == TUBE_FIX_SAFE_VIDEO && tube_recovery_pending());
    assert(tube_take_recovery(&item, &position, &subtitles) && position == 10 && subtitles == 1 && tube_error_fix_relaxed());
    resolve_to("1080", "30", "avc", "\"max_height\":1080,\"max_fps\":30,\"codec\":\"avc\"");
    assert(tube_error_fix_choice(offered, 1) == 0); start(); tube_recovery_finish();
    /* ...then a smaller picture, as long buffering does once it repeats. */
    assert(tube_error_fix_begin(TUBE_ERROR_VIDEO_DECODER, 11, 0, 0) == TUBE_FIX_LOWER_QUALITY);
    assert(tube_take_recovery(&item, &position, &subtitles) && !subtitles && tube_error_fix_relaxed());
    resolve_to("720", "30", "avc", "\"max_height\":720,\"max_fps\":30,\"codec\":\"avc\"");
    assert(tube_error_fix_choice(offered, 1) == 0); start(); tube_recovery_finish();
    assert(tube_error_fix_begin(TUBE_ERROR_VIDEO_DECODER, 12, 0, 0) == TUBE_FIX_NONE && !tube_recovery_pending());
    /* Long buffering turns captions off and reloads the same stream through the strict match. */
    assert(tube_error_fix_begin(TUBE_ERROR_BUFFERING, 13, 0, 1) == (TUBE_FIX_RELOAD | TUBE_FIX_SUBTITLES_OFF));
    assert(tube_take_recovery(&item, &position, &subtitles) && position == 13 && !subtitles && !tube_error_fix_relaxed());
    resolve_to("720", "30", "avc", "\"max_height\":720"); assert(tube_recovery_choice(offered, 1) == 0); start(); tube_recovery_finish();
    /* The budget of three reloads for one video also ends the fixer. */
    assert(tube_error_fix_begin(TUBE_ERROR_BUFFERING, 14, 0, 0) == TUBE_FIX_NONE && !tube_queue_recovery(14, 0, 0));
    /* A new selection starts over; switched off, the fixer leaves everything alone. */
    assert(tube_player_set_quality(1440, 0, 0));
    resolve_to("1440", "30", "vp9", "\"max_height\":1440"); start();
    assert(tube_extra_set_error_fixer(0) && tube_error_fix_begin(TUBE_ERROR_BUFFERING, 5, 0, 1) == TUBE_FIX_NONE && !tube_recovery_pending());
    assert(tube_extra_set_error_fixer(1) && tube_error_fix_begin(TUBE_ERROR_BUFFERING, 5, 0, 0) == TUBE_FIX_RELOAD);
    assert(tube_take_recovery(&item, &position, &subtitles));
    resolve_to("1440", "30", "vp9", NULL); assert(tube_recovery_choice(offered, 1) == 0); start(); tube_recovery_finish();
    assert(tube_error_fix_begin(TUBE_ERROR_BUFFERING, 6, 0, 0) == TUBE_FIX_LOWER_QUALITY);
    assert(tube_take_recovery(&item, &position, &subtitles) && tube_error_fix_relaxed());
    resolve_to("1080", "30", "vp9", "\"max_height\":1080"); assert(tube_error_fix_choice(offered, 1) == 0);
    tube_recovery_finish();
    p->shutdown();
    puts("PASS: in-player video actions, lists and launches, browser paging state, quality presets, reload and error fixer");
    return 0;
}
