// ProsperoTube - Text engine: from a text to lines of positioned glyphs.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tube_text_layout.hpp"

#include "tube_text_atlas.hpp"
#include "tube_text_unicode.hpp"

#include <fribidi.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tube::text
{

namespace
{

static_assert(sizeof(FriBidiChar) == sizeof(char32_t), "FriBidi reads the code points in place");
static_assert(sizeof(hb_codepoint_t) == sizeof(char32_t), "HarfBuzz reads the code points in place");

// Widths are sums of floats: a text that measures exactly the limit must still fit.
constexpr float kSlack = 0.01f;

bool is_common(hb_script_t s)
{
    return s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN || s == HB_SCRIPT_INVALID;
}

bool is_cjk(hb_script_t s)
{
    return s == HB_SCRIPT_HAN || s == HB_SCRIPT_HIRAGANA || s == HB_SCRIPT_KATAKANA || s == HB_SCRIPT_HANGUL ||
           s == HB_SCRIPT_BOPOMOFO;
}

// A stretch of a line drawn in one go: part of a run, or (run -1) the cut mark.
struct Piece
{
    int run;
    std::uint32_t from;
    std::uint32_t to;
    std::uint8_t level;
};

// Rule L2 of the bidirectional algorithm: from the highest level down to the lowest odd one,
// reverse every stretch at that level or above.
void reorder(std::vector<Piece> &pieces)
{
    std::uint8_t highest = 0;
    std::uint8_t lowest_odd = 255;
    for (const Piece &piece : pieces)
    {
        highest = std::max(highest, piece.level);
        if ((piece.level & 1) != 0)
            lowest_odd = std::min(lowest_odd, piece.level);
    }
    if (lowest_odd == 255)
        return;
    for (int level = highest; level >= lowest_odd; --level)
    {
        std::size_t i = 0;
        while (i < pieces.size())
        {
            if (pieces[i].level < level)
            {
                ++i;
                continue;
            }
            std::size_t j = i;
            while (j < pieces.size() && pieces[j].level >= level)
                ++j;
            std::reverse(pieces.begin() + static_cast<std::ptrdiff_t>(i), pieces.begin() + static_cast<std::ptrdiff_t>(j));
            i = j;
        }
    }
}

} // namespace

Shaper::Shaper(const Config &config, Fonts *fonts) : config_(config), fonts_(fonts), buffer_(hb_buffer_create())
{
    language_ = config_.language.empty() ? HB_LANGUAGE_INVALID : hb_language_from_string(config_.language.c_str(), -1);
    han_language_[static_cast<int>(HanForms::japanese)] = hb_language_from_string("ja", -1);
    han_language_[static_cast<int>(HanForms::korean)] = hb_language_from_string("ko", -1);
    han_language_[static_cast<int>(HanForms::chinese)] =
        config_.language.compare(0, 2, "zh") == 0 ? language_ : hb_language_from_string("zh-Hans", -1);
}

Shaper::~Shaper()
{
    hb_buffer_destroy(buffer_);
}

std::uint32_t Shaper::next_cluster(std::uint32_t at) const
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    std::uint32_t i = at + 1;
    while (i < count && !cluster_[i])
        ++i;
    return std::min(i, count);
}

bool Shaper::hangs(std::uint32_t at) const
{
    // A space that carries a mark shows that mark: it is drawn and measured like a letter.
    return uni::space(p_[at]) && cluster_[at] && (at + 1 >= p_.size() || cluster_[at + 1]);
}

