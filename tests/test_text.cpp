// ProsperoTube - Text engine tests: characters, fonts, shaping, layout, the atlas and its budget.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: test_text <scratch folder>
// The fonts are this PC's (tests/text_host.hpp). Exact glyph numbers and advances are those of
// DejaVu Sans 2.37 (sha256 b4c632e3...419c of DejaVuSans.ttf); they were checked once against the
// distribution's own HarfBuzz 12.3.2, a different version from the pinned 9.0.0 the engine uses.

#include "text_host.hpp"
#include "tube_text.hpp"
#include "tube_text_atlas.hpp"
#include "tube_text_fonts.hpp"
#include "tube_text_layout.hpp"
#include "tube_text_unicode.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace tube::text;

namespace
{

int g_checks = 0;
int g_failed = 0;
int g_skipped = 0;
std::string g_scratch;

#define CHECK(condition)                                                              \
    do                                                                                \
    {                                                                                 \
        ++g_checks;                                                                   \
        if (!(condition))                                                             \
        {                                                                             \
            ++g_failed;                                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);          \
        }                                                                             \
    } while (0)

#define CHECK_EQ(actual, expected)                                                    \
    do                                                                                \
    {                                                                                 \
        ++g_checks;                                                                   \
        const long long actual_value = static_cast<long long>(actual);                \
        const long long expected_value = static_cast<long long>(expected);            \
        if (actual_value != expected_value)                                           \
        {                                                                             \
            ++g_failed;                                                               \
            std::printf("FAIL %s:%d: %s is %lld, expected %lld (%s)\n", __FILE__, __LINE__, #actual, actual_value, \
                        expected_value, #expected);                                   \
        }                                                                             \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                       \
    do                                                                                \
    {                                                                                 \
        ++g_checks;                                                                   \
        const double actual_value = static_cast<double>(actual);                      \
        const double expected_value = static_cast<double>(expected);                  \
        if (!(std::fabs(actual_value - expected_value) <= (tolerance)))               \
        {                                                                             \
            ++g_failed;                                                               \
            std::printf("FAIL %s:%d: %s is %.4f, expected %.4f (%s)\n", __FILE__, __LINE__, #actual, actual_value, \
                        expected_value, #expected);                                   \
        }                                                                             \
    } while (0)

bool skip(const char *test, bool have, const char *what)
{
    if (have)
        return false;
    ++g_skipped;
    std::printf("SKIP %s: needs %s\n", test, what);
    return true;
}

std::string utf8(const std::u32string &text)
{
    std::string out;
    for (const char32_t c : text)
    {
        if (c < 0x80)
            out += static_cast<char>(c);
        else if (c < 0x800)
        {
            out += static_cast<char>(0xc0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3f));
        }
        else if (c < 0x10000)
        {
            out += static_cast<char>(0xe0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        }
        else
        {
            out += static_cast<char>(0xf0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (c & 0x3f));
        }
    }
    return out;
}

// The engine's inside, for what the public interface does not show (glyph numbers, faces).
struct Engine
{
    Config config;
    std::vector<std::string> messages;
    Fonts fonts;
    Shaper shaper;

    explicit Engine(const Config &c) : config(c), fonts(config, &messages), shaper(config, &fonts)
    {
    }
    Shaped shape(const std::string &text, const Style &style = {}, const Constraints &constraints = {})
    {
        Shaped shaped;
        shaper.shape(text, style, constraints, &shaped);
        return shaped;
    }
    std::uint32_t glyph(int face, char32_t c)
    {
        hb_codepoint_t id = 0;
        hb_font_get_nominal_glyph(fonts.shaper(face), c, &id);
        return id;
    }
    bool said(const char *part) const
    {
        for (const std::string &message : messages)
            if (message.find(part) != std::string::npos)
                return true;
        return false;
    }
};

Config dejavu_only()
{
    Config config;
    config.faces.push_back({text_host::dejavu(), 0, FaceRole::latin, false});
    config.raster_ms_per_frame = 0.0; // the count alone: the same on every machine
    return config;
}

Style at(float size)
{
    Style style;
    style.size = size;
    return style;
}

std::vector<std::uint32_t> ids(const Shaped &shaped)
{
    std::vector<std::uint32_t> out;
    for (const ShapedGlyph &glyph : shaped.glyphs)
        out.push_back(glyph.glyph);
    return out;
}

std::string line_text(const std::string &text, const ShapedLine &line)
{
    return text.substr(line.text_begin, line.text_end - line.text_begin);
}

std::vector<std::uint8_t> read_file(const std::string &path)
{
    std::vector<std::uint8_t> data;
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return data;
    std::uint8_t buffer[65536];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        data.insert(data.end(), buffer, buffer + got);
    std::fclose(file);
    return data;
}

bool write_file(const std::string &path, const std::uint8_t *data, std::size_t size)
{
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool ok = size == 0 || std::fwrite(data, 1, size, file) == size;
    std::fclose(file);
    return ok;
}

// ---- characters ----

std::vector<char32_t> decoded(const std::string &text, std::uint32_t limit = 1000000, bool *cut = nullptr,
                              std::vector<std::uint32_t> *offsets = nullptr)
{
    std::vector<char32_t> cps;
    std::vector<std::uint32_t> local;
    bool was_cut = false;
    uni::decode(text, limit, &cps, offsets != nullptr ? offsets : &local, &was_cut);
    if (cut != nullptr)
        *cut = was_cut;
    return cps;
}

void test_decode()
{
    CHECK(decoded("abc") == (std::vector<char32_t>{'a', 'b', 'c'}));
    CHECK(decoded("\xc3\xa9\xe2\x82\xac\xf0\x9f\x94\xa5") == (std::vector<char32_t>{0xe9, 0x20ac, 0x1f525}));
    // Overlong forms, surrogates and values past U+10FFFF: one U+FFFD per byte that starts nothing.
    CHECK(decoded("\xc0\x80") == (std::vector<char32_t>{0xfffd, 0xfffd}));
    CHECK(decoded("\xe0\x80\x80") == (std::vector<char32_t>{0xfffd, 0xfffd, 0xfffd}));
    CHECK(decoded("\xed\xa0\x80") == (std::vector<char32_t>{0xfffd, 0xfffd, 0xfffd}));
    CHECK(decoded("\xf4\x90\x80\x80") == (std::vector<char32_t>{0xfffd, 0xfffd, 0xfffd, 0xfffd}));
    CHECK(decoded("\xf8\x88\x80\x80\x80") == (std::vector<char32_t>{0xfffd, 0xfffd, 0xfffd, 0xfffd, 0xfffd}));
    // A sequence cut short is one U+FFFD, and what follows is read.
    CHECK(decoded("\xe2\x82") == (std::vector<char32_t>{0xfffd}));
    CHECK(decoded("\xf0\x9f\x94") == (std::vector<char32_t>{0xfffd}));
    CHECK(decoded("a\xe2\x82z") == (std::vector<char32_t>{'a', 0xfffd, 'z'}));
    CHECK(decoded("\x80\xbf") == (std::vector<char32_t>{0xfffd, 0xfffd}));
    CHECK(decoded("\xff\xfe") == (std::vector<char32_t>{0xfffd, 0xfffd}));
    // Line ends become one line feed, a tab a space, other controls nothing.
    CHECK(decoded("a\r\nb\rc\nd") == (std::vector<char32_t>{'a', '\n', 'b', '\n', 'c', '\n', 'd'}));
    CHECK(decoded("a\tb") == (std::vector<char32_t>{'a', ' ', 'b'}));
    CHECK(decoded(std::string("a\0\x01\x1b\x7f" "b", 6)) == (std::vector<char32_t>{'a', 'b'}));
    CHECK(decoded("\xe2\x80\xa8") == (std::vector<char32_t>{'\n'}));
    CHECK(decoded("\xef\xbf\xbe\xef\xbf\xbf") .empty()); // noncharacters
    bool cut = false;
    std::vector<std::uint32_t> offsets;
    const std::vector<char32_t> some = decoded("a\xc3\xa9\x01\xe2\x82\xac" "b", 3, &cut, &offsets);
    CHECK(some == (std::vector<char32_t>{'a', 0xe9, 0x20ac}));
    CHECK(cut);
    CHECK(offsets == (std::vector<std::uint32_t>{0, 1, 4, 7}));
    decoded("abc", 3, &cut);
    CHECK(!cut);
    CHECK(decoded("").empty());
}

int clusters(const std::u32string &text)
{
    std::vector<std::uint8_t> starts;
    uni::grapheme_starts(text.data(), text.size(), &starts);
    return static_cast<int>(std::count(starts.begin(), starts.end(), 1));
}

void test_graphemes()
{
    CHECK_EQ(clusters(U"abc"), 3);
    CHECK_EQ(clusters(U"e\U00000301"), 1);
    CHECK_EQ(clusters(U"a\U00000300\U00000301\U00000302b"), 2);
    CHECK_EQ(clusters(U"\r\n"), 1);
    CHECK_EQ(clusters(U"\U0001F1EF\U0001F1F5\U0001F1FA\U0001F1F8"), 2);             // two flags
    CHECK_EQ(clusters(U"\U0001F1EF\U0001F1F5\U0001F1FA"), 2);                       // a flag and half of one
    CHECK_EQ(clusters(U"\U0001F468\U0000200D\U0001F469\U0000200D\U0001F467"), 1);           // a family
    CHECK_EQ(clusters(U"\U0001F44D\U0001F3FD"), 1);                                 // thumbs up, skin tone
    CHECK_EQ(clusters(U"\U00002764\U0000FE0F"), 1);                                         // heart, emoji presentation
    CHECK_EQ(clusters(U"1\U0000FE0F\U000020E3"), 1);                                        // keycap
    CHECK_EQ(clusters(U"\U0001F525\U0001F525"), 2);
    CHECK_EQ(clusters(U"\U00001112\U00001161\U000011AB"), 1);                                   // Hangul jamo: one syllable
    CHECK_EQ(clusters(U"\U0000D55C\U0000AE00"), 2);
    CHECK_EQ(clusters(U"\U00000915\U0000094D\U00000937"), 1);                                   // Devanagari conjunct ksha
    CHECK_EQ(clusters(U"\U00000915\U0000094D\U00000937\U0000093F"), 1);
    CHECK_EQ(clusters(U"\U00000938\U0000094D\U00000924\U0000094D\U00000930\U00000940"), 1);                 // stri
    CHECK_EQ(clusters(U"\U00000915\U00000915"), 2);
    CHECK_EQ(clusters(U"\U00000E17\U00000E35\U00000E48"), 1);                                   // Thai consonant, vowel, tone
    CHECK_EQ(clusters(U"\U00000E19\U00000E49\U00000E33"), 1);                                   // with sara am
    CHECK_EQ(clusters(U"\U00000E40\U00000E17"), 2);                                         // a leading vowel is its own
    CHECK_EQ(clusters(U"a\U0000200Db"), 2);
    CHECK_EQ(clusters(U"\U00000644\U00000627"), 2);                                         // lam, alef: joined by shaping only
    CHECK_EQ(clusters(U""), 0);
}

std::string breaks_of(const std::u32string &text)
{
    std::vector<std::uint8_t> starts;
    std::vector<std::uint8_t> breaks;
    uni::grapheme_starts(text.data(), text.size(), &starts);
    uni::line_breaks(text.data(), text.size(), starts, &breaks);
    std::string out;
    for (const std::uint8_t b : breaks)
        out += b == uni::strong_break ? '|' : b == uni::weak_break ? ':' : '.';
    return out;
}

void test_line_breaks()
{
    // One mark per character: '|' a line may end before it, ':' the same inside a word of an
    // unspaced script, '.' it may not.
    CHECK(breaks_of(U"ab cd") == "...|.");
    CHECK(breaks_of(U"ab  cd") == "....|.");
    CHECK(breaks_of(U"a-b") == "..|");
    CHECK(breaks_of(U"a -5") == "..|.");
    CHECK(breaks_of(U"ab, cd.") == ".." ".." "|..");
    CHECK(breaks_of(U"a\U000000A0b") == "...");                   // no-break space
    CHECK(breaks_of(U"a\U0000200Bb") == "..|");                   // zero-width space
    CHECK(breaks_of(U"(ab) cd") == "....." "|.");
    CHECK(breaks_of(U"a ( b") == "..|..");                    // not after an opening bracket, even with a space
    CHECK(breaks_of(U"Bonjour !") == ".........");            // not before the mark, even after a space
    CHECK(breaks_of(U"\U00006771\U00004EAC\U000030BF\U000030EF\U000030FC") == ".|||.");       // not before the long vowel mark
    CHECK(breaks_of(U"\U0000300C\U00006771\U00004EAC\U0000300D\U00003067\U00003059\U00003002") == "..|.||.");
    CHECK(breaks_of(U"\U00003042\U00003063\U0000305F") == "..|");                     // not before a small tsu
    CHECK(breaks_of(U"abc\U00006771") == "...|");
    CHECK(breaks_of(U"\U0000D55C\U0000AE00 \U0000D55C\U0000AE00") == "...|.");            // Korean breaks at its spaces
    // Thai: sa-wat-dee. Not after a word's first bare consonant, not after mai han-akat.
    CHECK(breaks_of(U"\U00000E2A\U00000E27\U00000E31\U00000E2A\U00000E14\U00000E35") == "....:.");
    // Not after a leading vowel, not before a following one.
    CHECK(breaks_of(U"\U00000E44\U00000E1B\U00000E21\U00000E32") == "..:.");
    CHECK(breaks_of(U"http://a.bc/de") == "............:.");
    CHECK(breaks_of(U"") == "");
}

// ---- fonts ----

void test_missing_faces()
{
    if (skip("missing faces", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    // A face that is not there is said and passed over.
    Config config;
    config.faces.push_back({g_scratch + "/no-such-font.ttf", 0, FaceRole::latin, false});
    config.faces.push_back({text_host::dejavu(), 0, FaceRole::other, false});
    config.font_dirs.push_back(g_scratch + "/no-such-folder");
    {
        Engine engine(config);
        const Shaped shaped = engine.shape("abc");
        CHECK_EQ(shaped.glyphs.size(), 3);
        CHECK_EQ(shaped.missing, 0);
        CHECK_EQ(shaped.glyphs[0].face, 1);
        CHECK_EQ(engine.fonts.primary(), 1);
        CHECK(engine.said("no-such-font.ttf cannot be opened"));
        CHECK(engine.said("no latin face of the console (SST-Medium.otf, SST-Roman.otf)"));
        CHECK(engine.said("no arabic face of the console"));
        CHECK(engine.said("no-such-folder cannot be listed"));
    }
    // Files that are no fonts: rubbish, an empty file, a font cut short. In both ways of reading.
    const std::vector<std::uint8_t> font = read_file(text_host::dejavu());
    std::vector<std::uint8_t> rubbish(5000);
    for (std::size_t i = 0; i < rubbish.size(); ++i)
        rubbish[i] = static_cast<std::uint8_t>(i * 7 + 3);
    CHECK(write_file(g_scratch + "/rubbish.ttf", rubbish.data(), rubbish.size()));
    CHECK(write_file(g_scratch + "/empty.ttf", nullptr, 0));
    CHECK(write_file(g_scratch + "/head-only.ttf", font.data(), 4096));
    CHECK(write_file(g_scratch + "/cut.ttf", font.data(), font.size() * 6 / 10));
    for (const IoMode mode : {IoMode::map, IoMode::stream})
    {
        Config broken;
        broken.io = mode;
        for (const char *name : {"/rubbish.ttf", "/empty.ttf", "/head-only.ttf", "/cut.ttf"})
            broken.faces.push_back({g_scratch + name, 0, FaceRole::latin, false});
        broken.faces.push_back({text_host::dejavu(), 0, FaceRole::latin, false});
        std::string error;
        std::unique_ptr<Context> context = Context::create(broken, &error);
        CHECK(context != nullptr);
        if (!context)
            continue;
        Layout layout;
        context->prewarm("The quick brown fox \xd0\x96\xd1\x8b \xce\xa9", at(40.0f));
        context->layout("The quick brown fox \xd0\x96\xd1\x8b \xce\xa9", at(40.0f), {}, &layout);
        CHECK(layout.width > 100.0f);
        CHECK(layout.quads.size() >= 18);
        bool rubbish_said = false;
        bool empty_said = false;
        for (const std::string &message : context->messages())
        {
            rubbish_said = rubbish_said || message.find("rubbish.ttf") != std::string::npos;
            empty_said = empty_said || message.find("empty.ttf is not a font file") != std::string::npos;
        }
        CHECK(rubbish_said);
        CHECK(empty_said);
    }
    // No face at all: every character is a box, nothing fails.
    {
        std::string error;
        std::unique_ptr<Context> context = Context::create(Config{}, &error);
        CHECK(context != nullptr);
        Layout layout;
        context->layout("abc", at(30.0f), {}, &layout);
        CHECK_EQ(layout.missing, 3);
        CHECK_EQ(layout.quads.size(), 3);
        CHECK_NEAR(layout.width, 3 * kBoxAdvance * 30.0f, 0.01);
        CHECK(!context->can_draw("abc"));
        CHECK(context->can_draw(""));
    }
    // A configuration that cannot work is refused with a reason.
    {
        Config bad;
        bad.page_size = 64;
        std::string error;
        CHECK(Context::create(bad, &error) == nullptr);
        CHECK(!error.empty());
        bad = Config{};
        bad.raster_sizes[0] = 0;
        CHECK(Context::create(bad, &error) == nullptr);
        bad = Config{};
        bad.raster_sizes[1] = 20;
        CHECK(Context::create(bad, &error) == nullptr);
    }
}

void test_system_folder()
{
    if (skip("system folder", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    // A folder laid out like the console's: a font under the console's name for the interface
    // face, another file nobody named, a text file. (Copies of DejaVu stand in; the names matter.)
    const std::string folder = g_scratch + "/preinst";
    const std::vector<std::uint8_t> font = read_file(text_host::dejavu());
    const std::vector<std::uint8_t> bold = read_file(text_host::dejavu("DejaVuSans-Bold.ttf"));
    // The folder is made by the test script; without it the test cannot run.
    if (!write_file(folder + "/SST-Roman.otf", font.data(), font.size()))
    {
        skip("system folder", false, "the scratch folder's preinst directory");
        return;
    }
    CHECK(write_file(folder + "/Zother.ttf", bold.data(), bold.size()));
    CHECK(write_file(folder + "/readme.txt", font.data(), 10));
    Config config;
    config.font_dirs.push_back(folder);
    Engine engine(config);
    CHECK_EQ(engine.fonts.count(), 2);
    CHECK(engine.fonts.role(0) == FaceRole::latin);
    CHECK(engine.fonts.path(0) == folder + "/SST-Roman.otf");
    CHECK(engine.fonts.role(1) == FaceRole::other);
    CHECK(engine.said("no japanese face of the console (SSTJpPro-Regular.otf, SIE-RDC-Pr5N-M-JPN.otf, SCE-RDC-R-JPN.otf)"));
    CHECK(!engine.said("no latin face"));
    const Shaped shaped = engine.shape("abc");
    CHECK_EQ(shaped.glyphs[0].face, 0);
    CHECK(engine.fonts.describe().find("SST-Roman.otf (latin, mapped") != std::string::npos);
    // The file nobody named is not read while the named ones have every character...
    CHECK_EQ(engine.fonts.open_count(), 1);
    // ... and without discovery it is not even known.
    config.discover = false;
    Engine plain(config);
    CHECK_EQ(plain.fonts.count(), 1);
    // The first name of a line wins when both are there.
    CHECK(write_file(folder + "/SST-Medium.otf", bold.data(), bold.size()));
    Engine both(config);
    CHECK(both.fonts.path(0) == folder + "/SST-Medium.otf");
    CHECK_EQ(both.fonts.count(), 1);
    const std::vector<std::string> folders = system_font_folders();
    CHECK(!folders.empty() && folders.back() == "/preinst/common/font");
}

void test_fallback_chain()
{
    if (skip("fallback chain", text_host::have_dejavu() && text_host::have_windows(), "DejaVu Sans and the Windows fonts"))
        return;
    // Faces: 0 DejaVu (latin), 1 Yu Gothic (japanese), 2 YaHei (chinese), 3 Malgun (korean),
    // 4 Leelawadee (thai), 5 Segoe UI (arabic), 6 Nirmala (other), 7 Segoe UI Symbol (other).
    const Config config = text_host::stand_in(text_host::dejavu());
    Engine engine(config);
    const auto face_of = [&](const std::string &text, std::size_t glyph)
    {
        const Shaped shaped = engine.shape(text);
        return glyph < shaped.glyphs.size() ? static_cast<int>(shaped.glyphs[glyph].face) : -99;
    };
    const auto all_in = [&](const std::string &text, int face)
    {
        const Shaped shaped = engine.shape(text);
        if (shaped.glyphs.empty() || shaped.missing != 0)
            return false;
        for (const ShapedGlyph &glyph : shaped.glyphs)
            if (glyph.face != face)
                return false;
        return true;
    };
    CHECK(all_in("Stra\xc3\x9f" "e caf\xc3\xa9", 0));
    CHECK(all_in("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", 0));               // Russian
    CHECK(all_in("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x82\xbf\xe3\x82\xa4\xe3\x83\x88\xe3\x83\xab", 1)); // kana: Japanese forms
    CHECK(all_in("\xe4\xb8\xad\xe6\x96\x87\xe6\xa0\x87\xe9\xa2\x98", 2));               // Han only: Chinese
    CHECK(all_in("\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", 3));                           // Hangul
    CHECK(all_in("\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4\xe6\xbc\xa2\xe5\xad\x97", 3));   // Han beside Hangul: Korean forms
    CHECK(all_in("\xe0\xb8\xaa\xe0\xb8\xa7\xe0\xb8\xb1\xe0\xb8\xaa\xe0\xb8\x94\xe0\xb8\xb5", 4)); // Thai
    CHECK(all_in("\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 \xd8\xa8\xd9\x83\xd9\x85", 5)); // Arabic, and the space between its words
    CHECK(all_in("\xe0\xa4\xa8\xe0\xa4\xae\xe0\xa4\xb8\xe0\xa5\x8d\xe0\xa4\xa4\xe0\xa5\x87", 6)); // Devanagari: no role, the first face that has it
    CHECK(all_in("\xf0\x9f\x94\xa5", 7));                                               // an emoji outline
    // Digits and Latin punctuation inside Japanese text stay in the interface face.
    const std::string episode = "\xe7\xac\xac" "3\xe8\xa9\xb1\xe3\x81\xa7\xe3\x81\x99"; // dai 3 wa desu
    CHECK_EQ(face_of(episode, 0), 1);
    CHECK_EQ(face_of(episode, 1), 0);
    CHECK_EQ(face_of(episode, 2), 1);
    // ... and in Chinese text too (the same Han, no kana: the Chinese face).
    CHECK_EQ(face_of("\xe7\xac\xac" "3\xe8\xa9\xb1", 0), 2);
    CHECK_EQ(face_of("\xe7\xac\xac" "3\xe8\xa9\xb1", 1), 0);
    // Japanese punctuation the interface face lacks comes from the Japanese face.
    CHECK_EQ(face_of("\xe3\x80\x8c\xe3\x81\x82\xe3\x80\x8d", 0), 1);
    // The interface language decides for Han alone.
    Config japanese = config;
    japanese.language = "ja-JP";
    Engine in_japanese(japanese);
    const Shaped tokyo = in_japanese.shape("\xe6\x9d\xb1\xe4\xba\xac");
    CHECK_EQ(tokyo.glyphs[0].face, 1);
    CHECK_EQ(engine.shape("\xe6\x9d\xb1\xe4\xba\xac").glyphs[0].face, 2);
    // ... unless that language's face lacks one of them: then one face that has them all.
    const Shaped simplified = in_japanese.shape("\xe8\xbf\x99\xe4\xb8\xaa\xe8\xa7\x86\xe9\xa2\x91"); // zhe ge shi pin
    CHECK_EQ(simplified.missing, 0);
    CHECK(simplified.glyphs[0].face == simplified.glyphs[3].face);
    // A script no face has: boxes, counted, one per user-perceived character.
    const Shaped tibetan = engine.shape("\xe0\xbd\x96\xe0\xbd\xbc\xe0\xbd\x91"); // ba, o, da
    CHECK_EQ(tibetan.missing, 2);
    CHECK_EQ(tibetan.glyphs.size(), 2);
    CHECK_EQ(tibetan.glyphs[0].face, -1);
    CHECK_EQ(tibetan.glyphs[0].glyph, 1);
    // A bold cut is asked for bold text only; faces without one are widened.
    Config with_bold = dejavu_only();
    with_bold.faces.push_back({text_host::dejavu("DejaVuSans-Bold.ttf"), 0, FaceRole::latin, true});
    with_bold.faces.push_back({text_host::windows("malgun.ttf"), 0, FaceRole::korean, false});
    Engine bold(with_bold);
    Style heavy;
    heavy.weight = Weight::bold;
    const Shaped regular = bold.shape("ab \xed\x95\x9c");
    const Shaped widened = bold.shape("ab \xed\x95\x9c", heavy);
    CHECK_EQ(regular.glyphs[0].face, 0);
    CHECK_EQ(regular.glyphs[0].bold, 0);
    CHECK_EQ(widened.glyphs[0].face, 1);
    CHECK_EQ(widened.glyphs[0].bold, 0);
    CHECK_EQ(widened.glyphs[3].face, 2);
    CHECK_EQ(widened.glyphs[3].bold, 1);
    CHECK_EQ(regular.glyphs[3].bold, 0);
}

// ---- shaping ----

void test_golden_dejavu()
{
    if (skip("golden shaping", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    if (hb_face_get_glyph_count(hb_font_get_face(engine.fonts.shaper(0))) != 6253)
    {
        skip("golden shaping", false, "DejaVu Sans 2.37 (this one has another glyph count)");
        return;
    }
    // The em at 2048 units is the font's own: positions are its design units.
    const Style units = at(2048.0f);
    Shaped shaped = engine.shape("AV fi", units);
    CHECK(ids(shaped) == (std::vector<std::uint32_t>{36, 57, 3, 5042})); // A, V, space, the fi ligature
    CHECK_NEAR(shaped.glyphs[1].x, 1270.0, 0.01);                       // A and V are kerned (1401 unkerned)
    CHECK_NEAR(shaped.glyphs[2].x, 2671.0, 0.01);
    CHECK_NEAR(shaped.glyphs[3].x, 3322.0, 0.01);
    CHECK_NEAR(shaped.width, 4612.0, 0.01);
    shaped = engine.shape("Hello", units);
    CHECK(ids(shaped) == (std::vector<std::uint32_t>{43, 72, 79, 79, 82}));
    CHECK_NEAR(shaped.width, 1540 + 1260 + 569 + 569 + 1253, 0.01);
    shaped = engine.shape("1:23", units);
    CHECK(ids(shaped) == (std::vector<std::uint32_t>{20, 29, 21, 22}));
    CHECK_NEAR(shaped.width, 1303 + 690 + 1303 + 1303, 0.01);
    // A letter and a combining accent become the accented letter.
    CHECK(ids(engine.shape("e\xcc\x81", units)) == (std::vector<std::uint32_t>{171}));
    CHECK(ids(engine.shape("\xc3\xa9", units)) == (std::vector<std::uint32_t>{171}));
    CHECK(ids(engine.shape("\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82", units)) ==
          (std::vector<std::uint32_t>{948, 981, 973, 967, 970, 983}));
    CHECK(ids(engine.shape("\xce\x95\xce\xbb\xce\xbb\xce\xac\xce\xb4\xce\xb1", units)) ==
          (std::vector<std::uint32_t>{811, 848, 848, 833, 841, 838}));
    // Arabic "salam": drawn right to left, the letters in their joining forms, lam and alef as one.
    shaped = engine.shape("\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", units);
    CHECK(ids(shaped) == (std::vector<std::uint32_t>{1390, 5366, 5293}));
    CHECK(shaped.rtl);
    CHECK_NEAR(shaped.width, 1268 + 1222 + 1716, 0.01);
    CHECK(shaped.glyphs[2].glyph != engine.glyph(0, 0x0633)); // seen is not in its isolated form
    CHECK(ids(engine.shape("\xd8\xa8\xd9\x8a\xd8\xaa", units)) == (std::vector<std::uint32_t>{5264, 5358, 5259}));
    // Hebrew does not join: the same glyphs, right to left.
    CHECK(ids(engine.shape("\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d", units)) == (std::vector<std::uint32_t>{1332, 1324, 1331, 1344}));
    // Line metrics of the face (hhea: 1901 up, 483 down, no gap).
    CHECK_NEAR(shaped.ascent, 1901.0, 0.5);
    CHECK_NEAR(shaped.descent, 483.0, 0.5);
    CHECK_NEAR(shaped.line_height, 2384.0, 1.0);
}

// The glyphs a string would have with one glyph per character, in the order given.
std::vector<std::uint32_t> nominal(Engine &engine, const std::u32string &text)
{
    std::vector<std::uint32_t> out;
    for (const char32_t c : text)
        out.push_back(engine.glyph(0, c));
    return out;
}

void test_visual_order()
{
    if (skip("visual order", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    // Each case: the text as typed, and the characters as they appear from left to right
    // (worked out by hand from UAX #9).
    struct Case
    {
        std::u32string logical;
        std::u32string visual;
        bool rtl;
    };
    const Case cases[] = {
        // Hebrew inside English: the Hebrew word alone is reversed.
        {U"abc \U000005D0\U000005D1\U000005D2 def", U"abc \U000005D2\U000005D1\U000005D0 def", false},
        // English and a number inside Hebrew: the paragraph runs right to left, they do not.
        {U"\U000005D0\U000005D1\U000005D2 abc 123", U"abc 123 \U000005D2\U000005D1\U000005D0", true},
        // A number inside Hebrew keeps its digits in order.
        {U"\U000005DE\U000005D7\U000005D9\U000005E8 100 \U000005E9\U000005E7\U000005DC", U"\U000005DC\U000005E7\U000005E9 100 \U000005E8\U000005D9\U000005D7\U000005DE", true},
        // Brackets around Hebrew are mirrored: the one typed last is drawn first.
        {U"(\U000005D0\U000005D1\U000005D2) abc", U"abc (\U000005D2\U000005D1\U000005D0)", true},
        // A full stop after English in a Hebrew paragraph goes to the left end.
        {U"\U000005D0\U000005D1 abc.", U".abc \U000005D1\U000005D0", true},
        // An explicit right-to-left mark makes an all-Latin paragraph right to left.
        {U"\U0000200Fabc!", U"!abc", true},
        {U"abc", U"abc", false},
    };
    for (const Case &c : cases)
    {
        const Shaped shaped = engine.shape(utf8(c.logical), at(20.0f));
        CHECK(ids(shaped) == nominal(engine, c.visual));
        CHECK_EQ(shaped.rtl, c.rtl);
        for (std::size_t i = 1; i < shaped.glyphs.size(); ++i)
            CHECK(shaped.glyphs[i].x > shaped.glyphs[i - 1].x);
    }
    // The direction can be imposed: the same glyph order for plain Latin, but the line belongs to
    // a right-to-left paragraph (and aligns to the right).
    Style forced = at(20.0f);
    forced.direction = Direction::rtl;
    Constraints box;
    box.max_width = 500.0f;
    const Shaped shaped = engine.shape("abc", forced, box);
    CHECK(shaped.rtl);
    CHECK(ids(shaped) == nominal(engine, U"abc"));
    CHECK_NEAR(shaped.lines[0].x, 500.0f - shaped.lines[0].width, 0.01);
    forced.direction = Direction::ltr;
    CHECK(!engine.shape("\xd7\x90\xd7\x91", forced).rtl);
}

void test_scripts()
{
    if (skip("scripts", text_host::have_dejavu() && text_host::have_windows(), "DejaVu Sans and the Windows fonts"))
        return;
    Engine engine(text_host::stand_in(text_host::dejavu()));
    const float size = 100.0f;
    // Thai: a consonant, the vowel above it and the tone above that are one cluster of three
    // glyphs; the marks take no room and the tone sits higher than the vowel.
    Shaped shaped = engine.shape("\xe0\xb8\x97\xe0\xb8\xb5\xe0\xb9\x88", at(size));
    CHECK_EQ(shaped.glyphs.size(), 3);
    CHECK_EQ(shaped.missing, 0);
    const Shaped consonant = engine.shape("\xe0\xb8\x97", at(size));
    CHECK_NEAR(shaped.width, consonant.width, 0.01);
    CHECK(shaped.glyphs[2].y < shaped.glyphs[1].y);
    // "nam": the sign sara am is taken apart, its ring goes above the consonant under the tone.
    shaped = engine.shape("\xe0\xb8\x99\xe0\xb9\x89\xe0\xb8\xb3", at(size));
    CHECK_EQ(shaped.glyphs.size(), 4);
    // Devanagari "ki": the vowel sign is typed after the consonant and drawn before it.
    const std::uint32_t ka = engine.glyph(6, 0x0915);
    shaped = engine.shape("\xe0\xa4\x95\xe0\xa4\xbf", at(size));
    CHECK_EQ(shaped.glyphs.size(), 2);
    CHECK_EQ(shaped.glyphs[1].glyph, ka);
    CHECK(shaped.glyphs[0].glyph != ka);
    // "ksha": two consonants and a virama become one conjunct glyph.
    shaped = engine.shape("\xe0\xa4\x95\xe0\xa5\x8d\xe0\xa4\xb7", at(size));
    CHECK_EQ(shaped.glyphs.size(), 1);
    // "rka": ra and virama become the hook above the consonant that follows.
    shaped = engine.shape("\xe0\xa4\xb0\xe0\xa5\x8d\xe0\xa4\x95", at(size));
    CHECK_EQ(shaped.glyphs.size(), 2);
    CHECK_EQ(shaped.glyphs[0].glyph, ka);
    // Hangul letters typed one by one are the syllable.
    const Shaped syllable = engine.shape("\xed\x95\x9c", at(size));
    shaped = engine.shape("\xe1\x84\x92\xe1\x85\xa1\xe1\x86\xab", at(size));
    CHECK_EQ(shaped.glyphs.size(), 1);
    CHECK_EQ(shaped.glyphs[0].glyph, syllable.glyphs[0].glyph);
    // Ideographs are one em each.
    shaped = engine.shape("\xe6\x9d\xb1\xe4\xba\xac", at(size));
    CHECK_EQ(shaped.glyphs.size(), 2);
    CHECK_NEAR(shaped.width, 2 * size, 0.01);
    // Arabic in the stand-in Arabic face joins too: the word is narrower than its letters apart.
    const Shaped joined = engine.shape("\xd8\xa8\xd9\x8a\xd8\xaa", at(size));
    const Shaped apart = engine.shape("\xd8\xa8 \xd9\x8a \xd8\xaa", at(size));
    CHECK_EQ(joined.glyphs.size(), 3);
    CHECK(joined.width < apart.width - size * 0.3f);
    // Persian with a zero-width non-joiner: nothing is drawn for it and the letters beside it do not join.
    shaped = engine.shape("\xd9\x85\xdb\x8c\xe2\x80\x8c\xd8\xaa\xd9\x88\xd8\xa7\xd9\x86", at(size));
    CHECK_EQ(shaped.missing, 0);
    CHECK_EQ(shaped.glyphs.size(), 6); // six letters, nothing for the non-joiner
}

void test_emoji()
{
    if (skip("emoji", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    // DejaVu Sans has no fire, no people, no flags; it has the heart U+2764 and the digit one.
    const std::string fire = "\xf0\x9f\x94\xa5";
    const std::string family = "\xf0\x9f\x91\xa8\xe2\x80\x8d\xf0\x9f\x91\xa9\xe2\x80\x8d\xf0\x9f\x91\xa7";
    const std::string flag = "\xf0\x9f\x87\xaf\xf0\x9f\x87\xb5";
    const std::string thumb = "\xf0\x9f\x91\x8d\xf0\x9f\x8f\xbd";
    {
        Engine engine(dejavu_only()); // the default: left out
        const Shaped plain = engine.shape("a b", at(40.0f));
        for (const std::string &text : {"a " + fire + " b", "a " + fire + fire + " b", "a " + family + " b",
                                        "a " + flag + thumb + " b", fire + " a b", "a b " + fire, fire + "a b" + fire})
        {
            const Shaped shaped = engine.shape(text, at(40.0f));
            CHECK_EQ(shaped.missing, 0);
            CHECK(ids(shaped) == ids(plain));
            CHECK_NEAR(shaped.width, plain.width, 0.01);
        }
        // A text of emoji only is an empty line, not a failure.
        const Shaped none = engine.shape(fire + family, at(40.0f));
        CHECK_EQ(none.lines.size(), 1);
        CHECK_EQ(none.glyphs.size(), 0);
        CHECK_NEAR(none.width, 0.0, 0.001);
        // A symbol the face has stays, whatever selector follows it; the selector draws nothing.
        const Shaped heart = engine.shape("\xe2\x9d\xa4\xef\xb8\x8f", at(40.0f));
        CHECK_EQ(heart.glyphs.size(), 1);
        CHECK_EQ(heart.glyphs[0].glyph, engine.glyph(0, 0x2764));
        CHECK_EQ(heart.missing, 0);
        // A keycap: the digit stays whether or not the face has the enclosing key.
        const Shaped keycap = engine.shape("1\xef\xb8\x8f\xe2\x83\xa3", at(40.0f));
        CHECK_EQ(keycap.missing, 0);
        CHECK(!keycap.glyphs.empty());
        CHECK_EQ(keycap.glyphs[0].glyph, engine.glyph(0, '1'));
        // Joiners and selectors alone, and direction marks, draw nothing and are not missing.
        const Shaped invisible = engine.shape("a\xe2\x80\x8d\xef\xb8\x8f\xe2\x80\x8e\xc2\xad" "b", at(40.0f));
        CHECK_EQ(invisible.missing, 0);
        CHECK(ids(invisible) == ids(engine.shape("ab", at(40.0f))));
        // An ideographic variation selector after a letter is no box either.
        CHECK_EQ(engine.shape("a\xf3\xa0\x84\x80", at(40.0f)).missing, 0);
    }
    {
        Config config = dejavu_only();
        config.emoji = EmojiPolicy::placeholder; // one dot per sequence
        Engine engine(config);
        const auto dots = [&](const std::string &text)
        {
            const Shaped shaped = engine.shape(text, at(40.0f));
            int count = 0;
            for (const ShapedGlyph &glyph : shaped.glyphs)
                count += glyph.face == -1 && glyph.glyph == 2 ? 1 : 0;
            return shaped.missing == 0 ? count : -1;
        };
        CHECK_EQ(dots(fire), 1);
        CHECK_EQ(dots(fire + fire), 2);
        CHECK_EQ(dots(family), 1);
        CHECK_EQ(dots(flag), 1);
        CHECK_EQ(dots(thumb), 1);
        CHECK_EQ(dots("a " + fire + " b"), 1);
        CHECK_NEAR(engine.shape(fire, at(40.0f)).width, kDotAdvance * 40.0f, 0.01);
        // The line still tells which bytes it shows.
        const std::string text = "a " + family + " b";
        const Shaped shaped = engine.shape(text, at(40.0f));
        CHECK(line_text(text, shaped.lines[0]) == text);
    }
    if (!skip("emoji outlines", text_host::have_windows(), "the Windows fonts"))
    {
        // A face with outline emoji draws them: no dot, no box, no gap.
        Engine engine(text_host::stand_in(text_host::dejavu()));
        for (const std::string &text : {fire, family, flag, thumb})
        {
            const Shaped shaped = engine.shape(text, at(40.0f));
            CHECK_EQ(shaped.missing, 0);
            CHECK(!shaped.glyphs.empty());
            for (const ShapedGlyph &glyph : shaped.glyphs)
                CHECK(glyph.face >= 0);
        }
        // A colour bitmap or colour-layer font is not what draws them when an outline face comes first,
        // and a font that is colour bitmaps only is refused outright.
        Config colour = dejavu_only();
        colour.faces.push_back({text_host::windows("seguiemj.ttf"), 0, FaceRole::other, false});
        if (text_host::exists(text_host::windows("seguiemj.ttf")))
        {
            Engine with_colour(colour);
            const Shaped shaped = with_colour.shape(fire, at(40.0f));
            CHECK_EQ(shaped.missing, 0);
        }
    }
}

// ---- layout ----

void test_ellipsis()
{
    if (skip("ellipsis", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    const std::uint32_t mark = engine.glyph(0, 0x2026);
    const std::string text = "The quick brown fox jumps over the lazy dog";
    const Style style = at(20.0f);
    const float full = engine.shape(text, style).width;
    const float mark_width = engine.shape("\xe2\x80\xa6", style).width;
    Constraints constraints;
    constraints.max_width = full * 0.5f;
    // No mark asked for: the line runs past the width and nothing is reported cut.
    Shaped shaped = engine.shape(text, style, constraints);
    CHECK_EQ(shaped.lines.size(), 1);
    CHECK_NEAR(shaped.width, full, 0.01);
    CHECK(!shaped.truncated);
    CHECK(!shaped.lines[0].ellipsis);
    // Cut at the end.
    constraints.ellipsis = Ellipsis::end;
    shaped = engine.shape(text, style, constraints);
    CHECK_EQ(shaped.lines.size(), 1);
    CHECK(shaped.truncated);
    CHECK(shaped.lines[0].ellipsis);
    CHECK(shaped.width <= constraints.max_width + 0.02f);
    CHECK(shaped.width > constraints.max_width - 16.0f); // as much as fits: less than one more letter is left
    CHECK_EQ(shaped.glyphs.back().glyph, mark);
    CHECK_EQ(shaped.lines[0].text_begin, 0);
    CHECK(shaped.lines[0].text_end > 10 && shaped.lines[0].text_end < text.size());
    const std::string kept = line_text(text, shaped.lines[0]);
    CHECK(kept.back() != ' ');
    CHECK_NEAR(shaped.width, engine.shape(kept, style).width + mark_width, 0.05);
    // Cut at the start.
    constraints.ellipsis = Ellipsis::start;
    shaped = engine.shape(text, style, constraints);
    CHECK(shaped.truncated);
    CHECK(shaped.width <= constraints.max_width + 0.02f);
    CHECK(shaped.width > constraints.max_width - 16.0f);
    CHECK_EQ(shaped.glyphs.front().glyph, mark);
    CHECK_EQ(shaped.lines[0].text_end, text.size());
    CHECK(shaped.lines[0].text_begin > 0);
    CHECK(line_text(text, shaped.lines[0]).front() != ' ');
    // A text that fits exactly is not cut.
    constraints.ellipsis = Ellipsis::end;
    constraints.max_width = full;
    shaped = engine.shape(text, style, constraints);
    CHECK(!shaped.truncated);
    CHECK_NEAR(shaped.width, full, 0.01);
    // Hardly any room: the mark alone, and it never fails.
    for (const float width : {0.5f, mark_width - 1.0f, mark_width + 0.5f})
    {
        constraints.max_width = width;
        shaped = engine.shape(text, style, constraints);
        CHECK_EQ(shaped.glyphs.size(), 1);
        CHECK_EQ(shaped.glyphs[0].glyph, mark);
        CHECK_EQ(shaped.lines[0].text_end, shaped.lines[0].text_begin);
    }
    // A right-to-left paragraph is cut at its end too: the mark is at the left, the line at the right.
    const std::string hebrew = utf8(U"\U000005E9\U000005DC\U000005D5\U000005DD \U000005E2\U000005D5\U000005DC\U000005DD \U000005E9\U000005DC\U000005D5\U000005DD \U000005E2\U000005D5\U000005DC\U000005DD \U000005E9\U000005DC\U000005D5\U000005DD");
    const float hebrew_full = engine.shape(hebrew, style).width;
    constraints.max_width = hebrew_full * 0.6f;
    shaped = engine.shape(hebrew, style, constraints);
    CHECK(shaped.rtl && shaped.truncated);
    CHECK_EQ(shaped.glyphs.front().glyph, mark);
    CHECK_NEAR(shaped.lines[0].x + shaped.lines[0].width, constraints.max_width, 0.02);
    CHECK_EQ(shaped.lines[0].text_begin, 0);
    // The first word typed is the rightmost one drawn: its first letter, shin, is the last glyph.
    CHECK_EQ(shaped.glyphs.back().glyph, engine.glyph(0, 0x05E9));
    // ... and cut at the start, the mark is at the right.
    constraints.ellipsis = Ellipsis::start;
    shaped = engine.shape(hebrew, style, constraints);
    CHECK_EQ(shaped.glyphs.back().glyph, mark);
    CHECK_EQ(shaped.lines[0].text_end, hebrew.size());
    // A mark never splits a letter from its accents.
    const std::string accents = utf8(U"a\U00000301\U00000302\U00000303a\U00000301\U00000302\U00000303a\U00000301\U00000302\U00000303a\U00000301\U00000302\U00000303");
    constraints.ellipsis = Ellipsis::end;
    constraints.max_width = engine.shape(accents, style).width * 0.7f;
    shaped = engine.shape(accents, style, constraints);
    CHECK(shaped.truncated);
    CHECK_EQ(shaped.lines[0].text_end % 7, 0); // each letter with its three accents is seven bytes
    // A face without the ellipsis character cuts with three full stops.
    if (text_host::have_windows())
    {
        Config thai;
        thai.faces.push_back({text_host::windows("msgothic.ttc"), 0, FaceRole::latin, false});
        Engine other(thai);
        if (other.fonts.usable(0) && !other.fonts.has(0, 0x2026))
        {
            constraints.max_width = other.shape(text, style).width * 0.5f;
            shaped = other.shape(text, style, constraints);
            CHECK_EQ(shaped.glyphs.back().glyph, other.glyph(0, '.'));
            CHECK(shaped.width <= constraints.max_width + 0.02f);
        }
    }
}

void test_wrap()
{
    if (skip("wrap", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    const Style style = at(20.0f);
    const auto lines_of = [&](const std::string &text, const Constraints &constraints)
    {
        const Shaped shaped = engine.shape(text, style, constraints);
        std::vector<std::string> lines;
        for (const ShapedLine &line : shaped.lines)
            lines.push_back(line_text(text, line));
        return lines;
    };
    Constraints wrap;
    wrap.max_lines = 0;
    wrap.max_width = engine.shape("aaa bbb", style).width + 1.0f;
    CHECK(lines_of("aaa bbb ccc ddd", wrap) == (std::vector<std::string>{"aaa bbb", "ccc ddd"}));
    // The space a line ends with hangs past the edge: it neither counts nor starts the next line.
    wrap.max_width = engine.shape("aaa bbb", style).width + 0.005f;
    CHECK(lines_of("aaa bbb ccc ddd", wrap) == (std::vector<std::string>{"aaa bbb", "ccc ddd"}));
    Shaped shaped = engine.shape("aaa bbb   ccc", style, wrap);
    CHECK_NEAR(shaped.lines[0].width, engine.shape("aaa bbb", style).width, 0.01);
    CHECK_EQ(shaped.lines.size(), 2);
    // A word wider than the line is broken between letters, never inside a letter with its accents.
    wrap.max_width = engine.shape("aaaaaa", style).width + 3.0f;
    CHECK(lines_of("aaaaaaaaaaaaaaaaaaaa", wrap) == (std::vector<std::string>{"aaaaaa", "aaaaaa", "aaaaaa", "aa"}));
    const std::string accented = utf8(U"a\U00000301a\U00000301a\U00000301a\U00000301a\U00000301a\U00000301a\U00000301a\U00000301");
    for (const std::string &line : lines_of(accented, wrap))
        CHECK_EQ(line.size() % 3, 0);
    // A break after a hyphen; none at a no-break space.
    wrap.max_width = std::max(engine.shape("well-", style).width, engine.shape("known", style).width) + 2.0f;
    CHECK(lines_of("well-known", wrap) == (std::vector<std::string>{"well-", "known"}));
    wrap.max_width = engine.shape("aaa\xc2\xa0" "bb", style).width - 1.0f;
    CHECK(lines_of("x aaa\xc2\xa0" "bb", wrap).front() == "x");
    // Line feeds: paragraphs, an empty one is an empty line; as one line they are spaces.
    Constraints free;
    free.max_lines = 0;
    CHECK(lines_of("ab\ncd\n\nef", free) == (std::vector<std::string>{"ab", "cd", "", "ef"}));
    shaped = engine.shape("ab\ncd\n\nef", style, free);
    CHECK_NEAR(shaped.height, 4 * shaped.line_height, 0.01);
    CHECK_NEAR(shaped.lines[3].baseline, shaped.ascent + 3 * shaped.line_height, 0.01);
    CHECK(lines_of("ab\ncd", Constraints{}) == (std::vector<std::string>{"ab\ncd"}));
    CHECK_NEAR(engine.shape("ab\ncd", style).width, engine.shape("ab cd", style).width, 0.01);
    // A limit of lines: the last one takes the mark when text is left over, and fits.
    const std::string sentence = "one two three four five six seven eight nine ten eleven twelve";
    Constraints clamp;
    clamp.max_lines = 2;
    clamp.max_width = engine.shape("one two three four", style).width + 4.0f;
    clamp.ellipsis = Ellipsis::end;
    shaped = engine.shape(sentence, style, clamp);
    CHECK_EQ(shaped.lines.size(), 2);
    CHECK(shaped.truncated);
    CHECK(!shaped.lines[0].ellipsis && shaped.lines[1].ellipsis);
    CHECK(line_text(sentence, shaped.lines[0]) == "one two three four");
    CHECK(shaped.lines[1].width <= clamp.max_width + 0.02f);
    CHECK(shaped.lines[1].width > clamp.max_width - 16.0f);
    CHECK_EQ(shaped.glyphs.back().glyph, engine.glyph(0, 0x2026));
    // Without a mark the text simply ends at the last line's break.
    clamp.ellipsis = Ellipsis::none;
    shaped = engine.shape(sentence, style, clamp);
    CHECK_EQ(shaped.lines.size(), 2);
    CHECK(shaped.truncated);
    Constraints unlimited = clamp;
    unlimited.max_lines = 0;
    CHECK(line_text(sentence, shaped.lines[1]) == lines_of(sentence, unlimited)[1]);
    CHECK(line_text(sentence, shaped.lines[1]).rfind("five six", 0) == 0);
    // Room enough: no mark, nothing cut.
    clamp.max_lines = 10;
    clamp.ellipsis = Ellipsis::end;
    shaped = engine.shape(sentence, style, clamp);
    CHECK(!shaped.truncated);
    CHECK_EQ(shaped.lines.size(), 4);
    // Paragraphs beyond the limit: the mark follows the last line shown, even when that line fits.
    Constraints paragraphs;
    paragraphs.max_lines = 2;
    paragraphs.ellipsis = Ellipsis::end;
    shaped = engine.shape("one\ntwo\nthree", style, paragraphs);
    CHECK_EQ(shaped.lines.size(), 2);
    CHECK(shaped.truncated && shaped.lines[1].ellipsis);
    CHECK(line_text("one\ntwo\nthree", shaped.lines[1]) == "two");
    shaped = engine.shape("one\ntwo\n \n", style, paragraphs); // only blank lines follow: nothing is lost
    CHECK(!shaped.truncated);
    // Without a width nothing wraps.
    CHECK_EQ(engine.shape(sentence, style, free).lines.size(), 1);

    if (skip("wrap, unspaced scripts", text_host::have_windows(), "the Windows fonts"))
        return;
    Engine cjk(text_host::stand_in(text_host::dejavu()));
    const auto cjk_lines = [&](const std::u32string &text, float ems, int max_lines = 0)
    {
        Constraints constraints;
        constraints.max_lines = max_lines;
        constraints.max_width = ems * 20.0f + 1.0f;
        const std::string bytes = utf8(text);
        const Shaped result = cjk.shape(bytes, style, constraints);
        std::vector<std::string> lines;
        for (const ShapedLine &line : result.lines)
        {
            lines.push_back(line_text(bytes, line));
            if (line.count > 1)
                CHECK(line.width <= constraints.max_width + 0.02f);
        }
        return lines;
    };
    // Japanese breaks between any two characters...
    CHECK(cjk_lines(U"\U00003042\U00003044\U00003046\U00003048\U0000304A\U0000304B\U0000304D\U0000304F", 4) ==
          (std::vector<std::string>{utf8(U"\U00003042\U00003044\U00003046\U00003048"), utf8(U"\U0000304A\U0000304B\U0000304D\U0000304F")}));
    // ... but a line does not start with a long vowel mark or a full stop: the character before comes along.
    CHECK(cjk_lines(U"\U00003042\U00003044\U00003046\U00003048\U0000304A\U000030FC\U00003002\U0000304B\U0000304D", 5) ==
          (std::vector<std::string>{utf8(U"\U00003042\U00003044\U00003046\U00003048"), utf8(U"\U0000304A\U000030FC\U00003002\U0000304B\U0000304D")}));
    // ... and does not end with an opening bracket.
    CHECK(cjk_lines(U"\U00003042\U00003044\U00003046\U0000300C\U00003048\U0000304A\U0000300D", 4) ==
          (std::vector<std::string>{utf8(U"\U00003042\U00003044\U00003046"), utf8(U"\U0000300C\U00003048\U0000304A\U0000300D")}));
    // Thai without spaces is broken between its clusters, a mark never leaving its consonant.
    const std::u32string thai = U"\U00000E40\U00000E17\U00000E35\U00000E48\U00000E22\U00000E27\U00000E40\U00000E0A\U00000E35\U00000E22\U00000E07\U00000E43\U00000E2B\U00000E21\U00000E48\U00000E2A\U00000E32\U00000E21\U00000E27\U00000E31\U00000E19\U00000E2A\U00000E2D\U00000E07\U00000E04\U00000E37\U00000E19";
    const std::vector<std::string> thai_lines = cjk_lines(thai, 4.5f);
    CHECK(thai_lines.size() >= 3);
    for (const std::string &line : thai_lines)
    {
        std::vector<char32_t> cps = decoded(line);
        CHECK(!cps.empty());
        if (cps.empty())
            continue;
        CHECK(uni::category(cps.front()) != HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK);
        CHECK(!(cps.back() >= 0x0E40 && cps.back() <= 0x0E44)); // a leading vowel does not end a line
    }
}

void test_alignment()
{
    if (skip("alignment", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    const Style style = at(20.0f);
    const std::string hebrew = utf8(U"\U000005E9\U000005DC\U000005D5\U000005DD");
    const float latin_width = engine.shape("abc", style).width;
    const float hebrew_width = engine.shape(hebrew, style).width;
    Constraints box;
    box.max_width = 500.0f;
    const auto x_of = [&](const std::string &text, Align align)
    {
        box.align = align;
        return engine.shape(text, style, box).lines[0].x;
    };
    CHECK_NEAR(x_of("abc", Align::left), 0.0, 0.001);
    CHECK_NEAR(x_of("abc", Align::start), 0.0, 0.001);
    CHECK_NEAR(x_of("abc", Align::right), 500.0f - latin_width, 0.01);
    CHECK_NEAR(x_of("abc", Align::end), 500.0f - latin_width, 0.01);
    CHECK_NEAR(x_of("abc", Align::center), (500.0f - latin_width) * 0.5f, 0.01);
    CHECK_NEAR(x_of(hebrew, Align::start), 500.0f - hebrew_width, 0.01);
    CHECK_NEAR(x_of(hebrew, Align::end), 0.0, 0.001);
    CHECK_NEAR(x_of(hebrew, Align::left), 0.0, 0.001);
    CHECK_NEAR(x_of(hebrew, Align::center), (500.0f - hebrew_width) * 0.5f, 0.01);
    // Each paragraph by its own direction.
    box.align = Align::start;
    box.max_lines = 0;
    Shaped shaped = engine.shape("abc\n" + hebrew, style, box);
    CHECK_EQ(shaped.lines.size(), 2);
    CHECK(!shaped.lines[0].rtl && shaped.lines[1].rtl);
    CHECK_NEAR(shaped.lines[0].x, 0.0, 0.001);
    CHECK_NEAR(shaped.lines[1].x, 500.0f - hebrew_width, 0.01);
    // Without a width the box is the widest line.
    Constraints loose;
    loose.max_lines = 0;
    loose.align = Align::right;
    shaped = engine.shape("abcdef\nabc", style, loose);
    CHECK_NEAR(shaped.lines[0].x, 0.0, 0.001);
    CHECK_NEAR(shaped.lines[1].x, shaped.lines[0].width - shaped.lines[1].width, 0.01);
    // Every wrapped line of a right-to-left paragraph hangs from the right edge.
    Constraints wrap;
    wrap.max_lines = 0;
    wrap.max_width = hebrew_width * 2.5f;
    const std::string longer = hebrew + " " + hebrew + " " + hebrew + " abc " + hebrew + " " + hebrew;
    shaped = engine.shape(longer, style, wrap);
    CHECK(shaped.lines.size() >= 2);
    for (const ShapedLine &line : shaped.lines)
    {
        CHECK(line.rtl);
        CHECK_NEAR(line.x + line.width, wrap.max_width, 0.02);
    }
}

void test_digits_tracking_bold()
{
    if (skip("digits, tracking, bold", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    Style style = at(40.0f);
    // Tabular digits: any two times of the same shape are as wide as each other.
    Style tabular = style;
    tabular.tabular_digits = true;
    CHECK_NEAR(engine.shape("11:11:11", tabular).width, engine.shape("00:00:00", tabular).width, 0.001);
    CHECK_NEAR(engine.shape("1:23:45", tabular).width, engine.shape("9:08:07", tabular).width, 0.001);
    // A face whose digits differ in width shows that the engine evens them out itself when the
    // face has no tabular figures, and uses the face's when it has.
    bool evened = false;
    for (const char *name : {"calibri.ttf", "gadugi.ttf", "ebrima.ttf", "tahoma.ttf", "times.ttf", "arial.ttf"})
    {
        if (!text_host::exists(text_host::windows(name)))
            continue;
        Config config;
        config.faces.push_back({text_host::windows(name), 0, FaceRole::latin, false});
        Engine other(config);
        const float ones = other.shape("1111", style).width;
        const float zeros = other.shape("0000", style).width;
        if (std::fabs(ones - zeros) < 0.5f)
            continue;
        CHECK_NEAR(other.shape("1111", tabular).width, other.shape("0000", tabular).width, 0.001);
        // A digit keeps its place in the middle of its cell.
        const Shaped one = other.shape("1", tabular);
        CHECK(one.glyphs[0].x >= 0.0f);
        std::printf("  tabular digits evened out in %s (1111 is %.1f wide, 0000 %.1f; tabular %.1f)\n", name, ones,
                    zeros, other.shape("1111", tabular).width);
        evened = true;
        break;
    }
    if (!evened)
        std::printf("  note: no font here has digits of different widths; the tabular path ran on equal ones only\n");

    // Letter-spacing: between clusters, not after the last; ligatures are taken apart.
    Style tracked = style;
    tracked.tracking = 3.0f;
    CHECK_NEAR(engine.shape("abc", tracked).width, engine.shape("abc", style).width + 2 * 3.0f, 0.001);
    CHECK_EQ(engine.shape("fi", style).glyphs.size(), 1);
    CHECK_EQ(engine.shape("fi", tracked).glyphs.size(), 2);
    // An accent is not spaced away from its letter.
    CHECK_NEAR(engine.shape("e\xcc\x81" "e\xcc\x81", tracked).width, engine.shape("e\xcc\x81" "e\xcc\x81", style).width + 3.0f, 0.001);
    // Never inside joined writing.
    const std::string salam = "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85";
    CHECK_NEAR(engine.shape(salam, tracked).width, engine.shape(salam, style).width, 0.001);
    // A wrapped line's last gap does not count against the width.
    Constraints wrap;
    wrap.max_lines = 0;
    wrap.max_width = engine.shape("abc", tracked).width + 0.005f;
    CHECK_EQ(engine.shape("abc abc", tracked, wrap).lines.size(), 2);
    CHECK_NEAR(engine.shape("abc abc", tracked, wrap).lines[0].width, engine.shape("abc", tracked).width, 0.001);

    // Bold without a bold cut: the outline is widened and every glyph that takes room takes more.
    Style heavy = style;
    heavy.weight = Weight::bold;
    const Shaped regular = engine.shape("abc", style);
    const Shaped widened = engine.shape("abc", heavy);
    CHECK_EQ(widened.glyphs[0].bold, 1);
    CHECK_NEAR(widened.width, regular.width + 3 * 2.0f * engine.config.bold * 40.0f, 0.001);
    // An accent that takes no room is widened without moving the line.
    const Shaped accent = engine.shape(utf8(U"a\U00000310"), heavy);
    const Shaped bare = engine.shape("a", heavy);
    CHECK_NEAR(accent.width, bare.width, 0.001);
}

// ---- the context: atlas, budget, caches ----

std::unique_ptr<Context> make(const Config &config)
{
    std::string error;
    std::unique_ptr<Context> context = Context::create(config, &error);
    if (!context)
        std::printf("FAIL create: %s\n", error.c_str());
    return context;
}

// The pixels of a quad's rectangle in its page.
std::vector<std::uint8_t> pixels_of(const Context &context, const Quad &quad)
{
    const int size = context.page_size();
    const int x0 = static_cast<int>(std::lround(quad.u0 * static_cast<float>(size)));
    const int y0 = static_cast<int>(std::lround(quad.v0 * static_cast<float>(size)));
    const int x1 = static_cast<int>(std::lround(quad.u1 * static_cast<float>(size)));
    const int y1 = static_cast<int>(std::lround(quad.v1 * static_cast<float>(size)));
    std::vector<std::uint8_t> out;
    const std::uint8_t *page = context.page_pixels(quad.page);
    for (int y = y0; y < y1; ++y)
        out.insert(out.end(), page + static_cast<std::size_t>(y) * size + x0, page + static_cast<std::size_t>(y) * size + x1);
    return out;
}

void check_quads(const Context &context, const Layout &layout)
{
    const float size = static_cast<float>(context.page_size());
    for (const Quad &quad : layout.quads)
    {
        CHECK(quad.x1 > quad.x0 && quad.y1 > quad.y0);
        CHECK(quad.u0 >= 0.0f && quad.v0 >= 0.0f && quad.u1 <= 1.0f && quad.v1 <= 1.0f);
        CHECK(quad.u1 > quad.u0 && quad.v1 > quad.v0);
        CHECK(quad.page < context.page_count());
        CHECK(quad.range > 0.0f);
        // The rectangle is whole pixels of the page and drawn at one scale both ways.
        CHECK_NEAR(quad.u0 * size, std::round(quad.u0 * size), 0.001);
        const float scale_x = (quad.x1 - quad.x0) / ((quad.u1 - quad.u0) * size);
        const float scale_y = (quad.y1 - quad.y0) / ((quad.v1 - quad.v0) * size);
        CHECK_NEAR(scale_x, scale_y, 0.001);
    }
}

void test_families()
{
    if (skip("families", text_host::have_dejavu() && text_host::exists(text_host::dejavu("DejaVuSansMono.ttf")),
             "DejaVu Sans and DejaVu Sans Mono"))
        return;
    Config config = dejavu_only(); // face 0, for every text
    FaceSpec fixed_width{text_host::dejavu("DejaVuSansMono.ttf"), 0, FaceRole::latin, false};
    fixed_width.family = 1;
    config.faces.push_back(fixed_width); // face 1, for styles of family 1
    Engine engine(config);
    const Style plain = at(20.0f);
    Style fixed = at(20.0f);
    fixed.family = 1;
    // A style of the family takes its letters from the family's face: here each as wide as the next.
    CHECK_EQ(engine.shape("iiii", fixed).glyphs[0].face, 1);
    CHECK_NEAR(engine.shape("iiii", fixed).width, engine.shape("WWWW", fixed).width, 0.001);
    // Other styles never see that face.
    CHECK_EQ(engine.shape("iiii", plain).glyphs[0].face, 0);
    CHECK(engine.shape("iiii", plain).width < engine.shape("WWWW", plain).width - 10.0f);
    Style other = plain;
    other.family = 7; // a family nobody named: the shared faces
    CHECK_EQ(engine.shape("iiii", other).glyphs[0].face, 0);
    // What the family's face lacks comes from the shared faces, in the same line.
    char32_t lacking = 0;
    for (const char32_t c : {U'\U00002D30', U'\U000005D0', U'\U00000531', U'\U00000E01', U'\U000010D0', U'\U00002654'})
        if (lacking == 0 && engine.fonts.has(0, c) && !engine.fonts.has(1, c))
            lacking = c;
    CHECK(lacking != 0);
    if (lacking != 0)
    {
        const Shaped mixed = engine.shape(utf8(std::u32string{U'a', lacking}), fixed);
        CHECK_EQ(mixed.missing, 0);
        CHECK_EQ(mixed.glyphs.size(), 2);
        CHECK_EQ(mixed.glyphs[0].face, 1);
        CHECK_EQ(mixed.glyphs[1].face, 0);
    }
    // Line metrics are the family's own face's.
    CHECK_EQ(engine.fonts.primary(1), 1);
    CHECK_EQ(engine.fonts.primary(0), 0);
    CHECK_EQ(engine.fonts.primary(7), 0);
    // The two styles are kept apart in the cache of shaped texts.
    std::unique_ptr<Context> context = make(config);
    const float fixed_width_of = context->measure("iiii", fixed).width;
    CHECK(context->measure("iiii", plain).width < fixed_width_of - 5.0f);
    CHECK_NEAR(context->measure("iiii", fixed).width, fixed_width_of, 0.001);
    CHECK(context->describe().find("DejaVuSansMono.ttf (latin family 1") != std::string::npos);
}

void test_fit()
{
    if (skip("fit", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    std::unique_ptr<Context> context = make(dejavu_only());
    const Style style = at(20.0f);
    const std::string mark = "\xe2\x80\xa6";
    const std::string text = "The quick brown fox jumps over the lazy dog";
    const float full = context->measure(text, style).width;
    CHECK(context->fit(text, style, full + 1.0f) == text);
    CHECK(context->fit(text, style, 0.0f) == text);
    CHECK(context->fit(text, style, 10.0f, Ellipsis::none) == text);
    // Cut at the end: a start of the text and the mark, as much as fits.
    const std::string cut = context->fit(text, style, full * 0.5f);
    CHECK(cut.size() > mark.size() + 5 && cut.size() < text.size());
    CHECK(cut.compare(cut.size() - mark.size(), mark.size(), mark) == 0);
    CHECK(text.compare(0, cut.size() - mark.size(), cut, 0, cut.size() - mark.size()) == 0);
    CHECK(context->measure(cut, style).width <= full * 0.5f + 0.02f);
    CHECK(context->measure(cut, style).width > full * 0.5f - 16.0f);
    // Cut at the start: the mark and an end of the text.
    const std::string tail = context->fit(text, style, full * 0.5f, Ellipsis::start);
    CHECK(tail.compare(0, mark.size(), mark) == 0);
    CHECK(text.size() > tail.size() - mark.size() &&
          text.compare(text.size() - (tail.size() - mark.size()), std::string::npos, tail, mark.size(), std::string::npos) == 0);
    CHECK(context->measure(tail, style).width <= full * 0.5f + 0.02f);
    // No room: the mark alone.
    CHECK(context->fit(text, style, 1.0f) == mark);
    // Arabic: the letter at the cut changes its form once the word is cut there (it no longer
    // joins what followed). Whatever the width, the string that comes back still fits when it
    // is measured as a text of its own.
    const std::string arabic = "\U00000623\U00000641\U00000636\U00000644 \U00000639\U00000634\U00000631 \U00000648\U0000062C\U00000647\U00000627\U0000062A \U00000633\U0000064A\U00000627\U0000062D\U0000064A\U00000629 \U00000641\U0000064A \U00000627\U00000644\U00000639\U00000627\U00000644\U00000645 \U00000627\U00000644\U00000639\U00000631\U00000628\U0000064A \U00000644\U00000639\U00000627\U00000645";
    const float arabic_full = context->measure(arabic, style).width;
    int cuts = 0;
    for (float width = context->measure(mark, style).width + 0.5f; width < arabic_full; width += 3.7f)
    {
        const std::string fitted = context->fit(arabic, style, width);
        CHECK(context->measure(fitted, style).width <= width + 0.02f);
        cuts += fitted != arabic ? 1 : 0;
    }
    CHECK(cuts > 20);
    CHECK(context->fit(arabic, style, arabic_full + 1.0f) == arabic);
}

void test_budget()
{
    if (skip("frame budget", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Config config = dejavu_only();
    config.glyphs_per_frame = 4;
    std::unique_ptr<Context> context = make(config);
    const Style style = at(30.0f);
    const std::string text = "abcdefghij"; // ten different glyphs
    const Metrics measured = context->measure(text, style);
    CHECK_EQ(context->stats().rasterised, 0); // measuring draws nothing
    CHECK_EQ(context->page_count(), 0);
    Layout layout;
    std::vector<std::uint32_t> quads_per_frame;
    float first_x = -1.0f;
    for (int frame = 0; frame < 5; ++frame)
    {
        context->advance_frame();
        const std::uint64_t before = context->stats().rasterised;
        context->layout(text, style, {}, &layout);
        CHECK(context->stats().rasterised - before <= 4);
        quads_per_frame.push_back(static_cast<std::uint32_t>(layout.quads.size()));
        CHECK_EQ(layout.quads.size() + layout.pending, 10);
        // The line does not move while its glyphs arrive.
        CHECK_NEAR(layout.width, measured.width, 0.001);
        CHECK_NEAR(layout.lines[0].baseline, measured.ascent, 0.001);
        if (frame == 0)
            first_x = layout.quads[0].x0;
        else
            CHECK_NEAR(layout.quads[0].x0, first_x, 0.001);
        check_quads(*context, layout);
    }
    CHECK(quads_per_frame == (std::vector<std::uint32_t>{4, 8, 10, 10, 10}));
    CHECK_EQ(context->stats().rasterised, 10);
    CHECK_EQ(context->stats().queued, 0);
    // A glyph that draws nothing (the space) costs no budget.
    context->advance_frame();
    context->layout("k l m n", style, {}, &layout);
    CHECK_EQ(layout.quads.size(), 4);
    CHECK_EQ(layout.pending, 0);
    // Before the first frame a loading screen can draw without the budget.
    std::unique_ptr<Context> warm = make(config);
    warm->prewarm("abcdefghij", style);
    warm->advance_frame();
    warm->layout(text, style, {}, &layout);
    CHECK_EQ(layout.pending, 0);
    CHECK_EQ(layout.quads.size(), 10);
    // A glyph still waiting at one size is shown from a copy at another size meanwhile.
    std::unique_ptr<Context> sizes = make(config);
    sizes->prewarm(text, at(30.0f)); // the small raster size
    sizes->advance_frame();
    sizes->layout(text, at(60.0f), {}, &layout); // needs the middle one
    CHECK_EQ(layout.pending, 6);
    CHECK_EQ(layout.quads.size(), 10);
    check_quads(*sizes, layout);
    float ranges[2] = {layout.quads[0].range, layout.quads[9].range};
    CHECK_NEAR(ranges[0], ranges[1], 0.001); // the same spread on screen whichever copy is drawn
}

void test_atlas_updates()
{
    if (skip("atlas updates", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Config config = dejavu_only();
    config.glyphs_per_frame = 100;
    config.page_size = 512;
    std::unique_ptr<Context> context = make(config);
    const Style style = at(30.0f);
    Layout layout;
    context->advance_frame();
    CHECK(context->atlas_updates().empty());
    context->layout("abc", style, {}, &layout);
    {
        const std::vector<AtlasUpdate> &updates = context->atlas_updates();
        CHECK_EQ(updates.size(), 1);
        CHECK(updates[0].created);
        CHECK_EQ(updates[0].page, 0);
        CHECK_EQ(updates[0].x, 0);
        CHECK_EQ(updates[0].y, 0);
        CHECK_EQ(updates[0].w, 512);
        CHECK_EQ(updates[0].h, 512);
        CHECK_EQ(updates[0].stride, 512);
        CHECK(updates[0].pixels == context->page_pixels(0));
        CHECK_EQ(context->atlas_updates().size(), 1); // asking twice gives the same
    }
    // A later frame: only the rectangle of the new glyphs, and it covers each of them.
    std::vector<std::uint8_t> mirror(context->page_pixels(0), context->page_pixels(0) + 512 * 512);
    context->advance_frame();
    CHECK(context->atlas_updates().empty());
    context->layout("abcxyz", style, {}, &layout);
    {
        const std::vector<AtlasUpdate> &updates = context->atlas_updates();
        CHECK(!updates.empty() && updates.size() <= 8);
        std::uint64_t area = 0;
        for (const AtlasUpdate &update : updates)
        {
            CHECK(!update.created);
            CHECK(update.x + update.w <= 512 && update.y + update.h <= 512);
            area += static_cast<std::uint64_t>(update.w) * update.h;
            // What a renderer does: copy the rectangle into its texture.
            for (int row = 0; row < update.h; ++row)
                std::memcpy(&mirror[static_cast<std::size_t>(update.y + row) * 512 + update.x],
                            update.pixels + static_cast<std::size_t>(row) * update.stride, update.w);
        }
        CHECK(area < 512 * 512 / 8);
        // The texture now equals the page: nothing changed outside the rectangles.
        CHECK(std::memcmp(mirror.data(), context->page_pixels(0), mirror.size()) == 0);
        for (std::size_t q = 3; q < layout.quads.size(); ++q)
        {
            const std::vector<std::uint8_t> pixels = pixels_of(*context, layout.quads[q]);
            CHECK(*std::max_element(pixels.begin(), pixels.end()) > 128);
        }
    }
    // Nothing new: nothing to upload.
    context->advance_frame();
    context->layout("abcxyz", style, {}, &layout);
    CHECK(context->atlas_updates().empty());
}

void test_eviction()
{
    if (skip("atlas bound", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    // One small page: 256 x 256 pixels hold about fifty glyphs of the 28-pixel size.
    Config config = dejavu_only();
    config.page_size = 256;
    config.max_pages = 1;
    config.raster_sizes[1] = 0;
    config.glyphs_per_frame = 1000;
    std::unique_ptr<Context> context = make(config);
    const Style style = at(28.0f);
    // Eight different letters a frame, from Latin, Greek and Cyrillic; and one word every frame.
    // (What two frames in a row draw stays in the atlas: it must be well under a page.)
    const std::size_t per_frame = 8;
    std::u32string alphabet;
    for (char32_t c = 0x21; c < 0x7f; ++c)
        alphabet += c;
    for (char32_t c = 0x391; c < 0x3ca; ++c)
        if (c != 0x3a2)
            alphabet += c;
    for (char32_t c = 0x410; c < 0x450; ++c)
        alphabet += c;
    const std::string hot = "HOT";
    Layout layout;
    Layout hot_layout;
    std::vector<std::vector<std::uint8_t>> hot_pixels;
    std::uint64_t rasterised_for_hot = 0;
    const int frames = static_cast<int>(alphabet.size() / per_frame);
    for (int frame = 0; frame < frames; ++frame)
    {
        context->advance_frame();
        const std::uint64_t before = context->stats().rasterised;
        const std::uint64_t repacked_before = context->stats().compactions;
        context->layout(hot, style, {}, &hot_layout);
        rasterised_for_hot += context->stats().rasterised - before;
        context->layout(utf8(alphabet.substr(static_cast<std::size_t>(frame) * per_frame, per_frame)), style, {}, &layout);
        // A repacked page is uploaded whole, a page that only gained glyphs is not.
        bool whole = false;
        for (const AtlasUpdate &update : context->atlas_updates())
            whole = whole || (update.w == 256 && update.h == 256);
        CHECK_EQ(whole, frame == 0 || context->stats().compactions != repacked_before);
        // What a frame draws is there in that frame, whatever had to leave.
        CHECK_EQ(layout.pending, 0);
        CHECK_EQ(hot_layout.pending, 0);
        CHECK_EQ(hot_layout.quads.size(), 3);
        check_quads(*context, layout);
        // The layout of the hot word was made before the page was repacked: lay it out again to
        // see where its glyphs are now, and compare their pixels with the first frame's.
        context->layout(hot, style, {}, &hot_layout);
        for (std::size_t q = 0; q < hot_layout.quads.size(); ++q)
        {
            const std::vector<std::uint8_t> pixels = pixels_of(*context, hot_layout.quads[q]);
            if (frame == 0)
                hot_pixels.push_back(pixels);
            else
                CHECK(pixels == hot_pixels[q]);
        }
        const Stats stats = context->stats();
        CHECK_EQ(stats.pages, 1);
        CHECK_EQ(stats.atlas_bytes, 256 * 256);
    }
    const Stats stats = context->stats();
    CHECK(stats.evicted > 50);
    CHECK(stats.compactions >= 2);
    CHECK_EQ(stats.overflows, 0);
    CHECK(stats.glyphs < 80);
    CHECK_EQ(rasterised_for_hot, 3); // the word in use every frame was never drawn again
    // Least recently used leaves first: the last letters are still there, the first are not.
    context->advance_frame();
    std::uint64_t before = context->stats().rasterised;
    context->layout(utf8(alphabet.substr(static_cast<std::size_t>(frames - 1) * per_frame, per_frame)), style, {}, &layout);
    CHECK_EQ(context->stats().rasterised, before);
    context->layout(utf8(alphabet.substr(0, per_frame)), style, {}, &layout);
    CHECK_EQ(context->stats().rasterised, before + per_frame);

    // More in one frame than the page can hold: the rest waits, nothing breaks, the bound holds.
    std::unique_ptr<Context> small = make(config);
    small->advance_frame();
    small->layout(utf8(alphabet), style, {}, &layout);
    CHECK(layout.pending > 50);
    CHECK(small->stats().overflows > 50);
    CHECK_EQ(small->stats().pages, 1);
    CHECK_EQ(small->stats().atlas_bytes, 256 * 256);
    check_quads(*small, layout);
    for (int frame = 0; frame < 3; ++frame)
    {
        small->advance_frame();
        small->layout(utf8(alphabet), style, {}, &layout);
        CHECK(layout.pending > 50);
        CHECK(layout.quads.size() > 20);
    }
    // Two pages are two pages, never three.
    config.max_pages = 2;
    std::unique_ptr<Context> two = make(config);
    for (int frame = 0; frame < frames; ++frame)
    {
        two->advance_frame();
        two->layout(utf8(alphabet.substr(static_cast<std::size_t>(frame) * per_frame, per_frame)), style, {}, &layout);
        CHECK_EQ(layout.pending, 0);
    }
    CHECK_EQ(two->stats().pages, 2);
    CHECK_EQ(two->stats().atlas_bytes, 2 * 256 * 256);
    CHECK(two->stats().evicted > 0);
}

void test_layout_cache()
{
    if (skip("layout cache", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Config config = dejavu_only();
    config.layout_cache_entries = 4;
    std::unique_ptr<Context> context = make(config);
    const Style style = at(24.0f);
    for (int i = 0; i < 10; ++i)
        context->measure("text " + std::to_string(i), style);
    Stats stats = context->stats();
    CHECK_EQ(stats.layouts, 4);
    CHECK_EQ(stats.layout_misses, 10);
    CHECK_EQ(stats.layout_hits, 0);
    context->measure("text 9", style);
    context->measure("text 6", style);
    CHECK_EQ(context->stats().layout_hits, 2);
    context->measure("text 0", style); // long gone
    CHECK_EQ(context->stats().layout_misses, 11);
    // The same text another way is another entry.
    Constraints narrow;
    narrow.max_width = 30.0f;
    narrow.ellipsis = Ellipsis::end;
    const Metrics cut = context->measure("text 9", style, narrow);
    CHECK(cut.truncated && cut.width <= 30.02f);
    CHECK_EQ(context->stats().layout_misses, 12);
    Style larger = style;
    larger.size = 25.0f;
    context->measure("text 9", larger);
    CHECK_EQ(context->stats().layout_misses, 13);
    // A bound in bytes: few large texts stay.
    config.layout_cache_entries = 1000;
    config.layout_cache_bytes = 20000;
    std::unique_ptr<Context> bytes = make(config);
    for (int i = 0; i < 50; ++i)
        bytes->measure(std::string(200, static_cast<char>('a' + i % 26)) + std::to_string(i), style);
    stats = bytes->stats();
    CHECK(stats.layout_bytes <= 20000);
    CHECK(stats.layouts >= 1 && stats.layouts < 20);
    // No cache at all still works.
    config.layout_cache_entries = 0;
    std::unique_ptr<Context> none = make(config);
    CHECK_NEAR(none->measure("abc", style).width, context->measure("abc", style).width, 0.001);
    CHECK_EQ(none->stats().layouts, 0);
    // measure() and layout() agree.
    Layout layout;
    Constraints wrap;
    wrap.max_lines = 3;
    wrap.max_width = 120.0f;
    wrap.ellipsis = Ellipsis::end;
    const std::string text = "one two three four five six seven eight nine ten";
    const Metrics metrics = context->measure(text, style, wrap);
    context->advance_frame();
    context->layout(text, style, wrap, &layout);
    CHECK_EQ(metrics.lines, layout.lines.size());
    CHECK_NEAR(metrics.width, layout.width, 0.001);
    CHECK_NEAR(metrics.height, layout.height, 0.001);
    CHECK_EQ(metrics.truncated, layout.truncated);
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_height = 0.0f;
    context->font_metrics(style, &ascent, &descent, &line_height);
    CHECK_NEAR(ascent, layout.ascent, 0.001);
    CHECK_NEAR(line_height, layout.line_height, 0.001);
    Style spaced = style;
    spaced.line_height = 50.0f;
    CHECK_NEAR(context->measure("a\nb", spaced, wrap).height, 100.0, 0.001);
}

void test_distance_field()
{
    if (skip("distance field", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Engine engine(dejavu_only());
    FT_Face face = engine.fonts.outlines(0);
    CHECK(face != nullptr);
    if (face == nullptr)
        return;
    Rasteriser rasteriser;
    for (const int em : {28, 56, 112})
    {
        const float spread = static_cast<float>(em) * 8.0f / 56.0f;
        for (const char32_t c : {U'l', U'O', U'g', U'W', U'\U000000C5', U'\U00000416', U'%'})
        {
            const std::uint32_t glyph = engine.glyph(0, c);
            GlyphImage image;
            CHECK(rasteriser.glyph(face, glyph, em, spread, 0.0f, &image));
            CHECK(image.w > 0 && image.h > 0);
            // FreeType's own fill at this size is the truth: wholly covered pixels are inside the
            // field's outline, empty ones outside, and the field is within 0.75 pixel of the
            // edge wherever the fill says the edge is.
            CHECK(FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(em)) == 0);
            CHECK(FT_Load_Glyph(face, glyph, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_RENDER) == 0);
            const FT_Bitmap &bitmap = face->glyph->bitmap;
            const int left = face->glyph->bitmap_left - static_cast<int>(image.left);
            const int top = -face->glyph->bitmap_top - static_cast<int>(image.top);
            CHECK(left >= 1 && top >= 1);
            CHECK(left + static_cast<int>(bitmap.width) < image.w && top + static_cast<int>(bitmap.rows) < image.h);
            int wrong = 0;
            int far_off = 0;
            for (int y = 0; y < image.h; ++y)
                for (int x = 0; x < image.w; ++x)
                {
                    const int bx = x - left;
                    const int by = y - top;
                    const int covered = bx >= 0 && by >= 0 && bx < static_cast<int>(bitmap.width) && by < static_cast<int>(bitmap.rows)
                                            ? bitmap.buffer[by * bitmap.pitch + bx]
                                            : 0;
                    const int value = image.pixels[static_cast<std::size_t>(y) * image.w + x];
                    const float distance = (static_cast<float>(value) - 128.0f) * spread / 128.0f;
                    if (covered == 255 && value < 128)
                        ++wrong;
                    if (covered == 0 && value > 128)
                        ++wrong;
                    if (covered > 0 && covered < 255 && std::fabs(distance) > 0.75f)
                        ++far_off;
                }
            CHECK_EQ(wrong, 0);
            CHECK(far_off <= (image.w + image.h) / 20); // a few pixels at sharp corners
            // The rim of the picture is far outside: nothing bleeds into a neighbour in the atlas.
            for (int x = 0; x < image.w; ++x)
                CHECK(image.pixels[static_cast<std::size_t>(x)] <= 8 &&
                      image.pixels[static_cast<std::size_t>(image.h - 1) * image.w + x] <= 8);
        }
        // Across the stem of "l": the field rises by 128 / spread a pixel towards the outline.
        GlyphImage stem;
        CHECK(rasteriser.glyph(face, engine.glyph(0, 'l'), em, spread, 0.0f, &stem));
        const std::uint8_t *row = &stem.pixels[static_cast<std::size_t>(stem.h / 2) * stem.w];
        int edge = 0;
        while (edge < stem.w && row[edge] < 128)
            ++edge;
        CHECK(edge >= 4 && edge < stem.w);
        const float per_pixel = 128.0f / spread;
        CHECK_NEAR(static_cast<float>(row[edge - 2]) - static_cast<float>(row[edge - 3]), per_pixel, per_pixel * 0.2f + 1.0f);
        // Widening moves the outline out by that much: the same pixel reads higher.
        GlyphImage bold;
        CHECK(rasteriser.glyph(face, engine.glyph(0, 'l'), em, spread, 2.0f, &bold));
        const int shift = static_cast<int>(stem.left) - static_cast<int>(bold.left);
        const std::uint8_t *bold_row = &bold.pixels[static_cast<std::size_t>(bold.h / 2) * bold.w];
        CHECK(shift >= 0);
        CHECK_NEAR(static_cast<float>(bold_row[edge - 3 + shift]) - static_cast<float>(row[edge - 3]), 2.0f * per_pixel, 2.5f);
    }
    // A space draws nothing and is not an error; a glyph number past the font's is one.
    GlyphImage image;
    CHECK(rasteriser.glyph(face, engine.glyph(0, ' '), 56, 8.0f, 0.0f, &image));
    CHECK_EQ(image.w, 0);
    CHECK(!rasteriser.glyph(face, 1000000, 56, 8.0f, 0.0f, &image));
    CHECK(!rasteriser.glyph(nullptr, 1, 56, 8.0f, 0.0f, &image));
    // The engine's own two glyphs.
    rasteriser.box(56, 8.0f, &image);
    CHECK(image.w > 30 && image.h > 40);
    CHECK(*std::max_element(image.pixels.begin(), image.pixels.end()) > 128);
    CHECK(image.pixels[0] == 0 && image.pixels[static_cast<std::size_t>(image.h / 2) * image.w + image.w / 2] < 128);
    rasteriser.dot(56, 8.0f, &image);
    CHECK(image.pixels[static_cast<std::size_t>(image.h / 2) * image.w + image.w / 2] > 200);
    CHECK_EQ(image.pixels[0], 0);
}

std::uint64_t digest(const Context &context)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (int page = 0; page < context.page_count(); ++page)
    {
        const std::uint8_t *pixels = context.page_pixels(page);
        const std::size_t count = static_cast<std::size_t>(context.page_size()) * context.page_size();
        for (std::size_t i = 0; i < count; ++i)
        {
            hash ^= pixels[i];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

void test_io_modes()
{
    if (skip("mapped and streamed fonts", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    const std::string text = utf8(U"The quick brown fox \U00000416\U0000044B \U000003A9 \U00000633\U00000644\U00000627\U00000645 \U000005E9\U000005DC\U000005D5\U000005DD e\U00000301");
    std::uint64_t digests[2] = {0, 0};
    float widths[2] = {0.0f, 0.0f};
    std::size_t quads[2] = {0, 0};
    int index = 0;
    for (const IoMode mode : {IoMode::map, IoMode::stream})
    {
        Config config = dejavu_only();
        config.io = mode;
        std::unique_ptr<Context> context = make(config);
        Layout layout;
        context->prewarm(text, at(40.0f));
        context->layout(text, at(40.0f), {}, &layout);
        digests[index] = digest(*context);
        widths[index] = layout.width;
        quads[index] = layout.quads.size();
        const Stats stats = context->stats();
        if (mode == IoMode::map)
        {
            CHECK(stats.font_bytes_mapped > 700000);
            CHECK_EQ(stats.font_bytes_read, 0);
            CHECK(context->describe().find("mapped") != std::string::npos);
        }
        else
        {
            // Read on demand: the tables shaping needs and the glyphs drawn, not the file.
            CHECK_EQ(stats.font_bytes_mapped, 0);
            CHECK(stats.font_bytes_read > 1000 && stats.font_bytes_read < 400000);
            CHECK(context->describe().find("read on demand") != std::string::npos);
        }
        ++index;
    }
    CHECK(digests[0] == digests[1]);
    CHECK_NEAR(widths[0], widths[1], 0.0001);
    CHECK_EQ(quads[0], quads[1]);
    // A font larger than the limit is refused, by name.
    Config config = dejavu_only();
    config.max_font_bytes = 1000;
    std::unique_ptr<Context> context = make(config);
    Layout layout;
    context->layout("abc", at(20.0f), {}, &layout);
    CHECK_EQ(layout.missing, 3);
    bool said = false;
    for (const std::string &message : context->messages())
        said = said || message.find("is larger than the limit") != std::string::npos;
    CHECK(said);
}

// ---- hostile and huge input ----

struct Random
{
    std::uint64_t state;
    std::uint32_t next()
    {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(state >> 33);
    }
    std::uint32_t below(std::uint32_t limit)
    {
        return next() % limit;
    }
};

void check_layout(const Context &context, const std::string &text, const Constraints &constraints, const Layout &layout)
{
    CHECK(!layout.lines.empty());
    CHECK(std::isfinite(layout.width) && layout.width >= 0.0f);
    CHECK(std::isfinite(layout.height) && layout.height > 0.0f);
    if (constraints.max_lines > 0)
        CHECK(static_cast<int>(layout.lines.size()) <= constraints.max_lines);
    std::uint32_t quads = 0;
    for (std::size_t i = 0; i < layout.lines.size(); ++i)
    {
        const Line &line = layout.lines[i];
        CHECK(line.text_begin <= line.text_end && line.text_end <= text.size());
        if (i > 0)
            CHECK(line.text_begin >= layout.lines[i - 1].text_end);
        CHECK(std::isfinite(line.width) && line.width >= 0.0f);
        CHECK_EQ(line.first_quad, quads);
        quads += line.quad_count;
        // A line that was wrapped or cut fits, unless it is one cluster (or only the mark) wide.
        const bool bounded = constraints.max_width > 0.0f &&
                             (constraints.max_lines != 1 || constraints.ellipsis != Ellipsis::none) &&
                             !(constraints.max_lines > 1 && constraints.ellipsis == Ellipsis::none && i + 1 == layout.lines.size());
        if (bounded && line.width > constraints.max_width + 0.05f)
        {
            // Count the clusters of what it shows: one cannot be broken.
            std::vector<char32_t> cps;
            std::vector<std::uint32_t> offsets;
            bool cut = false;
            uni::decode(text.substr(line.text_begin, line.text_end - line.text_begin), 100000, &cps, &offsets, &cut);
            std::vector<std::uint8_t> starts;
            uni::grapheme_starts(cps.data(), cps.size(), &starts);
            // Shaping may join several (a ligature, a conjunct): allow a few.
            const bool one_cluster = std::count(starts.begin(), starts.end(), 1) <= 4 || line.ellipsis;
            CHECK(one_cluster);
            if (!one_cluster)
            {
                std::printf("  line %zu of %zu is %.2f wide in %.2f (max_lines %d, ellipsis %d); the text:", i,
                            layout.lines.size(), line.width, constraints.max_width, constraints.max_lines,
                            static_cast<int>(constraints.ellipsis));
                for (const char32_t c : decoded(text))
                    std::printf(" %04X", static_cast<unsigned>(c));
                std::printf("\n  the line:");
                for (const char32_t c : cps)
                    std::printf(" %04X", static_cast<unsigned>(c));
                std::printf("\n");
            }
        }
    }
    CHECK_EQ(quads, layout.quads.size());
    check_quads(context, layout);
}

void test_hostile_input()
{
    if (skip("hostile input", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Config config = text_host::have_windows() ? text_host::stand_in(text_host::dejavu()) : dejavu_only();
    config.glyphs_per_frame = 16;
    config.page_size = 1024;
    std::unique_ptr<Context> context = make(config);
    Layout layout;
    Random random{20261013};
    // Random bytes.
    for (int round = 0; round < 1500; ++round)
    {
        std::string text;
        const std::uint32_t length = random.below(48);
        for (std::uint32_t i = 0; i < length; ++i)
            text += static_cast<char>(random.below(256));
        Constraints constraints;
        constraints.max_width = random.below(3) == 0 ? 0.0f : static_cast<float>(random.below(400));
        constraints.max_lines = static_cast<int>(random.below(4));
        constraints.ellipsis = static_cast<Ellipsis>(random.below(3));
        constraints.align = static_cast<Align>(random.below(5));
        Style style = at(static_cast<float>(8 + random.below(120)));
        style.weight = random.below(2) != 0 ? Weight::bold : Weight::regular;
        style.tracking = random.below(4) == 0 ? 2.0f : 0.0f;
        style.tabular_digits = random.below(2) != 0;
        style.direction = static_cast<Direction>(random.below(3));
        if (round % 50 == 0)
            context->advance_frame();
        context->layout(text, style, constraints, &layout);
        check_layout(*context, text, constraints, layout);
    }
    // Random characters from every kind the engine treats specially.
    const std::u32string pool =
        U"aAfi .,-!?(\"0123456789\n \U000000A0\U000000AD\U00000301\U00000308\U0000200B\U0000200C\U0000200D\U0000200E\U0000200F\U0000202B\U0000202E\U0000202C\U00002066\U00002069"
        U"\U000005D0\U000005D1\U000005E9\U00000627\U00000644\U00000645\U00000633\U0000064E\U00000651\U00000660\U000006F4\U00000915\U0000094D\U00000937\U0000093F\U00000930\U00000E01\U00000E35\U00000E48\U00000E40\U00000E33"
        U"\U00003042\U000030FC\U00003002\U0000300C\U0000300D\U00006771\U00004EAC\U0000D55C\U00001112\U00001161\U000011AB\U0000FE0F\U000020E3\U00002764\U00002026\U0000FFFD"
        U"\U0001F525\U0001F468\U0001F469\U0001F3FD\U0001F1EF\U0001F1F5\U000E0100\U00010330\U0010FFFD";
    for (int round = 0; round < 1500; ++round)
    {
        std::u32string chosen;
        const std::uint32_t length = random.below(40);
        for (std::uint32_t i = 0; i < length; ++i)
            chosen += pool[random.below(static_cast<std::uint32_t>(pool.size()))];
        const std::string text = utf8(chosen);
        Constraints constraints;
        constraints.max_width = random.below(4) == 0 ? 0.0f : static_cast<float>(20 + random.below(500));
        constraints.max_lines = static_cast<int>(random.below(5));
        constraints.ellipsis = static_cast<Ellipsis>(random.below(3));
        constraints.align = static_cast<Align>(random.below(5));
        Style style = at(static_cast<float>(10 + random.below(60)));
        style.direction = static_cast<Direction>(random.below(3));
        style.tracking = random.below(5) == 0 ? 1.5f : 0.0f;
        if (round % 40 == 0)
            context->advance_frame();
        context->layout(text, style, constraints, &layout);
        check_layout(*context, text, constraints, layout);
        const Metrics metrics = context->measure(text, style, constraints);
        CHECK_NEAR(metrics.width, layout.width, 0.001);
    }
    // Sizes and widths that are no numbers.
    Style odd;
    for (const float size : {0.0f, -5.0f, std::nanf(""), INFINITY, 1e-6f, 1e6f})
    {
        odd.size = size;
        context->layout("abc", odd, {}, &layout);
        CHECK(std::isfinite(layout.width));
    }
    Constraints nan;
    nan.max_width = std::nanf("");
    nan.max_lines = -3;
    nan.ellipsis = Ellipsis::end;
    context->layout("abc def", at(20.0f), nan, &layout);
    CHECK(std::isfinite(layout.width) && !layout.lines.empty());
}

void test_long_strings()
{
    if (skip("long strings", text_host::have_dejavu(), "DejaVu Sans"))
        return;
    Config config = dejavu_only();
    std::unique_ptr<Context> context = make(config);
    Layout layout;
    const Style style = at(20.0f);
    // Far more characters than the limit: cut there, said so, and still laid out.
    const std::string huge(300000, 'a');
    Constraints one;
    context->advance_frame();
    context->layout(huge, style, one, &layout);
    CHECK(layout.truncated);
    CHECK_EQ(layout.lines.size(), 1);
    CHECK_EQ(layout.lines[0].text_end, config.max_codepoints);
    CHECK_EQ(layout.quads.size(), config.max_codepoints);
    // The same wrapped without a limit of lines.
    Constraints wrap;
    wrap.max_lines = 0;
    wrap.max_width = 400.0f;
    context->layout(huge, style, wrap, &layout);
    CHECK(layout.lines.size() > 400);
    for (const Line &line : layout.lines)
        CHECK(line.width <= 400.02f);
    // ... and with words.
    std::string words;
    for (int i = 0; i < 4000; ++i)
        words += "word" + std::to_string(i) + " ";
    context->layout(words, style, wrap, &layout);
    CHECK(layout.lines.size() > 50);
    check_layout(*context, words, wrap, layout);
    // Cut with a mark: one pass, the right size.
    Constraints cut;
    cut.max_width = 300.0f;
    cut.ellipsis = Ellipsis::end;
    context->layout(words, style, cut, &layout);
    CHECK(layout.width <= 300.02f && layout.width > 280.0f);
    cut.ellipsis = Ellipsis::start;
    context->layout(words, style, cut, &layout);
    CHECK(layout.width <= 300.02f && layout.width > 280.0f);
    // Ten thousand accents on one letter: one cluster, nothing to break.
    std::string stacked = "a";
    for (int i = 0; i < 10000; ++i)
        stacked += "\xcc\x81";
    context->layout(stacked + "b", style, wrap, &layout);
    CHECK_EQ(layout.lines.size(), 1);
    // Ten thousand line feeds.
    context->layout(std::string(10000, '\n'), style, wrap, &layout);
    CHECK_EQ(layout.lines.size(), 10001);
    Constraints three;
    three.max_lines = 3;
    context->layout(std::string(10000, '\n') + "x", style, three, &layout);
    CHECK_EQ(layout.lines.size(), 3);
    CHECK(layout.truncated);
    // A long right-to-left paragraph with numbers and Latin in it.
    std::string mixed;
    for (int i = 0; i < 1500; ++i)
        mixed += utf8(U"\U000005E9\U000005DC\U000005D5\U000005DD ") + "abc " + std::to_string(i) + " ";
    context->layout(mixed, style, wrap, &layout);
    CHECK(layout.rtl);
    check_layout(*context, mixed, wrap, layout);
    // A smaller limit, in the middle of a character: the cut falls between characters.
    config.max_codepoints = 5;
    std::unique_ptr<Context> small = make(config);
    small->layout(utf8(U"\U000000E9\U000000E9\U000000E9\U000000E9\U000000E9\U000000E9\U000000E9\U000000E9"), style, one, &layout);
    CHECK(layout.truncated);
    CHECK_EQ(layout.lines[0].text_end, 10);
    CHECK_EQ(layout.quads.size() + layout.pending, 5);
}

} // namespace

int main(int argc, char **argv)
{
    g_scratch = argc > 1 ? argv[1] : ".";
    std::printf("fonts: DejaVu %s (%s), Windows %s (%s)\n", text_host::have_dejavu() ? "found" : "absent",
                text_host::dejavu_folder().c_str(), text_host::have_windows() ? "found" : "absent",
                text_host::windows_folder().c_str());
    test_decode();
    test_graphemes();
    test_line_breaks();
    test_missing_faces();
    test_system_folder();
    test_fallback_chain();
    test_golden_dejavu();
    test_visual_order();
    test_scripts();
    test_emoji();
    test_ellipsis();
    test_wrap();
    test_alignment();
    test_digits_tracking_bold();
    test_families();
    test_fit();
    test_budget();
    test_atlas_updates();
    test_eviction();
    test_layout_cache();
    test_distance_field();
    test_io_modes();
    test_hostile_input();
    test_long_strings();
    std::printf("text tests: %d checks, %d failed, %d skipped for want of fonts\n", g_checks, g_failed, g_skipped);
    if (g_failed != 0)
        return 1;
    if (g_skipped != 0 && std::getenv("TUBE_TEXT_REQUIRE_FONTS") != nullptr)
    {
        std::printf("TUBE_TEXT_REQUIRE_FONTS is set: a skipped test is a failure\n");
        return 1;
    }
    return 0;
}
