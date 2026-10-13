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


def apply(root, work, replace, player):
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