void Shaper::shape(std::string_view text, const Style &style_in, const Constraints &constraints, Shaped *out)
{
    out->glyphs.clear();
    out->lines.clear();
    out->width = 0.0f;
    out->height = 0.0f;
    out->missing = 0;
    out->truncated = false;
    out->rtl = false;
    Style style = style_in;
    if (!(style.size > 0.0f) || !std::isfinite(style.size))
        style.size = 1.0f;
    if (!std::isfinite(style.tracking))
        style.tracking = 0.0f;
    marks_.clear();

    bool cut = false;
    uni::decode(text, config_.max_codepoints, &cps_, &offsets_, &cut);
    out->truncated = cut;
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_gap = 0.0f;
    fonts_->metrics(fonts_->primary(), &ascent, &descent, &line_gap);
    out->ascent = ascent * style.size;
    out->descent = descent * style.size;
    out->line_height = style.line_height > 0.0f && std::isfinite(style.line_height)
                           ? style.line_height
                           : (ascent + descent + line_gap) * style.size;

    const std::uint32_t count = static_cast<std::uint32_t>(cps_.size());
    if (constraints.max_lines == 1)
        for (char32_t &c : cps_)
            if (c == 0x0a)
                c = 0x20;
    // Where the last character that shows is: paragraphs after it hold nothing.
    std::uint32_t last_solid = 0;
    for (std::uint32_t i = 0; i < count; ++i)
        if (cps_[i] != 0x0a && !uni::space(cps_[i]))
            last_solid = i + 1;

    Progress progress;
    std::uint32_t begin = 0;
    while (true)
    {
        std::uint32_t end = begin;
        while (end < count && cps_[end] != 0x0a)
            ++end;
        paragraph(begin, end, end < last_solid, style, constraints, &progress, out);
        if (progress.stop || end >= count)
            break;
        begin = end + 1;
    }

    float widest = 0.0f;
    for (const ShapedLine &line : out->lines)
        widest = std::max(widest, line.width);
    const float box = constraints.max_width > 0.0f ? constraints.max_width : widest;
    for (std::size_t i = 0; i < out->lines.size(); ++i)
    {
        ShapedLine &line = out->lines[i];
        line.baseline = out->ascent + static_cast<float>(i) * out->line_height;
        Align align = constraints.align;
        if (align == Align::start)
            align = line.rtl ? Align::right : Align::left;
        else if (align == Align::end)
            align = line.rtl ? Align::left : Align::right;
        if (align == Align::right)
            line.x = box - line.width;
        else if (align == Align::center)
            line.x = (box - line.width) * 0.5f;
        else
            line.x = 0.0f;
    }
    out->width = widest;
    out->height = static_cast<float>(out->lines.size()) * out->line_height;
    out->rtl = !out->lines.empty() && out->lines.front().rtl;
}

void Shaper::filter_emoji(const Style &style)
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    kind_.assign(count, 0);
    bool any = false;
    for (std::uint32_t i = 0; i < count && !any; ++i)
        any = uni::pictographic(p_[i]) || uni::regional_indicator(p_[i]);
    if (!any)
        return;
    const bool bold = style.weight == Weight::bold;
    std::vector<char32_t> kept;
    std::vector<std::uint32_t> kept_begin;
    std::vector<std::uint32_t> kept_end;
    std::vector<std::uint8_t> kept_kind;
    kept.reserve(count);
    bool changed = false;
    bool dropped = false; // the last cluster was left out
    std::uint32_t i = 0;
    while (i < count)
    {
        std::uint32_t j = i + 1;
        while (j < count && !starts_[j])
            ++j;
        const char32_t base = p_[i];
        if ((uni::pictographic(base) || uni::regional_indicator(base)) &&
            fonts_->pick(&p_[i], j - i, HB_SCRIPT_COMMON, HB_SCRIPT_COMMON, HanForms::japanese, bold, -1) < 0)
        {
            changed = true;
            if (config_.emoji == EmojiPolicy::strip)
            {
                dropped = true;
            }
            else
            {
                kept.push_back(base);
                kept_begin.push_back(p_begin_[i]);
                kept_end.push_back(p_end_[j - 1]);
                kept_kind.push_back(2);
                dropped = false;
            }
            i = j;
            continue;
        }
        // The space after a sequence that was left out goes with it when it would double another
        // or start the paragraph.
        const bool doubled = dropped && uni::space(base) && j - i == 1 && (kept.empty() || uni::space(kept.back()));
        if (!doubled)
        {
            for (std::uint32_t k = i; k < j; ++k)
            {
                kept.push_back(p_[k]);
                kept_begin.push_back(p_begin_[k]);
                kept_end.push_back(p_end_[k]);
                kept_kind.push_back(0);
            }
        }
        dropped = false;
        i = j;
    }
    if (dropped && !kept.empty() && uni::space(kept.back()))
    {
        kept.pop_back();
        kept_begin.pop_back();
        kept_end.pop_back();
        kept_kind.pop_back();
    }
    if (!changed)
        return;
    p_.swap(kept);
    p_begin_.swap(kept_begin);
    p_end_.swap(kept_end);
    kind_.swap(kept_kind);
    uni::grapheme_starts(p_.data(), p_.size(), &starts_);
}

