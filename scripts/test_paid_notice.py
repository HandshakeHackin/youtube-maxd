"""Compile the generated shipping browser's successful-start notice hookup."""
import os
from pathlib import Path
import subprocess
import tempfile

work = Path(os.environ.get("PROSPEROTUBE_NATIVE_WORK", Path.home()/".cache/prosperotube/native"))
source = (work/"projects/evoplayer/core/src/screens/ProviderHostScreen.cpp").read_text()
a=source.index("            // ProsperoTube paid-content notice after successful start.")
b=source.index("            // End ProsperoTube paid-content notice.",a)
body=source[a:b]
scaffold=r'''#include <cassert>
#include <string>
static int consumed, shown;
static const char* next_notice;
static const char* tube_take_paid_content_notice() { ++consumed; auto value=next_notice; next_notice=nullptr; return value; }
static void toast(const char* heading, const char* message) {
    assert(std::string(heading)=="PAID PROMOTION" && std::string(message)=="Contains paid promotion"); ++shown;
}
struct Host { std::string m_providerId; void started() {
'''
ending=r'''
} };
int main() {
    Host host{"local"}; next_notice="Contains paid promotion";
    host.started(); assert(consumed==0 && shown==0);
    host.m_providerId="youtube"; host.started(); assert(consumed==1 && shown==1);
    host.started(); assert(consumed==2 && shown==1);
}
'''
with tempfile.TemporaryDirectory(prefix="tube-paid-notice-") as directory:
    path=Path(directory); (path/"test.cpp").write_text(scaffold+body+ending)
    subprocess.run(["g++","-std=c++17","-Wall","-Wextra","-Werror","-g","-O1",
        "-fsanitize=address,undefined","-fno-omit-frame-pointer",str(path/"test.cpp"),"-o",str(path/"test")],check=True)
    subprocess.run([str(path/"test")],check=True,env={**os.environ,"ASAN_OPTIONS":"detect_leaks=1"})
print("PASS: shipping successful-start notice toast, provider ownership and once-only consumption")
