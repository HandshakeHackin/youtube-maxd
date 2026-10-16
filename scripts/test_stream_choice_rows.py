"""The generated playback options list the video's qualities and audio tracks in both interfaces
and reopen the video through the provider when one is chosen (provider side: tests/test_stream_choice.c)."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
work = Path(subprocess.check_output(["python3", str(root / "scripts/prepare_native.py")], text=True).strip())
player = work / "projects/evoplayer"
picker = (player / "core/src/screens/AudioTrackPickerScreen.cpp").read_text()
header = (player / "core/include/evo/screens/AudioTrackPickerScreen.hpp").read_text()

def part(start, end):
    a = picker.index(start); return picker[a:picker.index(end, a)]

assert "KindTubeQuality, KindTubeAudio };" in header
rows = part("    // ProsperoTube stream choices (extracted by host tests).", "    // End ProsperoTube stream choices.")
refresh = part("void AudioTrackPickerScreen::refreshTracks() {", "\nvoid ")
# Rows come before EVO's own quality and audio rows; EVO's single audio row gives way to the tracks.
assert refresh.index("// ProsperoTube stream choices") < refresh.index("const auto variants = playback->getVideoVariants();")
assert "for (const auto& t : playback->getAudioTracks()) {\n        if (tubeAudio) break;" in refresh
for needed in ("tube_player_quality_count() > 1", 'automatic.label = "AUTO"', "tube_player_quality_label(i, label, sizeof label)",
               "tube_player_audio_count() > 1", "e.label = tube_player_audio_name(i);", '" - audio description"'):
    assert needed in rows, needed
selection = part("        // ProsperoTube stream choice selection (extracted by host tests).", "        // End ProsperoTube stream choice selection.")
for needed in ("tube_player_quality_select(e.streamIndex, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled)",
               "tube_player_audio_select(e.streamIndex, playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled)",
               "playback->stopPlayback();", "sm->returnFromPlayback();"):
    assert needed in selection, needed
# Afterglow groups them under Quality and Audio.
assert "case KindAuto: case KindVideo: case KindTubeQuality: return KindVideo;\n    case KindTubeAudio: return KindAudio;" in picker
print("PASS: generated quality and audio-track rows, reopen on selection, Afterglow groups")