void Shaper::resolve_scripts()
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    own_script_.resize(count);
    script_.resize(count);
    hb_script_t last = HB_SCRIPT_INVALID;
    bool kana = false;
    bool hangul = false;
    bool han = false;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        const hb_script_t s = uni::script(p_[i]);
        own_script_[i] = s;
        if (!is_common(s))
        {
            last = s;
            kana = kana || s == HB_SCRIPT_HIRAGANA || s == HB_SCRIPT_KATAKANA;
            hangul = hangul || s == HB_SCRIPT_HANGUL;
            han = han || s == HB_SCRIPT_HAN;
        }
        script_[i] = last;
    }
    // What comes before the first letter takes that letter's script.
    hb_script_t first = HB_SCRIPT_COMMON;
    for (std::uint32_t i = 0; i < count; ++i)
        if (script_[i] != HB_SCRIPT_INVALID)
        {
            first = script_[i];
            break;
        }
    for (std::uint32_t i = 0; i < count && script_[i] == HB_SCRIPT_INVALID; ++i)
        script_[i] = first;
    // A cluster is of one script: that of its first character.
    for (std::uint32_t i = 1; i < count; ++i)
        if (!starts_[i])
            script_[i] = script_[i - 1];

    // Whose forms shared Han characters take: the text's own evidence first (kana, Hangul), then
    // the interface language; text with Han only is far more often Chinese than Japanese.
    if (kana)
        han_ = HanForms::japanese;
    else if (hangul)
        han_ = HanForms::korean;
    else if (config_.language.compare(0, 2, "ja") == 0)
        han_ = HanForms::japanese;
    else if (config_.language.compare(0, 2, "ko") == 0)
        han_ = HanForms::korean;
    else
        han_ = HanForms::chinese;
    if (han && !kana && !hangul)
    {
        // Prefer one face that has every Han character of the paragraph over a mix of faces.
        std::vector<char32_t> wanted;
        for (std::uint32_t i = 0; i < count && wanted.size() < 128; ++i)
            if (own_script_[i] == HB_SCRIPT_HAN)
                wanted.push_back(p_[i]);
        const HanForms order[3] = {han_, han_ == HanForms::chinese ? HanForms::japanese : HanForms::chinese,
                                   han_ == HanForms::korean ? HanForms::japanese : HanForms::korean};
        for (const HanForms forms : order)
        {
            const FaceRole role = forms == HanForms::japanese  ? FaceRole::japanese
                                  : forms == HanForms::korean ? FaceRole::korean
                                                              : FaceRole::chinese;
            const int face = fonts_->role_face(role);
            if (face >= 0 && fonts_->has_all(face, wanted.data(), wanted.size()))
            {
                han_ = forms;
                break;
            }
        }
    }
}

void Shaper::resolve_levels(Direction direction)
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    level_.assign(count, 0);
    base_level_ = direction == Direction::rtl ? 1 : 0;
    bool needed = direction == Direction::rtl;
    for (std::uint32_t i = 0; i < count && !needed; ++i)
        needed = uni::needs_bidi(p_[i]);
    if (!needed)
        return;
    std::vector<FriBidiCharType> types(count);
    std::vector<FriBidiBracketType> brackets(count);
    std::vector<FriBidiLevel> levels(count);
    const auto *text = reinterpret_cast<const FriBidiChar *>(p_.data());
    fribidi_get_bidi_types(text, static_cast<FriBidiStrIndex>(count), types.data());
    fribidi_get_bracket_types(text, static_cast<FriBidiStrIndex>(count), types.data(), brackets.data());
    FriBidiParType base = direction == Direction::ltr   ? FRIBIDI_PAR_LTR
                          : direction == Direction::rtl ? FRIBIDI_PAR_RTL
                                                        : FRIBIDI_PAR_ON;
    const FriBidiLevel highest = fribidi_get_par_embedding_levels_ex(
        types.data(), brackets.data(), static_cast<FriBidiStrIndex>(count), &base, levels.data());
    if (highest == 0)
    {
        // Out of memory inside FriBidi: the paragraph runs one way.
        level_.assign(count, static_cast<std::uint8_t>(base_level_));
        return;
    }
    base_level_ = FRIBIDI_IS_RTL(base) ? 1 : 0;
    for (std::uint32_t i = 0; i < count; ++i)
        level_[i] = static_cast<std::uint8_t>(std::clamp<int>(levels[i], 0, 125));
    // A mark runs with the letter it sits on.
    for (std::uint32_t i = 1; i < count; ++i)
        if (!starts_[i])
            level_[i] = level_[i - 1];
}

