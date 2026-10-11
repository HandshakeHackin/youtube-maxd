/* Execute the shipping host blocks for Refresh section, folders opened from
 * typed results, Touchpad section menus and the startup section. */
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
static std::string folder_menu, search_folder, boot_section, boot_title, startup_parent, startup_title;
static int account_on_boot, menu_calls, startups;
static const char* tube_folder_menu(const char* id, const char* title) { assert(*id && title); ++menu_calls; return folder_menu.c_str(); }
static const char* tube_search_folder() { return search_folder.c_str(); }
static int tube_select_account_on_boot() { return account_on_boot; }
static const char* tube_boot_section() { return boot_section.c_str(); }
static const char* tube_boot_section_title() { return boot_title.c_str(); }
static void evo_rmlui_provider_startup_folder(const char* parent, const char* title) { ++startups; startup_parent = parent; startup_title = title; }
struct Row { std::string id, title; bool is_folder; int index; };
struct Handle { int dirty = 0; explicit operator bool() const { return true; } void DirtyVariable(const char*) { ++dirty; } };
struct Host {
    struct LevelPos { size_t offset; int slot; };
    std::string m_provider_id = "youtube";
    std::vector<std::string> m_stack, m_crumbs, requests, searches;
    std::vector<LevelPos> m_pos_stack;
    size_t m_saved_offset = 0, m_row_offset = 7, m_search_depth = 0;
    int m_saved_slot = 0, m_page = 3, delivered = 0;
    struct { std::string query; } m_model;
    Handle m_model_handle;
    bool m_startup_pending = false, status_error = false;
    std::string status;
    void RequestPage(const char* parent, int page) { requests.push_back(std::string(parent) + "#" + std::to_string(page)); }
    void Search(const char* query) { searches.push_back(query); }
    void SetStatus(const std::string& text, bool error) { status = text; status_error = error; }
    void ReplaceFolder(const char* parent, const char* title) {
        if (m_stack.empty()) { m_pos_stack.push_back({m_row_offset, 0}); m_stack.push_back(parent); m_crumbs.push_back(title); }
        else { m_stack.back() = parent; m_crumbs.back() = title; }
        m_page = 0; RequestPage(m_stack.back().c_str(), 0);
    }
    void activate(const Row* hit) {
#include "refresh.inc"
        if (hit->is_folder) {
#include "results.inc"
            m_pos_stack.push_back({m_row_offset, hit->index});
            m_stack.push_back(hit->id); m_crumbs.push_back(hit->title);
            m_page = 0; RequestPage(m_stack.back().c_str(), 0);
        }
    }
    int touchpad(const Row* row) {
#include "menu.inc"
        return 0;
    }
    void StartupFolder(const char* parent, const char* title) {
#include "startup_folder.inc"
    }
    static void items(Host* self, int ok) {
#include "fallback.inc"
        self->delivered = ok ? 1 : -1;
    }
};
/* Each instantiation is one application launch: the block runs once per process. */
template <int Launch> static void open_provider(bool m_opened, const std::string& m_providerId) {
#include "startup.inc"
}
int main() {
    const Row refresh{"settings:refresh", "Refresh section", true, 5}, channel{"channel:UCaaaaaaaaaaaaaaaaaaaaaa", "Channel", true, 4};
    const Row video{"abcdefghijk", "Video", false, 2}, history{"history", "History", true, 1};
    /* Refresh leaves its menu and reloads the list under it from the first page. */
    Host a; a.m_stack = {"library:home", "account:menu:abcdefghijk"}; a.m_crumbs = {"Home", "Video actions"}; a.m_pos_stack = {{0, 0}, {48, 9}};
    a.activate(&refresh);
    assert(a.m_stack.size() == 1 && a.m_crumbs.size() == 1 && a.m_pos_stack.size() == 1 && a.requests == std::vector<std::string>{"library:home#0"});
    assert(a.m_saved_offset == 48 && a.m_saved_slot == 9 && !a.m_page && a.searches.empty());
    Host b; b.m_stack = {"settings:section:2"}; b.m_crumbs = {"History / Section actions"}; b.m_pos_stack = {{0, 3}};
    b.activate(&refresh); assert(b.m_stack.empty() && b.requests == std::vector<std::string>{"#0"} && b.m_saved_slot == 3);
    Host c; c.m_stack = {"account:menu:abcdefghijk"}; c.m_crumbs = {"Video actions"}; c.m_pos_stack = {{0, 1}}; c.m_model.query = "cats";
    c.activate(&refresh); assert(c.m_stack.empty() && c.requests.empty() && c.searches == std::vector<std::string>{"cats"});
    Host d; d.m_provider_id = "iptv"; d.m_stack = {"group"}; d.activate(&refresh);
    assert(d.m_stack.size() == 2 && d.m_stack.back() == "settings:refresh"); /* another provider's row is an ordinary folder */
    /* A folder opened from typed results stacks those results under it and ends the query. */
    Host e; e.m_model.query = "cats"; search_folder = "search-again:5"; e.activate(&channel);
    assert((e.m_stack == std::vector<std::string>{"search-again:5", "channel:UCaaaaaaaaaaaaaaaaaaaaaa"}));
    assert((e.m_crumbs == std::vector<std::string>{"Search: cats", "Channel"}) && e.m_model.query.empty() && e.m_model_handle.dirty == 1);
    assert(e.m_pos_stack.size() == 2 && !e.m_pos_stack[0].offset && !e.m_pos_stack[0].slot && e.m_pos_stack[1].offset == 7 && e.m_pos_stack[1].slot == 4);
    assert(e.requests == std::vector<std::string>{"channel:UCaaaaaaaaaaaaaaaaaaaaaa#0"});
    e.activate(&history); assert(e.m_stack.size() == 3 && e.m_crumbs.size() == 3); /* no query now: one level */
    Host f; f.m_model.query = "cats"; search_folder.clear(); f.activate(&channel);
    assert(f.m_stack.size() == 1 && f.m_model.query == "cats"); /* no retained search: the earlier behaviour */
    Host over; over.m_model.query = "cats"; over.m_stack = {"account:menu:abcdefghijk"}; over.m_crumbs = {"Video actions"}; over.m_pos_stack = {{0, 2}};
    search_folder = "search-again:5"; over.activate(&channel);
    assert(over.m_stack.size() == 2 && over.m_model.query == "cats"); /* a menu over the results is not the results */
    Host g; g.m_provider_id = "iptv"; g.m_model.query = "cats"; search_folder = "search-again:5"; g.activate(&channel); assert(g.m_stack.size() == 1);
    /* Touchpad opens the provider's menu for a folder row and nothing for other rows. */
    Host h; h.m_stack = {"discover"}; h.m_crumbs = {"Discover"}; h.m_pos_stack = {{0, 0}}; folder_menu = "settings:section:2"; menu_calls = 0;
    assert(h.touchpad(&history) == 1 && menu_calls == 1 && h.m_stack.back() == "settings:section:2" && h.m_crumbs.back() == "History / Section actions");
    assert(h.m_pos_stack.back().offset == 7 && h.m_pos_stack.back().slot == 1 && h.requests == std::vector<std::string>{"settings:section:2#0"} && !h.m_page);
    assert(h.touchpad(&video) == 0 && h.touchpad(nullptr) == 0 && menu_calls == 1 && h.m_stack.size() == 2);
    folder_menu.clear(); assert(h.touchpad(&history) == 0 && menu_calls == 2 && h.m_stack.size() == 2);
    Host i; i.m_model.query = "cats"; folder_menu = "settings:pin:channel:UCaaaaaaaaaaaaaaaaaaaaaa"; assert(i.touchpad(&channel) == 1);
    assert((i.m_stack == std::vector<std::string>{"search-again:5", "settings:pin:channel:UCaaaaaaaaaaaaaaaaaaaaaa"}) && i.m_model.query.empty());
    assert(i.m_crumbs.back() == "Channel / Section actions" && i.m_pos_stack.back().slot == 4);
    /* Startup: nothing by default, the account list when asked for, else the profile's section; once per launch. */
    open_provider<0>(true, "youtube"); assert(!startups);
    boot_section = "section:music"; boot_title = "Music";
    open_provider<0>(true, "youtube"); assert(!startups); /* launch 0 already decided */
    open_provider<1>(false, "youtube"); open_provider<1>(true, "iptv"); assert(!startups);
    open_provider<1>(true, "youtube"); assert(startups == 1 && startup_parent == "section:music" && startup_title == "Music");
    open_provider<1>(true, "youtube"); assert(startups == 1);
    account_on_boot = 1; open_provider<2>(true, "youtube"); assert(startups == 2 && startup_parent == "library:profiles" && startup_title == "Accounts");
    /* The startup folder is one level above the main menu; a failed load returns to the menu. */
    Host j; j.StartupFolder("section:music", "Music");
    assert(j.m_startup_pending && (j.m_stack == std::vector<std::string>{"section:music"}) && j.requests == std::vector<std::string>{"section:music#0"});
    Host::items(&j, 0);
    assert(!j.m_startup_pending && j.m_stack.empty() && j.m_crumbs.empty() && j.m_pos_stack.empty() && j.requests.back() == "#0" && !j.delivered);
    assert(j.status_error && j.status.find("main menu") != std::string::npos);
    Host::items(&j, 0); assert(j.delivered == -1 && j.requests.size() == 2); /* later failures are ordinary */
    Host k; k.StartupFolder("history", "History"); Host::items(&k, 1);
    assert(!k.m_startup_pending && k.m_stack.size() == 1 && k.delivered == 1 && k.requests.size() == 1);
    Host::items(&k, 0); assert(k.m_stack.size() == 1 && k.delivered == -1);
    Host l; l.m_stack = {"discover"}; l.m_crumbs = {"Discover"}; l.StartupFolder("history", "History"); assert(!l.m_startup_pending && l.requests.empty());
    Host m; m.StartupFolder("", ""); m.StartupFolder(nullptr, nullptr); assert(!m.m_startup_pending && m.m_stack.empty());
    Host n; n.m_provider_id = "iptv"; n.StartupFolder("history", "History"); assert(n.m_stack.empty());
    puts("PASS: shipping host refresh, results level under opened folders, section menus, startup section and its fallback");
}
