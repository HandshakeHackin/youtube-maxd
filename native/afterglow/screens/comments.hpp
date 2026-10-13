// ProsperoTube Afterglow - the comments of a video: what the class holds (comments.cpp, comments_draw.cpp).
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The thread beside the video it is about. route.argument is the video's row id
// (a catalog id "discussion:top:<id>" or "discussion:newest:<id>" is taken
// too and names the order to start with). The screen reads, through the data
// layer:
//
//   discussion:top:<id>          the thread, most liked first, page after page;
//   discussion:newest:<id>       ... newest first. The first row of the first
//                                page is the way to write ("comment:post:<id>",
//                                with the number of comments) or, where the
//                                profile cannot, a notice with that number
//   discussion:replies:<cursor>  the replies of a comment: its own row id
//   details:<id>                 the video: picture, title, channel
//   Data::menu(<comment row>)    what can be done with a comment: the rows that
//                                rate it and the row that answers it
//
// Private to the comments screen: no other screen includes this file.

#pragma once

#include "afterglow/screen.hpp"

#include "ui/components/keyboard.hpp"
#include "ui/components/progress.hpp"
#include "ui/components/skeleton.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace afterglow::comments_page
{

// ---- the approved board, in virtual pixels ---------------------------------------
constexpr float kThreadTop = 200.0f;   // where the first comment starts
constexpr float kThreadView = 772.0f;  // five comments at rest
constexpr float kPitch = 152.0f;       // a comment of up to two lines
constexpr float kThreadW = 1016.0f;    // the plate under the comment in focus
constexpr float kIndent = 64.0f;       // a reply
constexpr float kTextLine = 31.0f;
constexpr int kLinesAtRest = 2;
constexpr int kLinesInFocus = 8;       // the comment in focus opens this far
constexpr Rect kThumb{1228.0f, 200.0f, 596.0f, 335.0f};
constexpr Rect kCompose{1228.0f, 716.0f, 596.0f, 68.0f};
constexpr Rect kSheet{kLeft - 28.0f, 588.0f, 1704.0f, 560.0f}; // writing: rises from the foot
constexpr Rect kField{kLeft + 14.0f, 772.0f, 760.0f, 76.0f};
constexpr Rect kKeys{1004.0f, 640.0f, 780.0f, 340.0f};
constexpr int kTextLimit = 500; // characters of a comment written here
// REQUEST: theme.hpp has kRadiusThumb, kRadiusPanel and kRadiusChip only (as video.hpp).
constexpr float kRadiusPlate = 24.0f;
constexpr float kRadiusPicture = 20.0f;
constexpr float kRadiusSheet = 40.0f;
constexpr float kRadiusField = 22.0f;
// REQUEST: theme.hpp's type scale stops at kTypeMeta (20) and kTypeBody (24) (as video.hpp).
constexpr float kTypeFine = 19.0f;   // an age, a count
constexpr float kTypeNote = 21.0f;   // a line under a title
constexpr float kTypeAuthor = 22.0f; // who wrote it; the header's second line
constexpr float kTypeSaid = 23.0f;   // what they wrote
constexpr float kTypeName = 25.0f;   // the video's title beside the thread

// ---- small helpers (REQUEST: the same as video.hpp carries; a screen may not include another's) ----

inline bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// Puts values into a translated pattern: "{0}" and "{1}".
inline std::string fill(const char *pattern, std::string_view first, std::string_view second = {})
{
    std::string out;
    for (const char *at = pattern; *at != '\0'; ++at)
    {
        if (at[0] == '{' && (at[1] == '0' || at[1] == '1') && at[2] == '}')
        {
            out += at[1] == '0' ? first : second;
            at += 2;
        }
        else
        {
            out.push_back(*at);
        }
    }
    return out;
}

// What a hand-over does to a whole screen (the motion table's "deeper / back out" and "section change").
struct Motion
{
    float opacity = 1.0f;
    float scale = 1.0f;
    float dx = 0.0f;
};
inline Motion hand_over(const Phase &phase, bool reduced)
{
    Motion m;
    if (phase.out > 0.0f)
    {
        m.opacity = 1.0f - tween::smoothstep(phase.out * 2.4f);
        if (!reduced && phase.move == Move::fade)
            m.scale = 1.0f - 0.025f * tween::cubic_out(phase.out);
        if (!reduced && phase.move == Move::push)
            m.dx = -90.0f * tween::cubic_in(phase.out);
        if (!reduced && phase.move == Move::pop)
            m.dx = 90.0f * tween::cubic_in(phase.out);
    }
    else if (phase.in < 1.0f)
    {
        m.opacity = tween::smoothstep((phase.in - 0.12f) / 0.6f);
        if (!reduced && phase.move == Move::push)
            m.dx = 110.0f * (1.0f - tween::quint_out(phase.in));
        if (!reduced && phase.move == Move::pop)
            m.dx = -110.0f * (1.0f - tween::quint_out(phase.in));
    }
    return m;
}

// ---- the thread ---------------------------------------------------------------------

struct Item
{
    enum class Kind : std::uint8_t
    {
        comment,
        waiting, // more of the thread (or a comment's replies) on its way
        note,    // more could not be loaded: the provider's words
    };
    Kind kind = Kind::comment;
    std::string id; // the row: "discussion:replies:<cursor>" when the comment has replies
    std::string author;
    std::string age;
    std::string text;
    std::string likes; // as YouTube words them
    std::string avatar;
    int replies = 0;
    bool pinned = false;
    bool owner = false;   // the channel wrote it
    bool hearted = false; // the channel loved it
    bool liked = false;   // by this profile
    bool written_here = false; // posted from this screen a moment ago: shown until the thread is read again
    int depth = 0;   // 1: a reply
    int parent = -1; // a reply's comment, as an index of the list
    int lines = 1;   // lines the text takes in full, up to kLinesInFocus
    float y = 0.0f;  // top, from the start of the thread, as laid out this frame
    float h = kPitch;

    bool focusable() const
    {
        return kind == Kind::comment;
    }
    bool has_replies() const
    {
        return replies > 0 && starts_with(id, "discussion:replies:");
    }
};

constexpr float kTailHeight = 76.0f; // a row that says more is coming, or why it is not

// What a row takes when nothing is in focus.
inline float rest_height(const Item &item)
{
    return item.kind == Item::Kind::comment ? kPitch : kTailHeight;
}

// Something asked of YouTube for a comment, through Data::act().
struct Job
{
    enum class Kind : std::uint8_t
    {
        none,
        like,
        unlike,
        post,  // a new comment
        reply,
    };
    enum class Stage : std::uint8_t
    {
        menu,   // reading what the comment offers
        acting, // a row is in flight
    };
    Kind kind = Kind::none;
    Stage stage = Stage::menu;
    std::string target; // the comment's row id; "" for a new comment
    std::string menu;   // Data::menu() of the comment
    std::string row;    // the row to act on first, when it is known without a menu
    unsigned ticket = 0;
    int steps = 0;
    bool applied = false; // the row that sends the change has been listed
};

class Comments final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::comments;
    }
    void enter(const Route &route, UpdateContext &context, bool restored) override;
    void update(const InputFrame &input, UpdateContext &context) override;
    void draw(const DrawContext &context, const Phase &phase) const override;
    std::string title() const override;
    std::span<const ui::Hint> hints() const override;
    FocusInfo focus() const override;
    std::vector<std::pair<std::string, std::string>> focusable() const override;
    void snapshot(Snapshot &out) const override;

  private:
    // ---- comments.cpp ----
    void read(UpdateContext &context);
    void read_video(UpdateContext &context);
    Item item_of(const model::Row &row, int depth, const ui::Fonts &fonts);
    void lay_out();
    void input_thread(const InputFrame &input, UpdateContext &context);
    void input_writing(const InputFrame &input, UpdateContext &context);
    void move(int direction, const InputFrame &input, UpdateContext &context);
    void set_order(int order, UpdateContext &context);
    void write(int target, UpdateContext &context);
    void submit(UpdateContext &context);
    void rate(UpdateContext &context);
    void advance_job(UpdateContext &context);
    void job_done(UpdateContext &context);
    void job_failed(const std::string &message, UpdateContext &context);
    void refuse(const InputFrame &input, UpdateContext &context);
    bool needs_account(UpdateContext &context);
    int focused() const; // the item in focus, -1 when the thread has none
    int first_shown() const;
    float full_height(const Item &item) const;
    Rect item_rect(int index) const; // on screen, as the springs have it now
    bool can_write() const
    {
        return profile_.signed_in && !post_row_.empty();
    }
    const std::string &catalog() const
    {
        return order_ == 0 ? top_id_ : newest_id_;
    }

    // ---- comments_draw.cpp ----
    float arrive(int index) const;
    void draw_thread(G &g, const DrawContext &context) const;
    void draw_comment(G &g, const DrawContext &context, const Item &item, const Rect &r, float focus) const;
    void draw_state(G &g) const;
    void bone(G &g, const Rect &r, float radius) const;
    void draw_about(G &g, const DrawContext &context) const;
    void draw_writing(G &g, const DrawContext &context, float amount) const;

    Route route_;
    std::string video_; // the video's row id
    std::string top_id_;
    std::string newest_id_;
    std::string details_id_;
    model::Profile profile_;

    // The thread.
    int order_ = 0; // 0 top, 1 newest
    int pages_[2] = {1, 1};
    std::vector<Item> items_;
    model::Load load_ = model::Load::idle;
    std::string error_;
    std::string count_;    // how many comments, as YouTube words it
    std::string post_row_; // the row that writes a new comment, "" when the profile cannot
    std::map<std::string, int> open_;   // comments whose replies are shown: pages of them asked for
    std::map<std::string, int> lines_;  // lines a text takes, by row id
    std::map<std::string, bool> rated_; // what YouTube confirmed here, over what the thread said
    std::vector<Item> echoes_;          // written here and confirmed: parent in Item::author's row via echo_parents_
    std::vector<std::string> echo_parents_;

    // The video beside it.
    std::string video_title_;
    std::string video_channel_;
    std::string video_thumb_;
    model::Load video_load_ = model::Load::idle;

    // Focus and scroll.
    std::string focus_id_; // the row in focus: kept when the list changes around it
    int focus_ = 0;
    tween::Spring focus_pos_;
    tween::Spring scroll_;
    bool placed_ = false;
    ui::Pulse nudge_;
    ui::Pulse like_pop_;
    std::string like_pop_id_;
    tween::Spring order_pos_;

    // Writing.
    bool writing_ = false;
    int writing_to_ = -1; // the comment answered, -1 for a new comment
    std::string writing_id_;
    std::string text_;
    std::string draft_for_; // whose answer text_ is: reopening the same one finds it again
    std::string trouble_;   // why the last attempt was not confirmed
    tween::Spring writing_amount_;
    ui::Keyboard keys_;

    // Requests.
    Job job_;
    std::shared_ptr<int> answer_ = std::make_shared<int>(0); // a dialog's: 0 none, 1 post, 2 sign in
    ui::Spinner spinner_;
    ui::Skeleton bones_; // the shimmer every bone of the screen shares
    int echo_serial_ = 0;

    float age_ = 0.0f;
    float clock_ = 0.0f;
};

} // namespace afterglow::comments_page
