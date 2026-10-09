/* ProsperoTube - bounded Lapy startup and durable storage gate.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "elevation.hpp"
#include "evo_jailbreak.h"
#include "evo_data_path.h"
#include "evo_boot_trace.h"
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#ifdef EVO_APP_MODULE
static bool attempted, ready;

extern "C" int evo_jailbreak_self(void)
{
    if (attempted) return ready;
    attempted = true;
    const auto status = elevation::request(elevation::Capability::filesystem);
    if (status != elevation::Status::ok) {
        evo_bt("Lapy: path=%s status=%u; durable storage unavailable",
               elevation::path(), static_cast<unsigned>(status));
        return 0;
    }
    if (evo_mkdir("/data/prosperotube") != 0) return 0;
    char path[128];
    std::snprintf(path, sizeof path, "/data/prosperotube/.storage-probe-%ld",
                  static_cast<long>(getpid()));
    const char token[] = "ProsperoTube storage\n";
    char actual[sizeof token] = {};
    const int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) return 0;
    ready = write(fd, token, sizeof token) == sizeof token &&
            lseek(fd, 0, SEEK_SET) == 0 &&
            read(fd, actual, sizeof actual) == sizeof actual &&
            std::memcmp(token, actual, sizeof token) == 0;
    const int closed = close(fd);
    const int removed = unlink(path);
    ready = ready && closed == 0 && removed == 0;
    evo_bt("Lapy: path=%s status=%u storage=%s", elevation::path(),
           static_cast<unsigned>(status), ready ? "ready" : "failed");
    return ready;
}

extern "C" int evo_jailbreak_ensure(void) { return ready; }
extern "C" int evo_jailbreak_is_open(void) { return ready; }
/* Startup is bounded. No resident/helper races or retries on the render thread. */
extern "C" int evo_jailbreak_poll(void) { return 0; }
#endif
