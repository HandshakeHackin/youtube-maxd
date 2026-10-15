"""The Afterglow interface inside the app: sources, the pinned UI kit and three hooks in EVO.

prepare_native.py calls apply() after EVO's tree and every other overlay are in
place. It
  - (stage_kit, before the other overlays) lays the pinned UI kit of
    scripts/ui_kit.py over the copy EVO pins, so EVO's own kit screens and the
    interface use one kit; and gives that copy's gfx::Font the door for the
    text engine (scripts/kit_text_patch.py);
  - copies native/afterglow and native/text beside EVO's kit screens and adds
    them to EVO's Makefile, with the faces of the approved boards as assets;
  - hooks EVO in three places: where the provider screen is drawn, where it
    takes the pad, and where the kit screens report what is on display.
The interface draws only while the Interface preference (or the console
session's override file) says so; with it off, nothing of EVO behaves
differently.
"""
import pathlib
import shutil

import kit_text_patch
import ui_kit

FONTS = (("Inter-Regular.ttf", "afterglow-inter-regular.ttf"), ("Inter-SemiBold.ttf", "afterglow-inter-semibold.ttf"),
         ("Montserrat-Medium.ttf", "afterglow-montserrat-medium.ttf"), ("DejaVuSansMono.ttf", "afterglow-dejavu-sans-mono.ttf"))
FONT_LICENSES = ("Inter-LICENSE.txt", "Montserrat-LICENSE.txt", "DejaVu-LICENSE.txt")

RENDER = '''int evo_hui_render_afterglow(int width, int height)
{
    /* ProsperoTube: the Afterglow interface draws the YouTube provider's screen
     * with this application's batch and fonts (native/afterglow/app/mount.cpp). */
    Kit &k = kit();
    if (!tube_afterglow_enabled() || !k.start())
        return 0;
    const float dt = k.step();
    evo_agc_runtime_frame_begin();
    if (!afterglow::mount_render(k.batch, k.fonts, dt, width, height))
        return 0;
    k.current = Screen::provider;
    k.drawn_last_frame = Screen::provider;
    k.idle_frames = 0;
    return 1;
}

'''

FOCUS = '''    Kit &k = kit();
    /* ProsperoTube: while the Afterglow interface is on screen, its own snapshot is the report. */
    if (tube_afterglow_shown())
    {
        if (screen_doc)
            *screen_doc = "afterglow";
        return tube_afterglow_snapshot();
    }
'''

PAD = '''    // ProsperoTube Afterglow: while the interface of the kit is on screen the pad is its, and a video it
    // chose goes to the player the classic browser uses.
    if (!m_streamPick && tube_afterglow_shown()) {
        tube_afterglow_input(pressed, held);
        char tube_item[EVO_PROVIDER_MAX_ITEM_ID], tube_title[EVO_PROVIDER_MAX_TITLE];
        int tube_live = 0;
        long long tube_resume = 0;
        if (tube_afterglow_take_play(tube_item, sizeof tube_item, tube_title, sizeof tube_title, &tube_live, &tube_resume)) {
            evo_rmlui_provider_offer_selection(tube_item, tube_title, tube_live, tube_resume);
            startSelected();
        }
        if (!tube_afterglow_take_exit()) return true;
        // Circle on the interface's last screen: what follows applies the profile's rule for the main menu.
        pressed = PadButtons::Circle;
    }
'''

OFFER = '''/* ProsperoTube Afterglow: a video chosen in the interface of the kit, offered as the classic browser offers one. */
extern "C" void evo_rmlui_provider_offer_selection(const char *item_id, const char *title, int is_live, long long resume_sec)
{
    std::memset(&g_selection, 0, sizeof(g_selection));
    std::snprintf(g_selection.provider_id, sizeof(g_selection.provider_id), "%s", "youtube");
    std::snprintf(g_selection.item_id, sizeof(g_selection.item_id), "%s", item_id ? item_id : "");
    std::snprintf(g_selection.title, sizeof(g_selection.title), "%s", title ? title : "");
    g_selection.is_live = is_live;
    g_selection.resume_sec = resume_sec;
    g_selection_pending = true;
}

'''


# Original, reproducible ProsperoTube assets; no UI-kit recordings are packaged.
SOUND_NOTICE = """ProsperoTube Afterglow interface sounds.
Original procedural synthesis using Python math/wave; no sampled or downloaded audio.
Copyright (C) 2026 BlackBearReloaded. GPL-3.0-or-later.
Source: scripts/generate_afterglow_sounds.py; metadata: native/afterglow/sound/recordings/manifest.json.
Human listening and console acceptance remain separate from format/level checks.

"""


