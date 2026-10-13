"""QR codes on the sign-in screen and on "link:" pages of the catalog screen.

The code itself is black on white in every theme (colour names, which the
palette of youtube.rcss leaves alone); the caption and the key line use tokens.

The provider host asks native/tube_qr_page.h which row of the current page
carries a link, encodes it with native/tube_qr.h and fills one element with
box markup, so no texture is involved. Apply after the device sign-in overlay,
whose review layout and footer this extends.
"""
import shutil


RML_PANEL = '''            <div id="tube-qr" data-if="qr">
                <div id="tube-qr-box"></div>
                <div id="tube-qr-caption">Scan with a phone camera, or open<br/>{{ qr_text }}</div>
            </div>
'''

RML_FOOTER = '''        <div id="tube-link-footer" data-if="link">
            <span class="tube-key">L2 / R2</span> Scroll
            <span class="tube-key">○</span> Back
        </div>
'''

RCSS = '''#tube-discussion-text.with-qr { height: 150dp; }
#tube-qr { position: absolute; left: 0; top: 240dp; width: 1100dp; height: 500dp; }
#tube-qr-box { position: absolute; left: 0; top: 0; background: white; }
#tube-qr-symbol { position: relative; }
#tube-qr-symbol div { position: absolute; background: black; }
#tube-qr-caption { position: absolute; left: 448dp; top: 0; width: 640dp; font-size: 24dp; line-height: 36dp; color: #f5f6fa; word-break: break-all; }
#tube-link-footer { position: absolute; bottom: 0; font-size: 21dp; color: #c3c5d0; height: 40dp; width: 1792dp; border-top: 1dp #1e2130; }
'''

METHODS = '''// ProsperoTube QR codes
void EvoRmlProviderHost::UpdateQr()
{
    static char markup[TUBE_QR_MARKUP_SIZE];
    char payload[TUBE_QR_PAYLOAD_SIZE] = "";
    const char* parent = m_qr_parent.c_str();
    bool found = false;
    if (tube_qr_page_kind(parent) != TUBE_QR_PAGE_NONE && m_model.query.empty() &&
        !m_stack.empty() && m_stack.back() == m_qr_parent) {
        /* A reload keeps what is on screen until its rows arrive. */
        if (m_model.loading) return;
        if (const EvoProviderRow* row = GetFocusedRow())
            found = tube_qr_row_payload(parent, row->id.c_str(), row->title.c_str(), row->overview.c_str(),
                                        payload, sizeof payload) != 0;
        for (size_t i = 0; !found && i < m_all_rows.size(); ++i)
            found = tube_qr_row_payload(parent, m_all_rows[i].id.c_str(), m_all_rows[i].title.c_str(),
                                        m_all_rows[i].overview.c_str(), payload, sizeof payload) != 0;
    }
    Rml::Element* box = m_doc ? m_doc->GetElementById("tube-qr-box") : nullptr;
    if (found && box && m_model.qr && m_qr_payload == payload) return;
    tube_qr_code code;
    if (found && box && tube_qr_encode_text(payload, &code) == 0 && tube_qr_markup(&code, markup, sizeof markup) > 0) {
        box->SetInnerRML(markup);
        m_qr_payload = payload;
        m_model.qr = true;
        m_model.qr_text = payload;
    } else {
        if (!m_model.qr && m_qr_payload.empty()) return;
        if (box) box->SetInnerRML("");
        m_qr_payload.clear();
        m_model.qr = false;
        m_model.qr_text = "";
    }
    if (m_model_handle) { m_model_handle.DirtyVariable("qr"); m_model_handle.DirtyVariable("qr_text"); }
    m_dirty = true;
}
// End ProsperoTube QR codes

'''


