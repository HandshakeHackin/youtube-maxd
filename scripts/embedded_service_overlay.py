"""The built-in YouTube service in the app: host wiring, and the package build.

Two parts. `apply_host` always applies: the calls it adds ask the provider at
run time whether the built-in service is this run's source, and do nothing
when it is not. `apply_package` applies only when TUBE_EMBEDDED_SERVICE=1
(scripts/build_ps5.sh sets it by default; a plain prepare_native.py, as the
host tests run it, does not): it compiles the JavaScript engine, the native
bindings and the service into the app and embeds the service bundle and the
root certificates.

The service: every standalone/src/*.c except main.c (the host tool), with the
headers of standalone/src and standalone/include, and the bundle
PROSPEROTUBE_JS_BUNDLE (default standalone/build/tube_js.bundle.js, which
scripts/build_ps5.sh builds when it is missing or older than its sources).
SPDX-License-Identifier: GPL-3.0-or-later
"""
import os
import shutil
import subprocess
import sys

BINDINGS = ("tube_js_platform", "tube_js_http", "tube_js_console")
# tube_js_http.c and tube_js_console.c reach the link only inside tube_js_net.o:
# joined with libcurl under --wrap=fcntl, then reduced to their own interface.
MAKE = """C_OBJS := $(C_SRCS:.c=.o) addons/src/tube_js_net.o
addons/src/quickjs-amalgam.o: CFLAGS += -w
addons/src/tube_provider.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1
TUBE_JS_LIB := $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib
addons/src/tube_js_net.o: addons/src/tube_js_http.o addons/src/tube_js_console.o
\t$(LD) -r --wrap=fcntl -o $@ $^ $(TUBE_JS_LIB)/libcurl.a $(TUBE_JS_LIB)/libpsl.a
\t$(OBJCOPY) --wildcard --keep-global-symbol='tube_js_*' --keep-global-symbol='ZSTD_trace_*' $@
"""
NOTICES = {"QuickJS-NG-MIT.txt": "QuickJS-NG-MIT.txt", "curl-COPYING.txt": "curl-COPYING.txt", "libpsl-MIT.txt": "libpsl-MIT.txt",
           "MPL-2.0.txt": "Mozilla-CA-MPL-2.0.txt", "YouTube.js-MIT.txt": "YouTube.js-MIT.txt"}


def enabled():
    return os.environ.get("TUBE_EMBEDDED_SERVICE", "0") == "1"


def apply_host(replace, player, host):
    # L3 starts voice search only with the PC companion; the built-in service has none yet.
    replace(host, 'if (m_providerId == "youtube" && (pressed & PadButtons::L3)) {',
            'if (m_providerId == "youtube" && !tube_embedded_mode() && (pressed & PadButtons::L3)) {')
    # First soft-close frame, before the park: requests cancelled, service stopped and joined.
    replace(player + "core/src/Application.cpp", 'evo_boot_log("soft close: media released");',
            'tube_embedded_stop();\n                evo_boot_log("soft close: media released");')
    # One log line says whether the service is up and where; the token is never logged.
    replace(player + "core/src/Application.cpp", "    evo_provider_mgr_init();\n", """    evo_provider_mgr_init();
    if (tube_embedded_mode()) {
        if (*tube_embedded_address()) evo_boot_log("embedded service: listening on %s", tube_embedded_address());
        else evo_boot_log("embedded service: did not start: %s", tube_embedded_error());
    }
""")
    # Artwork addresses pass the provider's check before anything is fetched.
    art = player + "ui_rml/src/evo_rmlui_provider_art.cpp"
    replace(art, '#include "evo_provider_bundle.h"', '#include "evo_provider_bundle.h"\n#include "tube_provider.h"')
    replace(art, '''    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return -1;
''', '''    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0)
        return -1;
    if (!tube_art_url_allowed(url))
        return -1;
''')


