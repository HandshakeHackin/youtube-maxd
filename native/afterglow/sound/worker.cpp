// ProsperoTube Afterglow - the turn of the thread that plays and vibrates.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/sound/worker.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::sound
{

Worker::Worker(audio::Mixer &mixer, AudioDevice &audio, PadDevice &pad) : mixer_(mixer), device_(audio), pad_(pad) {}

void Worker::wake()
{
    posted_.fetch_add(1, std::memory_order_release);
}

void Worker::pulse(float strength, float seconds)
{
    const std::uint64_t level = static_cast<std::uint64_t>(std::lround(std::clamp(strength, 0.0f, 1.0f) * 65535.0f));
    const std::uint64_t ms = static_cast<std::uint64_t>(std::lround(std::clamp(seconds, 0.0f, 60.0f) * 1000.0f));
    if (level == 0 || ms == 0)
        return;
    pulse_.store(static_cast<std::uint64_t>(++sequence_ | 0x80000000u) << 32 | level << 16 | ms, std::memory_order_release);
}

void Worker::park(float seconds)
{
    park_ms_.store(std::max(1, static_cast<int>(std::lround(seconds * 1000.0f))), std::memory_order_release);
}

void Worker::quit()
{
    quit_.store(true, std::memory_order_release);
}

void Worker::stop_pad()
{
    // A stop that is refused is tried again on the next turn: motors left
    // running would be worse than any missing pulse.
    if (pad_.vibrate(0.0f))
    {
        rumbling_ = false;
        pad_errors_ = 0;
    }
    else if (++pad_errors_ >= kPadErrors)
    {
        rumbling_ = false;
        pad_failed_.store(true, std::memory_order_relaxed);
    }
}

void Worker::release()
{
    if (rumbling_)
    {
        pad_.vibrate(0.0f);
        rumbling_ = false;
    }
    if (audio_.load(std::memory_order_relaxed) == Audio::open)
    {
        device_.close();
        audio_.store(Audio::closed, std::memory_order_relaxed);
    }
    finished_.store(true, std::memory_order_release);
}

int Worker::turn(std::int64_t now)
{
    if (finished_.load(std::memory_order_relaxed))
        return -1;
    if (quit_.load(std::memory_order_acquire))
    {
        release();
        return -1;
    }
    if (!parking_)
    {
        const int budget = park_ms_.load(std::memory_order_acquire);
        if (budget > 0)
        {
            parking_ = true;
            park_until_ = now + static_cast<std::int64_t>(budget) * 1000;
            if (rumbling_)
                stop_pad();
        }
    }

    // ---- vibration ----
    const std::uint64_t request = pulse_.exchange(0, std::memory_order_acq_rel);
    if (request != 0 && !parking_ && !pad_failed_.load(std::memory_order_relaxed))
    {
        const float strength = static_cast<float>((request >> 16) & 0xffffu) / 65535.0f;
        const std::int64_t ms = static_cast<std::int64_t>(request & 0xffffu);
        if (pad_.vibrate(strength))
        {
            pad_errors_ = 0;
            rumbling_ = true;
            rumble_until_ = now + ms * 1000;
        }
        else if (++pad_errors_ >= kPadErrors)
        {
            pad_failed_.store(true, std::memory_order_relaxed);
        }
    }
    if (rumbling_ && now >= rumble_until_)
        stop_pad();

    // ---- sound ----
    if (parking_ && now >= park_until_)
    {
        release();
        return -1;
    }
    const std::uint32_t posted = posted_.load(std::memory_order_acquire);
    const bool news = posted != seen_;
    Audio state = audio_.load(std::memory_order_relaxed);
    if (news && state == Audio::closed && !parking_)
    {
        // The first voice opens the port: an interface that stays silent never asks for one.
        state = device_.open() ? Audio::open : Audio::failed;
        audio_.store(state, std::memory_order_relaxed);
    }
    if (state == Audio::open && (news || tail_ > 0 || mixer_.active_voices() > 0))
    {
        seen_ = posted;
        mixer_.render(grain_, kGrain);
        if (!device_.write(grain_))
        {
            write_errors_.fetch_add(1, std::memory_order_relaxed);
            if (++refused_ >= kWriteErrors)
            {
                device_.close();
                audio_.store(Audio::failed, std::memory_order_relaxed);
            }
            return 5000;
        }
        refused_ = 0;
        grains_.fetch_add(1, std::memory_order_relaxed);
        // One silent grain after the last voice, then nothing is written until the next.
        tail_ = mixer_.active_voices() > 0 ? 1 : std::max(0, tail_ - 1);
        return 0;
    }
    seen_ = posted;
    if (parking_ && !rumbling_)
    {
        release();
        return -1;
    }
    if (rumbling_)
        return static_cast<int>(std::clamp<std::int64_t>(rumble_until_ - now, 500, 2000));
    return kIdle;
}

} // namespace afterglow::sound
