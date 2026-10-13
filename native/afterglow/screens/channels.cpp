// ProsperoTube Afterglow - the channels a profile follows, as tiles.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Board 28 of the proposal. With an account the tiles are "library:channels"
// (each row a channel with the facts subscribers, unseen and live), in the
// order the provider keeps for the profile: the three orders are the rows of
// "settings:channel-sort" (fact selected), choosing one is listing its row
// (Data::act), and "channels:watched" clears the new marks. Without an account,
// and when the account cannot be reached, the tiles are "subscriptions": the
// channels followed on this console, which need no network.

#include "afterglow/screens/channel_family.hpp"

namespace afterglow
{

namespace
{

using namespace family;

constexpr int kAcross = 6;
constexpr float kTileW = 258.0f;
constexpr float kTileH = 340.0f;
constexpr float kTilePitchX = 278.0f;
constexpr float kTilePitchY = 364.0f;
constexpr float kTilesY = 204.0f;
constexpr float kBannerY = 196.0f; // the notice above the tiles when the account cannot be reached
constexpr float kBannerH = 72.0f;
constexpr float kChipY = 150.0f;
constexpr float kChipsRight = kRight - 130.0f; // the clock keeps the corner

enum class Mode : std::uint8_t
{
    loading,
    account, // the channels the account subscribes to
    local,   // no account: the channels followed on this console
    kept,    // the account cannot be reached: the same, with the reason above them
    empty,   // an account that subscribes to nothing
    invite,  // no account and nothing followed
    trouble, // nothing arrived and nothing is kept
};

const char *mode_name(Mode mode)
{
    switch (mode)
    {
    case Mode::loading:
        return "loading";
    case Mode::account:
        return "account";
    case Mode::local:
        return "local";
    case Mode::kept:
        return "kept";
    case Mode::empty:
        return "empty";
    case Mode::invite:
        return "invite";
    case Mode::trouble:
        return "trouble";
    }
    return "";
}

enum class Zone : std::uint8_t
{
    order,
    tiles,
    notice,
};

// The provider's three orders ("settings:channel-sort:<n>"), in the words of the board.
const char *order_name(int number)
{
    return number == 2 ? "new" : number == 1 ? "name" : "account";
}
const char *order_label(int number)
{
    return number == 2 ? tr("New first") : number == 1 ? tr("A to Z") : tr("Account order");
}
const char *order_phrase(int number)
{
    return number == 2 ? tr("new uploads first") : number == 1 ? tr("A to Z") : tr("in your account's order");
}

class Channels final : public Screen
{
  public:
    ScreenId id() const override
    {
        return ScreenId::channels;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        profile_ = context.services.data.profile();
        if (restored)
        {
            account_.restart();
            local_.restart();
            read(context);
            return;
        }
        route_ = route;
        account_ = Feed{};
        local_ = Feed{};
        local_.start("subscriptions");
        if (profile_.signed_in)
            account_.start("library:channels");
        tiles_.clear();
        orders_.clear();
        orders_known_ = false;
        orders_chosen_ = false;
        mode_ = Mode::loading;
        zone_ = Zone::tiles;
        focus_ = 0;
        chip_ = 0;
        choice_ = 0;
        cleared_ = false;
        doing_ = 0;
        act_ = Act{};
        age_ = 0.0f;
        scroll_.snap(0.0f);
        ring_set_ = false;
        read(context);
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        read(context);
        finish(context);
        sheet_.update(context);
        if (sheet_.take_changed())
        {
            context.services.data.forget("library:channels");
            context.services.data.forget("subscriptions");
        }
        lay_out(context.services.fonts);
        if (context.top)
        {
            handle(input, context);
            lay_out(context.services.fonts);
            const Tile *tile = zone_ == Zone::tiles && !tiles_.empty() ? &tiles_[static_cast<std::size_t>(focus_)] : nullptr;
            context.nav.ambient(tile != nullptr ? tile->channel.avatar : std::string());
        }
        advance(context);
        // The faces in view are asked for as soon as they are known (see Channel::ask_pictures).
        const int from = std::max(focus_ / kAcross - 1, 0) * kAcross;
        for (int i = from; i < std::min(static_cast<int>(tiles_.size()), from + 4 * kAcross); ++i)
            context.services.images.get(tiles_[static_cast<std::size_t>(i)].channel.avatar, PictureSize::avatar);
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        Travel travel(g, phase);
        draw_head(g);
        switch (mode_)
        {
        case Mode::loading:
            draw_bones(g);
            break;
        case Mode::empty:
        case Mode::invite:
        case Mode::trouble:
        {
            Enter enter(g, arrive(age_, 2, 0.07f, 0.55f), 26.0f);
            notice_.draw(g, notice_icon(), notice_title(), notice_body(), choices_, choice_pos_.value, zone_ == Zone::notice && !sheet_.open());
            break;
        }
        default:
            draw_tiles(g, context);
            break;
        }
        foot_scrim(g);
        sheet_.draw(g);
    }

