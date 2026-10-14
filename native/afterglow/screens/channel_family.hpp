// ProsperoTube Afterglow - what the channel, channels and subscriptions screens share.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the three screens of this family share beyond the design language and
// the data layer.
//

#pragma once

#include "afterglow/data_feed.hpp"
#include "afterglow/screen.hpp"
#include "ui/components/tabs.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace afterglow::family
{
inline float under_ring(const Rect &ring, const Rect &thing, float reach)
{
    return tween::clamp01(1.0f - std::hypot(ring.cx() - thing.cx(), ring.cy() - thing.cy()) / reach);
}
} // namespace afterglow::family
