// Startup update offer and progress, using the normal Afterglow modal.
#pragma once
#include "tube_update.h"
#include <algorithm>
#include <string>
#include <vector>

namespace afterglow {
class UpdatePrompt {
    bool progress_ = false;
    bool notes_pending_ = false;
    bool offer_pending_ = false;
    std::vector<std::string> pages_;
    std::size_t page_ = 0;
public:
    template<class FrameType> void step(FrameType &frame) {
        tube_update_status status{};
        if (progress_) {
            if (!tube_update_app_progress(&status)) {
                frame.app_question_close();
                progress_ = false;
            } else {
                std::string body = status.phase == TUBE_UPDATE_DOWNLOADING ? tr("Downloading") : tr("Checking and unpacking");
                if (status.total) body += ": " + std::to_string(status.done * 100 / status.total) + "%";
                frame.app_question_body(body + ". " + tr("ProsperoTube will close when the update is ready. Wait for the update notification before reopening it."));
            }
            return;
        }
        if (!frame.background_return_allowed()) return;
        if (notes_pending_) {
            notes_pending_ = false;
            frame.app_question(Question{ui::StatusKind::question, tr("What's new"), pages_[page_],
                {page_ + 1 < pages_.size() ? tr("Next") : tr("Back")}}, [this](int answer) {
                if (answer == 0 && ++page_ < pages_.size()) notes_pending_ = true;
                else offer_pending_ = true;
            });
            return;
        }
        if (!tube_update_app_offer(&status) && !offer_pending_) return;
        offer_pending_ = false;
        tube_update_app_dismiss_offer();
        const bool allowed = !status.blocked && (status.check == TUBE_UPDATE_AVAILABLE || status.phase == TUBE_UPDATE_STAGED);
        std::string body = std::string(tr("Version")) + " " + (*status.latest ? status.latest : status.staged) + " " + tr("is available.");
        if (!allowed) body += " " + std::string(status.blocked ? tr(status.blocked_text) : tr("This release cannot be installed from this copy."));
        frame.app_question(Question{ui::StatusKind::question, tr("ProsperoTube update"), body,
            {allowed ? tr("Update now") : tr("Close"), tr("What's new"), tr("Skip")}, -1, 2},
            [this, &frame, allowed](int answer) {
                if (answer == 0 && allowed) {
                    tube_update_app_action("install");
                    tube_update_status state{};
                    if (tube_update_app_progress(&state)) {
                        progress_ = true;
                        frame.app_question(Question{ui::StatusKind::question, tr("Updating ProsperoTube"), tr("Preparing the update..."), {tr("Cancel")}},
                            [this](int) { tube_update_app_action("cancel"); progress_ = false; });
                    }
                } else if (answer == 1) {
                    std::string notes = tube_update_app_notes();
                    if (notes.empty()) notes = tr("No release notes were provided.");
                    pages_.clear();
                    while (!notes.empty()) {
                        std::size_t length = std::min<std::size_t>(600, notes.size());
                        while (length < notes.size() && length && (static_cast<unsigned char>(notes[length]) & 0xc0) == 0x80) --length;
                        pages_.push_back(notes.substr(0, length)); notes.erase(0, length);
                    }
                    page_ = 0; notes_pending_ = true;
                } else if (answer == 2) tube_update_app_action("skip");
            });
    }
};
}
