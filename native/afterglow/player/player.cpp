// ProsperoTube Afterglow - the player's controls: what moves, what sounds, what it reports.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/player/parts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace afterglow::player
{

namespace
{

void json_string(std::string &out, std::string_view value)
{
    out.push_back('"');
    for (const char c : value)
    {
        const unsigned char u = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\')
        {
            out.push_back('\\');
            out.push_back(c);
        }
        else if (c == '\n')
            out += "\\n";
        else if (u < 0x20)
        {
            char buffer[8];
            std::snprintf(buffer, sizeof buffer, "\\u%04x", u);
            out += buffer;
        }
        else
            out.push_back(c);
    }
    out.push_back('"');
}

void json_field(std::string &out, const char *name, std::string_view value, bool comma = true)
{
    json_string(out, name);
    out.push_back(':');
    json_string(out, value);
    if (comma)
        out.push_back(',');
}

// Everything of a sheet that its list shows: when it changes, the list is laid out again.
std::string key_of(const Sheet &sheet)
{
    std::string key = sheet.kind + '\n' + sheet.title + '\n' + std::to_string(sheet.first);
    for (const Row &row : sheet.rows)
    {
        key += '\n' + row.label + '\t' + row.value + '\t' + row.tag + '\t' + row.icon;
        key += row.current ? 'c' : '-';
        key += row.disabled ? 'd' : '-';
        key += row.chevron ? '>' : '-';
    }
    return key;
}

int focused_row(const Sheet &sheet)
{
    for (std::size_t i = 0; i < sheet.rows.size(); ++i)
    {
        if (sheet.rows[i].focused)
            return static_cast<int>(i);
    }
    return sheet.rows.empty() ? -1 : 0;
}

ui::StatusKind status_of(std::string_view kind)
{
    return kind == "info"      ? ui::StatusKind::info
           : kind == "warning" ? ui::StatusKind::warning
           : kind == "danger"  ? ui::StatusKind::danger
           : kind == "success" ? ui::StatusKind::success
           : kind == "none"    ? ui::StatusKind::none
                               : ui::StatusKind::question;
}

ui::DialogContent content_of(const Dialog &dialog)
{
    ui::DialogContent content;
    content.icon = status_of(dialog.kind);
    content.title = dialog.title;
    content.body = dialog.body;
    for (const DialogButton &button : dialog.buttons)
        content.buttons.push_back({button.label, button.primary ? ui::ButtonKind::primary : ui::ButtonKind::secondary, button.destructive});
    content.default_button = std::clamp(dialog.focus, 0, std::max(0, static_cast<int>(dialog.buttons.size()) - 1));
    return content;
}

} // namespace

std::string clock_text(double seconds, bool negative)
{
    if (!(seconds >= 0.0))
        seconds = 0.0;
    const long total = static_cast<long>(seconds + 0.5);
    char buffer[32];
    if (total >= 3600)
        std::snprintf(buffer, sizeof buffer, "%s%ld:%02ld:%02ld", negative ? "-" : "", total / 3600, total / 60 % 60, total % 60);
    else
        std::snprintf(buffer, sizeof buffer, "%s%ld:%02ld", negative ? "-" : "", total / 60, total % 60);
    return buffer;
}

float hint_row(G &g, const std::vector<Hint> &hints, float x, float cy, bool right_align, bool on_video, float max_width,
               float size, float text_size, float gap)
{
    ui::Hint items[8];
    int count = 0;
    for (const Hint &hint : hints)
    {
        if (count == 8)
            break;
        if (hint.button == ui::Button::none || hint.label.empty())
            continue;
        items[count++] = {hint.button, hint.label.c_str(), hint.second};
    }
    ui::HintLayout layout;
    layout.size = size;
    layout.text_size = text_size;
    layout.cy = cy;
    layout.item_gap = gap;
    // Written in order of importance: the last ones give way in a language that needs the room.
    while (count > 1 && ui::measure_hints(g.f, items, count, layout) > max_width)
        --count;
    if (count == 0)
        return 0.0f;
    ui::GlyphStyle style = on_video || g.k.dark ? ui::GlyphStyle::dark() : ui::GlyphStyle::light();
    style.label = on_video ? kOnVideo.with_alpha(0.8f) : g.k.ink2;
    return ui::draw_hints(g.l, g.f, style, items, count, x, right_align, layout);
}

std::string fit_tracked(const Face &face, std::string_view value, float size, float width, float tracking)
{
    if (measure(face, value, size, tracking) <= width)
        return std::string(value);
    static const char *const kEllipsis = "\xE2\x80\xA6";
    std::string cut(value);
    while (!cut.empty())
    {
        // One character less, whole: never half of a UTF-8 sequence.
        std::size_t end = cut.size() - 1;
        while (end > 0 && (static_cast<unsigned char>(cut[end]) & 0xc0) == 0x80)
            --end;
        cut.erase(end);
        while (!cut.empty() && cut.back() == ' ')
            cut.pop_back();
        if (measure(face, cut + kEllipsis, size, tracking) <= width)
            return cut + kEllipsis;
    }
    return {};
}

model::Video video_of(const Card &card)
{
    model::Video video;
    video.id = card.id;
    video.title = card.title;
    video.channel = card.channel;
    video.thumb = card.thumb;
    video.duration_s = card.duration_s;
    video.badges = card.badges;
    return video;
}

Player::Player(const ui::Fonts &fonts, ImageCache *images) : fonts_(fonts), images_(images), look_(&look_afterglow())
{
    // The rows of a sheet are the kit's list; what stands at the right of a
    // row (its value, the mark of the choice in effect, a chevron) is ours.
    list_.style.highlight.kind = ui::HighlightKind::fill;
    list_.style.title_size = kTypeRow;
    list_.style.gap = 4.0f;
    list_.content = [this](ui::Canvas &canvas, const Rect &row, const ui::ListItem &item, int, float focus)
    {
        if (item.tag < 0 || item.tag >= static_cast<int>(sheet_.rows.size()))
            return;
        const Row &source = sheet_.rows[static_cast<std::size_t>(item.tag)];
        G g = make_g(canvas.list, canvas.glass);
        const Look &k = *look_;
        const bool options = sheet_.kind != "captions";
        const float fade = source.disabled ? 0.42f : 1.0f;
        const Color ink = gfx::mix(k.ink, k.on_plate, focus).with_alpha(fade);
        const Color quiet = gfx::mix(k.ink2, k.on_plate.with_alpha(0.74f), focus).with_alpha(fade);
        float left = row.x + 26.0f;
        float right = row.x + row.w - 26.0f;
        if (list_.style.leading_width > 0.0f)
        {
            if (!source.icon.empty())
                icon(g, icon_by_name(source.icon), row.x + 42.0f, row.cy(), 26.0f, gfx::mix(k.ink2, k.on_plate, focus).with_alpha(fade));
            left += list_.style.leading_width + 18.0f;
        }
        left += reduced_ ? 0.0f : 8.0f * focus;
        if (source.chevron)
        {
            icon(g, Icon::chevron_right, right - 8.0f, row.cy(), 26.0f, quiet);
            right -= 34.0f;
        }
        if (source.current)
        {
            // The choice in effect keeps its mark whatever is in focus.
            icon(g, Icon::check, right - 10.0f, row.cy(), 24.0f, gfx::mix(k.accent, k.on_plate, focus));
            right -= 38.0f;
        }
        const std::string &value = source.value.empty() ? source.tag : source.value;
        if (!value.empty())
        {
            // The name is what is being chosen: the value takes what the name leaves, and never less than two fifths.
            const float size = options ? kTypeHint : kTypeMeta;
            const Face &face = options ? g.f.regular : g.f.semibold;
            const float free = right - left;
            const float room = std::max(free * 0.4f, free - measure(g.f.semibold, source.label, kTypeRow) - 18.0f);
            right -= text(g, face, fit(face, value, size, room), right, row.cy() + size * 0.36f, size, quiet, gfx::Align::right) + 18.0f;
        }
        text(g, g.f.semibold, fit(g.f.semibold, source.label, kTypeRow, std::max(40.0f, right - left)), left, row.cy() + kTypeRow * 0.34f, kTypeRow, ink);
    };
    tabs_.style.kind = ui::TabKind::pill;
    tabs_.style.width = ui::TabWidth::fill;
    tabs_.style.height = 54.0f;
    tabs_.style.text_size = kTypeClock;
    tabs_.set_focused(false);
    ring_.style.shape = ui::CountdownShape::ring;
    ring_.style.thickness = 6.0f;
    ring_.style.warning_at = 0.0f;
    ring_.style.danger_at = 0.0f;
    ring_.style.pulse_at = 0.0f;
    ring_.style.on_panel = true;
    ring_.set_bounds({kNextCard.x + kNextCard.w - 110.0f, kNextCard.y + 28.0f, 70.0f, 70.0f});
    dialog_.style.frosted = true;
    dialog_.style.frost = 0.6f;
    dialog_.style.scrim = 0.5f;
    dialog_.style.width = 760.0f;
    dialog_.style.centered = true;
    dialog_.set_bounds(kFull);
    toasts_.style.anchor = ui::ToastAnchor::top_right;
    toasts_.style.width = 560.0f;
    toasts_.style.margin = 60.0f;
    toasts_.style.frosted = true;
    toasts_.style.frost = 0.62f;
    toasts_.set_bounds({0.0f, 76.0f, gfx::kVirtualWidth - 36.0f, gfx::kVirtualHeight - 76.0f});
    wear_look();
}

void Player::set_look(const Look &look)
{
    if (look_ == &look)
        return;
    look_ = &look;
    wear_look();
}

void Player::wear_look()
{
    const Look &look = *look_;
    wear(list_.style, look, reduced_);
    wear(tabs_.style, look, reduced_);
    wear(ring_.style, look, reduced_);
    ring_.style.color = look.accent;
    wear(dialog_.style, look, reduced_);
    wear(toasts_.style, look, reduced_);
}

G Player::make_g(gfx::DrawList &list, std::uint32_t glass) const
{
    return G{list, fonts_, *look_, glass, time_, reduced_};
}

float Player::share(double seconds) const
{
    return view_.duration > 0.0 ? static_cast<float>(std::clamp(seconds / view_.duration, 0.0, 1.0)) : 0.0f;
}

int Player::chapter_at(double seconds) const
{
    int index = -1;
    for (std::size_t i = 0; i < view_.chapters.size(); ++i)
    {
        if (seconds + 0.001 >= view_.chapters[i].start)
            index = static_cast<int>(i);
    }
    return index;
}

Picture Player::picture(const std::string &url, PictureSize size) const
{
    return images_ != nullptr && !url.empty() ? images_->get(url, size) : Picture{};
}

Rect Player::sheet_rect() const
{
    return sheet_.kind == "captions" ? kCaptionSheet : kOptionSheet;
}

Rect Player::card_rect(int index, float focus) const
{
    // The shelf shows five cards; a focus past the fifth slides the row under it.
    const float count = static_cast<float>(up_next_.cards.size());
    const float slide = std::clamp(focus - static_cast<float>(kUpNextShown - 1), 0.0f, std::max(0.0f, count - static_cast<float>(kUpNextShown)));
    return {kEdge + kUpNextPitch * (static_cast<float>(index) - slide), 744.0f, kUpNextCardW, kUpNextCardH};
}

Rect Player::lit_control() const
{
    return {kEdge, kMarksY, kMark, kMark};
}

void Player::build_sheet()
{
    const bool captions = sheet_.kind == "captions";
    const Rect sheet = sheet_rect();
    const float left = sheet.x + 40.0f;
    float top = 148.0f;
    if (captions)
    {
        list_.style.row_height = 66.0f;
        list_.style.value_size = kTypeMeta;
        list_.style.leading_width = 0.0f;
        if (!sheet_.tabs.empty())
        {
            std::vector<ui::TabItem> tabs;
            for (const std::string &label : sheet_.tabs)
                tabs.push_back({label});
            bool same = tabs.size() == tabs_.tabs().size();
            for (std::size_t i = 0; same && i < tabs.size(); ++i)
                same = tabs[i].label == tabs_.tabs()[i].label;
            if (!same)
            {
                tabs_.set_tabs(std::move(tabs));
                tabs_.set_active(sheet_.tab, true);
            }
            tabs_.set_bounds({left, 150.0f, 508.0f, 54.0f});
            top = 224.0f;
        }
        else if (!sheet_.pills.empty())
            top = 204.0f;
        list_.set_bounds({left, top, 508.0f, 864.0f - top});
    }
    else
    {
        bool icons = false;
        for (const Row &row : sheet_.rows)
            icons = icons || !row.icon.empty();
        list_.style.row_height = 70.0f;
        list_.style.value_size = kTypeHint;
        list_.style.leading_width = icons ? 50.0f : 0.0f;
        list_.set_bounds({left, top, 480.0f, 660.0f});
    }
    const std::string key = key_of(sheet_);
    const int focus = focused_row(sheet_);
    if (key != sheet_key_)
    {
        std::vector<ui::ListItem> items;
        for (std::size_t i = 0; i < sheet_.rows.size(); ++i)
        {
            ui::ListItem item;
            item.title = sheet_.rows[i].label;
            item.disabled = sheet_.rows[i].disabled;
            item.tag = static_cast<int>(i);
            items.push_back(std::move(item));
        }
        list_.set_items(std::move(items));
        sheet_key_ = key;
        if (focus >= 0)
            list_.set_focus(focus, true);
    }
    else if (focus >= 0 && focus != list_.focus())
    {
        list_.set_focus(focus, false);
    }
    sheet_focus_ = focus;
}

void Player::sounds(const PlayerView &before)
{
    // Beside a playing video the interface speaks quietly: what follows is the
    // "Playing" column of sound-design.md, 6 dB under the browser.
    constexpr float kQuiet = 0.5f;
    if (view_.paused != before.paused && !view_.scrubbing)
        feedback_.play(view_.paused ? audio::Cue::toggle : audio::Cue::resume, view_.paused ? 0.92f : 1.0f);
    const float thumb_x = kEdge + kBarWidth * share(view_.scrub_target);
    if (view_.scrubbing)
    {
        const int chapter = chapter_at(view_.scrub_target);
        if (!before.scrubbing)
        {
            feedback_.play(audio::Cue::tick, 1.0f, ui::pan_for_x(thumb_x), 0.8f * kQuiet);
        }
        else if (chapter != scrub_chapter_ && chapter >= 0)
        {
            // A detent at every chapter mark, like a dial.
            feedback_.play(audio::Cue::tick, 1.0f + 0.04f * static_cast<float>(chapter), ui::pan_for_x(thumb_x), kQuiet);
            feedback_.rumble(0.18f, 0.03f);
        }
        scrub_chapter_ = chapter;
    }
    else if (before.scrubbing)
    {
        // Landed where the target was, or went back to where it came from.
        const bool landed = std::fabs(view_.position - before.scrub_target) < 3.0;
        feedback_.play(landed ? audio::Cue::select : audio::Cue::back, landed ? 1.1f : 1.0f,
                       ui::pan_for_x(kEdge + kBarWidth * share(view_.position)), kQuiet);
    }
    if (view_.sheet.open != before.sheet.open)
        feedback_.play(view_.sheet.open ? audio::Cue::modal_open : audio::Cue::modal_close, 1.0f, view_.sheet.open ? 0.4f : 0.2f, kQuiet);
    else if (view_.sheet.open && (focused_row(view_.sheet) != focused_row(before.sheet) || view_.sheet.first != before.sheet.first))
        feedback_.play(audio::Cue::focus, 1.0f, 0.4f, kQuiet);
    else if (view_.sheet.open && view_.sheet.tab != before.sheet.tab)
        feedback_.play(audio::Cue::tab, 1.0f, 0.4f, kQuiet);
    if (view_.up_next.open != before.up_next.open)
        feedback_.play(view_.up_next.open ? audio::Cue::modal_open : audio::Cue::modal_close, 1.0f, 0.0f, kQuiet);
    else if (view_.up_next.open && (view_.up_next.focus != before.up_next.focus || view_.up_next.first != before.up_next.first))
        feedback_.play(audio::Cue::focus, 1.0f, ui::pan_for_x(card_rect(view_.up_next.focus, static_cast<float>(view_.up_next.focus)).cx()), kQuiet);
    if ((view_.chat.side != 0) != (before.chat.side != 0))
        feedback_.play(view_.chat.side != 0 ? audio::Cue::modal_open : audio::Cue::modal_close, 1.0f, view_.chat.side == 1 ? -0.4f : 0.4f, kQuiet);
    if (view_.notice.shown && (!before.notice.shown || before.notice.title != view_.notice.title) &&
        (view_.notice.kind == "skip" || view_.notice.kind == "ask" || view_.notice.kind == "error"))
        feedback_.play(audio::Cue::notify, view_.notice.kind == "error" ? 0.9f : 1.1f, 0.4f, 0.7f * kQuiet);
    if (view_.next.shown != before.next.shown)
        feedback_.play(view_.next.shown ? audio::Cue::modal_open : audio::Cue::modal_close, 1.0f, 0.3f, kQuiet);
    else if (view_.next.shown && view_.next.focus != before.next.focus)
        feedback_.play(audio::Cue::focus, 1.0f, 0.3f, kQuiet);
}

void Player::update(const PlayerView &view, float dt)
{
    const PlayerView before = std::move(view_);
    view_ = view;
    feedback_.clear();
    time_ += dt;
    // Another video, or the first frame: nothing glides in from the one before.
    const bool fresh = !started_ || before.title != view_.title || before.live != view_.live;
    if (reduced_ != view_.reduced_motion)
    {
        reduced_ = view_.reduced_motion;
        wear_look();
    }
    const auto w = [&](float omega) { return reduced_ ? 60.0f : omega; };

    // ---- the controls ----
    chrome_.target = view_.chrome_hidden ? 0.0f : tween::clamp01(view_.chrome);
    paused_.target = view_.paused && !view_.scrubbing && !view_.loading ? 1.0f : 0.0f;
    loading_.target = view_.loading ? 1.0f : 0.0f;
    scrub_.target = view_.scrubbing ? 1.0f : 0.0f;
    progress_.target = share(view_.position);
    target_.target = share(view_.scrubbing ? view_.scrub_target : view_.position);
    dim_.target = tween::clamp01(view_.dim);
    stats_.target = view_.stats.shown ? 1.0f : 0.0f;
    if (fresh)
    {
        chrome_.snap(0.0f);
        progress_.snap(progress_.target);
        target_.snap(target_.target);
        paused_.snap(paused_.target);
        scrub_.snap(scrub_.target);
        scrub_chapter_ = chapter_at(view_.scrub_target);
    }
    else if (view_.paused != before.paused && !reduced_)
    {
        pause_pop_.trigger();
    }
    chrome_.update(dt, w(20.0f));
    paused_.update(dt, w(16.0f));
    loading_.update(dt, w(14.0f));
    scrub_.update(dt, w(20.0f));
    progress_.update(dt, w(14.0f));
    target_.update(dt, w(20.0f));
    dim_.update(dt, w(6.0f));
    stats_.update(dt, w(16.0f));
    pause_pop_.update(dt, 8.0f);

    // ---- a sheet from the right ----
    sheet_amount_.target = view_.sheet.open ? 1.0f : 0.0f;
    if (view_.sheet.open)
    {
        const bool opened = fresh || !before.sheet.open || before.sheet.kind != view_.sheet.kind;
        sheet_ = view_.sheet;
        if (opened)
        {
            sheet_key_.clear();
            list_.enter();
            if (!sheet_.tabs.empty())
                tabs_.set_active(sheet_.tab, true);
        }
        else if (!sheet_.tabs.empty() && tabs_.active() != sheet_.tab)
        {
            tabs_.set_active(sheet_.tab, false);
        }
        build_sheet();
        if (fresh)
            sheet_amount_.snap(1.0f);
    }
    sheet_amount_.update(dt, w(13.0f));
    list_.update(dt);
    tabs_.update(dt);

    // ---- the shelf of what is next ----
    up_next_amount_.target = view_.up_next.open ? 1.0f : 0.0f;
    if (view_.up_next.open)
    {
        if (fresh || !before.up_next.open)
            up_next_pos_.snap(static_cast<float>(view_.up_next.focus));
        up_next_ = view_.up_next;
        if (fresh)
            up_next_amount_.snap(1.0f);
    }
    up_next_pos_.target = static_cast<float>(up_next_.focus);
    up_next_pos_.update(dt, w(20.0f));
    up_next_amount_.update(dt, w(13.0f));

    // ---- the chat ----
    const bool chat_on = view_.chat.side != 0;
    chat_amount_.target = chat_on ? 1.0f : 0.0f;
    theatre_.target = chat_on && view_.chat.theatre ? 1.0f : 0.0f;
    if (chat_on)
    {
        const std::string newest = view_.chat.lines.empty() ? std::string() : view_.chat.lines.back().author + '\n' + view_.chat.lines.back().text;
        if (newest != chat_newest_)
        {
            chat_newest_ = newest;
            chat_fresh_ = fresh || before.chat.side == 0 ? 10.0f : 0.0f; // the newest line rises in; a chat that opens is simply there
        }
        chat_ = view_.chat;
        if (fresh)
        {
            chat_amount_.snap(1.0f);
            theatre_.snap(theatre_.target);
        }
    }
    chat_fresh_ += dt;
    chat_amount_.update(dt, w(13.0f));
    theatre_.update(dt, w(13.0f));

    // ---- the next video ----
    next_amount_.target = view_.next.shown ? 1.0f : 0.0f;
    if (view_.next.shown)
    {
        if (fresh || !before.next.shown)
            ring_.start(std::max(0.1f, view_.next.seconds_total));
        next_ = view_.next;
        ring_.set_remaining(next_.seconds_left);
        ring_.update(dt, feedback_);
        if (fresh)
            next_amount_.snap(1.0f);
    }
    next_amount_.update(dt, w(13.0f));

    // ---- a notice ----
    if (view_.notice.shown)
    {
        if (!before.notice.shown || before.notice.title != view_.notice.title || before.notice.kind != view_.notice.kind)
            notice_in_ = fresh ? 1.0f : 0.0f;
        notice_ = view_.notice;
        notice_in_ += dt;
        notice_out_ = 0.0f;
    }
    else if (notice_out_ < 10.0f)
    {
        notice_out_ += dt;
    }

    // ---- the dialog and, outside the player, toasts ----
    if (view_.dialog.shown)
    {
        if (!dialog_.is_open())
        {
            dialog_.open(content_of(view_.dialog), feedback_);
        }
        else if (before.dialog.title != view_.dialog.title || before.dialog.body != view_.dialog.body ||
                 before.dialog.buttons.size() != view_.dialog.buttons.size())
        {
            dialog_.set_content(content_of(view_.dialog));
        }
        if (dialog_.focus() != view_.dialog.focus)
        {
            if (before.dialog.shown)
                feedback_.play(audio::Cue::focus);
            dialog_.set_focus(view_.dialog.focus);
        }
    }
    else if (dialog_.is_open())
    {
        dialog_.close(feedback_);
    }
    dialog_.update(dt);
    const std::string toast_key = view_.toast.shown ? view_.toast.title + '\n' + view_.toast.message : std::string();
    if (toast_key != toast_key_)
    {
        if (toast_id_ != 0)
            toasts_.dismiss(toast_id_);
        toast_id_ = 0;
        if (view_.toast.shown)
        {
            const ui::StatusKind kind = view_.toast.kind == 2 ? ui::StatusKind::danger : view_.toast.kind == 3 ? ui::StatusKind::success : ui::StatusKind::info;
            toast_id_ = toasts_.push(kind, view_.toast.title, view_.toast.message, -1.0f);
        }
        toast_key_ = toast_key;
    }
    toasts_.update(dt, feedback_);

    if (!fresh)
        sounds(before);
    else if (!view_.title.empty() && started_)
        feedback_.play(audio::Cue::launch, 1.0f, 0.0f, 0.8f); // the one start of a video
    if (!feedback_.cues.empty())
        last_cue_ = audio::cue_name(feedback_.cues.back().cue);
    started_ = true;
}

Player::Motion Player::motion() const
{
    Motion m;
    m.theatre = tween::smoothstep(tween::clamp01(theatre_.value));
    m.chat = tween::clamp01(chat_amount_.value);
    m.sheet = tween::clamp01(sheet_amount_.value);
    m.up_next = tween::clamp01(up_next_amount_.value);
    m.next = tween::clamp01(next_amount_.value);
    // The controls give the picture to the theatre layout and to the card of the next video.
    m.hud = tween::clamp01(chrome_.value) * (1.0f - m.theatre) * (1.0f - m.next);
    m.scrub = tween::clamp01(scrub_.value) * m.hud;
    m.foot = m.hud * (1.0f - m.up_next);
    m.rise = reduced_ ? 0.0f : 30.0f * (1.0f - tween::clamp01(chrome_.value));
    const float in = tween::cubic_out(notice_in_ / 0.4f);
    const float out = tween::smoothstep(notice_out_ / 0.4f);
    m.notice = in * (1.0f - out);
    m.notice_slide = reduced_ ? 0.0f : 60.0f * (1.0f - in) + 40.0f * out;
    m.dialog = dialog_.visible() ? 1.0f : 0.0f;
    return m;
}

Rect Player::video_rect() const
{
    return lerp_rect(kFull, kTheatre, tween::smoothstep(tween::clamp01(theatre_.value)));
}

float Player::video_radius() const
{
    return 22.0f * tween::smoothstep(tween::clamp01(theatre_.value));
}

bool Player::animating() const
{
    return true; // the live halo breathes, a notice drains, the clock of the video runs
}

Rect Player::notice_rect(const Motion &m) const
{
    // As wide as what it says, between the board's width and most of the picture.
    const bool two = notice_.second.button != ui::Button::none && !notice_.second.label.empty();
    const bool one = notice_.action.button != ui::Button::none && !notice_.action.label.empty();
    const std::string &first = notice_.label.empty() ? notice_.title : notice_.label;
    const std::string &second = notice_.label.empty() ? notice_.detail : notice_.title;
    float text_w = std::max(measure(fonts_.semibold, first, notice_.label.empty() ? kTypeRow : kTypeOver, notice_.label.empty() ? 0.0f : 3.2f),
                            measure(notice_.label.empty() ? fonts_.regular : fonts_.semibold, second, notice_.label.empty() ? kTypeMeta : kTypeRow));
    // What it offers stands at the right: a glyph, its label, and room before the text.
    float actions = 0.0f;
    if (one)
        actions += ui::button_width(notice_.action.button, 34.0f) + 12.0f + measure(fonts_.semibold, notice_.action.label, kTypeClock) + 24.0f;
    if (two)
        actions += ui::button_width(notice_.second.button, 34.0f) + 12.0f + measure(fonts_.semibold, notice_.second.label, kTypeClock) + 28.0f;
    const float width = std::clamp(74.0f + text_w + actions + 44.0f, 560.0f, 1180.0f);
    // A sheet from the right pushes it aside; the shelf lifts it.
    const float right = tween::lerp(kRight, sheet_rect().x - 40.0f, m.sheet);
    const float lifted = 168.0f * m.up_next + 190.0f * m.next;
    return {right - width, kNoticeY - lifted, width, kNoticeH};
}

FocusInfo Player::focus() const
{
    FocusInfo info;
    const Motion m = motion();
    if (dialog_.is_open())
    {
        ui::Canvas canvas{scratch_, fonts_, 0, time_};
        const int at = dialog_.focus();
        info.id = "dialog/" + std::to_string(at);
        info.text = at >= 0 && at < static_cast<int>(view_.dialog.buttons.size()) ? view_.dialog.buttons[static_cast<std::size_t>(at)].label : view_.dialog.title;
        info.rect = dialog_.panel_rect(canvas);
        info.index = at;
        info.total = static_cast<int>(view_.dialog.buttons.size());
        return info;
    }
    if (view_.next.shown)
    {
        info.id = view_.next.focus == 0 ? "next/play" : "next/cancel";
        info.text = view_.next.card.title;
        info.rect = view_.next.focus == 0 ? Rect{kNextCard.x + 40.0f, kNextCard.y + 296.0f, 300.0f, 64.0f}
                                          : Rect{kNextCard.x + 356.0f, kNextCard.y + 296.0f, 200.0f, 64.0f};
        info.index = view_.next.focus;
        info.total = 2;
        return info;
    }
    if (view_.sheet.open)
    {
        const int at = focused_row(view_.sheet);
        info.id = "sheet/" + std::to_string(view_.sheet.first + std::max(at, 0));
        info.text = at >= 0 ? view_.sheet.rows[static_cast<std::size_t>(at)].label : view_.sheet.title;
        info.rect = at >= 0 ? list_.row_rect(at) : Rect{sheet_rect().x, 0.0f, gfx::kVirtualWidth - sheet_rect().x, gfx::kVirtualHeight};
        info.index = view_.sheet.first + std::max(at, 0);
        info.total = view_.sheet.total > 0 ? view_.sheet.total : static_cast<int>(view_.sheet.rows.size());
        return info;
    }
    if (view_.up_next.open)
    {
        const int at = std::clamp(view_.up_next.focus, 0, std::max(0, static_cast<int>(view_.up_next.cards.size()) - 1));
        info.id = "upnext/" + std::to_string(view_.up_next.first + at);
        info.text = view_.up_next.cards.empty() ? view_.up_next.message : view_.up_next.cards[static_cast<std::size_t>(at)].title;
        info.rect = card_rect(at, static_cast<float>(at));
        info.index = view_.up_next.first + at;
        info.total = view_.up_next.total > 0 ? view_.up_next.total : static_cast<int>(view_.up_next.cards.size());
        return info;
    }
    if (view_.notice.shown && view_.notice.kind == "ask")
    {
        info.id = "sponsor-review";
        info.text = view_.notice.title;
        info.rect = notice_rect(m);
        info.index = 0;
        info.total = 1;
        return info;
    }
    // The player has no cursor: Cross acts on the lit mark.
    info.id = view_.scrubbing ? "osd-scrub" : view_.paused ? "osd-paused" : "osd-playing";
    info.text = view_.title;
    info.rect = view_.chat.side != 0 && view_.chat.theatre ? kTheatre : view_.chrome > 0.5f && !view_.chrome_hidden && !view_.live ? lit_control() : kFull;
    info.index = 0;
    info.total = 1;
    return info;
}

std::string Player::snapshot_json() const
{
    std::string out = "{";
    json_field(out, "interface", "afterglow");
    json_field(out, "look", look_->id);
    json_field(out, "screen", "player");
    out += "\"dialog\":";
    out += dialog_.is_open() ? "1," : "0,";
    const FocusInfo at = focus();
    out += "\"focus\":{";
    json_field(out, "id", at.id);
    json_field(out, "text", at.text);
    char buffer[160];
    std::snprintf(buffer, sizeof buffer, "\"index\":%d,\"total\":%d,\"rect\":[%.1f,%.1f,%.1f,%.1f]},", at.index, at.total, at.rect.x, at.rect.y,
                  at.rect.w, at.rect.h);
    out += buffer;
    out += "\"focusable\":[";
    if (view_.sheet.open)
    {
        for (std::size_t i = 0; i < view_.sheet.rows.size(); ++i)
        {
            out += i == 0 ? "{" : ",{";
            json_field(out, "id", "sheet/" + std::to_string(view_.sheet.first + static_cast<int>(i)));
            json_field(out, "text", view_.sheet.rows[i].label, false);
            out += "}";
        }
    }
    else if (view_.up_next.open)
    {
        for (std::size_t i = 0; i < view_.up_next.cards.size(); ++i)
        {
            out += i == 0 ? "{" : ",{";
            json_field(out, "id", "upnext/" + std::to_string(view_.up_next.first + static_cast<int>(i)));
            json_field(out, "text", view_.up_next.cards[i].title, false);
            out += "}";
        }
    }
    out += "],\"state\":{";
    const auto number = [&](const char *name, long value) { json_field(out, name, std::to_string(value)); };
    json_field(out, "title", view_.title);
    json_field(out, "controls", view_.chrome_hidden || view_.chrome <= 0.01f ? "hidden" : "shown");
    json_field(out, "playback", view_.loading ? "loading" : view_.scrubbing ? "seeking" : view_.paused ? "paused" : "playing");
    number("live", view_.live ? 1 : 0);
    number("position", static_cast<long>(view_.position));
    number("target", static_cast<long>(view_.scrubbing ? view_.scrub_target : view_.position));
    number("duration", static_cast<long>(view_.duration));
    number("chapters", static_cast<long>(view_.chapters.size()));
    number("chapter", chapter_at(view_.scrubbing ? view_.scrub_target : view_.position) + 1);
    number("segments", static_cast<long>(view_.segments.size()));
    number("preview", static_cast<long>(view_.preview.size()));
    number("captions", static_cast<long>(view_.captions.lines.size() + view_.captions.second.size()));
    json_field(out, "sheet", view_.sheet.open ? view_.sheet.kind : "");
    number("rows", view_.sheet.open ? static_cast<long>(view_.sheet.rows.size()) : 0);
    number("up_next", view_.up_next.open ? 1 : 0);
    number("cards", view_.up_next.open ? static_cast<long>(view_.up_next.cards.size()) : 0);
    json_field(out, "chat", view_.chat.side == 0 ? "off" : view_.chat.theatre ? "theatre" : view_.chat.side == 1 ? "left" : "right");
    number("chat_lines", static_cast<long>(view_.chat.lines.size()));
    json_field(out, "notice", view_.notice.shown ? view_.notice.kind : "");
    number("next", view_.next.shown ? 1 : 0);
    number("toast", view_.toast.shown ? 1 : 0);
    number("details", view_.stats.shown ? 1 : 0);
    number("dim", static_cast<long>(view_.dim * 100.0f + 0.5f));
    number("reduced", reduced_ ? 1 : 0);
    json_field(out, "cue", last_cue_, false);
    out += "}}";
    return out;
}

void Player::draw(gfx::DrawList &world, gfx::DrawList &ui, std::uint32_t glass, unsigned layers) const
{
    const Motion m = motion();
    // A dialog frosts the whole picture, so while one is up the controls are
    // part of what gets blurred; otherwise only the shade is, and panels frost that.
    const bool flatten = m.dialog > 0.0f && (layers & kDialog) != 0 && &world != &ui;
    gfx::DrawList &surface = flatten ? world : ui;
    if ((layers & kControls) != 0)
        draw_ground(world, m);
    G g = make_g(surface, flatten ? 0u : glass);
    if ((layers & kControls) != 0)
    {
        draw_captions(g, m);
        draw_header(g, m);
        draw_foot(g, m);
        draw_seek(g, m);
        draw_centre(g, m);
        draw_chat(g, m);
        draw_up_next(g, m);
        draw_sheet(g, m);
        draw_next(g, m);
    }
    if ((layers & kNotice) != 0)
        draw_notice(g, m);
    if ((layers & kControls) != 0)
    {
        draw_stats(g, m);
        if (view_.fps >= 0)
        {
            char fps[24];
            std::snprintf(fps, sizeof fps, "%d FPS", view_.fps);
            const Rect pill{gfx::kVirtualWidth - 180.0f, 20.0f, 140.0f, 36.0f};
            g.l.rounded_rect(pill, 18.0f, kBlack.with_alpha(0.6f));
            text(g, g.f.mono, fps, pill.cx(), pill.y + 25.0f, kTypeSmall, kOnVideo, gfx::Align::center);
        }
        // Screen dimming and screen off: over everything the player shows.
        if (dim_.value > 0.004f)
            surface.rounded_rect(kFull, 0.0f, kBlack.with_alpha(tween::clamp01(dim_.value)));
    }
    ui::Canvas canvas{ui, fonts_, glass, time_};
    if ((layers & kToast) != 0)
        toasts_.draw(canvas);
    if ((layers & kDialog) != 0)
        dialog_.draw(canvas);
}

} // namespace afterglow::player
