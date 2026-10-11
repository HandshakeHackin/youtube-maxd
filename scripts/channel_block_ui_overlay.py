"""Return successful channel block actions to their originating browser view."""


def apply_channel_block_ui_overlay(replace, player):
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(ui, '    self->m_has_more = has_more != 0;', '''    if (self->m_provider_id == "youtube" && !self->m_stack.empty()) {
        const std::string action = self->m_stack.back();
        const bool video_block = action.rfind("local:video:", 0) == 0 &&
            (action.find(":block:", 12) != std::string::npos || action.find(":unblock:", 12) != std::string::npos);
        const bool list_unblock = action.rfind("block:", 0) == 0;
        const bool video_parent = self->m_stack.size() >= 2 &&
            self->m_stack[self->m_stack.size()-2].rfind("account:menu:", 0) == 0;
        if ((video_block && video_parent) || list_unblock) {
            const int levels = video_block ? 2 : 1;
            for (int i = 0; i < levels; ++i) {
                self->m_stack.pop_back();
                if (!self->m_crumbs.empty()) self->m_crumbs.pop_back();
                if (!self->m_pos_stack.empty()) {
                    self->m_saved_offset = self->m_pos_stack.back().offset;
                    self->m_saved_slot = self->m_pos_stack.back().slot;
                    self->m_pos_stack.pop_back();
                }
            }
            self->m_page = 0;
            if (video_block && !self->m_model.query.empty()) {
                const std::string query = self->m_model.query.c_str();
                self->Search(query.c_str());
            } else {
                self->RequestPage(self->m_stack.empty() ? "" : self->m_stack.back().c_str(), 0);
            }
            return;
        }
    }
    self->m_has_more = has_more != 0;''')
