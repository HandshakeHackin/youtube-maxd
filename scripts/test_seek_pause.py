"""Exercise shipping controller seek overloads and mailbox pause ownership."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home() / ".cache/prosperotube/native"))
source = (work / "projects/evoplayer/core/src/services/PlaybackController.cpp").read_text()
start = source.index("void PlaybackController::seekTo(double targetSeconds) {")
end = source.index("void PlaybackController::jumpChapter", start)
body = source[start:end]
scaffold = r'''
#include <cassert>
#include <cmath>
#include <initializer_list>
static bool active=true, accepted=true;
static int requests, resumes, pause_flag;
static double requested;
static int g_pp_pb;
static int prospero_request_inplace_seek(double target,int paused) {
    ++requests; requested=target; pause_flag=paused; return accepted;
}
static void pp_playback_resume(int*) { ++resumes; }
static void evo_boot_log(const char*,...) {}
class PlaybackController {
public:
    bool isActive() { return active; }
    void seekTo(double target);
    bool seekTo(double target,bool keepPaused);
};
'''
checks = r'''
static void reset() { active=accepted=true; requests=resumes=0; pause_flag=-1; requested=-1; }
int main() {
    PlaybackController playback;
    reset(); playback.seekTo(12.25); assert(requests==1 && requested==12.25 && !pause_flag && !resumes);
    for (bool keepPaused:{false,true}) {
        reset(); assert(playback.seekTo(12.25,keepPaused));
        assert(requests==1 && requested==12.25 && pause_flag==int(keepPaused) && !resumes);
        reset(); accepted=false; assert(!playback.seekTo(12.25,keepPaused));
        assert(requests==1 && pause_flag==int(keepPaused) && resumes==int(!keepPaused));
        for (double target:{-1.0,double(NAN),double(INFINITY),1000001.0}) {
            reset(); assert(!playback.seekTo(target,keepPaused)); assert(!requests && !resumes);
        }
        reset(); active=false; assert(!playback.seekTo(12.25,keepPaused)); assert(!requests && !resumes);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="tube-seek-pause-") as directory:
    test = Path(directory) / "test.cpp"
    binary = Path(directory) / "test"
    test.write_text(scaffold + body + checks)
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: shipping seek mailbox preserves explicit pause on success/refusal; legacy scrub seeks resume and invalid targets never queue")