    std::string title() const override
    {
        return tr("Channels");
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[4];
        std::size_t count = 0;
        if (sheet_.open())
            return sheet_.hints();
        if (zone_ == Zone::notice)
        {
            items[count++] = {ui::Button::cross, tr("Choose")};
            items[count++] = {ui::Button::circle, tr("Back")};
            return {items, count};
        }
        items[count++] = {ui::Button::cross, zone_ == Zone::order ? tr("Choose") : tr("Open")};
        if (mode_ == Mode::account)
        {
            if (orders_.size() > 1)
                items[count++] = {ui::Button::square, tr("Order")};
            items[count++] = {ui::Button::triangle, tr("Mark all as watched")};
        }
        else if (mode_ == Mode::kept)
            items[count++] = {ui::Button::triangle, tr("Try again")};
        else if (mode_ == Mode::local)
            items[count++] = {ui::Button::options, tr("Sign in")};
        if (zone_ == Zone::tiles)
            items[count++] = {ui::Button::touchpad, tr("Actions")};
        return {items, count};
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (sheet_.open())
            return sheet_.focus();
        if (zone_ == Zone::notice && notice_.count > 0)
        {
            info.id = std::string("state/") + choice_names_[choice_];
            info.text = choices_[choice_].label;
            info.rect = notice_.buttons[static_cast<std::size_t>(choice_)];
            info.index = choice_;
            info.total = notice_.count;
        }
        else if (zone_ == Zone::order && !chips_.empty())
        {
            const Chip &chip = chips_[static_cast<std::size_t>(chip_)];
            info.id = std::string("order/") + order_name(chip.number);
            info.text = order_label(chip.number);
            info.rect = chip.rect;
            info.index = chip_;
            info.total = static_cast<int>(chips_.size());
        }
        else if (!tiles_.empty())
        {
            info.id = "channel/" + std::to_string(focus_);
            info.text = tiles_[static_cast<std::size_t>(focus_)].channel.name;
            info.rect = tile_rect(focus_, scroll_.target);
            info.index = focus_;
            info.total = static_cast<int>(tiles_.size());
        }
        else
        {
            info.id = "state/loading";
            info.text = tr("Channels");
            info.rect = tile_rect(0, 0.0f);
            info.total = 1;
        }
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        for (const Chip &chip : chips_)
            out.emplace_back(std::string("order/") + order_name(chip.number), order_label(chip.number));
        for (std::size_t i = 0; i < tiles_.size() && i < 36; ++i)
            out.emplace_back("channel/" + std::to_string(i), tiles_[i].channel.name);
        if (zone_ == Zone::notice)
            for (int i = 0; i < notice_.count; ++i)
                out.emplace_back(std::string("state/") + choice_names_[i], choices_[i].label);
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("mode", mode_name(mode_));
        out.set("load", load_name(profile_.signed_in ? account_.load : local_.load));
        out.set("guest", profile_.signed_in ? 0 : 1);
        out.set("zone", zone_ == Zone::order ? "order" : zone_ == Zone::tiles ? "tiles" : "notice");
        out.set("channels", static_cast<int>(tiles_.size()));
        out.set("pages", list().taken);
        out.set("order", selected_order() >= 0 ? order_name(selected_order()) : "");
        out.set("action", doing_ != 0 ? "pending" : outcome_);
        out.set("marks", marks());
        out.set("scroll", static_cast<int>(scroll_.target));
        sheet_.snapshot(out);
        std::vector<std::string> names;
        for (std::size_t i = 0; i < tiles_.size() && i < 18; ++i)
            names.push_back(tiles_[i].channel.name);
        out.list("channels", std::move(names));
    }

