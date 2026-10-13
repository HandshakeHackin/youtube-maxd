/* Interface language through the provider: its rows are the same in every language and carry a mark,
 * the marked rows translate where they are shown, rows of YouTube content never do. The main menu,
 * YouTube preferences on all their pages, the Interface screens, the video actions menu setup, About
 * and more are walked in six languages: a row that stays English fails the test. */
#define main existing_provider_regression
#include "test_native.c"
#undef main
#include "tube_i18n.h"
static const evo_provider_t *p;
static void show(const char *id) { p->list_catalog(id, 0, items_cb, NULL); assert(last_ok && !pending_count); }
static int find_id(const char *id) {
    for (int i = 0; i < last_count; ++i) if (!strcmp(last_items[i].id, id)) return i;
    return -1;
}
/* What the browser does with a row before showing it. */
static int shown(const evo_provider_item_t *item, char *title, char *subtitle) {
    int changed = 0;
    snprintf(title, EVO_PROVIDER_MAX_TITLE * 3, "%s", item->title); snprintf(subtitle, EVO_PROVIDER_MAX_SUBTITLE * 3, "%s", item->subtitle);
    if (!item->tube_interface) return 0;
    changed += tube_i18n_translate(item->title, title, EVO_PROVIDER_MAX_TITLE * 3, TUBE_I18N_LABELS);
    changed += 2 * tube_i18n_translate(item->subtitle, subtitle, EVO_PROVIDER_MAX_SUBTITLE * 3, TUBE_I18N_LABELS);
    return changed;
}
/* A title the catalog is not meant to hold: a number with its unit ("1.25x", "2"), or the app's own name. */
static int untranslatable(const char *text) {
    if (!strncmp(text, "ProsperoTube", 12)) return 1;
    for (; *text; ++text) if (!strchr("0123456789.:x ", *text)) return 0;
    return 1;
}
/* The pages walked in every language: lists of interface rows that ask nothing of the service and change nothing. */
static const char *const pages[] = {"", "settings", "discover", "library", "settings:ui", "settings:ui:scale", "settings:ui:title-lines",
    "settings:ui:title-scroll", "settings:ui:scheme", "settings:ui:exit", "settings:ui:search-exit", "settings:menu", "settings:menu:like",
    "settings:about", "settings:search", "settings:sections", "settings:hide", "settings:experimental", "settings:extra:player", "settings:pixel-ratio"};
#define PAGES ((int)(sizeof pages / sizeof *pages))
#define LISTS 40
static evo_provider_item_t english[LISTS][24];
static int counts[LISTS], lists;
/* language NULL: remember what English lists. Else: the provider lists the same, and every text of it translates. */
static void walk(const char *language) {
    char title[EVO_PROVIDER_MAX_TITLE * 3], subtitle[EVO_PROVIDER_MAX_SUBTITLE * 3];
    int list = 0;
    for (int page = 0; page < PAGES; ++page) for (int part = 0, more = 1; more; ++part, ++list) {
        p->list_catalog(pages[page], part, items_cb, NULL);
        assert(last_ok && !pending_count && last_count > 0 && last_count <= 24 && list < LISTS);
        more = last_more;
        if (!language) { counts[list] = last_count; memcpy(english[list], last_items, sizeof last_items); lists = list + 1; }
        assert(list < lists && last_count == counts[list]);
        for (int i = 0; i < last_count; ++i) {
            const evo_provider_item_t *item = &last_items[i];
            assert(item->tube_interface == 1);
            if (!language) { assert(!shown(item, title, subtitle) && !strcmp(title, item->title)); continue; }
            if (!strcmp(item->id, TUBE_I18N_SETTING)) continue;        /* names the chosen language */
            /* The provider's own rows do not depend on the language: tests, the player menu and logs read English. */
            assert(!strcmp(item->title, english[list][i].title) && !strcmp(item->subtitle, english[list][i].subtitle));
            /* Every text of these pages is in the catalog: no English line is left among translated ones. */
            const int changed = shown(item, title, subtitle), wanted = (untranslatable(item->title) ? 0 : 1) + (*item->subtitle ? 2 : 0);
            if (changed != wanted) {
                fprintf(stderr, "%s: not translated on page '%s': '%s' / '%s'\n", language, pages[page], item->title, item->subtitle);
                assert(0);
            }
            assert(*title && strlen(title) < sizeof title - 1 && strlen(subtitle) < sizeof subtitle - 1);
        }
    }
    assert(list == lists);
}
/* YouTube preferences span several lists: leaves the one with the Interface language row listed; -1 without it. */
static int setting_row(void) {
    for (int part = 0, more = 1; more; ++part) {
        p->list_catalog("settings", part, items_cb, NULL); assert(last_ok && !pending_count);
        more = last_more;
        if (find_id(TUBE_I18N_SETTING) >= 0) return find_id(TUBE_I18N_SETTING);
    }
    return -1;
}
static const char *videos = "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Subscriptions\",\"subtitle\":\"History\","
    "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":false}}";