void Shaper::assign_faces(const Style &style, Shaped *out)
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    face_.assign(count, -1);
    bold_.assign(count, 0);
    const bool bold = style.weight == Weight::bold;
    constexpr std::int16_t kUnset = -2;
    const auto cluster_end = [&](std::uint32_t i)
    {
        std::uint32_t j = i + 1;
        while (j < count && !starts_[j])
            ++j;
        return j;
    };
    // Letters first: each asks the face of its script.
    for (std::uint32_t i = 0; i < count;)
    {
        const std::uint32_t j = cluster_end(i);
        if (kind_[i] == 2)
            face_[i] = -1;
        else if (is_common(own_script_[i]))
            face_[i] = kUnset;
        else
            face_[i] = static_cast<std::int16_t>(fonts_->pick(&p_[i], j - i, own_script_[i], script_[i], han_, bold, -1));
        i = j;
    }
    // Then spaces, digits and punctuation: the face of the text beside them when it has them.
    int previous = -1;
    for (std::uint32_t i = 0; i < count;)
    {
        const std::uint32_t j = cluster_end(i);
        if (face_[i] == kUnset)
        {
            int neighbour = previous;
            for (std::uint32_t k = j; neighbour < 0 && k < count; k = cluster_end(k))
                if (face_[k] >= 0)
                    neighbour = face_[k];
            face_[i] = static_cast<std::int16_t>(fonts_->pick(&p_[i], j - i, own_script_[i], script_[i], han_, bold, neighbour));
        }
        if (face_[i] >= 0)
            previous = face_[i];
        i = j;
    }
    for (std::uint32_t i = 0; i < count;)
    {
        const std::uint32_t j = cluster_end(i);
        if (kind_[i] == 0 && face_[i] < 0)
        {
            bool draws = false;
            for (std::uint32_t k = i; k < j; ++k)
                draws = draws || !uni::default_ignorable(p_[k]);
            kind_[i] = draws ? 1 : 3;
            if (draws)
                ++out->missing;
        }
        const std::uint8_t widened = kind_[i] == 0 && bold && !fonts_->is_bold(face_[i]) ? 1 : 0;
        for (std::uint32_t k = i; k < j; ++k)
        {
            face_[k] = face_[i];
            kind_[k] = kind_[i];
            bold_[k] = widened;
        }
        i = j;
    }
}

void Shaper::build_runs()
{
    runs_.clear();
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    for (std::uint32_t i = 0; i < count; ++i)
    {
        if (i > 0 && level_[i] == level_[i - 1] && script_[i] == script_[i - 1] && face_[i] == face_[i - 1] &&
            kind_[i] == kind_[i - 1] && bold_[i] == bold_[i - 1])
        {
            runs_.back().end = i + 1;
            continue;
        }
        Run run{};
        run.begin = i;
        run.end = i + 1;
        run.script = script_[i];
        run.face = face_[i];
        run.level = level_[i];
        run.kind = kind_[i];
        run.bold = bold_[i];
        runs_.push_back(run);
    }
}