  private:
    struct Tile
    {
        model::Channel channel;
        std::string line; // subscribers, as YouTube words them
        int unseen = 0;
        bool live = false;
    };
    struct Chip
    {
        int number = 0; // the provider's: 0 account order, 1 by name, 2 new first
        std::string row;
        bool selected = false;
        Rect rect{};
    };

    bool uses_local() const
    {
        return mode_ == Mode::local || mode_ == Mode::kept || (!profile_.signed_in && mode_ != Mode::account);
    }
    const Feed &list() const
    {
        return uses_local() ? local_ : account_;
    }
    Feed &list()
    {
        return uses_local() ? local_ : account_;
    }
    int selected_order() const
    {
        for (const Chip &chip : orders_)
        {
            if (chip.selected)
                return chip.number;
        }
        return -1;
    }
    int marks() const
    {
        int count = 0;
        for (const Tile &tile : tiles_)
            count += tile.unseen > 0 && !cleared_ ? 1 : 0;
        return count;
    }
    Rect tile_rect(int index, float scroll) const
    {
        return {kLeft + static_cast<float>(index % kAcross) * kTilePitchX,
                kTilesY + (mode_ == Mode::kept ? kBannerH + 24.0f : 0.0f) + static_cast<float>(index / kAcross) * kTilePitchY - scroll, kTileW, kTileH};
    }

    // ---- data ---------------------------------------------------------------------------

    static bool waiting(const Feed &feed)
    {
        return feed.load == model::Load::loading || feed.load == model::Load::idle;
    }
    static bool failed(const Feed &feed)
    {
        return feed.load == model::Load::failed || feed.load == model::Load::offline;
    }

    void read(UpdateContext &context)
    {
        Data &data = context.services.data;
        bool changed = local_.read(data);
        if (profile_.signed_in)
        {
            if (!account_.open)
                account_.start("library:channels");
            changed = account_.read(data) || changed;
            // The orders the provider offers, and which one it keeps for this profile.
            // Once an order was chosen in this visit the provider's answer to that is the newer word.
            const Page &orders = data.catalog("settings:channel-sort");
            if (orders.load == model::Load::ready && !orders_known_ && !orders_chosen_)
            {
                orders_known_ = true;
                read_orders(orders.rows);
            }
            else if (orders.load == model::Load::loading)
                orders_known_ = false;
        }
        Mode mode = Mode::loading;
        if (profile_.signed_in && !failed(account_))
            mode = waiting(account_) ? Mode::loading : account_.channels.empty() ? Mode::empty : Mode::account;
        else if (waiting(local_))
            mode = Mode::loading;
        else if (!local_.channels.empty())
            mode = profile_.signed_in ? Mode::kept : Mode::local;
        else
            mode = profile_.signed_in ? Mode::trouble : Mode::invite;
        if (mode != mode_)
        {
            const bool arrived = mode_ == Mode::loading;
            mode_ = mode;
            changed = true;
            if (arrived)
                age_ = std::min(age_, 0.1f);
        }
        if (changed)
            read_tiles();
    }

    void read_orders(const std::vector<model::Row> &rows)
    {
        orders_.clear();
        for (const model::Row &row : rows)
        {
            if (!starts_with(row.id, "settings:channel-sort:") || row.id.size() != 23)
                continue;
            Chip chip;
            chip.number = row.id.back() - '0';
            chip.row = row.id;
            chip.selected = row.has("selected");
            if (chip.number >= 0 && chip.number <= 2)
                orders_.push_back(std::move(chip));
        }
    }

    void read_tiles()
    {
        const Feed &feed = list();
        const std::string was = focus_ < static_cast<int>(tiles_.size()) ? tiles_[static_cast<std::size_t>(focus_)].channel.id : std::string();
        tiles_.clear();
        for (std::size_t i = 0; i < feed.channels.size(); ++i)
        {
            const model::Row &row = *feed.row_of_channel(i);
            Tile tile;
            tile.channel = feed.channels[i];
            tile.line = row.fact("subscribers").empty() ? (uses_local() ? std::string() : feed.channels[i].subscribers) : row.fact("subscribers");
            tile.unseen = row.has("unseen") ? std::max(number_of(row.fact("unseen")), 1) : 0;
            tile.live = row.has("live");
            tiles_.push_back(std::move(tile));
        }
        // The focus stays on the channel it was on, or on the nearest tile that is still there.
        const int count = static_cast<int>(tiles_.size());
        for (int i = 0; i < count && !was.empty(); ++i)
        {
            if (tiles_[static_cast<std::size_t>(i)].channel.id == was)
                focus_ = i;
        }
        focus_ = std::clamp(focus_, 0, std::max(count - 1, 0));
        const bool notice = mode_ == Mode::empty || mode_ == Mode::invite || mode_ == Mode::trouble;
        if (notice)
            zone_ = Zone::notice;
        else if (zone_ == Zone::notice || (zone_ == Zone::order && mode_ != Mode::account))
            zone_ = Zone::tiles;
    }

