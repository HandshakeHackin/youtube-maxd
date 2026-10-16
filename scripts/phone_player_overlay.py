"""Connect Lounge commands to the shipping EVO controller and launch worker."""


def apply_phone_player_overlay(replace, player, host):
    replace(host, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_phone.h"')
    replace(host, 'static bool g_tube_recovery_launch;', '''static unsigned g_phone_ticket;
static double g_phone_resume;
static bool g_tube_recovery_launch;''')
    replace(host, '    g_pending.active_choice = idx;', '''    tube_phone_sync_scope();
    if (g_phone_ticket && !tube_phone_launch_current(g_phone_ticket)) {
        g_phone_ticket = 0; g_resolve_finished = true;
        evo_rmlui_provider_set_tuning(0); evo_rmlui_provider_set_loading(0, "");
        return -1;
    }
    g_pending.active_choice = idx;''')
    replace(host, '? g_tube_recovery_resume : (double)g_pending.resume_sec;', '? g_tube_recovery_resume : g_phone_ticket ? g_phone_resume : (double)g_pending.resume_sec;')
    replace(host, '    if (rc != 0) {\n        tube_recovery_finish();', '''    if (rc != 0) {
        if (g_phone_ticket) { tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0; }
        tube_recovery_finish();''')
    replace(host, '    if (!ok || count <= 0 || !choices) {', '''    if (!ok || count <= 0 || !choices) {
        if (g_phone_ticket) { tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0; }''')
    replace(host, '    if ((unsigned)(uintptr_t)ud != g_pick_gen) return;\n    if (g_tube_recovery_launch)', '''    if ((unsigned)(uintptr_t)ud != g_pick_gen) return;
    if (g_phone_ticket && !tube_phone_launch_current(g_phone_ticket)) {
        g_phone_ticket = 0; on_resolved(0, nullptr, 0, &g_pending); return;
    }
    if (g_tube_recovery_launch)''')
    replace(host, '    evo_provider_item_t queued;\n    if (tube_take_recovery', '''    evo_provider_item_t queued;
    char phone_video[12];
    if ((g_phone_ticket = tube_phone_take(phone_video, &g_phone_resume)) != 0) {
        tube_recovery_finish(); g_tube_recovery_launch = false;
        std::memset(&sel, 0, sizeof sel);
        std::snprintf(sel.provider_id, sizeof sel.provider_id, "youtube");
        std::snprintf(sel.item_id, sizeof sel.item_id, "%s", phone_video);
        std::snprintf(sel.title, sizeof sel.title, "Phone video");
    } else if (tube_take_recovery''')
    replace(host, '        g_pending.ask = tube_choose_stream_enabled();', '        g_pending.ask = g_phone_ticket ? 0 : tube_choose_stream_enabled();')
    replace(host, '                if (g_tube_recovery_launch) { tube_recovery_finish();', '''                if (g_phone_ticket) { tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0; }
                if (g_tube_recovery_launch) { tube_recovery_finish();''')
    replace(host, '        if (g_tube_recovery_launch) {\n            if (g_start_success', '''        if (g_phone_ticket && g_start_success) {
            const int reason = tube_phone_launch_reason(g_phone_ticket);
            const unsigned selection = tube_phone_selection();
            const int bound = tube_phone_launch_bind(g_phone_ticket, selection);
            const int finished = tube_phone_launch_finish(g_phone_ticket, bound);
            if (!finished) {
                static unsigned rejected_logs;
                if (rejected_logs++ < 24)
                    evo_bt("phone launch rejected: reason=%d after=%d selection=%d bound=%d remote=%d",
                        reason, tube_phone_launch_reason(g_phone_ticket), selection != 0, bound, tube_remote_state());
                if (auto pb = Application::getInstance().getPlaybackController()) pb->stopPlayback();
                g_start_success = false;
                g_pending.choice_count = 0; /* retired owner must not launch a fallback */
            }
            g_phone_ticket = 0;
        }
        if (g_tube_recovery_launch) {
            if (g_start_success''')
    replace(host, '            evo_rmlui_provider_set_status(have_why ? why', '''            if (g_phone_ticket) { tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0; }
            evo_rmlui_provider_set_status(have_why ? why''')
    replace(host, 'void ProviderHostScreen::onExit()\n{', '''void ProviderHostScreen::onExit()
{
    const bool joined_phone_worker = g_phone_ticket && g_start_running;''')
    replace(host, '    if (!m_navigatingToPlayer) { tube_recovery_finish();', '''    if (!m_navigatingToPlayer && g_phone_ticket) {
        if (joined_phone_worker && g_start_success) {
            if (auto pb = Application::getInstance().getPlaybackController()) pb->stopPlayback();
        }
        tube_phone_launch_finish(g_phone_ticket, 0); g_phone_ticket = 0;
    }
    if (!m_navigatingToPlayer) tube_phone_cancel();
    if (!m_navigatingToPlayer) { tube_recovery_finish();''')
    replace(host, 'void ProviderHostScreen::startSelected()', '''bool ProviderHostScreen::playbackStartupBusy() { return g_start_running.load(); }

void ProviderHostScreen::startSelected()''')
    replace(player + 'core/include/evo/screens/ProviderHostScreen.hpp', '    void setNavigatingToPlayer(bool b)', '    static bool playbackStartupBusy();\n    void setNavigatingToPlayer(bool b)')
    app = player + 'core/src/Application.cpp'
    replace(app, '        evo_net_poll();', '        tube_phone_sync_scope(); /* retire changed profiles before resolver callbacks */\n        evo_net_poll();')
    replace(app, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_phone.h"\n#include "tube_caption_apply.h"\n#include "evo_audio_out.h"\n#include "evo_toast.h"')
    replace(app, '        tube_poll();', '''        tube_poll();
        if (!ProviderHostScreen::playbackStartupBusy()) {
            tube_phone_controls phone{};
            phone.context = this;
            phone.recommendation = [](void*, unsigned owner, unsigned selection, const char* video, const char* playlist, int index, tube_phone_next_t* next) -> int {
                return tube_phone_recommendation(owner, selection, video, playlist, index, next);
            };
            phone.recommendation_cancel = [](void*) { tube_phone_recommendation_cancel(); };
            phone.stop_on_disconnect = tube_phone_stop_on_disconnect();
            phone.snapshot = [](void* ctx, tube_remote_player* p) -> int {
                auto pb = static_cast<Application*>(ctx)->getPlaybackController();
                p->volume = evo_audio_volume(); p->muted = p->volume == 0;
                if (!pb || !pb->isActive() || !tube_phone_video(pb->getCurrentFilePath().c_str(), p->video)) return 0;
                p->position = pb->getPositionSeconds(); p->duration = pb->getDurationSeconds();
                p->selection = tube_phone_selection();
                p->state = pb->isPaused() ? 2 : pb->getPlaybackState() == PlaybackState::Finished ? 0 : 1;
                if (prospero_subtitle_enabled && prospero_subtitle_count) std::snprintf(p->language, sizeof p->language, "%s", tube_phone_caption_language());
                return 1;
            };
            phone.pause = [](void* ctx, int paused) -> int {
                auto pb = static_cast<Application*>(ctx)->getPlaybackController();
                if (!pb || !pb->isActive()) return 0;
                pb->setPaused(paused != 0); return pb->isPaused() == (paused != 0);
            };
            phone.seek = [](void* ctx, double position) -> int {
                auto pb = static_cast<Application*>(ctx)->getPlaybackController();
                if (!pb || !pb->isActive() || pb->isLiveSource() || pb->isScrubbing()) return 0;
                double duration = pb->getDurationSeconds();
                if (duration > 0 && position >= duration) position = std::max(0.0, duration - 0.05);
                return pb->seekTo(position, pb->isPaused());
            };
            phone.stop = [](void* ctx) -> int {
                auto app = static_cast<Application*>(ctx); auto pb = app->getPlaybackController();
                if (!pb) return 0;
                pb->stopPlayback();
                if (auto sm = app->getScreenManager()) if (sm->getCurrentScreenId() == ScreenId::Player) sm->returnFromPlayback();
                return !pb->isActive();
            };
            phone.volume = [](void*, int value) -> int { evo_audio_set_volume(value); return evo_audio_volume() == value; };
            phone.key = [](void* ctx, const char* key) -> int {
                auto sm = static_cast<Application*>(ctx)->getScreenManager(); if (!sm) return 0;
                uint32_t button = !std::strcmp(key, "UP") ? PadButtons::Up : !std::strcmp(key, "DOWN") ? PadButtons::Down
                    : !std::strcmp(key, "LEFT") ? PadButtons::Left : !std::strcmp(key, "RIGHT") ? PadButtons::Right
                    : !std::strcmp(key, "ENTER") ? PadButtons::Cross : PadButtons::Circle;
                sm->handleInput(button, button, 0); sm->handleInput(0, 0, button); return 1;
            };
            phone.captions = [](void*, const char* language, unsigned owner, int sequence) -> int {
                if (!*language) { prospero_subtitle_enabled = 0; return 1; }
                if (prospero_subtitle_count && !std::strcmp(language, tube_phone_caption_language())) {
                    return prospero_subtitle_select_primary(-1);
                }
                return tube_caption_switch_begin(language, owner, sequence);
            };
            phone.captions_status = [](void*) -> int { return tube_caption_apply_ready(); };
            phone.browser = [](void* ctx) {
                auto sm = static_cast<Application*>(ctx)->getScreenManager(); if (!sm) return;
                ProviderHostScreen::setPendingProvider("youtube");
                if (sm->getCurrentScreenId() == ScreenId::Player) sm->returnFromPlayback();
                else if (sm->getCurrentScreenId() != ScreenId::EmbyBrowse) sm->navigateTo(ScreenId::EmbyBrowse);
            };
            if (tube_caption_switch_local()) {
                tube_remote_player current{};
                const bool secondary = tube_caption_switch_secondary() != 0;
                int result = phone.snapshot(this, &current) ? tube_caption_apply_ready() : -1;
                if (result < 0) { tube_caption_switch_cancel(); toast("CAPTIONS", "Couldn't load this language"); }
                else if (result > 0) toast(secondary ? "SECONDARY CAPTIONS" : "CAPTIONS",
                    secondary ? tube_caption_secondary_language() : tube_phone_caption_language());
            }
            tube_phone_tick(&phone);
        }''')
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_phone.h"')
    replace(screen, 'if (ended && tube_finish_drained_and_next(playback->getPositionSeconds()))', 'if (ended && (tube_phone_finished() || (!tube_phone_owns_media() && tube_finish_drained_and_next(playback->getPositionSeconds()))))')
    audio = player + 'media/src/evo_audio_out.c'
    replace(audio, '#include "evo_audio_out.h"', '#include "evo_audio_out.h"\n#include "tube_volume.h"')
    replace(audio, 'static int16_t audio_queue[', '''static int tube_volume = 100;
int evo_audio_volume(void) { return __atomic_load_n(&tube_volume, __ATOMIC_RELAXED); }
void evo_audio_set_volume(int value) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    __atomic_store_n(&tube_volume, value, __ATOMIC_RELAXED);
}
static int16_t audio_queue[''')
    replace(audio, '                const int submitted = sceAudioOutOutput(audio_handle, audio_queue[audio_queue_read]);', '''                static int16_t gained[AUDIO_BLOCK_SAMPLES * EVO_AUDIO_MAX_CH];
                tube_volume_pcm(gained, audio_queue[audio_queue_read],
                    (size_t)AUDIO_BLOCK_SAMPLES * evo_audio_channels, evo_audio_volume());
                const int submitted = sceAudioOutOutput(audio_handle, gained);''')
    replace(player + 'media/include/evo_audio_out.h', '#define EVO_AUDIO_MAX_CH', 'int evo_audio_volume(void);\nvoid evo_audio_set_volume(int value);\n\n#define EVO_AUDIO_MAX_CH')
