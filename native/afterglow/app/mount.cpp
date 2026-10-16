// ProsperoTube Afterglow - the mount: the frame inside EVO's kit application.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app's Platform (EVO's AGC batch for textures, EVO's network for
// pictures, the asset bundle, the app's catalogs), the frame's life, the pad
// and the hand-over of a chosen video to the player the classic browser uses.
// Console only: nothing here is compiled for the host.

#include "afterglow/app/mount.hpp"
#include "afterglow/app/physical_keyboard.hpp"

#include "afterglow/app/afterglow.h"
#include "afterglow/app/data_provider.hpp"
#include "afterglow/app/picture_decode.hpp"
#include "afterglow/app/play_start.hpp"
#include "afterglow/frame.hpp"
#include "afterglow/app/update_prompt.hpp"
#include "afterglow/sound/app/sound_app.hpp" // stage S
#include "afterglow/text_engine.hpp"

#include "evo_rmlui_bundle.h"

extern "C"
{
#include "evo_boot_log.h"
#include "evo_data_path.h"
#include "evo_net.h"
#include "tube_i18n.h"
#include "tube_provider.h"
#include "tube_background_play.h"
#include "tube_ui_style.h"
}

#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace afterglow
{

namespace
{

double monotonic_seconds()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) * 1e-9;
}

bool file_exists(const char *name)
{
    std::FILE *file = std::fopen(evo_data_path(name), "rb");
    if (file == nullptr)
        return false;
    std::fclose(file);
    return true;
}

class AppPlatform final : public Platform
{
  public:
    void set_batch(evo::hui_agc::AgcBatch *batch)
    {
        batch_ = batch;
    }

    std::uint32_t create_texture(int width, int height, const std::uint8_t *rgba) override
    {
        return batch_ != nullptr ? batch_->create_texture(width, height, rgba) : 0;
    }
    bool update_texture(std::uint32_t handle, int width, int height, const std::uint8_t *rgba) override
    {
        return batch_ != nullptr && batch_->update_texture(handle, width, height, rgba);
    }
    void release_texture(std::uint32_t handle) override
    {
        if (batch_ != nullptr && handle != 0)
            batch_->release_texture(handle);
    }

    unsigned fetch(const std::string &url, Fetched done) override
    {
        if (!tube_art_url_allowed(url.c_str()))
            return 0;
        auto *request = new Request{next_ticket_, this, std::move(done)};
        if (evo_net_request_async("GET", url.c_str(), nullptr, nullptr, 0, &AppPlatform::fetched, request) != 0)
        {
            delete request;
            return 0;
        }
        waiting_.insert(next_ticket_);
        return next_ticket_++;
    }
    void cancel_fetch(unsigned ticket) override
    {
        waiting_.erase(ticket); // the answer still arrives; nobody is told
    }
    bool decode_picture(const std::string &bytes, int *width, int *height, std::vector<std::uint8_t> *rgba) override
    {
        return decode_webp(bytes, width, height, rgba); // what YouTube sends for a search's pictures
    }

    bool read_asset(const std::string &path, std::string *bytes) override
    {
        const EvoRmlBundleFile *file = evo_rmlui_bundle_find(path);
        if (file == nullptr)
            return false;
        bytes->assign(reinterpret_cast<const char *>(file->data), file->size);
        return true;
    }

    void wall_clock(int *hour, int *minute) override
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_r(&now, &local);
        *hour = local.tm_hour;
        *minute = local.tm_min;
    }

    const char *translate(const char *english) override
    {
        if (language_ != tube_i18n_generation())
        {
            language_ = tube_i18n_generation();
            texts_.clear();
        }
        const auto found = texts_.find(english);
        if (found != texts_.end())
            return found->second.c_str();
        char out[1024];
        const bool changed = tube_i18n_translate(english, out, sizeof out, TUBE_I18N_LABELS) != 0;
        return texts_.emplace(english, changed ? out : english).first->second.c_str();
    }
    // The app's catalogs match the finished English text against their counted patterns and pick the
    // form of the language's plural rule (native/tube_i18n.c).
    std::string translate_count(const char *one, const char *other, long count) override
    {
        const std::string english = fill(count == 1 ? one : other, std::to_string(count));
        char out[1024];
        return tube_i18n_translate(english.c_str(), out, sizeof out, TUBE_I18N_LABELS) != 0 ? std::string(out) : english;
    }
    unsigned language_revision() override
    {
        return tube_i18n_generation();
    }

    void log(const char *line) override
    {
        evo_boot_log("afterglow: %s", line);
    }

  private:
    struct Request
    {
        unsigned ticket;
        AppPlatform *platform;
        Fetched done;
    };
    static void fetched(int success, int status, const char *body, size_t size, void *user)
    {
        std::unique_ptr<Request> request(static_cast<Request *>(user));
        AppPlatform &self = *request->platform;
        if (self.waiting_.erase(request->ticket) == 0)
            return; // cancelled meanwhile
        const bool ok = success != 0 && status >= 200 && status < 300 && body != nullptr && size > 0;
        request->done(ok, ok ? std::string(body, size) : std::string());
    }

    evo::hui_agc::AgcBatch *batch_ = nullptr;
    std::set<unsigned> waiting_;
    unsigned next_ticket_ = 1;
    std::map<std::string, std::string> texts_;
    unsigned language_ = 0;
};