    // ---- actions ----------------------------------------------------------------------------

    // doing_: 1 an order is being chosen, 2 the marks are being cleared.
    void choose_order(const Chip &chip, UpdateContext &context)
    {
        if (doing_ != 0)
            return;
        if (chip.selected)
        {
            context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(chip.rect.cx()));
            return;
        }
        doing_ = 1;
        wanted_ = chip.number;
        outcome_ = "idle";
        act_.start(context.services.data, chip.row);
        context.feedback.play(audio::Cue::toggle, 1.08f, ui::pan_for_x(chip.rect.cx()));
    }

    void clear_marks(const InputFrame &input, UpdateContext &context)
    {
        if (doing_ != 0)
            return;
        if (marks() == 0)
        {
            refuse(context, input, nudge_);
            context.nav.toast(tr("No channel is marked as new."), ToastKind::info);
            return;
        }
        doing_ = 2;
        outcome_ = "idle";
        act_.start(context.services.data, "channels:watched");
        context.feedback.play(audio::Cue::toggle, 0.94f);
    }

    void finish(UpdateContext &context)
    {
        if (doing_ == 0)
            return;
        const Page *answer = nullptr;
        const model::Load result = act_.poll(context.services.data, &answer);
        if (result == model::Load::loading || result == model::Load::idle)
            return;
        const int done = doing_;
        doing_ = 0;
        Data &data = context.services.data;
        if (result == model::Load::ready)
        {
            outcome_ = "confirmed";
            if (done == 1)
            {
                // The provider answers with the orders again: the one it keeps now is marked.
                read_orders(answer->rows);
                if (selected_order() != wanted_)
                {
                    for (Chip &chip : orders_)
                        chip.selected = chip.number == wanted_;
                }
                data.forget("settings:channel-sort");
                orders_chosen_ = true;
                focus_ = 0;
            }
            else
            {
                cleared_ = true;
                context.nav.toast(answer->rows.empty() ? tr("Marked as watched") : answer->rows.front().title.c_str(), ToastKind::success);
            }
            data.forget("library:channels"); // the list is read again; what is on screen stays until it is there
            return;
        }
        outcome_ = "failed";
        const std::string message = answer != nullptr ? answer->error : std::string();
        context.feedback.play(audio::Cue::error);
        context.feedback.rumble(0.25f, 0.05f);
        context.nav.toast(message.empty() ? tr("YouTube did not confirm the change. Nothing was changed.") : message.c_str(), ToastKind::error);
    }

    // ---- input ------------------------------------------------------------------------------

    void press_choice(UpdateContext &context)
    {
        const std::string name = choice_names_[choice_];
        if (name == "sign_in")
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
        else if (name == "discover")
            context.nav.root(Route{ScreenId::home, {}, {}});
        else if (name == "search")
            context.nav.root(Route{ScreenId::search, {}, {}});
        else
        {
            context.feedback.play(audio::Cue::select);
            context.services.data.refresh();
            age_ = 0.0f;
        }
    }

