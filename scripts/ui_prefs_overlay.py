"""Interface preferences of a profile on the browser host: interface scale and
video grid scale (page size, columns, style rules), card text, colour schemes,
the title marquee, the settings levels, and Circle on the main menu and on
search results."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

LAYOUT = '''// ProsperoTube interface layout
namespace {
/* What the last listed level is: the page of a list (comments, chat, review)
 * and the page of the card grid hold different numbers of rows. */
bool g_tube_youtube = false;
bool g_tube_list_mode = false;

/* Columns the Up/Down paging counts in. A list of the unscaled interface keeps
 * the paging it always had; any other list is one column. */
int tube_grid_columns()
{
    if (!g_tube_youtube) return 4;
    tube_ui_layout_t layout;
    tube_ui_layout(tube_ui_prefs(), &layout);
    if (g_tube_list_mode) return layout.list_rows == 8 ? 4 : 1;
    return layout.columns;
}

bool tube_read_sheet(const std::string& path, std::string& out)
{
    Rml::FileInterface* files = Rml::GetFileInterface();
    Rml::FileHandle handle = files ? files->Open(path) : 0;
    if (!handle) return false;
    const size_t length = files->Length(handle);
    out.assign(length, '\\0');
    const size_t got = length ? files->Read(&out[0], length, handle) : 0;
    files->Close(handle);
    out.resize(got);
    return got > 0;
}
} // namespace

size_t evo_provider_page_size()
{
    if (!g_tube_youtube) return 8;
    tube_ui_layout_t layout;
    tube_ui_layout(tube_ui_prefs(), &layout);
    return (size_t)(g_tube_list_mode ? layout.list_rows : layout.columns * layout.rows);
}

/* Restyle the browser when the profile's interface preferences change: the
 * scheme's colours in the sheet, the rules of the scales and card text after
 * it, the dp ratio, and the page that holds the focused row. The defaults
 * leave an untouched document alone. */
void EvoRmlProviderHost::TubeApplyStyle()
{
    if (m_provider_id != "youtube" || !m_doc || !m_context) return;
    const unsigned revision = tube_ui_revision();
    if (m_tube_style_document == m_doc && m_tube_style_revision == revision) return;
    const bool fresh = m_tube_style_document != m_doc;
    const TubeUiPrefs* prefs = tube_ui_prefs();
    if (fresh) { m_tube_styled = false; m_tube_base_sheet.clear(); }
    if (m_tube_style_context != m_context) {
        m_tube_base_dp = m_context->GetDensityIndependentPixelRatio();
        m_tube_style_context = m_context;
    }
    /* A level that still waits for its first focus has no focused row yet. */
    const int focused = m_needs_initial_focus ? m_focus_slot : FocusedRowIndex();
    const size_t absolute = m_row_offset + (size_t)(focused > 0 ? focused : 0);
    m_tube_style_document = m_doc;
    m_tube_style_revision = revision;
    if (!tube_ui_plain(prefs) || m_tube_styled) {
        if (m_tube_base_sheet.empty()) {
            std::string path = m_doc->GetSourceURL().c_str();
            const size_t slash = path.find_last_of('/');
            path = (slash == std::string::npos ? std::string() : path.substr(0, slash + 1)) + "youtube.rcss";
            tube_read_sheet(path, m_tube_base_sheet);
        }
        if (!m_tube_base_sheet.empty()) {
            std::string sheet(m_tube_base_sheet.size() + 1, '\\0');
            const int length = tube_ui_scheme_sheet(m_tube_base_sheet.c_str(), prefs->scheme, &sheet[0], sheet.size());
            sheet.resize(length > 0 ? (size_t)length : 0);
            static char rules[8192];
            if (length > 0 && tube_ui_rules(prefs, rules, sizeof rules) >= 0) {
                sheet += "\\n";
                sheet += rules;
                if (auto container = Rml::Factory::InstanceStyleSheetString(sheet)) {
                    m_doc->SetStyleSheetContainer(std::move(container));
                    m_tube_styled = true;
                }
            }
        }
    }
    tube_ui_layout_t layout;
    tube_ui_layout(prefs, &layout);
    const float ratio = m_tube_base_dp * (float)layout.scale;
    const float change = m_context->GetDensityIndependentPixelRatio() - ratio;
    if (change > 0.0001f || change < -0.0001f) m_context->SetDensityIndependentPixelRatio(ratio);
    /* The page may hold another number of cards now: keep the focused row on it. */
    if (!fresh && !m_all_rows.empty()) {
        const size_t page = EVO_PROVIDER_PAGE_SIZE;
        SetPageOffset(absolute / page * page, (int)(absolute % page));
    }
    m_tube_marquee_key = "\\x01";   /* no row has this key: the marquee starts over */
    m_tube_marquee_offset = -1.0f;
    m_dirty = true;
}

/* The focused card's title, when it does not fit on its one line, travels to
 * its end and starts over (tube_ui_marquee). */
void EvoRmlProviderHost::TubeMarquee()
{
    if (m_provider_id != "youtube" || !m_doc || !m_context) return;
    const TubeUiPrefs* prefs = tube_ui_prefs();
    const bool enabled = prefs->title_scroll && prefs->title_lines == 1 && !g_tube_list_mode;
    Rml::Element* card = enabled ? m_context->GetFocusElement() : nullptr;
    while (card && card->GetAttribute<Rml::String>("rowid", Rml::String()).empty()) card = card->GetParentNode();
    Rml::String key;
    if (card) {
        key = card->GetAttribute<Rml::String>("rowkey", Rml::String());
        if (key.empty()) key = card->GetAttribute<Rml::String>("rowid", Rml::String());
    }
    const double now = Rml::GetSystemInterface() ? Rml::GetSystemInterface()->GetElapsedTime() : 0.0;
    if (m_tube_marquee_key != key.c_str()) {
        /* Another card (or none) has the focus: every title returns to its start. */
        Rml::ElementList titles;
        m_doc->QuerySelectorAll(titles, ".tube-card-title");
        for (Rml::Element* title : titles)
            if (title->GetScrollLeft() != 0.0f) { title->SetScrollLeft(0.0f); m_dirty = true; }
        m_tube_marquee_key = key.c_str();
        m_tube_marquee_start = now;
        m_tube_marquee_offset = 0.0f;
    }
    Rml::Element* title = card ? card->QuerySelector(".tube-card-title") : nullptr;
    if (!title) return;
    const double overflow = (double)title->GetScrollWidth() - (double)title->GetClientWidth();
    const float offset = (float)tube_ui_marquee(now - m_tube_marquee_start, overflow, prefs->scroll_speed,
                                               (double)m_context->GetDensityIndependentPixelRatio());
    const float moved = offset - m_tube_marquee_offset;
    if (moved > 0.5f || moved < -0.5f) {
        title->SetScrollLeft(offset);
        m_tube_marquee_offset = offset;
        m_dirty = true;
    }
}
// End ProsperoTube interface layout

'''

SEARCH_EXIT = '''        // ProsperoTube search exit (extracted by host tests).
        if (m_providerId == "youtube" && evo_rmlui_provider_search_results() &&
            tube_search_exit_press(tube_ui_now_ms()) == TUBE_UI_PRESS_ARMED) {
            toast("SEARCH", "Press Circle again to leave the results");
            return true;
        }
        // End ProsperoTube search exit.
'''

APP_EXIT = '''            // ProsperoTube app exit (extracted by host tests).
            const int exitAction = tube_app_exit_press(tube_ui_now_ms());
            if (exitAction == TUBE_UI_PRESS_ARMED) {
                toast("PROSPEROTUBE", "Press Circle again to close ProsperoTube");
                return true;
            }
            if (exitAction == TUBE_UI_PRESS_NOW) {
                Application::getInstance().requestSoftClose();
                return true;
            }
            // End ProsperoTube app exit.
'''


def levels():
    """Settings ids of the interface preferences that open a list of their own."""
    groups = re.findall(r'\{"(settings:ui:[a-z-]+)", "', (ROOT / "native/tube_ui_style.h").read_text(encoding="utf-8"))
    if len(groups) != 10 or len(set(groups)) != 10:
        raise RuntimeError("Interface preference groups changed: update the settings levels")
    return ["settings:ui", "settings:menu"] + groups


def apply_ui_prefs_overlay(replace, player, host):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    # The lists of the interface preferences are real levels; a row of the
    # video actions menu is one too, so Circle returns to the menu's list.
    replace(ui, 'id != "settings:languages" && ',
            'id != "settings:languages" && ' + "".join(f'id != "{level}" && ' for level in levels())
            + '!(id.rfind("settings:menu:", 0) == 0 && id != "settings:menu:reset" && id.find(\':\', 14) == std::string::npos) && ')
    # Page size and columns follow the profile's scales.
    replace(header, "constexpr size_t EVO_PROVIDER_PAGE_SIZE = 8;",
            "/* ProsperoTube: the page follows the profile's interface scale and video grid\n"
            " * scale (native/tube_ui_style.h); eight cards with the defaults. */\n"
            "size_t evo_provider_page_size();\n#define EVO_PROVIDER_PAGE_SIZE (evo_provider_page_size())")
    replace(header, "    void ReplaceFolder(const char* parent, const char* title);",
            "    void ReplaceFolder(const char* parent, const char* title);\n    void TubeApplyStyle();\n    void TubeMarquee();\n"
            "    bool SearchResultsShown() const { return IsOpen() && !m_model.query.empty(); }")
    replace(header, "    bool m_startup_pending = false;",
            "    bool m_startup_pending = false;\n"
            "    unsigned m_tube_style_revision = 0;\n    const void* m_tube_style_document = nullptr;\n"
            "    const void* m_tube_style_context = nullptr;\n    bool m_tube_styled = false;\n    float m_tube_base_dp = 1.0f;\n"
            "    std::string m_tube_base_sheet, m_tube_marquee_key;\n    double m_tube_marquee_start = 0.0;\n"
            "    float m_tube_marquee_offset = 0.0f;\n    size_t m_tube_advance_seen = 0;\n    int m_tube_advance_from = -1;")
    replace(header, "int evo_rmlui_provider_qr_shown(void);",
            "int evo_rmlui_provider_qr_shown(void);\n/* 1 = typed search results are on screen. */\nint evo_rmlui_provider_search_results(void);")
    replace(ui, '#include "tube_qr_page.h"', '#include "tube_qr_page.h"\n#include "tube_ui_style.h"')
    replace(ui, "struct ItemsCtx {\n    EvoRmlProviderHost* host;", LAYOUT + "struct ItemsCtx {\n    EvoRmlProviderHost* host;")
    replace(ui, "int evo_rmlui_provider_qr_shown(void)\n{",
            "int evo_rmlui_provider_search_results(void)\n{\n    return EvoRmlProviderHost::Instance().SearchResultsShown() ? 1 : 0;\n}\n\n"
            "int evo_rmlui_provider_qr_shown(void)\n{")
    replace(ui, '    if (m_model_handle) { m_model_handle.DirtyVariable("discussion"); m_model_handle.DirtyVariable("review"); }\n',
            '    if (m_model_handle) { m_model_handle.DirtyVariable("discussion"); m_model_handle.DirtyVariable("review"); }\n'
            '    g_tube_youtube = m_provider_id == "youtube";\n    g_tube_list_mode = m_model.discussion || m_model.review;\n')
    replace(ui, "m_model.chat = m_model.discussion = m_model.review = m_model.auth = m_model.description = false;",
            "m_model.chat = m_model.discussion = m_model.review = m_model.auth = m_model.description = false; g_tube_list_mode = false;", count=2)
    # Up/Down paging for any number of columns and rows; 4 x 2 gives the earlier rules.
    replace(ui, "        int idx = FocusedRowIndex();\n\n        /* Deterministic 2x4 grid paging for Up/Down */",
            "        int idx = FocusedRowIndex();\n        const int tube_cols = tube_grid_columns();\n"
            "        /* A one-column list steps row by row itself, however small its rows are drawn. */\n"
            "        if (tube_cols == 1 && idx >= 0 && ((k == KeyDown && idx + 1 < (int)m_model.rows.size()) || (k == KeyUp && idx > 0))) {\n"
            "            FocusPublishedRow(k == KeyDown ? idx + 1 : idx - 1);\n"
            "            m_dirty = true;\n            UpdateSelectedPreview();\n            return true;\n        }\n\n"
            "        /* Deterministic grid paging for Up/Down */")
    replace(ui, "            if (idx < 4 && idx + 4 >= n && n > 4) {",
            "            if (idx + tube_cols >= n && idx / tube_cols < (n - 1) / tube_cols) {")
    replace(ui, "            if (idx >= 4 || idx + 4 >= (int)m_model.rows.size()) {", "            if (idx + tube_cols >= n) {")
    replace(ui, "                    int target_col = idx % 4;", "                    int target_col = idx % tube_cols;")
    replace(ui, "                FetchMoreAndAdvance(idx % 4);", "                m_tube_advance_from = idx;\n                FetchMoreAndAdvance(idx % tube_cols);")
    replace(ui, "        FetchMoreAndAdvance(idx >= 0 ? idx : 0);", "        m_tube_advance_from = -1;\n        FetchMoreAndAdvance(idx >= 0 ? idx : 0);")
    replace(ui, "            if (idx < 4) {\n                if (m_row_offset >= EVO_PROVIDER_PAGE_SIZE) {\n                    int target_col = idx + 4;",
            "            if (idx < tube_cols) {\n                if (m_row_offset >= EVO_PROVIDER_PAGE_SIZE) {\n"
            "                    int target_col = idx + tube_cols * ((int)EVO_PROVIDER_PAGE_SIZE / tube_cols - 1);")
    # A position saved under another page size still names the same row.
    replace(ui, "        size_t off = m_saved_offset;\n        int slot = m_saved_slot;\n",
            "        size_t off = m_saved_offset;\n        int slot = m_saved_slot;\n"
            "        if (m_provider_id == \"youtube\") {\n"
            "            const size_t absolute = off + (size_t)(slot > 0 ? slot : 0), page = EVO_PROVIDER_PAGE_SIZE;\n"
            "            off = absolute / page * page; slot = (int)(absolute % page);\n        }\n")
    # More rows for a page that was not full arrive on that page: stay on it.
    replace(ui, "    m_advance_after_load = true;\n    m_advance_col = col;",
            "    m_advance_after_load = true;\n    m_advance_col = col;\n    m_tube_advance_seen = m_all_rows.size();")
    replace(ui, "        m_advance_after_load = false;\n        if (m_row_offset + EVO_PROVIDER_PAGE_SIZE < m_all_rows.size())",
            "        m_advance_after_load = false;\n"
            "        if (m_tube_advance_seen < m_row_offset + EVO_PROVIDER_PAGE_SIZE && m_tube_advance_seen < m_all_rows.size())\n"
            "        {\n"
            "            /* Down goes to the card below when it has arrived and else waits on its row,\n"
            "             * which is filling in; R1 goes to the first new card. */\n"
            "            const int shown = (int)std::min<size_t>(EVO_PROVIDER_PAGE_SIZE, m_all_rows.size() - m_row_offset);\n"
            "            const int below = m_tube_advance_from + tube_grid_columns();\n"
            "            SetPageOffset(m_row_offset, m_tube_advance_from < 0 ? (int)(m_tube_advance_seen - m_row_offset)\n"
            "                                        : below < shown ? below : m_tube_advance_from);\n"
            "        }\n"
            "        else if (m_row_offset + EVO_PROVIDER_PAGE_SIZE < m_all_rows.size())")
    replace(ui, "    if (!IsOpen()) return;\n    // ProsperoTube screen dimming",
            "    if (!IsOpen()) return;\n    // ProsperoTube interface style\n    TubeApplyStyle();\n    TubeMarquee();\n"
            "    // End ProsperoTube interface style\n    // ProsperoTube screen dimming")
    # A chosen colour scheme lends its accent to the player controls; with the
    # default scheme EVO's own theme (Player settings, device-wide) colours them.
    hud = player + "ui_kit/src/evo_hui_osd.cpp"
    replace(hud, '#include "tube_player_extra.h"', '#include "tube_player_extra.h"\n#include "tube_ui_style.h"')
    replace(hud, "void OsdScreen::draw(DrawList &list, const Context &ctx) const\n{\n    const ui::Fonts &fonts = ctx.fonts;",
            "void OsdScreen::draw(DrawList &list, const Context &evo_ctx) const\n{\n"
            "    // ProsperoTube colour scheme HUI.\n"
            "    Palette tube_palette = evo_ctx.palette;\n"
            "    if (const int tube_scheme = tube_ui_prefs()->scheme)\n"
            "        tube_palette.accent = Color::rgb(g_tube_schemes[tube_scheme].accent);\n"
            "    const Context ctx{evo_ctx.fonts, evo_ctx.textures, tube_palette, evo_ctx.time};\n"
            "    // End ProsperoTube colour scheme HUI.\n"
            "    const ui::Fonts &fonts = ctx.fonts;")
    # Circle on typed results and on the main menu.
    replace(host, '#include "evo_provider.h"\n#include "tube_provider.h"',
            '#include "evo_provider.h"\n#include "tube_provider.h"\n#include "tube_ui_style.h"')
    replace(host, "static long long queue_retry_now_ms() {",
            "static uint64_t tube_ui_now_ms() {\n    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(\n"
            "        std::chrono::steady_clock::now().time_since_epoch()).count();\n}\nstatic long long queue_retry_now_ms() {")
    replace(host, "        if (evo_rmlui_provider_key(EvoRmlProviderHost::KeyBack) != 0)\n            return true;",
            SEARCH_EXIT + "        if (evo_rmlui_provider_key(EvoRmlProviderHost::KeyBack) != 0)\n            return true;")
    replace(host, '        if (m_providerId == "youtube") {\n            evo_rmlui_provider_show_setup(); return true;\n        }',
            '        if (m_providerId == "youtube") {\n' + APP_EXIT + '            evo_rmlui_provider_show_setup(); return true;\n        }')