void Shaper::shape_run(Run &run, const Style &style, Shaped *out)
{
    run.first_glyph = static_cast<std::uint32_t>(run_glyphs_.size());
    run.glyph_count = 0;
    const float size = style.size;
    hb_font_t *font = run.kind == 0 ? fonts_->shaper(run.face) : nullptr;
    if (run.kind == 3 || (run.kind == 0 && font == nullptr))
        return;
    if (run.kind != 0)
    {
        // One glyph of the engine's own per user-perceived character.
        const float advance = (run.kind == 1 ? kBoxAdvance : kDotAdvance) * size;
        scratch_.clear();
        for (std::uint32_t i = run.begin; i < run.end; ++i)
            if (starts_[i])
                scratch_.push_back(i);
        if ((run.level & 1) != 0)
            std::reverse(scratch_.begin(), scratch_.end());
        for (const std::uint32_t at : scratch_)
        {
            run_glyphs_.push_back({run.kind, at, advance, 0.0f, 0.0f, 0.0f, run.kind});
            cluster_[at] = 1;
            advance_[at] = advance;
        }
        run.glyph_count = static_cast<std::uint32_t>(run_glyphs_.size()) - run.first_glyph;
        return;
    }

    const std::uint32_t total = static_cast<std::uint32_t>(p_.size());
    const float scale = size / fonts_->units_per_em(run.face);
    hb_buffer_clear_contents(buffer_);
    // The whole paragraph is handed over as context: letters join across a change of font.
    hb_buffer_add_utf32(buffer_, reinterpret_cast<const std::uint32_t *>(p_.data()), static_cast<int>(total), run.begin,
                        static_cast<int>(run.end - run.begin));
    hb_buffer_set_direction(buffer_, (run.level & 1) != 0 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer_, run.script);
    hb_buffer_set_language(buffer_, is_cjk(run.script) ? han_language_[static_cast<int>(han_)] : language_);
    unsigned flags = HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES;
    if (run.begin == 0)
        flags |= HB_BUFFER_FLAG_BOT;
    if (run.end == total)
        flags |= HB_BUFFER_FLAG_EOT;
    hb_buffer_set_flags(buffer_, static_cast<hb_buffer_flags_t>(flags));
    hb_buffer_set_cluster_level(buffer_, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
    hb_feature_t features[3];
    unsigned feature_count = 0;
    if (style.tabular_digits)
        features[feature_count++] = {HB_TAG('t', 'n', 'u', 'm'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END};
    if (style.tracking != 0.0f)
    {
        // Spaced-out letters do not merge into ligatures.
        features[feature_count++] = {HB_TAG('l', 'i', 'g', 'a'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END};
        features[feature_count++] = {HB_TAG('c', 'l', 'i', 'g'), 0, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END};
    }
    hb_shape(font, buffer_, features, feature_count);

    unsigned glyph_count = 0;
    const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer_, &glyph_count);
    const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer_, &glyph_count);
    const float widen = run.bold != 0 ? 2.0f * config_.bold * size : 0.0f;
    const bool spaced = style.tracking != 0.0f && !uni::joined_script(run.script);
    const float digit = style.tabular_digits ? fonts_->digit_advance(run.face) * size : 0.0f;
    for (unsigned i = 0; i < glyph_count;)
    {
        const std::uint32_t at = infos[i].cluster;
        unsigned j = i;
        bool drawn = false;
        while (j < glyph_count && infos[j].cluster == at)
        {
            drawn = drawn || infos[j].codepoint != 0;
            ++j;
        }
        if (at >= total)
        {
            i = j;
            continue;
        }
        float cluster_advance = 0.0f;
        std::size_t pushed = 0;
        if (!drawn)
        {
            // The face had the character but no glyph came out of shaping: a box, as for a missing one.
            cluster_advance = kBoxAdvance * size;
            run_glyphs_.push_back({1, at, cluster_advance, 0.0f, 0.0f, 0.0f, 1});
            pushed = 1;
            ++out->missing;
        }
        else
        {
            for (unsigned k = i; k < j; ++k)
            {
                if (infos[k].codepoint == 0)
                    continue; // a mark the face lacks is left out rather than drawn as a box
                RunGlyph glyph{infos[k].codepoint,
                               at,
                               static_cast<float>(positions[k].x_advance) * scale,
                               static_cast<float>(positions[k].x_offset) * scale,
                               static_cast<float>(positions[k].y_offset) * scale,
                               0.0f,
                               0};
                if (glyph.advance > 0.0f)
                    glyph.advance += widen;
                cluster_advance += glyph.advance;
                run_glyphs_.push_back(glyph);
                ++pushed;
            }
            const bool lone = at + 1 >= total || starts_[at + 1];
            if (digit > 0.0f && pushed == 1 && lone && p_[at] >= '0' && p_[at] <= '9')
            {
                RunGlyph &glyph = run_glyphs_.back();
                const float target = digit + widen;
                glyph.x += (target - glyph.advance) * 0.5f;
                glyph.advance = target;
                cluster_advance = target;
            }
        }
        if (spaced && cluster_advance > 0.0f && pushed > 0)
        {
            run_glyphs_.back().advance += style.tracking;
            run_glyphs_.back().gap = style.tracking;
            cluster_advance += style.tracking;
            gap_[at] = style.tracking;
        }
        cluster_[at] = 1;
        advance_[at] += cluster_advance;
        i = j;
    }
    run.glyph_count = static_cast<std::uint32_t>(run_glyphs_.size()) - run.first_glyph;
}

const Shaper::Mark &Shaper::mark_for(int face_hint, const Style &style)
{
    if (const auto found = marks_.find(face_hint); found != marks_.end())
        return found->second;
    Mark mark;
    const bool bold = style.weight == Weight::bold;
    // The horizontal ellipsis of the face the cut text is in, else of any face, else three full stops.
    std::uint32_t text[3] = {0x2026, 0, 0};
    int length = 1;
    int face = face_hint >= 0 && fonts_->has(face_hint, 0x2026) ? face_hint : -1;
    if (face < 0)
    {
        const char32_t ellipsis = 0x2026;
        face = fonts_->pick(&ellipsis, 1, HB_SCRIPT_COMMON, HB_SCRIPT_LATIN, han_, bold, -1);
    }
    if (face < 0)
    {
        const char32_t stop = '.';
        face = face_hint >= 0 && fonts_->has(face_hint, stop)
                   ? face_hint
                   : fonts_->pick(&stop, 1, HB_SCRIPT_COMMON, HB_SCRIPT_LATIN, han_, bold, -1);
        text[0] = text[1] = text[2] = '.';
        length = 3;
    }
    hb_font_t *font = fonts_->shaper(face);
    if (font != nullptr)
    {
        mark.face = static_cast<std::int16_t>(face);
        mark.bold = bold && !fonts_->is_bold(face) ? 1 : 0;
        const float scale = style.size / fonts_->units_per_em(face);
        const float widen = mark.bold != 0 ? 2.0f * config_.bold * style.size : 0.0f;
        hb_buffer_clear_contents(buffer_);
        hb_buffer_add_utf32(buffer_, text, length, 0, length);
        hb_buffer_set_direction(buffer_, HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer_, HB_SCRIPT_COMMON);
        hb_buffer_set_language(buffer_, language_);
        hb_buffer_set_flags(buffer_, HB_BUFFER_FLAG_DEFAULT);
        hb_shape(font, buffer_, nullptr, 0);
        unsigned glyph_count = 0;
        const hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer_, &glyph_count);
        const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer_, &glyph_count);
        for (unsigned i = 0; i < glyph_count; ++i)
        {
            if (infos[i].codepoint == 0)
                continue;
            RunGlyph glyph{infos[i].codepoint,
                           0,
                           static_cast<float>(positions[i].x_advance) * scale,
                           static_cast<float>(positions[i].x_offset) * scale,
                           static_cast<float>(positions[i].y_offset) * scale,
                           0.0f,
                           0};
            if (glyph.advance > 0.0f)
                glyph.advance += widen;
            mark.width += glyph.advance;
            mark.glyphs.push_back(glyph);
        }
    }
    return marks_.emplace(face_hint, std::move(mark)).first->second;
}