def sound_files(root, kit):
    """Original assets under hui/afterglow/sfx; kit is retained for caller compatibility."""
    return [(file.parent.name + "/" + file.name, file)
            for file in sorted((root / "native/afterglow/sound/recordings").glob("*/*.wav"))]


SOUND_PARK = '''                if (m_soundEffectEngine)   m_soundEffectEngine->shutdown();
                tube_afterglow_sound_park(); /* ProsperoTube stage S: the interface's port and vibration let go */
'''

SOUND_EXIT = '''    if (m_soundEffectEngine) {
        m_soundEffectEngine->shutdown();
    }
    tube_afterglow_sound_close(); /* ProsperoTube stage S */
'''


def apply_sound(root, work, replace, player, kit):
    """Interface sound and vibration (native/afterglow/sound): the recordings as assets with where they come from,
    the sources in EVO's Makefile, and three places of EVO's application: the pad it opened, its park, its exit."""
    evo = work / player
    sounds = evo / "assets/hui/afterglow/sfx"
    shutil.rmtree(sounds, ignore_errors=True)
    listing = [SOUND_NOTICE]
    for name, file in sound_files(root, kit):
        (sounds / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(file, sounds / name)
        listing.append("%-28s %7d bytes  ProsperoTube %s\n" %
                       (name, file.stat().st_size, file.relative_to(root).as_posix()))
    notices = evo / "assets/prosperotube-notices"
    notices.mkdir(exist_ok=True)
    (notices / "afterglow-sounds.txt").write_text("".join(listing), encoding="utf-8")
    shutil.copyfile(root / "native/afterglow/sound/recordings/manifest.json", notices / "afterglow-sounds.json")
    shutil.copyfile(root / "native/afterglow/sound/app/tube_afterglow_sound.h", evo / "addons/include/tube_afterglow_sound.h")
    if "ui_kit/src/afterglow/sound" not in (evo / "Makefile").read_text(encoding="utf-8"):
        replace(player + "Makefile", "                   $(wildcard ui_kit/src/afterglow/app/*.cpp) $(wildcard ui_kit/src/text/*.cpp)\n",
                "                   $(wildcard ui_kit/src/afterglow/app/*.cpp) $(wildcard ui_kit/src/text/*.cpp) \\\n"
                "                   $(wildcard ui_kit/src/afterglow/sound/*.cpp) $(wildcard ui_kit/src/afterglow/sound/app/*.cpp)\n")
    application = player + "core/src/Application.cpp"
    if "tube_afterglow_sound" not in (work / application).read_text(encoding="utf-8"):
        replace(application, '#include "tube_provider.h"\n', '#include "tube_provider.h"\n#include "tube_afterglow_sound.h"\n')
        replace(application, "    evo_feedback_init(m_padHandle, SoundEffectCallback);\n",
                "    evo_feedback_init(m_padHandle, SoundEffectCallback);\n"
                "    tube_afterglow_sound_pad(m_padHandle); /* ProsperoTube stage S: the pad the interface vibrates */\n")
        replace(application, "                if (m_soundEffectEngine)   m_soundEffectEngine->shutdown();\n", SOUND_PARK)
        replace(application, "    if (m_soundEffectEngine) {\n        m_soundEffectEngine->shutdown();\n    }\n", SOUND_EXIT)
# end of stage S


def stage_kit(work):
    """The pinned UI kit in place of the copy EVO pins. Called before the overlays, so that the ones
    that patch kit files (the language hook in ui/fonts.hpp) patch the kit that is compiled."""
    kit = ui_kit.checkout()
    target = work / "third_party/ps5-homebrew-ui"
    stamp = target / ".prosperotube-kit"
    if stamp.exists() and stamp.read_text(encoding="utf-8").strip() == ui_kit.COMMIT:
        return
    for name in ("src", "assets/fonts", "assets/audio"):
        shutil.rmtree(target / name, ignore_errors=True)
        shutil.copytree(kit / name, target / name)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copyfile(kit / name, target / name)
    stamp.write_text(ui_kit.COMMIT + "\n", encoding="utf-8")


ATLAS_UPDATE = r"""bool AgcBatch::update_texture(std::uint32_t handle, int width, int height, const std::uint8_t *rgba)
{
    if (!ok_ || handle == 0 || hui::gfx::is_font_handle(handle) ||
        handle > textures_.size() || !textures_[handle - 1])
        return false;
    Texture *replacement = make_texture(width, height, 4, rgba);
    if (!replacement)
        return false;
    // Earlier draws have copied the old descriptor; its allocation stays fenced.
    Texture *previous = textures_[handle - 1];
    textures_[handle - 1] = replacement;
    pending_free_.push_back({previous, kFreeDelayFrames});
    return true;
}

"""


def apply_atlas_update(replace, player):
    replace(player + "ui_kit/include/hui_agc_batch.hpp",
            "    void release_texture(std::uint32_t handle);",
            "    bool update_texture(std::uint32_t handle, int width, int height, const std::uint8_t *rgba);\n"
            "    void release_texture(std::uint32_t handle);")
    replace(player + "ui_kit/src/hui_agc_batch.cpp",
            "void AgcBatch::release_texture(std::uint32_t handle)",
            ATLAS_UPDATE + "void AgcBatch::release_texture(std::uint32_t handle)")


def apply(root, work, replace, player):
    apply_atlas_update(replace, player)
    evo = work / player
    kit = ui_kit.checkout()
    target = work / "third_party/ps5-homebrew-ui"
    # The kit's font gets the door for the text engine (idempotent).
    header, source = kit_text_patch.patch((target / "src/gfx/font.hpp").read_text(encoding="utf-8"),
                                          (target / "src/gfx/font.cpp").read_text(encoding="utf-8"))
    for name, patched in (("src/gfx/font.hpp", header), ("src/gfx/font.cpp", source)):
        if (target / name).read_text(encoding="utf-8") != patched:
            (target / name).write_text(patched, encoding="utf-8")

    # ---- sources beside EVO's kit screens ----
    for name, folder in (("native/afterglow", "ui_kit/src/afterglow"), ("native/text", "ui_kit/src/text")):
        destination = evo / folder
        shutil.rmtree(destination, ignore_errors=True)
        shutil.copytree(root / name, destination, ignore=shutil.ignore_patterns("ps5", "licenses", "*.md"))
    shutil.copyfile(root / "native/afterglow/app/afterglow.h", evo / "addons/include/tube_afterglow.h")
    for source_name, asset in FONTS:
        shutil.copyfile(kit / "third_party/fonts" / source_name, evo / "assets/fonts" / asset)
    notices = evo / "assets/prosperotube-notices"
    notices.mkdir(exist_ok=True)
    for name in FONT_LICENSES:
        shutil.copyfile(kit / "third_party/fonts" / name, notices / ("afterglow-" + name))
    for file in sorted((root / "native/text/licenses").iterdir()):
        shutil.copyfile(file, notices / ("text-" + file.name))

    makefile = (evo / "Makefile").read_text(encoding="utf-8")
    if "ui_kit/src/afterglow" not in makefile:
        replace(player + "Makefile", "                   ui_kit/src/evo_hui_provider.cpp\nendif\n",
                "                   ui_kit/src/evo_hui_provider.cpp\n"
                "    # ProsperoTube: the Afterglow interface and the text engine behind the kit's fonts.\n"
                "    UI_KIT_SRCS += $(wildcard ui_kit/src/afterglow/*.cpp) $(wildcard ui_kit/src/afterglow/screens/*.cpp) \\\n"
                "                   $(wildcard ui_kit/src/afterglow/app/*.cpp) $(wildcard ui_kit/src/text/*.cpp)\n"
                "endif\n")
        replace(player + "Makefile", "    -I$(HUI_ROOT)/src -I$(HUI_ROOT) -Iui_kit/include\n",
                "    -I$(HUI_ROOT)/src -I$(HUI_ROOT) -Iui_kit/include \\\n"
                "    -Iui_kit/src -Iui_kit/src/text -I. \\\n"
                "    -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/harfbuzz -I$(PS5_PAYLOAD_SDK)/target/user/homebrew/include/fribidi\n")

    # ---- EVO's kit application: draw the interface, report it ----
    app = player + "ui_kit/src/evo_hui_app.cpp"
    if "evo_hui_render_afterglow" not in (work / app).read_text(encoding="utf-8"):
        replace(app, '#include "hui_agc_batch.hpp"\n', '#include "hui_agc_batch.hpp"\n#include "afterglow/app/mount.hpp"\n#include "tube_afterglow.h"\n')
        replace(app, "int evo_hui_render_provider(int width, int height)\n{", RENDER + "int evo_hui_render_provider(int width, int height)\n{")
        replace(app, "const char *evo_hui_dev_focus_json(const char **screen_doc)\n{\n    Kit &k = kit();\n",
                "const char *evo_hui_dev_focus_json(const char **screen_doc)\n{\n" + FOCUS)
        replace(player + "ui_kit/include/evo_hui.h", "int  evo_hui_render_provider(int width, int height);",
                "int  evo_hui_render_provider(int width, int height);\n/* ProsperoTube: the Afterglow interface, when it is on; 0 = the classic browser draws. */\n"
                "int  evo_hui_render_afterglow(int width, int height);")

    # ---- the provider screen: the interface draws instead of the document, and can offer a video ----
    screen = player + "ui_rml/src/evo_rmlui_provider.cpp"
    if "evo_hui_render_afterglow" not in (work / screen).read_text(encoding="utf-8"):
        replace(screen, '    if (m_provider_id != "youtube" && (m_bundle_version == "embedded" || m_using_fallback) && RenderKit(width, height))\n        return true;\n',
                '    /* ProsperoTube: the Afterglow interface, when the profile chose it, draws the YouTube provider. */\n'
                '    if (m_provider_id == "youtube" && evo_hui_render_afterglow(width, height)) {\n'
                '        EvoRmlApp::Instance().NoteExternalDraw();\n        return true;\n    }\n'
                '    if (m_provider_id != "youtube" && (m_bundle_version == "embedded" || m_using_fallback) && RenderKit(width, height))\n        return true;\n')
        replace(screen, 'extern "C" int evo_rmlui_provider_take_selection(evo_provider_selection_t *out)\n{',
                OFFER + 'extern "C" int evo_rmlui_provider_take_selection(evo_provider_selection_t *out)\n{')
        replace(player + "ui_rml/include/evo_rmlui_provider.h", "int  evo_rmlui_provider_take_selection(evo_provider_selection_t *out);",
                "int  evo_rmlui_provider_take_selection(evo_provider_selection_t *out);\n"
                "void evo_rmlui_provider_offer_selection(const char *item_id, const char *title, int is_live, long long resume_sec);")

    # ---- the pad ----
    host = player + "core/src/screens/ProviderHostScreen.cpp"
    if "tube_afterglow_shown" not in (work / host).read_text(encoding="utf-8"):
        replace(host, "    if (m_streamPick) {\n        const int n = g_pending.choice_count;",
                PAD + "    if (m_streamPick) {\n        const int n = g_pending.choice_count;")
        replace(host, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_afterglow.h"', count=1)
    # ---- typed text: while the interface is on screen its own sheet takes it (Data::act(row, text)),
    # so the classic browser leaves the provider's prompt alone ----
    if "!tube_afterglow_shown() && tube_take_playlist_prompt" not in (work / host).read_text(encoding="utf-8"):
        replace(host, "        if (tube_take_playlist_prompt(&prompt)) {",
                "        if (!tube_afterglow_shown() && tube_take_playlist_prompt(&prompt)) {")

    start_hooks(work, replace, player)
    replace(player + "core/src/Application.cpp", "        evo_usb_remote_poll();\n",
            "        evo_usb_remote_poll();\n        tube_afterglow_keyboard_poll(m_screenManager && m_screenManager->getCurrentScreenId() == ScreenId::EmbyBrowse);\n")
    replace(player + "core/src/Application.cpp", "    m_shutdownDone = true;\n",
            "    m_shutdownDone = true;\n    tube_afterglow_keyboard_close();\n")
    apply_sound(root, work, replace, player, kit)  # stage S
    player_mount(work, replace, player)  # package H
    stop_confirmation_focus(replace, player)  # H11
    import afterglow_picker_overlay
    afterglow_picker_overlay.apply(root, work, replace, player)
    import background_play_overlay
    background_play_overlay.apply(root, work, replace, player)
    import afterglow_controls_overlay
    afterglow_controls_overlay.apply(work, replace, player)
    import video_destination_overlay
    video_destination_overlay.apply(root, work, replace, player)
    import native_frame_lease_overlay
    native_frame_lease_overlay.apply(root, work, replace, player)


# ---- a chosen video until the player has the screen ---------------------------------------------------
# The provider screen resolves the video and opens its stream before EVO's player takes the screen; the
# interface is what is drawn all that time and keeps the player's screen up (Frame, Source::play_state).
# Three places tell it how the start stands (native/afterglow/app/play_start.hpp): the provider screen
# says every frame whether a start is pending and can still be called back, its status line carries the
# reason of a failure, and Circle on the player's screen calls a start back as the classic browser's does.

CANCEL = '''        // Circle while that video starts: called back as the classic browser's Circle does, while the stream is not yet being opened.
        if (tube_afterglow_take_cancel() && (m_resolving || m_tunePending) && !g_start_running) {
            ++g_pick_gen; tube_cancel(); m_resolving = false; m_tunePending = false;
            evo_rmlui_provider_set_tuning(0); evo_rmlui_provider_set_loading(0, "");
        }
'''

BUSY = '''    /* ProsperoTube Afterglow: how the start of a chosen video stands, for the player's screen the interface keeps up meanwhile. */
    tube_afterglow_play_busy((m_resolving || m_tunePending || g_start_running.load()) ? 1 : 0,
                             ((m_resolving || m_tunePending) && !g_start_running.load()) ? 1 : 0);
'''


def start_hooks(work, replace, player):
    host = player + "core/src/screens/ProviderHostScreen.cpp"
    if "tube_afterglow_take_cancel" not in (work / host).read_text(encoding="utf-8"):
        replace(host, "            startSelected();\n        }\n        if (!tube_afterglow_take_exit()) return true;\n",
                "            startSelected();\n        }\n" + CANCEL + "        if (!tube_afterglow_take_exit()) return true;\n")
        replace(host, "    if (!m_opened) return;\n    const bool kit_drew = evo_rmlui_provider_render(framebuffer, width, height) != 0;\n",
                "    if (!m_opened) return;\n" + BUSY + "    const bool kit_drew = evo_rmlui_provider_render(framebuffer, width, height) != 0;\n")
    screen = player + "ui_rml/src/evo_rmlui_provider.cpp"
    if "tube_afterglow_status" not in (work / screen).read_text(encoding="utf-8"):
        replace(screen, '#include "evo_hui.h"\n', '#include "evo_hui.h"\n#include "tube_afterglow.h"\n')
        replace(screen, "void evo_rmlui_provider_set_status(const char *status, int error)\n{\n",
                "void evo_rmlui_provider_set_status(const char *status, int error)\n{\n"
                "#if defined(EVO_AGC_DEVICE) /* the header above is included for the console only */\n"
                "    tube_afterglow_status(status, error); /* ProsperoTube Afterglow: the reason a chosen video did not start */\n"
                "#endif\n")


# ---- package H: the player (docs/afterglow/ARCHITECTURE.md, section 12b) ----------------------------
# The player stays EVO's PlayerScreen with ProsperoTube's overlays: playback, the pad and every menu
# are untouched. While the Interface preference is Afterglow, what EVO hands its kit application for
# the player (the controls, the caption picker, the list of the options, the dialog, the toast) is also
# kept by native/afterglow/player/app/mount.cpp, which draws it instead of EVO's own kit screens. With
# the preference on Classic none of the lines added here runs.

PLAYER_FOCUS = '''    /* package H: while the Afterglow player drew its controls or a sheet, its own snapshot is the report. */
    if (k.state == Kit::up && tube_afterglow_enabled())
    {
        const char *player_report = afterglow::player_snapshot(k.frame_serial);
        if (player_report[0] != '\\0')
        {
            if (screen_doc)
                *screen_doc = "afterglow_player";
            return player_report;
        }
    }
'''

PLAYER_LIST = '''    /* package H: the player's own list (options, their choices, related videos) is a sheet over the video. */
    if (tube_afterglow_enabled() && afterglow::player_list_is_sheet())
        return k.render(
            Screen::list, width, height,
            [&](float dt, hui::gfx::DrawList &list, const Context &) {
                k.list.update(dt);
                afterglow::player_draw_list(list, k.fonts, dt, k.frame_serial);
            },
            false);
'''

# The Afterglow frame drew this frame's screen: a dialog or toast over it belongs to the browser.
PLAYER_PAGE = "k.idle_frames == 0 && k.drawn_last_frame == Screen::provider && tube_afterglow_shown() != 0"


def player_mount(work, replace, player):
    makefile = (work / player / "Makefile").read_text(encoding="utf-8")
    if "ui_kit/src/afterglow/player" not in makefile:
        # Stage S runs first and ends the list of sources with its own line.
        sources = "                   $(wildcard ui_kit/src/afterglow/sound/*.cpp) $(wildcard ui_kit/src/afterglow/sound/app/*.cpp)\n"
        replace(player + "Makefile", sources,
                sources + "    # package H: the player's controls in the Afterglow design.\n"
                "    UI_KIT_SRCS += $(wildcard ui_kit/src/afterglow/player/*.cpp) $(wildcard ui_kit/src/afterglow/player/app/*.cpp)\n")
    app = player + "ui_kit/src/evo_hui_app.cpp"
    if "afterglow::player_" in (work / app).read_text(encoding="utf-8"):
        return
    replace(app, '#include "afterglow/app/mount.hpp"\n', '#include "afterglow/app/mount.hpp"\n#include "afterglow/player/app/mount.hpp"\n')
    # What EVO says is kept for the Afterglow player as well.
    for member, keep in (("osd.set(*params)", "player_set_playback(*params)"), ("subtitles.set(*params)", "player_set_captions_picker(*params)"),
                         ("list.set(*params)", "player_set_list(*params)"), ("dialog.set(*params)", "player_set_dialog(*params)"),
                         ("toast.set(*params)", "player_set_toast(*params)")):
        replace(app, "    if (params)\n        kit()." + member + ";\n}",
                "    if (params)\n        kit()." + member + ";\n    if (params && tube_afterglow_enabled()) /* package H */\n        afterglow::" + keep + ";\n}")
    replace(app, "    if (hud)\n        kit().osd.set_hud(*hud);\n}",
            "    if (hud)\n        kit().osd.set_hud(*hud);\n    if (hud && tube_afterglow_enabled()) /* package H */\n        afterglow::player_set_details(*hud);\n}")
    # ... and drawn by it instead of EVO's own kit screen.
    replace(app, "            k.osd.update(dt);\n            k.osd.draw(list, ctx);\n",
            "            k.osd.update(dt);\n            /* package H: the same state in the Afterglow design. */\n"
            "            if (tube_afterglow_enabled() && afterglow::player_draw_controls(list, k.fonts, *k.textures, dt, k.frame_serial))\n"
            "                return;\n            k.osd.draw(list, ctx);\n")
    replace(app, "            k.subtitles.update(dt);\n            k.subtitles.draw(list, ctx);\n",
            "            k.subtitles.update(dt);\n            /* package H */\n"
            "            if (tube_afterglow_enabled() && afterglow::player_draw_captions_picker(list, k.fonts, dt, k.frame_serial))\n"
            "                return;\n            k.subtitles.draw(list, ctx);\n")
    replace(app, "int evo_hui_render_list(int width, int height)\n{\n    Kit &k = kit();\n",
            "int evo_hui_render_list(int width, int height)\n{\n    Kit &k = kit();\n" + PLAYER_LIST)
    pad = " " * 32
    for part, draw in (("dialog", "player_draw_dialog"), ("toast", "player_draw_toast")):
        replace(app, pad + "k." + part + ".update(dt);\n" + pad + "k." + part + ".draw(list, ctx);\n",
                pad + "k." + part + ".update(dt);\n" + pad + "/* package H */\n"
                + pad + "if (tube_afterglow_enabled() && afterglow::" + draw + "(list, k.fonts, dt, k.frame_serial, " + PLAYER_PAGE + "))\n"
                + pad + "    return;\n" + pad + "k." + part + ".draw(list, ctx);\n")
    replace(app, "    if (k.state != Kit::up || k.drawn_last_frame == Screen::none)\n        return nullptr;\n    const char *doc = doc_name(k.drawn_last_frame);",
            PLAYER_FOCUS + "    if (k.state != Kit::up || k.drawn_last_frame == Screen::none)\n        return nullptr;\n    const char *doc = doc_name(k.drawn_last_frame);")
# ---- end of package H ---------------------------------------------------------------------------------


def stop_confirmation_focus(replace, player):
    """The real input focus follows Afterglow's safe primary stop action."""
    modal = player + "core/src/screens/ModalDialogScreen.cpp"
    replace(modal, '#include "evo_feedback.h"\n',
            '#include "evo_feedback.h"\n#if defined(EVO_AGC_DEVICE)\n#include "tube_afterglow.h"\n#endif\n')
    default = '    m_focusedButton = (m_type == ModalType::ExitConfirm) ? 1 : 0; // Default STOP (1) or RESUME (0)\n'
    replace(modal, default, default +
            '#if defined(EVO_AGC_DEVICE)\n'
            '    // ProsperoTube H11: Keep watching is the safe primary action in Afterglow.\n'
            '    if (m_type == ModalType::ExitConfirm && tube_afterglow_enabled())\n'
            '        m_focusedButton = 0;\n'
            '#endif\n')
