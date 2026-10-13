// The Circle blocks of the generated provider screen, compiled as they ship:
// typed search results (one press or two) and the main menu (nothing, two
// presses to close, or close at once). SPDX-License-Identifier: GPL-3.0-or-later
#include "tube_ui_style.h"
#include <cassert>
#include <cstdio>
#include <string>

static int g_results, g_closed, g_setup, g_back, g_back_consumes, g_app_mode, g_search_double;
static uint64_t g_now, g_app_armed, g_search_armed;
static std::string g_toast;
static int tube_app_exit_press(uint64_t now) { g_search_armed = 0; return tube_ui_press(g_app_mode, now, &g_app_armed); }
static int tube_search_exit_press(uint64_t now) {
    g_app_armed = 0; return tube_ui_press(g_search_double ? TUBE_UI_EXIT_DOUBLE : TUBE_UI_EXIT_SINGLE, now, &g_search_armed);
}
static uint64_t tube_ui_now_ms() { return g_now; }
static int evo_rmlui_provider_search_results() { return g_results; }
static void evo_rmlui_provider_show_setup() { ++g_setup; }
static void toast(const char* title, const char* text) { g_toast = std::string(title) + ": " + text; }
struct Application {
    static Application& getInstance() { static Application app; return app; }
    void requestSoftClose() { ++g_closed; }
};

/* The Circle branch of the provider screen around the two generated blocks. */
static bool circle(const std::string& m_providerId) {
#include "search_exit.inc"
    if (g_back_consumes) { ++g_back; return true; }      /* the browser popped a level or left the results */
    if (m_providerId == "youtube") {
#include "app_exit.inc"
        evo_rmlui_provider_show_setup(); return true;
    }
    return true;
}

int main() {
    /* Defaults: results leave on one press; the main menu keeps what Circle did before. */
    g_results = 1; g_back_consumes = 1; g_now = 1000;
    assert(circle("youtube") && g_back == 1 && g_toast.empty());
    g_results = 0; g_back_consumes = 0;
    assert(circle("youtube") && g_setup == 1 && !g_closed && g_toast.empty());
    /* Two presses to leave the results. */
    g_search_double = 1; g_results = 1; g_back_consumes = 1; g_now = 2000;
    assert(circle("youtube") && g_back == 1 && g_toast == "SEARCH: Press Circle again to leave the results");
    g_now = 3500; g_toast.clear();
    assert(circle("youtube") && g_back == 2 && g_toast.empty());
    g_now = 4000; assert(circle("youtube") && g_back == 2);          /* armed again */
    g_now = 6001; assert(circle("youtube") && g_back == 2);          /* too late: asks again */
    g_now = 6500; assert(circle("youtube") && g_back == 3);
    /* A deeper level is not a results list: Back acts at once. */
    g_results = 0; g_now = 7000; g_toast.clear();
    assert(circle("youtube") && g_back == 4 && g_toast.empty());
    /* Another provider never sees these rules. */
    g_results = 1; g_now = 8000;
    assert(circle("iptv") && g_back == 5 && g_toast.empty());
    /* The main menu: press twice to close. */
    g_results = 0; g_back_consumes = 0; g_app_mode = TUBE_UI_EXIT_DOUBLE; g_now = 9000; g_setup = 0;
    assert(circle("youtube") && !g_closed && !g_setup && g_toast == "PROSPEROTUBE: Press Circle again to close ProsperoTube");
    g_now = 10999; assert(circle("youtube") && g_closed == 1 && !g_setup);
    g_now = 12000; assert(circle("youtube") && g_closed == 1);
    g_now = 14001; assert(circle("youtube") && g_closed == 1);       /* too late */
    g_now = 14002; assert(circle("youtube") && g_closed == 2);
    /* Close at once. */
    g_app_mode = TUBE_UI_EXIT_SINGLE; g_now = 20000;
    assert(circle("youtube") && g_closed == 3 && !g_setup);
    /* A press that the browser consumed (a level popped) never reaches the exit rule. */
    g_back_consumes = 1; g_back = 0;
    assert(circle("youtube") && g_back == 1 && g_closed == 3);
    g_back_consumes = 0; g_app_mode = TUBE_UI_EXIT_NONE;
    assert(circle("youtube") && g_setup == 1 && g_closed == 3);
    assert(circle("iptv") && g_setup == 1 && g_closed == 3);
    puts("PASS: Circle on search results and on the main menu as generated");
    return 0;
}