std::uint32_t Shaper::find_break(std::uint32_t from, float max_width, float tracking) const
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    float width = 0.0f;
    std::uint32_t strong = 0; // the last place a line may end; 0: none yet
    std::uint32_t weak = 0;
    float strong_width = 0.0f;
    for (std::uint32_t at = from; at < count;)
    {
        if (at > from && breaks_[at] == uni::strong_break)
        {
            strong = at;
            strong_width = width;
        }
        else if (at > from && breaks_[at] == uni::weak_break)
        {
            weak = at;
        }
        const float advance = advance_[at];
        // Spaces hang past the edge; the letter-spacing after a line's last cluster does not count.
        if (at > from && !hangs(at) && width + advance - (gap_[at] != 0.0f ? tracking : 0.0f) > max_width + kSlack)
        {
            std::uint32_t chosen = std::max(strong, weak);
            // A break inside a word of an unspaced script loses to a real one not far before it.
            if (weak > strong && strong != 0 && strong_width >= 0.6f * max_width)
                chosen = strong;
            return chosen != 0 ? chosen : at;
        }
        width += advance;
        at = next_cluster(at);
    }
    return count;
}

void Shaper::emit_line(std::uint32_t from, std::uint32_t to, Ellipsis mark, const Style &style, Shaped *out)
{
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    while (to > from && hangs(to - 1))
        --to;
    if (mark == Ellipsis::start)
        while (from < to && hangs(from))
            ++from;
    ShapedLine line;
    line.first = static_cast<std::uint32_t>(out->glyphs.size());
    line.rtl = (base_level_ & 1) != 0;
    line.ellipsis = mark != Ellipsis::none;
    if (count == 0)
        line.text_begin = line.text_end = 0;
    else if (from < count)
        line.text_begin = p_begin_[from];
    else
        line.text_begin = p_end_[count - 1];
    line.text_end = to > from ? p_end_[to - 1] : line.text_begin;

    std::vector<Piece> pieces;
    const std::uint8_t base = static_cast<std::uint8_t>(base_level_);
    if (mark == Ellipsis::start)
        pieces.push_back({-1, 0, 0, base});
    for (std::size_t r = 0; r < runs_.size(); ++r)
    {
        const Run &run = runs_[r];
        const std::uint32_t a = std::max(from, run.begin);
        const std::uint32_t b = std::min(to, run.end);
        if (a < b)
            pieces.push_back({static_cast<int>(r), a, b, run.level});
    }
    if (mark == Ellipsis::end)
        pieces.push_back({-1, 0, 0, base});
    reorder(pieces);

    float pen = 0.0f;
    float last_gap = 0.0f;
    for (const Piece &piece : pieces)
    {
        if (piece.run < 0)
        {
            // The mark sits in the face of the text it follows (or, at the start, precedes).
            int hint = -1;
            if (to > from)
                hint = mark == Ellipsis::end ? face_[to - 1] : face_[from];
            else if (count > 0)
                hint = face_[std::min(from, count - 1)];
            const Mark &cut = mark_for(hint, style);
            for (const RunGlyph &glyph : cut.glyphs)
            {
                out->glyphs.push_back({glyph.glyph, cut.face, cut.bold, 0, pen + glyph.x, -glyph.y});
                pen += glyph.advance;
            }
            last_gap = 0.0f;
            continue;
        }
        const Run &run = runs_[static_cast<std::size_t>(piece.run)];
        const RunGlyph *glyphs = run_glyphs_.data() + run.first_glyph;
        for (std::uint32_t g = 0; g < run.glyph_count; ++g)
        {
            const RunGlyph &glyph = glyphs[g];
            if (glyph.cluster < piece.from || glyph.cluster >= piece.to)
                continue;
            const bool own = glyph.kind != 0;
            out->glyphs.push_back({own ? glyph.kind : glyph.glyph, own ? static_cast<std::int16_t>(-1) : run.face,
                                   own ? static_cast<std::uint8_t>(0) : run.bold, 0, pen + glyph.x, -glyph.y});
            pen += glyph.advance;
            last_gap = glyph.gap;
        }
    }
    line.width = std::max(0.0f, pen - last_gap);
    line.count = static_cast<std::uint32_t>(out->glyphs.size()) - line.first;
    out->lines.push_back(line);
}

