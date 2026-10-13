// ProsperoTube Afterglow - the comments of a video: the focus, writing, rating, one frame.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the screen reads is in comments.cpp, the drawing in comments_draw.cpp.

#include "afterglow/screens/comments.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow::comments_page
{

namespace
{

int characters(std::string_view utf8)
{
    int count = 0;
    for (const char c : utf8)
        count += (static_cast<unsigned char>(c) & 0xc0) != 0x80 ? 1 : 0;
    return count;
}

void erase_last(std::string &utf8)
{
    while (!utf8.empty())
    {
        const unsigned char c = static_cast<unsigned char>(utf8.back());
        utf8.pop_back();
        if ((c & 0xc0) != 0x80)
            break;
    }
}

bool blank(std::string_view text)
{
    return text.find_first_not_of(' ') == std::string_view::npos;
}

const char *load_name(model::Load load)
{
    switch (load)
    {
    case model::Load::ready:
        return "ready";
    case model::Load::failed:
        return "failed";
    case model::Load::offline:
        return "offline";
    case model::Load::idle:
    case model::Load::loading:
        break;
    }
    return "loading";
}

const char *job_name(Job::Kind kind)
{
    static constexpr const char *kNames[] = {"", "like", "unlike", "post", "reply"};
    return kNames[static_cast<std::size_t>(kind)];
}

} // namespace

// ---- requests --------------------------------------------------------------------------

void Comments::refuse(const InputFrame &input, UpdateContext &context)
{
    if (input.nav_repeat)
        return; // a held direction stays silent
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    nudge_.trigger();
}

bool Comments::needs_account(UpdateContext &context)
{
    if (profile_.signed_in)
        return false;
    const std::weak_ptr<int> answer = answer_;
    context.nav.confirm(tr("Sign in to YouTube"), tr("Writing, answering and liking comments use a YouTube account. This profile has none yet."),
                        tr("Sign in"), [answer](bool yes)
                        {
                            if (const std::shared_ptr<int> held = answer.lock())
                                *held = yes ? 2 : 0;
                        });
    return true;
}

void Comments::rate(UpdateContext &context)
{
    const InputFrame press{};
    const int at = focused();
    if (at < 0 || items_[static_cast<std::size_t>(at)].written_here)
    {
        refuse(press, context);
        return;
    }
    if (needs_account(context))
        return;
    if (job_.kind != Job::Kind::none)
    {
        refuse(press, context); // one change at a time: the first is still on its way
        return;
    }
    const Item &item = items_[static_cast<std::size_t>(at)];
    Job job;
    job.kind = item.liked ? Job::Kind::unlike : Job::Kind::like;
    job.target = item.id;
    job.menu = context.services.data.menu(item.id, item.author);
    if (job.menu.empty())
    {
        refuse(press, context);
        context.nav.toast(tr("This comment cannot be rated from here."), ToastKind::info);
        return;
    }
    context.services.data.forget(job.menu); // what a comment offers goes stale in minutes: it is read now
    context.feedback.play(audio::Cue::select, 1.0f, -0.2f);
    job_ = std::move(job);
}

void Comments::submit(UpdateContext &context)
{
    const InputFrame press{};
    if (blank(text_))
    {
        refuse(press, context); // nothing to post
        return;
    }
    const std::weak_ptr<int> answer = answer_;
    context.nav.confirm(writing_to_ >= 0 ? tr("Post this reply?") : tr("Post this comment?"),
                        tr("It is posted to YouTube from your account, once. It cannot be changed from this console afterwards."), tr("Post"),
                        [answer](bool yes)
                        {
                            if (const std::shared_ptr<int> held = answer.lock())
                                *held = yes ? 1 : 0;
                        });
}

void Comments::advance_job(UpdateContext &context)
{
    Data &data = context.services.data;
    if (*answer_ == 2)
    {
        *answer_ = 0;
        context.nav.push(Route{ScreenId::accounts, {}, {}});
    }
    else if (*answer_ == 1)
    {
        // The viewer confirmed: the comment is sent, once.
        *answer_ = 0;
        if (writing_ && job_.kind == Job::Kind::none && !blank(text_))
        {
            Job job;
            job.kind = writing_to_ >= 0 ? Job::Kind::reply : Job::Kind::post;
            job.target = writing_id_;
            trouble_.clear();
            if (job.kind == Job::Kind::post)
            {
                // REQUEST: Data::act() lists a row and carries nothing; the text typed here has to
                // travel with "comment:post:<id>" and "comment:reply:<cursor>" (the provider takes
                // it from its text prompt today).
                job.row = post_row_;
                job.stage = Job::Stage::acting;
                job.ticket = data.act(job.row);
                job.steps = 1;
            }
            else
            {
                const Item *to = writing_to_ >= 0 ? &items_[static_cast<std::size_t>(writing_to_)] : nullptr;
                job.menu = to != nullptr ? data.menu(to->id, to->author) : std::string();
                if (job.menu.empty())
                {
                    job_failed(tr("This comment cannot be answered from here."), context);
                    return;
                }
                data.forget(job.menu);
            }
            job_ = std::move(job);
        }
    }
    if (job_.kind == Job::Kind::none)
        return;
    if (job_.stage == Job::Stage::menu)
    {
        const Page &menu = data.catalog(job_.menu);
        if (menu.load == model::Load::loading || menu.load == model::Load::idle)
            return;
        if (menu.load != model::Load::ready)
        {
            job_failed(menu.error, context);
            return;
        }
        const char *wanted = job_.kind == Job::Kind::like     ? "comment:confirm:like:"
                             : job_.kind == Job::Kind::unlike ? "comment:confirm:none:"
                                                              : "comment:reply:";
        std::string row;
        for (const model::Row &entry : menu.rows)
        {
            if (row.empty() && starts_with(entry.id, wanted))
                row = entry.id;
        }
        if (row.empty())
        {
            job_failed(job_.kind == Job::Kind::reply ? tr("YouTube does not take answers to this comment.")
                                                     : tr("YouTube does not take a rating for this comment."),
                       context);
            return;
        }
        job_.stage = Job::Stage::acting;
        job_.ticket = data.act(row);
        job_.steps = 1;
        return;
    }
    const Page &answer = data.action(job_.ticket);
    if (answer.load == model::Load::loading)
        return;
    if (answer.load != model::Load::ready)
    {
        job_failed(answer.error, context);
        return;
    }
    // A change is prepared, reviewed and then sent by the rows the provider answers with.
    std::string next;
    std::string words;
    for (const model::Row &row : answer.rows)
    {
        if (next.empty() && (starts_with(row.id, "comment:review:") || starts_with(row.id, "account:apply:")))
            next = row.id;
        else if (words.empty())
            words = row.title;
    }
    if (!next.empty() && !job_.applied && job_.steps < 4)
    {
        job_.applied = starts_with(next, "account:apply:");
        job_.ticket = data.act(next);
        ++job_.steps;
        return;
    }
    if (!job_.applied)
    {
        job_failed(words, context); // the provider said something else than "sent"
        return;
    }
    job_done(context);
}

void Comments::job_done(UpdateContext &context)
{
    const Job done = std::move(job_);
    job_ = Job{};
    if (done.kind == Job::Kind::like || done.kind == Job::Kind::unlike)
    {
        const bool on = done.kind == Job::Kind::like;
        rated_[done.target] = on;
        like_pop_id_ = done.target;
        if (on)
        {
            like_pop_.trigger();
            context.feedback.rumble(0.3f, 0.06f);
        }
        context.feedback.play(on ? audio::Cue::favorite_on : audio::Cue::favorite_off, on ? 1.12f : 1.0f, -0.2f);
        return;
    }
    // Posted. YouTube shows it to everybody after a while; here it stands in the thread at once.
    Item echo;
    echo.id = "written:" + std::to_string(++echo_serial_);
    echo.author = profile_.name.empty() ? std::string(tr("You")) : profile_.name;
    echo.age = tr("Just now");
    echo.text = text_;
    echo.written_here = true;
    echo.depth = done.kind == Job::Kind::reply ? 1 : 0;
    echo.lines = kLinesAtRest;
    echoes_.push_back(std::move(echo));
    echo_parents_.push_back(done.kind == Job::Kind::reply ? done.target : std::string());
    if (done.kind == Job::Kind::reply && open_.find(done.target) == open_.end())
        open_[done.target] = 1; // the answer is shown under its comment
    context.nav.toast(done.kind == Job::Kind::reply ? tr("Reply posted") : tr("Comment posted"), ToastKind::success);
    context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
    text_.clear();
    draft_for_.clear();
    trouble_.clear();
    writing_ = false;
}

void Comments::job_failed(const std::string &message, UpdateContext &context)
{
    const Job::Kind kind = job_.kind;
    job_ = Job{};
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    const std::string words = !message.empty() ? message
                              : kind == Job::Kind::like || kind == Job::Kind::unlike
                                    ? std::string(tr("The comment rating was not confirmed."))
                                    : std::string(tr("The comment was not confirmed. Reload the comments before writing it again."));
    if (writing_)
        trouble_ = words; // what was typed stays; nothing is sent a second time by itself
    else
        context.nav.toast(words, ToastKind::error);
}

// ---- input -----------------------------------------------------------------------------

void Comments::move(int direction, const InputFrame &input, UpdateContext &context)
{
    const int total = static_cast<int>(items_.size());
    int next = focused();
    if (next < 0)
    {
        refuse(input, context);
        return;
    }
    do
        next += direction;
    while (next >= 0 && next < total && !items_[static_cast<std::size_t>(next)].focusable());
    if (next < 0 || next >= total)
    {
        // While more comments are on their way the end is not the end.
        const bool waiting = direction > 0 && total > 0 && items_.back().kind == Item::Kind::waiting;
        if (!waiting)
            refuse(input, context);
        return;
    }
    focus_ = next;
    focus_id_ = items_[static_cast<std::size_t>(next)].id;
    context.feedback.play(audio::Cue::focus, 1.06f - 0.02f * static_cast<float>(std::min(next - first_shown(), 5)), -0.2f);
}

void Comments::set_order(int order, UpdateContext &context)
{
    if (order == order_)
        return;
    order_ = order;
    focus_ = 0;
    focus_id_.clear();
    placed_ = false; // the other thread starts at its top
    context.feedback.play(audio::Cue::tab, order == 0 ? 0.96f : 1.04f);
}

void Comments::write(int target, UpdateContext &context)
{
    const InputFrame press{};
    if (needs_account(context))
        return;
    if (target < 0 && post_row_.empty())
    {
        refuse(press, context);
        context.nav.toast(tr("Comments cannot be added to this video."), ToastKind::info);
        return;
    }
    if (target >= 0 && items_[static_cast<std::size_t>(target)].written_here)
    {
        refuse(press, context); // it is not on YouTube's list yet
        return;
    }
    writing_ = true;
    writing_to_ = target;
    writing_id_ = target >= 0 ? items_[static_cast<std::size_t>(target)].id : std::string();
    if (draft_for_ != writing_id_ || text_.empty())
    {
        text_.clear(); // another comment: another answer
        draft_for_ = writing_id_;
    }
    trouble_.clear();
    keys_.set_layout(0);
    keys_.set_focus(1, 0);
    keys_.set_length(characters(text_));
    keys_.enter();
    context.feedback.play(audio::Cue::modal_open);
}

void Comments::input_writing(const InputFrame &input, UpdateContext &context)
{
    if (job_.kind == Job::Kind::post || job_.kind == Job::Kind::reply)
        return; // sent: the answer decides what happens next
    if (pressed(input, Action::menu))
    {
        submit(context); // Options posts, as the Done key does
        return;
    }
    const ui::Event event = keys_.handle(input, context.feedback);
    for (int i = 0; i < keys_.erased(); ++i)
        erase_last(text_);
    text_ += keys_.typed();
    if (keys_.erased() > 0 || !keys_.typed().empty())
        trouble_.clear();
    keys_.set_length(characters(text_));
    if (event == ui::Event::activated)
    {
        submit(context);
    }
    else if (event == ui::Event::cancelled)
    {
        writing_ = false; // what was typed is kept for this comment
        context.feedback.play(audio::Cue::modal_close);
    }
}

void Comments::input_thread(const InputFrame &input, UpdateContext &context)
{
    if (pressed(input, Action::back))
    {
        context.nav.back();
        return;
    }
    if (pressed(input, Action::page_prev) || pressed(input, Action::page_next))
    {
        set_order(1 - order_, context);
        return;
    }
    const int at = focused();
    const bool broken = items_.empty() && (load_ == model::Load::failed || load_ == model::Load::offline);
    if (input.nav == Direction::up || input.nav == Direction::down)
    {
        move(input.nav == Direction::down ? 1 : -1, input, context);
    }
    else if (input.nav == Direction::left)
    {
        if (at >= 0 && items_[static_cast<std::size_t>(at)].depth > 0 && items_[static_cast<std::size_t>(at)].parent >= 0)
        {
            // Out of the replies, back to the comment they answer.
            focus_ = items_[static_cast<std::size_t>(at)].parent;
            focus_id_ = items_[static_cast<std::size_t>(focus_)].id;
            context.feedback.play(audio::Cue::focus, 1.0f, -0.3f);
        }
        else
        {
            context.nav.open_rail();
        }
    }
    else if (input.nav == Direction::right)
    {
        if (at >= 0 && items_[static_cast<std::size_t>(at)].has_replies())
        {
            const std::string &row = items_[static_cast<std::size_t>(at)].id;
            const auto open = open_.find(row);
            if (open == open_.end())
                open_[row] = 1;
            else
                open_.erase(open);
            context.feedback.play(audio::Cue::toggle, open_.count(row) != 0 ? 1.08f : 0.94f, -0.2f);
        }
        else
        {
            refuse(input, context);
        }
    }
    else if (pressed(input, Action::confirm))
    {
        if (broken)
        {
            context.feedback.play(audio::Cue::select);
            context.services.data.forget(catalog()); // asked again
            pages_[order_] = 1;
        }
        else
        {
            write(at, context); // an answer to the comment in focus; a new comment where there is none
        }
    }
    else if (pressed(input, Action::west))
    {
        if (broken)
            refuse(input, context);
        else
            write(-1, context);
    }
    else if (pressed(input, Action::north))
    {
        rate(context);
    }
}

// ---- one frame ---------------------------------------------------------------------------

void Comments::update(const InputFrame &input, UpdateContext &context)
{
    age_ += context.dt;
    clock_ = context.time;
    advance_job(context);
    read(context);
    // Pictures are asked for while updating, a frame before they are drawn.
    if (!video_thumb_.empty())
    {
        context.services.images.get(video_thumb_, PictureSize::ambient);
        context.services.images.get(video_thumb_, PictureSize::hero);
    }
    for (const Item &item : items_)
    {
        if (!item.avatar.empty())
            context.services.images.get(item.avatar, PictureSize::avatar);
    }
    if (context.top)
    {
        if (!video_thumb_.empty())
            context.nav.ambient(video_thumb_); // the page keeps the video's colour
        if (writing_)
            input_writing(input, context);
        else
            input_thread(input, context);
    }
    const float quick = context.reduced_motion ? 60.0f : 20.0f;
    const float soft = context.reduced_motion ? 60.0f : 13.0f;
    focus_pos_.target = static_cast<float>(std::max(focused(), 0));
    float rest = 0.0f; // where the first comment shown starts when everything is at rest
    const int first = first_shown();
    for (int i = 0; i < first; ++i)
        rest += rest_height(items_[static_cast<std::size_t>(i)]);
    scroll_.target = rest;
    if (!placed_)
    {
        focus_pos_.snap(focus_pos_.target);
        scroll_.snap(scroll_.target);
        placed_ = load_ == model::Load::ready;
    }
    focus_pos_.update(context.dt, quick);
    scroll_.update(context.dt, context.reduced_motion ? 60.0f : 14.0f);
    lay_out();
    order_pos_.target = static_cast<float>(order_);
    order_pos_.update(context.dt, quick);
    writing_amount_.target = writing_ ? 1.0f : 0.0f;
    writing_amount_.update(context.dt, soft);
    nudge_.update(context.dt, 9.0f);
    like_pop_.update(context.dt, 5.0f);
    keys_.update(context.dt);
    spinner_.update(context.dt);
    bones_.update(context.dt);
}

// ---- what the frame and the tests ask ---------------------------------------------------

std::span<const ui::Hint> Comments::hints() const
{
    static ui::Hint items[5];
    int count = 0;
    if (writing_ || writing_amount_.value > 0.5f)
        return {}; // the sheet carries its own, at its left
    const int at = focused();
    if (items_.empty() && (load_ == model::Load::failed || load_ == model::Load::offline))
    {
        items[count++] = {ui::Button::cross, tr("Try again")};
    }
    else if (at < 0)
    {
        if (load_ == model::Load::ready)
            items[count++] = {ui::Button::cross, tr("Write a comment")};
        items[count++] = {ui::Button::l1, tr("Top or newest"), ui::Button::r1};
    }
    else
    {
        items[count++] = {ui::Button::cross, tr("Reply")};
        items[count++] = {ui::Button::triangle, tr("Like")};
        if (items_[static_cast<std::size_t>(at)].has_replies())
            items[count++] = {ui::Button::dpad, tr("Replies")};
        items[count++] = {ui::Button::l1, tr("Top or newest"), ui::Button::r1};
    }
    items[count++] = {ui::Button::circle, tr("Back")};
    return {items, static_cast<std::size_t>(count)};
}

FocusInfo Comments::focus() const
{
    FocusInfo info;
    if (writing_)
    {
        info.id = "write/key";
        info.text = keys_.focused().kind == ui::KeyKind::character ? keys_.focused().text : std::string(tr("Post"));
        info.rect = keys_.key_rect(keys_.focus_row(), keys_.focus_key());
        info.index = keys_.focus_key();
        info.total = characters(text_);
        return info;
    }
    const int at = focused();
    if (at < 0)
    {
        const bool broken = items_.empty() && (load_ == model::Load::failed || load_ == model::Load::offline);
        info.id = broken ? "retry" : load_ == model::Load::ready ? "compose" : "thread";
        info.text = broken ? tr("Try again") : tr("Comments");
        info.rect = load_ == model::Load::ready ? kCompose : Rect{kLeft - 20.0f, kThreadTop, kThreadW, kPitch - 12.0f};
        info.total = 0;
        return info;
    }
    const Item &item = items_[static_cast<std::size_t>(at)];
    float rest = 0.0f;
    for (int i = 0; i < at; ++i)
        rest += rest_height(items_[static_cast<std::size_t>(i)]);
    const float indent = kIndent * static_cast<float>(item.depth);
    info.id = "comment/" + std::to_string(at);
    info.text = item.author;
    info.rect = {kLeft - 20.0f + indent, kThreadTop + rest - scroll_.target, kThreadW - indent, full_height(item) - 12.0f};
    info.index = at;
    info.total = static_cast<int>(items_.size());
    return info;
}

std::vector<std::pair<std::string, std::string>> Comments::focusable() const
{
    std::vector<std::pair<std::string, std::string>> out;
    if (writing_)
        return {{"write/key", text_}};
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        if (items_[i].focusable())
            out.emplace_back("comment/" + std::to_string(i), items_[i].author);
    }
    if (out.empty())
        out.emplace_back(items_.empty() && (load_ == model::Load::failed || load_ == model::Load::offline) ? "retry"
                         : load_ == model::Load::ready                                                     ? "compose"
                                                                                                             : "thread",
                         tr("Comments"));
    return out;
}

