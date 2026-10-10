"""Preserve provider stick controls through the application's screenshot layer."""


def apply_controller_overlay(replace, player):
    app = player + "core/src/Application.cpp"
    replace(app, """            /*
             * Either stick click captures the screen, and is swallowed so no
             * screen sees it as a normal press.
             *
             * It was bound to L3 alone and never once fired: across a logged
             * session the pad reported 0x0004 (R3) for the stick click and
             * 0x0002 (L3) not at all. Accepting both means it works whichever
             * bit this pad and firmware decide to send.
             */
            if (pressed & (PadButtons::L3 | PadButtons::R3)) {""", """            /* ProsperoTube controller routing: provider stick clicks belong
             * to discussion/chat controls; player R3 jumps to a highlight.
             * Keyboard/browser input keeps its buttons. Player L3 and both
             * sticks on other screens retain screenshot capture. */
            const bool providerSticks =
                m_screenManager->getCurrentScreenId() == ScreenId::EmbyBrowse;
            const uint32_t captureMask = m_screenManager->getCurrentScreenId() == ScreenId::Player ?
                PadButtons::L3 : (PadButtons::L3 | PadButtons::R3);
            if (!providerSticks && !evo_keyboard_is_open() && !evo_webui_active() &&
                (pressed & captureMask)) {""")
    replace(app, "                pressed &= ~(PadButtons::L3 | PadButtons::R3);",
            "                pressed &= ~captureMask;")
