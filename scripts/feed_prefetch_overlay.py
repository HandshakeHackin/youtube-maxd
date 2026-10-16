"""Classic feeds read ahead: the next page is asked for before Down reaches the end.

Before, a Classic list fetched its next page only when Down was pressed on the
last row of what had arrived, and then waited on that row. Now, once the focus
is within two rows of the last row that has arrived, the next page is asked for
in the background: the rows on screen stay usable with no loading note, the
focus and page stay where they are when the rows land, a Down that arrives
while they are on their way moves as soon as they land, and a page that brings
nothing (or fails) stops the reading ahead until the list is opened again.
Applied after ui_prefs_overlay (it uses tube_grid_columns and m_tube_advance_seen).
"""

PREFETCH = r'''/* Feeds read ahead (scripts/feed_prefetch_overlay.py). */
void EvoRmlProviderHost::TubePrefetch()
{
    if (m_provider_id != "youtube" || !m_has_more || m_model.loading || m_tube_prefetching || m_tube_prefetch_stopped ||
        m_advance_after_load || m_needs_initial_focus || !m_model.query.empty() || m_model.chat || m_model.review ||
        m_all_rows.empty()) return;   /* a focus still to be seeded is not where the person is */
    const int idx = FocusedRowIndex();
    if (idx < 0) return;
    const size_t ahead = (size_t)std::max(2 * tube_grid_columns(), 4);
    if (m_row_offset + (size_t)idx + ahead < m_all_rows.size()) return;
    m_focus_slot = idx;   /* anything that seeds the focus again finds it where it is */
    m_tube_prefetching = true;
    RequestPage(m_stack.empty() ? "" : m_stack.back().c_str(), ++m_page);
    if (m_tube_prefetching && !m_model.loading) {   /* refused at once: no answer will come */
        m_tube_prefetching = false; m_tube_prefetch_stopped = true; --m_page;
    } else if (m_tube_prefetching) {                /* on its way: the rows on screen stay usable */
        m_model.loading = false;
        if (m_model_handle) m_model_handle.DirtyVariable("loading");
    }
}

void EvoRmlProviderHost::Tick()
{'''


def apply_feed_prefetch_overlay(replace, player):
    header = player + "ui_rml/include/evo_rmlui_provider.h"
    ui = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(header, "    int m_tube_advance_from = -1;",
            "    int m_tube_advance_from = -1;\n    bool m_tube_prefetching = false;\n    bool m_tube_prefetch_stopped = false;\n"
            "    void TubePrefetch();")
    # Anchors stay clear of settings_pages_overlay's edits around them, which come first.
    replace(ui, "    if (page == 0) {\n        m_model.rows.clear();",
            "    if (page == 0) {\n        m_tube_prefetching = false; m_tube_prefetch_stopped = false;\n        m_model.rows.clear();")
    replace(ui, '    self->m_model.loading = false;\n    if (self->m_model_handle) self->m_model_handle.DirtyVariable("loading");\n\n'
            '    // ProsperoTube startup fallback',
            '    self->m_model.loading = false;\n    if (self->m_model_handle) self->m_model_handle.DirtyVariable("loading");\n'
            '    const bool tube_prefetched = self->m_tube_prefetching;\n    self->m_tube_prefetching = false;\n'
            '    if (tube_prefetched && (!ok || count == 0)) self->m_tube_prefetch_stopped = true; /* no reading ahead past it */\n\n'
            '    // ProsperoTube startup fallback')
    replace(ui, "    self->ApplyItems(items, count, has_more);\n",
            "    /* Rows read ahead land behind the person: the page and the focused card stay. */\n"
            "    const int tube_keep = tube_prefetched && !self->m_advance_after_load ? self->FocusedRowIndex() : -1;\n"
            "    const size_t tube_offset = self->m_row_offset;\n"
            "    self->ApplyItems(items, count, has_more);\n"
            "    if (tube_keep >= 0) self->SetPageOffset(tube_offset, tube_keep);\n")
    replace(ui, "    if (!m_has_more || m_model.loading || !m_model.query.empty()) return;\n    m_advance_after_load = true;",
            "    if (m_tube_prefetching) {   /* the next page is already on its way: move once it lands */\n"
            "        m_advance_after_load = true; m_advance_col = col; m_tube_advance_seen = m_all_rows.size(); return;\n    }\n"
            "    if (!m_has_more || m_model.loading || !m_model.query.empty()) return;\n    m_advance_after_load = true;")
    replace(ui, "    TubeApplyStyle();\n    TubeMarquee();\n    // End ProsperoTube interface style",
            "    TubeApplyStyle();\n    TubeMarquee();\n    TubePrefetch();\n    // End ProsperoTube interface style")
    replace(ui, "void EvoRmlProviderHost::Tick()\n{", PREFETCH)
