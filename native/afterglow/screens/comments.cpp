// ProsperoTube Afterglow - the comments of a video: what is read, the focus, writing and rating.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Writing, rating and the input are in comments_input.cpp, the drawing in
// comments_draw.cpp; comments.hpp says what the screen reads.

#include "afterglow/screens/comments.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace afterglow
{

namespace comments_page
{

namespace
{

constexpr int kPagesLimit = 12; // pages of a thread one visit reads
int number_of(const std::string &text)
{
    return text.empty() ? 0 : static_cast<int>(std::strtol(text.c_str(), nullptr, 10));
}

// "4.1K likes" is "4.1K": what stands before the first space, when it starts with a digit.
std::string leading_count(std::string_view text)
{
    if (text.empty() || text[0] < '0' || text[0] > '9')
        return {};
    return std::string(text.substr(0, text.find(' ')));
}

float text_width(int depth)
{
    return kThreadW - 72.0f - 40.0f - kIndent * static_cast<float>(depth);
}

} // namespace

// ---- entering -------------------------------------------------------------------------

void Comments::enter(const Route &route, UpdateContext &context, bool restored)
{
    (void)context;
    if (restored)
        return; // back from signing in: the thread, the focus and what was typed stay
    *this = Comments();
    route_ = route;
    // The argument is the video; a discussion's own catalog id names the order too.
    std::string_view video = route.argument;
    if (starts_with(video, "discussion:"))
    {
        video.remove_prefix(11);
        if (starts_with(video, "newest:"))
        {
            order_ = 1;
            video.remove_prefix(7);
        }
        else if (starts_with(video, "top:"))
        {
            video.remove_prefix(4);
        }
    }
    video_ = std::string(video);
    top_id_ = "discussion:top:" + video_;
    newest_id_ = "discussion:newest:" + video_;
    details_id_ = "details:" + video_;
    order_pos_.snap(static_cast<float>(order_));
    keys_.style.bindings = ui::KeyboardBindings::standard();
    keys_.style.gap = 8.0f;
    keys_.style.panel = false;
    keys_.style.max_length = kTextLimit;
    keys_.set_bounds(kKeys);
}

std::string Comments::title() const
{
    return tr("Comments");
}

// ---- reading --------------------------------------------------------------------------

Item Comments::item_of(const model::Row &row, int depth, const ui::Fonts &fonts)
{
    Item item;
    item.id = row.id;
    item.depth = depth;
    item.author = row.fact("author").empty() ? row.title : clean(row.fact("author"));
    item.text = row.overview;
    item.age = clean(row.fact("published"));
    item.likes = clean(row.fact("likes"));
    item.replies = number_of(row.fact("replies"));
    item.avatar = row.fact("author_art_url").empty() ? row.thumb : row.fact("author_art_url");
    item.pinned = row.has("pinned");
    item.owner = row.has("owner");
    item.hearted = row.has("hearted");
    item.liked = row.has("liked");
    // What the facts leave open stands in the provider's own line: "2 weeks ago | 2.8K likes | 12 replies | ...".
    int part = 0;
    for (std::size_t at = 0; at < row.subtitle.size(); ++part)
    {
        std::size_t end = row.subtitle.find(" | ", at);
        if (end == std::string::npos)
            end = row.subtitle.size();
        const std::string_view token(row.subtitle.data() + at, end - at);
        if (part == 0 && item.age.empty())
            item.age = std::string(token);
        else if (part == 1 && item.likes.empty())
            item.likes = leading_count(token);
        else if (part == 2 && item.replies == 0)
            item.replies = number_of(leading_count(token));
        at = end + 3;
    }
    const auto rated = rated_.find(row.id);
    if (rated != rated_.end())
        item.liked = rated->second;
    const auto known = lines_.find(row.id);
    if (known != lines_.end())
    {
        item.lines = known->second;
    }
    else
    {
        item.lines = std::clamp(measure_block(fonts.regular, item.text, kTypeCard, text_width(depth), kTextLine, kLinesInFocus).lines, 1,
                                kLinesInFocus);
        lines_[row.id] = item.lines;
    }
    return item;
}

void Comments::read_video(UpdateContext &context)
{
    const Page &page = context.services.data.catalog(details_id_);
    video_load_ = page.load;
    if (page.load != model::Load::ready || page.rows.empty())
        return;
    const model::Row &head = page.rows.front();
    video_title_ = head.title;
    video_thumb_ = head.thumb;
    video_channel_ = clean(head.fact("channel"));
    for (const model::Row &row : page.rows)
    {
        if (video_channel_.empty() && starts_with(row.id, "channel:"))
            video_channel_ = row.title;
    }
}

void Comments::read(UpdateContext &context)
{
    Data &data = context.services.data;
    const ui::Fonts &fonts = context.services.fonts;
    profile_ = data.profile();
    read_video(context);

    std::vector<Item> items;
    std::string count;
    std::string post_row;
    std::string open_parent; // the comment whose replies have a further page
    bool further = false;
    bool own_placed = false; // what was written here stands above the first comment that is not pinned
    const auto tail = [&](model::Load load, const std::string &words, int depth, int parent)
    {
        Item item;
        item.kind = load == model::Load::loading || load == model::Load::idle ? Item::Kind::waiting : Item::Kind::note;
        item.id = "tail:" + std::to_string(items.size());
        item.text = words;
        item.depth = depth;
        item.parent = parent;
        item.h = kTailHeight;
        items.push_back(std::move(item));
    };
    const auto place_own = [&](const std::string &parent_id, int parent)
    {
        for (std::size_t i = 0; i < echoes_.size(); ++i)
        {
            if (echo_parents_[i] != parent_id)
                continue;
            Item item = echoes_[i];
            item.parent = parent;
            items.push_back(std::move(item));
        }
    };
    model::Load first = model::Load::loading;
    std::string error;
    for (int index = 0; index < pages_[order_]; ++index)
    {
        const Page &page = data.catalog(catalog(), index);
        if (index == 0)
        {
            first = page.load;
            error = page.error;
        }
        if (page.load != model::Load::ready)
        {
            if (index > 0)
                tail(page.load, page.error, 0, -1); // the thread stays as far as it came
            further = false;
            break;
        }
        for (const model::Row &row : page.rows)
        {
            if (starts_with(row.id, "comment:post:"))
            {
                post_row = row.id;
                count = row.subtitle;
                continue;
            }
            if (row.id == "account:notice:comments")
            {
                count = row.title;
                continue;
            }
            if (!starts_with(row.id, "discussion:"))
                continue;
            Item item = item_of(row, 0, fonts);
            if (!own_placed && !item.pinned)
            {
                place_own({}, -1);
                own_placed = true;
            }
            const int parent = static_cast<int>(items.size());
            const bool has_replies = item.has_replies();
            items.push_back(std::move(item));
            const auto open = open_.find(row.id);
            if (open == open_.end())
                continue;
            place_own(row.id, parent);
            if (!has_replies)
                continue;
            for (int reply_page = 0; reply_page < open->second; ++reply_page)
            {
                const Page &replies = data.catalog(row.id, reply_page);
                if (replies.load != model::Load::ready)
                {
                    tail(replies.load, replies.error, 1, parent);
                    break;
                }
                for (const model::Row &reply : replies.rows)
                {
                    if (!starts_with(reply.id, "discussion:"))
                        continue;
                    Item answer = item_of(reply, 1, fonts);
                    answer.parent = parent;
                    answer.replies = 0; // one level: a reply's own thread is not opened here
                    items.push_back(std::move(answer));
                }
                if (reply_page + 1 == open->second && replies.more)
                    open_parent = row.id;
            }
        }
        further = page.more;
    }
    if (!own_placed && first == model::Load::ready)
        place_own({}, -1);

    load_ = first;
    error_ = error;
    if (first == model::Load::ready)
    {
        // While a newer answer is on its way the thread keeps what it shows.
        items_ = std::move(items);
        count_ = std::move(count);
        post_row_ = std::move(post_row);
    }
    else if (first == model::Load::failed || first == model::Load::offline)
    {
        items_.clear();
    }

    // The focus stays on its comment when the list changes around it.
    const int total = static_cast<int>(items_.size());
    int found = -1;
    for (int i = 0; i < total && found < 0; ++i)
    {
        if (!focus_id_.empty() && items_[static_cast<std::size_t>(i)].id == focus_id_ && items_[static_cast<std::size_t>(i)].focusable())
            found = i;
    }
    if (found < 0)
    {
        // ... or moves to the nearest one there is.
        for (int reach = 0; reach < total && found < 0; ++reach)
        {
            for (const int at : {focus_ - reach, focus_ + reach})
            {
                if (at >= 0 && at < total && items_[static_cast<std::size_t>(at)].focusable())
                {
                    found = at;
                    break;
                }
            }
        }
    }
    focus_ = std::max(found, 0);
    focus_id_ = found >= 0 ? items_[static_cast<std::size_t>(found)].id : std::string();
    if (writing_ && writing_to_ >= 0)
    {
        // The comment being answered is found again by its row.
        writing_to_ = -1;
        for (int i = 0; i < total; ++i)
        {
            if (items_[static_cast<std::size_t>(i)].id == writing_id_)
                writing_to_ = i;
        }
    }

    // More is asked for when the focus comes near the end of what there is.
    if (found >= 0)
    {
        const Item &at = items_[static_cast<std::size_t>(found)];
        if (further && found + 4 >= total && pages_[order_] < kPagesLimit)
            ++pages_[order_];
        if (!open_parent.empty())
        {
            const std::string &parent_id = at.depth > 0 && at.parent >= 0 ? items_[static_cast<std::size_t>(at.parent)].id : at.id;
            int last = found;
            while (last + 1 < total && items_[static_cast<std::size_t>(last + 1)].depth > 0)
                ++last;
            if (parent_id == open_parent && at.depth > 0 && found + 2 >= last && open_[open_parent] < kPagesLimit)
                ++open_[open_parent];
        }
    }
}

// ---- layout ---------------------------------------------------------------------------

float Comments::full_height(const Item &item) const
{
    if (item.kind != Item::Kind::comment)
        return kTailHeight;
    return kPitch + kTextLine * static_cast<float>(std::clamp(item.lines, kLinesAtRest, kLinesInFocus) - kLinesAtRest);
}

int Comments::focused() const
{
    return focus_ >= 0 && focus_ < static_cast<int>(items_.size()) && items_[static_cast<std::size_t>(focus_)].focusable() ? focus_ : -1;
}

// The first comment shown: the one in focus keeps three above it where that fits.
int Comments::first_shown() const
{
    const int at = focused();
    if (at < 0)
        return 0;
    const float open = full_height(items_[static_cast<std::size_t>(at)]);
    const int above = std::clamp(static_cast<int>((kThreadView - open) / kPitch), 0, 3);
    return std::max(0, at - above);
}

void Comments::lay_out()
{
    float y = 0.0f;
    for (std::size_t i = 0; i < items_.size(); ++i)
    {
        Item &item = items_[i];
        // The comment in focus opens to its whole text; the others rest at two lines.
        const float near = tween::clamp01(1.0f - std::fabs(focus_pos_.value - static_cast<float>(i)));
        item.h = tween::lerp(rest_height(item), full_height(item), near);
        item.y = y;
        y += item.h;
    }
}

Rect Comments::item_rect(int index) const
{
    if (index < 0 || index >= static_cast<int>(items_.size()))
        return {kLeft - 20.0f, kThreadTop, kThreadW, kPitch - 12.0f};
    const Item &item = items_[static_cast<std::size_t>(index)];
    const float indent = kIndent * static_cast<float>(item.depth);
    return {kLeft - 20.0f + indent, kThreadTop + item.y - scroll_.value, kThreadW - indent, item.h - 12.0f};
}

} // namespace comments_page

std::unique_ptr<Screen> make_comments()
{
    return std::make_unique<comments_page::Comments>();
}

} // namespace afterglow
