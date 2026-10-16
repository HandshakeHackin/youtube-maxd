// Interface language on the real provider host: the shipping document as
// prepare_native.py marks it, the app's fonts, pinned RmlUi and the CPU
// rasteriser. Every catalog draws the main menu and the preferences: the rows
// and the static texts change language, every letter has a glyph in a loaded
// face, the titles are rasterised inside their cards and no line outgrows its
// place. A row of YouTube content keeps its text.
// Five languages are drawn again under other interface preferences (scales
// from 0.4x to 1.4x, more title and subtitle lines, colour schemes), and the
// release-notes page keeps the notes as they were written.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include <RmlUi/Core/ElementText.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <time.h>
#include <unistd.h>
#define private public
#include "evo_rmlui_provider.h"
#undef private
#include "evo_rmlui_provider.cpp"
#include "evo_rmlui_render.h"
#include "evo_rmlui_system.h"
extern "C" {
#include "evo_data_path.h"
}

const EvoRmlBundleFile* evo_rmlui_bundle_find(const std::string&) { return nullptr; }
extern "C" int tube_test_clock_gettime(clockid_t, struct timespec* t) { t->tv_sec=100; t->tv_nsec=0; return 0; }
extern "C" int evo_net_request_async(const char*,const char*,const char*,const char**,int,evo_net_cb,void*) { return -1; }
static char last_error[256];
extern "C" void evo_error_set(const char* format, ...) { va_list list; va_start(list, format); vsnprintf(last_error, sizeof last_error, format, list); va_end(list); }
extern "C" int evo_failure_reason(char* out, size_t capacity) { snprintf(out, capacity, "%s", last_error); return *last_error != 0; }
extern "C" int evo_provider_art_ready(const char*) { return 0; }
extern "C" void evo_bundle_poll(void) {}
extern "C" void evo_provider_art_poll(void) {}
/* The updater is not linked here; these stand in for it, so that the release-notes page has notes to show. */
extern "C" const char* tube_update_app_notes(void) { return "Subscriptions"; }
extern "C" const char* tube_update_app_notes_title(void) { return "What is new in ProsperoTube 0.2.0"; }
extern "C" int evo_provider_art_request(const char*,const char*,char*,size_t) { return -1; }

/* The provider's own pages, with one row of content added: a video named like a menu entry. */
static int with_content(const char* parent, int page, evo_provider_items_cb cb, void* ud) {
    struct Context { evo_provider_items_cb cb; void* ud; } context{cb, ud};
    return evo_provider_youtube.list_catalog(parent, page, [](int ok, const evo_provider_item_t* items, int count, int more, void* inner) {
        auto* to = (Context*)inner;
        std::vector<evo_provider_item_t> rows(items, items + (ok ? count : 0));
        evo_provider_item_t video; memset(&video, 0, sizeof video);
        snprintf(video.id, sizeof video.id, "abcdefghijk"); snprintf(video.title, sizeof video.title, "Subscriptions");
        snprintf(video.subtitle, sizeof video.subtitle, "History"); snprintf(video.overview, sizeof video.overview, "Watch later");
        video.kind = EVO_MEDIA_VIDEO;
        if (ok && !more) rows.push_back(video);   /* once, after the last page: a menu of preferences arrives whole */
        to->cb(ok, rows.data(), (int)rows.size(), more, to->ud);
    }, &context);
}

static std::vector<FT_Face> faces;
static bool has_glyph(char32_t code) {
    for (FT_Face face : faces) if (FT_Get_Char_Index(face, code)) return true;
    return false;
}
static std::u32string decode(const std::string& text) {
    std::u32string out;
    for (size_t i = 0; i < text.size();) {
        const unsigned char lead = (unsigned char)text[i];
        const int length = lead < 0x80 ? 1 : lead >> 5 == 6 ? 2 : lead >> 4 == 14 ? 3 : lead >> 3 == 30 ? 4 : 0;
        assert(length && i + length <= text.size());
        char32_t code = length == 1 ? lead : lead & (0xFF >> (length + 1));
        for (int k = 1; k < length; ++k) { assert(((unsigned char)text[i + k] & 0xC0) == 0x80); code = code << 6 | ((unsigned char)text[i + k] & 0x3F); }
        out.push_back(code); i += length;
    }
    return out;
}
static void visible_texts(Rml::Element* element, std::vector<std::string>& out) {
    if (!element->IsVisible(true)) return;
    if (auto* text = rmlui_dynamic_cast<Rml::ElementText*>(element)) { if (text->GetText().find_first_not_of(" \n\t") != std::string::npos) out.push_back(text->GetText()); }
    for (int i = 0; i < element->GetNumChildren(); ++i) visible_texts(element->GetChild(i), out);
}

