// ProsperoTube Afterglow - Home: what the controller does.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Left and right walk a shelf, up and down change shelves, and past the first
// card is the rail. Cross plays the video in focus or opens the topic, Triangle
// opens the video's page, Square puts it on Watch later (where there is no
// account, and on the topics, Square searches), Options signs in while the
// invitation stands, Touchpad opens the actions of the video in focus.

#include "afterglow/screens/home.hpp"

namespace afterglow::home_page
{

void Home::refuse(UpdateContext &context, float direction)
{
    context.feedback.play(audio::Cue::error, 1.0f, 0.0f, 0.6f);
    context.feedback.rumble(0.25f, 0.05f);
    nudge_dir_ = direction;
    nudge_.trigger();
}

void Home::handle(const InputFrame &input, UpdateContext &context)
{
    Row &row = rows_[static_cast<std::size_t>(row_)];
    RowState &s = state(row);
    const int count = row.count();
    switch (input.nav)
    {
    case Direction::left:
    case Direction::right:
    {
        const int next = s.col + (input.nav == Direction::right ? 1 : -1);
        if (next >= 0 && next < count)
        {
            s.col = next;
            context.feedback.play(audio::Cue::focus, 1.0f, ui::pan_for_x(card_rect(row_, next).cx()));
        }
        else if (next < 0)
        {
            context.nav.open_rail(); // past the first card is the rail
        }
        else if (!input.nav_repeat)
        {
            refuse(context, 1.0f);
        }
        break;
    }
    case Direction::up:
    case Direction::down:
    {
        const int next = row_ + (input.nav == Direction::down ? 1 : -1);
        if (next >= 0 && next < static_cast<int>(rows_.size()))
        {
            row_ = next;
            wanted_ = rows_[static_cast<std::size_t>(row_)].shelf.id;
            // Rows sound like steps: lower shelves, lower pitch.
            context.feedback.play(audio::Cue::tab, 1.1f - 0.07f * static_cast<float>(row_));
        }
        else if (!input.nav_repeat)
        {
            refuse(context, 0.0f);
        }
        break;
    }
    case Direction::none:
        break;
    }
    const Row &now = rows_[static_cast<std::size_t>(row_)];
    const int col = state(now).col;
    if (pressed(input, Action::back))
    {
        context.nav.back();
    }
    else if (pressed(input, Action::west) && (!signed_in_ || now.topics() || now.count() == 0))
    {
        context.nav.push(Route{ScreenId::search, {}, {}});
    }
    else if (pressed(input, Action::menu) && invite_)
    {
        context.nav.push(Route{ScreenId::sign_in, {}, {}}, Move::fade);
    }
    else if (now.count() == 0)
    {
        return;
    }
    else if (now.topics())
    {
        if (pressed(input, Action::confirm))
        {
            const model::Topic &topic = now.shelf.topics[static_cast<std::size_t>(col)];
            context.nav.push(Route{ScreenId::grid, topic.id, topic.title});
        }
    }
    else
    {
        const model::Video &video = now.shelf.videos[static_cast<std::size_t>(col)];
        if (pressed(input, Action::confirm))
        {
            // Picture to player: the card's picture becomes the whole screen.
            play_video(context, video, card_rect(row_, col), card_rect(row_, col).cx());
        }
        else if (pressed(input, Action::north))
        {
            context.nav.push(Route{ScreenId::video, video.id, video.title}, Move::zoom, ring_.value(), video.thumb);
        }
        else if (pressed(input, Action::west))
        {
            // Watch later: the provider names the change (a confirm row), then makes it (its apply row).
            if (later_.running)
            {
                refuse(context, 0.0f); // one at a time: the answer of the last one is on its way
            }
            else
            {
                later_.start(context.services.data, "account:confirm:watch_later_add:" + video.id);
                later_title_ = video.title;
                context.feedback.play(audio::Cue::select, 1.0f, ui::pan_for_x(card_rect(row_, col).cx()));
            }
        }
        else if (pressed(input, Action::touch))
        {
            // The actions of the video in focus, as the frame's sheet. What they change, Home reads again.
            // A Guest's shelves are YouTube's public lists, which no action here changes: only what is kept on this PS5 is read again.
            Data *data = &context.services.data;
            const bool account = signed_in_;
            context.nav.actions(video.id, video.title, [data, account]
            {
                data->forget("history");
                if (!account)
                    return;
                for (const char *catalog : {"library:home", "library:subscriptions", "section:trending", "library:watch_later"})
                    data->forget(catalog);
            });
        }
    }
}

} // namespace afterglow::home_page
