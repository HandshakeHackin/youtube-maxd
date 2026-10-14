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

#include "afterglow/screens/accounts_screen.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace afterglow
{

namespace acct
{

Accounts::Accounts()
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

ScreenId Accounts::id() const
{
    return ScreenId::accounts;
}

void Accounts::enter(const Route &route, UpdateContext &context, bool restored)
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

void Accounts::update(const InputFrame &input, UpdateContext &context)
{
    dress(context);
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

    const int decided = std::exchange(*decision_, -2);
    if (decided != -2)
    {
        answered(decided, context);
        return;
    }
    const int typed = sheet_.update(input, context);
    if (typed == 1)
    {
        std::string text = sheet_.take();
        work_.ticket = data.answer(text);
        std::fill(text.begin(), text.end(), '\0');
        sheet_.set_busy(true);
        return;
    }
    if (typed == -1)
    {
        work_ = Work{};
        return;
    }
    if (!context.top || (asking_ != Asking::nothing) || sheet_.is_open())
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

void Accounts::read(Data &data)
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

void Accounts::sync()
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
            if (!tile.subscriptions.empty())
                card.extra = tr_n("{0} subscription", "{0} subscriptions", tile.subscriptions);
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

void Accounts::remember_focus()
{
    const Tile *tile = tile_in_focus();
    focus_row_ = tile != nullptr ? tile->row : std::string("add");
}

const Tile *Accounts::tile_in_focus() const
{
    const int at = picker_.focus();
    return at >= 0 && at < static_cast<int>(tiles_.size()) ? &tiles_[static_cast<std::size_t>(at)] : nullptr;
}

int Accounts::action_count() const
{
    const Tile *tile = load_ == model::Load::ready ? tile_in_focus() : nullptr;
    return tile == nullptr ? 0 : tile->guest ? 2 : 3;
}

bool Accounts::action_offered(int index) const
{
    return index == 0 ? sign_in_ != 0 : index == 1 ? casting_ != 0 : true;
}

void Accounts::refuse(const InputFrame &input, UpdateContext &context)
{
    if (input.nav_repeat)
        return; // a held direction stays silent
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    nudge_.trigger();
}

void Accounts::not_offered(UpdateContext &context)
{
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    nudge_.trigger();
}

void Accounts::update_tiles(const InputFrame &input, UpdateContext &context)
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

void Accounts::update_actions(const InputFrame &input, UpdateContext &context)
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
            context.nav.ask(Question{ui::StatusKind::danger, fill(tr("Remove {0} from this PS5?"), work_.name),
                          tr("The sign-in is deleted from this console. You continue as Guest; other accounts stay available."),
                          {tr("Keep"), tr("Remove")}, 1, 0, false}, [decision = decision_](int choice) { *decision = choice; });
        }
    }
}

void Accounts::password(UpdateContext &context)
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
        context.nav.toast(fill(tr("Watch as {0} first to set its password"), shown_name(*tile)), ToastKind::info);
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
    context.nav.ask(Question{ui::StatusKind::question, tr("Password protection (experimental)"),
                  tr("Asked before the profile is selected; this PS5 stores a salted hash, never the text"),
                  {tr("Change the password of this profile (experimental)"), tr("Remove the password of this profile (experimental)"),
                   tr("Cancel")},
                  -1, 0, true}, [decision = decision_](int choice) { *decision = choice; });
}

void Accounts::answered(int choice, UpdateContext &context)
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

void Accounts::wait(Data &data, const std::string &row)
{
    work_.ticket = data.act(row);
}

void Accounts::forget_lists(Data &data)
{
    data.forget("library:profiles");
    data.forget("library");
    data.forget("settings:remote");
}

void Accounts::watch(const Tile &tile, Data &data)
{
    work_ = Work{};
    work_.kind = Work::Kind::watch;
    work_.step = Work::Step::select;
    work_.row = tile.row;
    work_.name = shown_name(tile);
    work_.locked = tile.locked && !tile.selected;
    wait(data, tile.row);
}

bool Accounts::wants_password(const Page &page) const
{
    if (page.rows.empty() || page.rows.front().id != "account:notice")
        return false;
    return work_.locked || page.rows.front().fact("prompt") == "password";
}

void Accounts::failed(const Page &page, UpdateContext &context)
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

void Accounts::advance(const Page &page, UpdateContext &context)
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
            sheet_.open(fill(tr("Watch as {0}"), work_.name), tr("Password of this profile"), true, context.services.fonts, context.feedback);
        }
        else if (work_.step == Work::Step::select || work_.step == Work::Step::password)
        {
            sheet_.close();
            if (work_.kind == Work::Kind::watch)
            {
                context.nav.toast(fill(tr("Watching as {0}"), work_.name), ToastKind::success);
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

void Accounts::saved(const char *what, UpdateContext &context)
{
    sheet_.close();
    context.feedback.play(audio::Cue::saved, 1.0f, 0.0f, 0.7f);
    context.nav.toast(what, ToastKind::success);
    work_ = Work{};
    forget_lists(context.services.data);
}

} // namespace acct

std::unique_ptr<Screen> make_accounts()
{
    return std::make_unique<acct::Accounts>();
}

} // namespace afterglow