int main(int argc, char** argv) {
    assert(argc == 3); std::filesystem::path source(argv[1]), fonts(argv[2]);
    char directory[] = "/tmp/prosperotube-i18n-ui-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE", directory, 1));
    assert(!evo_provider_youtube.init());
    tube_i18n_set_system_language(1);
    evo_provider_t provider = evo_provider_youtube; provider.list_catalog = with_content;
    const int width = 1920, height = 1080;
    EvoSystemInterface system; EvoRenderInterface renderer(width, height);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    /* The faces and their order are the app's (evo_rmlui_app.cpp): every one may stand in for a missing letter. */
    static const char* const names[] = {"LatoLatin-Regular.ttf", "LatoLatin-Bold.ttf", "Roboto-Regular.ttf", "Roboto-Bold.ttf", "Roboto-Medium.ttf",
                                        "NotoSans-Regular.ttf", "NotoSans-Bold.ttf", "DejaVuSans.ttf", "DejaVuSans-Bold.ttf"};
    FT_Library library; assert(!FT_Init_FreeType(&library));
    for (const char* name : names) {
        assert(Rml::LoadFontFace((fonts / name).string(), true));
        FT_Face face; assert(!FT_New_Face(library, (fonts / name).string().c_str(), 0, &face)); faces.push_back(face);
    }
    assert(has_glyph(U'A') && has_glyph(U'я') && has_glyph(U'ω') && has_glyph(U'ễ') && !has_glyph(U'語') && !has_glyph(U'한') && !has_glyph(U'ก'));
    EvoRmlProviderHost host; host.m_context = Rml::CreateContext("i18n-ui", {width, height}); assert(host.m_context);
    host.m_provider_id = "youtube"; host.m_provider = &provider;
    host.m_model_name = "youtube"; assert(host.RegisterDataModel()); auto& model = host.m_model;
    host.m_doc = host.m_context->LoadDocument((source / "youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    auto element = [&](const char* id) { auto found = host.m_doc->GetElementById(id); assert(found); return found; };
    auto pump = [&] {
        host.Tick(); host.m_context->Update();
        if (host.m_needs_initial_focus) { host.SeedFocus(); host.m_context->Update(); }
        host.UpdateSelectedPreview(); host.m_context->Update();
    };
    auto show = [&](const char* parent) {
        host.m_stack.clear(); host.m_crumbs.clear(); host.m_pos_stack.clear();
        if (*parent) { host.m_stack = {parent}; host.m_crumbs = {"Page"}; }
        host.m_page = 0; host.RequestPage(parent, 0); pump();
        assert(!model.loading && host.m_all_rows.size() > 3);
    };
    auto footer_text = [&] { std::vector<std::string> texts; visible_texts(element("tube-footer"), texts); std::string all; for (auto& text : texts) all += text + "|"; return all; };
    auto translated = [&](const std::string& english, int flags) {
        char text[1024];
        return tube_i18n_translate(english.c_str(), text, sizeof text, flags) ? std::string(text) : english;
    };
    std::vector<uint32_t> frame((size_t)width * height);
    auto bright = [&](float left, float top, float right, float bottom) {
        int count = 0;
        for (int y = std::max(0, (int)top); y < std::min(height, (int)std::ceil(bottom)); ++y)
            for (int x = std::max(0, (int)left); x < std::min(width, (int)std::ceil(right)); ++x) {
                const uint32_t pixel = frame[(size_t)y * width + x];
                if ((pixel & 255) > 0xC0 && (pixel >> 8 & 255) > 0xC0 && (pixel >> 16 & 255) > 0xC0) ++count;
            }
        return count;
    };

    /* The interface preferences of the profile, chosen the way the preferences screen does. */
    auto choose = [&](const std::string& id) {
        int ok = 0;
        evo_provider_youtube.list_catalog(id.c_str(), 0, [](int done, const evo_provider_item_t*, int, int, void* ud) { *(int*)ud = done; }, &ok);
        assert(ok);
    };
    struct Look { int scale, grid, title_lines, subtitle_lines, scheme; };
    auto dress = [&](const Look& look) {
        choose("settings:ui:scale:" + std::to_string(look.scale)); choose("settings:ui:grid:" + std::to_string(look.grid));
        choose("settings:ui:title-lines:" + std::to_string(look.title_lines)); choose("settings:ui:subtitle-lines:" + std::to_string(look.subtitle_lines));
        choose("settings:ui:scheme:" + std::to_string(look.scheme));
    };
    static const char* const pages[] = {"", "settings"};
    struct English { std::vector<EvoProviderRow> rows[2]; std::string pager[2], footer; };
    /* What the two pages say in English under the preferences in use, to compare a language against. */
    auto english_pages = [&] {
        English english;
        assert(!tube_i18n_set_preference("en"));
        for (int page = 0; page < 2; ++page) { show(pages[page]); english.rows[page] = host.m_all_rows; english.pager[page] = model.page_info.c_str(); }
        show(""); english.footer = footer_text();
        assert(english.footer.find("Watch / open") != std::string::npos && english.footer.find("Search") != std::string::npos);
        assert(english.rows[0].back().title == "Subscriptions");
        return english;
    };
    std::set<char32_t> letters;
    int wrapped = 0, checked = 0;
    /* One language on both pages: texts, glyphs, rasterised titles, bounds. */
    auto check_language = [&](const std::string& tag, const English& english, const Look& look) {
        assert(!tube_i18n_set_preference(tag.c_str()));
        const float dp = host.m_context->GetDensityIndependentPixelRatio();
        for (int page = 0; page < 2; ++page) {
            show(pages[page]);
            const auto& rows = host.m_all_rows; assert(rows.size() == english.rows[page].size());
            int changed = 0;
            for (size_t i = 0; i + 1 < rows.size(); ++i) {
                if (rows[i].id == "settings:interface-language") { ++changed; continue; }   /* names the language in use */
                assert(rows[i].title == translated(english.rows[page][i].title.c_str(), TUBE_I18N_LABELS).c_str());
                assert(rows[i].subtitle == translated(english.rows[page][i].subtitle.c_str(), TUBE_I18N_LABELS).c_str());
                changed += rows[i].title != english.rows[page][i].title;
            }
            assert(changed * 10 >= ((int)rows.size() - 1) * 8);       /* a few names (Shorts, Trending) are the same in some languages */
            /* The video keeps its title, channel and description although the catalog knows those words. */
            assert(rows.back().title == "Subscriptions" && rows.back().subtitle == "History" && rows.back().overview == "Watch later");
            assert(translated("Subscriptions", 0) != "Subscriptions");

            /* Every letter on screen has a glyph: nothing is drawn as an empty box. */
            std::vector<std::string> texts; visible_texts(host.m_doc, texts); assert(texts.size() > 12);
            for (const std::string& text : texts) for (char32_t code : decode(text)) {
                if (code == U' ' || code == U'\n' || code == 0xA0) continue;
                if (!has_glyph(code)) { fprintf(stderr, "%s: no glyph for U+%04X in '%s'\n", tag.c_str(), (unsigned)code, text.c_str()); assert(0); }
                letters.insert(code);
            }
            /* Static texts of the document follow the language too, also after the preferences restyled it. */
            const std::string footer = footer_text();
            assert(footer != english.footer && footer.find("Watch / open") == std::string::npos);
            assert(footer.find(translated("Watch / open", 0)) != std::string::npos && footer.find(translated("Back", 0)) != std::string::npos);
            assert(model.page_info == translated(english.pager[page], 0).c_str() && model.page_info != english.pager[page].c_str());

            /* Layout: a line that outgrows its place is cut by its own box, never drawn over a neighbour. */
            Rml::ElementList all; host.m_doc->QuerySelectorAll(all, ".tube-card");
            std::vector<Rml::Element*> cards;
            for (Rml::Element* card : all) if (card->IsVisible(true) && card->QuerySelector(".tube-card-title")) cards.push_back(card);   /* not the template the rows are made from */
            assert(cards.size() == std::min<size_t>(EVO_PROVIDER_PAGE_SIZE, rows.size()));
            std::fill(frame.begin(), frame.end(), 0xFF101116u);
            renderer.SetFramebuffer(frame.data()); renderer.SetDimensions(width, height);
            host.m_context->Render();
            Rml::Element* grid = element("tube-grid");
            const float grid_bottom = grid->GetAbsoluteOffset(Rml::BoxArea::Border).y + grid->GetBox().GetSize(Rml::BoxArea::Border).y;
            for (size_t i = 0; i < cards.size(); ++i) {
                Rml::Element* card = cards[i];
                const Rml::Vector2f at = card->GetAbsoluteOffset(Rml::BoxArea::Border), size = card->GetBox().GetSize(Rml::BoxArea::Border);
                assert(at.x >= 0 && at.x + size.x <= width + 0.5f && at.y + size.y <= grid_bottom + 1.0f);
                Rml::Element* lines[2] = {card->QuerySelector(".tube-card-title"), card->QuerySelector(".tube-card-author")};
                assert(lines[1]);
                for (int line = 0; line < 2; ++line) {
                    const Rml::Vector2f line_at = lines[line]->GetAbsoluteOffset(Rml::BoxArea::Border), line_size = lines[line]->GetBox().GetSize(Rml::BoxArea::Border);
                    assert(line_at.x >= at.x - 0.5f && line_at.x + line_size.x <= at.x + size.x + 0.5f);
                    assert(line_at.y >= at.y - 0.5f && line_at.y + line_size.y <= at.y + size.y + 0.5f);
                    /* One line is cut at its end; more lines wrap inside a box of that many lines. */
                    const bool one = (line ? look.subtitle_lines : look.title_lines) == 0;
                    assert(lines[line]->GetComputedValues().overflow_x() == Rml::Style::Overflow::Hidden);
                    assert((lines[line]->GetComputedValues().white_space() == Rml::Style::WhiteSpace::Nowrap) == one);
                }
                const Rml::Vector2f title_at = lines[0]->GetAbsoluteOffset(Rml::BoxArea::Content), title_size = lines[0]->GetBox().GetSize(Rml::BoxArea::Content);
                /* The title is rasterised in the card... */
                if (bright(title_at.x, title_at.y, title_at.x + title_size.x, title_at.y + title_size.y) < (dp >= 1.0f ? 40 : 6)) {
                    fprintf(stderr, "%s: title %zu of '%s' not drawn\n", tag.c_str(), i, pages[page]); assert(0);
                }
                /* ...and not between this card and the next one of its row (the focused card grows a white frame there). */
                if (i + 1 < cards.size() && card != host.m_context->GetFocusElement() && cards[i + 1] != host.m_context->GetFocusElement()) {
                    const Rml::Vector2f next = cards[i + 1]->GetAbsoluteOffset(Rml::BoxArea::Border);
                    const float gap = next.x - (at.x + size.x);
                    if (next.y == at.y && gap >= 4.0f)
                        assert(!bright(at.x + size.x + gap * 0.3f, title_at.y, at.x + size.x + gap * 0.7f, title_at.y + title_size.y));
                }
            }
            /* One-line strips stay one line and inside the screen. */
            for (const char* id : {"tube-pager", "tube-location", "tube-selected-title", "tube-selected-detail"}) {
                Rml::Element* strip = element(id);
                if (!strip->IsVisible(true)) continue;
                const Rml::Vector2f at = strip->GetAbsoluteOffset(Rml::BoxArea::Border), size = strip->GetBox().GetSize(Rml::BoxArea::Border);
                assert(at.x >= 0 && at.y >= 0 && at.x + size.x <= width + 0.5f && at.y + size.y <= height + 0.5f);
                if (strip->GetScrollHeight() > strip->GetClientHeight() + 1.0f) {
                    fprintf(stderr, "%s: #%s needs %.0f of %.0f pixels in height on page '%s'\n", tag.c_str(), id, strip->GetScrollHeight(), strip->GetClientHeight(), pages[page]);
                    assert(0);
                }
            }
            /* The key line may need a second line (a long language, a large interface scale): it grows upward,
             * stays inside the page and clear of the text above it. It never needs a third. */
            Rml::Element* footer_strip = element("tube-footer");
            Rml::Element* root = element("tube-root");
            const Rml::Vector2f footer_at = footer_strip->GetAbsoluteOffset(Rml::BoxArea::Border), footer_size = footer_strip->GetBox().GetSize(Rml::BoxArea::Border);
            const float root_bottom = root->GetAbsoluteOffset(Rml::BoxArea::Border).y + root->GetBox().GetSize(Rml::BoxArea::Border).y;
            Rml::Element* note = element("tube-selected-overview");
            assert(footer_strip->IsVisible(true) && std::abs(footer_at.y + footer_size.y - root_bottom) < 1.0f && root_bottom <= height + 0.5f);
            if (footer_size.y < 39.5f * dp || footer_size.y > 56.0f * dp) {
                fprintf(stderr, "%s: the key line is %.1f dp high on page '%s'\n", tag.c_str(), footer_size.y / dp, pages[page]); assert(0);
            }
            assert(footer_strip->GetScrollHeight() <= footer_strip->GetClientHeight() + 1.0f);
            assert(!note->IsVisible(true) || note->GetAbsoluteOffset(Rml::BoxArea::Border).y + note->GetBox().GetSize(Rml::BoxArea::Border).y <= footer_at.y + 0.5f);
            if (footer_size.y > 41.0f * dp) ++wrapped;
            ++checked;
        }
    };

    /* The defaults: every language. */
    const Look plain = {10, 5, 0, 0, 0};
    const English english = english_pages();
    assert(english.pager[0] == "PAGE 1 OF 2" && EVO_PROVIDER_PAGE_SIZE == 8);
    /* English keeps the one line and the size it always had. */
    assert(std::abs(element("tube-footer")->GetBox().GetSize(Rml::BoxArea::Border).y - 40.0f) < 0.01f && element("tube-footer")->GetAbsoluteOffset(Rml::BoxArea::Border).y == 1002.0f);
    int languages = 0;
    for (int index = 0; index < tube_i18n_language_count(); ++index, ++languages) check_language(tube_i18n_language_tag(index), english, plain);
    assert(languages >= 21);
    /* Scripts drawn: Latin with its marks, Cyrillic, Greek and Vietnamese. */
    assert(letters.count(U'ñ') && letters.count(U'ß') && letters.count(U'ł') && letters.count(U'ő') && letters.count(U'ș') && letters.count(U'ğ'));
    assert(letters.count(U'ж') && letters.count(U'ї') && letters.count(U'λ') && letters.count(U'ệ'));

    /* Interface language together with the interface preferences: scales, card text and colour schemes. */
    struct Combo { const char* tag; Look look; };
    static const Combo combos[] = {
        {"de", {18, 5, 0, 0, 0}},      /* German at 1.4x, the largest interface */
        {"ru", {10, 3, 1, 0, 7}},      /* Russian with two title lines, cards at 0.8x, the red scheme */
        {"el", {6, 5, 3, 1, 9}},       /* Greek at 0.8x with four title lines and two subtitle lines, teal on true black */
        {"vi", {0, 0, 0, 0, 3}},       /* Vietnamese at 0.4x with the smallest cards, the High contrast theme */
        {"uk", {14, 12, 1, 1, 10}},    /* Ukrainian at 1.2x with the largest cards, monochrome */
    };
    for (const Combo& combo : combos) {
        dress(combo.look);
        const English dressed = english_pages();
        tube_ui_layout_t layout; tube_ui_layout(tube_ui_prefs(), &layout);
        assert(host.m_tube_styled && std::abs(host.m_context->GetDensityIndependentPixelRatio() - (float)layout.scale) < 0.001f);
        assert((int)EVO_PROVIDER_PAGE_SIZE == layout.columns * layout.rows);
        check_language(combo.tag, dressed, combo.look);
        /* A scheme paints the page in its own background, the key line included: nothing keeps the default colour. */
        if (combo.look.scheme) {
            const unsigned background = g_tube_schemes[combo.look.scheme].background;
            const Rml::Vector2f at = element("tube-footer")->GetAbsoluteOffset(Rml::BoxArea::Border);
            for (const uint32_t pixel : {frame[(size_t)5 * width + 5], frame[(size_t)(at.y + 4) * width + (size_t)at.x + 1]})
                assert((pixel & 255) == (background >> 16 & 255) && (pixel >> 8 & 255) == (background >> 8 & 255) && (pixel >> 16 & 255) == (background & 255));
        }
    }
    dress(plain);
    const English again = english_pages();
    assert(again.footer == english.footer && again.pager[0] == english.pager[0] && EVO_PROVIDER_PAGE_SIZE == 8);

    /* The release-notes page: its title and its hint are the interface's, the notes are the developer's words. */
    assert(!tube_i18n_set_preference("es"));
    host.m_stack = {"description:update"}; host.m_crumbs = {"Page"}; host.m_pos_stack.clear(); host.m_page = 0;
    host.RequestPage("description:update", 0); pump();
    assert(!model.loading && !host.m_all_rows.empty() && host.m_all_rows[0].id == "description:text:update");
    assert(host.m_all_rows[0].title == translated("What is new in ProsperoTube 0.2.0", 0).c_str() && host.m_all_rows[0].title != "What is new in ProsperoTube 0.2.0");
    assert(host.m_all_rows[0].title.find("ProsperoTube 0.2.0") != std::string::npos);
    assert(host.m_all_rows[0].subtitle == translated("Release notes; L2 / R2 scroll", 0).c_str() && host.m_all_rows[0].subtitle != "Release notes; L2 / R2 scroll");
    assert(host.m_all_rows[0].overview == "Subscriptions" && translated("Subscriptions", 0) != "Subscriptions");

    /* The language can change while a page is open: static texts and the next page follow at once. */
    assert(!tube_i18n_set_preference("en")); show("");
    assert(footer_text() == english.footer && host.m_all_rows[0].title == english.rows[0][0].title);
    assert(!tube_i18n_set_preference("de")); pump();
    assert(footer_text() != english.footer && footer_text().find(translated("Search", 0)) != std::string::npos);
    /* An error line of the provider is shown in the language; a message from the service is shown as it came. */
    host.SetStatus("Select a listed language.", true); assert(model.status == translated("Select a listed language.", 0).c_str() && model.status != "Select a listed language.");
    host.SetStatus("YouTube said: quota exceeded", true); assert(model.status == "YouTube said: quota exceeded");
    host.SetStatus("", false);
    assert(!tube_i18n_set_preference("")); pump(); assert(footer_text() == english.footer);

    host.m_doc->Close(); Rml::RemoveContext("i18n-ui"); host.m_context = nullptr; host.m_doc = nullptr; host.m_model_handle = {};
    Rml::Shutdown(); evo_provider_youtube.shutdown();
    for (FT_Face face : faces) FT_Done_Face(face);
    FT_Done_FreeType(library);
    unlink(evo_data_path("youtube.json")); unlink(evo_data_path("interface-language")); rmdir(directory);
    printf("PASS: %d languages draw the main menu and the preferences, five of them again with other scales, card text and colour schemes; "
           "%zu different letters, all with glyphs; the key line takes two lines on %d of %d pages\n", languages, letters.size(), wrapped, checked);
}

extern "C" int evo_net_callback_error(void) { return 0; }