def service_inputs(root):
    """(sources, headers, bundle) of the service this package carries."""
    interface = root / "standalone/include/tube_service.h"
    sources = sorted(path for path in (root / "standalone/src").glob("*.c") if path.name != "main.c")
    headers = sorted((root / "standalone/src").glob("*.h")) + sorted((root / "standalone/include").glob("*.h"))
    bundle = root / "standalone/build/tube_js.bundle.js"
    if os.environ.get("PROSPEROTUBE_JS_BUNDLE"):
        bundle = type(root)(os.environ["PROSPEROTUBE_JS_BUNDLE"])
    missing = [str(path) for path in (interface, bundle) if not path.is_file()]
    if not sources:
        missing.append(str(root / "standalone/src/*.c"))
    if missing:
        raise RuntimeError("TUBE_EMBEDDED_SERVICE=1 needs the built-in service, and these are missing: " + ", ".join(missing) +
                           ". Build the bundle with standalone/build_host.sh, "
                           "or set TUBE_EMBEDDED_SERVICE=0 for a companion-only package.")
    return sources, headers, bundle


def apply_package(replace, player, work, root):
    tools = root / "standalone/ps5"
    sources, headers, bundle = service_inputs(root)
    engine = type(root)(subprocess.run([str(tools / "fetch_quickjs.sh")], check=True, capture_output=True, text=True).stdout.strip())
    roots = subprocess.run([str(tools / "fetch_cacert.sh")], check=True, capture_output=True, text=True).stdout.strip()
    source_dir, include_dir = work / player / "addons/src", work / player / "addons/include"
    shutil.copyfile(engine / "quickjs-amalgam.c", source_dir / "quickjs-amalgam.c")
    shutil.copyfile(engine / "quickjs.h", include_dir / "quickjs.h")
    for name in BINDINGS:
        shutil.copyfile(tools / (name + ".c"), source_dir / (name + ".c"))
        shutil.copyfile(tools / (name + ".h"), include_dir / (name + ".h"))
    shutil.copyfile(tools / "tube_js_libc.c", source_dir / "tube_js_libc.c")
    for path in headers:
        shutil.copyfile(path, include_dir / path.name)
    names = ["quickjs-amalgam.c", "tube_js_platform.c", "tube_js_libc.c", "tube_js_data.c"]
    for path in sources:
        if path.name in names or path.stem in BINDINGS:
            raise RuntimeError("Service source name collides with the bindings: " + path.name)
        shutil.copyfile(path, source_dir / path.name)
        names.append(path.name)
    subprocess.run([sys.executable, str(tools / "embed_data.py"), str(source_dir / "tube_js_data.c"),
                    f"tube_js_ca_pem={roots}", f"tube_js_bundle={bundle}"], check=True)
    shutil.copyfile(source_dir / "tube_js_data.h", include_dir / "tube_js_data.h")
    replace(player + "Makefile", "    addons/src/tube_provider.c",
            "    addons/src/tube_provider.c" + "".join(" \\\n    addons/src/" + name for name in names))
    replace(player + "Makefile", "C_OBJS := $(C_SRCS:.c=.o)\n", MAKE)
    notices = work / player / "assets/prosperotube-notices"
    for name, shipped in NOTICES.items():
        shutil.copyfile(root / "native/licenses" / name, notices / shipped)
    # Words on screen that name the companion.
    rml = player + "assets/rml/youtube.rml"
    replace(rml, "Connecting to your companion… Circle to cancel.", "Loading… Circle to cancel.")
    replace(rml, "Square to search. Triangle saves a video. Options pairs your companion.", "Square to search. Triangle saves a video.")
    replace(rml, '            <button id="tv-card-setup-url" class="tv-setup-card" data-if="!discussion" data-event-click="setup_url()">PAIR COMPANION</button>\n', "")
    replace(rml, '            <span class="tube-key">HOLD L3</span> Voice search (experimental)\n', "")
    replace(rml, '            <span class="tube-key">OPTIONS</span> Companion\n', "")
    replace(player + "core/src/screens/AboutSupportScreen.cpp", "LAN COMPANION REQUIRED", "BUILT-IN YOUTUBE SERVICE")


def retire_stale_objects(work, player):
    """The Makefile tracks sources, not flags: the provider object of the other mode must not be reused."""
    stamp = work / ".prosperotube-embedded-service"
    state = "1" if enabled() else "0"
    if not stamp.exists() or stamp.read_text() != state:
        for suffix in (".o", ".d"):
            (work / player / ("addons/src/tube_provider" + suffix)).unlink(missing_ok=True)
        stamp.write_text(state)
