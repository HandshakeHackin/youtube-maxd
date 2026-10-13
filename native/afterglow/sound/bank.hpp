// ProsperoTube Afterglow - the recordings of the interface's cues, decoded once.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "afterglow/sound/cues.hpp"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace afterglow::sound
{

// A file of the package's sound folder by its name ("glass/focus_01.wav");
// false when the package has no such file.
using Reader = std::function<bool(const std::string &file, std::string *bytes)>;

// What serves each cue of cues.hpp: for every cue the first of its takes the
// package carries, with its variations ("_01", "_02", ...). A cue no file
// serves stays silent; that is not an error. After load() the bank never
// changes, so the audio thread may read its clips while the interface runs.
class Bank
{
  public:
    static constexpr int kVariations = 4;

    struct Recording
    {
        audio::Clip clip;
        float seconds = 0.0f;
    };
    struct Entry
    {
        const Take *take = nullptr; // nullptr: nothing serves the cue
        int count = 0;
        std::array<const Recording *, kVariations> recordings{};
    };
    struct Report
    {
        int files = 0;                   // decoded
        int served = 0;                  // cues with a recording
        int silent = 0;                  // cues without
        std::vector<std::string> errors; // files that are there and could not be used
    };

    Report load(const Reader &reader);
    const Entry &entry(int index) const
    {
        return entries_[static_cast<std::size_t>(index)];
    }
    // Decoded samples held, in bytes.
    std::size_t bytes() const;

  private:
    struct File
    {
        std::string name;
        std::vector<float> samples;
        Recording recording;
        bool usable = false;
    };
    const File *file(const Reader &reader, const std::string &name, Report *report);

    std::vector<std::unique_ptr<File>> files_;
    std::array<Entry, kMaxSpecs> entries_{};
};

} // namespace afterglow::sound
