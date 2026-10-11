"""Section menus, refresh, search result levels, the description reader and the startup section."""

RESULTS_LEVEL = '''        // ProsperoTube search results level
        if (m_provider_id == "youtube" && !m_model.query.empty() && m_stack.size() == m_search_depth && *tube_search_folder()) {
            /* A folder opened from typed results sits above those results as a
             * level of their own: Back returns to them and their pages load.
             * A menu already stacked on the results keeps its earlier behaviour. */
            m_pos_stack.push_back({0, 0});
            m_stack.push_back(tube_search_folder());
            m_crumbs.push_back(std::string("Search: ") + m_model.query.c_str());
            m_model.query = "";
            if (m_model_handle) m_model_handle.DirtyVariable("query");
        }
        // End ProsperoTube search results level
'''

REFRESH = '''    // ProsperoTube refresh section
    if (m_provider_id == "youtube" && hit->id == "settings:refresh") {
        /* The row lives in a menu: leave the menu and reload the list under it. */
        if (!m_stack.empty()) {
            m_stack.pop_back();
            if (!m_crumbs.empty()) m_crumbs.pop_back();
            if (!m_pos_stack.empty()) {
                m_saved_offset = m_pos_stack.back().offset;
                m_saved_slot = m_pos_stack.back().slot;
                m_pos_stack.pop_back();
            }
        }
        m_page = 0;
        if (!m_model.query.empty()) {
            const std::string query = m_model.query.c_str();
            Search(query.c_str());
        } else {
            RequestPage(m_stack.empty() ? "" : m_stack.back().c_str(), 0);
        }
        return;
    }
    // End ProsperoTube refresh section
'''

SECTION_MENU = '''        // ProsperoTube section menu
        const std::string section_menu = row && row->is_folder
            ? tube_folder_menu(row->id.c_str(), row->title.c_str()) : "";
        if (!section_menu.empty()) {
            const std::string crumb = std::string(row->title.c_str()) + " / Section actions";
            const int slot = row->index;
            if (!m_model.query.empty() && m_stack.size() == m_search_depth && *tube_search_folder()) {
                m_pos_stack.push_back({0, 0});
                m_stack.push_back(tube_search_folder());
                m_crumbs.push_back(std::string("Search: ") + m_model.query.c_str());
                m_model.query = "";
                if (m_model_handle) m_model_handle.DirtyVariable("query");
            }
            m_pos_stack.push_back({m_row_offset, slot});
            m_stack.push_back(section_menu); m_crumbs.push_back(crumb);
            m_page = 0; RequestPage(m_stack.back().c_str(), 0); return 1;
        }
        // End ProsperoTube section menu
'''

STARTUP_FALLBACK = '''    // ProsperoTube startup fallback
    const bool startup_folder = self->m_startup_pending;
    self->m_startup_pending = false;
    if (!ok && startup_folder && self->m_stack.size() == 1) {
        self->m_stack.clear(); self->m_crumbs.clear(); self->m_pos_stack.clear();
        self->m_page = 0; self->RequestPage("", 0);
        self->SetStatus("The startup section could not open. Showing the main menu.", true);
        return;
    }
    // End ProsperoTube startup fallback
'''

STARTUP_FOLDER = '''void EvoRmlProviderHost::StartupFolder(const char* parent, const char* title)
{
    // ProsperoTube startup folder
    if (!parent || !*parent || m_provider_id != "youtube" || !m_stack.empty()) return;
    m_startup_pending = true; /* before the request: a local list answers at once */
    ReplaceFolder(parent, title);
    // End ProsperoTube startup folder
}

'''

STARTUP_SECTION = '''    // ProsperoTube startup section
    static bool tube_startup_done = false;
    if (m_opened && m_providerId == "youtube" && !tube_startup_done) {
        /* Once per launch: the account list when asked for, else the profile's
         * section. Neither set keeps the main menu; a failed load returns to it. */
        tube_startup_done = true;
        if (tube_select_account_on_boot()) evo_rmlui_provider_startup_folder("library:profiles", "Accounts");
        else if (*tube_boot_section()) evo_rmlui_provider_startup_folder(tube_boot_section(), tube_boot_section_title());
    }
    // End ProsperoTube startup section
'''


