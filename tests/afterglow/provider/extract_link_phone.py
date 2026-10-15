"""Extract the real LinkPhone data/update path for the native provider integration test."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
source = (root / "native/afterglow/screens/link_phone.cpp").read_text()


def function(signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


# Rendering and input animation are irrelevant here; keep the complete catalog/action/poll path.
update = function("    void update(")
update = update[update.index("        // The catalog,"):update.index("        const int decided")]
structs = source[source.index("struct Device"):source.index("class LinkPhone")]
header = r'''
#pragma once
#include "afterglow/data.hpp"
namespace pairing_test {
using namespace afterglow;
namespace audio { enum class Cue { notify, error }; }
enum class ToastKind { success, error };
struct UpdateContext {
    float dt;
    struct Feedback { void play(audio::Cue, float = 1, float = 0, float = 1) {} } feedback;
    struct Nav { void toast(const std::string &, ToastKind) {} } nav;
};
const char *tr(const char *words) { return words; }
bool pending(const Page &page) { return page.load == model::Load::loading; }
const model::Row *find_row(const Page &page, const char *id) {
    for (const auto &row : page.rows) if (row.id == id) return &row;
    return nullptr;
}
constexpr float kPoll = 2.0f;
constexpr const char *kOptionRows[] = {"settings:remote:disconnect", "settings:remote:history", "settings:remote:reset"};
''' + structs + r'''
struct LinkPhoneProbe {
    Remote remote_;
    bool known_ = false;
    unsigned ticket_ = 0;
    float poll_in_ = 0, code_age_ = 0;
    model::Load load_ = model::Load::idle;
    std::string acted_, error_;
    void update(Data &data, float dt) {
        UpdateContext context{dt, {}, {}};
''' + update + "\n    }\n" + "\n".join(function(x) for x in [
    "    void take(", "    const std::string &state()", "    void act("])
header += "\n};\n} // namespace pairing_test\n"
Path(sys.argv[1]).write_text(header)
