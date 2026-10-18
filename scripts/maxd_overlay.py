"""YouTube Max'd's changes, applied after all pinned ProsperoTube/EVO overlays.

Keep identity, storage, controller ownership and artwork together. This file
changes generated build inputs; pinned dependency checkouts stay unchanged.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import json
import hashlib
import io
import os
import shutil
import subprocess
import tarfile

NAME = "YouTube Max’d"


def apply(root, work, player):
    app = work / player
    # Rebrand visible copy after upstream's exact-match overlays have finished.
    # Keep source/credit URLs and copyright notices intact.
    for p in app.rglob("*"):
        if not p.is_file() or p.suffix not in {".c", ".cpp", ".h", ".hpp", ".rml", ".rcss"}:
            continue
        text = p.read_text()
        lines = []
        for line in text.splitlines(keepends=True):
            if "github.com/blackbearreloaded/ProsperoTube" not in line and "Copyright" not in line:
                line = line.replace("ProsperoTube", NAME)
                line = line.replace('"PROSPEROTUBE', '"YOUTUBE MAX’D').replace('>PROSPEROTUBE<', '>YOUTUBE MAX’D<')
            lines.append(line)
        changed = "".join(lines)
        changed = changed.replace("DUALSENSE LIGHT FOLLOWS THE THEME ACCENT", "MAX’D COLORS FOLLOW BROWSING, PLAYBACK AND PAUSE")
        if changed != text:
            p.write_text(changed)
    param = app / "sce_sys/param.json"
    meta = json.loads(param.read_text())
    for locale in meta["localizedParameters"].values():
        if isinstance(locale, dict) and "titleName" in locale:
            locale["titleName"] = NAME
    param.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n")
    # The fork keeps its own launcher art, and does not ship upstream's splash/music.
    shutil.copyfile(root / "native/branding/icon0.png", app / "sce_sys/icon0.png")
    for name in ("pic0.dds", "pic1.dds", "snd0.at9"):
        (app / "sce_sys" / name).unlink(missing_ok=True)
    for target in (app / "assets/rml", app / "assets/icons"):
        for name in ("tube-mark.png", "tube-mark-large.png"):
            shutil.copyfile(root / "native/branding/icon0.png", target / name)
    for name in ("brand.h", "lightbar.h", "maxd_feedback.h", "voice_gesture.h"):
        shutil.copyfile(root / "native/maxd" / name, app / "addons/include" / name)
    (app / "addons/include/maxd").mkdir(exist_ok=True)
    for name in ("auto_max.h", "auto_provider.h", "smart_rules.h", "smart_catalog.h"):
        shutil.copyfile(root / "native/maxd" / name, app / "addons/include/maxd" / name)
    shutil.copyfile(root / "native/maxd/lightbar.c", app / "addons/src/maxd_lightbar.c")
    shutil.copyfile(root / "native/maxd/evo_feedback.c", app / "ui/src/evo_feedback.c")
    make = app / "Makefile"
    text = make.read_text()
    anchor = "    addons/src/tube_provider.c"
    assert text.count(anchor) == 1
    make.write_text(text.replace(anchor, anchor + " \\\n    addons/src/maxd_lightbar.c"))
    provider = app / "addons/src/tube_provider.c"
    transforms = bool(os.environ.get("PROSPEROTUBE_TRANSFORM_ARTIFACT"))
    shader_info = {"mode": "transformed" if transforms else "evo-pinned",
                   "evo_commit": "9715f6ee9bf9b53461d6fdf9a2e11b4c48357783"}
    if not transforms:
        # Base shaders cover normal video/HDR/scaling. Never offer transform
        # controls or apply stored rotation bits without their matching shaders.
        text = provider.read_text()
        text = "".join(line for line in text.splitlines(keepends=True)
                       if 'CATALOG_FOLDER("settings:video-rotate"' not in line and
                          'CATALOG_FOLDER("settings:video-mirror"' not in line)
        old = "unsigned tube_player_video_geometry(void) {return atomic_load(&g_video_geometry);}"
        assert text.count(old) == 1
        text = text.replace(old, "unsigned tube_player_video_geometry(void) {return atomic_load(&g_video_geometry)&~((3u<<20)|(1u<<22));}")
        provider.write_text(text)
        picker = app / "core/src/screens/AudioTrackPickerScreen.cpp"
        text = picker.read_text()
        start = text.index("    for(int degrees=0;degrees<=270;degrees+=90) {")
        end = text.index("    // End " + NAME + " video geometry rows.", start)
        picker.write_text(text[:start] + text[end:])
        # Verify every shader header against its pinned source, not a cache.
        prefix = "projects/evoplayer/shaders/agc/"
        data = subprocess.check_output(["git", "-C", str(root / "third_party/evo"),
                                       "archive", shader_info["evo_commit"], prefix])
        headers = {}
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            for member in archive:
                if member.isfile() and member.name.endswith(".h"):
                    expected = archive.extractfile(member).read()
                    actual = work / member.name
                    assert actual.read_bytes() == expected, member.name
                    headers[member.name[len(prefix):]] = hashlib.sha256(expected).hexdigest()
        assert headers
        shader_info["headers"] = headers
    provider.write_text(provider.read_text() + '''
/* YouTube Max'd: main-thread light-bar state from foreground service work. */
#include "maxd_feedback.h"
int tube_maxd_ui_light_state(void) {
    if (g_auth.state == AUTH_STARTING || g_auth.state == AUTH_PENDING) return YM_LIGHT_SIGN_IN;
    for (int i = 0; i < REQUESTS; ++i)
        if (g_requests[i].used && g_requests[i].kind != 8 && g_requests[i].kind != 10)
            return YM_LIGHT_LOADING;
    return YM_LIGHT_MENU;
}
''')
    path = app / "core/src/Application.cpp"
    text = path.read_text()
    text = text.replace('#include "evo_feedback.h"', '#include "evo_feedback.h"\n#include "maxd_feedback.h"')
    anchor = "        m_screenManager->update(frameDeltaMs);"
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '''
        // YouTube Max'd: use the existing native pad and playback state.
        YmLightState maxd_light = static_cast<YmLightState>(tube_maxd_ui_light_state());
        if (m_playbackController) {
            const auto ps = m_playbackController->getPlaybackState();
            if (ps == PlaybackState::Opening || ps == PlaybackState::Seeking) maxd_light = YM_LIGHT_LOADING;
            else if (ps == PlaybackState::Playing) maxd_light = YM_LIGHT_PLAYING;
            else if (ps == PlaybackState::Paused || ps == PlaybackState::Scrubbing) maxd_light = YM_LIGHT_PAUSED;
            else if (ps == PlaybackState::Error) maxd_light = YM_LIGHT_ERROR;
        }
        if (tube_voice_state() == TUBE_VOICE_RECORDING) maxd_light = YM_LIGHT_SIGN_IN;
        else if (tube_voice_state() == TUBE_VOICE_TRANSCRIBING) maxd_light = YM_LIGHT_LOADING;
        evo_feedback_set_pad(m_padHandle);
        ym_feedback_state(maxd_light, !m_softClose, static_cast<uint64_t>(now_ms()));
        evo_feedback_tick(static_cast<uint64_t>(now_ms()));
''')
    text = text.replace('    evo_boot_log("shutdown: begin");', '    ym_feedback_shutdown();\n    evo_boot_log("shutdown: begin");')
    text = text.replace('        if (m_softClose) {', '        if (m_softClose) {\n            tube_voice_shutdown();\n            ym_feedback_shutdown();', 1)
    path.write_text(text)
    import maxd_features_overlay
    maxd_features_overlay.apply(root, work, player)
    import local_voice_overlay
    local_voice_overlay.apply(root, work, player, os.environ.get("TUBE_EMBEDDED_SERVICE", "0") == "1")
    # Include fork-specific instructions and provenance in every installable build.
    dest = app / "assets/prosperotube-notices"
    (dest / "MAXD-SHADERS.json").write_text(json.dumps(shader_info, indent=2) + "\n")
    for name in ("MAXD_CHANGES.md", "UPSTREAM.json"):
        shutil.copyfile(root / name, dest / name)
    shutil.copyfile(root / "README.md", dest / "YOUTUBE-MAXD-README.md")
    for name in ("GOOGLE_LOGIN.md", "MAXD_VALIDATION.md", "VOICE_SEARCH.md", "MAXD_FEATURES.md"):
        shutil.copyfile(root / "docs" / name, dest / name)
    native_licenses = root / "runtime-sources/licenses"
    if native_licenses.is_dir():
        shutil.copytree(native_licenses, dest / "native-libraries", dirs_exist_ok=True)
        shutil.copyfile(root / "runtime-sources/SOURCES.json", dest / "NATIVE-SOURCES.json")
