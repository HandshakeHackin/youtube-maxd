#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <vector>
#include "tube_update.h"
namespace ui { enum class StatusKind { question }; }
namespace afterglow {
inline const char *tr(const char *value) { return value; }
struct Question { ui::StatusKind icon; std::string title, body; std::vector<std::string> answers; int danger = -1, safe = 0; };
}
#include "afterglow/app/update_prompt.hpp"
static tube_update_status status{};
static bool offer, progress;
static std::vector<std::string> actions;
extern "C" int tube_update_app_offer(tube_update_status *s) { *s = status; return offer; }
extern "C" int tube_update_app_progress(tube_update_status *s) { *s = status; return progress; }
extern "C" void tube_update_app_dismiss_offer() { offer = false; }
extern "C" const char *tube_update_app_notes() { return "Signed release notes"; }
extern "C" int tube_update_app_action(const char *action) { actions.push_back(action); progress = actions.back() == "install"; return 1; }
struct Frame {
    bool open = false;
    afterglow::Question question{};
    std::function<void(int)> callback;
    bool background_return_allowed() { return !open; }
    void app_question(afterglow::Question q, std::function<void(int)> cb) { open = true; question = std::move(q); callback = std::move(cb); }
    void app_question_body(std::string body) { question.body = std::move(body); }
    void app_question_close() { open = false; callback = nullptr; }
    void choose(int answer) { open = false; auto cb = std::move(callback); cb(answer); }
};
int main() {
    status.check = TUBE_UPDATE_AVAILABLE;
    afterglow::UpdatePrompt prompt; Frame frame;
    offer = true; frame.open = true; prompt.step(frame); assert(offer); // don't replace another modal
    frame.open = false; prompt.step(frame);
    assert(!offer && frame.open && frame.question.answers.size() == 3 && frame.question.safe == 2 && actions.empty());
    frame.choose(1); prompt.step(frame); assert(frame.question.body == "Signed release notes");
    frame.choose(0); prompt.step(frame); assert(frame.question.answers[0] == "Update now");
    frame.choose(0); assert(progress && frame.open && actions.back() == "install");
    status.phase = TUBE_UPDATE_DOWNLOADING; status.total = 100; status.done = 42;
    prompt.step(frame); assert(frame.question.body.find("42%") != std::string::npos);
    frame.choose(-1); assert(!progress && actions.back() == "cancel");
    offer = true; prompt.step(frame); frame.choose(2); assert(actions.back() == "skip");
    offer = true; status.blocked = 1; prompt.step(frame); assert(frame.question.answers[0] == "Close");
    auto count = actions.size(); frame.choose(0); assert(actions.size() == count);
    status.blocked = 0; offer = true; prompt.step(frame); frame.choose(0);
    progress = false; prompt.step(frame); assert(!frame.open); // failure/apply finishes modal
}
