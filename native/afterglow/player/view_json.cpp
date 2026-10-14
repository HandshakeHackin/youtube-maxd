// ProsperoTube Afterglow - a player view from JSON: fixture files and the steps of a board.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "afterglow/player/view.hpp"

#include "cJSON.h"

#include <cstdlib>
#include <cstring>

namespace afterglow::player
{

namespace
{

constexpr struct
{
    const char *name;
    ui::Button button;
} kButtons[] = {
    {"cross", ui::Button::cross},     {"circle", ui::Button::circle},         {"square", ui::Button::square},
    {"triangle", ui::Button::triangle}, {"l1", ui::Button::l1},               {"r1", ui::Button::r1},
    {"l2", ui::Button::l2},           {"r2", ui::Button::r2},                 {"options", ui::Button::options},
    {"dpad", ui::Button::dpad},       {"touchpad", ui::Button::touchpad},     {"left_stick", ui::Button::left_stick},
    {"right_stick", ui::Button::right_stick},
};

const cJSON *member(const cJSON *object, const char *name)
{
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

void get(const cJSON *object, const char *name, std::string *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsString(item) && item->valuestring != nullptr)
        *out = item->valuestring;
}

void get(const cJSON *object, const char *name, bool *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsBool(item))
        *out = cJSON_IsTrue(item);
}

void get(const cJSON *object, const char *name, int *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsNumber(item))
        *out = item->valueint;
}

void get(const cJSON *object, const char *name, float *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsNumber(item))
        *out = static_cast<float>(item->valuedouble);
}

void get(const cJSON *object, const char *name, double *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsNumber(item))
        *out = item->valuedouble;
}

// A colour as "#rrggbb" or as a number.
void get_rgb(const cJSON *object, const char *name, std::uint32_t *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsNumber(item))
        *out = static_cast<std::uint32_t>(item->valuedouble);
    else if (cJSON_IsString(item) && item->valuestring != nullptr)
        *out = static_cast<std::uint32_t>(std::strtoul(item->valuestring + (item->valuestring[0] == '#' ? 1 : 0), nullptr, 16));
}

void get(const cJSON *object, const char *name, std::vector<std::string> *out)
{
    const cJSON *list = member(object, name);
    if (!cJSON_IsArray(list))
        return;
    out->clear();
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, list)
    {
        if (cJSON_IsString(item) && item->valuestring != nullptr)
            out->emplace_back(item->valuestring);
    }
}

void read(const cJSON *object, Hint *hint)
{
    std::string button;
    std::string second;
    get(object, "button", &button);
    get(object, "second", &second);
    if (!button.empty())
        hint->button = button_by_name(button);
    if (!second.empty())
        hint->second = button_by_name(second);
    get(object, "label", &hint->label);
}

void read(const cJSON *object, Chapter *chapter)
{
    get(object, "start", &chapter->start);
    get(object, "title", &chapter->title);
}

void read(const cJSON *object, Segment *segment)
{
    get(object, "from", &segment->from);
    get(object, "to", &segment->to);
    get_rgb(object, "rgb", &segment->rgb);
    get(object, "point", &segment->point);
}

void read(const cJSON *object, Tile *tile)
{
    get(object, "art", &tile->art);
    get(object, "offset", &tile->offset);
    get(object, "aspect", &tile->aspect);
}

void read(const cJSON *object, ChatLine *line)
{
    get(object, "author", &line->author);
    get(object, "text", &line->text);
    get(object, "badge", &line->badge);
    get(object, "avatar", &line->avatar);
}

void read(const cJSON *object, Row *row)
{
    get(object, "label", &row->label);
    get(object, "detail", &row->detail);
    get(object, "value", &row->value);
    get(object, "tag", &row->tag);
    get(object, "icon", &row->icon);
    get(object, "current", &row->current);
    get(object, "focused", &row->focused);
    get(object, "action", &row->action);
    get(object, "disabled", &row->disabled);
    get(object, "chevron", &row->chevron);
}

void read(const cJSON *object, Choice *choice)
{
    get(object, "label", &choice->label);
    get(object, "note", &choice->note);
    get(object, "current", &choice->current);
}

void read(const cJSON *object, Card *card)
{
    get(object, "id", &card->id);
    get(object, "title", &card->title);
    get(object, "channel", &card->channel);
    get(object, "meta", &card->meta);
    get(object, "thumb", &card->thumb);
    get(object, "duration_s", &card->duration_s);
    int badges = static_cast<int>(card->badges);
    get(object, "badges", &badges);
    card->badges = static_cast<unsigned>(badges);
}

