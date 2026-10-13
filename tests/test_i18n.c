/* Interface localization: lookup, patterns, plural forms, fallbacks, buffer bounds and language choice. */
#include "tube_i18n.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char out[512];
static const char *tr(const char *source, int flags) {
    memset(out, '#', sizeof out); out[sizeof out - 1] = 0;
    return tube_i18n_translate(source, out, sizeof out, flags) ? out : NULL;
}
static void use(const char *tag) { assert(!tube_i18n_set_preference(tag)); assert(!strcmp(tube_i18n_language(), *tag ? tag : "en")); }
/* The translated line with its number taken out, to compare plural forms. */
static const char *shape(const char *source) {
    static char kept[8][256]; static int next;
    char *line = kept[next++ % 8]; size_t used = 0;
    const char *text = tr(source, TUBE_I18N_LABELS);
    assert(text);
    for (; *text && used + 1 < 256; ++text) if ((*text < '0' || *text > '9') && *text != '.' && *text != ',') line[used++] = *text;
    line[used] = 0;
    return line;
}
static int has_tag(const char *tag) {
    for (int i = 0; i < tube_i18n_language_count(); ++i) if (!strcmp(tube_i18n_language_tag(i), tag)) return 1;
    return 0;
}

int main(void) {
    char directory[] = "/tmp/tube-i18n-XXXXXX", path[128], other[128];
    assert(mkdtemp(directory));
    snprintf(path, sizeof path, "%s/interface-language", directory);
    snprintf(other, sizeof other, "%s/elsewhere", directory);
    tube_i18n_set_storage(path);

    /* Nothing chosen and an English console: English, and nothing is translated or touched. */
    assert(!strcmp(tube_i18n_language(), "en") && !*tube_i18n_preference());
    assert(!tr("Subscriptions", 0) && out[0] == '#');
    char field[32] = "Subscriptions";
    tube_i18n_field(field, sizeof field, TUBE_I18N_LABELS); assert(!strcmp(field, "Subscriptions"));
    const unsigned english = tube_i18n_generation();

    /* The catalogs this version ships. */
    static const char *const shipped[] = {"cs", "da", "de", "el", "es", "fi", "fr", "hu", "id", "it", "nb", "nl", "pl", "pt-BR", "pt-PT",
                                          "ro", "ru", "sv", "tr", "uk", "vi"};
    assert(tube_i18n_language_count() == (int)(sizeof shipped / sizeof *shipped));
    for (size_t i = 0; i < sizeof shipped / sizeof *shipped; ++i) {
        assert(has_tag(shipped[i]));
        use(shipped[i]);
        /* Terms every catalog must hold, and names no catalog may change. */
        assert(tr("Subscriptions", 0) && tr("Watch later", 0) && tr("Playlists", 0) && tr("Selected", 0));
        assert(tr("Close ProsperoTube", 0) && strstr(out, "ProsperoTube"));
        assert(tr("YouTube preferences", 0) && strstr(out, "YouTube"));
        assert(tr("SponsorBlock actions", 0) && strstr(out, "SponsorBlock"));
        assert(!tr("ProsperoTube", 0) && !tr("YouTube", 0));
        /* Numbers and units pass through patterns. */
        assert(tr("Up to 1440p", 0) && strstr(out, "1440"));
        assert(tr("Zoom 150%", 0) && strstr(out, "150"));
        assert(tr("Speed for all videos: 1.25x", 0) && strstr(out, "1.25"));
        assert(tr("PAGE 2 OF 17", 0) && strstr(out, "2") && strstr(out, "17"));
        assert(tr("Seek interval: 1 second", 0) && strstr(out, "1"));
        assert(tr("Seek interval: 10 seconds", 0) && strstr(out, "10"));
        assert(tr("12 channels marked as watched", 0) && strstr(out, "12"));
        /* Lines the updater makes at run time: versions, percentages and names pass through, a known reason is translated. */
        assert(tr("Version 1.2.3 is available. Open Settings, About to read what is new and update.", 0) && strstr(out, "1.2.3"));
        assert(tr("Downloading: 37% - select to pause", 0) && strstr(out, "37"));
        assert(tr("Installed 0.1.0; latest 0.2.0 (ProsperoTube 0.2.0)", 0) && strstr(out, "0.1.0") && strstr(out, "ProsperoTube 0.2.0"));
        assert(tr("What is new in ProsperoTube 0.2.0", 0) && strstr(out, "ProsperoTube 0.2.0") && strcmp(out, "What is new in ProsperoTube 0.2.0"));
        assert(tr("The update server could not be reached (too many redirects).", 0) && !strstr(out, "too many redirects"));
        assert(tr("The update server could not be reached (Connection reset by peer).", 0) && strstr(out, "(Connection reset by peer)"));
        assert(tr("Notify about updates: on", TUBE_I18N_LABELS) && tr("Theme: Teal (true black)", TUBE_I18N_LABELS));
        char notice[256] = "ProsperoTube was updated to version 0.2.0.";        /* a toast's own buffer */
        tube_i18n_field(notice, sizeof notice, TUBE_I18N_LABELS);
        assert(strstr(notice, "0.2.0") && strstr(notice, "ProsperoTube") && strcmp(notice, "ProsperoTube was updated to version 0.2.0."));
        /* Interface preferences: a label with a value that is a number, a name or a term of the catalog. */
        assert(tr("Interface scale: 1.25x", TUBE_I18N_LABELS) && strstr(out, "1.25x") && tr("4K: any", TUBE_I18N_LABELS) && !strncmp(out, "4K", 2));
        assert(tr("1.3333:1 (4:3 display)", 0) && strstr(out, "1.3333:1") && strstr(out, "4:3"));
#ifdef TUBE_I18N_TEST_KIT
        /* The player interface path: whole lines and counted lines, never the two-part label rule. */
        assert(tube_i18n_language_kit((int)i) && tube_i18n_kit("Subscriptions", out, sizeof out) && tr("Subscriptions", TUBE_I18N_KIT));
        assert(tube_i18n_kit("Up to 720p", out, sizeof out) && strstr(out, "720") && !tube_i18n_kit("Show dislikes: on", out, sizeof out));
        assert(!tube_i18n_kit("Chat message by a viewer", out, sizeof out) && !tube_i18n_kit("", out, sizeof out) && !tube_i18n_kit("Subscriptions", out, 2));
#else
        /* The player interface fonts hold ASCII and a few signs today: no catalog is drawn there. */
        assert(!tube_i18n_language_kit((int)i) && !tube_i18n_kit("Subscriptions", out, sizeof out) && !tr("Subscriptions", TUBE_I18N_KIT));
        /* A toast is one of its lines: toast() asks with the same flag, and the line stays as it is. */
        char toast[64] = "Skipped an optional segment";
        tube_i18n_field(toast, sizeof toast, TUBE_I18N_LABELS | TUBE_I18N_KIT);
        assert(!strcmp(toast, "Skipped an optional segment") && tr("Skipped an optional segment", TUBE_I18N_LABELS));
#endif
    }
    assert(tube_i18n_generation() != english);

    /* Exact texts. */
    use("es");
    assert(!strcmp(tr("Subscriptions", 0), "Suscripciones") && !strcmp(tr("History", 0), "Historial"));
    use("de");
    assert(!strcmp(tr("Subscriptions", 0), "Abos") && !strcmp(tr("Playlists", 0), "Playlists"));
    use("ru");
    assert(!strcmp(tr("Subscriptions", 0), "Подписки"));

    /* "<label>: <value>" is translated in two parts, only where the caller says the line is the interface's own. */
    use("es");
    char label[512], value[512], joined[1100];
    assert(tr("Show dislikes", 0)); snprintf(label, sizeof label, "%s", out);
    assert(tr("on", 0)); snprintf(value, sizeof value, "%s", out);
    snprintf(joined, sizeof joined, "%s: %s", label, value);
    assert(!strcmp(tr("Show dislikes: on", TUBE_I18N_LABELS), joined));
    assert(!tr("Show dislikes: on", 0) && out[0] == '#');
    /* A value the catalog does not know passes through; an unknown label leaves the line alone. */
    snprintf(joined, sizeof joined, "%s: 1080p", label);
    assert(!strcmp(tr("Show dislikes: 1080p", TUBE_I18N_LABELS), joined));
    assert(!tr("Some creator: on", TUBE_I18N_LABELS) && !tr("Linus: Live", TUBE_I18N_LABELS));
    /* French puts a space before the colon. */
    use("fr");
    assert(tr("Show dislikes: on", TUBE_I18N_LABELS) && strstr(out, " : "));
    /* The experimental mark of a row made at run time. */
    use("es");
    assert(tr("Subscriptions (experimental)", TUBE_I18N_LABELS) && !strncmp(out, "Suscripciones (", 15));
    assert(!tr("My holiday (experimental)", TUBE_I18N_LABELS));
    /* A text inside a pattern is translated when known and kept when not. */
    assert(tr("Pinned", 0)); snprintf(label, sizeof label, "%s", out);
    assert(tr("Pinned: Lofi beats to study", TUBE_I18N_LABELS) && strstr(out, "Lofi beats to study") && !strncmp(out, label, strlen(label)));
    assert(tr("Add to Road trip 2024", 0) && strstr(out, "Road trip 2024") && !strstr(out, "Add to"));
    assert(tr("3 channels found in subscriptions.json", 0) && strstr(out, "subscriptions.json") && strstr(out, "3"));

    /* Misses: content, near misses and odd input come back untouched. */
    static const char *const misses[] = {"", "Rick Astley - Never Gonna Give You Up", "subscriptions", "Subscriptions ", " Subscriptions",
        "Up to p", "Up to 10 p", "Zoom %", "PAGE OF", "{0}: {1}", "{0}", "%s: %s", "%d", "Seek interval: second", "\xff\xfe", "{", "}{"};
    for (size_t i = 0; i < sizeof misses / sizeof *misses; ++i) assert(!tr(misses[i], 0) && out[0] == '#');
    assert(!tube_i18n_translate(NULL, out, sizeof out, 0) && !tube_i18n_translate("Subscriptions", NULL, 8, 0));
    char huge[2048]; memset(huge, 'a', sizeof huge - 1); huge[sizeof huge - 1] = 0;
    assert(!tr(huge, TUBE_I18N_LABELS));
    memcpy(huge, "Pinned: ", 8); assert(!tr(huge, TUBE_I18N_LABELS));
    huge[400] = 0; assert(tr(huge, TUBE_I18N_LABELS) && strlen(out) >= 392);
    huge[505] = 0; (void)tr(huge, TUBE_I18N_LABELS);       /* fits or not: never past the buffer */

    /* Buffer bounds: a translation that does not fit leaves the buffer as it was. */
    const size_t length = strlen(tr("Subscriptions", 0));
    char small[64];
    for (size_t capacity = 0; capacity <= length + 1; ++capacity) {
        memset(small, '#', sizeof small);
        const int done = tube_i18n_translate("Subscriptions", small, capacity, 0);
        assert(done == (capacity > length));
        if (!done) for (size_t i = 0; i < sizeof small; ++i) assert(small[i] == '#');
        else assert(strlen(small) == length && small[length + 1] == '#');
    }
    for (size_t capacity = 1; capacity < 48; ++capacity) {
        memset(small, '#', sizeof small);
        const int done = tube_i18n_translate("Seek interval: 10 seconds", small, capacity, TUBE_I18N_LABELS);
        if (done) assert(strlen(small) < capacity); else assert(small[0] == '#');
        for (size_t i = capacity; i < sizeof small; ++i) assert(small[i] == '#');
    }
    char tight[14] = "Subscriptions";                       /* 13 letters: the Spanish word has the same length */
    tube_i18n_field(tight, sizeof tight, 0); assert(!strcmp(tight, "Suscripciones"));
    char tighter[9] = "Playback";
    tube_i18n_field(tighter, 4, 0); assert(!strcmp(tighter, "Playback"));     /* not terminated inside the given size: left alone */
    tube_i18n_field(NULL, 4, 0); tube_i18n_field(tight, 0, 0);

    /* Plural forms, each language by its own rule. */
    use("ru");
    const char *one = shape("1 second"), *few = shape("2 seconds"), *many = shape("5 seconds");
    assert(strcmp(one, few) && strcmp(few, many) && strcmp(one, many));
    assert(!strcmp(shape("21 seconds"), one) && !strcmp(shape("101 seconds"), one) && !strcmp(shape("11 seconds"), many));
    assert(!strcmp(shape("3 seconds"), few) && !strcmp(shape("24 seconds"), few) && !strcmp(shape("12 seconds"), many) && !strcmp(shape("14 seconds"), many));
    assert(!strcmp(shape("20 seconds"), many) && !strcmp(shape("0 seconds"), many) && !strcmp(shape("1.5 seconds"), shape("0.5 seconds")));
    use("uk");
    assert(strcmp(shape("1 second"), shape("2 seconds")) && !strcmp(shape("22 seconds"), shape("2 seconds")) && !strcmp(shape("5 seconds"), shape("11 seconds")));
    use("pl");
    one = shape("1 second"); few = shape("2 seconds"); many = shape("5 seconds");
    assert(strcmp(one, few) && strcmp(few, many) && !strcmp(shape("22 seconds"), few) && !strcmp(shape("21 seconds"), many) && !strcmp(shape("12 seconds"), many));
    use("cs");
    one = shape("1 second"); few = shape("3 seconds"); many = shape("5 seconds");
    assert(strcmp(one, few) && strcmp(few, many) && !strcmp(shape("4 seconds"), few) && !strcmp(shape("22 seconds"), many));
    use("ro");
    one = shape("1 second"); few = shape("2 seconds"); many = shape("20 seconds");
    assert(strcmp(one, few) && !strcmp(shape("19 seconds"), few) && !strcmp(shape("119 seconds"), few) && !strcmp(shape("100 seconds"), many) && !strcmp(shape("0 seconds"), few));
    use("fr");
    assert(!strcmp(shape("0 seconds"), shape("1 second")) && strcmp(shape("1 second"), shape("2 seconds")));
    use("es");
    assert(strcmp(shape("1 second"), shape("2 seconds")) && !strcmp(shape("0 seconds"), shape("2 seconds")) && !strcmp(shape("1.5 seconds"), shape("2 seconds")));
    /* The English singular and plural lines reach the same translation: the count decides, not the English form. */
    assert(!strcmp(shape("1 seconds"), shape("1 second")) && !strcmp(shape("2 second"), shape("2 seconds")));
    use("tr");
    assert(!strcmp(shape("1 second"), shape("2 seconds")));
    use("id");
    assert(!strcmp(shape("1 channel marked as watched"), shape("7 channels marked as watched")));

    /* Language choice: the preference, then the console, then English. */
    use("");
    assert(!strcmp(tube_i18n_language(), "en"));
    static const struct { int system; const char *tag, *used; } consoles[] = {
        {0, "ja-JP", "en"}, {1, "en-US", "en"}, {2, "fr-FR", "fr"}, {3, "es-ES", "es"}, {4, "de-DE", "de"}, {5, "it-IT", "it"},
        {6, "nl-NL", "nl"}, {7, "pt-PT", "pt-PT"}, {8, "ru-RU", "ru"}, {9, "ko-KR", "en"}, {10, "zh-Hant", "en"}, {11, "zh-Hans", "en"},
        {12, "fi-FI", "fi"}, {13, "sv-SE", "sv"}, {14, "da-DK", "da"}, {15, "nb-NO", "nb"}, {16, "pl-PL", "pl"}, {17, "pt-BR", "pt-BR"},
        {18, "en-GB", "en"}, {19, "tr-TR", "tr"}, {20, "es-419", "es"}, {21, "ar", "en"}, {22, "fr-CA", "fr"}, {23, "cs-CZ", "cs"},
        {24, "hu-HU", "hu"}, {25, "el-GR", "el"}, {26, "ro-RO", "ro"}, {27, "th-TH", "en"}, {28, "vi-VN", "vi"}, {29, "id-ID", "id"},
        {30, "uk-UA", "uk"}, {31, "", "en"}, {-7, "", "en"}, {4096, "", "en"}};
    for (size_t i = 0; i < sizeof consoles / sizeof *consoles; ++i) {
        assert(!strcmp(tube_i18n_system_tag(consoles[i].system), consoles[i].tag));
        if (consoles[i].system < 0) continue;       /* a negative number asks the console itself */
        tube_i18n_set_system_language(consoles[i].system);
        assert(!strcmp(tube_i18n_language(), consoles[i].used));
    }
    tube_i18n_set_system_language(8);
    assert(!strcmp(tube_i18n_language(), "ru") && !strcmp(tube_i18n_setting_title(), "Interface language: System default"));
    use("en"); assert(!tr("Subscriptions", 0) && !strcmp(tube_i18n_setting_title(), "Interface language: English"));
    use("vi"); assert(tr("Subscriptions", 0));
    assert(tube_i18n_set_preference("xx") == -1 && tube_i18n_set_preference("es-ES") == -1 && tube_i18n_set_preference("a-very-long-language-tag") == -1);
    assert(!strcmp(tube_i18n_language(), "vi"));
    /* The preference is kept in its file and read again by the next start. */
    tube_i18n_set_storage(path);
    assert(!strcmp(tube_i18n_preference(), "vi") && !strcmp(tube_i18n_language(), "vi"));
    FILE *file = fopen(path, "wb"); assert(file); fputs("not a language\n", file); fclose(file);
    tube_i18n_set_storage(path);
    assert(!*tube_i18n_preference() && !strcmp(tube_i18n_language(), "ru"));
    file = fopen(path, "wb"); assert(file); fputs("pt-BR", file); fclose(file);
    tube_i18n_set_storage(path);
    assert(!strcmp(tube_i18n_language(), "pt-BR"));
    tube_i18n_set_storage(other);                           /* no file there: the console decides */
    assert(!*tube_i18n_preference() && !strcmp(tube_i18n_language(), "ru"));
    tube_i18n_set_system_language(1);
    assert(!strcmp(tube_i18n_language(), "en"));
    /* A file that appears later is found: the console grants storage after the first texts were asked for. */
    file = fopen(other, "wb"); assert(file); fputs("sv\n", file); fclose(file);
    for (int waited = 0; waited < 30 && strcmp(tube_i18n_language(), "sv"); ++waited) usleep(100000);
    assert(!strcmp(tube_i18n_language(), "sv") && !strcmp(tube_i18n_preference(), "sv"));
    unlink(other);
    assert(!strcmp(tube_i18n_language(), "sv"));         /* once read, the file is not read again */

    /* The preference page. */
    tube_i18n_set_storage(path); use("");
    assert(tube_i18n_choices() == tube_i18n_language_count() + 2);
    assert(!strcmp(tube_i18n_choice_id(0), TUBE_I18N_SETTING ":system") && !strcmp(tube_i18n_choice_title(0), "System default"));
    assert(!strcmp(tube_i18n_choice_detail(0), "Selected") && !strcmp(tube_i18n_choice_id(1), TUBE_I18N_SETTING ":en"));
    assert(!*tube_i18n_choice_detail(-1) && !*tube_i18n_choice_detail(tube_i18n_choices()) && !*tube_i18n_choice_title(tube_i18n_choices()));
    int spanish = -1;
    for (int i = 2; i < tube_i18n_choices(); ++i) {
        assert(*tube_i18n_choice_title(i) && *tube_i18n_choice_detail(i) && strcmp(tube_i18n_choice_detail(i), "Selected"));
        if (!strcmp(tube_i18n_choice_id(i), TUBE_I18N_SETTING ":es")) spanish = i;
    }
    assert(spanish > 1 && !strcmp(tube_i18n_choice_title(spanish), "Espa\xc3\xb1ol") && !strcmp(tube_i18n_choice_detail(spanish), "Spanish"));
    assert(tube_i18n_select(TUBE_I18N_SETTING) == 0 && !strcmp(tube_i18n_language(), "en"));
    assert(tube_i18n_select(TUBE_I18N_SETTING ":es") == 1 && !strcmp(tube_i18n_language(), "es"));
    assert(!strcmp(tube_i18n_choice_detail(spanish), "Selected") && strcmp(tube_i18n_choice_detail(0), "Selected"));
    assert(!strcmp(tube_i18n_setting_title(), "Interface language: Espa\xc3\xb1ol"));
    assert(tr(tube_i18n_setting_title(), TUBE_I18N_LABELS) && strstr(out, ": Espa\xc3\xb1ol") && strncmp(out, "Interface", 9));
    assert(tube_i18n_select(TUBE_I18N_SETTING ":xx") == -1 && tube_i18n_select(TUBE_I18N_SETTING "s") == -1 && tube_i18n_select("settings") == -1 && tube_i18n_select(NULL) == -1);
    assert(!strcmp(tube_i18n_language(), "es"));
    assert(tube_i18n_select(TUBE_I18N_SETTING ":system") == 1 && !*tube_i18n_preference());

    unlink(path); rmdir(directory);
    puts("PASS: lookup, patterns, plural forms, fallbacks, buffer bounds and language choice");
    return 0;
}
