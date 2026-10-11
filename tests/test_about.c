// About tables: links, feedback, credits against the notices file, and the
// generated changelog rows against CHANGELOG.md.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_about.h"
#include "tube_qr.h"

#include <assert.h>
#include <stdlib.h>

/* Provider row limits (EVO_PROVIDER_MAX_TITLE/SUBTITLE/OVERVIEW) the rows must fit. */
enum { ROW_TITLE = 192, ROW_SUBTITLE = 160, ROW_OVERVIEW = 320 };

static char *slurp(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    char *text = malloc((size_t)size + 1);
    assert(text && fread(text, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    text[size] = 0;
    return text;
}

int main(int argc, char **argv) {
    char title[64], tiny[8];
    assert(argc == 3);

    /* Version: three numbers, the same one the changelog starts with. */
    int major = -1, minor = -1, patch = -1, used = 0;
    assert(sscanf(tube_about_version(), "%d.%d.%d%n", &major, &minor, &patch, &used) == 3);
    assert(major >= 0 && minor >= 0 && patch >= 0 && (size_t)used == strlen(tube_about_version()));
    assert(!strcmp(tube_about_version(), TUBE_ABOUT_CHANGELOG_VERSION));
    assert(tube_about_title(title, sizeof title) == 0 && !strncmp(title, "ProsperoTube ", 13) && !strcmp(title + 13, tube_about_version()));
    assert(tube_about_title(tiny, sizeof tiny) == -1 && !*tiny && tube_about_title(NULL, 0) == -1);

    /* Links: the project's own pages, unique ids, row-sized text, QR-sized URLs. */
    int links = tube_about_link_count(), feedback = 0;
    assert(links >= 4 && !tube_about_link_at(-1) && !tube_about_link_at(links));
    for (int i = 0; i < links; i++) {
        const tube_about_link *link = tube_about_link_at(i);
        tube_qr_code code;
        assert(*link->id && strlen(link->title) < ROW_TITLE && *link->title && strlen(link->detail) < ROW_SUBTITLE && *link->detail);
        assert(!strncmp(link->url, "https://github.com/blackbearreloaded/ProsperoTube", 49));
        assert(link->url[49] == 0 || link->url[49] == '/');
        assert(strlen(link->url) < ROW_OVERVIEW && !strchr(link->url, ' '));
        assert(tube_qr_encode(link->url, strlen(link->url), TUBE_QR_LEVEL_M, &code) == 0 && code.version <= 5);
        assert(tube_about_link_find(link->id) == link);
        /* Feedback entries follow the project links. */
        assert(link->feedback == 0 || link->feedback == 1);
        if (i) assert(link->feedback >= tube_about_link_at(i - 1)->feedback);
        feedback += link->feedback;
    }
    assert(feedback >= 1 && !tube_about_link_at(0)->feedback);
    assert(!strcmp(tube_about_link_find("releases")->url, "https://github.com/blackbearreloaded/ProsperoTube/releases"));
    assert(!strcmp(tube_about_link_find("source")->url, "https://github.com/blackbearreloaded/ProsperoTube"));
    assert(!strcmp(tube_about_link_find("issues")->url, "https://github.com/blackbearreloaded/ProsperoTube/issues"));
    assert(tube_about_link_find("issues")->feedback && !tube_about_link_find("nothing") && !tube_about_link_find(NULL));

    /* Credits: every line points at text that is really in the notices file. */
    char *notices = slurp(argv[1]);
    for (char *at = notices; *at; at++) if (*at == '\n' || *at == '\r') *at = ' ';
    int credits = tube_about_credit_count();
    assert(credits >= 10 && !tube_about_credit_at(-1) && !tube_about_credit_at(credits));
    for (int i = 0; i < credits; i++) {
        const tube_about_credit *credit = tube_about_credit_at(i);
        assert(*credit->name && strlen(credit->name) < ROW_TITLE && *credit->detail && strlen(credit->detail) < ROW_SUBTITLE);
        if (!strstr(notices, credit->notice)) { fprintf(stderr, "Not in the notices: %s\n", credit->notice); abort(); }
        for (int j = 0; j < i; j++) assert(strcmp(credit->name, tube_about_credit_at(j)->name));
    }
    assert(strstr(tube_about_credit_at(credits - 1)->detail, "THIRD_PARTY_NOTICES.md"));
    free(notices);

    /* Changelog: bounded rows, newest version first, one row per bullet. */
    int all = tube_about_change_count(NULL), current = tube_about_change_count(tube_about_version());
    assert(all == TUBE_ABOUT_CHANGELOG_ROWS && all <= 96 && current >= 1 && current <= all);
    assert(!tube_about_change_at(NULL, -1) && !tube_about_change_at(NULL, all) && !tube_about_change_count("0.0.0"));
    assert(!tube_about_change_at(tube_about_version(), current));
    for (int i = 0; i < all; i++) {
        const tube_about_change *change = tube_about_change_at(NULL, i);
        assert(*change->text && strlen(change->text) <= TUBE_ABOUT_CHANGELOG_ROW_BYTES && strlen(change->text) < ROW_OVERVIEW);
        if (i < current) assert(!strcmp(change->version, tube_about_version()) && change == tube_about_change_at(tube_about_version(), i));
    }
    char *changelog = slurp(argv[2]);
    int bullets = 0;
    for (char *line = strtok(changelog, "\n"); line; line = strtok(NULL, "\n")) bullets += !strncmp(line, "- ", 2);
    assert(bullets == all);
    free(changelog);
    assert(strstr(tube_about_change_at(NULL, 0)->text, "PS5"));

    puts("PASS: About version, links, feedback, credits and changelog rows are bounded and match their sources");
    return 0;
}