int main(void) {
    char dir[] = "/tmp/tube-i18n-provider-XXXXXX", title[EVO_PROVIDER_MAX_TITLE * 3], subtitle[EVO_PROVIDER_MAX_SUBTITLE * 3];
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    p = &evo_provider_youtube; assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    tube_i18n_set_system_language(1);

    /* English: the preference row exists, every menu row is marked, nothing is translated. */
    assert(!strcmp(tube_i18n_language(), "en"));
    walk(NULL);
    assert(lists >= PAGES + 2);                     /* YouTube preferences alone fill three lists */
    int row = setting_row();
    assert(row >= 0 && !strcmp(last_items[row].title, "Interface language: System default"));
    assert(!strcmp(last_items[row].subtitle, "Language of ProsperoTube's menus and messages") && last_items[row].is_folder);

    /* The language page: the console's language, English, then every catalog under its own name. */
    show(TUBE_I18N_SETTING);
    assert(last_count == tube_i18n_choices() && last_count == tube_i18n_language_count() + 2 && !last_more);
    assert(!strcmp(last_items[0].id, TUBE_I18N_SETTING ":system") && !strcmp(last_items[0].title, "System default") && !strcmp(last_items[0].subtitle, "Selected"));
    assert(!strcmp(last_items[1].title, "English") && !strcmp(last_items[1].subtitle, "English"));
    p->list_catalog(TUBE_I18N_SETTING, 1, items_cb, NULL); assert(last_ok && !last_count);
    error_text[0] = 0; p->list_catalog(TUBE_I18N_SETTING ":klingon", 0, items_cb, NULL);
    assert(!last_ok && !strcmp(error_text, "Select a listed language.") && !strcmp(tube_i18n_language(), "en"));

    static const char *const languages[] = {"es", "ru", "vi", "de", "fr", "el"};
    for (size_t language = 0; language < sizeof languages / sizeof *languages; ++language) {
        char id[64]; snprintf(id, sizeof id, TUBE_I18N_SETTING ":%s", languages[language]);
        show(id);                                   /* choosing answers with the page again, the choice marked */
        assert(!strcmp(tube_i18n_language(), languages[language]) && last_count == tube_i18n_choices());
        row = find_id(id); assert(row >= 2 && !strcmp(last_items[row].subtitle, "Selected") && strcmp(last_items[0].subtitle, "Selected"));
        assert(shown(&last_items[row], title, subtitle) == 2 && !strcmp(title, last_items[row].title) && strcmp(subtitle, "Selected"));
        assert(shown(&last_items[0], title, subtitle) == 3 && strcmp(title, "System default"));
        /* The provider's own rows do not depend on the language: tests, the player menu and logs read English. */
        walk(languages[language]);
        row = setting_row();
        assert(row >= 0 && !strncmp(last_items[row].title, "Interface language: ", 20) && strcmp(last_items[row].title + 20, "System default"));
        assert(shown(&last_items[row], title, subtitle) == 3 && strncmp(title, "Interface", 9) && strstr(title, last_items[row].title + 20));

        /* Rows parsed from the service are content: a video named like a menu entry keeps its name. */
        p->list_catalog("section:news", 0, items_cb, NULL);
        if (!language) reply(0, 1, 200, "{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}");     /* the first request pairs */
        poll_job(videos);
        assert(last_ok && last_count == 1 && !last_items[0].tube_interface && !strcmp(last_items[0].title, "Subscriptions"));
        assert(!shown(&last_items[0], title, subtitle) && !strcmp(title, "Subscriptions") && !strcmp(subtitle, last_items[0].subtitle));
        /* An error is reported in English to the log; the browser translates it when it shows it. */
        error_text[0] = 0; p->list_catalog(TUBE_I18N_SETTING ":klingon", 0, items_cb, NULL);
        assert(!last_ok && !strcmp(error_text, "Select a listed language.") && tube_i18n_translate(error_text, title, sizeof title, TUBE_I18N_LABELS));
    }

    /* Back to the console's language, and that language changing. */
    show(TUBE_I18N_SETTING ":system");
    assert(!strcmp(tube_i18n_language(), "en") && !strcmp(last_items[0].subtitle, "Selected"));
    tube_i18n_set_system_language(17);
    assert(!strcmp(tube_i18n_language(), "pt-BR"));
    show(""); assert(shown(&last_items[0], title, subtitle) && !strcmp(last_items[0].title, english[0][0].title));
    show(TUBE_I18N_SETTING ":en");
    assert(!strcmp(tube_i18n_language(), "en"));
    show(""); assert(!shown(&last_items[0], title, subtitle));
    p->shutdown();
    puts("PASS: provider rows are marked and language-independent; interface rows translate, content rows do not");
    return 0;
}
