"""Apply a checked overlay to a generated copy of pinned EVO, never its checkout."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys
import tarfile

ROOT = Path(__file__).resolve().parents[1]
EVO = ROOT / "third_party/evo"
PIN = "9715f6ee9bf9b53461d6fdf9a2e11b4c48357783"
TITLE = "PPSA99142"
WORK = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", str(Path.home() / ".cache/prosperotube/native")))


def replace(path, old, new, count=1):
    file = WORK / path
    source = file.read_text()
    if old == new:
        return
    actual = source.count(old)
    if actual == 0 and new and new in source:
        return
    if actual != count:
        raise RuntimeError(f"Overlay mismatch: {path}: expected {count} matches, found {actual}")
    file.write_text(source.replace(old, new))


def main():
    transform_artifact = os.environ.get("PROSPEROTUBE_TRANSFORM_ARTIFACT")
    if transform_artifact:
        from install_video_transform_shaders import validate
        validate(transform_artifact)  # Reject bad inputs before touching native outputs.
    actual = subprocess.check_output(["git", "-C", str(EVO), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN:
        raise RuntimeError(f"EVO pin mismatch: {actual}")
    stamp = WORK / ".prosperotube-source"
    if not stamp.exists():
        if WORK.exists() and any(WORK.iterdir()):
            raise RuntimeError("Native work directory must be empty or an existing ProsperoTube build.")
        WORK.mkdir(parents=True, exist_ok=True)
        archive = ROOT / ".deps/evo-source.tar"
        archive.parent.mkdir(exist_ok=True)
        subprocess.run(["git", "-C", str(EVO), "archive", "--format=tar", "-o", str(archive), PIN], check=True)
        with tarfile.open(archive) as tar:
            # Python's data filter rejects escaping symlinks and archive paths.
            tar.extractall(WORK, filter="data")
        ui = EVO / "third_party/ps5-homebrew-ui"
        subprocess.run(["git", "-C", str(ui), "archive", "--format=tar", "-o", str(archive), "HEAD"], check=True)
        with tarfile.open(archive) as tar:
            tar.extractall(WORK / "third_party/ps5-homebrew-ui", filter="data")
        stamp.write_text(PIN)
    elif stamp.read_text().strip() != PIN:
        raise RuntimeError("Native work directory has a different upstream pin.")
    transform_stamp = WORK / ".prosperotube-video-transform.json"
    transform_stamp.unlink(missing_ok=True)  # Only successful preparation can publish this stamp.
    player = "projects/evoplayer/"
    overlay_files = [player + name for name in (
        "Makefile", "addons/src/evo_provider_mgr.c", "addons/include/evo_provider.h", "src/evo_data_path.c", "include/evo_data_path.h",
        "core/src/Application.cpp", "core/include/evo/Application.hpp", "core/src/screens/ProviderHostScreen.cpp",
        "core/src/services/PlaybackController.cpp", "core/src/screens/PlayerScreen.cpp", "core/src/screens/SettingsScreen.cpp", "core/src/screens/AboutSupportScreen.cpp",
        "core/include/evo/screens/PlayerScreen.hpp", "core/include/evo/screens/ProviderHostScreen.hpp",
        "core/src/screens/AudioTrackPickerScreen.cpp", "core/include/evo/screens/AudioTrackPickerScreen.hpp",
        "core/src/screens/SubtitlePickerScreen.cpp", "core/include/evo/screens/SubtitlePickerScreen.hpp",
        "core/include/evo/interfaces/IPlaybackController.hpp", "core/include/evo/services/PlaybackController.hpp",
        "ui_kit/src/evo_hui_osd.cpp", "ui_kit/src/evo_hui_modals.cpp", "ui_rml/src/evo_rmlui_app.cpp",
        "assets/rml/playback.rml", "assets/rml/playback.rcss",
        "ui_rml/src/evo_rmlui_provider.cpp", "ui/src/evo_nav.c", "src/evo_boot_log.c", "src/evo_usb_remote.c",
        "ui_rml/include/evo_rmlui_provider.h", "ui_rml/src/evo_rmlui_provider_art.cpp",
        "ui/src/evo_keyboard.c",
        "media/src/evo_subtitle.c", "media/include/evo_subtitle.h",
        "media/src/evo_audio_out.c", "media/include/evo_audio_out.h", "media/src/evo_demux.c", "media/include/evo_demux.h",
        "media/src/evo_playback.c", "media/include/evo_playback.h",
        "media/src/evo_vdec_ffmpeg.c", "media/src/evo_vdec_native.c", "media/src/evo_vdec_native.h", "media/include/evo_vdec.h",
        "pp/src/pp_playback.c", "pp/include/pp_playback.h", "media/src/evo_agc_runtime.c", "media/include/evo_agc_runtime.h",
        "pp/src/pp_clock.c", "pp/include/pp_clock.h",
        "include/evo_boot_trace.h", "sce_sys/param.json", "VERSION")]
    overlay_files += ["scripts/build-ffmpeg.sh", "scripts/package-app.sh", player+"shaders/agc"]
    # Restore all shader headers too: disabling a transform build must never
    # leave 48-byte shaders paired with the original 16/32-byte runtime.
    # Always start each overlay from its pinned sources. Build outputs remain
    # intact, and edits to generated files are never the project's source.
    archive = ROOT / ".deps/evo-overlay-base.tar"
    subprocess.run(["git", "-C", str(EVO), "archive", "--format=tar", "-o", str(archive), PIN, *overlay_files], check=True)
    with tarfile.open(archive) as tar:
        tar.extractall(WORK, filter="data")
    for name in ("tube_provider.c", "tube_provider.h", "tube_sponsor.h", "tube_profile_settings.h", "tube_sponsor_draw.h", "tube_subtitles.h", "tube_caption_apply.h", "tube_audio_drain.h", "tube_seek_timeline.h", "tube_audio_seek.h", "tube_audio_tempo.c", "tube_audio_tempo.h", "tube_remote.c", "tube_remote.h", "tube_phone.c", "tube_phone.h", "tube_voice.c", "tube_voice.h", "tube_microphone.c", "tube_microphone.h", "tube_search_query.h", "tube_channel_name.h", "tube_name_case_data.h", "tube_volume.h", "tube_random.h", "tube_time_display.h", "tube_video_geometry.h", "tube_video_transform.h"):
        target = "addons/src" if name.endswith(".c") else "addons/include"
        shutil.copyfile(ROOT / "native" / name, WORK / player / target / name)
    shutil.copytree(ROOT / "native/elevation", WORK / player / "src/elevation", dirs_exist_ok=True)
    for name in ("cJSON.c", "cJSON.h"):
        target = "addons/src" if name.endswith(".c") else "addons/include"
        shutil.copyfile(ROOT / "native/vendor/cJSON" / name, WORK / player / target / name)
    for name in ("youtube.rml", "youtube.rcss"):
        shutil.copyfile(ROOT / "native" / name, WORK / player / "assets/rml" / name)
    for name in ("icon0.png", "pic0.png", "pic1.png"):
        shutil.copyfile(ROOT / "native/branding" / name, WORK / player / "sce_sys" / name)
    notices = WORK / player / "assets/prosperotube-notices"
    notices.mkdir(exist_ok=True)
    for name in ("LICENSE", "THIRD_PARTY_NOTICES.md"):
        shutil.copyfile(ROOT / name, notices / name)
    shutil.copyfile(ROOT / "native/vendor/cJSON/LICENSE", notices / "cJSON-MIT.txt")
    shutil.copyfile(ROOT / "native/licenses/Unicode-3.0.txt", notices / "Unicode-3.0.txt")
    shutil.copyfile(EVO / "COPYRIGHT.md", notices / "EVO-COPYRIGHT.md")
    replace(player + "Makefile", "    addons/src/provider_stremio.c", "    addons/src/provider_stremio.c \\\n    addons/src/tube_provider.c \\\n    addons/src/tube_remote.c \\\n    addons/src/tube_phone.c \\\n    addons/src/tube_voice.c \\\n    addons/src/tube_microphone.c")
    replace(player + "addons/src/evo_provider_mgr.c", "extern const evo_provider_t evo_provider_iptv;", "extern const evo_provider_t evo_provider_youtube;\nextern const evo_provider_t evo_provider_iptv;")
    replace(player + "addons/src/evo_provider_mgr.c", "    &evo_provider_iptv,\n    &evo_provider_xtream,\n    &evo_provider_emby,\n    &evo_provider_jellyfin,     /* native screens; the web UI as an extra */\n    &evo_provider_stremio,      /* Stremio addons, native (provider_stremio.c) */\n    &evo_provider_nuvio,        /* web UI only - Stremio addons, debrid */", "    &evo_provider_youtube,")
    replace(player + "src/evo_data_path.c", '"/data/evoplayer"', '"/data/prosperotube"', count=2)
    replace(player + "include/evo_data_path.h", '#define EVO_DATA_DIR "/download0/evoplayer"', '#define EVO_DATA_DIR "/data/prosperotube"')
    replace(player + "include/evo_data_path.h", '#define EVO_DATA_DIR "/data/evoplayer"', '#define EVO_DATA_DIR "/data/prosperotube"')
    replace(player + "src/evo_data_path.c", '''    if (!evo_jailbreak_is_open())
        return EVO_DATA_DIR;              /* "/download0/evoplayer" - transient */
''', "")
    replace(player + "Makefile", "    src/evo_webui.c \\\n    src/evo_jailbreak.c", "    src/evo_webui.c")
    # EVO packaging gathers every .o in the generated tree, including stale
    # objects no longer in the Makefile. Remove only the replaced implementation.
    for suffix in (".o", ".d"):
        (WORK / player / ("src/evo_jailbreak" + suffix)).unlink(missing_ok=True)
    replace(player + "Makefile", "CORE_SRCS := \\\n", "CORE_SRCS := \\\n    src/elevation/elevation.cpp \\\n    src/elevation/tube_jailbreak.cpp \\\n")
    replace(player + "core/src/Application.cpp", '#include "evo/screens/ProviderHostScreen.hpp"', '#include "evo/screens/ProviderHostScreen.hpp"\n#include "tube_provider.h"')
    replace(player + "core/src/Application.cpp", "    evo_jailbreak_self();", '''    if (!evo_jailbreak_self()) {
        evo_bt("ProsperoTube requires writable /data/prosperotube; startup stopped");
        return false;
    }''')
    from startup_overlay import apply as apply_startup_overlay
    apply_startup_overlay(WORK, replace, player)
    replace(player + "core/src/Application.cpp", 'std::snprintf(path, sizeof(path), "/mnt/usb0/evo_shot_%03d.bmp", slot);',
            'std::snprintf(path, sizeof(path), "%s/evo_shot_%03d.bmp", evo_data_dir(), slot);')
    replace(player + "core/src/Application.cpp", '''    if (!fp) {
        /* USB is the nice place for these (the user can pull them off without
         * FTP), but it is not always writable. Fall back to the data root
         * rather than just failing. */
        std::snprintf(path, sizeof(path), "%s/evo_shot_%03d.bmp", evo_data_dir(), slot);
        fp = std::fopen(path, "wb");
    }
''', "")
    replace(player + "core/src/Application.cpp", "        evo_net_poll();", "        evo_net_poll();\n        tube_poll();")
    replace(player + "core/src/Application.cpp", "    m_screenManager->navigateTo(ScreenId::MainMenu);", '    ProviderHostScreen::setPendingProvider("youtube");\n    m_screenManager->navigateTo(ScreenId::EmbyBrowse);')
    host = player + "core/src/screens/ProviderHostScreen.cpp"
    replace(host, '#include "evo_provider.h"', '#include "evo_provider.h"\n#include "tube_provider.h"')
    # Preserve the provider when returning from playback: pending is one-shot.
    replace(host, "    std::string want = g_pendingProvider;", "    std::string want = g_pendingProvider;\n    g_pendingProvider.clear();")
    # Resolve completions carry a generation; cancelling a request must not
    # let its callback replace a later selection's state.
    replace(host, "} // namespace\n\nProviderHostScreen::ProviderHostScreen()", "void on_tube_resolved(int ok, const evo_stream_choice_t* choices, int count, void* ud)\n{\n    if ((unsigned)(uintptr_t)ud == g_pick_gen)\n        on_resolved(ok, choices, count, &g_pending);\n}\n\n} // namespace\n\nProviderHostScreen::ProviderHostScreen()")
    replace(host, "on_resolved, &g_pending", "on_tube_resolved, (void*)(uintptr_t)g_pick_gen")
    # YouTube's selected quality plays immediately by default. EVO's global
    # live-TV chooser and PICK capability must not force a prompt for every video.
    replace(host, "    g_pending.picked = 0;", '    if (!std::strcmp(sel.provider_id, "youtube"))\n        g_pending.ask = tube_choose_stream_enabled();\n    g_pending.picked = 0;')
    replace(host, "    if (pressed & PadButtons::Triangle) {\n        if (!evo_rmlui_provider_get_focused_is_folder())", "    if (m_providerId == \"youtube\" && !evo_rmlui_provider_get_focused_is_folder()) {\n        const char* id = evo_rmlui_provider_get_focused_id();\n        int state = -2; const char* name = \"\";\n        if (pressed & PadButtons::Triangle) { state = tube_toggle_favorite(id); name = \"FAVORITE\"; }\n        if (pressed & PadButtons::R2) { state = tube_toggle_queue(id); name = \"QUEUE\"; }\n        if (pressed & PadButtons::L2) { state = tube_toggle_subscription(id); name = \"FOLLOW CHANNEL\"; }\n        if (state != -2) {\n            toast(name, state < 0 ? \"This item does not have the required metadata\" : state ? \"Added locally on this PS5\" : \"Removed\");\n            evo_rmlui_provider_reload(); return true;\n        }\n    }\n\n    if (pressed & PadButtons::Triangle) {\n        if (!evo_rmlui_provider_get_focused_is_folder())")
    replace(host, "    if (pressed & PadButtons::Options) {\n        evo_feedback(EVO_FB_OPEN);\n        const evo_provider_t* op", "    if (pressed & PadButtons::Options) {\n        if (m_providerId == \"youtube\") {\n            evo_keyboard_open(\"Companion LAN address#pairing code\", evo_provider_youtube.get_source(), 255,\n                [](const char* text, void*) {\n                    if (evo_provider_youtube.set_source(text) == 0) {\n                        evo_provider_set_enabled(\"youtube\", 1);\n                        evo_rmlui_provider_reload();\n                        toast(\"COMPANION\", \"Address saved. Search or select a video to pair.\");\n                    } else toast(\"COMPANION\", \"Use http://PC-IP:8765#123456 with the companion code\");\n                }, nullptr);\n            return true;\n        }\n        evo_feedback(EVO_FB_OPEN);\n        const evo_provider_t* op")
    replace(host, 'evo_keyboard_open("Search channels..."', 'evo_keyboard_open("Search YouTube videos..."')
    replace(host, "    if (m_resolving || g_start_running) return;", "    if (m_providerId == \"youtube\" && (!std::strcmp(sel.item_id, \"app:settings\") || !std::strcmp(sel.item_id, \"app:exit\"))) {\n        if (auto sm = Application::getInstance().getScreenManager())\n            if (!std::strcmp(sel.item_id, \"app:settings\")) sm->navigateTo(ScreenId::Settings);\n            else Application::getInstance().requestSoftClose();\n        return;\n    }\n    if (m_resolving || g_start_running) return;")
    replace(host, "        if (sm) { sm->setRailFocused(true); return true; }", "        if (m_providerId == \"youtube\") return true;\n        if (sm) { sm->setRailFocused(true); return true; }")
    replace(host, "    if (pressed & PadButtons::Circle) {\n        if (m_resolving || m_tunePending || g_start_running) return true;", "    if (pressed & PadButtons::Circle) {\n        if (m_providerId == \"youtube\" && (m_resolving || m_tunePending) && !g_start_running) {\n            ++g_pick_gen; tube_cancel(); m_resolving = false; m_tunePending = false;\n            evo_rmlui_provider_set_tuning(0); evo_rmlui_provider_set_loading(0, \"\");\n            evo_rmlui_provider_set_status(\"Cancelled. Choose a video to retry.\", 0); return true;\n        }\n        if (m_resolving || m_tunePending || g_start_running) return true;")
    replace(host, "    if (!evo_rmlui_provider_take_selection(&sel)) return;", "    evo_provider_item_t queued;\n    if (tube_take_next(&queued)) {\n        std::memset(&sel, 0, sizeof sel);\n        std::snprintf(sel.provider_id, sizeof sel.provider_id, \"youtube\");\n        std::snprintf(sel.item_id, sizeof sel.item_id, \"%s\", queued.id);\n        std::snprintf(sel.title, sizeof sel.title, \"%s\", queued.title);\n        sel.is_live = queued.is_live; sel.resume_sec = queued.resume_pos_sec;\n    } else if (!evo_rmlui_provider_take_selection(&sel)) return;")
    replace(host, "    if (!kit_drew)\n        evo_rmlui_render_nav_overlay(framebuffer, width, height);", "    if (!kit_drew && m_providerId != \"youtube\")\n        evo_rmlui_render_nav_overlay(framebuffer, width, height);")
    # Native caption sidecar is complete before the playback worker starts.
    path = player + "core/src/services/PlaybackController.cpp"
    replace(path, '#include "evo_provider.h"', '#include "evo_provider.h"\n#include "tube_provider.h"\n#include "tube_subtitles.h"')
    replace(path, "    prospero_subtitle_load_for_media(filePath.c_str());", "    const int tube_cues = prospero_subtitle_load_for_media(m_source.provider == \"youtube\" && *tube_caption_media_path()\n        ? tube_caption_media_path() : filePath.c_str());")
    replace(path, "    prospero_secondary_subtitle_open(play_fmt);   // #110: only if one was chosen for this file", "    prospero_secondary_subtitle_open(play_fmt);   // #110: only if one was chosen for this file\n    if (m_source.provider == \"youtube\" && tube_subtitle_reopen(tube_cues) < 0) toast(\"SECONDARY CAPTIONS\", \"Could not restore this language\");")
    replace(path, "void PlaybackController::jumpChapter(int direction) {", "void PlaybackController::jumpChapter(int direction) {\n    if (m_source.provider == \"youtube\") {\n        double target = tube_chapter_target(getPositionSeconds(), direction);\n        if (target >= 0) { seekTo(clampScrubTarget(target)); return; }\n    }")
    path = player + "core/src/screens/PlayerScreen.cpp"
    replace(path, '#include "evo/screens/PlayerScreen.hpp"', '#include "evo/screens/PlayerScreen.hpp"\n#include "tube_provider.h"')
    replace(path, "    const bool ended = (evo_pb_is_eof() != 0);", "    const bool ended = (evo_pb_is_eof() != 0) &&\n        tube_playback_at_end(playback->getPositionSeconds(), playback->getDurationSeconds());\n    if (ended && tube_finish_and_next(playback->getPositionSeconds(), playback->getDurationSeconds())) {\n        playback->stopPlayback();\n        if (auto sm = Application::getInstance().getScreenManager()) sm->returnFromPlayback();\n        return;\n    }\n    if (!ended && !playback->isPaused() && !playback->isScrubbing()) {\n        double target = tube_skip_target(playback->getPositionSeconds());\n        if (target >= 0 && target < playback->getDurationSeconds()) {\n            playback->seekTo(target); toast(\"SPONSORBLOCK\", \"Skipped an optional segment\");\n        }\n    }")
    path = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(path, "if ((m_bundle_version == \"embedded\" || m_using_fallback) && RenderKit(width, height))", "if (m_provider_id != \"youtube\" && (m_bundle_version == \"embedded\" || m_using_fallback) && RenderKit(width, height))")
    replace(player + "src/evo_boot_log.c", "    while (*p && o + 1 < sizeof out) {", "    while (*p && o + 1 < sizeof out) {\n        if (is_url && strncmp(p, \"/media/\", 7) == 0) {\n            const char* end = p + 7;\n            while (*end && *end != '/' && *end != ' ' && *end != '\\n') ++end;\n            if (o + 10 >= sizeof out) break;\n            memcpy(out + o, \"/media/***\", 10); o += 10; p = end;\n            continue;\n        }")
    replace(player + "include/evo_boot_trace.h", "    va_end(ap);", "    va_end(ap);\n    evo_log_redact(r.msg, sizeof r.msg);")
    # Actions change settings in place rather than growing the folder stack.
    replace(path, "    if (hit->is_folder) {\n        m_pos_stack.push_back", "    if (m_provider_id == \"youtube\" && id.rfind(\"settings:\", 0) == 0 &&\n        id != \"settings:languages\" && id != \"settings:audio-languages\") {\n        m_page = 0; RequestPage(id.c_str(), 0); return;\n    }\n    if (hit->is_folder && !(m_provider_id == \"youtube\" && id.rfind(\"app:\", 0) == 0)) {\n        m_pos_stack.push_back")
    # Returning to home goes to the tube browser, never EVO's multi-source tiles.
    replace(player + "ui/src/evo_nav.c", '{ "HOME",      "Back to the launch screen",          EVO_IC_HOME,     EVO_SCREEN_LAUNCH     }', '{ "HOME",      "YouTube videos",                      EVO_IC_HOME,     EVO_SCREEN_EMBY_BROWSE }')
    replace(player + "ui/src/evo_nav.c", '{ "BROWSE",    "Videos and folders on USB storage",  EVO_IC_USB,      EVO_SCREEN_BROWSER    }', '{ "LIBRARY",   "Local YouTube library",                EVO_IC_USB,      EVO_SCREEN_EMBY_BROWSE }')
    replace(player + "ui/src/evo_nav.c", '{ "EMBY",      "Emby and media server streaming",    EVO_IC_EMBY,     EVO_SCREEN_EMBY_SETUP }', '{ "YOUTUBE",   "Search and watch YouTube",              EVO_IC_EMBY,     EVO_SCREEN_EMBY_BROWSE }')
    replace(player + "core/src/screens/SettingsScreen.cpp", "screenMgr->navigateTo(ScreenId::MainMenu);", "screenMgr->navigateTo(ScreenId::EmbyBrowse);")
    about = player + "core/src/screens/AboutSupportScreen.cpp"
    replace(about, 'params.app_name = "EVO PLAYER PRO";', 'params.app_name = "PROSPEROTUBE";')
    replace(about, 'params.tagline = "CINEMATIC MEDIA PLAYER FOR PLAYSTATION 5 HOMEBREW";', 'params.tagline = "NATIVE YOUTUBE CLIENT - EVO / PROSPEROPLAYER FOUNDATION";')
    replace(about, 'params.themes_info = "PRESS CROSS FOR CHANGELOG, SQUARE FOR DEV TOOLS";', 'params.themes_info = "GPL-3.0-OR-LATER - LAN COMPANION REQUIRED - HARDWARE ACCEPTANCE PENDING";')
    replace(about, "ScreenId::Changelog", "ScreenId::EmbyBrowse")
    replace(about, "ScreenId::MainMenu", "ScreenId::EmbyBrowse")
    replace(host, "        if (sm) sm->navigateTo(ScreenId::MainMenu);\n        return true;", "        if (m_providerId == \"youtube\") {\n            evo_rmlui_provider_show_setup(); return true;\n        }\n        if (sm) sm->navigateTo(ScreenId::MainMenu);\n        return true;")
    from artwork_overlay import apply_artwork_overlay
    apply_artwork_overlay(replace, player)
    from discussion_overlay import apply_discussion_overlay
    apply_discussion_overlay(replace, player, host)
    from device_auth_overlay import apply_device_auth_overlay
    apply_device_auth_overlay(replace, player)
    from phone_overlay import apply_phone_overlay
    apply_phone_overlay(replace, player)
    from live_chat_overlay import apply_live_chat_overlay
    apply_live_chat_overlay(replace, player)
    from chat_compose_overlay import apply_chat_compose_overlay
    apply_chat_compose_overlay(replace, player, host)
    from controller_overlay import apply_controller_overlay
    apply_controller_overlay(replace, player)
    from audio_drain_overlay import apply_audio_drain_overlay
    apply_audio_drain_overlay(replace, player)
    from video_drain_overlay import apply_video_drain_overlay
    apply_video_drain_overlay(replace, player)
    from playback_recovery_overlay import apply_playback_recovery_overlay
    apply_playback_recovery_overlay(replace, player, host)
    from seek_overlay import apply_seek_overlay
    apply_seek_overlay(replace, player)
    from accurate_resume_overlay import apply_accurate_resume_overlay
    apply_accurate_resume_overlay(replace, player, WORK)
    from sponsor_overlay import apply_sponsor_overlay
    apply_sponsor_overlay(replace, player)
    from sponsor_review_overlay import apply_sponsor_review_overlay
    apply_sponsor_review_overlay(replace, player)
    from playback_rate_overlay import apply_playback_rate_overlay, apply_audio_rate_overlay
    apply_playback_rate_overlay(replace, player)
    apply_audio_rate_overlay(replace, player, WORK)
    from speed_controls_overlay import apply_speed_controls_overlay
    apply_speed_controls_overlay(replace, player)
    from player_chat_overlay import apply_player_chat_overlay
    apply_player_chat_overlay(replace, player)
    from phone_player_overlay import apply_phone_player_overlay
    apply_phone_player_overlay(replace, player, host)
    from queue_failure_overlay import apply_queue_failure_overlay
    apply_queue_failure_overlay(replace, host)
    from phone_queue_failure_overlay import apply_phone_queue_failure_overlay
    apply_phone_queue_failure_overlay(replace, host)
    from voice_controls_overlay import apply_voice_controls_overlay
    apply_voice_controls_overlay(replace, player, host)
    from phone_exit_overlay import apply_phone_exit_overlay
    apply_phone_exit_overlay(replace, player)
    from sleep_timer_overlay import apply_sleep_timer_overlay
    apply_sleep_timer_overlay(replace, player)
    from quality_preferences_overlay import apply_quality_preferences_overlay
    apply_quality_preferences_overlay(replace, player)
    from player_controls_overlay import apply_player_controls_overlay
    apply_player_controls_overlay(replace, player)
    from seek_controls_overlay import apply_seek_controls_overlay
    apply_seek_controls_overlay(replace, player)
    from time_display_overlay import apply_time_display_overlay
    apply_time_display_overlay(replace, player)
    from video_geometry_overlay import apply_video_geometry_overlay
    apply_video_geometry_overlay(replace, player)
    transform_manifest = None
    if transform_artifact:
        from install_video_transform_shaders import install
        from video_transform_overlay import apply_video_transform_overlay
        transform_manifest = install(transform_artifact, WORK)
        apply_video_transform_overlay(replace, player)
    from caption_controls_overlay import apply_caption_controls_overlay
    apply_caption_controls_overlay(replace, player)
    from secondary_captions_overlay import apply_secondary_captions_overlay
    apply_secondary_captions_overlay(replace, player, WORK, ROOT)
    from playlist_context_overlay import apply_playlist_context_overlay
    apply_playlist_context_overlay(replace, player, host)
    from channel_block_ui_overlay import apply_channel_block_ui_overlay
    apply_channel_block_ui_overlay(replace, player)
    replace(player + "ui/src/evo_keyboard.c", "#define KB_MAX_BUF 256", "#define KB_MAX_BUF 1024")
    param_path = WORK / player / "sce_sys/param.json"
    param = json.loads(param_path.read_text())
    param.update(titleId=TITLE, conceptId="99142", contentId=f"UP9000-{TITLE}_00-PROSPEROTUBE0000", contentVersion="01.000.000")
    param["localizedParameters"]["en-US"]["titleName"] = "ProsperoTube"
    param_path.write_text(json.dumps(param, indent=2) + "\n")
    (WORK / player / "VERSION").write_text("0.1.0\n")
    revision = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "--short=8", "HEAD"], text=True).strip()
    # Dependency trees are regenerated from checked pins above, so their working
    # files do not contribute to this package. Avoid recursively scanning them.
    dirty = subprocess.run(["git", "-C", str(ROOT), "diff", "--ignore-submodules=all", "--quiet", "HEAD"], check=False).returncode
    fingerprint = "prosperotube-" + revision + ("-dirty" if dirty else "")
    replace("scripts/package-app.sh", 'BUILD_SHA="$(git -C "${EVO}" rev-parse --short=8 HEAD 2>/dev/null || echo unknown)"\n    git -C "${EVO}" diff --quiet -- "${EVO}" 2>/dev/null || BUILD_SHA="${BUILD_SHA}-dirty"', f'BUILD_SHA="{fingerprint}"')
    # Include helpers and notices before PFS and ZIP creation.
    replace("scripts/package-app.sh", 'begin "inspecting signed containers"', '''cp "${REPO_ROOT}/prosperotube-lapy/lapy.elf" "${APPDIR}/lapy.elf"
cp "${REPO_ROOT}/prosperotube-lapy/lapy-manifest.json" "${APPDIR}/lapy-manifest.json"
cp "${REPO_ROOT}/prosperotube-lapy/Lapy-MIT.txt" "${APPDIR}/assets/prosperotube-notices/Lapy-MIT.txt"
begin "inspecting signed containers"''')
    # Direct diagnostic/data paths must be independent too, including upstream
    # code reached by inherited settings and caption tools.
    for file in (WORK / player).rglob("*"):
        if file.suffix not in (".c", ".cpp", ".h", ".hpp", ".rml", ".rcss"):
            continue
        source = file.read_text()
        updated = source.replace("/data/evoplayer", "/data/prosperotube").replace("/download0/evoplayer", "/data/prosperotube")
        updated = updated.replace("/mnt/usb0/evo_compatibility_report.txt", "/data/prosperotube/compatibility_report.txt")
        updated = updated.replace("/mnt/usb0/evo_%s_%s.bmp", "/data/prosperotube/evo_%s_%s.bmp")
        updated = updated.replace("/mnt/usb0/evo.log", "/data/prosperotube/console.log").replace("QUIT EVO", "QUIT PROSPEROTUBE")
        updated = updated.replace("/mnt/usb0/evo_cmd", "/data/prosperotube/dev_command")
        updated = updated.replace("/mnt/usb0/evo_status", "/data/prosperotube/dev_status")
        updated = updated.replace("/mnt/usb0/evo_ui.json", "/data/prosperotube/dev_ui.json")
        if source != updated:
            file.write_text(updated)
    # App modules can remain sandboxed. Resolve diagnostic paths through EVO's
    # existing data-root contract, just like the provider's persistent state.
    log = player + "src/evo_boot_log.c"
    replace(log, '#include "evo_boot_log.h"', '#include "evo_boot_log.h"\n#include "evo_data_path.h"\n#include "evo_jailbreak.h"')
    # Mounted sandbox paths can accept writes before Lapy succeeds. Opening the
    # log starts both a writer and a server, violating the helper's single-thread
    # precondition. Keep breadcrumbs in RAM/klog until the complete storage proof.
    replace(log, 'void evo_boot_log_flush(void)\n{', '''void evo_boot_log_flush(void)
{
    if (!evo_jailbreak_is_open())
        return;''')
    replace(log, '#define EVO_LOG_PATH "/data/prosperotube/console.log"', '#define EVO_LOG_PATH evo_data_path("console.log")')
    replace(log, 'EVO_LOG_PATH ".1"', 'evo_data_path("console.log.1")')
    remote = player + "src/evo_usb_remote.c"
    replace(remote, '#include "evo_usb_remote.h"', '#include "evo_usb_remote.h"\n#include "evo_data_path.h"')
    replace(remote, '#define CMD_PATH    "/data/prosperotube/dev_command"', '''/* Separate buffers preserve both rename arguments; evo_data_path itself
 * returns one thread-local buffer. Resolve again if the sandbox root changes. */
static const char* tube_dev_path(unsigned index) {
    static char paths[5][512];
    static const char* leaves[] = {"dev_command", "dev_status", "dev_ui.json", "dev_ui.json.tmp", "dev_status.tmp"};
    snprintf(paths[index], sizeof paths[index], "%s/%s", evo_data_dir(), leaves[index]);
    return paths[index];
}
#define CMD_PATH tube_dev_path(0)''')
    replace(remote, '#define STATUS_PATH "/data/prosperotube/dev_status"', '#define STATUS_PATH tube_dev_path(1)\n#define STATUS_TMP_PATH tube_dev_path(4)')
    replace(remote, '#define UI_PATH     "/data/prosperotube/dev_ui.json"', '#define UI_PATH tube_dev_path(2)')
    replace(remote, '#define UI_TMP_PATH "/data/prosperotube/dev_ui.json.tmp"', '#define UI_TMP_PATH tube_dev_path(3)')
    # Publish a complete status snapshot, including the GPU-park checkpoint.
    # A reader must retain the prior snapshot during writes and on I/O failure.
    replace(remote, '    FILE *sf = fopen(STATUS_PATH, "w");', '    FILE *sf = fopen(STATUS_TMP_PATH, "w");')
    replace(remote, '    fprintf(sf,', '    const int written = fprintf(sf,')
    replace(remote, '    fclose(sf);', '''    const int closed = fclose(sf);
    if (written < 0 || closed != 0) {
        remove(STATUS_TMP_PATH);
        return;
    }
    if (rename(STATUS_TMP_PATH, STATUS_PATH) != 0)
        remove(STATUS_TMP_PATH);''')
    # Own build trees live on Linux; leave the installed SDK untouched.
    replace("scripts/build-ffmpeg.sh", 'BUILD_DIR="/build/ffmpeg/${FFMPEG_VERSION}-${PROFILE}"', 'BUILD_DIR="${REPO_ROOT}/build/ffmpeg/${FFMPEG_VERSION}-${PROFILE}"')
    replace("scripts/build-ffmpeg.sh", 'STAGE_DIR="/build/ffmpeg/stage-${FFMPEG_VERSION}-${PROFILE}"', 'STAGE_DIR="${REPO_ROOT}/build/ffmpeg/stage-${FFMPEG_VERSION}-${PROFILE}"')
    shutil.copyfile(ROOT / "scripts/patch_ffmpeg_dash_seek.py", WORK / "scripts/patch_ffmpeg_dash_seek.py")
    replace("scripts/build-ffmpeg.sh",
            'need_file "${SRC_DIR}/configure" "FFmpeg source tree looks wrong."',
            'need_file "${SRC_DIR}/configure" "FFmpeg source tree looks wrong."\n'
            'python3 "${REPO_ROOT}/scripts/patch_ffmpeg_dash_seek.py" "${SRC_DIR}/libavformat/dashdec.c"')
    replace(player + "Makefile", "    addons/src/tube_provider.c", "    addons/src/tube_provider.c \\\n    addons/src/tube_audio_tempo.c")
    replace(player + "Makefile", "    $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib/libavformat.a", "    $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib/libavfilter.a \\\n    $(PS5_PAYLOAD_SDK)/target/user/homebrew/lib/libavformat.a")
    replace("scripts/package-app.sh", "             libavformat libavcodec", "             libavfilter libavformat libavcodec")
    replace("scripts/build-ffmpeg.sh", "        --enable-filter=anull", "        --enable-filter=anull\n        --enable-filter=atempo")
    # Assert integration in generated inputs, rather than trusting patch counts:
    # a replacement can contain text that also exists elsewhere in the source.
    makefile = (WORK / player / "Makefile").read_text()
    if "src/evo_jailbreak.c" in makefile or "src/elevation/tube_jailbreak.cpp" not in makefile:
        raise RuntimeError("Generated app must link only the bounded Lapy bridge")
    for name in ("src/evo_data_path.c", "include/evo_data_path.h"):
        if "/download0/" in (WORK / player / name).read_text():
            raise RuntimeError("Transient app storage remains in " + name)
    app = (WORK / player / "core/src/Application.cpp").read_text()
    if '"/mnt/usb0/evo_shot_' in app or '"/mnt/usb0/evo_%s_%s.bmp"' in app:
        raise RuntimeError("Screenshot output must use ProsperoTube storage")
    if transform_manifest is None:
        transform_stamp.unlink(missing_ok=True)
    else:
        transform_stamp.write_text(json.dumps(transform_manifest, indent=2, sort_keys=True)+"\n")
    print(WORK)


if __name__ == "__main__":
    main()