def apply_browse_sections_overlay(replace, player, host):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    # Search options and the section list are real levels; their rows act in place.
    replace(ui, 'id != "settings:languages" && ',
            'id != "settings:languages" && id != "settings:search" && id != "settings:sections" && ')
    replace(ui, '    if (m_provider_id == "youtube" && id.rfind("settings:", 0) == 0 &&',
            REFRESH + '    if (m_provider_id == "youtube" && id.rfind("settings:", 0) == 0 &&')
    folder_branch = '    if (hit->is_folder && !(m_provider_id == "youtube" && id.rfind("app:", 0) == 0)) {\n'
    replace(ui, folder_branch, folder_branch + RESULTS_LEVEL)
    replace(ui, '        const auto* row = GetFocusedRow();\n        if (row && row->is_folder && row->id.rfind("library:playlist:", 0) == 0) {',
            '        const auto* row = GetFocusedRow();\n' + SECTION_MENU +
            '        if (row && row->is_folder && row->id.rfind("library:playlist:", 0) == 0) {')
    # The description uses the long-text reader with its own footer.
    replace(header, "    bool auth = false;", "    bool auth = false;\n    bool description = false;")
    replace(ui, '    c.Bind("auth", &m_model.auth);', '    c.Bind("auth", &m_model.auth);\n    c.Bind("description", &m_model.description);')
    replace(ui, '    if (m_model_handle) m_model_handle.DirtyVariable("auth");\n    m_model.review =',
            '    if (m_model_handle) m_model_handle.DirtyVariable("auth");\n'
            '    m_model.description = m_provider_id == "youtube" && parent_id && std::strncmp(parent_id, "description:", 12) == 0;\n'
            '    if (m_model_handle) m_model_handle.DirtyVariable("description");\n    m_model.review =')
    replace(ui, 'std::strncmp(parent_id, "message:review:", 15) == 0);',
            'std::strncmp(parent_id, "message:review:", 15) == 0 || std::strncmp(parent_id, "description:", 12) == 0);')
    replace(ui, "m_model.chat = m_model.discussion = m_model.review = m_model.auth = false;",
            "m_model.chat = m_model.discussion = m_model.review = m_model.auth = m_model.description = false;", count=2)
    replace(ui, 'm_model_handle.DirtyVariable("review"); m_model_handle.DirtyVariable("auth"); }',
            'm_model_handle.DirtyVariable("review"); m_model_handle.DirtyVariable("auth");\n'
            '            m_model_handle.DirtyVariable("description"); }', count=2)
    replace(ui, '    if (m_provider_id == "youtube" && hit->id == "account:notice") {',
            '    if (m_provider_id == "youtube" && hit->id.rfind("description:text:", 0) == 0) {\n'
            '        SetStatus("L2 / R2 scroll the description. Circle returns to the video actions.", false); return;\n    }\n'
            '    if (m_provider_id == "youtube" && hit->id == "account:notice") {')
    # Startup: one folder above the main menu, with a way back if it cannot load.
    replace(header, "    void ReplaceFolder(const char* parent, const char* title);",
            "    void ReplaceFolder(const char* parent, const char* title);\n    void StartupFolder(const char* parent, const char* title);")
    replace(header, "    unsigned m_request_generation = 0;", "    unsigned m_request_generation = 0;\n    bool m_startup_pending = false;\n    size_t m_search_depth = 0;")
    # Results are on screen while the stack is as deep as when the query began.
    replace(ui, "    m_model.query = Rml::String(q.c_str());", "    m_model.query = Rml::String(q.c_str());\n    m_search_depth = m_stack.size();")
    replace(header, "void evo_rmlui_provider_replace_folder(const char* parent, const char* title);",
            "void evo_rmlui_provider_replace_folder(const char* parent, const char* title);\n"
            "void evo_rmlui_provider_startup_folder(const char* parent, const char* title);")
    replace(ui, "void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)\n{",
            STARTUP_FOLDER + "void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)\n{")
    replace(ui, "void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{",
            "void evo_rmlui_provider_startup_folder(const char* parent, const char* title)\n{\n"
            "    EvoRmlProviderHost::Instance().StartupFolder(parent, title);\n}\n\n"
            "void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{")
    replace(ui, "    if (!ok) {\n        self->m_model.empty = self->m_all_rows.empty();",
            STARTUP_FALLBACK + "    if (!ok) {\n        self->m_model.empty = self->m_all_rows.empty();")
    replace(host, "    if (!m_opened) {\n        evo_bt(\"provider: could not open host for '%s'\", m_providerId.c_str());",
            STARTUP_SECTION + "    if (!m_opened) {\n        evo_bt(\"provider: could not open host for '%s'\", m_providerId.c_str());")
    # A renamed pin returns to its own menu, not to a playlist confirmation.
    replace(host, 'evo_rmlui_provider_replace_folder(parent, "Confirm playlist change");',
            'evo_rmlui_provider_replace_folder(parent, std::strncmp(parent, "settings:pin:", 13) ? "Confirm playlist change" : "Pinned section");')
