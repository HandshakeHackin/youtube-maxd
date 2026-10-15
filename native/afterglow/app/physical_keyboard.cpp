// Physical keyboards attached to the console; polled independently of the pad.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
#include "afterglow/app/physical_keyboard.hpp"
#include "afterglow/app/keyboard_input.hpp"
#include "afterglow/app/afterglow.h"
#include <ctime>
extern "C"
{
#include "evo_keyboard.h"
int sceSysmoduleLoadModule(std::uint16_t);
int sceUserServiceGetInitialUser(std::int32_t *);
int sceKeyboardInit();
int sceKeyboardOpen(std::int32_t, std::int32_t, std::int32_t, const void *);
int sceKeyboardRead(std::int32_t, void *, std::int32_t);
int sceKeyboardClose(std::int32_t);
}
namespace afterglow::keyboard
{
namespace
{
struct Device { int handle = -1; Input input; };
// Bound discovery by the library's handle capacity, not an assumed device count.
// Local probes validated indexes 0 and 1; unsupported indexes are simply rejected.
std::array<Device, 12> devices;
std::string typed;
bool ready = false;
bool suspended = false;
std::uint64_t retry_at = 0;
std::uint64_t now_us()
{
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return std::uint64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}
}
void poll(bool browser_active)
{
    typed.clear(); // an unseen page never receives old keystrokes on return
    const auto now = now_us();
    // Player and native dialogs own input independently. Leave the device
    // alone there, then rebaseline its queued reports when browsing resumes.
    if (!browser_active || !tube_afterglow_shown() || evo_keyboard_is_open())
    {
        suspended = true;
        for (auto &device : devices) device.input.repeat(false, now, typed);
        return;
    }
    if (!ready && now >= retry_at && tube_afterglow_enabled())
    {
        retry_at = now + 3000000;
        std::int32_t user = -1;
        if (sceUserServiceGetInitialUser(&user) >= 0 && user >= 0 &&
            sceSysmoduleLoadModule(0x0106) >= 0 && sceKeyboardInit() >= 0)
        {
            for (unsigned i = 0; i < devices.size(); ++i)
            {
                devices[i].handle = sceKeyboardOpen(user, 0, static_cast<int>(i), nullptr);
                ready = ready || devices[i].handle >= 0;
            }
            // Retain inactive accepted handles for hot-plug. With no accepted
            // handle, retry later instead of permanently disabling keyboards.
        }
    }
    const bool accept = !suspended;
    suspended = false;
    for (auto &device : devices)
    {
        if (device.handle < 0) continue;
        Report reports[16]{};
        const int count = sceKeyboardRead(device.handle, reports, 16);
        for (int i = 0; i < count && i < 16; ++i)
            device.input.sample(reports[i], accept, now, typed);
        if (count < 0)
            device.input = Input{};
        device.input.repeat(accept, now, typed);
    }
}
std::string_view text() { return typed; }
void close()
{
    for (auto &device : devices)
    {
        if (device.handle >= 0) sceKeyboardClose(device.handle);
        device = Device{};
    }
    typed.clear();
    ready = false;
    suspended = false;
    retry_at = 0;
}
}
extern "C" void tube_afterglow_keyboard_poll(int browser_active) { afterglow::keyboard::poll(browser_active != 0); }

extern "C" void tube_afterglow_keyboard_close(void) { afterglow::keyboard::close(); }
