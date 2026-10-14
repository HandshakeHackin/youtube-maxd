"""Exercise the mounted player dialog's actual snapshot gate on the host."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'native/afterglow/player/app/mount.cpp').read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


code = r'''
#include <cassert>
#include <memory>
#include <string>
namespace hui { namespace gfx { struct DrawList {}; } namespace ui { struct Fonts {}; } }
namespace afterglow {
namespace player {
struct Dialog { bool shown = false; };
struct Player {
    static constexpr unsigned kDialog = 1;
    std::string snapshot_json() const { return "dialog/0"; }
    void draw(hui::gfx::DrawList &, hui::gfx::DrawList &, int, unsigned) {}
};
}
struct Mount {
    std::unique_ptr<player::Player> player = std::make_unique<player::Player>();
    player::Dialog dialog{true};
    unsigned controls = 0x80000000u, listed = 0x80000000u;
    unsigned picked = 0x80000000u, asked = 0x80000000u;
    unsigned asked_player = 0x80000000u;
    std::string snapshot;
};
Mount &mount() { static Mount m; return m; }
player::Player &step(Mount &m, const hui::ui::Fonts &, float, unsigned, bool) { return *m.player; }
void finish(Mount &) {}
'''+function('bool recent(unsigned stamp, unsigned frame)')+'\n'+function('bool player_draw_dialog(')+'\n'+function('const char *player_snapshot(')+r'''
}
int main() {
    auto &m = afterglow::mount();
    hui::gfx::DrawList list;
    hui::ui::Fonts fonts;
    assert(afterglow::player_draw_dialog(list, fonts, 0, 100, true));
    assert(std::string(afterglow::player_snapshot(100)).empty()); // Provider page dialog.
    assert(afterglow::player_draw_dialog(list, fonts, 0, 101, false));
    assert(std::string(afterglow::player_snapshot(101)) == "dialog/0"); // Player stop dialog.
    assert(std::string(afterglow::player_snapshot(102)) == "dialog/0"); // Next-frame observer.
    assert(std::string(afterglow::player_snapshot(103)).empty()); // Stale stamp expires.
    assert(afterglow::player_draw_dialog(list, fonts, 0, 104, false));
    assert(afterglow::player_draw_dialog(list, fonts, 0, 105, true));
    assert(std::string(afterglow::player_snapshot(105)).empty()); // Page cannot borrow old player stamp.
    m.controls = 106;
    assert(std::string(afterglow::player_snapshot(106)) == "dialog/0"); // Existing controls path.
}
'''
with tempfile.TemporaryDirectory(prefix='tube-afterglow-dialog-') as folder:
    binary = Path(folder) / 'test'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-x', 'c++', '-',
                    '-o', str(binary)], input=code, text=True, check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: mounted stop dialog snapshot follows the player and excludes provider-page dialogs')
