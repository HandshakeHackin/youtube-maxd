"""Automatic device sign-in updates in the existing native account browser."""


def apply_device_auth_overlay(replace, player):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(header, "    bool review = false;", "    bool review = false;\n    bool auth = false;")
    replace(ui, '    c.Bind("review", &m_model.review);', '    c.Bind("review", &m_model.review);\n    c.Bind("auth", &m_model.auth);')
    replace(ui, '    m_model.review = m_provider_id == "youtube" && parent_id &&', '''    m_model.auth = m_provider_id == "youtube" && parent_id && std::strncmp(parent_id, "auth:", 5) == 0;
    if (m_model_handle) m_model_handle.DirtyVariable("auth");
    m_model.review = m_provider_id == "youtube" && parent_id &&''')
    replace(ui, '    if (!IsOpen()) return;\n    evo_bundle_poll();', '''    if (!IsOpen()) return;
    static unsigned auth_revision = 0;
    static unsigned accounts_revision = 0;
    if (m_provider_id == "youtube" && !m_model.loading &&
        accounts_revision != tube_accounts_revision()) {
        accounts_revision = tube_accounts_revision();
        m_stack.clear(); m_crumbs.clear();
        m_stack.push_back("library:profiles"); m_crumbs.push_back("Accounts");
        m_page = 0;
        RequestPage(m_stack.back().c_str(), 0);
    }
    if (m_provider_id == "youtube" && !m_stack.empty() &&
        m_stack.back().rfind("auth:", 0) == 0 && !m_model.loading &&
        auth_revision != tube_auth_revision()) {
        auth_revision = tube_auth_revision();
        m_stack.back() = "auth:session";
        m_crumbs.back() = "YouTube sign-in";
        m_page = 0;
        RequestPage(m_stack.back().c_str(), 0);
    }
    evo_bundle_poll();''')
    replace(ui, '    if (m_provider_id == "youtube" && hit->id == "account:notice") {', '''    if (m_provider_id == "youtube" && hit->id.rfind("auth:info", 0) == 0) {
        SetStatus("Sign-in status updates automatically. Circle returns and cancels pending sign-in.", false); return;
    }
    if (m_provider_id == "youtube" && hit->id == "account:notice") {''')
    replace(ui, 'void EvoRmlProviderHost::Close()\n{', '''void EvoRmlProviderHost::Close()
{
    if (m_provider_id == "youtube") tube_cancel();''')
    replace(ui, '(std::strncmp(parent_id, "playlist:review:", 16) == 0 || std::strncmp(parent_id, "playlist:confirm:", 17) == 0);',
            '(std::strncmp(parent_id, "playlist:review:", 16) == 0 || std::strncmp(parent_id, "playlist:confirm:", 17) == 0 || std::strncmp(parent_id, "auth:", 5) == 0 || std::strcmp(parent_id, "account:remove") == 0);')
