"""EVO Player's own name and mark, wherever a person using ProsperoTube could still meet them.

The app is built on EVO Player and says so where credit belongs: the About screen, the notices and
the README. Everywhere else the person is in ProsperoTube, so the places below say ProsperoTube
(or nothing), in the order a session meets them:

  - at launch, the notice with EVO's name and version (and the one that the controller opened);
  - the system notifications of a screenshot and of the comparison captures;
  - the rail of EVO's own screens (Player settings): its mark, its name, its "Quit" entry;
  - the caption style preview's sample sentence, the media information's engine line, the pickers of a
    stream and of an audio track, and what EVO's documents say when nothing names them;
  - the safe-to-close screen the app parks on after it was closed: mark, name and both sentences;
  - the launch screen EVO opens with, which this app never navigates to, for the day something does.

Every replacement is exact and must be found: an EVO update that rewords one of these places stops
the preparation here instead of letting the old name back in unnoticed.
"""

MARK = "../icons/tube-mark.png"
MARK_LARGE = "../icons/tube-mark-large.png"

# What must not be left in the files touched here once they are done (the credit lines of About are elsewhere).
GONE = ('"EVO Player"', "EVO PLAYER<", '"EVO PLAYER"', "EVO picks the", "Welcome to EVO", '"EVO: ', "Quit EVO", "Highlight EVO",
        "PAD OPEN OK")


def apply(work, player):
    evo = work / player
    touched = set()

    def replace(name, old, new):
        """Every place of a file that has EVO's text gets ours; a file with neither is a mismatch."""
        file = evo / name
        source = file.read_text(encoding="utf-8")
        touched.add(name)
        if old in source:
            file.write_text(source.replace(old, new), encoding="utf-8")
        elif new not in source:
            raise RuntimeError(f"Branding mismatch: {name}: EVO's text was not found: {old[:60]!r}")

    # ---- launch: no notice names EVO and its version, and none says that the controller opened ----
    app = "core/src/Application.cpp"
    replace(app, '    toast("EVO Player", (EVO_PLAYER_VERSION[0] == \'v\' || EVO_PLAYER_VERSION[0] == \'V\')\n'
                 '                         ? EVO_PLAYER_VERSION\n'
                 '                         : ("v" EVO_PLAYER_VERSION));\n',
            "    /* ProsperoTube: no notice at launch (EVO announces its own name and version here). */\n")
    replace(app, '        toast("PAD OPEN OK", "Controller connected");\n', "        /* ProsperoTube: a controller that opened needs no notice. */\n")
    # ---- system notifications ----
    replace(app, '"EVO: ', '"ProsperoTube: ')
    # ---- the rail of EVO's own screens ----
    screens = "ui_kit/src/evo_hui_screens.cpp"
    replace(screens, '    const std::uint32_t logo = ctx.textures.icon("../icons/icon_logo.png");\n    if (logo)\n        list.image(logo, {36, 64, 56, 56}',
            '    const std::uint32_t logo = ctx.textures.icon("' + MARK + '");\n    if (logo)\n        list.image(logo, {36, 64, 56, 56}')
    replace(screens, 'ui::text(list, fonts.display, "EVO", 112, 104, 34, kWhite);', 'ui::text(list, fonts.display, "ProsperoTube", 112, 102, 28, kWhite);')
    replace(screens, '{"Quit EVO", "../icons/icon_power.png"}', '{"Quit ProsperoTube", "../icons/icon_power.png"}')
    # ---- the caption preview, the media information, a notice without a title, the pickers ----
    document = "ui_rml/src/evo_rmlui_app.cpp"
    replace(document, "Welcome to EVO Player on PlayStation 5", "Welcome to ProsperoTube on PlayStation 5")
    replace("assets/rml/subtitles.rml", "Welcome to EVO Player on PlayStation 5", "Welcome to ProsperoTube on PlayStation 5")
    replace(document, '"EVO PRO DECODER "', '"PROSPEROTUBE DECODER "')
    replace("assets/rml/mediainfo.rml", "EVO PRO DECODER", "PROSPEROTUBE DECODER")
    replace(document, '"EVO PLAYER"', '"PROSPEROTUBE"')
    replace("assets/rml/toast.rml", '<div id="toast-title">EVO PLAYER</div>', '<div id="toast-title">PROSPEROTUBE</div>')
    replace("assets/rml/navbar.rml", '<div id="nav-app-label">EVO</div>', '<div id="nav-app-label">ProsperoTube</div>')
    replace("assets/rml/playback.rml", '<div id="music-title">EVO MUSIC</div>', '<div id="music-title">PROSPEROTUBE</div>')
    replace("core/src/screens/ProviderHostScreen.cpp", "EVO picks the quality", "ProsperoTube picks the quality")
    replace("core/src/screens/AudioTrackPickerScreen.cpp", "EVO picks the best quality available", "ProsperoTube picks the best quality available")
    # ---- safe to close ----
    closed = "ui_kit/src/evo_hui_misc.cpp"
    replace(closed, '    std::uint32_t logo = ctx.textures.icon("../icons/icon_logo.png");\n    if (logo)\n        list.image(logo, {cx - 60, 240, 120, 120}',
            '    std::uint32_t logo = ctx.textures.icon("' + MARK_LARGE + '");\n    if (logo)\n        list.image(logo, {cx - 60, 240, 120, 120}')
    replace(closed, 'ui::text(list, fonts.display, "EVO Player", cx, 440, 56, kWhite, Align::center);',
            'ui::text(list, fonts.display, "ProsperoTube", cx, 440, 56, kWhite, Align::center);')
    replace(closed, '"Highlight EVO Player and close it from the options menu."', '"Highlight ProsperoTube and close it from the options menu."')
    page = "assets/rml/closed.rml"
    replace(page, '<img id="closed-logo" src="../icons/icon_logo.png"/>', '<img id="closed-logo" src="' + MARK_LARGE + '"/>')
    replace(page, '<div id="closed-eyebrow">EVO PLAYER</div>', '<div id="closed-eyebrow">PROSPEROTUBE</div>')
    replace(page, "EVO is idle now", "ProsperoTube is idle now")
    replace(page, '<span class="closed-strong">EVO</span>', '<span class="closed-strong">ProsperoTube</span>')
    replace(page, '<div id="closed-footer">EVO PLAYER</div>', '<div id="closed-footer">PROSPEROTUBE</div>')
    # ---- EVO's launch screen ----
    launch = "core/src/screens/LaunchScreen.cpp"
    replace(launch, 'params.app_name = "EVO PLAYER";', 'params.app_name = "PROSPEROTUBE";')
    replace(launch, '"Welcome to EVO Player"', '"Welcome to ProsperoTube"')
    replace("assets/rml/launch.rml", '<div id="brand-name">EVO PLAYER</div>', '<div id="brand-name">PROSPEROTUBE</div>')
    replace("assets/rml/launch.rml", '<div id="hero-title">EVO PLAYER</div>', '<div id="hero-title">PROSPEROTUBE</div>')

    for name in sorted(touched):
        source = (evo / name).read_text(encoding="utf-8")
        for text in GONE:
            if text in source:
                raise RuntimeError(f"Branding mismatch: {name} still says {text!r}")