// The faces of the approved boards travel in the asset bundle; the text engine
// reads font files, so they are written into the app's data folder once.
std::string font_file(Platform &platform, const char *name)
{
    std::string bytes;
    if (!platform.read_asset(std::string("fonts/") + name, &bytes))
        return {};
    const std::string path = evo_data_path(name);
    std::FILE *have = std::fopen(path.c_str(), "rb");
    if (have != nullptr)
    {
        std::fseek(have, 0, SEEK_END);
        const long size = std::ftell(have);
        std::fclose(have);
        if (size == static_cast<long>(bytes.size()))
            return path;
    }
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
        return {};
    const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    std::fclose(file);
    return written ? path : std::string();
}

struct Mount
{
    AppPlatform platform;
    ProviderSource source;
    std::unique_ptr<TextEngine> text;
    std::unique_ptr<Frame> frame;
    UpdatePrompt update_prompt;
    gfx::DrawList world;
    gfx::DrawList ui;
    InputFrame input;
    model::Video play;
    bool play_pending = false;
    PlayStart start; // how the start of that video stands, while the player's screen is the interface's
    bool shown = false;
    bool failed = false;
    int scheme = -1;
    double last_render = 0.0;
    std::string snapshot;
};

Mount *g_mount = nullptr;
bool g_cold = true; // the first frame of this run opens with the opening moment

} // namespace

