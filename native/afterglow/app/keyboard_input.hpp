// Physical keyboard reports and US-layout text input.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace afterglow::keyboard
{
// Device-tested interoperability ABI: ps5-native-gamepad-input-research/KEYBOARD-MOUSE.md.
struct Report
{
    std::uint64_t timestamp_us;
    std::uint8_t intercepted, reserved0[7], connected, reserved1[3];
    std::int32_t length;
    std::uint32_t leds, modifiers;
    std::uint16_t keys[16];
    std::uint8_t reserved2[32];
};
static_assert(sizeof(Report) == 96);
static_assert(offsetof(Report, connected) == 0x10);
static_assert(offsetof(Report, modifiers) == 0x1c);
static_assert(offsetof(Report, keys) == 0x20);

inline char character(std::uint16_t key, std::uint32_t mods, std::uint32_t leds)
{
    if (mods & 0xdd) // Control, Alt/AltGr and GUI are not ordinary text.
        return 0;
    const bool shift = (mods & 0x22) != 0;
    if (key >= 4 && key <= 29)
        return static_cast<char>((shift != ((leds & 2) != 0) ? 'A' : 'a') + key - 4);
    if (key >= 30 && key <= 39)
        return (shift ? "!@#$%^&*()" : "1234567890")[key - 30];
    switch (key)
    {
    case 40: case 88: return '\r';
    case 41: return '\x1b';
    case 42: return '\b';
    case 44: return ' ';
    case 45: return shift ? '_' : '-';
    case 46: return shift ? '+' : '=';
    case 47: return shift ? '{' : '[';
    case 48: return shift ? '}' : ']';
    case 49: return shift ? '|' : '\\';
    case 51: return shift ? ':' : ';';
    case 52: return shift ? '"' : '\'';
    case 53: return shift ? '~' : '`';
    case 54: return shift ? '<' : ',';
    case 55: return shift ? '>' : '.';
    case 56: return shift ? '?' : '/';
    case 84: return '/';
    case 85: return '*';
    case 86: return '-';
    case 87: return '+';
    default:
        if ((leds & 1) && key >= 89 && key <= 99)
            return "1234567890."[key - 89];
        return 0;
    }
}

class Input
{
  public:
    void sample(const Report &report, bool accept, std::uint64_t now, std::string &out)
    {
        if (report.intercepted)
            intercepted_ = true;
        else if (intercepted_)
        {
            accept = false; // rebaseline held keys after the system gives input back
            intercepted_ = false;
        }
        std::array<std::uint16_t, 16> current{};
        if (report.connected && !report.intercepted)
            for (unsigned i = 0; i < current.size(); ++i)
                current[i] = report.keys[i];
        for (unsigned i = 0; i < current.size(); ++i)
        {
            const auto key = current[i];
            bool duplicate = false;
            for (unsigned j = 0; j < i; ++j)
                duplicate |= current[j] == key;
            if (!key || duplicate || contains(previous_, key))
                continue;
            if (const char ch = character(key, report.modifiers, report.leds); accept && ch)
            {
                append(out, ch);
                repeat_key_ = ch == '\r' || ch == '\x1b' ? 0 : key;
                repeat_at_ = now + 400000;
            }
        }
        previous_ = current;
        modifiers_ = report.modifiers;
        leds_ = report.leds;
        if (!accept || !contains(current, repeat_key_))
            repeat_key_ = 0;
    }
    void repeat(bool accept, std::uint64_t now, std::string &out)
    {
        if (!accept)
            repeat_key_ = 0;
        if (repeat_key_ && now >= repeat_at_)
        {
            if (const char ch = character(repeat_key_, modifiers_, leds_))
                append(out, ch);
            repeat_at_ = now + 45000; // no catch-up burst after a slow frame
        }
    }
  private:
    static bool contains(const std::array<std::uint16_t, 16> &keys, std::uint16_t key)
    {
        if (!key) return false;
        for (auto held : keys) if (held == key) return true;
        return false;
    }
    static void append(std::string &out, char ch)
    {
        if (out.size() < 256) out.push_back(ch);
    }
    std::array<std::uint16_t, 16> previous_{};
    bool intercepted_ = false;
    std::uint16_t repeat_key_ = 0;
    std::uint64_t repeat_at_ = 0;
    std::uint32_t modifiers_ = 0, leds_ = 0;
};
} // namespace afterglow::keyboard
