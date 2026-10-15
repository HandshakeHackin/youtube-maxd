"""Continue the same player behind Afterglow; no hidden next-video offers."""
import shutil

def apply(root,work,replace,player):
    for name in ('tube_background_play.c','tube_background_play.h'):
        shutil.copyfile(root/'native'/name,work/player/('addons/src' if name.endswith('.c') else 'addons/include')/name)
    replace(player+'Makefile','    addons/src/tube_backup.c','    addons/src/tube_backup.c \\\n    addons/src/tube_background_play.c')
    app=player+'core/src/Application.cpp'
    replace(app,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_background_play.h"\n#include "tube_afterglow.h"')
    replace(app,'        m_screenManager->update(frameDeltaMs);',r"""        // ProsperoTube background ownership: retire before updating another profile or owner.
        uint64_t tube_hidden_epoch=0,tube_hidden_serial=0;
        if (tube_background_play_active()) {
            if (tube_phone_owns_media()) tube_background_play_clear();
            else if (m_screenManager->getCurrentScreenId()==ScreenId::Player) {
                if (tube_background_play_take_end_pause() && m_playbackController &&
                    m_playbackController->isActive() &&
                    tube_background_play_matches(tube_caption_selection(),tube_browse_profile()))
                    m_playbackController->setPaused(false);
                tube_background_play_clear();
            }
            else if (m_softClose || !tube_afterglow_enabled() || !m_playbackController ||
                !m_playbackController->isActive() ||
                !tube_background_play_matches(tube_caption_selection(),tube_browse_profile())) {
                if (m_playbackController && m_playbackController->isActive()) m_playbackController->stopPlayback();
                tube_background_play_clear();
            }
        }
        m_screenManager->update(frameDeltaMs);
        // Browser screens do not tick PlayerScreen. Keep the same lifecycle running without drawing it.
        if (tube_background_play_active() && m_screenManager->getCurrentScreenId()!=ScreenId::Player) {
            if (auto hiddenPlayer=m_screenManager->getScreen(ScreenId::Player)) hiddenPlayer->update(frameDeltaMs);
            if (m_playbackController && m_playbackController->isActive()) {
                tube_background_play_status(m_playbackController->getDisplayTitle().c_str(),m_playbackController->isPaused());
                // No new video draw is submitted; discard retires after the browser present below.
                pp_video_frame hiddenFrame{};
                if (pp_playback_get_video_frame(&g_pp_pb,&hiddenFrame) && hiddenFrame.ready) {
                    tube_hidden_epoch=hiddenFrame.tube_epoch;tube_hidden_serial=hiddenFrame.tube_serial;
                }
            }
        }
        // End ProsperoTube background ownership.""")
    replace(app,'            evo_agc_runtime_present();',r"""            evo_agc_runtime_present();
            // ProsperoTube hidden discard waits for the prior picture's GPU lifetime too.
            if (tube_hidden_serial && evo_agc_runtime_last_present_retired())
                pp_playback_note_retired(&g_pp_pb,tube_hidden_epoch,tube_hidden_serial);
            // End ProsperoTube hidden discard.""")
    screen=player+'core/src/screens/PlayerScreen.cpp'
    replace(screen,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_background_play.h"')
    # One EOF pause, before any watched/next-video state is consumed. Foreground flow remains unchanged.
    replace(screen,'    if (!tube_phone_owns_media()) tube_completion_playback_state(ended,completionBusy);',r"""    // ProsperoTube background completion: no invisible offers or automatic navigation.
    if (tube_background_play_active()) {
        if (completionBusy || !playback->isPaused()) tube_background_play_end_pause(0);
        if (ended && !completionBusy) {
            if (!playback->isPaused()) {
                playback->setPaused(true);
                tube_background_play_end_pause(1);
            }
            return;
        }
    }
    // End ProsperoTube background completion.
    if (!tube_phone_owns_media()) tube_completion_playback_state(ended,completionBusy);""")
    # Failure/recovery stop still runs, but must not take the frame out of its current browse route.
    source=(work/screen).read_text();a=source.index('void PlayerScreen::update(');b=source.index('void PlayerScreen::feedPerformanceHud',a)
    chunk=source[a:b];new=chunk.replace('sm->returnFromPlayback();','if (!tube_background_play_active()) sm->returnFromPlayback();')
    replace(screen,chunk,new)
    picker=player+'core/src/screens/AudioTrackPickerScreen.cpp'
    replace(picker,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_background_play.h"')
    replace(player+'core/include/evo/screens/AudioTrackPickerScreen.hpp','KindPreset };','KindPreset, KindBackgroundBrowse };')
    replace(picker,'    // End ProsperoTube playback mode rows.',r"""    // End ProsperoTube playback mode rows.
    // ProsperoTube explicit background entry; asks/offers remain visible in the player.
    tube_next_offer_t nextOffer;
    if (tube_afterglow_picker_active() && playback->isActive() && !tube_phone_owns_media() &&
        tube_caption_selection() && !playback->isScrubbing() && !tube_next_offer(&nextOffer) &&
        !tube_manual_navigation_pending() && !tube_playlist_eof_pending() &&
        !tube_sponsor_prompt(playback->getPositionSeconds())) {
        Entry e;e.kind=KindBackgroundBrowse;e.label="Keep playing while browsing";
        e.detail="L3: return to video";m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube explicit background entry.""")
    replace(picker,'    } else if (e.kind == KindVideoNavigation) {',r"""    } else if (e.kind == KindBackgroundBrowse) {
        tube_next_offer_t nextOffer;
        if (tube_afterglow_picker_active() && playback->isActive() && !tube_phone_owns_media() &&
            !playback->isScrubbing() && !tube_next_offer(&nextOffer) && !tube_playlist_eof_pending() &&
            !tube_manual_navigation_pending() && !tube_sponsor_prompt(playback->getPositionSeconds()) &&
            tube_background_play_begin(tube_caption_selection(),tube_browse_profile())) {
            tube_background_play_status(playback->getDisplayTitle().c_str(),playback->isPaused());
            if (sm) sm->returnFromPlayback();
        }
    } else if (e.kind == KindVideoNavigation) {""")
    host=player+'core/src/screens/ProviderHostScreen.cpp'
    replace(host,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_background_play.h"')
    replace(host,'    if (!m_streamPick && tube_afterglow_shown()) {',r"""    if (!m_streamPick && tube_afterglow_shown()) {
        // Voice recording/transcription and discovery/sign-in guards above keep their input ownership.
        if (tube_background_play_active() && pressed==PadButtons::L3 && !evo_keyboard_is_open() && tube_afterglow_background_return_allowed()) {
            auto &app=Application::getInstance();
            if (auto playback=app.getPlaybackController())
                if (tube_background_play_take_end_pause() && playback->isActive() &&
                    tube_background_play_matches(tube_caption_selection(),tube_browse_profile()))
                    playback->setPaused(false);
            tube_background_play_clear();
            if (auto sm=app.getScreenManager()) sm->navigateTo(ScreenId::Player);
            return true;
        }""")
    # Keep the old audio through resolution failures. Retire the background lease only at launch commit.
    replace(host,'    g_pending.active_choice = idx;',r"""    const bool backgroundReplace=tube_background_play_active();
    tube_background_play_clear();
    g_pending.active_choice = idx;""")
    replace(host,'    if (rc != 0) {\n        if (g_phone_ticket) { tube_phone_launch_finish',r"""    if (rc != 0) {
        if (backgroundReplace) {
            if (auto pb=Application::getInstance().getPlaybackController()) pb->stopPlayback();
        }
        if (g_phone_ticket) { tube_phone_launch_finish""")
    # Recovery may reopen a decoder and navigate. Defer it until the user returns to the visible player.
    path=work/screen;source=path.read_text();a=source.index('// ProsperoTube error fixer: decoder failure');b=source.index('// End ProsperoTube error fixer: decoder failure',a)
    chunk=source[a:b];replace(screen,chunk,chunk.replace('if (!tube_phone_owns_media())','if (!tube_background_play_active() && !tube_phone_owns_media())'))
    replace(screen,'if (tube_queue_recovery(playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled))',
        'if (!tube_background_play_active() && tube_queue_recovery(playback->getPositionSeconds(), playback->isLiveSource(), prospero_subtitle_enabled))')
    replace(screen,'else if (tube_buffering_tick(&m_tubeBuffering, now,',
        'else if (!tube_background_play_active() && tube_buffering_tick(&m_tubeBuffering, now,')

    # Explicit controls supersede our automatic EOF pause, including a repeated manual pause.
    controller=player+'core/src/services/PlaybackController.cpp'
    replace(controller,'#include "tube_provider.h"','#include "tube_provider.h"\n#include "tube_background_play.h"')
    for signature in ('void PlaybackController::setPaused(bool paused)',
                      'void PlaybackController::stopPlayback()',
                      'void PlaybackController::beginScrub()',
                      'bool PlaybackController::seekTo(double targetSeconds, bool keepPaused)'):
        replace(controller,signature+' {',signature+' {\n    tube_background_play_end_pause(0);')
