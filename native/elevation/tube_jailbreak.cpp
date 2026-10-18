/* ProsperoTube - bounded Lapy startup and durable storage gate.
 * Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "elevation.hpp"
#include "evo_jailbreak.h"
#include "evo_data_path.h"
#include "evo_boot_trace.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#ifdef EVO_APP_MODULE
static bool attempted, ready;

/* Write, read back, close and delete a file in the app's own folder. `error` is the
 * kernel's code when the folder cannot be made, otherwise errno. */
static bool prove_storage(int &error)
{
    error = 0;
    const int made = evo_mkdir("/data/youtube-maxd");
    if (made != 0) {
        error = made;
        return false;
    }
    char path[128];
    std::snprintf(path, sizeof path, "/data/youtube-maxd/.storage-probe-%ld",
                  static_cast<long>(getpid()));
    const char token[] = "ProsperoTube storage\n";
    char actual[sizeof token] = {};
    const int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        error = errno;
        return false;
    }
    bool passed = write(fd, token, sizeof token) == sizeof token &&
                  lseek(fd, 0, SEEK_SET) == 0 &&
                  read(fd, actual, sizeof actual) == sizeof actual &&
                  std::memcmp(token, actual, sizeof token) == 0;
    const int closed = close(fd);
    const int removed = unlink(path);
    passed = passed && closed == 0 && removed == 0;
    if (!passed) error = errno != 0 ? errno : EIO;
    return passed;
}

extern "C" int evo_jailbreak_self(void)
{
    if (attempted) return ready;
    attempted = true;
    /* As ProsperoEden logs it: the request first, then one line with the route, the
     * status and the ids the process was left with. Every outcome is logged. */
    evo_bt("Lapy: requesting filesystem access");
    const auto status = elevation::request(elevation::Capability::filesystem);
    const int uid = static_cast<int>(getuid()), euid = static_cast<int>(geteuid());
    const int gid = static_cast<int>(getgid()), egid = static_cast<int>(getegid());
    if (status != elevation::Status::ok) {
        evo_bt("Lapy: path=%s status=%u; durable storage unavailable; uid %d/%d gid %d/%d",
               elevation::path(), static_cast<unsigned>(status), uid, euid, gid, egid);
        return 0;
    }
    int error = 0;
    ready = prove_storage(error);
    evo_bt("Lapy: path=%s status=%u storage=%s error=%#x uid %d/%d gid %d/%d", elevation::path(),
           static_cast<unsigned>(status), ready ? "ready" : "failed",
           static_cast<unsigned>(ready ? 0 : error), uid, euid, gid, egid);
    return ready;
}

extern "C" int evo_jailbreak_ensure(void) { return ready; }
extern "C" int evo_jailbreak_is_open(void) { return ready; }
/* Startup is bounded. No resident/helper races or retries on the render thread. */
extern "C" int evo_jailbreak_poll(void) { return 0; }
#endif
