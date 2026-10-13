// Interface language on the real provider host: the shipping document as
// prepare_native.py marks it, the app's fonts, pinned RmlUi and the CPU
// rasteriser. Every catalog draws the main menu and the preferences: the rows
// and the static texts change language, every letter has a glyph in a loaded
// face, the titles are rasterised inside their cards and no line outgrows its
// place. A row of YouTube content keeps its text.
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
        if (ok) rows.push_back(video);
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

    /* English first: what the two pages say, to compare every language against. */
    static const char* const pages[] = {"", "settings"};
    std::vector<std::vector<EvoProviderRow>> english;
    std::vector<std::string> english_pager;
    for (const char* page : pages) { show(page); english.push_back(host.m_all_rows); english_pager.push_back(model.page_info.c_str()); }
    show("");
    const std::string english_footer = footer_text();
    assert(english_footer.find("Watch / open") != std::string::npos && english_footer.find("Search") != std::string::npos);
    assert(english[0].back().title == "Subscriptions" && english_pager[0] == "PAGE 1 OF 2");
    /* English keeps the one line and the size it always had. */
    assert(std::abs(element("tube-footer")->GetBox().GetSize(Rml::BoxArea::Border).y - 40.0f) < 0.01f && element("tube-footer")->GetAbsoluteOffset(Rml::BoxArea::Border).y == 1002.0f);

    int languages = 0, widest_footer = 0, wrapped = 0; std::string widest;
    std::set<char32_t> letters;
    for (int index = 0; index < tube_i18n_language_count(); ++index) {
        const std::string tag = tube_i18n_language_tag(index);
        assert(!tube_i18n_set_preference(tag.c_str()));
        for (size_t page = 0; page < 2; ++page) {
            show(pages[page]);
            const auto& rows = host.m_all_rows; assert(rows.size() == english[page].size());
            int changed = 0;
            for (size_t i = 0; i + 1 < rows.size(); ++i) {
                assert(rows[i].title == translated(english[page][i].title.c_str(), TUBE_I18N_LABELS).c_str());
                assert(rows[i].subtitle == translated(english[page][i].subtitle.c_str(), TUBE_I18N_LABELS).c_str());
                changed += rows[i].title != english[page][i].title;
            }
            assert(changed * 10 >= ((int)rows.size() - 1) * 8);       /* a few names (Shorts, Trending) are the same in some languages */
            /* The video keeps its title, channel and description although the catalog knows those words. */
            assert(rows.back().title == "Subscriptions" && rows.back().subtitle == "History" && rows.back().overview == "Watch later");
            assert(translated("Subscriptions", 0) != "Subscriptions");

            /* Every letter on screen has a glyph: nothing is drawn as an empty box. */
            std::vector<std::string> texts; visible_texts(host.m_doc, texts); assert(texts.size() > 20);
            for (const std::string& text : texts) for (char32_t code : decode(text)) {
                if (code == U' ' || code == U'\n' || code == 0xA0) continue;
                if (!has_glyph(code)) { fprintf(stderr, "%s: no glyph for U+%04X in '%s'\n", tag.c_str(), (unsigned)code, text.c_str()); assert(0); }
                letters.insert(code);
            }
            /* Static texts of the document follow the language too. */
            const std::string footer = footer_text();
            assert(footer != english_footer && footer.find("Watch / open") == std::string::npos);
            assert(footer.find(translated("Watch / open", 0)) != std::string::npos && footer.find(translated("Back", 0)) != std::string::npos);
            assert(model.page_info == translated(english_pager[page], 0).c_str() && model.page_info != english_pager[page].c_str());

            /* Layout: a line that outgrows its place is cut by its own box, never drawn over a neighbour. */
            Rml::ElementList cards; host.m_doc->QuerySelectorAll(cards, ".tube-card"); assert(cards.size() >= 4);
            std::fill(frame.begin(), frame.end(), 0xFF101116u);
            renderer.SetFramebuffer(frame.data()); renderer.SetDimensions(width, height);
            host.m_context->Render();
            Rml::Element* grid = element("tube-grid");
            const float grid_bottom = grid->GetAbsoluteOffset(Rml::BoxArea::Border).y + grid->GetBox().GetSize(Rml::BoxArea::Border).y;
            int drawn = 0;
            for (size_t i = 0; i < cards.size(); ++i) {
                Rml::Element* card = cards[i];
                const Rml::Vector2f at = card->GetAbsoluteOffset(Rml::BoxArea::Border), size = card->GetBox().GetSize(Rml::BoxArea::Border);
                if (at.y + size.y > grid_bottom + 1.0f) continue;        /* a row below the visible grid */
                std::vector<Rml::Element*> lines = {card->QuerySelector(".tube-card-title"), card->QuerySelector(".tube-card-author")};
                if (!card->IsVisible(true) || !lines[0]) continue;         /* the template the rows are made from */
                assert(lines[1]);
                for (Rml::Element* line : lines) {
                    const Rml::Vector2f line_at = line->GetAbsoluteOffset(Rml::BoxArea::Border), line_size = line->GetBox().GetSize(Rml::BoxArea::Border);
                    assert(line_at.x >= at.x - 0.5f && line_at.x + line_size.x <= at.x + size.x + 0.5f);
                    assert(line_at.y >= at.y - 0.5f && line_at.y + line_size.y <= at.y + size.y + 0.5f);
                    assert(line->GetComputedValues().white_space() == Rml::Style::WhiteSpace::Nowrap && line->GetComputedValues().overflow_x() == Rml::Style::Overflow::Hidden);
                }
                const Rml::Vector2f title_at = lines[0]->GetAbsoluteOffset(Rml::BoxArea::Content), title_size = lines[0]->GetBox().GetSize(Rml::BoxArea::Content);
                /* The title is rasterised in the card... */
                if (bright(title_at.x, title_at.y, title_at.x + title_size.x, title_at.y + title_size.y) >= 40) ++drawn;
                else { fprintf(stderr, "%s: title %zu of '%s' not drawn\n", tag.c_str(), i, pages[page]); assert(0); }
                /* ...and not in the gap to the next card (the focused card grows a white frame there). */
                if (card != host.m_context->GetFocusElement())
                    assert(!bright(at.x + size.x + 5, title_at.y, at.x + size.x + 12, title_at.y + title_size.y));
            }
            assert(drawn >= 4);
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
            /* The key line may need a second line in a long language: it grows upward, stays on screen and clear of the text above it. */
            Rml::Element* footer_strip = element("tube-footer");
            const Rml::Vector2f footer_at = footer_strip->GetAbsoluteOffset(Rml::BoxArea::Border), footer_size = footer_strip->GetBox().GetSize(Rml::BoxArea::Border);
            Rml::Element* note = element("tube-selected-overview");
            assert(footer_strip->IsVisible(true) && footer_at.y + footer_size.y <= height - 37.5f && footer_size.y >= 39.5f && footer_size.y <= 70.0f);
            assert(footer_strip->GetScrollHeight() <= footer_strip->GetClientHeight() + 1.0f);
            assert(!note->IsVisible(true) || note->GetAbsoluteOffset(Rml::BoxArea::Border).y + note->GetBox().GetSize(Rml::BoxArea::Border).y <= footer_at.y + 0.5f);
            if (footer_size.y > 41.0f) ++wrapped;
            float used = 0;
            for (int i = 0; i < footer_strip->GetNumChildren(); ++i) {
                Rml::Element* child = footer_strip->GetChild(i);
                used = std::max(used, child->GetAbsoluteOffset(Rml::BoxArea::Border).x + child->GetBox().GetSize(Rml::BoxArea::Border).x);
            }
            if ((int)used > widest_footer) { widest_footer = (int)used; widest = tag; }
        }
        ++languages;
    }
    assert(languages >= 21);
    /* Scripts drawn: Latin with its marks, Cyrillic, Greek and Vietnamese. */
    assert(letters.count(U'ñ') && letters.count(U'ß') && letters.count(U'ł') && letters.count(U'ő') && letters.count(U'ș') && letters.count(U'ğ'));
    assert(letters.count(U'ж') && letters.count(U'ї') && letters.count(U'λ') && letters.count(U'ệ'));

    /* The language can change while a page is open: static texts and the next page follow at once. */
    assert(!tube_i18n_set_preference("en")); show("");
    assert(footer_text() == english_footer && host.m_all_rows[0].title == english[0][0].title);
    assert(!tube_i18n_set_preference("de")); pump();
    assert(footer_text() != english_footer && footer_text().find(translated("Search", 0)) != std::string::npos);
    /* An error line of the provider is shown in the language; a message from the service is shown as it came. */
    host.SetStatus("Select a listed language.", true); assert(model.status == translated("Select a listed language.", 0).c_str() && model.status != "Select a listed language.");
    host.SetStatus("YouTube said: quota exceeded", true); assert(model.status == "YouTube said: quota exceeded");
    host.SetStatus("", false);
    assert(!tube_i18n_set_preference("")); pump(); assert(footer_text() == english_footer);

    host.m_doc->Close(); Rml::RemoveContext("i18n-ui"); host.m_context = nullptr; host.m_doc = nullptr; host.m_model_handle = {};
    Rml::Shutdown(); evo_provider_youtube.shutdown();
    for (FT_Face face : faces) FT_Done_Face(face);
    FT_Done_FreeType(library);
    unlink(evo_data_path("youtube.json")); unlink(evo_data_path("interface-language")); rmdir(directory);
    printf("PASS: %d languages draw the main menu and the preferences; %zu different letters, all with glyphs; the key line takes two lines on %d of %d pages\n",
           languages, letters.size(), wrapped, languages * 2);
    (void)widest_footer;
}
