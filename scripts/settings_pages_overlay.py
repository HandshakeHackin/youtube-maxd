"""Preferences levels on the browser host: a menu of preferences arrives whole,
so its page count is right from the first page and paging stops at its last
page."""

MEMBERS = '''    int m_tube_advance_from = -1;
    /* Settings levels (scripts/settings_pages_overlay.py): the level whose further
     * pages are read at once. */
    std::string m_tube_menu_next;'''

REQUEST = '''    m_is_usb_picker = false;
    // ProsperoTube whole settings menus
    if (page == 0) {
        /* A menu of preferences is local and short: every page of it is read before it is
         * shown, so "PAGE 3 OF 3" never turns into a fourth page. */
        m_tube_menu_next.clear();
        if (m_provider_id == "youtube" && parent_id) {
            const std::string level = parent_id;
            if (level.rfind("settings", 0) == 0) { m_tube_menu_next = level; m_page = 0; }
        }
    }
    // End ProsperoTube whole settings menus
    if (page == 0) {
        m_model.rows.clear();'''

CHAIN = '''    self->ApplyItems(items, count, has_more);
    // ProsperoTube whole settings menus
    if (has_more && !refreshing_chat && !self->m_tube_menu_next.empty() && self->m_page < 15) {
        const std::string next = self->m_tube_menu_next;
        self->RequestPage(next.c_str(), ++self->m_page);
        return;
    }
    // End ProsperoTube whole settings menus'''

POSITION = '''    m_focus_slot = 0;
    // ProsperoTube whole settings menus: the place is taken once the last page is here.
    const bool tube_more_coming = has_more && !m_tube_menu_next.empty();
    if (!tube_more_coming && (m_saved_offset > 0 || m_saved_slot > 0)) {'''


def apply_settings_pages_overlay(replace, player):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(header, "    int m_tube_advance_from = -1;", MEMBERS)
    replace(ui, "    m_is_usb_picker = false;\n    if (page == 0) {\n        m_model.rows.clear();", REQUEST)
    replace(ui, "    self->ApplyItems(items, count, has_more);", CHAIN)
    replace(ui, "    m_focus_slot = 0;\n    if (m_saved_offset > 0 || m_saved_slot > 0) {", POSITION)
