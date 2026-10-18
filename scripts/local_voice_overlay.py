"""Stage the pinned CPU recognizer and model in the generated native app."""
import shutil
from bootstrap_local_voice import CACHE, ASSETS, REVISION, MODEL_SHA, MODEL_SIZE, sha, header


def apply(root, work, player, enabled):
    app = work / player
    stamp = work / ".maxd-local-voice"
    mode = "local" if enabled else "companion"
    if not stamp.exists() or stamp.read_text() != mode:
        for suffix in (".o", ".d"):
            (app / ("addons/src/tube_voice" + suffix)).unlink(missing_ok=True)
    if not enabled:
        for suffix in (".cpp", ".o", ".d"):
            (app / ("addons/src/maxd_local_asr" + suffix)).unlink(missing_ok=True)
        shutil.rmtree(app / "assets/voice", ignore_errors=True)
        stamp.write_text(mode)
        return
    model = ASSETS / "ggml-tiny.bin"
    if not model.exists() or model.stat().st_size != MODEL_SIZE or sha(model) != MODEL_SHA:
        raise RuntimeError("Run scripts/bootstrap_local_voice.py before preparing local voice")
    source = CACHE / ("whisper.cpp-" + REVISION)
    includes = app / "addons/voice-include"
    includes.mkdir(exist_ok=True)
    for folder in (source / "include", source / "ggml/include"):
        for item in folder.glob("*.h"):
            shutil.copyfile(item, includes / item.name)
    libraries = app / "addons/voice-lib"
    libraries.mkdir(exist_ok=True)
    for name in ("libwhisper.a", "libggml.a", "libggml-cpu.a", "libggml-base.a"):
        found = list((CACHE / "ps5").rglob(name))
        if len(found) != 1:
            raise RuntimeError("Build the PS5 voice libraries first: " + name)
        shutil.copyfile(found[0], libraries / name)
    shutil.copyfile(root / "native/maxd/local_asr.cpp", app / "addons/src/maxd_local_asr.cpp")
    shutil.copyfile(root / "native/maxd/local_asr.h", app / "addons/include/local_asr.h")
    header(app / "addons/include/maxd_voice_model.h")
    shutil.copytree(ASSETS, app / "assets/voice", dirs_exist_ok=True)
    make = app / "Makefile"
    text = make.read_text()
    anchor = "CORE_OBJS := $(CORE_SRCS:.cpp=.o)"
    assert text.count(anchor) == 1
    text = text.replace(anchor, "CORE_SRCS += addons/src/maxd_local_asr.cpp\n" + anchor)
    text += "\naddons/src/tube_voice.o: CFLAGS += -DTUBE_LOCAL_VOICE=1\n"
    text += "addons/src/maxd_local_asr.o: CXXFLAGS += -Iaddons/voice-include\n"
    make.write_text(text)
    package = work / "scripts/package-app.sh"
    text = package.read_text()
    anchor = "ARCHIVE_GROUP=()"
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '''
if [[ "${MODE}" == "player" ]]; then
    for voice_lib in libwhisper libggml libggml-cpu libggml-base; do
        voice_archive="${EVO}/addons/voice-lib/${voice_lib}.a"
        need_file "${voice_archive}" "run scripts/build_local_voice.py --ps5"
        ARCHIVE_GROUP+=("${voice_archive}")
    done
fi
''')
    text = text.replace("--exclude-libs=ALL --error-limit=0", '--exclude-libs=ALL --error-limit=0 --why-extract="${BUILD}/voice-extract.tsv"')
    package.write_text(text)
    stamp.write_text(mode)
