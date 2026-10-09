"""Native discussion layout and controller integration for the pinned EVO copy."""


def apply_discussion_overlay(replace, player, host):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(header, "    bool selected_is_folder = false;", "    bool selected_is_folder = false;\n    bool discussion = false;\n    bool review = false;")
    replace(header, "    void ReloadCurrentLevel();", "    void ReloadCurrentLevel();\n    int DiscussionKey(int key);")
    replace(header, "    int DiscussionKey(int key);", "    int DiscussionKey(int key);\n    void ReplaceFolder(const char* parent, const char* title);")
    replace(header, "void evo_rmlui_provider_search(const char *query);", "void evo_rmlui_provider_search(const char *query);\nint evo_rmlui_provider_discussion_key(int key);")
    replace(header, "int evo_rmlui_provider_discussion_key(int key);", "int evo_rmlui_provider_discussion_key(int key);\nvoid evo_rmlui_provider_replace_folder(const char* parent, const char* title);")
    replace(ui, '#include "evo_net.h"   /* evo_failure_reason */', '#include "evo_net.h"   /* evo_failure_reason */\n#include "tube_provider.h"')
    replace(ui, '    c.Bind("selected_overview",  &m_model.selected_overview);', '    c.Bind("selected_overview",  &m_model.selected_overview);\n    c.Bind("discussion", &m_model.discussion);\n    c.Bind("review", &m_model.review);')
    replace(ui, "    m_setup_shown = false;\n    if (!m_provider", "    m_setup_shown = false;\n    m_model.discussion = m_provider_id == \"youtube\" && parent_id &&\n        std::strncmp(parent_id, \"discussion:\", 11) == 0;\n    m_model.review = m_provider_id == \"youtube\" && parent_id &&\n        (std::strncmp(parent_id, \"playlist:review:\", 16) == 0 || std::strncmp(parent_id, \"playlist:confirm:\", 17) == 0);\n    if (m_model_handle) { m_model_handle.DirtyVariable(\"discussion\"); m_model_handle.DirtyVariable(\"review\"); }\n    if (!m_provider")
    replace(ui, "        r.overview  = display_text(s.overview);", "        const char* comment = m_provider_id == \"youtube\" ? tube_comment_text(s.id) : \"\";\n        r.overview = display_text(*comment ? comment : s.overview);")
    replace(ui, "        m_model.selected_id = row->id;", "        if (m_model.selected_id != row->id && m_doc)\n            if (auto text = m_doc->GetElementById(\"tube-discussion-text\")) text->SetScrollTop(0);\n        m_model.selected_id = row->id;")
    folder_branch = '    if (hit->is_folder && !(m_provider_id == "youtube" && id.rfind("app:", 0) == 0)) {'
    replace(ui, folder_branch, '    if (m_provider_id == "youtube" && hit->id == "account:notice") {\n        SetStatus("Circle returns to the previous screen.", false); return;\n    }\n    if (m_provider_id == "youtube" && hit->id.rfind("discussion:leaf:", 0) == 0) {\n        SetStatus("This comment has no reply thread. L2 / R2 scroll the text.", false); return;\n    }\n' + folder_branch)
    replace(ui, "void evo_rmlui_provider_search(const char *query)", "int evo_rmlui_provider_discussion_key(int key)\n{\n    return EvoRmlProviderHost::Instance().DiscussionKey(key);\n}\n\nvoid evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{\n    EvoRmlProviderHost::Instance().ReplaceFolder(parent, title);\n}\n\nvoid evo_rmlui_provider_search(const char *query)")
    replace(ui, "void EvoRmlProviderHost::ReloadCurrentLevel()", '''void EvoRmlProviderHost::ReplaceFolder(const char* parent, const char* title)
{
    if (!parent || !*parent || m_provider_id != "youtube") return;
    if (m_stack.empty()) {
        m_pos_stack.push_back({m_row_offset, 0}); m_stack.push_back(parent); m_crumbs.push_back(title ? title : "Playlist");
    } else { m_stack.back() = parent; m_crumbs.back() = title ? title : "Playlist"; }
    m_page = 0; RequestPage(m_stack.back().c_str(), 0);
}

int EvoRmlProviderHost::DiscussionKey(int key)
{
    if (m_provider_id != "youtube") return 0;
    if (key == 1 || key == 2) {
        if (!m_model.discussion && !m_model.review) return 0;
        if (m_doc) if (auto text = m_doc->GetElementById("tube-discussion-text"))
            text->SetScrollTop(text->GetScrollTop() + (key == 1 ? -1 : 1) *
                (m_context ? m_context->GetDimensions().y * 0.2f : 180.0f));
        m_dirty = true; return 1;
    }
    if (key == 4) {
        if (m_model.discussion) return 0;
        const auto* row = GetFocusedRow();
        if (row && row->is_folder && row->id.rfind("library:playlist:", 0) == 0) {
            m_pos_stack.push_back({m_row_offset, row->index});
            m_stack.push_back(std::string("playlist:menu:") + row->id.substr(17).c_str());
            m_crumbs.push_back(std::string(row->title.c_str()) + " / Playlist actions"); m_page = 0;
            RequestPage(m_stack.back().c_str(), 0); return 1;
        }
        if (!row || row->is_folder || row->id.size() != 11) return 0;
        m_pos_stack.push_back({m_row_offset, row->index});
        m_stack.push_back(std::string("account:menu:") + row->id.c_str());
        m_crumbs.push_back("YouTube account actions"); m_page = 0;
        RequestPage(m_stack.back().c_str(), 0); return 1;
    }
    if (key != 0) return 0;
    if (m_model.discussion) {
        if (!m_stack.empty()) {
            auto& parent = m_stack.back();
            const bool top = parent.rfind("discussion:top:", 0) == 0;
            const bool newest = parent.rfind("discussion:newest:", 0) == 0;
            if (top || newest) {
                parent = std::string(top ? "discussion:newest:" : "discussion:top:") +
                    parent.substr(top ? 15 : 18);
                m_crumbs.back() = top ? "Newest comments" : "Top comments";
                m_page = 0; RequestPage(parent.c_str(), 0);
            }
        }
        return 1;
    }
    const auto* row = GetFocusedRow();
    if (!row || row->is_folder || row->id.size() != 11) return 0;
    const std::string parent = std::string("discussion:top:") + row->id.c_str();
    m_pos_stack.push_back({m_row_offset, row->index});
    m_stack.push_back(parent); m_crumbs.push_back("Top comments");
    m_page = 0; RequestPage(m_stack.back().c_str(), 0); return 1;
}

void EvoRmlProviderHost::ReloadCurrentLevel()''')
    replace(host, "    if (pressed & PadButtons::Square) {\n        openSearch();", "    if (m_providerId == \"youtube\") {\n        if ((pressed & PadButtons::TouchPad) && evo_rmlui_provider_discussion_key(4)) return true;\n        if ((pressed & PadButtons::R3) && evo_rmlui_provider_discussion_key(0)) return true;\n        if ((pressed & PadButtons::L2) && evo_rmlui_provider_discussion_key(1)) return true;\n        if ((pressed & PadButtons::R2) && evo_rmlui_provider_discussion_key(2)) return true;\n    }\n    if (pressed & PadButtons::Square) {\n        openSearch();")
    replace(host, "    /* Bundle refresh, artwork decode, and any catalog reply that landed. */", '''    if (m_providerId == "youtube") {
        tube_playlist_prompt_t prompt;
        if (tube_take_playlist_prompt(&prompt)) {
            evo_keyboard_open(prompt.label, prompt.initial, 600,
                [](const char* text, void* userdata) {
                    char parent[80];
                    if (tube_submit_playlist_name((unsigned)(uintptr_t)userdata, text, parent, sizeof parent) == 0)
                        evo_rmlui_provider_replace_folder(parent, "Confirm playlist change");
                    else toast("PLAYLIST", "Enter a name of 1 to 150 characters for the current profile");
                }, (void*)(uintptr_t)prompt.ticket);
        }
    }

    /* Bundle refresh, artwork decode, and any catalog reply that landed. */''')
