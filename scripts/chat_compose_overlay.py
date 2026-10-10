"""Controller-driven chat composition with explicit scoped review and one send."""


def apply_chat_compose_overlay(replace, player, host):
    ui = player + 'ui_rml/src/evo_rmlui_provider.cpp'
    replace(ui, 'std::strcmp(parent_id, "account:remove") == 0);',
            'std::strcmp(parent_id, "account:remove") == 0 || std::strncmp(parent_id, "message:review:", 15) == 0);')
    replace(ui, '    if (key == 4) {', '''    if (key == 5) {
        if (!m_model.chat) return 0;
        if (m_model.loading || m_chat_refresh) {
            SetStatus("Wait for the chat update before composing a message.", false); return 1;
        }
        m_pos_stack.push_back({m_row_offset, FocusedRowIndex()});
        m_stack.push_back("message:compose"); m_crumbs.push_back("Compose chat message");
        m_page = 0; RequestPage(m_stack.back().c_str(), 0); return 1;
    }
    if (key == 4) {''')
    replace(ui, 'R3 changes chat mode. Touchpad opens comments.',
            'R3 changes chat mode. Touchpad opens comments. L3 composes a message.')
    replace(host, '        if ((pressed & PadButtons::TouchPad) && evo_rmlui_provider_discussion_key(4)) return true;',
            '        if ((pressed & PadButtons::L3) && evo_rmlui_provider_discussion_key(5)) return true;\n        if ((pressed & PadButtons::TouchPad) && evo_rmlui_provider_discussion_key(4)) return true;')
    replace(host, '''        tube_playlist_prompt_t prompt;
        if (tube_take_playlist_prompt(&prompt)) {''', '''        tube_chat_prompt_t message;
        if (tube_take_chat_prompt(&message)) {
            evo_keyboard_open(message.label, message.initial, 800,
                [](const char* text, void* userdata) {
                    const unsigned ticket = (unsigned)(uintptr_t)userdata;
                    if (!text) { tube_cancel_chat_prompt(ticket); return; }
                    char parent[80];
                    if (tube_submit_chat_message(ticket, text, parent, sizeof parent) == 0)
                        evo_rmlui_provider_replace_folder(parent, "Review chat message");
                    else toast("CHAT", "Enter 1 to 200 characters for the current chat and profile");
                }, (void*)(uintptr_t)message.ticket);
        }
        tube_playlist_prompt_t prompt;
        if (tube_take_playlist_prompt(&prompt)) {''')
