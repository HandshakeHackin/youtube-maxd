// ProsperoTube - Text engine tests: the fonts of this PC that stand in for the console's.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Nothing is downloaded and no font is kept in the repository. The tests read:
//   DejaVu Sans 2.37 (the fonts-dejavu-core package)       exact glyph numbers and advances
//   the fonts Windows installs (seen from WSL in /mnt/c/Windows/Fonts): Segoe UI, Yu Gothic,
//   Malgun Gothic, Microsoft YaHei and JhengHei, Leelawadee UI, Nirmala UI, Segoe UI Symbol,
//   Segoe UI Emoji                                         the other scripts and the pictures
// TUBE_TEXT_DEJAVU and TUBE_TEXT_WINDOWS_FONTS name other folders. A test whose fonts are absent
// says so and is counted as skipped.

#pragma once

#include "tube_text.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace text_host
{

inline bool exists(const std::string &path)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    std::fclose(file);
    return true;
}

inline std::string dejavu_folder()
{
    const char *folder = std::getenv("TUBE_TEXT_DEJAVU");
    return folder != nullptr && folder[0] != 0 ? folder : "/usr/share/fonts/truetype/dejavu";
}

inline std::string windows_folder()
{
    const char *folder = std::getenv("TUBE_TEXT_WINDOWS_FONTS");
    return folder != nullptr && folder[0] != 0 ? folder : "/mnt/c/Windows/Fonts";
}

inline std::string dejavu(const char *name = "DejaVuSans.ttf")
{
    return dejavu_folder() + "/" + name;
}

inline std::string windows(const char *name)
{
    return windows_folder() + "/" + name;
}

inline bool have_dejavu()
{
    return exists(dejavu());
}

inline bool have_windows()
{
    for (const char *name : {"segoeui.ttf", "YuGothR.ttc", "malgun.ttf", "msyh.ttc", "msjh.ttc", "LeelawUI.ttf",
                             "Nirmala.ttc", "seguisym.ttf"})
        if (!exists(windows(name)))
            return false;
    return true;
}

// The roles of the console's fonts, played by this PC's: `latin` is the interface face.
inline tube::text::Config stand_in(const std::string &latin)
{
    using tube::text::FaceRole;
    tube::text::Config config;
    config.raster_ms_per_frame = 0.0; // the glyph count alone: the same frames on every machine
    config.faces.push_back({latin, 0, FaceRole::latin, false});
    config.faces.push_back({windows("YuGothR.ttc"), 0, FaceRole::japanese, false});
    config.faces.push_back({windows("msyh.ttc"), 0, FaceRole::chinese, false});
    config.faces.push_back({windows("malgun.ttf"), 0, FaceRole::korean, false});
    config.faces.push_back({windows("LeelawUI.ttf"), 0, FaceRole::thai, false});
    config.faces.push_back({windows("segoeui.ttf"), 0, FaceRole::arabic, false});
    // What an app would carry itself: the console has no Hebrew or Indic font.
    config.faces.push_back({windows("Nirmala.ttc"), 0, FaceRole::other, false});
    config.faces.push_back({windows("seguisym.ttf"), 0, FaceRole::other, false});
    return config;
}

inline std::vector<std::string> fonts_used(const tube::text::Config &config)
{
    std::vector<std::string> paths;
    for (const tube::text::FaceSpec &face : config.faces)
        paths.push_back(face.path);
    return paths;
}

} // namespace text_host