void Comments::snapshot(Snapshot &out) const
{
    out.set("video", video_);
    out.set("order", order_ == 0 ? "top" : "newest");
    out.set("load", load_name(load_));
    int comments = 0;
    int answers = 0;
    for (const Item &item : items_)
    {
        comments += item.kind == Item::Kind::comment && item.depth == 0 ? 1 : 0;
        answers += item.kind == Item::Kind::comment && item.depth > 0 ? 1 : 0;
    }
    out.set("comments", comments);
    out.set("replies_shown", answers);
    out.set("pages", pages_[order_]);
    out.set("count", count_);
    out.set("can_write", can_write() ? "1" : "0");
    out.set("writing", writing_ ? (writing_to_ >= 0 ? "reply" : "comment") : "no");
    out.set("text", text_);
    out.set("trouble", trouble_);
    out.set("pending", job_name(job_.kind));
    out.set("scroll", static_cast<int>(scroll_.target));
    const int at = focused();
    out.set("liked", at >= 0 && items_[static_cast<std::size_t>(at)].liked ? "1" : "0");
    out.set("lines", at >= 0 ? items_[static_cast<std::size_t>(at)].lines : 0);
    std::vector<std::string> authors;
    for (const Item &item : items_)
        authors.push_back(item.kind == Item::Kind::comment ? (item.depth > 0 ? "> " : "") + item.author : std::string("..."));
    out.list("thread", std::move(authors));
}

} // namespace afterglow::comments_page