bool mount_render(evo::hui_agc::AgcBatch &batch, const hui::ui::Fonts &fonts, float dt, int width, int height)
{
    if (!tube_afterglow_enabled())
    {
        if (g_mount != nullptr)
            g_mount->shown = false;
        return false;
    }
    if (g_mount == nullptr)
        g_mount = new Mount();
    Mount &m = *g_mount;
    if (m.failed)
        return false;
    m.platform.set_batch(&batch);
    const TubeUiPrefs *prefs = tube_ui_prefs();
    if (!m.frame)
    {
        TextSetup setup;
        setup.regular = font_file(m.platform, "afterglow-inter-regular.ttf");
        setup.semibold = font_file(m.platform, "afterglow-inter-semibold.ttf");
        setup.display = font_file(m.platform, "afterglow-montserrat-medium.ttf");
        setup.mono = font_file(m.platform, "afterglow-dejavu-sans-mono.ttf");
        setup.font_dirs = tube::text::system_font_folders();
        setup.language = tube_i18n_language();
        setup.pixel_scale = static_cast<float>(height) / gfx::kVirtualHeight;
        setup.page_size = 1024;
        setup.max_pages = 4;
        std::string why;
        m.text = TextEngine::create(m.platform, setup, &why);
        if (m.text)
            m.text->bind(fonts);
        else
            evo_boot_log("afterglow: the text engine did not start (%s); the kit's baked fonts draw", why.c_str());
        FrameOptions options;
        options.scheme = prefs != nullptr ? prefs->scheme : 0;
        options.opening = g_cold;
        g_cold = false;
        m.scheme = options.scheme;
        m.source.set_player([](const model::Video &video)
        {
            if (g_mount == nullptr)
                return false;
            g_mount->play = video;
            g_mount->play_pending = true;
            g_mount->start.asked(monotonic_seconds());
            return true;
        });
        m.source.set_start(
            [](std::string *reason)
            {
                if (g_mount == nullptr)
                    return Source::Start::idle;
                const Source::Start state = g_mount->start.state(reason, monotonic_seconds());
                // The status line is the provider's or EVO's English: said in the profile's language where a catalog has it.
                if (state == Source::Start::failed && reason != nullptr && !reason->empty())
                    *reason = g_mount->platform.translate(reason->c_str());
                return state;
            },
            []()
            {
                if (g_mount == nullptr)
                    return false;
                if (g_mount->play_pending && g_mount->start.starting())
                {
                    // The provider screen has not even taken the video yet: it never will.
                    g_mount->play_pending = false;
                    g_mount->start.reset();
                    return true;
                }
                return g_mount->start.cancel();
            });
        m.frame = std::make_unique<Frame>(m.platform, m.source, fonts, options);
        // A startup section that is a protected profile: the accounts screen asks for its password first.
        if (const char *boot = tube_boot_section(); boot != nullptr && std::strncmp(boot, "library:profile:", 16) == 0)
            m.frame->open(Route{ScreenId::accounts, boot, {}});
        // stage S: the interface's recordings (read once per run), and the welcome of a cold start.
        sound::app_begin(m.platform, options.opening);
        evo_boot_log("afterglow: interface up, look %s, %dx%d", m.frame->look().id, width, height);
    }
    if (prefs != nullptr && prefs->scheme != m.scheme)
    {
        m.scheme = prefs->scheme;
        m.frame->set_scheme(m.scheme);
    }
    // Back on screen after the player or another screen of the app had it.
    const double now = monotonic_seconds();
    if (tube_background_play_take_browser_resume() || (m.last_render != 0.0 && now - m.last_render > 1.0))
    {
        m.start.reset(); // the player had the screen
        m.frame->resume();
    }
    else if (m.start.step(now))
    {
        m.frame->resume(); // the start ended without the player and without a failure of its own
    }
    const bool continuous_input = m.last_render != 0.0 && now - m.last_render < 0.5;
    m.last_render = now;

    if (m.text)
        m.text->begin_frame();
    // What the snapshot cannot know by itself: the size drawn at, and the text engine's numbers.
    m.frame->set_output(width, height);
    if (m.text)
    {
        const tube::text::Stats text = m.text->stats();
        m.frame->set_text_stats(true, static_cast<int>(text.pages), static_cast<int>(text.glyphs), static_cast<int>(text.faces_open),
                                static_cast<int>(m.text->missing()));
    }
    m.frame->set_background_playback(tube_background_play_active()
        ? (std::strlen(tube_background_play_title()) ? tube_background_play_title() : tr("Now playing")) : "",
        tube_background_play_paused()!=0);
    m.frame->update(m.input, dt, continuous_input ? keyboard::text() : std::string_view{});
    m.update_prompt.step(*m.frame);
    // stage S: what this step asked for is played and felt (the app's park and exit reach the sound through
    // tube_afterglow_sound.h, from EVO's application).
    sound::app_step(*m.frame, m.platform, dt);
    m.input = InputFrame{};
    m.world.clear();
    m.ui.clear();
    m.frame->draw(m.world, m.ui, 0); // no blurred copy on this backend yet: panels are solid
    if (m.text)
        m.text->end_frame();
    const gfx::Viewport viewport = gfx::fit_viewport(width, height);
    batch.draw(m.world, viewport, width, height);
    batch.draw(m.ui, viewport, width, height);
    m.shown = true;
    return true;
}

} // namespace afterglow

using afterglow::g_mount;

extern "C" int tube_afterglow_enabled(void)
{
    static double checked = -10.0;
    static bool on_file = false;
    static bool off_file = false;
    const double now = afterglow::monotonic_seconds();
    if (now - checked > 1.0)
    {
        checked = now;
        on_file = afterglow::file_exists("afterglow.on");
        off_file = afterglow::file_exists("afterglow.off");
    }
    if (off_file)
        return 0;
    const TubeUiPrefs *prefs = tube_ui_prefs();
    return on_file || (prefs != nullptr && prefs->interface_kind == TUBE_UI_INTERFACE_AFTERGLOW) ? 1 : 0;
}

extern "C" int tube_afterglow_shown(void)
{
    // The frame before last still counts: input arrives between two renders.
    return g_mount != nullptr && g_mount->shown && afterglow::monotonic_seconds() - g_mount->last_render < 0.5 ? 1 : 0;
}

