// ProsperoTube - Text engine: characters, clusters and where a line may break.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tube_text_unicode.hpp"

#include <algorithm>
#include <iterator>

namespace tube::text::uni
{

namespace
{

struct Range
{
    char32_t first;
    char32_t last;
};

template <std::size_t N> bool in_ranges(const Range (&ranges)[N], char32_t c)
{
    const Range *end = ranges + N;
    const Range *it = std::upper_bound(ranges, end, c, [](char32_t value, const Range &r) { return value < r.first; });
    return it != ranges && c <= (it - 1)->last;
}

template <std::size_t N> bool among(const char32_t (&set)[N], char32_t c)
{
    return std::find(std::begin(set), std::end(set), c) != std::end(set);
}

// Extended_Pictographic (emoji-data.txt), sorted.
constexpr Range kPictographic[] = {
    {0x00a9, 0x00a9},   {0x00ae, 0x00ae},   {0x203c, 0x203c},   {0x2049, 0x2049},   {0x2122, 0x2122},
    {0x2139, 0x2139},   {0x2194, 0x2199},   {0x21a9, 0x21aa},   {0x231a, 0x231b},   {0x2328, 0x2328},
    {0x2388, 0x2388},   {0x23cf, 0x23cf},   {0x23e9, 0x23f3},   {0x23f8, 0x23fa},   {0x24c2, 0x24c2},
    {0x25aa, 0x25ab},   {0x25b6, 0x25b6},   {0x25c0, 0x25c0},   {0x25fb, 0x25fe},   {0x2600, 0x2605},
    {0x2607, 0x2612},   {0x2614, 0x2685},   {0x2690, 0x2705},   {0x2708, 0x2712},   {0x2714, 0x2714},
    {0x2716, 0x2716},   {0x271d, 0x271d},   {0x2721, 0x2721},   {0x2728, 0x2728},   {0x2733, 0x2734},
    {0x2744, 0x2744},   {0x2747, 0x2747},   {0x274c, 0x274c},   {0x274e, 0x274e},   {0x2753, 0x2755},
    {0x2757, 0x2757},   {0x2763, 0x2767},   {0x2795, 0x2797},   {0x27a1, 0x27a1},   {0x27b0, 0x27b0},
    {0x27bf, 0x27bf},   {0x2934, 0x2935},   {0x2b05, 0x2b07},   {0x2b1b, 0x2b1c},   {0x2b50, 0x2b50},
    {0x2b55, 0x2b55},   {0x3030, 0x3030},   {0x303d, 0x303d},   {0x3297, 0x3297},   {0x3299, 0x3299},
    {0x1f000, 0x1f0ff}, {0x1f10d, 0x1f10f}, {0x1f12f, 0x1f12f}, {0x1f16c, 0x1f171}, {0x1f17e, 0x1f17f},
    {0x1f18e, 0x1f18e}, {0x1f191, 0x1f19a}, {0x1f1ad, 0x1f1e5}, {0x1f201, 0x1f20f}, {0x1f21a, 0x1f21a},
    {0x1f22f, 0x1f22f}, {0x1f232, 0x1f23a}, {0x1f23c, 0x1f23f}, {0x1f249, 0x1f3fa}, {0x1f400, 0x1f53d},
    {0x1f546, 0x1f64f}, {0x1f680, 0x1f6ff}, {0x1f774, 0x1f77f}, {0x1f7d5, 0x1f7ff}, {0x1f80c, 0x1f80f},
    {0x1f848, 0x1f84f}, {0x1f85a, 0x1f85f}, {0x1f888, 0x1f88f}, {0x1f8ae, 0x1f8ff}, {0x1f90c, 0x1f93a},
    {0x1f93c, 0x1f945}, {0x1f947, 0x1faff}, {0x1fc00, 0x1fffd},
};

constexpr Range kIgnorable[] = {
    {0x00ad, 0x00ad},   {0x034f, 0x034f},   {0x061c, 0x061c},   {0x115f, 0x1160},   {0x17b4, 0x17b5},
    {0x180b, 0x180f},   {0x200b, 0x200f},   {0x202a, 0x202e},   {0x2060, 0x206f},   {0x3164, 0x3164},
    {0xfe00, 0xfe0f},   {0xfeff, 0xfeff},   {0xffa0, 0xffa0},   {0xfff0, 0xfff8},   {0x1bca0, 0x1bca3},
    {0x1d173, 0x1d17a}, {0xe0000, 0xe0fff},
};

bool continuation(unsigned char b)
{
    return (b & 0xc0u) == 0x80u;
}

// ---- grapheme clusters ----

enum class Gb : std::uint8_t
{
    other,
    cr,
    lf,
    control,
    extend,
    zwj,
    regional,
    prepend,
    spacing,
    l,
    v,
    t,
    lv,
    lvt,
    pict,
};

constexpr char32_t kPrepend[] = {0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x06dd,
                                 0x070f, 0x0890, 0x0891, 0x08e2, 0x110bd, 0x110cd};

Gb gb_class(char32_t c)
{
    if (c == 0x0d)
        return Gb::cr;
    if (c == 0x0a)
        return Gb::lf;
    if (c == 0x200d)
        return Gb::zwj;
    if (c == 0x200c)
        return Gb::extend;
    if (regional_indicator(c))
        return Gb::regional;
    if ((c >= 0x1100 && c <= 0x115f) || (c >= 0xa960 && c <= 0xa97c))
        return Gb::l;
    if ((c >= 0x1160 && c <= 0x11a7) || (c >= 0xd7b0 && c <= 0xd7c6))
        return Gb::v;
    if ((c >= 0x11a8 && c <= 0x11ff) || (c >= 0xd7cb && c <= 0xd7fb))
        return Gb::t;
    if (c >= 0xac00 && c <= 0xd7a3)
        return (c - 0xac00) % 28 == 0 ? Gb::lv : Gb::lvt;
    if (emoji_modifier(c) || (c >= 0xe0020 && c <= 0xe007f))
        return Gb::extend;
    if (c == 0x0e33 || c == 0x0eb3)
        return Gb::spacing;
    if (among(kPrepend, c))
        return Gb::prepend;
    switch (category(c))
    {
    case HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK:
    case HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK:
        return Gb::extend;
    case HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK:
        return Gb::spacing;
    case HB_UNICODE_GENERAL_CATEGORY_CONTROL:
    case HB_UNICODE_GENERAL_CATEGORY_FORMAT:
    case HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR:
    case HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR:
    case HB_UNICODE_GENERAL_CATEGORY_SURROGATE:
        return Gb::control;
    default:
        break;
    }
    return pictographic(c) ? Gb::pict : Gb::other;
}

// The scripts whose consonants a virama joins into one conjunct.
bool conjunct_script(hb_script_t s)
{
    return s == HB_SCRIPT_DEVANAGARI || s == HB_SCRIPT_BENGALI || s == HB_SCRIPT_GUJARATI || s == HB_SCRIPT_ORIYA ||
           s == HB_SCRIPT_TELUGU || s == HB_SCRIPT_MALAYALAM || s == HB_SCRIPT_KANNADA || s == HB_SCRIPT_GURMUKHI ||
           s == HB_SCRIPT_SINHALA;
}

// ---- line breaking ----

enum class Lb : std::uint8_t
{
    al, // letters and everything that stays with them
    sp,
    zw, // zero-width space: a break without a space
    gl, // glue: no break on either side
    ba, // break after
    hy, // hyphen-minus
    op, // opening punctuation
    cl, // closing punctuation
    ex, // exclamation, question
    is, // comma, full stop, colon
    sy, // slash
    ns, // may not start a line (small kana, the long vowel mark, iteration marks)
    in, // ellipsis leaders
    id, // ideographs, kana, wide forms, emoji
    sa, // scripts written without spaces (Thai, Lao, Khmer, Burmese)
    nu, // digits
    qu, // quotation marks
};

constexpr char32_t kGlue[] = {0x00a0, 0x202f, 0x2007, 0x2011, 0x2060, 0xfeff, 0x0f0c, 0x180e};
constexpr char32_t kBreakAfter[] = {0x2010, 0x2012, 0x2013, 0x2014, 0x00ad, 0x058a, 0x05be, 0x0f0b, 0x1361, 0x2027};
constexpr char32_t kExclamation[] = {'!', '?', 0xff01, 0xff1f, 0x061b, 0x061f, 0x06d4, 0x07f9, 0x203c, 0x2047, 0x2048, 0x2049};
constexpr char32_t kInfix[] = {',', '.', ':', ';', 0x037e, 0x0589, 0x060c, 0x060d, 0x07f8, 0x2044};
constexpr char32_t kLeaders[] = {0x2024, 0x2025, 0x2026, 0x22ef, 0xfe19};
constexpr char32_t kClosing[] = {0x3001, 0x3002, 0xff0c, 0xff0e, 0xff61, 0xff64, 0xfe50, 0xfe52};
// What may not start a line: the long vowel mark, iteration marks, small kana, wide colon.
constexpr char32_t kNonStarter[] = {
    0x3005, 0x303b, 0x309b, 0x309c, 0x309d, 0x309e, 0x30a0, 0x30fb, 0x30fc, 0x30fd, 0x30fe, 0xff1a, 0xff1b,
    0xff65, 0xff70, 0xff9e, 0xff9f, 0x3041, 0x3043, 0x3045, 0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087,
    0x308e, 0x3095, 0x3096, 0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30c3, 0x30e3, 0x30e5, 0x30e7, 0x30ee,
    0x30f5, 0x30f6, 0xff67, 0xff68, 0xff69, 0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e, 0xff6f,
};

Lb lb_class(char32_t c)
{
    if (space(c))
        return Lb::sp;
    if (c == 0x200b)
        return Lb::zw;
    if (c == 0x002d)
        return Lb::hy;
    if (c == '/')
        return Lb::sy;
    if (c == '"' || c == '\'')
        return Lb::qu;
    if (c == 0x00a1 || c == 0x00bf)
        return Lb::op;
    if (among(kGlue, c))
        return Lb::gl;
    if (among(kBreakAfter, c))
        return Lb::ba;
    if (among(kExclamation, c))
        return Lb::ex;
    if (among(kInfix, c))
        return Lb::is;
    if (among(kLeaders, c))
        return Lb::in;
    if (among(kClosing, c))
        return Lb::cl;
    if (among(kNonStarter, c) || (c >= 0x31f0 && c <= 0x31ff))
        return Lb::ns;
    switch (category(c))
    {
    case HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION:
        return Lb::op;
    case HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION:
        return Lb::cl;
    case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION:
    case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION:
        return Lb::qu;
    case HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER:
        if (c < 0xff00)
            return Lb::nu;
        break;
    default:
        break;
    }
    if ((c >= 0x2e80 && c <= 0x33ff) || (c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0xa4cf) ||
        (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe30 && c <= 0xfe4f) || (c >= 0xff00 && c <= 0xff60) ||
        (c >= 0xff66 && c <= 0xff9d) || (c >= 0xffe0 && c <= 0xffe6) || (c >= 0x1f000 && c <= 0x1faff) ||
        (c >= 0x1fc00 && c <= 0x1fffd) || (c >= 0x20000 && c <= 0x3fffd))
        return Lb::id;
    if (unspaced_script(script(c)))
        return Lb::sa;
    return Lb::al;
}

bool thai_consonant(char32_t c)
{
    return (c >= 0x0e01 && c <= 0x0e2e) || (c >= 0x0e81 && c <= 0x0eae);
}

constexpr char32_t kLeadingVowel[] = {0x0e40, 0x0e41, 0x0e42, 0x0e43, 0x0e44, 0x0ec0, 0x0ec1, 0x0ec2, 0x0ec3, 0x0ec4};
// Vowels and signs written after the consonant they belong to.
constexpr char32_t kFollowing[] = {0x0e30, 0x0e32, 0x0e33, 0x0e45, 0x0e46, 0x0e2f, 0x0e4f, 0x0e5a, 0x0e5b,
                                   0x0eb0, 0x0eb2, 0x0eb3, 0x0ec6, 0x0eaf};
// Marks after which the syllable still needs its final consonant.
constexpr char32_t kNeedsFinal[] = {0x0e31, 0x0e47, 0x0eb1, 0x0ebb};
// Marks that make their consonant the silent or final one of a word.
constexpr char32_t kMakesFinal[] = {0x0e4c, 0x0ecc, 0x103a};

} // namespace

void decode(std::string_view text, std::uint32_t max_codepoints, std::vector<char32_t> *cps,
            std::vector<std::uint32_t> *offsets, bool *cut)
{
    cps->clear();
    offsets->clear();
    *cut = false;
    const auto *s = reinterpret_cast<const unsigned char *>(text.data());
    std::size_t n = text.size();
    if (n > 0x7fffffffu)
    {
        n = 0x7fffffffu;
        *cut = true;
    }
    std::size_t i = 0;
    while (i < n)
    {
        if (cps->size() >= max_codepoints)
        {
            *cut = true;
            break;
        }
        const std::size_t start = i;
        const unsigned char b = s[i];
        char32_t c = 0xfffd;
        if (b < 0x80)
        {
            c = b;
            i += 1;
        }
        else if (b >= 0xc2 && b <= 0xdf)
        {
            if (i + 1 < n && continuation(s[i + 1]))
            {
                c = (static_cast<char32_t>(b & 0x1fu) << 6) | (s[i + 1] & 0x3fu);
                i += 2;
            }
            else
            {
                i += 1;
            }
        }
        else if (b >= 0xe0 && b <= 0xef)
        {
            const unsigned char low = b == 0xe0 ? 0xa0 : 0x80;
            const unsigned char high = b == 0xed ? 0x9f : 0xbf;
            if (i + 1 < n && s[i + 1] >= low && s[i + 1] <= high)
            {
                if (i + 2 < n && continuation(s[i + 2]))
                {
                    c = (static_cast<char32_t>(b & 0x0fu) << 12) | (static_cast<char32_t>(s[i + 1] & 0x3fu) << 6) |
                        (s[i + 2] & 0x3fu);
                    i += 3;
                }
                else
                {
                    i += 2;
                }
            }
            else
            {
                i += 1;
            }
        }
        else if (b >= 0xf0 && b <= 0xf4)
        {
            const unsigned char low = b == 0xf0 ? 0x90 : 0x80;
            const unsigned char high = b == 0xf4 ? 0x8f : 0xbf;
            if (i + 1 < n && s[i + 1] >= low && s[i + 1] <= high)
            {
                if (i + 2 < n && continuation(s[i + 2]))
                {
                    if (i + 3 < n && continuation(s[i + 3]))
                    {
                        c = (static_cast<char32_t>(b & 0x07u) << 18) | (static_cast<char32_t>(s[i + 1] & 0x3fu) << 12) |
                            (static_cast<char32_t>(s[i + 2] & 0x3fu) << 6) | (s[i + 3] & 0x3fu);
                        i += 4;
                    }
                    else
                    {
                        i += 3;
                    }
                }
                else
                {
                    i += 2;
                }
            }
            else
            {
                i += 1;
            }
        }
        else
        {
            i += 1;
        }

        if (c == 0x0d)
        {
            if (i < n && s[i] == 0x0a)
                ++i;
            c = 0x0a;
        }
        else if (c == 0x0b || c == 0x0c || c == 0x85 || c == 0x2028 || c == 0x2029)
        {
            c = 0x0a;
        }
        else if (c == 0x09)
        {
            c = 0x20;
        }
        else if (c != 0x0a && (c < 0x20 || (c >= 0x7f && c < 0xa0)))
        {
            continue; // a control that draws nothing
        }
        else if ((c & 0xfffeu) == 0xfffeu || (c >= 0xfdd0 && c <= 0xfdef))
        {
            continue; // a noncharacter
        }
        cps->push_back(c);
        offsets->push_back(static_cast<std::uint32_t>(start));
    }
    offsets->push_back(static_cast<std::uint32_t>(i));
}

hb_unicode_general_category_t category(char32_t c)
{
    return hb_unicode_general_category(hb_unicode_funcs_get_default(), c);
}

hb_script_t script(char32_t c)
{
    return hb_unicode_script(hb_unicode_funcs_get_default(), c);
}

unsigned combining_class(char32_t c)
{
    return static_cast<unsigned>(hb_unicode_combining_class(hb_unicode_funcs_get_default(), c));
}

bool default_ignorable(char32_t c)
{
    return in_ranges(kIgnorable, c);
}

bool variation_selector(char32_t c)
{
    return (c >= 0xfe00 && c <= 0xfe0f) || (c >= 0xe0100 && c <= 0xe01ef) || (c >= 0x180b && c <= 0x180d);
}

bool regional_indicator(char32_t c)
{
    return c >= 0x1f1e6 && c <= 0x1f1ff;
}

bool emoji_modifier(char32_t c)
{
    return c >= 0x1f3fb && c <= 0x1f3ff;
}

bool pictographic(char32_t c)
{
    return in_ranges(kPictographic, c);
}

bool space(char32_t c)
{
    return c == 0x20 || c == 0x1680 || (c >= 0x2000 && c <= 0x2006) || (c >= 0x2008 && c <= 0x200a) || c == 0x205f ||
           c == 0x3000;
}

bool needs_bidi(char32_t c)
{
    if (c < 0x0590)
        return false;
    return (c <= 0x08ff) || (c >= 0xfb1d && c <= 0xfdff) || (c >= 0xfe70 && c <= 0xfeff) ||
           (c >= 0x10800 && c <= 0x10fff) || (c >= 0x1e800 && c <= 0x1efff) || c == 0x200f ||
           (c >= 0x202a && c <= 0x202e) || (c >= 0x2066 && c <= 0x2069);
}

bool joined_script(hb_script_t s)
{
    return s == HB_SCRIPT_ARABIC || s == HB_SCRIPT_SYRIAC || s == HB_SCRIPT_MONGOLIAN || s == HB_SCRIPT_NKO ||
           s == HB_SCRIPT_MANDAIC || s == HB_SCRIPT_PHAGS_PA || s == HB_SCRIPT_ADLAM ||
           s == HB_SCRIPT_HANIFI_ROHINGYA || s == HB_SCRIPT_DEVANAGARI || s == HB_SCRIPT_BENGALI ||
           s == HB_SCRIPT_GURMUKHI;
}

bool unspaced_script(hb_script_t s)
{
    return s == HB_SCRIPT_THAI || s == HB_SCRIPT_LAO || s == HB_SCRIPT_MYANMAR || s == HB_SCRIPT_KHMER ||
           s == HB_SCRIPT_TAI_LE || s == HB_SCRIPT_NEW_TAI_LUE || s == HB_SCRIPT_TAI_THAM || s == HB_SCRIPT_TAI_VIET;
}

void grapheme_starts(const char32_t *cps, std::size_t count, std::vector<std::uint8_t> *starts)
{
    starts->assign(count, 0);
    if (count == 0)
        return;
    (*starts)[0] = 1;
    Gb previous = gb_class(cps[0]);
    int regionals = previous == Gb::regional ? 1 : 0; // regional indicators in a row before this one
    int emoji = previous == Gb::pict ? 1 : 0;         // 1: pictograph and its marks; 2: ... then a joiner
    // A consonant of a conjunct script, then (when linked) a virama, with only marks between.
    hb_script_t conjunct = HB_SCRIPT_INVALID;
    bool linked = false;
    const auto consonant = [](char32_t c, hb_script_t *of)
    {
        if (category(c) != HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER)
            return false;
        *of = script(c);
        return conjunct_script(*of);
    };
    {
        hb_script_t s = HB_SCRIPT_INVALID;
        if (consonant(cps[0], &s))
            conjunct = s;
    }
    for (std::size_t i = 1; i < count; ++i)
    {
        const char32_t c = cps[i];
        const Gb current = gb_class(c);
        hb_script_t consonant_script = HB_SCRIPT_INVALID;
        const bool is_consonant = current == Gb::other && consonant(c, &consonant_script);
        bool boundary = true;
        const auto is_control = [](Gb g) { return g == Gb::control || g == Gb::cr || g == Gb::lf; };
        if (previous == Gb::cr && current == Gb::lf)
            boundary = false;
        else if (is_control(previous) || is_control(current))
            boundary = true;
        else if (previous == Gb::l && (current == Gb::l || current == Gb::v || current == Gb::lv || current == Gb::lvt))
            boundary = false;
        else if ((previous == Gb::lv || previous == Gb::v) && (current == Gb::v || current == Gb::t))
            boundary = false;
        else if ((previous == Gb::lvt || previous == Gb::t) && current == Gb::t)
            boundary = false;
        else if (current == Gb::extend || current == Gb::zwj || current == Gb::spacing)
            boundary = false;
        else if (previous == Gb::prepend)
            boundary = false;
        else if (emoji == 2 && current == Gb::pict)
            boundary = false;
        else if (previous == Gb::regional && current == Gb::regional && (regionals % 2) == 1)
            boundary = false;
        else if (linked && is_consonant && consonant_script == conjunct)
            boundary = false;
        (*starts)[i] = boundary ? 1 : 0;

        regionals = current == Gb::regional ? regionals + 1 : 0;
        if (current == Gb::regional && previous != Gb::regional)
            regionals = 1;
        if (current == Gb::pict)
            emoji = 1;
        else if (current == Gb::extend && emoji == 1)
            emoji = 1;
        else if (current == Gb::zwj && emoji == 1)
            emoji = 2;
        else
            emoji = 0;
        if (is_consonant)
        {
            conjunct = consonant_script;
            linked = false;
        }
        else if (current == Gb::extend || current == Gb::zwj)
        {
            if (conjunct != HB_SCRIPT_INVALID && combining_class(c) == 9)
                linked = true;
        }
        else
        {
            conjunct = HB_SCRIPT_INVALID;
            linked = false;
        }
        previous = current;
    }
}

void line_breaks(const char32_t *cps, std::size_t count, const std::vector<std::uint8_t> &starts,
                 std::vector<std::uint8_t> *breaks)
{
    breaks->assign(count, no_break);
    // The clusters: where each starts, and the class of its first character.
    std::vector<std::uint32_t> at;
    std::vector<Lb> classes;
    at.reserve(count);
    classes.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!starts[i])
            continue;
        at.push_back(static_cast<std::uint32_t>(i));
        classes.push_back(lb_class(cps[i]));
    }
    const std::size_t clusters = at.size();
    const auto cluster_end = [&](std::size_t k) { return k + 1 < clusters ? at[k + 1] : static_cast<std::uint32_t>(count); };
    const auto contains = [&](std::size_t k, const char32_t *set, std::size_t set_size)
    {
        for (std::uint32_t i = at[k]; i < cluster_end(k); ++i)
            if (std::find(set, set + set_size, cps[i]) != set + set_size)
                return true;
        return false;
    };
    const auto bare_consonant = [&](std::size_t k) { return cluster_end(k) - at[k] == 1 && thai_consonant(cps[at[k]]); };

    Lb last_solid = Lb::al; // the last cluster that is not a space
    bool any_solid = false;
    bool after_zero_width = false; // a zero-width space, then only spaces
    for (std::size_t k = 0; k < clusters; ++k)
    {
        const Lb current = classes[k];
        if (k > 0)
        {
            const Lb previous = classes[k - 1];
            const Lb before = k > 1 ? classes[k - 2] : Lb::sp;
            std::uint8_t result = no_break;
            if (current == Lb::sp || current == Lb::zw)
                result = no_break;
            else if (after_zero_width)
                result = strong_break;
            else if (previous == Lb::gl)
                result = no_break;
            else if (current == Lb::gl)
                result = previous == Lb::sp ? strong_break : no_break;
            else if (current == Lb::cl || current == Lb::ex || current == Lb::is || current == Lb::sy)
                result = no_break;
            else if (any_solid && last_solid == Lb::op)
                result = no_break;
            else if (previous == Lb::sp)
                result = strong_break;
            else if (previous == Lb::qu || current == Lb::qu)
                result = no_break;
            else if (current == Lb::ns || current == Lb::ba || current == Lb::hy || current == Lb::in)
                result = no_break;
            else if (previous == Lb::ba)
                result = strong_break;
            else if (previous == Lb::hy)
                result = (current == Lb::al || current == Lb::id || current == Lb::sa) &&
                                 (before == Lb::al || before == Lb::nu || before == Lb::id || before == Lb::sa)
                             ? strong_break
                             : no_break;
            else if (previous == Lb::sy)
                result = (current == Lb::al || current == Lb::id) && before != Lb::sy && before != Lb::is ? weak_break
                                                                                                         : no_break;
            else if (previous == Lb::id || current == Lb::id)
                result = strong_break;
            else if (previous == Lb::sa && current == Lb::sa)
            {
                const char32_t previous_base = cps[at[k - 1]];
                const char32_t current_base = cps[at[k]];
                const bool first_of_run = k < 2 || classes[k - 2] != Lb::sa;
                const bool last_of_run = k + 1 >= clusters || classes[k + 1] != Lb::sa;
                bool allowed = true;
                if (combining_class(cps[cluster_end(k - 1) - 1]) == 9)
                    allowed = false; // an invisible stacker: the next consonant hangs under this one
                else if (among(kLeadingVowel, previous_base) || among(kFollowing, current_base))
                    allowed = false;
                else if (contains(k - 1, kNeedsFinal, std::size(kNeedsFinal)) ||
                         contains(k, kMakesFinal, std::size(kMakesFinal)))
                    allowed = false;
                else if ((bare_consonant(k) && last_of_run) || (bare_consonant(k - 1) && first_of_run))
                    allowed = false;
                result = allowed ? weak_break : no_break;
            }
            else if ((previous == Lb::sa) != (current == Lb::sa) &&
                     (previous == Lb::sa || previous == Lb::al || previous == Lb::nu) &&
                     (current == Lb::sa || current == Lb::al || current == Lb::nu))
                result = weak_break;
            (*breaks)[at[k]] = result;
        }
        if (current == Lb::zw)
            after_zero_width = true;
        else if (current != Lb::sp)
            after_zero_width = false;
        if (current != Lb::sp)
        {
            last_solid = current;
            any_solid = true;
        }
    }
}

} // namespace tube::text::uni
