// ProsperoTube Afterglow - what every source of the interface starts from.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The interface is drawn with the UI kit (ps5-homebrew-ui, fetched at the
// commit pinned in scripts/ui_kit.py). Its namespaces are used under their
// short names inside afterglow, as the kit's own sources use them.

#pragma once

#include "core/input.hpp"
#include "core/tween.hpp"
#include "gfx/draw_list.hpp"
#include "ui/feedback.hpp"
#include "ui/fonts.hpp"
#include "ui/glyphs.hpp"
#include "ui/motion.hpp"
#include "ui/theme.hpp"

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace afterglow
{

namespace gfx = hui::gfx;
namespace ui = hui::ui;
namespace tween = hui::tween;
namespace audio = hui::audio;

using gfx::Color;
using gfx::Rect;
using hui::Action;
using hui::Direction;
using hui::InputFrame;
using ui::Feedback;

inline bool pressed(const InputFrame &input, Action action)
{
    return (input.pressed & hui::action_bit(action)) != 0;
}
inline bool held(const InputFrame &input, Action action)
{
    return (input.held & hui::action_bit(action)) != 0;
}

inline Color clear()
{
    return Color{0.0f, 0.0f, 0.0f, 0.0f};
}
inline const Color kWhite = Color::rgb(0xffffff);
inline const Color kBlack = Color::rgb(0x000000);

// An interface text in the profile's language. english is the text as the
// source writes it and the key of native/i18n; the result lives until the
// language changes (strings.cpp). Texts that come from YouTube are not passed
// through here.
const char *tr(const char *english);
// Marks a text for the catalogs where it is not passed to tr() on the spot
// (a table of labels): the text itself, shown later through tr().
constexpr const char *tr_mark(const char *english)
{
    return english;
}
// A translated pattern with its values: fill(tr("Search in {0}"), name).
// A value that is a number goes through tr_n() instead, for the plural.
std::string fill(std::string_view pattern, std::string_view first, std::string_view second = {});
// A counted interface text: tr_n("{0} reply", "{0} replies", 3) is "3 replies"
// in English and the catalog's form for that number in the profile's language
// (one, few, many, other: the language's own rule). The catalogs key it by the
// second form; scripts/i18n_extract.py reads both from this call.
std::string tr_n(const char *one, const char *other, long count);

inline bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}
// Parts joined by a middle dot, the empty ones left out: "Quarterline · 1.9M views".
std::string dotted(std::initializer_list<std::string_view> parts);
// The number a text starts with ("3", "12 new"); 0 for none.
int leading_number(std::string_view text);

} // namespace afterglow
