"""Regression for a provider start still inside a network resume seek at quit."""
from pathlib import Path
import importlib.util
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PLAYER = "projects/evoplayer/"
FILES = (
    "media/include/evo_stream_io.h",
    "media/src/evo_stream_io.c",
    "core/include/evo/screens/ProviderHostScreen.hpp",
    "core/src/screens/ProviderHostScreen.cpp",
    "core/src/Application.cpp",
    "core/src/services/PlaybackController.cpp",
)


def patched(temp, with_phone=False):
    for name in FILES:
        src = ROOT / "third_party/evo" / PLAYER / name
        dst = temp / PLAYER / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
    if with_phone:
        host = temp / PLAYER / "core/src/screens/ProviderHostScreen.cpp"
        source = host.read_text()
        source = source.replace("void ProviderHostScreen::onExit()\n{\n",
                                "void ProviderHostScreen::onExit()\n{\n"
                                "    const bool joined_phone_worker = g_phone_ticket && g_start_running;\n", 1)
        at_exit = source.index("void ProviderHostScreen::onExit()")
        head, source = source[:at_exit], source[at_exit:]
        source = source.replace("    m_tunePending = false;\n    m_tuneFrames = 0;",
                                "    m_tunePending = false;\n    m_tuneFrames = 0;\n"
                                "    if (!m_navigatingToPlayer && g_phone_ticket) {\n"
                                "        if (joined_phone_worker && g_start_success) {\n"
                                "            if (auto pb = Application::getInstance().getPlaybackController()) pb->stopPlayback();\n"
                                "        }\n"
                                "        tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0;\n"
                                "    }", 1)
        host.write_text(head + source)
    spec = importlib.util.spec_from_file_location(
        "close_pending_start_overlay", ROOT / "scripts/close_pending_start_overlay.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    def replace(name, old, new, count=1):
        path = temp / name
        source = path.read_text()
        if source.count(old) != count:
            raise AssertionError(f"{name}: expected {count} anchors, got {source.count(old)}")
        path.write_text(source.replace(old, new))

    module.apply(replace, PLAYER, temp)
    return temp / PLAYER


class ClosePendingStartTest(unittest.TestCase):
    def test_close_cancels_and_joins_before_media_release(self):
        with tempfile.TemporaryDirectory() as folder:
            player = patched(Path(folder))
            app = (player / "core/src/Application.cpp").read_text()
            start = app.index("if (m_softCloseFrames == 0)")
            section = app[start:app.index('evo_boot_log("soft close: media released")', start)]
            self.assertLess(section.index("provider->cancelPendingStart()"),
                            section.index("m_playbackController->stopPlayback()"))
            host = (player / "core/src/screens/ProviderHostScreen.cpp").read_text()
            start = host.index("void ProviderHostScreen::cancelPendingStart()")
            section = host[start:host.index("void ProviderHostScreen::onExit()", start)]
            self.assertLess(section.index("evo_stream_io_cancel_pending_start()"),
                            section.index("pthread_join(g_start_thread"))
            self.assertLess(section.index("pthread_join(g_start_thread"),
                            section.index("evo_stream_io_reset_start_cancel()"))
            on_exit = host[host.index("void ProviderHostScreen::onExit()"):host.index("bool ProviderHostScreen::handleInput", host.index("void ProviderHostScreen::onExit()"))]
            self.assertIn("cancelPendingStart();", on_exit)
            self.assertIn("if (hadPendingStart && !m_navigatingToPlayer)", on_exit)
            self.assertLess(on_exit.index("const bool hadPendingStart = g_start_running"),
                            on_exit.index("cancelPendingStart();"))
            self.assertLess(on_exit.index("cancelPendingStart();"),
                            on_exit.index("pb->stopPlayback();"))
            controller = (player / "core/src/services/PlaybackController.cpp").read_text()
            start = controller.index("const int resumeSeekRc = seekPrepared")
            section = controller[start:controller.index("if (resumeSeekRc >= 0)", start)]
            self.assertLess(section.index("evo_stream_io_set_deadline(m_streamIo, 0.0)"),
                            section.index("return false;"))
            self.assertIn("evo_stream_io_start_cancelled()", section)

    def test_phone_pending_start_stops_once_after_join(self):
        with tempfile.TemporaryDirectory() as folder:
            player = patched(Path(folder), with_phone=True)
            host = (player / "core/src/screens/ProviderHostScreen.cpp").read_text()
            on_exit = host[host.index("void ProviderHostScreen::onExit()"):host.index("bool ProviderHostScreen::handleInput", host.index("void ProviderHostScreen::onExit()"))]
            self.assertNotIn("joined_phone_worker", on_exit)
            self.assertEqual(on_exit.count("pb->stopPlayback();"), 1)
            self.assertLess(on_exit.index("cancelPendingStart();"), on_exit.index("pb->stopPlayback();"))
            self.assertIn("tube_phone_launch_finish(g_phone_ticket, 0)", on_exit)
    def test_actual_cancel_functions_release_a_blocked_start_before_free(self):
        with tempfile.TemporaryDirectory() as folder:
            player = patched(Path(folder))
            code = (player / "media/src/evo_stream_io.c").read_text()
            block = re.search(
                r"void evo_stream_io_reset_start_cancel\(void\).*?"
                r"(?=void evo_stream_io_abort)", code, re.S)
            self.assertIsNotNone(block)
            source = """
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
static int sio_start_cancelled;
""" + block.group(0).replace(
                " /* atomic operations below; checked by FFmpeg's interrupt callback */", ""
            ) + """
static int worker_exited;
static void *open_and_seek(void *unused) {
    (void)unused;
    while (!evo_stream_io_start_cancelled()) usleep(1000);
    __atomic_store_n(&worker_exited, 1, __ATOMIC_RELEASE);
    return 0;
}
int main(void) {
    pthread_t worker;
    evo_stream_io_reset_start_cancel();
    assert(pthread_create(&worker, 0, open_and_seek, 0) == 0);
    usleep(10000);
    evo_stream_io_cancel_pending_start();
    assert(pthread_join(worker, 0) == 0);
    assert(__atomic_load_n(&worker_exited, __ATOMIC_ACQUIRE));
    evo_stream_io_reset_start_cancel();
    assert(!evo_stream_io_start_cancelled()); /* a later Classic/direct open must proceed */
    return 0;
}
"""
            path = Path(folder) / "overlap.c"
            exe = Path(folder) / "overlap"
            path.write_text(source)
            subprocess.run(["cc", "-std=c11", "-D_DEFAULT_SOURCE", "-pthread",
                            str(path), "-o", str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