def apply_qr_overlay(replace, player, work, root):
    for name in ("tube_qr.h", "tube_qr_page.h"):
        shutil.copyfile(root / "native" / name, work / player / "addons/include" / name)
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    rml = player + "assets/rml/youtube.rml"
    rcss = work / player / "assets/rml/youtube.rcss"

    replace(header, "    bool auth = false;", "    bool auth = false;\n    bool link = false;\n    bool qr = false;\n    Rml::String qr_text;")
    replace(header, "    void ReloadCurrentLevel();", "    void ReloadCurrentLevel();\n    void UpdateQr();\n    bool QrShown() const { return IsOpen() && m_model.qr; }\n    std::string m_qr_parent, m_qr_payload;")
    # A code being scanned gets no input: screen dimming leaves it alone.
    replace(header, "void evo_rmlui_provider_replace_folder(const char* parent, const char* title);",
            "void evo_rmlui_provider_replace_folder(const char* parent, const char* title);\n"
            "/* 1 = a QR code is on screen. */\nint evo_rmlui_provider_qr_shown(void);")
    replace(ui, "void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{",
            "int evo_rmlui_provider_qr_shown(void)\n{\n    return EvoRmlProviderHost::Instance().QrShown() ? 1 : 0;\n}\n\n"
            "void evo_rmlui_provider_replace_folder(const char* parent, const char* title)\n{")
    app = player + "core/src/Application.cpp"
    if (work / app).exists():   # the host test of this overlay copies the provider screen alone
        replace(app, "|| tube_player_dim_blocked()) context = TUBE_DIM_NONE;",
                "|| tube_player_dim_blocked() ||\n"
                "                (current == ScreenId::EmbyBrowse && evo_rmlui_provider_qr_shown())) context = TUBE_DIM_NONE;")
    replace(ui, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_qr_page.h"')
    replace(ui, '    c.Bind("auth", &m_model.auth);', '    c.Bind("auth", &m_model.auth);\n    c.Bind("link", &m_model.link);\n    c.Bind("qr", &m_model.qr);\n    c.Bind("qr_text", &m_model.qr_text);')
    # A link page uses the list-and-reader layout of the review pages.
    replace(ui, '    if (m_model_handle) m_model_handle.DirtyVariable("auth");', '''    if (m_model_handle) m_model_handle.DirtyVariable("auth");
    m_qr_parent = m_provider_id == "youtube" && parent_id ? parent_id : "";
    m_model.link = tube_qr_page_kind(m_qr_parent.c_str()) == TUBE_QR_PAGE_LINK;
    if (m_model_handle) m_model_handle.DirtyVariable("link");
    if (tube_qr_page_kind(m_qr_parent.c_str()) == TUBE_QR_PAGE_NONE) UpdateQr();''')
    replace(ui, 'std::strcmp(parent_id, "account:remove") == 0', 'std::strcmp(parent_id, "account:remove") == 0 || std::strncmp(parent_id, "link:", 5) == 0')
    replace(ui, '        m_model_handle.DirtyVariable("selected_played");\n    }\n}', '        m_model_handle.DirtyVariable("selected_played");\n    }\n    UpdateQr();\n}')
    replace(ui, '    if (m_provider_id == "youtube" && hit->id.rfind("auth:info", 0) == 0) {', '''    if (m_provider_id == "youtube" && m_model.link && hit->id.rfind("link:", 0) == 0) {
        SetStatus(m_model.qr ? "Scan the code with a phone camera, or type the address shown." : "This link is too long for a QR code. Type the address shown.", false); return;
    }
    if (m_provider_id == "youtube" && hit->id.rfind("auth:info", 0) == 0) {''')
    replace(ui, "bool EvoRmlProviderHost::FocusPublishedRow(int local_idx)\n{", METHODS + "bool EvoRmlProviderHost::FocusPublishedRow(int local_idx)\n{")

    replace(rml, '<div id="tube-discussion-text" data-if="discussion || review">{{ selected_overview }}</div>\n',
            '<div id="tube-discussion-text" data-if="discussion || review" data-class-with-qr="qr">{{ selected_overview }}</div>\n' + RML_PANEL)
    replace(rml, '<div id="tube-review-footer" data-if="review &amp;&amp; !auth &amp;&amp; !description">', '<div id="tube-review-footer" data-if="review &amp;&amp; !auth &amp;&amp; !description &amp;&amp; !link">')
    replace(rml, '        <div id="tube-auth-footer" data-if="auth">', RML_FOOTER + '        <div id="tube-auth-footer" data-if="auth">')
    style = rcss.read_text(encoding="utf-8")
    if "#tube-qr-box" not in style:
        rcss.write_text(style.rstrip("\n") + "\n" + RCSS, encoding="utf-8")
