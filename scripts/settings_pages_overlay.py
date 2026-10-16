"""Preferences levels on the browser host: a menu of preferences arrives whole,
so its page count is right from the first page and paging stops at its last
page; a changed value keeps its row's page and focus."""

MEMBERS = '''    int m_tube_advance_from = -1;
    /* Settings levels (scripts/settings_pages_overlay.py): the level whose further
     * pages are read at once, and the row a change keeps on screen. */
    std::string m_tube_menu_next, m_tube_keep_pending, m_tube_keep_id;
    size_t m_tube_keep_absolute = 0, m_tube_keep_count = 0;'''

REQUEST = '''    m_is_usb_picker = false;
    // ProsperoTube whole settings menus
    if (page == 0) {
        /* A menu of preferences is local and short: every page of it is read before it is
         * shown, so "PAGE 3 OF 3" never turns into a fourth page. A change is asked for with
         * the changed row's id; the further pages are those of the level it was made on. */
        m_tube_keep_id = m_tube_keep_pending; m_tube_keep_pending.clear();
        m_tube_menu_next.clear();
        if (m_provider_id == "youtube" && parent_id) {
            const std::string level = !m_tube_keep_id.empty() && !m_stack.empty() ? m_stack.back() : std::string(parent_id);
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
    // ProsperoTube settings position
    const bool tube_more_coming = has_more && !m_tube_menu_next.empty();
    if (!tube_more_coming && !m_tube_keep_id.empty()) {
        /* The changed row by its id; a row that took another id keeps its place when the
         * menu kept its length; anything else is another screen and starts at its top. */
        const size_t total = m_all_rows.size();
        size_t target = total == m_tube_keep_count && m_tube_keep_absolute < total ? m_tube_keep_absolute : 0;
        for (size_t i = 0; i < total; ++i)
            if ((m_all_rows[i].selection_key.empty() ? m_all_rows[i].id : m_all_rows[i].selection_key) == m_tube_keep_id.c_str()) { target = i; break; }
        m_tube_keep_id.clear();
        m_saved_offset = target; m_saved_slot = 0;
    }
    // End ProsperoTube settings position
    if (!tube_more_coming && (m_saved_offset > 0 || m_saved_slot > 0)) {'''

KEEP = '''        m_page = 0;
        // ProsperoTube settings position: the changed row keeps its page and focus.
        if (m_provider_id == "youtube") {
            m_tube_keep_pending = id;
            m_tube_keep_absolute = m_row_offset + (size_t)(hit->index > 0 ? hit->index : 0);
            m_tube_keep_count = m_all_rows.size();
        }
        RequestPage(id.c_str(), 0); return;'''


def apply_settings_pages_overlay(replace, player):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(header, "    int m_tube_advance_from = -1;", MEMBERS)
    replace(ui, "    m_is_usb_picker = false;\n    if (page == 0) {\n        m_model.rows.clear();", REQUEST)
    replace(ui, "    self->ApplyItems(items, count, has_more);", CHAIN)
    replace(ui, "    m_focus_slot = 0;\n    if (m_saved_offset > 0 || m_saved_slot > 0) {", POSITION)
    replace(ui, "        m_page = 0; RequestPage(id.c_str(), 0); return;", KEEP)
