"""Compile the generated shipping browser's update hookup: notice, redraw and close."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home()/".cache/prosperotube/native"))
player = work/"projects/evoplayer"
source = (player/"core/src/screens/ProviderHostScreen.cpp").read_text()
a = source.index("    // ProsperoTube update messages.")
b = source.index("    // End ProsperoTube update messages.", a)
body = source[a:b]
assert '#include "tube_update.h"' in source
# Ending the process, not parking it: the exit the phone overlay added to the application.
assert "requestAutoClose()" in body and "void Application::requestAutoClose()" in (player/"core/src/Application.cpp").read_text()
# Runs before the selection of the frame is taken, in the browser's per-frame function.
assert source.index("void ProviderHostScreen::startSelected()") < a < source.index("    evo_provider_selection_t sel;", a)
makefile = (player/"Makefile").read_text()
for name in ("tube_update.c", "tube_update_zip.c", "tube_update_app.c"):
    assert "    addons/src/" + name in makefile and (player/"addons/src"/name).is_file(), name
for name in ("tube_update.h", "tube_update_fs.h", "tube_update_sha256.h", "tube_update_zip.h"):
    assert (player/"addons/include"/name).is_file(), name
# The transport and the console's updater belong to the package that carries the built-in service.
embedded = "addons/src/tube_js_net.o" in makefile
assert ("    addons/src/tube_update_http.c" in makefile) == embedded
assert ("addons/src/tube_update_app.o: CFLAGS += -DTUBE_EMBEDDED_SERVICE=1" in makefile) == embedded
scaffold = r'''#include <cassert>
#include <string>
static const char* next_notice; static const char* focused = "";
static int refresh, restart, shown, reloads, closed, asked;
static const char* tube_update_app_take_notice() { auto value = next_notice; next_notice = nullptr; return value; }
static int tube_update_app_take_refresh(const char* id) { ++asked; assert(id == focused); auto value = refresh; refresh = 0; return value; }
static int tube_update_app_take_restart() { auto value = restart; restart = 0; return value; }
static const char* evo_rmlui_provider_get_focused_id() { return focused; }
static void evo_rmlui_provider_reload() { ++reloads; }
static void toast(const char* heading, const char* message) { assert(std::string(heading) == "UPDATE" && std::string(message) == "Version 01.000.010 is available."); ++shown; }
struct Application { static Application& getInstance() { static Application app; return app; } void requestAutoClose() { ++closed; } };
struct Host { std::string m_providerId; int reached = 0; void frame() {
'''
ending = r'''
    ++reached;
} };
int main() {
    Host host{"local"}; next_notice = "Version 01.000.010 is available."; refresh = restart = 1;
    host.frame(); assert(!shown && !reloads && !closed && !asked && host.reached == 1);
    host.m_providerId = "youtube"; focused = "settings:update:cancel";
    host.frame(); assert(shown == 1 && reloads == 1 && closed == 1 && host.reached == 1);   /* closing ends the frame */
    host.frame(); assert(shown == 1 && reloads == 1 && closed == 1 && host.reached == 2);
    refresh = 1; host.frame(); assert(reloads == 2 && host.reached == 3);
}
'''
with tempfile.TemporaryDirectory(prefix="tube-update-host-") as directory:
    path = Path(directory); (path/"test.cpp").write_text(scaffold+body+ending)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-g", "-O1",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", str(path/"test.cpp"), "-o", str(path/"test")], check=True)
    subprocess.run([str(path/"test")], check=True, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=1"})
print("PASS: shipping update hookup: notice toast, About redraw on the focused update row, close after the replacement; sources in the package")
