// ProsperoTube Afterglow - the video page: what it reads, where the focus goes, what the buttons do.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the buttons do is in video_input.cpp, the drawing in video_draw.cpp;
// video.hpp says what the page reads.

#include "afterglow/screens/video.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace afterglow
{

namespace video_page
{

namespace
{

constexpr int kRelatedPagesLimit = 8; // pages of the related list one visit reads

int number_of(const std::string &text)
{
    return text.empty() ? 0 : static_cast<int>(std::strtol(text.c_str(), nullptr, 10));
}

// "48:12" is 2892, "1:02:44" is 3764; 0 for anything that is not a length.
int length_of(std::string_view token)
{
    int total = 0;
    int part = 0;
    int colons = 0;
    for (const char c : token)
    {
        if (c >= '0' && c <= '9')
            part = part * 10 + (c - '0');
        else if (c == ':')
        {
            total = (total + part) * 60;
            part = 0;
            ++colons;
        }
        else
            return 0;
    }
    return colons > 0 ? total + part : 0;
}

// "48,213 likes" is "48,213": a count as YouTube words it, without the word after it.
std::string leading_count(const std::string &text)
{
    if (text.empty() || text[0] < '0' || text[0] > '9')
        return {};
    const std::size_t end = text.find(' ');
    // "1 234 likes" keeps its inner spaces: stop at the first space a letter follows.
    for (std::size_t at = end; at != std::string::npos; at = text.find(' ', at + 1))
    {
        const char next = at + 1 < text.size() ? text[at + 1] : 'x';
        if (next < '0' || next > '9')
            return text.substr(0, at);
    }
    return text;
}

// The digits of a count YouTube wrote out ("48,213"), -1 for a rounded one ("48K").
// separator receives what stands between its groups of three.
long long written_out(const std::string &text, std::string *separator)
{
    long long value = 0;
    bool digits = false;
    bool between = false; // inside the first separator
    bool taken = false;   // the first separator is complete
    separator->clear();
    for (const char raw : text)
    {
        const unsigned char c = static_cast<unsigned char>(raw);
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            digits = true;
            taken = taken || between;
            between = false;
        }
        else if (c == ',' || c == '.' || c == ' ' || c == '\'' || c >= 0x80)
        {
            if (!taken)
            {
                separator->push_back(raw); // a narrow space takes several bytes
                between = true;
            }
        }
        else
        {
            return -1;
        }
    }
    // "1.2" ends in a group of fewer than three digits: a rounded number, not a count.
    const std::size_t last = text.find_last_not_of("0123456789");
    if (last != std::string::npos && text.size() - last - 1 != 3)
        return -1;
    return digits ? value : -1;
}

std::string grouped(long long value, const std::string &separator)
{
    std::string digits = std::to_string(std::max<long long>(value, 0));
    if (separator.empty())
        return digits;
    std::string out;
    const std::size_t lead = digits.size() % 3;
    for (std::size_t at = 0; at < digits.size(); ++at)
    {
        if (at != 0 && (at + 3 - lead) % 3 == 0)
            out += separator;
        out.push_back(digits[at]);
    }
    return out;
}

// What a row of the actions menu is. top is the menu itself: the page's own
// buttons stand for some of its rows there.
MenuRow classify(const model::Row &row, bool top)
{
    MenuRow out;
    out.id = row.id;
    out.title = row.title;
    out.detail = row.subtitle;
    const std::string_view id = row.id;
    if (starts_with(id, "account:confirm:"))
    {
        const std::string action = account_action(id);
        out.kind = RowKind::account;
        if (action == "like" || action == "unrate")
            out.icon = Icon::heart;
        else if (action == "dislike")
            out.icon = Icon::close;
        else if (starts_with(action, "watch_later") || action == "history_remove")
            out.icon = Icon::clock;
        else if (action == "subscribe")
            out.icon = Icon::plus;
        else if (action == "mark_watched" || action == "unsubscribe")
            out.icon = Icon::check;
        else
            out.icon = Icon::hide; // not interested, don't recommend, hide
        if (top && (action == "like" || action == "unrate" || starts_with(action, "watch_later") || action == "subscribe" ||
                    action == "unsubscribe"))
            out.kind = RowKind::hidden;
    }
    else if (starts_with(id, "playlist:confirm:"))
    {
        out.kind = RowKind::account;
        out.icon = Icon::playlist_add;
    }
    else if (starts_with(id, "playlist:pick:"))
    {
        out.kind = RowKind::sublist;
        out.icon = Icon::playlist_add;
    }
    else if (starts_with(id, "reminder:") || starts_with(id, "notify:"))
    {
        out.kind = RowKind::sublist;
        out.icon = Icon::bell;
    }
    else if (starts_with(id, "local:video:"))
    {
        const std::string verb = verb_of(id);
        out.kind = verb == "restart" ? RowKind::restart : verb == "incognito" ? RowKind::incognito : RowKind::local;
        out.icon = verb == "next"                               ? Icon::queue
                   : verb == "watched"                          ? Icon::check
                   : verb == "forget"                           ? Icon::clock
                   : verb == "block" || verb == "unblock"       ? Icon::block
                   : verb == "restart"                          ? Icon::update
                   : verb == "incognito"                        ? Icon::moon
                   : verb == "sbexclude" || verb == "sbinclude" ? Icon::skip
                                                                : Icon::more;
        if (top && (verb == "next" || verb == "restart"))
            out.kind = RowKind::hidden;
    }
    else if (starts_with(id, "channel:"))
    {
        out.kind = RowKind::channel;
        out.icon = Icon::user;
    }
    else if (starts_with(id, "description:"))
    {
        out.kind = RowKind::reader;
        out.icon = Icon::chapters;
    }
    else if (starts_with(id, "library:playlist:"))
    {
        out.kind = RowKind::grid;
        out.icon = Icon::grid;
    }
    else if (starts_with(id, "account:notice"))
    {
        out.kind = RowKind::note;
        out.icon = Icon::info;
    }
    // Everything else the page has itself (details, the related list, the
    // link) or this stage cannot do (a new playlist needs a name typed).
    return out;
}

} // namespace

// ---- entering -------------------------------------------------------------------------

void VideoPage::enter(const Route &route, UpdateContext &context, bool restored)
{
    (void)context;
    if (restored)
    {
        age_ = 0.0f; // the page assembles again; the focus and the shelf stay where they were
        return;
    }
    *this = VideoPage();
    route_ = route;
    id_ = route.argument;
    details_id_ = "details:" + id_;
    links_id_ = "link:video:" + id_;
    list_.style.highlight.kind = ui::HighlightKind::fill;
    list_.style.row_height = 68.0f;
    list_.style.title_size = kTypeName;
    list_.style.value_size = kTypeMeta;
    list_.style.leading_width = 52.0f;
    list_.style.gap = 4.0f;
    list_.style.sounds.cancel = audio::Cue::modal_close;
    list_.set_bounds(kSheetList);
    spinner_.style.kind = ui::SpinnerKind::arc;
}

// ---- reading --------------------------------------------------------------------------

void VideoPage::read_details(const Page &page)
{
    Details d;
    const model::Row &head = page.rows.front();
    d.title = head.title;
    d.overview = head.overview;
    d.thumb = head.thumb;
    d.views = clean(head.fact("views"));
    d.age = clean(head.fact("published"));
    d.likes = clean(head.fact("likes"));
    d.channel = clean(head.fact("channel"));
    d.channel_id = head.fact("channel_id");
    d.avatar = head.fact("channel_art_url");
    d.subscribers = clean(head.fact("subscribers"));
    d.rating = head.fact("rating");
    d.subscribed = head.has("subscribed");
    d.saved = head.has("saved");
    d.chapters = number_of(head.fact("chapters"));
    d.segments = number_of(head.fact("segments"));
    d.captions = number_of(head.fact("captions"));
    d.fps = number_of(head.fact("fps"));
    d.duration_s = number_of(head.fact("duration"));
    d.resume_s = number_of(head.fact("resume"));
    d.live = head.has("live");
    d.upcoming = head.has("upcoming");
    const std::string &quality = head.fact("quality");
    d.uhd = quality.find("4k") != std::string::npos;
    d.hdr = quality.find("hdr") != std::string::npos;
    // The row's second line is the provider's own summary ("4K | 48:12"): what the facts leave open.
    for (std::size_t at = 0; at < head.subtitle.size();)
    {
        std::size_t end = head.subtitle.find(" | ", at);
        if (end == std::string::npos)
            end = head.subtitle.size();
        const std::string_view token(head.subtitle.data() + at, end - at);
        if (token == "4K")
            d.uhd = true;
        else if (d.duration_s == 0)
            d.duration_s = length_of(token);
        at = end + 3;
    }
    // A provider that lists the video itself first says the same in the video's own fields.
    if (!head.folder && !page.videos.empty() && page.videos.front().id == head.id)
    {
        const model::Video &video = page.videos.front();
        if (d.views.empty())
            d.views = video.views;
        if (d.age.empty())
            d.age = video.age;
        if (d.avatar.empty())
            d.avatar = video.avatar;
        if (d.duration_s == 0)
            d.duration_s = video.duration_s;
        if (d.resume_s == 0 && video.progress > 0.0f && video.progress < 1.0f)
            d.resume_s = static_cast<int>(video.progress * static_cast<float>(video.duration_s));
        d.live = d.live || (video.badges & model::kBadgeLive) != 0;
        d.upcoming = d.upcoming || (video.badges & model::kBadgeUpcoming) != 0;
        d.uhd = d.uhd || (video.badges & model::kBadge4K) != 0;
        d.hdr = d.hdr || (video.badges & model::kBadgeHdr) != 0;
        if (d.captions == 0 && (video.badges & model::kBadgeCaptions) != 0)
            d.captions = 1;
    }
    // The rows after the first: the channel, the numbers, the description, the related list.
    for (std::size_t i = 1; i < page.rows.size(); ++i)
    {
        const model::Row &row = page.rows[i];
        if (starts_with(row.id, "channel:"))
        {
            if (d.channel_id.empty())
                d.channel_id = row.id.substr(8);
            if (d.channel.empty())
                d.channel = row.title;
            if (d.subscribers.empty() && !row.interface_text)
                d.subscribers = row.subtitle;
            if (d.avatar.empty())
                d.avatar = row.thumb;
        }
        else if (row.id == "account:notice:views" && d.views.empty())
            d.views = row.title;
        else if (row.id == "account:notice:published" && d.age.empty())
            d.age = row.title;
        else if (row.id == "account:notice:likes" && d.likes.empty())
            d.likes = leading_count(row.title);
        else if (starts_with(row.id, "description:"))
            d.description = true;
        else if (starts_with(row.id, "library:suggestions:"))
            d.related = row.id;
    }
    if (d.live || d.upcoming)
    {
        d.duration_s = 0;
        d.resume_s = 0;
    }
    if (d.duration_s > 0 && d.resume_s >= d.duration_s)
        d.resume_s = 0; // watched to the end: it starts over
    likes_number_ = written_out(d.likes, &likes_separator_);
    details_ = std::move(d);
}

void VideoPage::read_related(UpdateContext &context)
{
    related_more_ = false;
    if (!have_ || details_.related.empty())
    {
        related_load_ = have_ ? model::Load::ready : model::Load::idle;
        if (have_)
            related_.clear();
    }
    else
    {
        std::vector<model::Video> all;
        bool further = false;
        for (int index = 0; index < related_pages_; ++index)
        {
            const Page &page = context.services.data.catalog(details_.related, index);
            if (index == 0)
                related_load_ = page.load;
            if (page.load != model::Load::ready)
            {
                // A further page that fails ends the shelf where it is.
                related_more_ = index > 0 && page.load == model::Load::loading;
                further = false;
                break;
            }
            all.insert(all.end(), page.videos.begin(), page.videos.end());
            further = page.more;
        }
        if (related_load_ == model::Load::ready)
            related_ = std::move(all); // while a newer answer is on its way the shelf keeps what it shows
        // The next page is asked for when the focus comes near the end of what there is.
        if (further && zone_ == Zone::related && related_col_ + 4 >= static_cast<int>(related_.size()) &&
            related_pages_ < kRelatedPagesLimit)
            ++related_pages_;
    }
    const int count = static_cast<int>(related_.size());
    related_col_ = std::clamp(related_col_, 0, std::max(count - 1, 0));
    if (count == 0 && zone_ == Zone::related)
        zone_ = Zone::actions; // the shelf went away under the focus
}

void VideoPage::read_menu(UpdateContext &context)
{
    Data &data = context.services.data;
    if (menu_id_.empty())
    {
        // The menu is named once the video's title is known, or when it is asked for before that.
        if (!have_ && layer_ != Layer::menu)
            return;
        menu_id_ = data.menu(id_, details_.title);
        // REQUEST: Data::menu() names the menus of folders; a video's actions are this catalog of the provider.
        if (menu_id_.empty())
            menu_id_ = "account:menu:" + id_;
    }
    const Page &first = data.catalog(menu_id_);
    menu_load_ = first.load;
    menu_error_ = first.error;
    if (first.load != model::Load::ready)
        return;
    menu_all_.clear();
    for (const model::Row &row : first.rows)
        menu_all_.push_back(classify(row, true));
    if (first.more)
    {
        const Page &second = data.catalog(menu_id_, 1);
        if (second.load == model::Load::ready)
        {
            for (const model::Row &row : second.rows)
                menu_all_.push_back(classify(row, true));
        }
    }
}

void VideoPage::read_links(UpdateContext &context)
{
    if (!share_asked_)
        return;
    const Page &page = context.services.data.catalog(links_id_);
    links_load_ = page.load;
    links_error_ = page.error;
    if (page.load != model::Load::ready)
        return;
    std::vector<ShareLink> links;
    const auto url_of = [&](std::string_view row_id) -> std::string
    {
        for (const model::Row &row : page.rows)
        {
            if (row.id == row_id)
                return row.overview.empty() ? row.subtitle : row.overview;
        }
        return {};
    };
    const std::string video = url_of("link:row:video");
    if (!video.empty())
    {
        links.push_back({"video", tr("Video"), video, {}});
        // The provider's link opens at the start; the place the profile stopped at is added here.
        if (details_.resume_s > 0 && video.find('?') == std::string::npos)
            links.push_back({"time", fill(tr("At {0}"), duration_text(details_.resume_s)),
                             video + "?t=" + std::to_string(details_.resume_s), {}});
    }
    const std::string channel = url_of("link:row:channel");
    if (!channel.empty())
        links.push_back({"channel", tr("Channel"), channel, {}});
    const std::string playlist = url_of("link:row:playlist");
    if (!playlist.empty())
        links.push_back({"playlist", tr("Playlist"), playlist, {}});
    if (links.empty())
    {
        links_load_ = model::Load::failed;
        links_error_ = tr("YouTube gave no link for this video.");
    }
    if (!link_chosen_)
    {
        // The link that opens where the viewer is comes first when there is one.
        link_ = 0;
        for (std::size_t i = 0; i < links.size(); ++i)
        {
            if (links[i].id == "time")
                link_ = static_cast<int>(i);
        }
    }
    link_ = std::clamp(link_, 0, std::max(static_cast<int>(links.size()) - 1, 0));
    links_ = std::move(links);
    lay_out_links(context.services.fonts);
}

void VideoPage::lay_out_links(const ui::Fonts &fonts)
{
    G g{nowhere_, fonts, look_afterglow(), 0, 0.0f, true}; // measures; draws nothing
    const float left = kSharePanel.x + 532.0f;
    const float room = 432.0f;
    float x = left;
    float cy = kSharePanel.y + 436.0f;
    for (ShareLink &link : links_)
    {
        const float w = chip_width(g, link.label);
        if (x > left && x + w > left + room)
        {
            x = left; // the chips of a longer language take a second row
            cy += 56.0f;
        }
        link.chip = {x, cy - 22.0f, w, 44.0f};
        x += w + 12.0f;
    }
}

void VideoPage::read(UpdateContext &context)
{
    Data &data = context.services.data;
    profile_ = data.profile();
    const Page &page = data.catalog(details_id_);
    if (page.load == model::Load::ready && !page.rows.empty())
    {
        read_details(page);
        have_ = true;
        load_ = model::Load::ready;
        error_.clear();
    }
    else if (!have_)
    {
        // An answer with nothing in it is a video YouTube says nothing about.
        load_ = page.load == model::Load::ready ? model::Load::failed : page.load;
        error_ = page.load == model::Load::ready ? std::string(tr("YouTube has no details for this video.")) : page.error;
    }
    read_related(context);
    read_menu(context);
    read_links(context);
}

void VideoPage::lay_out(const ui::Fonts &fonts)
{
    if (details_.title != measured_title_)
    {
        measured_title_ = details_.title;
        title_lines_ = std::clamp(measure_block(fonts.display, details_.title, kTypePage, kColumn, kTitleLine, 3).lines, 1, 3);
    }
    // A longer title leaves the excerpt fewer lines, so the facts never reach the buttons.
    const int limit = std::clamp(5 - title_lines_, 2, 4);
    if (details_.overview != measured_overview_ || limit != measured_limit_)
    {
        measured_overview_ = details_.overview;
        measured_limit_ = limit;
        description_lines_ = details_.overview.empty()
                                 ? 0
                                 : std::clamp(measure_block(fonts.regular, details_.overview, kTypeRead, kColumn, 32.0f, limit).lines, 1, limit);
    }
    const float last = kTitleY + kTitleLine * static_cast<float>(title_lines_ - 1);
    meta_y_ = last + 44.0f;
    const bool channel = !details_.channel.empty();
    channel_y_ = meta_y_ + 60.0f;
    const float under = channel ? channel_y_ : meta_y_ - 18.0f;
    description_y_ = under + 66.0f;
    chips_y_ = description_lines_ > 0 ? description_y_ + 32.0f * static_cast<float>(description_lines_) - 8.0f : under + 46.0f;
}

std::string VideoPage::like_label() const
{
    if (likes_number_ >= 0)
        return grouped(static_cast<long long>(std::llround(like_count_.value)), likes_separator_);
    return details_.likes.empty() ? std::string(tr("Like")) : details_.likes;
}

void VideoPage::build_buttons(const ui::Fonts &fonts)
{
    G g{nowhere_, fonts, look_afterglow(), 0, 0.0f, true}; // measures; draws nothing
    buttons_.clear();
    const auto add = [&](Act act, std::string label, Icon icon)
    {
        ActionButton button;
        button.act = act;
        button.label = std::move(label);
        button.icon = icon;
        buttons_.push_back(std::move(button));
    };
    const Request::Kind flying = request_.kind;
    if (!have_ && (load_ == model::Load::failed || load_ == model::Load::offline))
    {
        add(Act::retry, tr("Try again"), Icon::update);
        buttons_.back().primary = true;
    }
    else
    {
        add(Act::play, details_.live ? std::string(tr("Watch live"))
                       : details_.resume_s > 0 ? fill(tr("Resume {0}"), duration_text(details_.resume_s))
                                               : std::string(tr("Play")),
            Icon::play);
        buttons_.back().primary = true;
        if (have_)
        {
            if (details_.resume_s > 0)
            {
                add(Act::restart, tr("From start"), Icon::update);
                buttons_.back().waiting = flying == Request::Kind::restart;
            }
            add(Act::queue, tr("Queue"), Icon::queue);
            buttons_.back().waiting = flying == Request::Kind::queue;
            add(Act::later, saved() ? tr("Saved") : tr("Watch later"), saved() ? Icon::check : Icon::clock);
            buttons_.back().selected = saved();
            buttons_.back().waiting = flying == Request::Kind::save || flying == Request::Kind::unsave;
            add(Act::like, like_label(), Icon::heart);
            buttons_.back().selected = liked();
            buttons_.back().waiting = flying == Request::Kind::like || flying == Request::Kind::unrate;
            add(Act::share, tr("Share"), Icon::share);
            add(Act::more, {}, Icon::more);
            buttons_.back().waiting = flying == Request::Kind::menu || flying == Request::Kind::incognito;
        }
    }
    // Every button is as wide as its words. Where a language makes the row
    // longer than the page, the buttons whose mark says enough lose their word.
    static constexpr Act kDropOrder[] = {Act::share, Act::queue, Act::restart, Act::later};
    for (std::size_t dropped = 0;; ++dropped)
    {
        float x = kLeft;
        for (ActionButton &button : buttons_)
        {
            button.x = x;
            button.w = button.label.empty() ? kActionH : button_width(g, button.label, true) + (button.primary ? 24.0f : 0.0f);
            x += button.w + kActionGap;
        }
        if (x - kActionGap <= kRight || dropped >= std::size(kDropOrder))
            break;
        const int index = button_index(kDropOrder[dropped]);
        if (index >= 0)
            buttons_[static_cast<std::size_t>(index)].label.clear();
    }
    if (button_index(action_) < 0)
        action_ = buttons_.front().act; // the button under the focus is gone: the first one takes it
}

void VideoPage::fill_sheet(UpdateContext &context)
{
    if (layer_ != Layer::menu && layer_amount_.value <= 0.01f)
        return;
    std::vector<MenuRow> rows;
    if (sheet_catalog_.empty())
    {
        sheet_load_ = menu_load_;
        sheet_error_ = menu_error_;
        for (const MenuRow &row : menu_all_)
        {
            if (row.kind != RowKind::hidden)
                rows.push_back(row);
        }
    }
    else
    {
        const Page &page = context.services.data.catalog(sheet_catalog_);
        sheet_load_ = page.load;
        sheet_error_ = page.error;
        for (const model::Row &row : page.rows)
        {
            MenuRow item = classify(row, false);
            if (item.kind != RowKind::hidden)
                rows.push_back(std::move(item));
        }
    }
    std::string signature = sheet_catalog_;
    for (const MenuRow &row : rows)
        signature += '\n' + row.id + '\t' + row.title + '\t' + row.detail;
    if (signature == sheet_signature_)
        return;
    sheet_signature_ = std::move(signature);
    sheet_rows_ = std::move(rows);
    std::vector<ui::ListItem> items;
    for (std::size_t i = 0; i < sheet_rows_.size(); ++i)
    {
        ui::ListItem item;
        item.title = sheet_rows_[i].title;
        if (!sheet_catalog_.empty())
            item.value = sheet_rows_[i].detail; // a choice says what it is: a playlist's size
        item.chevron = sheet_rows_[i].kind == RowKind::channel || sheet_rows_[i].kind == RowKind::reader ||
                       sheet_rows_[i].kind == RowKind::grid || sheet_rows_[i].kind == RowKind::sublist;
        item.disabled = sheet_rows_[i].kind == RowKind::note;
        item.tag = static_cast<int>(i);
        items.push_back(std::move(item));
    }
    list_.set_items(std::move(items));
}

// ---- geometry --------------------------------------------------------------------------

int VideoPage::button_index(Act act) const
{
    for (std::size_t i = 0; i < buttons_.size(); ++i)
    {
        if (buttons_[i].act == act)
            return static_cast<int>(i);
    }
    return -1;
}

Rect VideoPage::button_rect(int index) const
{
    if (index < 0 || index >= static_cast<int>(buttons_.size()))
        return {kLeft, kActionY, kActionH, kActionH};
    const ActionButton &button = buttons_[static_cast<std::size_t>(index)];
    return {button.x, kActionY, button.w, kActionH};
}

Rect VideoPage::subscribe_rect() const
{
    const float w = std::max(subscribe_w_.value, 120.0f);
    return {kLeft + kColumn - w, channel_y_ - 36.0f, w, 56.0f};
}

Rect VideoPage::related_rect(int index, float scroll) const
{
    return {kLeft + static_cast<float>(index) * kRelatedPitch - scroll, kRelatedY, kRelatedW, kRelatedH};
}

model::Video VideoPage::own() const
{
    model::Video video;
    video.id = id_;
    video.video_id = id_.size() == 11 ? id_ : std::string();
    video.title = details_.title;
    video.channel = details_.channel;
    video.channel_id = details_.channel_id;
    video.views = details_.views;
    video.age = details_.age;
    video.overview = details_.overview;
    video.thumb = details_.thumb;
    video.avatar = details_.avatar;
    video.duration_s = details_.duration_s;
    if (details_.duration_s > 0)
        video.progress = static_cast<float>(details_.resume_s) / static_cast<float>(details_.duration_s);
    if (details_.live)
        video.badges |= model::kBadgeLive;
    return video;
}

std::string VideoPage::menu_row(std::string_view verb) const
{
    for (const MenuRow &row : menu_all_)
    {
        if (verb_of(row.id) == verb)
            return row.id;
    }
    return {};
}

} // namespace video_page

std::unique_ptr<Screen> make_video()
{
    return std::make_unique<video_page::VideoPage>();
}

} // namespace afterglow
