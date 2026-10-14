"""Afterglow's player button map; Classic and legacy aliases stay intact."""

PLAYER_INPUT = r'''
#if defined(EVO_AGC_DEVICE)
    // ProsperoTube H1: modal notices and scrubbing keep ownership of the pad.
    if (tube_afterglow_enabled() && (pressed & (PadButtons::Triangle | PadButtons::Square | PadButtons::Options))) {
        tube_sponsor_skip_t notice;
        if (playback->isScrubbing() || tube_phone_owns_media() ||
            tube_sponsor_prompt(playback->getPositionSeconds()) || tube_sponsor_last_skip(&notice)) return true;
        if (!screenMgr) return true;
        if (pressed & PadButtons::Triangle) {
            tube_extra_picker_request(TUBE_PICKER_SUGGESTIONS);
            screenMgr->navigateTo(ScreenId::AudioTrackPicker);
        } else if (pressed & PadButtons::Square) {
            screenMgr->navigateTo(ScreenId::SubtitlePicker);
        } else {
            tube_extra_picker_request(TUBE_PICKER_OPTIONS);
            screenMgr->navigateTo(ScreenId::AudioTrackPicker);
        }
        return true;
    }
    // End ProsperoTube H1 player map.
#endif
'''

DETAIL_ROWS = r'''
    // ProsperoTube H1 details: both displaced shortcuts stay reachable.
    if (tube_afterglow_picker_active()) {
        Entry media; media.kind = KindAfterglowDetails; media.label = "Details";
        m_tracks.push_back(std::move(media));
        Entry stats; stats.kind = KindAfterglowStats; stats.label = "Playback details";
        m_tracks.push_back(std::move(stats));
    }
    // End ProsperoTube H1 details rows.
'''

DETAIL_ACTIONS = r'''
    // ProsperoTube H1 details actions.
    if (e.kind == KindAfterglowDetails || e.kind == KindAfterglowStats) {
        if (sm) {
            if (e.kind == KindAfterglowDetails) sm->navigateTo(ScreenId::MediaInfo);
            else if (auto player = dynamic_cast<PlayerScreen*>(sm->getScreen(ScreenId::Player))) {
                player->toggleStatsForNerds();
                sm->navigateBack(ScreenId::Player);
            }
        }
        return;
    }
    // End ProsperoTube H1 details actions.
'''


def apply(work, replace, player):
    screen = player + "core/src/screens/PlayerScreen.cpp"
    replace(screen, '    if (pressed & PadButtons::Options) {\n', PLAYER_INPUT + '    if (pressed & PadButtons::Options) {\n')
    audio = player + "core/src/screens/AudioTrackPickerScreen.cpp"
    header = player + "core/include/evo/screens/AudioTrackPickerScreen.hpp"
    replace(header, 'KindBackgroundBrowse };', 'KindBackgroundBrowse, KindAfterglowDetails, KindAfterglowStats };')
    replace(audio, '#include "evo/screens/AudioTrackPickerScreen.hpp"\n',
            '#include "evo/screens/AudioTrackPickerScreen.hpp"\n#include "evo/screens/PlayerScreen.hpp"\n')
    replace(audio, '    // End ProsperoTube video action rows.\n', DETAIL_ROWS + '    // End ProsperoTube video action rows.\n')
    replace(audio, '    // ProsperoTube video action selection (extracted by host tests).\n',
            DETAIL_ACTIONS + '    // ProsperoTube video action selection (extracted by host tests).\n')
