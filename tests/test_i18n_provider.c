/* Interface language through the provider: its rows are the same in every language and carry a mark,
 * the marked rows translate where they are shown, rows of YouTube content never do. */
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
static const char *videos = "{\"state\":\"done\",\"result\":{\"kind\":\"catalog\",\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Subscriptions\",\"subtitle\":\"History\","
    "\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"}],\"has_more\":false}}";

int main(void) {
    char dir[] = "/tmp/tube-i18n-provider-XXXXXX", title[EVO_PROVIDER_MAX_TITLE * 3], subtitle[EVO_PROVIDER_MAX_SUBTITLE * 3];
    static evo_provider_item_t english[4][24]; static int counts[4];
    static const char *const pages[] = {"", "settings", "discover", "library"};
    assert(mkdtemp(dir) && !setenv("EVO_DATA_DIR_OVERRIDE", dir, 1));
    p = &evo_provider_youtube; assert(!p->init());
    SEPARATE_SETTINGS();
    assert(!p->set_source("http://127.0.0.1:8765#123456"));
    tube_i18n_set_system_language(1);

    /* English: the preference row exists, every menu row is marked, nothing is translated. */
    assert(!strcmp(tube_i18n_language(), "en"));
    for (int page = 0; page < 4; ++page) {
        show(pages[page]); assert(last_count > 2 && last_count <= 24);
        counts[page] = last_count; memcpy(english[page], last_items, sizeof last_items);
        for (int i = 0; i < last_count; ++i) assert(last_items[i].tube_interface == 1 && !shown(&last_items[i], title, subtitle) && !strcmp(title, last_items[i].title));
    }
    show("settings");
    int row = find_id(TUBE_I18N_SETTING);
    if (row < 0) { p->list_catalog("settings", 1, items_cb, NULL); assert(last_ok); row = find_id(TUBE_I18N_SETTING); }
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
        tube_i18n_misses(1);
        for (int page = 0; page < 4; ++page) {
            show(pages[page]); assert(last_count == counts[page]);
            for (int i = 0; i < last_count; ++i) {
                if (!strcmp(last_items[i].id, TUBE_I18N_SETTING)) continue;     /* names the chosen language */
                assert(!strcmp(last_items[i].title, english[page][i].title) && !strcmp(last_items[i].subtitle, english[page][i].subtitle));
                /* Every text of these pages is in the catalog: no English line is left among translated ones. */
                const int changed = shown(&last_items[i], title, subtitle);
                if (changed != (*last_items[i].subtitle ? 3 : 1)) {
                    fprintf(stderr, "%s: not translated on page '%s': '%s' / '%s'\n", languages[language], pages[page], last_items[i].title, last_items[i].subtitle);
                    assert(0);
                }
                assert(*title && strlen(title) < sizeof title - 1 && strlen(subtitle) < sizeof subtitle - 1);
            }
        }
        assert(!tube_i18n_misses(0));
        show("settings"); row = find_id(TUBE_I18N_SETTING);
        if (row < 0) { p->list_catalog("settings", 1, items_cb, NULL); assert(last_ok); row = find_id(TUBE_I18N_SETTING); }
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