void read(const cJSON *object, DialogButton *button)
{
    get(object, "label", &button->label);
    get(object, "primary", &button->primary);
    get(object, "destructive", &button->destructive);
}

void read(const cJSON *object, Graph *graph)
{
    get(object, "label", &graph->label);
    get(object, "value", &graph->value);
    const cJSON *samples = member(object, "samples");
    if (cJSON_IsArray(samples))
    {
        graph->samples.clear();
        const cJSON *sample = nullptr;
        cJSON_ArrayForEach(sample, samples)
        {
            graph->samples.push_back(static_cast<float>(sample->valuedouble));
        }
    }
}

// The parts that hold lists and objects of their own, defined below the two helpers that read those.
void read(const cJSON *object, Captions *captions);
void read(const cJSON *object, Chat *chat);
void read(const cJSON *object, Sheet *sheet);
void read(const cJSON *object, UpNext *up_next);
void read(const cJSON *object, Notice *notice);
void read(const cJSON *object, NextVideo *next);
void read(const cJSON *object, Dialog *dialog);
void read(const cJSON *object, Toast *toast);
void read(const cJSON *object, Stats *stats);

// A list of objects: replaced whole when the document names it.
template <typename T> void get_list(const cJSON *object, const char *name, std::vector<T> *out)
{
    const cJSON *list = member(object, name);
    if (!cJSON_IsArray(list))
        return;
    out->clear();
    const cJSON *item = nullptr;
    cJSON_ArrayForEach(item, list)
    {
        T value{};
        read(item, &value);
        out->push_back(std::move(value));
    }
}

// An object inside the view: its members are merged into what is there.
template <typename T> void get_object(const cJSON *object, const char *name, T *out)
{
    const cJSON *item = member(object, name);
    if (cJSON_IsObject(item))
        read(item, out);
}

void read(const cJSON *object, Captions *captions)
{
    get(object, "lines", &captions->lines);
    get(object, "second", &captions->second);
    get(object, "size", &captions->size);
    get(object, "baseline", &captions->baseline);
    get(object, "raised", &captions->raised);
    get(object, "custom", &captions->custom);
    get_rgb(object, "text_rgb", &captions->text_rgb);
    get_rgb(object, "plate_rgb", &captions->plate_rgb);
    get(object, "plate_alpha", &captions->plate_alpha);
    get(object, "edge", &captions->edge);
    get(object, "second_top", &captions->second_top);
    get_rgb(object, "second_rgb", &captions->second_rgb);
}

void read(const cJSON *object, Chat *chat)
{
    get(object, "side", &chat->side);
    get(object, "theatre", &chat->theatre);
    get(object, "title", &chat->title);
    get(object, "filter", &chat->filter);
    get(object, "status", &chat->status);
    get_list(object, "lines", &chat->lines);
    get(object, "compose", &chat->compose);
    get_list(object, "hints", &chat->hints);
}

void read(const cJSON *object, Sheet *sheet)
{
    get(object, "open", &sheet->open);
    get(object, "kind", &sheet->kind);
    get(object, "title", &sheet->title);
    get(object, "tabs", &sheet->tabs);
    get(object, "tab", &sheet->tab);
    get(object, "pills", &sheet->pills);
    get_list(object, "rows", &sheet->rows);
    get(object, "first", &sheet->first);
    get(object, "total", &sheet->total);
    get(object, "choices_title", &sheet->choices_title);
    get_list(object, "choices", &sheet->choices);
    get(object, "choices_first", &sheet->choices_first);
    get(object, "choices_total", &sheet->choices_total);
    get(object, "note", &sheet->note);
    get(object, "preview", &sheet->preview);
    get(object, "preview_size", &sheet->preview_size);
    get(object, "empty_title", &sheet->empty_title);
    get(object, "empty_hint", &sheet->empty_hint);
    get_list(object, "hints", &sheet->hints);
    // A step may move the focus without repeating the rows.
    int focus = -1;
    get(object, "focus", &focus);
    if (focus >= 0)
    {
        for (std::size_t i = 0; i < sheet->rows.size(); ++i)
            sheet->rows[i].focused = static_cast<int>(i) == focus;
    }
}

void read(const cJSON *object, UpNext *up_next)
{
    get(object, "open", &up_next->open);
    get(object, "title", &up_next->title);
    get(object, "chip", &up_next->chip);
    get(object, "marks_next", &up_next->marks_next);
    get(object, "note", &up_next->note);
    get(object, "message", &up_next->message);
    get(object, "loading", &up_next->loading);
    get_list(object, "cards", &up_next->cards);
    get(object, "first", &up_next->first);
    get(object, "total", &up_next->total);
    get(object, "focus", &up_next->focus);
    get_list(object, "hints", &up_next->hints);
}

