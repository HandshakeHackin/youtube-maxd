"""Refresh the native TV-code screen without disturbing its navigation stack."""
def apply_phone_overlay(replace, player):
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(ui, 'id != "settings:languages" && id != "settings:audio-languages"',
            'id != "settings:languages" && id != "settings:audio-languages" && id != "settings:remote"')
    replace(ui, '    static unsigned auth_revision = 0;', '''    static unsigned phone_revision = 0;
    if (m_provider_id == "youtube" && !m_stack.empty() &&
        m_stack.back() == "settings:remote" && !m_model.loading &&
        phone_revision != tube_phone_revision()) {
        phone_revision = tube_phone_revision();
        m_page = 0;
        RequestPage("settings:remote", 0);
    }
    static unsigned auth_revision = 0;''')
    replace(ui, '    if (m_provider_id == "youtube" && hit->id.rfind("auth:info", 0) == 0) {', '''    if (m_provider_id == "youtube" && (hit->id == "settings:remote:info" || hit->id == "settings:remote:code")) {
        SetStatus("Enter the TV code in your YouTube phone app. Link a phone requests a fresh code.", false); return;
    }
    if (m_provider_id == "youtube" && hit->id.rfind("auth:info", 0) == 0) {''')