    void handle(const InputFrame &input, UpdateContext &context)
    {
        if (sheet_.open())
        {
            Route route;
            if (sheet_.handle(input, context, &route))
                context.nav.push(route, Move::push);
            return;
        }
        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (mode_ == Mode::loading)
        {
            if (input.nav == Direction::left)
                context.nav.open_rail();
            return;
        }
        if (zone_ == Zone::notice)
        {
            if (input.nav == Direction::left && choice_ == 0)
                context.nav.open_rail();
            else if (input.nav == Direction::left || input.nav == Direction::right)
            {
                const int next = choice_ + (input.nav == Direction::right ? 1 : -1);
                if (next < notice_.count)
                {
                    choice_ = next;
                    context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(notice_.buttons[static_cast<std::size_t>(next)].cx()));
                }
                else
                    refuse(context, input, nudge_);
            }
            else if (input.nav != Direction::none)
                refuse(context, input, nudge_);
            else if (pressed(input, Action::confirm) && notice_.count > 0)
                press_choice(context);
            return;
        }
        if (mode_ == Mode::account && pressed(input, Action::west) && chips_.size() > 1)
        {
            // Order: the next of the three, round and round.
            std::size_t at = 0;
            for (std::size_t i = 0; i < chips_.size(); ++i)
                at = chips_[i].selected ? i : at;
            choose_order(chips_[(at + 1) % chips_.size()], context);
            return;
        }
        if (pressed(input, Action::north))
        {
            if (mode_ == Mode::account)
                clear_marks(input, context);
            else if (mode_ == Mode::kept)
                press_choice_retry(context);
            return;
        }
        if (pressed(input, Action::menu) && mode_ == Mode::local)
        {
            context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
            return;
        }
        if (zone_ == Zone::order)
            handle_order(input, context);
        else
            handle_tiles(input, context);
    }

    void press_choice_retry(UpdateContext &context)
    {
        context.feedback.play(audio::Cue::select);
        context.services.data.refresh();
    }

