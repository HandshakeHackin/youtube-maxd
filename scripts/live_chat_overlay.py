"""Live chat in the existing PS5 reader, with request-driven stable refreshes."""


def apply_live_chat_overlay(replace, player):
    header = player + 'ui_rml/include/evo_rmlui_provider.h'
    ui = player + 'ui_rml/src/evo_rmlui_provider.cpp'
    host = player + 'core/src/screens/ProviderHostScreen.cpp'
    replace(host, '        if ((pressed & PadButtons::TouchPad) && evo_rmlui_provider_discussion_key(4)) return true;',
            '        if ((pressed & PadButtons::Triangle) && evo_rmlui_provider_discussion_key(6)) return true;\n        if ((pressed & PadButtons::TouchPad) && evo_rmlui_provider_discussion_key(4)) return true;')
    replace(ui, '    switch (k) {\n    case KeyPageUp: {', '''    if (m_model.chat && !m_stack.empty() && (k == KeyPageUp || k == KeyPageDown)) {
        const double position = tube_chat_reader_position(m_stack.back().c_str());
        if (position >= 0) {
            const double target = std::max(0.0, std::min(1000000.0, position + (k == KeyPageUp ? -60.0 : 60.0)));
            if (!tube_chat_reader_seek(m_stack.back().c_str(), target)) {
                SetStatus("Wait for the replay update before seeking again", false); return true;
            }
            RefreshChat();
            char status[160];
            std::snprintf(status, sizeof status, "Chat replay %d:%02d: L1 / R1 move one minute; D-pad browses messages", (int)target / 60, (int)target % 60);
            SetStatus(status, false);
            return true;
        }
    }
    switch (k) {
    case KeyPageUp: {''')
    replace(ui, '    if (key == 1 || key == 2) {', '''    if (key == 6) {
        if (!m_model.discussion || m_model.chat || m_stack.empty()) return 0;
        const auto parent = m_stack.back();
        const size_t prefix = parent.rfind("discussion:top:", 0) == 0 ? 15 : parent.rfind("discussion:newest:", 0) == 0 ? 18 : 0;
        if (!prefix || parent.size() != prefix + 11) return 0;
        m_pos_stack.push_back({m_row_offset, FocusedRowIndex()});
        m_stack.push_back(std::string("chat:top:") + parent.substr(prefix));
        m_crumbs.push_back("Chat replay"); m_page = 0;
        RequestPage(m_stack.back().c_str(), 0); return 1;
    }
    if (key == 1 || key == 2) {''')
    replace(header, '    bool auth = false;', '    bool auth = false;\n    bool chat = false;')
    replace(header, '    void ReplaceFolder(const char* parent, const char* title);',
            '    void ReplaceFolder(const char* parent, const char* title);\n    void RefreshChat();')
    replace(header, '    bool m_dirty = true;', '''    bool m_chat_refresh = false;
    bool m_chat_restore_scroll = false;
    float m_chat_scroll = 0;
    std::string m_chat_restore_id;
    bool m_dirty = true;''')
    replace(ui, '    c.Bind("auth", &m_model.auth);', '    c.Bind("auth", &m_model.auth);\n    c.Bind("chat", &m_model.chat);')
    replace(ui, '''    m_model.discussion = m_provider_id == "youtube" && parent_id &&
        std::strncmp(parent_id, "discussion:", 11) == 0;''', '''    m_chat_refresh = false; m_chat_restore_scroll = false;
    m_model.chat = m_provider_id == "youtube" && parent_id &&
        (std::strncmp(parent_id, "chat:top:", 9) == 0 || std::strncmp(parent_id, "chat:live:", 10) == 0);
    if (m_model_handle) m_model_handle.DirtyVariable("chat");
    m_model.discussion = m_provider_id == "youtube" && parent_id &&
        (std::strncmp(parent_id, "discussion:", 11) == 0 || m_model.chat);''')
    replace(ui, 'void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)', '''void EvoRmlProviderHost::RefreshChat()
{
    if (!m_provider || !m_model.chat || m_model.loading || m_chat_refresh || m_stack.empty() ||
        !tube_chat_due(m_stack.back().c_str())) return;
    m_chat_refresh = true;
    ItemsCtx* ctx = new ItemsCtx{this, ++m_request_generation};
    int rc = m_provider->list_catalog(m_stack.back().c_str(), 0, &EvoRmlProviderHost::ItemsCallback, ctx);
    if (rc != 0) {
        delete ctx; m_chat_refresh = false;
        SetStatus("Chat update failed. Reopen the chat to retry.", true);
    }
}

void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)''')
    replace(ui, '''    if (!self->IsOpen() || gen != self->m_request_generation) return;

    self->m_model.loading = false;''', '''    if (!self->IsOpen() || gen != self->m_request_generation) return;
    const bool refreshing_chat = self->m_chat_refresh;
    self->m_chat_refresh = false;

    self->m_model.loading = false;''')
    replace(ui, '        if (self->m_doc) self->m_needs_initial_focus = true;',
            '        if (self->m_doc && !refreshing_chat) self->m_needs_initial_focus = true;')
    replace(ui, '    self->ApplyItems(items, count, has_more);', '''    std::string selected;
    int old_absolute = 0;
    float scroll = 0;
    if (refreshing_chat) {
        if (const auto* row = self->GetFocusedRow()) {
            selected = row->id.c_str(); old_absolute = (int)self->m_row_offset + row->index;
        }
        if (self->m_doc) if (auto text = self->m_doc->GetElementById("tube-discussion-text")) scroll = text->GetScrollTop();
        self->m_all_rows.clear(); self->m_art_urls.clear(); self->m_art_keys.clear(); self->m_row_offset = 0;
    }
    self->ApplyItems(items, count, has_more);
    if (self->m_model.chat && !refreshing_chat && count > 0) {
        const int target = count - 1;
        self->SetPageOffset((size_t)(target / EVO_PROVIDER_PAGE_SIZE) * EVO_PROVIDER_PAGE_SIZE,
                            target % EVO_PROVIDER_PAGE_SIZE);
    }
    if (refreshing_chat) {
        int target = count > 0 ? std::min(old_absolute, count - 1) : 0;
        bool retained = false;
        for (int i = 0; i < count; ++i) if (selected == items[i].id) { target = i; retained = true; break; }
        self->SetPageOffset((size_t)(target / EVO_PROVIDER_PAGE_SIZE) * EVO_PROVIDER_PAGE_SIZE, target % EVO_PROVIDER_PAGE_SIZE);
        self->m_chat_restore_id = selected; self->m_chat_scroll = scroll; self->m_chat_restore_scroll = retained;
    }''')
    replace(ui, '''    if (m_needs_initial_focus) {
        SeedFocus();
        m_context->Update();
    }''', '''    if (m_needs_initial_focus) {
        SeedFocus();
        m_context->Update();
    }
    if (m_chat_restore_scroll) {
        UpdateSelectedPreview(); m_context->Update();
        if (m_model.selected_id == m_chat_restore_id.c_str())
            if (auto text = m_doc->GetElementById("tube-discussion-text")) text->SetScrollTop(m_chat_scroll);
        m_context->Update(); m_chat_restore_scroll = false;
    }''')
    replace(ui, '    evo_bundle_poll();\n    evo_provider_art_poll();', '    RefreshChat();\n    evo_bundle_poll();\n    evo_provider_art_poll();')
    replace(ui, '    if (key == 4) {\n        if (m_model.discussion) {', '''    if (key == 4) {
        if (m_model.chat && !m_stack.empty()) {
            const auto parent = m_stack.back();
            const size_t prefix = parent.rfind("chat:top:", 0) == 0 ? 9 : 10;
            m_pos_stack.push_back({m_row_offset, FocusedRowIndex()});
            m_stack.push_back(std::string("discussion:top:") + parent.substr(prefix));
            m_crumbs.push_back("Top comments"); m_page = 0; RequestPage(m_stack.back().c_str(), 0); return 1;
        }
        if (m_model.discussion) {''')
    replace(ui, '    if (key != 0) return 0;\n    if (m_model.discussion) {', '''    if (key != 0) return 0;
    if (m_model.chat && !m_stack.empty()) {
        auto& parent = m_stack.back();
        const bool top = parent.rfind("chat:top:", 0) == 0;
        parent = std::string(top ? "chat:live:" : "chat:top:") + parent.substr(top ? 9 : 10);
        m_crumbs.back() = top ? "Live chat" : "Top chat";
        m_page = 0; RequestPage(parent.c_str(), 0); return 1;
    }
    if (m_model.discussion) {''')
    replace(ui, '    const std::string parent = std::string("discussion:top:") + row->id.c_str();',
            '    const std::string parent = std::string(row->is_live ? "chat:top:" : "discussion:top:") + row->id.c_str();')
    replace(ui, '    m_stack.push_back(parent); m_crumbs.push_back("Top comments");',
            '    m_stack.push_back(parent); m_crumbs.push_back(row->is_live ? "Top chat" : "Top comments");')
    replace(ui, '    if (m_provider_id == "youtube" && hit->id == "account:notice") {', '''    if (m_provider_id == "youtube" && hit->id.rfind("chat:leaf:", 0) == 0) {
        SetStatus("L2 / R2 scroll this message. R3 changes chat mode. Touchpad opens comments.", false); return;
    }
    if (m_provider_id == "youtube" && hit->id == "account:notice") {''')
    reset = '''    if (m_provider_id == "youtube") {
        tube_cancel(); ++m_request_generation;
        m_chat_refresh = false; m_chat_restore_scroll = false;
        m_model.chat = m_model.discussion = m_model.review = m_model.auth = false;
        if (m_model_handle) { m_model_handle.DirtyVariable("chat"); m_model_handle.DirtyVariable("discussion");
            m_model_handle.DirtyVariable("review"); m_model_handle.DirtyVariable("auth"); }
    }
'''
    replace(ui, 'void EvoRmlProviderHost::ShowSetupScreen()\n{', 'void EvoRmlProviderHost::ShowSetupScreen()\n{\n' + reset)
    replace(ui, 'void EvoRmlProviderHost::Search(const char* query)\n{', 'void EvoRmlProviderHost::Search(const char* query)\n{\n' + reset)
