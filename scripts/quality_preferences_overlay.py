"""Open resolution and frame-rate selectors as settings submenus."""


def apply_quality_preferences_overlay(replace, player):
    replace(player + 'ui_rml/src/evo_rmlui_provider.cpp', 'id != "settings:audio-languages"',
            'id != "settings:audio-languages" && id != "settings:quality" && id != "settings:frame-rate"')
