"""The update check and self-update in the app: sources, and the host's part.

The engine (native/tube_update*.c) is compiled into every package; its HTTPS
transport and the console's updater only into one that carries the built-in
service, whose client and root certificates they use. A companion-only package
links the same functions and they do nothing.

The host shows the updater's notices, redraws About while an update row has
the focus and its state changes, and once the new files are in place ends the
app the way a disconnecting phone can: media released, GPU parked, then the
PS5's own exit (scripts/phone_exit_overlay.py). A parked app would keep running
the old program. SPDX-License-Identifier: GPL-3.0-or-later
"""
import shutil

SOURCES = ("tube_update.c", "tube_update_app.c")
HEADERS = ("tube_update.h", "tube_update_paths.h")
KIT = ("self_update.c", "self_update_sha256.c", "update_check.c")
KIT_HEADERS = ("self_update.h", "self_update_sha256.h", "self_update_protocol.h", "update_check.h", "console_curl.h")
HOST = '''    // ProsperoTube update messages.
    if (m_providerId == "youtube") {
        if (const char* notice = tube_update_app_take_notice()) toast("UPDATE", notice);
        if (tube_update_app_take_refresh(evo_rmlui_provider_get_focused_id())) evo_rmlui_provider_reload();
        if (tube_update_app_take_restart()) { Application::getInstance().requestAutoClose(); return; }
    }
    // End ProsperoTube update messages.
'''


def apply_update_overlay(replace, player, host, work, root, embedded):
    shutil.copyfile(root / "native/update/third_party/miniz/LICENSE",
                    work / player / "assets/prosperotube-notices/miniz-MIT.txt")
    source_dir, include_dir = work / player / "addons/src", work / player / "addons/include"
    for name in SOURCES + (("tube_update_console.c",) if embedded else ()):
        shutil.copyfile(root / "native" / name, source_dir / name)
    for name in HEADERS:
        shutil.copyfile(root / "native" / name, include_dir / name)
    for name in KIT_HEADERS:
        shutil.copyfile(root / "native/update/kit" / name, include_dir / name)
    for name in KIT + (("self_update_ps5.c",) if embedded else ()):
        shutil.copyfile(root / "native/update/kit" / name, source_dir / name)
    names = list(SOURCES + KIT) + (["tube_update_console.c", "self_update_ps5.c"] if embedded else [])
    replace(player + "Makefile", "    addons/src/tube_backup.c",
            "    addons/src/tube_backup.c" + "".join(" \\\n    addons/src/" + name for name in names))
    if embedded:
        # Both users share the already-wrapped curl archive; retain its public API.
        replace(player + "Makefile", "--keep-global-symbol='tube_js_*'",
                "--keep-global-symbol='tube_js_*' --keep-global-symbol='curl_*'")
        flag = "addons/src/tube_provider.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1\n"
        replace(player + "Makefile", flag, flag + "addons/src/tube_update_app.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1\n"
                + "addons/src/self_update_ps5.o: CFLAGS += -include addons/include/tube_update_paths.h\n")
        replace("scripts/package-app.sh", 'begin "inspecting signed containers"',
                'cp "${REPO_ROOT}/prosperotube-update/self-updater.elf" "${APPDIR}/self-updater.elf"\nbegin "inspecting signed containers"')
    else:
        replace(player + "Makefile", "C_OBJS := $(C_SRCS:.c=.o)",
                "addons/src/update_check.o: CFLAGS += -DUPDATE_CHECK_NO_NETWORK\nC_OBJS := $(C_SRCS:.c=.o)")
    replace(host, '#include "evo_provider.h"\n#include "tube_provider.h"', '#include "evo_provider.h"\n#include "tube_provider.h"\n#include "tube_update.h"')
    anchor = "    // ProsperoTube player browse request: Open channel and Search left the player for this.\n"
    replace(host, anchor, HOST + anchor)


def retire_stale_objects(work, player, embedded):
    transport = work / ".prosperotube-update-curl"
    if embedded and not transport.exists():
        (work / player / "addons/src/tube_js_net.o").unlink(missing_ok=True)
        transport.write_text("1")
    stamp = work / ".prosperotube-embedded-service"
    if not stamp.exists() or stamp.read_text() != ("1" if embedded else "0"):
        for name in ("tube_update_app", "update_check"):
            for suffix in (".o", ".d"):
                (work / player / "addons/src" / (name + suffix)).unlink(missing_ok=True)
    for name in ("tube_update_http", "tube_update_zip") + (() if embedded else ("tube_update_console", "self_update_ps5")):
        for suffix in (".c", ".o", ".d"):
            (work / player / "addons/src" / (name + suffix)).unlink(missing_ok=True)
    for name in ("tube_update_fs.h", "tube_update_sha256.h", "tube_update_zip.h"):
        (work / player / "addons/include" / name).unlink(missing_ok=True)
