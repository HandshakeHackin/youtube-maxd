// ProsperoTube - Text engine: characters, clusters and where a line may break.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <hb.h>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tube::text::uni
{

// UTF-8 to code points. An ill-formed sequence becomes one U+FFFD (the longest prefix that could
// have started a character, as the Unicode Standard recommends). Controls that draw nothing are
// dropped, a tab becomes a space, every line end (CR LF, CR, LF, NEL, LS, PS) becomes U+000A.
// offsets[i] is the byte where code point i starts; one more entry holds where the text read
// ends. Reading stops after max_codepoints (*cut becomes true).
void decode(std::string_view text, std::uint32_t max_codepoints, std::vector<char32_t> *cps,
            std::vector<std::uint32_t> *offsets, bool *cut);

hb_unicode_general_category_t category(char32_t c);
hb_script_t script(char32_t c);
unsigned combining_class(char32_t c);

// Draws nothing by itself and may be absent from a font: joiners, direction marks, variation
// selectors, tags, the soft hyphen.
bool default_ignorable(char32_t c);
bool variation_selector(char32_t c);
bool regional_indicator(char32_t c);
bool emoji_modifier(char32_t c);
// Extended_Pictographic: the characters emoji sequences are built on.
bool pictographic(char32_t c);
// A space a line may end with without counting its width.
bool space(char32_t c);
// Strong right-to-left letters or explicit direction controls: text without any needs no
// bidirectional pass.
bool needs_bidi(char32_t c);
// Writing whose letters join or hang from a headline: no letter-spacing inside it.
bool joined_script(hb_script_t script);
// Scripts written without spaces between words that need more than "between any two characters".
bool unspaced_script(hb_script_t script);

// starts[i] is 1 when a user-perceived character (an extended grapheme cluster of UAX #29, with
// the conjuncts of the Indic scripts kept whole) starts at cps[i].
void grapheme_starts(const char32_t *cps, std::size_t count, std::vector<std::uint8_t> *starts);

enum Break : std::uint8_t
{
    no_break = 0,
    weak_break = 1,   // between clusters of a script written without spaces (may be inside a word)
    strong_break = 2, // after a space or a hyphen, between ideographs
};

// breaks[i]: may a line end before cps[i]? Only cluster starts are ever allowed. A subset of
// UAX #14 (see docs/afterglow/TEXT.md for what is left out).
void line_breaks(const char32_t *cps, std::size_t count, const std::vector<std::uint8_t> &starts,
                 std::vector<std::uint8_t> *breaks);

} // namespace tube::text::uni