    void handle_order(const InputFrame &input, UpdateContext &context)
    {
        const int count = static_cast<int>(chips_.size());
        if (count == 0)
        {
            zone_ = Zone::tiles;
            return;
        }
        chip_ = std::clamp(chip_, 0, count - 1);
        const Chip chip = chips_[static_cast<std::size_t>(chip_)];
        if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = chip_ + (input.nav == Direction::right ? 1 : -1);
            if (next >= 0 && next < count)
            {
                chip_ = next;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(chips_[static_cast<std::size_t>(next)].rect.cx()));
            }
            else
                refuse(context, input, nudge_, chip.rect.cx());
        }
        else if (input.nav == Direction::down && !tiles_.empty())
        {
            zone_ = Zone::tiles;
            context.feedback.play(audio::Cue::tab, 0.96f);
        }
        else if (input.nav == Direction::up)
            refuse(context, input, nudge_, chip.rect.cx());
        else if (pressed(input, Action::confirm))
            choose_order(chip, context);
    }

    void handle_tiles(const InputFrame &input, UpdateContext &context)
    {
        const int count = static_cast<int>(tiles_.size());
        if (count == 0)
            return;
        const int column = focus_ % kAcross;
        int next = focus_;
        bool edge = false;
        switch (input.nav)
        {
        case Direction::left:
            if (column == 0)
            {
                context.nav.open_rail();
                return;
            }
            next = focus_ - 1;
            break;
        case Direction::right:
            if (column < kAcross - 1 && focus_ + 1 < count)
                next = focus_ + 1;
            else
                edge = true;
            break;
        case Direction::up:
            if (focus_ >= kAcross)
                next = focus_ - kAcross;
            else if (!chips_.empty())
            {
                // Above the first row: the order, at the one in force.
                zone_ = Zone::order;
                for (std::size_t i = 0; i < chips_.size(); ++i)
                    chip_ = chips_[i].selected ? static_cast<int>(i) : chip_;
                context.feedback.play(audio::Cue::tab, 1.04f);
                return;
            }
            else
                edge = true;
            break;
        case Direction::down:
            if (focus_ + kAcross < count)
                next = focus_ + kAcross;
            else if (focus_ / kAcross < (count - 1) / kAcross)
                next = count - 1;
            else
                edge = true;
            break;
        case Direction::none:
            break;
        }
        const Rect at = tile_rect(focus_, scroll_.target);
        if (edge)
            refuse(context, input, nudge_, at.cx());
        else if (next != focus_)
        {
            focus_ = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(tile_rect(next, scroll_.target).cx()));
        }
        if ((count - 1) / kAcross - focus_ / kAcross <= 1)
            list().want_more();
        const Tile &tile = tiles_[static_cast<std::size_t>(focus_)];
        if (pressed(input, Action::confirm))
            context.nav.push(Route{ScreenId::channel, tile.channel.id, tile.channel.name}, Move::push);
        else if (pressed(input, Action::touch))
            sheet_.show(tile.channel.row, tile.channel.name, context);
    }

    // ---- where things are, and how they move ---------------------------------------------------

    Icon notice_icon() const
    {
        return mode_ == Mode::trouble ? trouble_icon(account_.load) : Icon::users;
    }
    const char *notice_title() const
    {
        if (mode_ == Mode::trouble)
            return trouble_title(account_.load);
        return mode_ == Mode::empty ? tr("No subscriptions yet") : tr("Your channels live in your account");
    }
    std::string notice_body() const
    {
        if (mode_ == Mode::trouble)
            return account_.error.empty() ? std::string(tr("The list of channels did not arrive. Check the connection and try again.")) : account_.error;
        if (mode_ == Mode::empty)
            return tr("Open a channel and choose Subscribe. It will have a tile here, with a mark when it has something new.");
        return tr("Sign in to see the channels you subscribe to on YouTube. A channel you follow from one of its videos is kept here on this PS5.");
    }

    void lay_out(const ui::Fonts &fonts)
    {
        // The order chips stand in the header, the last one against the clock's corner; they read from the left: the provider's last first.
        chips_.clear();
        if (mode_ == Mode::account && orders_.size() > 1)
        {
            float x = kChipsRight;
            for (const Chip &order : orders_)
            {
                Chip chip = order;
                const float width = chip_width(fonts, order_label(chip.number));
                x -= width;
                chip.rect = {x, kChipY - 22.0f, width, 44.0f};
                x -= 12.0f;
                chips_.insert(chips_.begin(), chip);
            }
        }
        chip_ = std::clamp(chip_, 0, std::max(static_cast<int>(chips_.size()) - 1, 0));
        notice_.count = 0;
        if (mode_ == Mode::invite)
        {
            choices_[0] = {tr("Sign in"), Icon::user};
            choices_[1] = {tr("Discover"), Icon::trending};
            choice_names_[0] = "sign_in";
            choice_names_[1] = "discover";
            notice_.lay_out(fonts, (kLeft + kRight) * 0.5f, 300.0f, notice_body(), choices_, 2);
        }
        else if (mode_ == Mode::empty)
        {
            choices_[0] = {tr("Search"), Icon::search};
            choice_names_[0] = "search";
            notice_.lay_out(fonts, (kLeft + kRight) * 0.5f, 300.0f, notice_body(), choices_, 1);
        }
        else if (mode_ == Mode::trouble)
        {
            choices_[0] = {tr("Try again"), Icon::update};
            choice_names_[0] = "retry";
            notice_.lay_out(fonts, (kLeft + kRight) * 0.5f, 300.0f, notice_body(), choices_, 1);
        }
        choice_ = std::clamp(choice_, 0, std::max(notice_.count - 1, 0));
        subtitle_ = fit(fonts.regular, subtitle(), 22.0f, (chips_.empty() ? kRight - 140.0f : chips_.front().rect.x - 40.0f) - kLeft);
    }

    std::string subtitle() const
    {
        const std::string count = std::to_string(tiles_.size()) + (list().more ? "+" : "");
        switch (mode_)
        {
        case Mode::account:
            return dotted({fill(tr("{0} subscriptions"), count), selected_order() >= 0 ? order_phrase(selected_order()) : ""});
        case Mode::local:
            return dotted({tr("Followed on this PS5"), tr("sign in to see your YouTube subscriptions")});
        case Mode::kept:
            return tr("Kept on this PS5");
        default:
            return {};
        }
    }

    void advance(UpdateContext &context)
    {
        const float dt = context.dt;
        const bool reduced = context.reduced_motion;
        const float quick = reduced ? 60.0f : 20.0f;
        // Two rows show; the page follows the focus a row at a time.
        const int row = tiles_.empty() ? 0 : focus_ / kAcross;
        float target = scroll_.target;
        target = std::min(target, kTilePitchY * static_cast<float>(row));
        target = std::max(target, kTilePitchY * static_cast<float>(row - 1));
        scroll_.target = std::max(target, 0.0f);
        scroll_.update(dt, reduced ? 60.0f : 12.0f);
        tiles_focus_.target = zone_ == Zone::tiles && !sheet_.open() ? 1.0f : 0.0f;
        tiles_focus_.update(dt, quick);
        order_focus_.target = zone_ == Zone::order ? 1.0f : 0.0f;
        order_focus_.update(dt, quick);
        chip_pos_.target = static_cast<float>(chip_);
        chip_pos_.update(dt, quick);
        choice_pos_.target = static_cast<float>(choice_);
        choice_pos_.update(dt, quick);
        marks_.target = cleared_ ? 0.0f : 1.0f;
        marks_.update(dt, reduced ? 60.0f : 10.0f);
        nudge_.update(dt, 10.0f);
        if (!tiles_.empty())
        {
            const Rect goal = grown(tile_rect(focus_, 0.0f), 0.04f, 4.0f);
            if (!ring_set_ || tiles_focus_.value < 0.02f)
                ring_.snap(goal);
            else
                ring_.target(goal);
            ring_set_ = true;
        }
        ring_.update(dt, quick);
    }

    // ---- drawing ----------------------------------------------------------------------------

    void draw_head(G &g) const
    {
        Enter enter(g, arrive(age_, 0, 0.05f, 0.5f), -14.0f);
        if (!subtitle_.empty())
            text(g, g.f.regular, subtitle_, kLeft, 166.0f, 22.0f, g.k.ink2);
        const float waiting = g.reduced ? 0.7f : 0.5f + 0.4f * ui::breathe(g.time, 0.9f);
        for (std::size_t i = 0; i < chips_.size(); ++i)
        {
            const Chip &each = chips_[i];
            const float focus = tween::clamp01(1.0f - std::fabs(chip_pos_.value - static_cast<float>(i))) * order_focus_.value;
            // The order that was asked for waits, dimmed, until the provider has taken it.
            g.l.push_opacity(doing_ == 1 && each.number == wanted_ ? waiting : 1.0f);
            chip(g, each.rect.x, each.rect.cy(), order_label(each.number), each.selected, focus);
            g.l.pop_opacity();
        }
    }

    void draw_bones(G &g) const
    {
        Enter enter(g, arrive(age_, 1, 0.05f, 0.4f), 18.0f);
        bone(g, {kLeft, 148.0f, 360.0f, 20.0f}, 10.0f);
        for (int i = 0; i < kAcross * 2; ++i)
        {
            const Rect r = tile_rect(i, 0.0f);
            g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, g.k.hairline);
            bone(g, {r.cx() - 74.0f, r.y + 36.0f, 148.0f, 148.0f}, 74.0f);
            bone(g, {r.cx() - 84.0f, r.y + 212.0f, 168.0f, 22.0f}, 11.0f);
            bone(g, {r.cx() - 60.0f, r.y + 246.0f, 120.0f, 16.0f}, 8.0f);
        }
    }

    void draw_tiles(G &g, const DrawContext &context) const
    {
        const float scroll = scroll_.value;
        const float top = tile_rect(0, 0.0f).y;
        if (mode_ == Mode::kept)
        {
            // The account cannot be reached; what the console keeps is still here.
            Enter enter(g, arrive(age_, 1, 0.06f, 0.5f), -30.0f);
            const Rect banner{kLeft, kBannerY, kRight - kLeft, kBannerH};
            glass_panel(g, banner, 24.0f);
            g.l.rounded_rect({banner.x, banner.y, 8.0f, banner.h}, 4.0f, g.k.theme.warning);
            icon(g, Icon::offline, banner.x + 48.0f, banner.cy(), 28.0f, g.k.theme.warning);
            const char *title = account_.load == model::Load::offline ? tr("Offline") : tr("Not loaded");
            const float used = text(g, g.f.semibold, title, banner.x + 84.0f, banner.cy() + 8.5f, kTypeBody, g.k.ink);
            const std::string why = account_.error.empty() ? std::string(tr("Your subscriptions could not be read.")) : account_.error;
            text(g, g.f.regular, fit(g.f.regular, why, 22.0f, banner.w - 84.0f - used - 60.0f), banner.x + 84.0f + used + 20.0f, banner.cy() + 8.0f, 22.0f,
                 g.k.ink2);
        }
        const bool clipped = scroll > 0.5f;
        if (clipped)
            g.l.push_clip({0.0f, top - 20.0f, gfx::kVirtualWidth, gfx::kVirtualHeight - top + 20.0f});
        const Rect ring = ring_.value();
        const int count = static_cast<int>(tiles_.size());
        const float zone_focus = tiles_focus_.value;
        const bool local = uses_local();
        g.l.push_transform(1.0f, 0.0f, 0.0f, 0.0f, -scroll);
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < count; ++i)
            {
                if ((pass == 1) != (i == focus_ && zone_focus > 0.01f))
                    continue;
                const Rect base = tile_rect(i, 0.0f);
                if (base.y - scroll > gfx::kVirtualHeight || base.y - scroll + base.h < 0.0f)
                    continue;
                const Tile &tile = tiles_[static_cast<std::size_t>(i)];
                const float focus = under_ring(ring, base, 200.0f) * zone_focus;
                Enter enter(g, arrive(age_, 2 + i % kAcross + std::min(i / kAcross, 2), 0.05f, 0.45f), 26.0f);
                const Rect r = grown(base, 0.04f * focus, 4.0f * focus);
                if (focus > 0.01f)
                    lift(g, r, kRadiusPanel, focus);
                glass_panel(g, r, kRadiusPanel);
                const Picture picture = context.images.get(tile.channel.avatar, PictureSize::avatar);
                avatar(g, r.cx(), r.y + 110.0f, 74.0f, tile.channel.name, tile.channel.id, picture);
                text(g, g.f.semibold, fit(g.f.semibold, tile.channel.name, 23.0f, r.w - 32.0f), r.cx(), r.y + 230.0f, 23.0f, g.k.ink, gfx::Align::center);
                const std::string line = local ? std::string(tr("On this PS5")) : tile.line;
                text(g, g.f.regular, fit(g.f.regular, line, 19.0f, r.w - 32.0f), r.cx(), r.y + 258.0f, 19.0f, g.k.ink3, gfx::Align::center);
                if (tile.live)
                {
                    const char *label = tr("Live now");
                    tag(g, r.cx() - tag_width(g, label) * 0.5f, r.y + 298.0f, label, g.k.accent, g.k.on_accent);
                }
                else if (tile.unseen > 0 && marks_.value > 0.01f)
                {
                    const std::string label = fill(tr("{0} new"), std::to_string(tile.unseen));
                    const float asked = doing_ == 2 ? (g.reduced ? 0.6f : 0.45f + 0.4f * ui::breathe(g.time, 0.9f)) : 1.0f;
                    g.l.push_opacity(marks_.value * asked);
                    tag(g, r.cx() - tag_width(g, label) * 0.5f, r.y + 298.0f, label, g.k.raised, g.k.ink);
                    g.l.pop_opacity();
                }
                if (i == focus_ && focus > 0.01f)
                {
                    Rect lit = ring;
                    lit.x += ui::shake(nudge_.value, g.time, 14.0f, 8.0f);
                    focus_ring(g, lit, kRadiusPanel, picture.ready() ? picture.glow : g.k.focus, zone_focus);
                }
            }
        }
        if (list().fetching_more())
        {
            const int from = (count + kAcross - 1) / kAcross * kAcross;
            for (int i = 0; i < kAcross; ++i)
            {
                const Rect r = tile_rect(from + i, 0.0f);
                g.l.bordered_rect(r, kRadiusPanel, clear(), 1.5f, g.k.hairline);
                bone(g, {r.cx() - 74.0f, r.y + 36.0f, 148.0f, 148.0f}, 74.0f);
            }
        }
        g.l.pop_transform();
        if (clipped)
            g.l.pop_clip();
    }

    Route route_;
    model::Profile profile_;
    Feed account_; // "library:channels"
    Feed local_;   // "subscriptions": followed on this console
    Mode mode_ = Mode::loading;
    std::vector<Tile> tiles_;
    std::vector<Chip> orders_; // as the provider lists them
    std::vector<Chip> chips_;  // as they stand in the header, left to right
    bool orders_known_ = false;
    bool orders_chosen_ = false;
    Zone zone_ = Zone::tiles;
    int focus_ = 0;
    int chip_ = 0;
    int choice_ = 0;
    Choice choices_[2];
    const char *choice_names_[2] = {"", ""};
    Notice notice_;
    Sheet sheet_;
    std::string subtitle_;

    Act act_;
    int doing_ = 0;
    int wanted_ = -1;
    std::string outcome_ = "idle";
    bool cleared_ = false; // the new marks were cleared in this visit

    float age_ = 0.0f;
    tween::Spring scroll_;
    tween::Spring tiles_focus_;
    tween::Spring order_focus_;
    tween::Spring chip_pos_;
    tween::Spring choice_pos_;
    tween::Spring marks_{1.0f, 0.0f, 1.0f};
    ui::SpringRect ring_;
    bool ring_set_ = false;
    ui::Pulse nudge_;
};

} // namespace

std::unique_ptr<Screen> make_channels()
{
    return std::make_unique<Channels>();
}

} // namespace afterglow