void Shaper::paragraph(std::uint32_t begin, std::uint32_t end, bool more_after, const Style &style,
                       const Constraints &constraints, Progress *progress, Shaped *out)
{
    p_.assign(cps_.begin() + begin, cps_.begin() + end);
    p_begin_.assign(offsets_.begin() + begin, offsets_.begin() + end);
    p_end_.assign(offsets_.begin() + begin + 1, offsets_.begin() + end + 1);
    uni::grapheme_starts(p_.data(), p_.size(), &starts_);
    filter_emoji(style);
    const std::uint32_t count = static_cast<std::uint32_t>(p_.size());
    const bool limited = constraints.max_lines > 0;
    const bool single = constraints.max_lines == 1;

    runs_.clear();
    run_glyphs_.clear();
    cluster_.assign(count, 0);
    advance_.assign(count, 0.0f);
    gap_.assign(count, 0.0f);
    if (count == 0)
    {
        // An empty paragraph is an empty line.
        base_level_ = style.direction == Direction::rtl ? 1 : 0;
        ShapedLine line;
        line.first = static_cast<std::uint32_t>(out->glyphs.size());
        line.text_begin = line.text_end = offsets_[begin];
        line.rtl = base_level_ != 0;
        out->lines.push_back(line);
        ++progress->lines;
        if (limited && progress->lines >= constraints.max_lines)
        {
            progress->stop = true;
            out->truncated = out->truncated || more_after;
        }
        return;
    }

    resolve_scripts();
    resolve_levels(style.direction);
    assign_faces(style, out);
    build_runs();
    for (Run &run : runs_)
        shape_run(run, style, out);
    cluster_[0] = 1;
    for (const Run &run : runs_)
        cluster_[run.begin] = 1;
    uni::line_breaks(p_.data(), count, starts_, &breaks_);

    const float max_width = constraints.max_width > 0.0f && std::isfinite(constraints.max_width) ? constraints.max_width : 0.0f;
    const bool wrap = !single && max_width > 0.0f;
    const float unlimited = std::numeric_limits<float>::max();
    const auto width_of = [&](std::uint32_t from)
    {
        float width = 0.0f;
        float solid = 0.0f;
        for (std::uint32_t at = from; at < count; at = next_cluster(at))
        {
            width += advance_[at];
            if (!hangs(at))
                solid = width - gap_[at];
        }
        return solid;
    };
    // The longest start of [from, count) that fits with the mark after it.
    const auto cut_end = [&](std::uint32_t from, float limit)
    {
        float width = 0.0f;
        std::uint32_t best = from;
        for (std::uint32_t at = from; at < count;)
        {
            const std::uint32_t next = next_cluster(at);
            width += advance_[at];
            if (!hangs(at))
            {
                if (width + mark_for(face_[at], style).width <= limit + kSlack)
                    best = next;
                else if (width > limit)
                    break;
            }
            at = next;
        }
        emit_line(from, best, Ellipsis::end, style, out);
    };
    // The longest end of [from, count) that fits with the mark before it.
    const auto cut_start = [&](std::uint32_t from, float limit)
    {
        scratch_.clear();
        for (std::uint32_t at = from; at < count; at = next_cluster(at))
            scratch_.push_back(at);
        float width = 0.0f;
        std::uint32_t best = count;
        for (std::size_t k = scratch_.size(); k-- > 0;)
        {
            const std::uint32_t at = scratch_[k];
            width += advance_[at];
            if (hangs(at))
                continue;
            if (width + mark_for(face_[at], style).width <= limit + kSlack)
                best = at;
            else if (width > limit)
                break;
        }
        emit_line(best, count, Ellipsis::start, style, out);
    };

    std::uint32_t from = 0;
    while (true)
    {
        if (limited && progress->lines == constraints.max_lines - 1)
        {
            // The last line there is room for.
            const bool overflow = max_width > 0.0f && width_of(from) > max_width + kSlack;
            if (!overflow && !more_after)
            {
                emit_line(from, count, Ellipsis::none, style, out);
            }
            else if (constraints.ellipsis == Ellipsis::none)
            {
                // One line without a mark runs past the width (the caller clips); the last of
                // several ends where it would have been broken.
                if (single || !overflow)
                    emit_line(from, count, Ellipsis::none, style, out);
                else
                    emit_line(from, find_break(from, max_width, style.tracking), Ellipsis::none, style, out);
                out->truncated = out->truncated || !single;
            }
            else
            {
                if (single && constraints.ellipsis == Ellipsis::start)
                    cut_start(from, max_width > 0.0f ? max_width : unlimited);
                else
                    cut_end(from, max_width > 0.0f ? max_width : unlimited);
                out->truncated = true;
            }
            ++progress->lines;
            progress->stop = true;
            return;
        }
        if (!wrap)
        {
            emit_line(from, count, Ellipsis::none, style, out);
            ++progress->lines;
            return;
        }
        const std::uint32_t next = find_break(from, max_width, style.tracking);
        emit_line(from, next, Ellipsis::none, style, out);
        ++progress->lines;
        if (next >= count)
            return;
        from = next;
    }
}

} // namespace tube::text