void read(const cJSON *object, Notice *notice)
{
    get(object, "shown", &notice->shown);
    get(object, "kind", &notice->kind);
    get(object, "label", &notice->label);
    get(object, "title", &notice->title);
    get(object, "detail", &notice->detail);
    get_rgb(object, "rgb", &notice->rgb);
    get_object(object, "action", &notice->action);
    get_object(object, "second", &notice->second);
    get(object, "left", &notice->left);
}

void read(const cJSON *object, NextVideo *next)
{
    get(object, "shown", &next->shown);
    get(object, "watched", &next->watched);
    get_object(object, "card", &next->card);
    get(object, "seconds_left", &next->seconds_left);
    get(object, "seconds_total", &next->seconds_total);
    get(object, "focus", &next->focus);
}

void read(const cJSON *object, Dialog *dialog)
{
    get(object, "shown", &dialog->shown);
    get(object, "kind", &dialog->kind);
    get(object, "title", &dialog->title);
    get(object, "body", &dialog->body);
    get_list(object, "buttons", &dialog->buttons);
    get(object, "focus", &dialog->focus);
    get(object, "progress", &dialog->progress);
}

void read(const cJSON *object, Toast *toast)
{
    get(object, "shown", &toast->shown);
    get(object, "title", &toast->title);
    get(object, "message", &toast->message);
    get(object, "kind", &toast->kind);
}

void read(const cJSON *object, Stats *stats)
{
    get(object, "shown", &stats->shown);
    get(object, "lines", &stats->lines);
    get_list(object, "graphs", &stats->graphs);
}

void read(const cJSON *object, PlayerView *view)
{
    get(object, "title", &view->title);
    get(object, "channel", &view->channel);
    get(object, "channel_id", &view->channel_id);
    get(object, "avatar", &view->avatar);
    get(object, "meta", &view->meta);
    get(object, "tags", &view->tags);
    get(object, "quality", &view->quality);
    get(object, "clock", &view->clock);
    get(object, "live", &view->live);
    get(object, "dvr", &view->dvr);
    get(object, "seek_start", &view->seek_start);
    get(object, "seek_end", &view->seek_end);
    get(object, "live_status", &view->live_status);
    get(object, "paused", &view->paused);
    get(object, "loading", &view->loading);
    get(object, "scrubbing", &view->scrubbing);
    get(object, "position", &view->position);
    get(object, "duration", &view->duration);
    get(object, "scrub_target", &view->scrub_target);
    get(object, "buffered", &view->buffered);
    get(object, "rate", &view->rate);
    get(object, "show_remaining", &view->show_remaining);
    get(object, "chrome", &view->chrome);
    get(object, "chrome_hidden", &view->chrome_hidden);
    get_list(object, "chapters", &view->chapters);
    get_list(object, "segments", &view->segments);
    get(object, "controls", &view->controls);
    get_list(object, "hints", &view->hints);
    get_list(object, "seek_hints", &view->seek_hints);
    get_list(object, "preview", &view->preview);
    get_object(object, "captions", &view->captions);
    get_object(object, "chat", &view->chat);
    get_object(object, "sheet", &view->sheet);
    get_object(object, "up_next", &view->up_next);
    get_object(object, "notice", &view->notice);
    get_object(object, "next", &view->next);
    get_object(object, "dialog", &view->dialog);
    get_object(object, "toast", &view->toast);
    get_object(object, "stats", &view->stats);
    get(object, "fps", &view->fps);
    get(object, "dim", &view->dim);
    get(object, "reduced_motion", &view->reduced_motion);
    get(object, "frame", &view->frame);
}

} // namespace

ui::Button button_by_name(std::string_view name)
{
    for (const auto &entry : kButtons)
    {
        if (name == entry.name)
            return entry.button;
    }
    return ui::Button::none;
}

const char *button_name(ui::Button button)
{
    for (const auto &entry : kButtons)
    {
        if (button == entry.button)
            return entry.name;
    }
    return "";
}

bool read_view(std::string_view json, PlayerView *view, std::string *error)
{
    cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
    if (!cJSON_IsObject(root))
    {
        if (error != nullptr)
            *error = "not a JSON object";
        cJSON_Delete(root);
        return false;
    }
    read(root, view);
    cJSON_Delete(root);
    return true;
}

} // namespace afterglow::player
