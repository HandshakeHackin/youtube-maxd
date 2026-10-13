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

SOURCES = ("tube_update.c", "tube_update_zip.c", "tube_update_app.c")
HEADERS = ("tube_update.h", "tube_update_fs.h", "tube_update_sha256.h", "tube_update_zip.h")
TRANSPORT = "tube_update_http.c"
HOST = '''    // ProsperoTube update messages.
    if (m_providerId == "youtube") {
        if (const char* notice = tube_update_app_take_notice()) toast("UPDATE", notice);
        if (tube_update_app_take_refresh(evo_rmlui_provider_get_focused_id())) evo_rmlui_provider_reload();
        if (tube_update_app_take_restart()) { Application::getInstance().requestAutoClose(); return; }
    }
    // End ProsperoTube update messages.
'''


def apply_update_overlay(replace, player, host, work, root, embedded):
    source_dir, include_dir = work / player / "addons/src", work / player / "addons/include"
    for name in SOURCES + ((TRANSPORT,) if embedded else ()):
        shutil.copyfile(root / "native" / name, source_dir / name)
    for name in HEADERS:
        shutil.copyfile(root / "native" / name, include_dir / name)
    names = list(SOURCES) + ([TRANSPORT] if embedded else [])
    replace(player + "Makefile", "    addons/src/tube_backup.c",
            "    addons/src/tube_backup.c" + "".join(" \\\n    addons/src/" + name for name in names))
    if embedded:
        # The console's updater exists only beside the built-in service's client.
        flag = "addons/src/tube_provider.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1\n"
        replace(player + "Makefile", flag, flag + "addons/src/tube_update_app.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1\n")
    replace(host, '#include "evo_provider.h"\n#include "tube_provider.h"', '#include "evo_provider.h"\n#include "tube_provider.h"\n#include "tube_update.h"')
    anchor = "    // ProsperoTube player browse request: Open channel and Search left the player for this.\n"
    replace(host, anchor, HOST + anchor)


def retire_stale_objects(work, player, embedded):
    """Before the built-in service's own stamp is rewritten: the other mode's object must not be reused."""
    stamp = work / ".prosperotube-embedded-service"
    state = "1" if embedded else "0"
    if not stamp.exists() or stamp.read_text() != state:
        for suffix in (".o", ".d"):
            (work / player / ("addons/src/tube_update_app" + suffix)).unlink(missing_ok=True)
        if not embedded:   # a source the companion-only Makefile no longer names
            for suffix in (".c", ".o", ".d"):
                (work / player / ("addons/src/tube_update_http" + suffix)).unlink(missing_ok=True)