extern "C" void tube_afterglow_input(uint32_t pressed, uint32_t held)
{
    using afterglow::Action;
    if (g_mount == nullptr)
        return;
    static const struct
    {
        uint32_t pad;
        Action action;
    } kButtons[] = {{hui::pad_bits::kCross, Action::confirm},   {hui::pad_bits::kCircle, Action::back},
                    {hui::pad_bits::kTriangle, Action::north},  {hui::pad_bits::kSquare, Action::west},
                    {hui::pad_bits::kL1, Action::page_prev},    {hui::pad_bits::kR1, Action::page_next},
                    {hui::pad_bits::kL2, Action::jump_prev},    {hui::pad_bits::kR2, Action::jump_next},
                    {hui::pad_bits::kOptions, Action::menu},    {hui::pad_bits::kTouchpad, Action::touch},
                    {hui::pad_bits::kL3, Action::l3},           {hui::pad_bits::kR3, Action::r3}};
    afterglow::InputFrame &input = g_mount->input;
    for (const auto &button : kButtons)
    {
        if ((pressed & button.pad) != 0)
            input.pressed |= hui::action_bit(button.action);
        if ((held & button.pad) != 0)
            input.held |= hui::action_bit(button.action);
    }
    const auto direction_of = [](uint32_t pad)
    {
        return (pad & hui::pad_bits::kUp) != 0     ? afterglow::Direction::up
               : (pad & hui::pad_bits::kDown) != 0 ? afterglow::Direction::down
               : (pad & hui::pad_bits::kLeft) != 0 ? afterglow::Direction::left
               : (pad & hui::pad_bits::kRight) != 0 ? afterglow::Direction::right
                                                    : afterglow::Direction::none;
    };
    // EVO's pad repeats a direction that stays down by itself: it is in `pressed` again after 220 ms
    // and then faster. Such a press is a repeat when the same direction was already down at the call
    // before; screens answer a repeat as a move and stay silent at an edge (input.nav_repeat).
    static afterglow::Direction down_before = afterglow::Direction::none;
    const afterglow::Direction down = direction_of(pressed);
    if (down != afterglow::Direction::none)
    {
        input.nav = down;
        input.nav_repeat = down == down_before;
    }
    down_before = direction_of(held);
}

extern "C" int tube_afterglow_take_play(char *item, size_t item_size, char *title, size_t title_size, int *is_live, long long *resume_sec)
{
    if (g_mount == nullptr || !g_mount->play_pending)
        return 0;
    g_mount->play_pending = false;
    const afterglow::model::Video &video = g_mount->play;
    std::snprintf(item, item_size, "%s", video.id.c_str());
    std::snprintf(title, title_size, "%s", video.title.c_str());
    if (is_live != nullptr)
        *is_live = (video.badges & afterglow::model::kBadgeLive) != 0 ? 1 : 0;
    if (resume_sec != nullptr)
        *resume_sec = video.progress > 0.02f && video.progress < 0.97f ? static_cast<long long>(video.progress * static_cast<float>(video.duration_s)) : 0;
    return 1;
}

extern "C" void tube_afterglow_play_busy(int pending, int cancellable)
{
    if (g_mount != nullptr)
        g_mount->start.busy(pending != 0, cancellable != 0);
}

extern "C" void tube_afterglow_status(const char *status, int error)
{
    if (g_mount != nullptr)
        g_mount->start.status(status, error != 0);
}

extern "C" int tube_afterglow_take_cancel(void)
{
    return g_mount != nullptr && g_mount->start.take_cancel() ? 1 : 0;
}

extern "C" int tube_afterglow_play_failed_recently(void)
{
    return g_mount != nullptr && g_mount->start.failed_recently(afterglow::monotonic_seconds()) ? 1 : 0;
}

extern "C" int tube_afterglow_take_exit(void)
{
    return g_mount != nullptr && g_mount->frame && g_mount->frame->take_exit_request() ? 1 : 0;
}

extern "C" uint64_t tube_afterglow_now_ms(void)
{
    return static_cast<uint64_t>(afterglow::monotonic_seconds() * 1000.0);
}

extern "C" const char *tube_afterglow_snapshot(void)
{
    if (!tube_afterglow_shown() || !g_mount->frame)
        return "";
    g_mount->snapshot = g_mount->frame->snapshot_json();
    return g_mount->snapshot.c_str();
}

extern "C" void tube_afterglow_close(void)
{
    afterglow::keyboard::close();
    if (g_mount == nullptr)
        return;
    g_mount->frame.reset();
    g_mount->text.reset();
    g_mount->shown = false;
    g_mount->last_render = 0.0;
}

extern "C" int tube_afterglow_background_return_allowed(void)
{
    return g_mount && g_mount->frame && g_mount->frame->background_return_allowed();
}
