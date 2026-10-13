// ProsperoTube Afterglow - accounts: "Who is watching?".
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The profiles of this console as tiles (the kit's profile picker), one more
// tile that adds an account, and what can be done for the profile in focus:
// sign in to YouTube, link a phone, remove the account. Cross watches as the
// profile, Triangle sets, changes or removes the password of the profile that
// is watching.
//
// What it reads: "library:profiles" (a row per profile, id
// "library:profile:<id>", facts guest, signed_in, selected, locked,
// subscriptions), "library" (the row "auth:start" is there when this build
// signs in to YouTube) and "settings:remote" (fact available on its first row:
// whether this build links phones). What it asks for by listing: a profile's
// row selects it; "account:remove" and the "account:apply:<n>" it answers
// with remove the selected account; "settings:lock:set|change|remove" arm the
// password prompt. route.argument may name a profile row: the screen then
// opens asking for that profile (the password of a protected profile at start).

#include "afterglow/screens/accounts.hpp"

#include "ui/components/profile_picker.hpp"

#include <algorithm>
#include <cmath>

namespace afterglow
{

namespace
{

using namespace acct;

constexpr Rect kTiles{kLeft, 208.0f, 1648.0f, 400.0f};
constexpr float kCentre = 1000.0f;
constexpr float kActionsTop = 716.0f;
constexpr float kActionHeight = 64.0f;
constexpr float kActionGap = 16.0f;
constexpr Rect kNotice{510.0f, 806.0f, 980.0f, 98.0f};
constexpr const char *kProfileRow = "library:profile:";

struct Tile
{
    std::string row; // "library:profile:<id>": listing it selects the profile
    std::string id;
    std::string name;
    std::string avatar;
    std::string subscriptions; // a count as text, "" when unknown
    bool guest = false;
    bool signed_in = false;
    bool selected = false;
    bool locked = false;
};

enum class Zone : std::uint8_t
{
    tiles,
    actions,
};

// What is being done, one listing after another.
struct Work
{
    enum class Kind : std::uint8_t
    {
        none,
        watch,       // select the profile, then Home
        remove,      // select it when it is not, then remove its account
        lock_set,    // a new password
        lock_change, // the current one, then a new one
        lock_remove, // the current one
    };
    enum class Step : std::uint8_t
    {
        select,   // the profile's row was listed
        password, // the profile's password was handed over
        removing, // "account:remove" was listed
        applying, // its "account:apply:<n>" was listed
        arming,   // "settings:lock:<verb>" was listed
        current,  // the current password was handed over
        fresh,    // the new password was handed over
    };
    Kind kind = Kind::none;
    Step step = Step::select;
    unsigned ticket = 0; // the action whose answer is awaited; 0 while the sheet has the word
    std::string row;
    std::string name;
    bool locked = false;
    float age = 0.0f;
};

class Accounts final : public Screen
{
  public:
    Accounts()
    {
        ui::ProfilePickerStyle &style = picker_.style;
        style.title.clear();
        style.card_width = 300.0f;
        style.card_height = 330.0f;
        style.avatar_size = 150.0f;
        style.gap = 32.0f;
        style.name_size = 30.0f;
        style.detail_size = 21.0f;
        style.align = gfx::Align::center;
        style.slots = false;
        style.dim = 0.25f;
        picker_.set_bounds(kTiles);
        picker_.avatar = [this](ui::Canvas &canvas, const Rect &area, const ui::Profile &, int index, float)
        { draw_face(canvas, area, index); };
    }

    ScreenId id() const override
    {
        return ScreenId::accounts;
    }

    void enter(const Route &route, UpdateContext &context, bool restored) override
    {
        fonts_ = &context.services.fonts;
        if (restored)
            return; // back from signing in or the phone page: the focus stays, the list is read again
        route_ = route;
        age_ = 0.0f;
        zone_ = Zone::tiles;
        action_ = 0;
        work_ = Work{};
        tiles_.clear();
        signature_.clear();
        have_tiles_ = false;
        load_ = model::Load::loading;
        focus_row_.clear();
        asked_at_start_ = route.argument.rfind(kProfileRow, 0) == 0 ? route.argument : std::string();
        sheet_.close();
        picker_.enter();
    }

    void update(const InputFrame &input, UpdateContext &context) override
    {
        age_ += context.dt;
        fonts_ = &context.services.fonts;
        Data &data = context.services.data;
        read(data);

        // The tile stays lit while the focus is on what can be done for it (board 41): the actions are its own.
        picker_.set_active(true);
        picker_.update(context.dt);
        const float speed = context.reduced_motion ? 60.0f : 20.0f;
        for (int i = 0; i < 3; ++i)
        {
            action_focus_[i].target = zone_ == Zone::actions && action_ == i ? 1.0f : 0.0f;
            action_focus_[i].update(context.dt, speed);
        }
        actions_shown_.target = action_count() > 0 ? 1.0f : 0.0f;
        actions_shown_.update(context.dt, context.reduced_motion ? 60.0f : 14.0f);
        nudge_.update(context.dt, 10.0f);
        if (work_.kind != Work::Kind::none)
            work_.age += context.dt;

        // An answer that was waited for: the next step of what is being done.
        if (work_.ticket != 0)
        {
            const Page &answer = data.action(work_.ticket);
            if (!pending(answer))
            {
                const Page copy = answer; // the next step asks for another action
                work_.ticket = 0;
                advance(copy, context);
            }
        }

        const int decided = question_.update(input, context.dt, context.feedback, context.top);
        if (decided != -2)
        {
            answered(decided, context);
            return;
        }
        const int typed = sheet_.update(input, context);
        if (typed == 1)
        {
            // REQUEST: Data::answer(text) (requests/F.md, 1). The text is not kept here in either case.
            std::string text = sheet_.take();
            std::fill(text.begin(), text.end(), '\0');
            sheet_.set_busy(true);
            wait(data, kPromptAnswer);
            return;
        }
        if (typed == -1)
        {
            work_ = Work{};
            return;
        }
        if (!context.top || question_.is_open() || sheet_.is_open())
            return;

        // A profile named by the route is asked for as soon as it is listed.
        if (!asked_at_start_.empty() && load_ == model::Load::ready && work_.kind == Work::Kind::none)
        {
            const std::string row = std::move(asked_at_start_);
            asked_at_start_.clear();
            for (std::size_t i = 0; i < tiles_.size(); ++i)
            {
                if (tiles_[i].row == row)
                {
                    picker_.set_focus(static_cast<int>(i));
                    watch(tiles_[i], data);
                    return;
                }
            }
        }

        if (pressed(input, Action::back))
        {
            context.nav.back();
            return;
        }
        if (work_.kind != Work::Kind::none)
            return; // an answer is on its way: nothing else is taken but Circle
        if (load_ != model::Load::ready)
        {
            if (input.nav == Direction::left)
                context.nav.open_rail();
            else if (pressed(input, Action::confirm) && load_ != model::Load::loading)
            {
                context.feedback.play(audio::Cue::select);
                forget_lists(data);
                load_ = model::Load::loading;
            }
            else if (input.nav != Direction::none)
                refuse(input, context);
            return;
        }
        if (pressed(input, Action::north))
        {
            password(context);
            return;
        }
        // A profile with a picture colours the page; one without leaves the page as it was.
        if (const Tile *tile = tile_in_focus(); tile != nullptr && !tile->avatar.empty())
            context.nav.ambient(tile->avatar);
        if (zone_ == Zone::tiles)
            update_tiles(input, context);
        else
            update_actions(input, context);
    }

    void draw(const DrawContext &context, const Phase &phase) const override
    {
        G g = context.g();
        drawing_ = &context;
        dress(context);
        {
            Travel travel(g, phase);
            {
                Enter enter(g, arrive(age_, 0, 0.06f, 0.5f), -14.0f);
                text(g, g.f.regular, fit(g.f.regular, tr("Each profile keeps its own history, queue and settings"), 22.0f, 1300.0f),
                     kLeft, 166.0f, 22.0f, g.k.ink2);
            }
            if (load_ == model::Load::ready)
            {
                {
                    Enter enter(g, arrive(age_, 1, 0.06f, 0.5f), 24.0f);
                    ui::Canvas canvas{context.ui, context.fonts, context.glass, context.time};
                    picker_.draw(canvas);
                }
                draw_actions(g);
                draw_note(g);
            }
            else if (load_ == model::Load::loading || load_ == model::Load::idle)
            {
                draw_bones(g);
            }
            else
            {
                const bool offline = load_ == model::Load::offline;
                message_page(g, offline ? Icon::offline : Icon::alert,
                             offline ? tr("No connection to the YouTube service") : tr("The profiles could not be loaded"), failure_words(),
                             tr("Try again"), 1.0f, arrive(age_, 1, 0.06f, 0.5f), offline ? g.k.theme.warning : g.k.theme.danger);
            }
        }
        sheet_.draw(context);
        question_.draw(context);
        drawing_ = nullptr;
    }

    std::string title() const override
    {
        return tr("Who is watching?");
    }

    std::span<const ui::Hint> hints() const override
    {
        static ui::Hint items[3];
        int count = 0;
        if (sheet_.is_open())
            return sheet_.hints();
        if (question_.is_open())
        {
            items[count++] = {ui::Button::cross, tr("Choose")};
            items[count++] = {ui::Button::circle, tr("Cancel")};
        }
        else if (work_.kind != Work::Kind::none || load_ == model::Load::loading)
        {
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        else if (load_ != model::Load::ready)
        {
            items[count++] = {ui::Button::cross, tr("Try again")};
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        else
        {
            const Tile *tile = tile_in_focus();
            if (zone_ == Zone::actions)
                items[count++] = {ui::Button::cross, tr("Choose")};
            else if (tile != nullptr)
                items[count++] = {ui::Button::cross, tr("Watch as")};
            else if (sign_in_ != 0)
                items[count++] = {ui::Button::cross, tr("Add an account")};
            if (tile != nullptr && tile->selected && !tile->guest)
                items[count++] = {ui::Button::triangle, tile->locked ? tr("Change the password") : tr("Set a password")};
            items[count++] = {ui::Button::circle, tr("Back")};
        }
        return std::span<const ui::Hint>(items, static_cast<std::size_t>(count));
    }

    FocusInfo focus() const override
    {
        FocusInfo info;
        if (question_.is_open())
        {
            info.id = "dialog/" + std::to_string(question_.focus());
            info.text = question_.answer(question_.focus());
            info.index = question_.focus();
            info.total = asking_ == Asking::remove ? 2 : 3;
            return info;
        }
        if (sheet_.is_open())
            return sheet_.focus();
        if (load_ == model::Load::loading || load_ == model::Load::idle)
        {
            info.id = "profiles";
            info.rect = kTiles;
            return info;
        }
        if (load_ != model::Load::ready)
        {
            info.id = "action/retry";
            info.text = tr("Try again");
            info.rect = fonts_ != nullptr ? message_button_rect(*fonts_, failure_words(), tr("Try again")) : Rect{};
            info.total = 1;
            return info;
        }
        if (zone_ == Zone::actions)
        {
            static constexpr const char *kIds[] = {"action/sign-in", "action/link-phone", "action/remove"};
            info.id = kIds[std::clamp(action_, 0, 2)];
            info.text = action_label(action_);
            info.rect = action_rect(action_);
            info.index = action_;
            info.total = action_count();
            return info;
        }
        const int at = picker_.focus();
        info.id = at < static_cast<int>(tiles_.size()) ? "profile/" + std::to_string(at) : std::string("profile/add");
        info.text = at < static_cast<int>(tiles_.size()) ? shown_name(tiles_[static_cast<std::size_t>(at)]) : std::string(tr("Add an account"));
        info.rect = picker_.count() > 0 ? picker_.card_rect(at) : kTiles;
        info.index = at;
        info.total = picker_.count();
        return info;
    }

    std::vector<std::pair<std::string, std::string>> focusable() const override
    {
        std::vector<std::pair<std::string, std::string>> out;
        if (load_ != model::Load::ready || question_.is_open() || sheet_.is_open())
        {
            const FocusInfo only = focus();
            out.emplace_back(only.id, only.text);
            return out;
        }
        for (std::size_t i = 0; i < tiles_.size(); ++i)
            out.emplace_back("profile/" + std::to_string(i), shown_name(tiles_[i]));
        out.emplace_back("profile/add", tr("Add an account"));
        static constexpr const char *kIds[] = {"action/sign-in", "action/link-phone", "action/remove"};
        for (int i = 0; i < action_count(); ++i)
            out.emplace_back(kIds[i], action_label(i));
        return out;
    }

    void snapshot(Snapshot &out) const override
    {
        out.set("load", load_name(load_));
        out.set("profiles", static_cast<int>(tiles_.size()));
        out.set("zone", zone_ == Zone::tiles ? "tiles" : "actions");
        out.set("sign_in", sign_in_ == 0 ? "unavailable" : sign_in_ == 1 ? "available" : "unknown");
        out.set("casting", casting_ == 0 ? "unavailable" : casting_ == 1 ? "available" : "unknown");
        static constexpr const char *kWork[] = {"", "watch", "remove", "lock_set", "lock_change", "lock_remove"};
        out.set("work", kWork[static_cast<std::size_t>(work_.kind)]);
        out.set("question", !question_.is_open() ? "" : asking_ == Asking::remove ? "remove" : "password");
        // Which password the sheet asks for; words of the interface are not state.
        out.set("sheet", !sheet_.is_open() ? "" : work_.step == Work::Step::fresh ? "new-password" : work_.step == Work::Step::current ? "current-password" : "password");
        std::vector<std::string> names;
        std::string selected;
        for (const Tile &tile : tiles_)
        {
            names.push_back(tile.guest ? std::string("guest") : tile.name);
            if (tile.selected)
                selected = names.back();
        }
        out.set("selected", selected);
        out.list("profiles", std::move(names));
    }

  private:
    enum class Asking : std::uint8_t
    {
        nothing,
        remove,
        password,
    };

    // ---- data ------------------------------------------------------------------

    void read(Data &data)
    {
        profile_ = data.profile();
        const Page &list = data.catalog("library:profiles");
        if (list.load == model::Load::ready)
        {
            std::vector<Tile> tiles;
            const auto take = [&](const Page &page)
            {
                for (const model::Row &row : page.rows)
                {
                    if (row.id.rfind(kProfileRow, 0) != 0)
                        continue;
                    Tile tile;
                    tile.row = row.id;
                    tile.id = row.id.substr(std::char_traits<char>::length(kProfileRow));
                    tile.name = row.title;
                    tile.avatar = row.thumb;
                    tile.subscriptions = row.fact("subscriptions");
                    tile.guest = row.has("guest") || tile.id == "guest";
                    tile.signed_in = row.has("signed_in");
                    tile.selected = row.has("selected");
                    tile.locked = row.has("locked");
                    tiles.push_back(std::move(tile));
                }
            };
            take(list);
            // A list longer than one page: the further pages are added as they arrive.
            bool more = list.more;
            for (int page = 1; more && page < 8; ++page)
            {
                const Page &next = data.catalog("library:profiles", page);
                if (next.load != model::Load::ready)
                    break;
                take(next);
                more = next.more;
            }
            tiles_ = std::move(tiles);
            have_tiles_ = true;
            load_ = model::Load::ready;
        }
        else if (list.load == model::Load::failed || list.load == model::Load::offline)
        {
            load_ = list.load;
            error_ = list.error;
            have_tiles_ = false;
            tiles_.clear();
        }
        else if (!have_tiles_)
        {
            load_ = model::Load::loading; // while a list is shown, it stays until the next one arrives
        }

        const Page &library = data.catalog("library");
        if (library.load == model::Load::ready)
            sign_in_ = find_row(library, "auth:start") != nullptr ? 1 : 0;
        else if (library.load != model::Load::loading && library.load != model::Load::idle)
            sign_in_ = -1;
        const Page &remote = data.catalog("settings:remote");
        if (remote.load == model::Load::ready)
            casting_ = remote.rows.empty() || fact_of(remote, "available") == "0" ? 0 : 1;
        else if (remote.load != model::Load::loading && remote.load != model::Load::idle)
            casting_ = -1;
        sync();
    }

    // The picker's cards from the tiles, only when something changed: setting
    // them restarts its springs.
    void sync()
    {
        if (load_ != model::Load::ready)
            return;
        std::string signature = sign_in_ == 0 ? "no sign-in" : "sign-in";
        for (const Tile &tile : tiles_)
            signature += '\n' + tile.row + '\t' + tile.name + '\t' + tile.subscriptions + (tile.signed_in ? 's' : '-') +
                         (tile.selected ? 'c' : '-') + (tile.locked ? 'l' : '-');
        if (signature == signature_)
            return;
        const bool first = signature_.empty();
        signature_ = std::move(signature);
        std::vector<ui::Profile> cards;
        int selected = -1;
        int kept = -1;
        for (std::size_t i = 0; i < tiles_.size(); ++i)
        {
            const Tile &tile = tiles_[i];
            ui::Profile card;
            card.name = shown_name(tile);
            card.detail = tile.guest ? tr("No account") : tile.signed_in ? tr("Signed in to YouTube") : tr("Signed out");
            if (tile.guest)
                card.extra = tr("History stays on this PS5");
            else
            {
                // One line under the state: the subscriptions where the service counts them, else what the marks on the face mean.
                if (tile.subscriptions == "1")
                    card.extra = tr("1 subscription");
                else if (!tile.subscriptions.empty())
                    card.extra = with(tr("{0} subscriptions"), tile.subscriptions);
                else if (tile.locked)
                    card.extra = tr("Password protected");
                else if (tile.selected)
                    card.extra = tr("Watching now");
            }
            card.tag = static_cast<int>(i);
            cards.push_back(std::move(card));
            if (tile.selected)
                selected = static_cast<int>(i);
            if (tile.row == focus_row_)
                kept = static_cast<int>(i);
        }
        // A build that does not sign in keeps the tile and says so on it.
        picker_.style.add_card = sign_in_ != 0;
        picker_.style.add_label = tr("Add an account");
        if (sign_in_ == 0)
        {
            ui::Profile card;
            card.name = tr("Add an account");
            card.detail = tr("Not in this build");
            cards.push_back(std::move(card));
        }
        const int before = picker_.focus();
        picker_.set_profiles(std::move(cards));
        // The focus stays on its profile; the first list opens on whoever is watching.
        int focus = kept >= 0 ? kept : first ? std::max(selected, 0) : std::min(before, picker_.count() - 1);
        if (focus_row_ == "add")
            focus = picker_.count() - 1;
        picker_.set_focus(std::max(focus, 0));
        remember_focus();
        if (action_count() == 0)
            zone_ = Zone::tiles;
        action_ = std::clamp(action_, 0, std::max(action_count() - 1, 0));
        if (first)
            picker_.enter();
    }

    void remember_focus()
    {
        const Tile *tile = tile_in_focus();
        focus_row_ = tile != nullptr ? tile->row : std::string("add");
    }

    const Tile *tile_in_focus() const
    {
        const int at = picker_.focus();
        return at >= 0 && at < static_cast<int>(tiles_.size()) ? &tiles_[static_cast<std::size_t>(at)] : nullptr;
    }

    static std::string shown_name(const Tile &tile)
    {
        return tile.guest || tile.name.empty() ? std::string(tr("Guest")) : tile.name;
    }

    // Why there is no list, and what stays: whoever is watching keeps watching.
    std::string failure_words() const
    {
        if (load_ != model::Load::offline)
            return error_.empty() ? std::string(tr("The list of profiles was not answered. Try again.")) : error_;
        const std::string name = profile_.guest || profile_.name.empty() ? std::string(tr("Guest")) : profile_.name;
        return with(tr("The profiles cannot be read while the YouTube service is not answering. You keep watching as {0}, and what is saved on this PS5 stays available."), name);
    }

    // ---- what the focus can do ---------------------------------------------------

    // Guest has no account to remove; the tile that adds an account has no actions of its own.
    int action_count() const
    {
        const Tile *tile = load_ == model::Load::ready ? tile_in_focus() : nullptr;
        return tile == nullptr ? 0 : tile->guest ? 2 : 3;
    }

    static const char *action_label(int index)
    {
        return index == 0 ? tr("Sign in to YouTube") : index == 1 ? tr("Link a phone") : tr("Remove this account");
    }

    bool action_offered(int index) const
    {
        return index == 0 ? sign_in_ != 0 : index == 1 ? casting_ != 0 : true;
    }

    Rect action_rect(int index) const
    {
        const int count = std::max(action_count(), 1);
        float widths[3] = {300.0f, 230.0f, 330.0f};
        float total = kActionGap * static_cast<float>(count - 1);
        for (int i = 0; i < count; ++i)
        {
            if (fonts_ != nullptr)
                widths[i] = measure(fonts_->semibold, action_label(i), kTypeBody) + 96.0f;
            total += widths[i];
        }
        float x = kCentre - total * 0.5f;
        for (int i = 0; i < std::min(index, count - 1); ++i)
            x += widths[i] + kActionGap;
        return {x, kActionsTop, widths[std::clamp(index, 0, 2)], kActionHeight};
    }

    void refuse(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav_repeat)
            return; // a held direction stays silent
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
    }

    // What a build does not offer refuses; the notice under the actions says why, in the app's own words.
    void not_offered(UpdateContext &context)
    {
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
        context.feedback.rumble(0.25f, 0.05f);
        nudge_.trigger();
    }

    void update_tiles(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav == Direction::left && picker_.focus() == 0)
        {
            context.nav.open_rail();
            return;
        }
        if (input.nav == Direction::down)
        {
            if (action_count() > 0)
            {
                zone_ = Zone::actions;
                action_ = std::clamp(action_, 0, action_count() - 1);
                context.feedback.play(audio::Cue::tab, 0.96f);
            }
            else
                refuse(input, context);
            return;
        }
        if (input.nav == Direction::up)
        {
            refuse(input, context);
            return;
        }
        const ui::Event event = picker_.handle(input, context.feedback);
        if (event == ui::Event::moved)
        {
            remember_focus();
            action_ = 0;
        }
        else if (event == ui::Event::activated)
        {
            const Tile *tile = tile_in_focus();
            if (tile != nullptr)
                watch(*tile, context.services.data);
            else if (sign_in_ == 0)
                not_offered(context);
            else
                context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
        }
    }

    void update_actions(const InputFrame &input, UpdateContext &context)
    {
        if (input.nav == Direction::up)
        {
            zone_ = Zone::tiles;
            context.feedback.play(audio::Cue::tab, 1.04f);
        }
        else if (input.nav == Direction::left || input.nav == Direction::right)
        {
            const int next = action_ + (input.nav == Direction::right ? 1 : -1);
            if (next < 0)
                context.nav.open_rail();
            else if (next < action_count())
            {
                action_ = next;
                context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(action_rect(next).cx()));
            }
            else
                refuse(input, context);
        }
        else if (input.nav == Direction::down)
        {
            refuse(input, context);
        }
        else if (pressed(input, Action::confirm))
        {
            const Tile *tile = tile_in_focus();
            if (tile == nullptr)
                return;
            if (!action_offered(action_))
            {
                not_offered(context);
            }
            else if (action_ == 0)
            {
                context.feedback.play(audio::Cue::select);
                context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::push);
            }
            else if (action_ == 1)
            {
                context.feedback.play(audio::Cue::select);
                context.nav.push(Route{ScreenId::link_phone, {}, {}}, Move::push);
            }
            else
            {
                // One sound per action: the question's own.
                asking_ = Asking::remove;
                work_ = Work{};
                work_.row = tile->row;
                work_.name = shown_name(*tile);
                work_.locked = tile->locked && !tile->selected;
                selected_before_ = tile->selected;
                question_.ask(ui::StatusKind::danger, with(tr("Remove {0} from this PS5?"), work_.name),
                              tr("The sign-in is deleted from this console. You continue as Guest; other accounts stay available."),
                              {tr("Keep"), tr("Remove")}, 1, 0, false, context.feedback);
            }
        }
    }

    // Triangle: the password of the profile that is watching.
    void password(UpdateContext &context)
    {
        const Tile *tile = tile_in_focus();
        if (tile == nullptr || tile->guest)
        {
            refuse(InputFrame{}, context);
            return;
        }
        if (!tile->selected)
        {
            // The provider keeps a password for the profile that is watching, and no other.
            refuse(InputFrame{}, context);
            context.nav.toast(with(tr("Watch as {0} first to set its password"), shown_name(*tile)), ToastKind::info);
            return;
        }
        work_ = Work{};
        work_.row = tile->row;
        work_.name = shown_name(*tile);
        if (!tile->locked)
        {
            context.feedback.play(audio::Cue::select);
            work_.kind = Work::Kind::lock_set;
            work_.step = Work::Step::arming;
            wait(context.services.data, "settings:lock:set");
            return;
        }
        asking_ = Asking::password;
        question_.ask(ui::StatusKind::question, tr("Password protection (experimental)"),
                      tr("Asked before the profile is selected; this PS5 stores a salted hash, never the text"),
                      {tr("Change the password of this profile (experimental)"), tr("Remove the password of this profile (experimental)"),
                       tr("Cancel")},
                      -1, 0, true, context.feedback);
    }

    void answered(int choice, UpdateContext &context)
    {
        const Asking asked = asking_;
        asking_ = Asking::nothing;
        Data &data = context.services.data;
        if (asked == Asking::remove && choice == 1)
        {
            work_.kind = Work::Kind::remove;
            if (selected_before_)
            {
                work_.step = Work::Step::removing;
                wait(data, "account:remove");
            }
            else
            {
                work_.step = Work::Step::select;
                wait(data, work_.row);
            }
        }
        else if (asked == Asking::password && (choice == 0 || choice == 1))
        {
            work_.kind = choice == 0 ? Work::Kind::lock_change : Work::Kind::lock_remove;
            work_.step = Work::Step::arming;
            wait(data, choice == 0 ? "settings:lock:change" : "settings:lock:remove");
        }
        else
        {
            work_ = Work{};
        }
    }

    // ---- what is being done --------------------------------------------------------

    void wait(Data &data, const std::string &row)
    {
        work_.ticket = data.act(row);
    }

    // What an action may have changed is read again; the tiles stay until the new list is there.
    static void forget_lists(Data &data)
    {
        data.forget("library:profiles");
        data.forget("library");
        data.forget("settings:remote");
    }

    void watch(const Tile &tile, Data &data)
    {
        work_ = Work{};
        work_.kind = Work::Kind::watch;
        work_.step = Work::Step::select;
        work_.row = tile.row;
        work_.name = shown_name(tile);
        work_.locked = tile.locked && !tile.selected;
        wait(data, tile.row);
    }

    // The provider answers a protected profile's row with the notice that asks for its password.
    bool wants_password(const Page &page) const
    {
        if (page.rows.empty() || page.rows.front().id != "account:notice")
            return false;
        return work_.locked || page.rows.front().fact("prompt") == "password";
    }

    void failed(const Page &page, UpdateContext &context)
    {
        const std::string reason = !page.error.empty()                   ? page.error
                                   : page.load == model::Load::offline ? std::string(tr("The YouTube service cannot be reached."))
                                                                       : std::string(tr("That did not work. Try again."));
        if (sheet_.is_open())
        {
            // Typed text the provider refused: the sheet stays for another try.
            const bool secret = work_.step == Work::Step::password || work_.step == Work::Step::current;
            sheet_.refuse(page.error.empty() && secret ? std::string(tr("Wrong password.")) : reason, context.feedback);
            return;
        }
        context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.8f);
        context.nav.toast(reason, ToastKind::error);
        work_ = Work{};
    }

    void advance(const Page &page, UpdateContext &context)
    {
        Data &data = context.services.data;
        if (page.load != model::Load::ready)
        {
            failed(page, context);
            return;
        }
        const auto ask = [&](const char *title, const char *label, bool masked)
        { sheet_.open(title, label, masked, context.services.fonts, context.feedback); };
        switch (work_.kind)
        {
        case Work::Kind::none:
            break;
        case Work::Kind::watch:
        case Work::Kind::remove:
            if (work_.step == Work::Step::select && wants_password(page))
            {
                work_.step = Work::Step::password;
                sheet_.open(with(tr("Watch as {0}"), work_.name), tr("Password of this profile"), true, context.services.fonts, context.feedback);
            }
            else if (work_.step == Work::Step::select || work_.step == Work::Step::password)
            {
                sheet_.close();
                if (work_.kind == Work::Kind::watch)
                {
                    context.nav.toast(with(tr("Watching as {0}"), work_.name), ToastKind::success);
                    work_ = Work{};
                    context.nav.root(Route{ScreenId::home, {}, {}});
                }
                else
                {
                    work_.step = Work::Step::removing;
                    wait(data, "account:remove");
                }
            }
            else if (work_.step == Work::Step::removing)
            {
                const model::Row *apply = find_prefix(page, "account:apply:");
                if (apply == nullptr)
                {
                    Page refused;
                    refused.load = model::Load::failed;
                    failed(refused, context);
                    break;
                }
                const std::string id = apply->id;
                work_.step = Work::Step::applying;
                wait(data, id);
            }
            else
            {
                context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
                context.nav.toast(tr("Account removed"), ToastKind::success);
                focus_row_ = std::string(kProfileRow) + "guest";
                zone_ = Zone::tiles;
                work_ = Work{};
                forget_lists(data);
            }
            break;
        case Work::Kind::lock_set:
            if (work_.step == Work::Step::arming)
            {
                work_.step = Work::Step::fresh;
                ask(tr("Protect this profile with a password (experimental)"), tr("New password of this profile"), false);
            }
            else
                saved(tr("Password saved"), context);
            break;
        case Work::Kind::lock_change:
            if (work_.step == Work::Step::arming)
            {
                work_.step = Work::Step::current;
                ask(tr("Change the password of this profile (experimental)"), tr("Current password of this profile"), true);
            }
            else if (work_.step == Work::Step::current)
            {
                work_.step = Work::Step::fresh;
                ask(tr("Change the password of this profile (experimental)"), tr("New password of this profile"), false);
            }
            else
                saved(tr("Password saved"), context);
            break;
        case Work::Kind::lock_remove:
            if (work_.step == Work::Step::arming)
            {
                work_.step = Work::Step::current;
                ask(tr("Remove the password of this profile (experimental)"), tr("Current password of this profile"), true);
            }
            else
                saved(tr("Password removed"), context);
            break;
        }
    }

    void saved(const char *what, UpdateContext &context)
    {
        sheet_.close();
        context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
        context.nav.toast(what, ToastKind::success);
        work_ = Work{};
        forget_lists(context.services.data);
    }

    // ---- drawing -------------------------------------------------------------------

    // REQUEST: UpdateContext::look, so a kit component is dressed where it is updated (requests/F.md, 3).
    void dress(const DrawContext &context) const
    {
        if (worn_ != &context.look || picker_.style.reduced_motion != context.reduced_motion)
        {
            worn_ = &context.look;
            picker_.style.theme = context.look.theme;
            picker_.style.sounds.set = context.look.theme.sounds;
            picker_.style.reduced_motion = context.reduced_motion;
        }
        // The light around the focused tile is its picture's colour, or the look's own focus.
        // REQUEST: the avatar's tint from design.hpp, so a tile without a picture glows in its avatar's colour (requests/F.md, 5).
        const int count = static_cast<int>(picker_.profiles().size());
        for (int i = 0; i < count; ++i)
        {
            Color light = context.look.focus;
            if (i < static_cast<int>(tiles_.size()) && !tiles_[static_cast<std::size_t>(i)].avatar.empty())
            {
                const Picture picture = context.images.get(tiles_[static_cast<std::size_t>(i)].avatar, PictureSize::avatar);
                if (picture.ready())
                    light = picture.glow;
            }
            picker_.profile(i).accent = Color{light.r, light.g, light.b, 1.0f};
        }
    }

    // A tile's face: the profile's picture or its initial, a check on whoever is watching, a lock on a protected one.
    void draw_face(ui::Canvas &canvas, const Rect &area, int index) const
    {
        if (drawing_ == nullptr)
            return;
        G g{canvas.list, canvas.fonts, drawing_->look, canvas.glass, canvas.time, drawing_->reduced_motion};
        const float radius = area.w * 0.5f;
        if (index < 0 || index >= static_cast<int>(tiles_.size()))
        {
            // The tile that would add an account, in a build that does not sign in.
            const float reach = area.w * 0.36f;
            g.l.ring(area.cx(), area.cy(), reach, 3.0f, g.k.ink3);
            icon(g, Icon::block, area.cx(), area.cy(), reach * 1.1f, g.k.ink3);
            return;
        }
        const Tile &tile = tiles_[static_cast<std::size_t>(index)];
        const Picture picture = tile.avatar.empty() ? Picture{} : drawing_->images.get(tile.avatar, PictureSize::avatar);
        avatar(g, area.cx(), area.cy(), radius, shown_name(tile), tile.id, picture);
        const float at = radius * 0.7071f;
        if (tile.selected)
        {
            g.l.circle(area.cx() + at, area.cy() + at, 26.0f, g.k.panel_solid.with_alpha(1.0f));
            g.l.circle(area.cx() + at, area.cy() + at, 22.0f, g.k.accent);
            icon(g, Icon::check, area.cx() + at, area.cy() + at, 32.0f, g.k.on_accent);
        }
        if (tile.locked)
        {
            g.l.circle(area.cx() + at, area.cy() - at, 21.0f, g.k.panel_solid.with_alpha(1.0f));
            g.l.ring(area.cx() + at, area.cy() - at, 21.0f, 1.5f, g.k.hairline);
            icon(g, Icon::lock, area.cx() + at, area.cy() - at, 24.0f, g.k.ink);
        }
    }

    void draw_actions(G &g) const
    {
        const float shown = tween::clamp01(actions_shown_.value);
        if (shown <= 0.01f)
            return;
        Enter enter(g, arrive(age_, 3, 0.07f, 0.5f) * shown, 22.0f);
        if (work_.kind != Work::Kind::none && work_.age > 0.25f && !sheet_.visible())
        {
            // What was asked for is on its way.
            const std::string line = work_.kind == Work::Kind::watch    ? with(tr("Switching to {0}"), work_.name)
                                     : work_.kind == Work::Kind::remove ? with(tr("Removing {0}"), work_.name)
                                                                        : std::string(tr("One moment"));
            const std::string fitted = fit(g.f.semibold, line, kTypeBody, 900.0f);
            const float width = measure(g.f.semibold, fitted, kTypeBody);
            spinner(g, kCentre - width * 0.5f - 30.0f, kActionsTop + 32.0f, 16.0f, g.k.accent);
            text(g, g.f.semibold, fitted, kCentre + 12.0f, kActionsTop + 40.5f, kTypeBody, g.k.ink, gfx::Align::center);
            return;
        }
        overline(g, tr("For the profile in focus"), kCentre, 688.0f, g.k.ink3, gfx::Align::center);
        static constexpr Icon kIcons[] = {Icon::user, Icon::phone, Icon::close};
        const float shift = g.reduced ? 0.0f : ui::shake(zone_ == Zone::actions ? nudge_.value : 0.0f, g.time);
        for (int i = 0; i < action_count(); ++i)
        {
            Rect r = action_rect(i);
            r.x += action_focus_[i].value > 0.5f ? shift : 0.0f;
            const float focus = action_focus_[i].value;
            if (action_offered(i))
            {
                button(g, r, action_label(i), kIcons[i], focus);
            }
            else
            {
                // Not part of this build: it rests dimmed and takes a ring, not the plate.
                g.l.push_opacity(0.45f);
                button(g, r, action_label(i), kIcons[i], 0.0f);
                g.l.pop_opacity();
                focus_ring(g, r, r.h * 0.5f, g.k.focus, focus);
            }
        }
    }

    void draw_note(G &g) const
    {
        Enter enter(g, arrive(age_, 4, 0.07f, 0.5f), 22.0f);
        const bool on_phone = zone_ == Zone::actions && action_ == 1 && action_count() > 1;
        if (on_phone && casting_ == 0)
        {
            notice(g, kNotice, Icon::phone, tr("Not available in this build"),
                   tr("Linking a phone is not part of this built-in YouTube service."), g.k.theme.warning);
            return;
        }
        if (sign_in_ == 0)
        {
            notice(g, kNotice, Icon::info, tr("Sign-in to YouTube arrives in a later version"),
                   tr("This version browses and plays as Guest, without an account"), g.k.theme.warning);
            return;
        }
        const char *words = tiles_.empty()
                                ? tr("Nobody has a profile on this PS5 yet. Add an account to bring your subscriptions and your history.")
                                : tr("Accounts are signed in on this console with a code you approve on another device. The sign-in stays in the app's own folder on this PS5.");
        paragraph(g, g.f.regular, words, kCentre, 846.0f, 22.0f, 980.0f, 32.0f, g.k.ink2, 2, gfx::Align::center);
    }

    // The page before its list has arrived, in the list's own shape.
    void draw_bones(G &g) const
    {
        const float width = 4.0f * 300.0f + 3.0f * 32.0f;
        for (int i = 0; i < 4; ++i)
        {
            Enter enter(g, arrive(age_, 1 + i, 0.05f, 0.45f), 24.0f);
            const Rect card{kCentre - width * 0.5f + 332.0f * static_cast<float>(i), 248.5f, 300.0f, 330.0f};
            glass_panel(g, card, kRadiusPanel, 0.7f);
            bone(g, {card.cx() - 75.0f, card.y + 24.0f, 150.0f, 150.0f}, 75.0f, i);
            bone(g, {card.cx() - 80.0f, card.y + 200.0f, 160.0f, 24.0f}, 12.0f, i + 1);
            bone(g, {card.cx() - 104.0f, card.y + 240.0f, 208.0f, 16.0f}, 8.0f, i + 2);
        }
        Enter enter(g, arrive(age_, 5, 0.05f, 0.45f), 22.0f);
        const float widths[3] = {300.0f, 230.0f, 330.0f};
        float x = kCentre - (widths[0] + widths[1] + widths[2] + 2.0f * kActionGap) * 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            bone(g, {x, kActionsTop, widths[i], kActionHeight}, kActionHeight * 0.5f, i + 4);
            x += widths[i] + kActionGap;
        }
        bone(g, {kCentre - 430.0f, 830.0f, 860.0f, 16.0f}, 8.0f, 7);
        bone(g, {kCentre - 250.0f, 862.0f, 500.0f, 16.0f}, 8.0f, 8);
    }

    Route route_;
    const ui::Fonts *fonts_ = nullptr;
    float age_ = 0.0f;

    // What was read.
    model::Load load_ = model::Load::loading;
    std::string error_;
    std::vector<Tile> tiles_;
    bool have_tiles_ = false;
    std::string signature_;
    model::Profile profile_;
    int sign_in_ = -1; // 1: this build signs in to YouTube, 0: it does not, -1: not known yet
    int casting_ = -1; // the same for linking a phone

    // The focus.
    mutable ui::ProfilePicker picker_;
    mutable const Look *worn_ = nullptr;
    mutable const DrawContext *drawing_ = nullptr; // while draw() runs: what the picker's faces are drawn with
    Zone zone_ = Zone::tiles;
    int action_ = 0;
    tween::Spring action_focus_[3];
    tween::Spring actions_shown_;
    ui::Pulse nudge_;
    std::string focus_row_;      // the profile the focus is on, kept across lists
    std::string asked_at_start_; // a profile's row from the route

    // What is being done.
    Work work_;
    Question question_;
    Asking asking_ = Asking::nothing;
    bool selected_before_ = false;
    PasswordSheet sheet_;
};

} // namespace

std::unique_ptr<Screen> make_accounts()
{
    return std::make_unique<Accounts>();
}

} // namespace afterglow
