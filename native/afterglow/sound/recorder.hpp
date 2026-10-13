// ProsperoTube Afterglow - what was heard and felt, written down instead of played.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// For the tests and the host harness only: a Recorder is the Output of a Sound
// that has no device, and a Tape renders the mixer as the console's port would
// hear it, optionally into a WAV file to listen to on the PC. Both are
// deterministic: the same steps give the same record.

#pragma once

#include "afterglow/sound/sound.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace afterglow::sound
{

class Recorder final : public Output
{
  public:
    struct Pulse
    {
        float strength;
        float seconds;
    };
    std::vector<Heard> heard;
    std::vector<Pulse> pulses;
    bool broken = false; // the audio device "failed"

    bool audio() const override
    {
        return !broken;
    }
    void voice(const Heard &voice) override
    {
        heard.push_back(voice);
    }
    void pulse(float strength, float seconds) override
    {
        pulses.push_back({strength, seconds});
    }
    // The names of what was heard, in order: "focus tab error".
    std::string names() const
    {
        std::string all;
        for (const Heard &voice : heard)
            all += (all.empty() ? "" : " ") + std::string(voice.name);
        return all;
    }
};

class Tape
{
  public:
    ~Tape()
    {
        close();
    }
    // 48 kHz stereo S16 WAV. False when the file cannot be written.
    bool open(const std::string &path)
    {
        close();
        file_ = std::fopen(path.c_str(), "wb");
        if (file_ == nullptr)
            return false;
        const unsigned char header[44] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0,
                                          2,   0,   0x80, 0xbb, 0, 0, 0, 0xee, 2, 0, 4, 0, 16, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0};
        std::fwrite(header, 1, sizeof header, file_);
        written_ = 0;
        return true;
    }
    // Renders the next frames of the mixer, as the port's thread would.
    void run(audio::Mixer &mixer, int frames)
    {
        buffer_.resize(static_cast<std::size_t>(frames) * 2u);
        mixer.render(buffer_.data(), frames);
        for (const std::int16_t sample : buffer_)
            peak_ = std::max(peak_, std::abs(static_cast<int>(sample)));
        frames_ += static_cast<std::uint64_t>(frames);
        if (file_ != nullptr)
            written_ += std::fwrite(buffer_.data(), 1, buffer_.size() * sizeof(std::int16_t), file_);
    }
    void close()
    {
        if (file_ == nullptr)
            return;
        const auto put = [&](long at, std::uint32_t value)
        {
            const unsigned char bytes[4] = {static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
                                            static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24)};
            std::fseek(file_, at, SEEK_SET);
            std::fwrite(bytes, 1, 4, file_);
        };
        put(4, static_cast<std::uint32_t>(written_ + 36));
        put(40, static_cast<std::uint32_t>(written_));
        std::fclose(file_);
        file_ = nullptr;
    }
    // The loudest sample rendered so far, 0..32767.
    int peak() const
    {
        return peak_;
    }
    std::uint64_t frames() const
    {
        return frames_;
    }

  private:
    std::FILE *file_ = nullptr;
    std::size_t written_ = 0;
    std::vector<std::int16_t> buffer_;
    std::uint64_t frames_ = 0;
    int peak_ = 0;
};

} // namespace afterglow::sound
