// ProsperoTube Afterglow - the recordings of the interface's cues, decoded once.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/sound/bank.hpp"

#include "audio/wav.hpp"

namespace afterglow::sound
{

// A file is read and decoded once however many cues it serves (the stand-ins
// share recordings), and a name that is not there is asked for only once.
const Bank::File *Bank::file(const Reader &reader, const std::string &name, Report *report)
{
    for (const auto &known : files_)
    {
        if (known->name == name)
            return known->usable ? known.get() : nullptr;
    }
    auto entry = std::make_unique<File>();
    entry->name = name;
    std::string bytes;
    if (reader && reader(name, &bytes))
    {
        audio::DecodedWav wav = audio::decode_wav(bytes);
        if (wav.ok() && wav.frames > 1)
        {
            entry->samples = std::move(wav.samples);
            entry->recording.clip.samples = entry->samples.data();
            entry->recording.clip.frames = wav.frames;
            entry->recording.seconds = static_cast<float>(wav.frames) / static_cast<float>(audio::kSampleRate);
            entry->usable = true;
            ++report->files;
        }
        else
        {
            report->errors.push_back(name + ": " + (wav.ok() ? std::string("empty") : wav.error));
        }
    }
    files_.push_back(std::move(entry));
    return files_.back()->usable ? files_.back().get() : nullptr;
}

Bank::Report Bank::load(const Reader &reader)
{
    Report report;
    files_.clear();
    entries_ = {};
    for (std::size_t index = 0; index < spec_count(); ++index)
    {
        const CueSpec &spec = spec_at(index);
        Entry &entry = entries_[index];
        for (const Take &take : spec.takes)
        {
            if (take.file == nullptr)
                break;
            for (int variation = 1; variation <= kVariations; ++variation)
            {
                const char suffix[] = {'_', static_cast<char>('0' + variation / 10), static_cast<char>('0' + variation % 10), 0};
                const File *found = file(reader, std::string(take.file) + suffix + ".wav", &report);
                if (found == nullptr)
                    break;
                entry.recordings[static_cast<std::size_t>(entry.count++)] = &found->recording;
            }
            if (entry.count > 0)
            {
                entry.take = &take;
                break;
            }
        }
        ++(entry.count > 0 ? report.served : report.silent);
    }
    return report;
}

std::size_t Bank::bytes() const
{
    std::size_t total = 0;
    for (const auto &known : files_)
        total += known->samples.size() * sizeof(float);
    return total;
}

} // namespace afterglow::sound
