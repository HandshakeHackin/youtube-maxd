"""Player preferences from native/tube_player_extra: caption look, volume and
audio delay, controls-bar labels, exit and D-pad behaviour, chapters and speed
lists on both shipping HUDs and in the in-player options picker."""


def _audio(replace, player):
    audio = player + 'media/src/evo_audio_out.c'
    header = player + 'media/include/evo_audio_out.h'
    playback = player + 'media/src/evo_playback.c'
    replace(audio, '#include "tube_volume.h"', '#include "tube_volume.h"\n#include "tube_audio_delay.h"')
    replace(audio, '''    __atomic_store_n(&tube_volume, value, __ATOMIC_RELAXED);
}''', '''    __atomic_store_n(&tube_volume, value, __ATOMIC_RELAXED);
}
/* ProsperoTube player gain and audio delay: written by the UI thread, read by
 * the output thread. The phone volume above keeps its own 0..100 meaning. */
static int tube_player_gain = TUBE_VOLUME_UNITY;
static int tube_delay_ms;
int evo_audio_player_gain(void) { return __atomic_load_n(&tube_player_gain, __ATOMIC_RELAXED); }
void evo_audio_set_player_gain(int milli) {
    if (milli < 0) milli = 0;
    if (milli > TUBE_VOLUME_MAX_MILLI) milli = TUBE_VOLUME_MAX_MILLI;
    __atomic_store_n(&tube_player_gain, milli, __ATOMIC_RELAXED);
}
int evo_audio_delay_ms(void) { return __atomic_load_n(&tube_delay_ms, __ATOMIC_RELAXED); }
void evo_audio_set_delay_ms(int ms) {
    __atomic_store_n(&tube_delay_ms, tube_audio_delay_clamp_ms(ms), __ATOMIC_RELAXED);
}
#define TUBE_DELAY_CAPACITY (TUBE_AUDIO_DELAY_MAX_FRAMES + AUDIO_BLOCK_SAMPLES + 1)
static int16_t tube_delay_ring[TUBE_DELAY_CAPACITY * EVO_AUDIO_MAX_CH];
static tube_audio_delay tube_delay_line;
/* End ProsperoTube player gain and audio delay. */''')
    replace(header, 'int evo_audio_volume(void);\nvoid evo_audio_set_volume(int value);', '''int evo_audio_volume(void);
void evo_audio_set_volume(int value);
/* Player gain in thousandths (0..3000) and audio delay in ms; UI thread setters. */
int evo_audio_player_gain(void);
void evo_audio_set_player_gain(int milli);
int evo_audio_delay_ms(void);
void evo_audio_set_delay_ms(int ms);''')
    replace(audio, '''    uint64_t drain_epoch = tube_audio_epoch(&tube_drain);
    unsigned drain_silence = 0;''', '''    uint64_t drain_epoch = tube_audio_epoch(&tube_drain);
    unsigned drain_silence = 0;
    tube_audio_delay_init(&tube_delay_line, tube_delay_ring, TUBE_DELAY_CAPACITY, EVO_AUDIO_MAX_CH);
    tube_audio_delay_reset(&tube_delay_line);''')
    # A new stream epoch or a seek must not play audio queued before it.
    replace(audio, '        if (epoch != drain_epoch) { drain_epoch = epoch; drain_silence = 0; }',
            '        if (epoch != drain_epoch) { drain_epoch = epoch; drain_silence = 0; tube_audio_delay_reset(&tube_delay_line); }')
    replace(audio, '''            video_rel_at_wait = -1.0;
            video_stuck_iters = 0;
            usleep(2000);
            continue;''', '''            video_rel_at_wait = -1.0;
            video_stuck_iters = 0;
            tube_audio_delay_reset(&tube_delay_line);
            usleep(2000);
            continue;''')
    # With a negative delay the picture trails the audio clock on purpose.
    replace(audio, '                audio_clock_seconds > video_rel + EVO_AV_AUDIO_LEAD_MAX_SEC &&',
            '                audio_clock_seconds > video_rel + evo_pb_video_hold_s() + EVO_AV_AUDIO_LEAD_MAX_SEC &&')
    replace(audio, '''                tube_volume_pcm(gained, audio_queue[audio_queue_read],
                    (size_t)AUDIO_BLOCK_SAMPLES * evo_audio_channels, evo_audio_volume());
                const int submitted = sceAudioOutOutput(audio_handle, gained);''', '''                tube_volume_pcm_gain(gained, audio_queue[audio_queue_read],
                    (size_t)AUDIO_BLOCK_SAMPLES * evo_audio_channels,
                    tube_volume_total_milli(evo_audio_volume(), evo_audio_player_gain()));
                static int16_t delayed[AUDIO_BLOCK_SAMPLES * EVO_AUDIO_MAX_CH];
                const int submitted = sceAudioOutOutput(audio_handle, tube_audio_delay_process(&tube_delay_line,
                    gained, delayed, AUDIO_BLOCK_SAMPLES, evo_audio_channels,
                    tube_audio_delay_frames(evo_audio_delay_ms()), 1));''')
    # Silence first pushes delayed audio out; only then can the tail be retired.
    replace(audio, '''                const int submitted = sceAudioOutOutput(audio_handle, silence);
                if (submitted < 0) {''', '''                const int tube_delay_pending = tube_audio_delay_tail(&tube_delay_line) != 0;
                static int16_t delayed_tail[AUDIO_BLOCK_SAMPLES * EVO_AUDIO_MAX_CH];
                const int submitted = sceAudioOutOutput(audio_handle, tube_delay_pending
                    ? tube_audio_delay_process(&tube_delay_line, silence, delayed_tail, AUDIO_BLOCK_SAMPLES,
                        evo_audio_channels, tube_audio_delay_frames(evo_audio_delay_ms()), 0)
                    : silence);
                if (submitted < 0) {''')
    replace(audio, '''                } else if (video_decode_done &&
                           (tube_audio_flags(&tube_drain, drain_epoch) & TUBE_AUDIO_DECODED)) {''',
            '''                } else if (video_decode_done && !tube_delay_pending &&
                           (tube_audio_flags(&tube_drain, drain_epoch) & TUBE_AUDIO_DECODED)) {''')
    # Negative delay: the picture waits for the audio clock minus the hold.
    replace(playback, '''double prospero_media_clock_seconds(void)
{''', '''/* ProsperoTube video hold: how far the picture trails the audio clock when
 * the audio delay is negative. Written by the UI thread. */
static int tube_video_hold_ms;
void evo_pb_set_video_hold_ms(int ms) {
    if (ms < 0) ms = 0;
    if (ms > 2000) ms = 2000;
    __atomic_store_n(&tube_video_hold_ms, ms, __ATOMIC_RELAXED);
}
double evo_pb_video_hold_s(void) { return __atomic_load_n(&tube_video_hold_ms, __ATOMIC_RELAXED) / 1000.0; }
/* End ProsperoTube video hold. */

double prospero_media_clock_seconds(void)
{''')
    replace(player + 'media/include/evo_playback.h', 'double prospero_media_clock_seconds(void);',
            'double prospero_media_clock_seconds(void);\nvoid evo_pb_set_video_hold_ms(int ms);\ndouble evo_pb_video_hold_s(void);')
    replace(playback, '''           (int64_t)(audio_clock_seconds * 1000000.0);''',
            '''           (int64_t)((audio_clock_seconds - evo_pb_video_hold_s()) * 1000000.0);''')
    replace(playback, '''            audio_rel = audio_clock_seconds;
            behind = audio_rel - video_rel; /* >0 => video late; <0 => video early */''',
            '''            audio_rel = audio_clock_seconds - evo_pb_video_hold_s();
            behind = audio_rel - video_rel; /* >0 => video late; <0 => video early */''')
    replace(playback, '''                    audio_rel = audio_clock_seconds;
                    behind = audio_rel - video_rel;''', '''                    audio_rel = audio_clock_seconds - evo_pb_video_hold_s();
                    behind = audio_rel - video_rel;''')


def _player_screen(replace, player):
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '#include "evo/Application.hpp"\n#include "evo_rmlui_bridge.h"',
            '#include "tube_player_extra.h"\n#include "evo/Application.hpp"\n#include "evo_rmlui_bridge.h"')
    replace(screen, '''    m_playerFsm.postEvent(PlayerScreenEvent::ShowOsd);
}

void PlayerScreen::onExit() {''', '''    m_playerFsm.postEvent(PlayerScreenEvent::ShowOsd);
    tube_extra_exit_reset();
}

void PlayerScreen::onExit() {''')
    replace(screen, '''    if (pressed & PadButtons::Circle) {
        // Stop playback confirmation prompt''', '''    if (pressed & PadButtons::Circle) {
        // ProsperoTube player exit (extracted by host tests).
        const int exitAction = tube_extra_exit_press(NowMs());
        if (exitAction == TUBE_EXIT_ARMED) {
            toast("PLAYBACK", "Press Circle again to exit");
            return true;
        }
        if (exitAction == TUBE_EXIT_NOW) {
            evo_feedback(EVO_FB_CONFIRM);
            playback->stopPlayback();
            if (screenMgr) screenMgr->returnFromPlayback();
            return true;
        }
        // End ProsperoTube player exit.
        // Stop playback confirmation prompt''')
    replace(screen, '''    /* Live TV has no timeline: seek and chapter keys are swallowed, not sent''', '''    // ProsperoTube D-pad actions (extracted by host tests).
    for (int axis = 0; axis < 2 && !playback->isScrubbing(); ++axis) {
        const uint32_t keys = pressed & (axis ? (PadButtons::Up | PadButtons::Down) : (PadButtons::Left | PadButtons::Right));
        if (!keys) continue;
        const bool controlsHidden = m_osdVisibilityAlpha <= 0;
        const int action = tube_extra_dpad_action(axis, controlsHidden);
        if (action == TUBE_KEY_ACTION_DEFAULT) continue;
        if (action == TUBE_KEY_ACTION_SUGGESTIONS) {
            // Up keeps its own action; Down lists related videos over the picture.
            if (!(keys & PadButtons::Down) || tube_phone_owns_media() || !screenMgr) continue;
            tube_extra_picker_request(TUBE_PICKER_SUGGESTIONS);
            screenMgr->navigateTo(ScreenId::AudioTrackPicker);
            return true;
        }
        // Quick skip and adjustments made with hidden controls keep them hidden.
        if (controlsHidden) m_tubeSuppressControls = true;
        if (action == TUBE_KEY_ACTION_SKIP) {
            const int direction = (keys & (PadButtons::Right | PadButtons::Down)) ? 1 : -1;
            if (!tube_phone_owns_media() && tube_manual_navigation_begin(direction))
                toast("PLAYBACK", direction > 0 ? "Next video" : "Previous video");
            else toast("PLAYBACK", "No video available in this direction");
        } else if (action == TUBE_KEY_ACTION_VOLUME) {
            char label[32];
            std::snprintf(label, sizeof label, "%d%%", tube_extra_volume_step((keys & (PadButtons::Right | PadButtons::Up)) ? 1 : -1));
            toast("VOLUME", label);
        } else {
            const int current = playback->playbackSpeedPercent();
            const int target = tube_extra_speed_step(current, (keys & PadButtons::Up) ? 1 : -1);
            char label[48]; std::snprintf(label, sizeof label, "%.3gx", target / 100.0);
            if (target == current) toast("SPEED", label);
            else if (playback->canChangePlaybackSpeed() && playback->changePlaybackSpeed(target)) toast("SPEED", label);
            else toast("SPEED", "Unable to change speed; try again when playback is ready");
        }
        return true;
    }
    // End ProsperoTube D-pad actions.
    /* Live TV has no timeline: seek and chapter keys are swallowed, not sent''')
    replace(screen, '''    if (!playback) return;

    const auto chatState = playback->getPlaybackState();''', '''    if (!playback) return;

    // ProsperoTube player gain and audio delay.
    {
        const int delayMs = tube_extra_effective_audio_delay_ms();
        evo_audio_set_player_gain(tube_extra_gain_milli());
        evo_audio_set_delay_ms(delayMs);
        evo_pb_set_video_hold_ms(delayMs < 0 ? -delayMs : 0);
    }
    // End ProsperoTube player gain and audio delay.
    const auto chatState = playback->getPlaybackState();''')
    replace(screen, '''        p.fps_badge   = fpsBuf[0]   ? fpsBuf   : "";''', '''        p.fps_badge   = fpsBuf[0]   ? fpsBuf   : "";
        // ProsperoTube quality label facts.
        {
            int infoHeight = 0;
            long long infoBitrate = 0;
            const char* infoCodec = "";
            if (play_fmt && video_stream_index >= 0 &&
                video_stream_index < static_cast<int>(play_fmt->nb_streams)) {
                if (AVCodecParameters* cp = play_fmt->streams[video_stream_index]->codecpar) {
                    infoHeight = cp->height;
                    infoBitrate = static_cast<long long>(cp->bit_rate);
                    switch (cp->codec_id) {
                        case AV_CODEC_ID_H264: infoCodec = "AVC";  break;
                        case AV_CODEC_ID_HEVC: infoCodec = "HEVC"; break;
                        case AV_CODEC_ID_VP9:  infoCodec = "VP9";  break;
                        case AV_CODEC_ID_AV1:  infoCodec = "AV1";  break;
                        default: infoCodec = avcodec_get_name(cp->codec_id); break;
                    }
                }
            }
            if (infoBitrate <= 0) {
                const int activeVideo = playback->getActiveVideoStream();
                for (const auto& v : playback->getVideoVariants())
                    if (v.streamIndex == activeVideo) infoBitrate = static_cast<long long>(v.bitrate);
            }
            tube_extra_set_stream_info(infoHeight, fps, infoCodec, infoBitrate);
        }
        // End ProsperoTube quality label facts.''')


def _kit_hud(replace, player):
    hud = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(hud, '#include "tube_time_display.h"\n#include "tube_video_geometry.h"',
            '#include "tube_time_display.h"\n#include "tube_video_geometry.h"\n#include "tube_player_extra.h"')
    replace(hud, 'tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode(),',
            'tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode()|tube_extra_time_flags(),')
    replace(hud, '''        // End ProsperoTube time display HUI.''', '''        // End ProsperoTube time display HUI.
        // ProsperoTube chapter title HUI.
        const char *tube_chapter = tube_extra_chapter_title_at(raw_.scrub_active ? raw_.scrub_target : raw_.position_sec);
        if (*tube_chapter)
            ui::text(list, fonts.regular, fit(fonts.regular, tube_chapter, 20, 1100), track.x, 896, 20,
                     kWhite.with_alpha(0.85f));
        // End ProsperoTube chapter title HUI.''')
    replace(hud, '''    /* Controls, one row of hints. */''', '''    // ProsperoTube controls info HUI.
    char tube_info[96];
    if (tube_extra_controls_info(tube_info, sizeof tube_info, time(NULL)))
        ui::text(list, fonts.mono, tube_info, track.x + track.w, 896, 20, kWhite.with_alpha(0.75f), Align::right);
    // End ProsperoTube controls info HUI.

    /* Controls, one row of hints. */''')
    replace(hud, '''    const float size = caption_size(raw_.subtitle_face);''', '''    // ProsperoTube caption style HUI.
    const tube_subtitle_style_t tube_style = tube_extra_subtitle_style();
    const int tube_position = tube_extra_subtitle_position();
    const float size = caption_size(raw_.subtitle_face) * static_cast<float>(tube_extra_subtitle_scale()) / 100.0f;''')
    replace(hud, '''    const float bottom = 1000.0f - (raw_.subtitle_raised ? 150.0f * chrome : 0.0f);''',
            '''    const float bottom = tube_position >= 0
        ? static_cast<float>(tube_subtitle_baseline(tube_position, 1080.0,
            static_cast<double>(sub_lines_.empty() ? 1 : sub_lines_.size()) * line_h, line_h))
        : 1000.0f - (raw_.subtitle_raised ? 150.0f * chrome : 0.0f);
    // End ProsperoTube caption style HUI.''')
    replace(hud, '''            list.rounded_rect({960 - w * 0.5f - 18, y - sz * 0.95f, w + 36, sz * 1.25f}, 10,
                              kBlack.with_alpha(0.55f));
            ui::text(list, fonts.semibold, l, 960, y, sz, ink, Align::center);''', '''            // ProsperoTube caption plate HUI.
            const Rect plate{960 - w * 0.5f - 18, y - sz * 0.95f, w + 36, sz * 1.25f};
            if (!tube_style.custom) list.rounded_rect(plate, 10, kBlack.with_alpha(0.55f));
            else {
                if (tube_style.background_alpha > 0)
                    list.rounded_rect(plate, 10, Color::rgb(tube_style.background_rgb,
                                                           static_cast<float>(tube_style.background_alpha) / 255.0f));
                const float edge = std::max(2.0f, sz * 0.05f);
                if (tube_style.edge == TUBE_SUBTITLE_EDGE_SHADOW)
                    ui::text(list, fonts.semibold, l, 960 + edge, y + edge, sz, kBlack, Align::center);
                else if (tube_style.edge == TUBE_SUBTITLE_EDGE_OUTLINE)
                    for (int dx = -1; dx <= 1; ++dx)
                        for (int dy = -1; dy <= 1; ++dy)
                            if (dx || dy)
                                ui::text(list, fonts.semibold, l, 960 + static_cast<float>(dx) * edge,
                                         y + static_cast<float>(dy) * edge, sz, kBlack, Align::center);
            }
            // End ProsperoTube caption plate HUI.
            ui::text(list, fonts.semibold, l, 960, y, sz, ink, Align::center);''')
    replace(hud, '        top = block(sub_lines_, bottom, size, kWhite);',
            '        top = block(sub_lines_, bottom, size, tube_style.custom ? Color::rgb(tube_style.text_rgb) : kWhite);')
    replace(hud, '''        ui::text(list, fonts.mono, fps, chip.cx(), chip.y + 28, 20, ctx.palette.accent, Align::center);
    }
}''', '''        ui::text(list, fonts.mono, fps, chip.cx(), chip.y + 28, 20, ctx.palette.accent, Align::center);
    }
    // ProsperoTube screen dimming HUI.
    if (const int tube_dim = tube_extra_dim_percent())
        list.rounded_rect(Rect{0, 0, 1920, 1080}, 0, kBlack.with_alpha(static_cast<float>(tube_dim) / 100.0f));
    // End ProsperoTube screen dimming HUI.
}''')


def _rml_hud(replace, player):
    rml = player + 'ui_rml/src/evo_rmlui_app.cpp'
    replace(rml, '#include "tube_time_display.h"\n#include "tube_video_geometry.h"',
            '#include "tube_time_display.h"\n#include "tube_video_geometry.h"\n#include "tube_player_extra.h"')
    replace(rml, 'tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode(),',
            'tube_time_display(tube_time_label,sizeof tube_time_label,tube_player_time_mode()|tube_extra_time_flags(),')
    replace(rml, '    // ProsperoTube video geometry RML.', '''    // ProsperoTube player extras RML.
    {
        auto tube_escape = [](const char* text) {
            std::string rml;
            for (const char* c = text; *c; ++c) {
                if (*c == '&') rml += "&amp;";
                else if (*c == '<') rml += "&lt;";
                else if (*c == '>') rml += "&gt;";
                else rml += *c;
            }
            return rml;
        };
        char tube_info[96];
        tube_extra_controls_info(tube_info, sizeof tube_info, time(NULL));
        const std::string info_rml = tube_escape(tube_info);
        const std::string chapter_rml = tube_escape(state.is_live ? "" :
            tube_extra_chapter_title_at(state.scrub_active ? state.scrub_target : state.position_sec));
        if (auto* element = m_playback_doc->GetElementById("tube-info")) {
            if (element->GetInnerRML() != info_rml) { element->SetInnerRML(info_rml); m_frame_dirty = true; }
        }
        if (auto* element = m_playback_doc->GetElementById("tube-chapter")) {
            if (element->GetInnerRML() != chapter_rml) { element->SetInnerRML(chapter_rml); m_frame_dirty = true; }
        }
        if (auto* row = m_playback_doc->GetElementById("tube-info-row")) {
            const bool shown = !info_rml.empty() || !chapter_rml.empty();
            if (row->IsClassSet("shown") != shown) { row->SetClass("shown", shown); m_frame_dirty = true; }
        }
        // Screen dimming: black over the picture and everything drawn on it.
        if (auto* dim = m_playback_doc->GetElementById("tube-dim")) {
            static int dim_shown = -1;
            static const void* dim_document = nullptr;
            const int percent = tube_extra_dim_percent();
            if (percent != dim_shown || dim_document != m_playback_doc) {
                char value[16];
                std::snprintf(value, sizeof value, "#000000%02x", percent * 255 / 100);
                dim->SetProperty("background-color", value);
                dim->SetClass("shown", percent > 0);
                dim_shown = percent; dim_document = m_playback_doc; m_frame_dirty = true;
            }
        }
        // Caption look: reapplied when a preference, the face or the document changes.
        static unsigned applied_revision = 0;
        static int applied_face = -1;
        static const void* applied_document = nullptr;
        if (applied_revision != tube_extra_revision() || applied_face != state.subtitle_face ||
            applied_document != m_playback_doc) {
            applied_revision = tube_extra_revision();
            applied_face = state.subtitle_face;
            applied_document = m_playback_doc;
            const tube_subtitle_style_t style = tube_extra_subtitle_style();
            const int scale = tube_extra_subtitle_scale();
            const int position = tube_extra_subtitle_position();
            const int face = state.subtitle_face == 1 ? 0 : state.subtitle_face == 3 ? 2 : 1;
            static const char* const boxes[] = {"subtitle-box", "subtitle-box-secondary", "subtitle-box-top"};
            static const int base_size[2][3] = {{31, 40, 52}, {26, 32, 42}};
            for (int i = 0; i < 3; ++i) {
                Rml::Element* box = m_playback_doc->GetElementById(boxes[i]);
                if (!box) continue;
                char value[32];
                if (style.custom) {
                    std::snprintf(value, sizeof value, "#%06x%02x", style.background_rgb & 0xffffffu,
                                  static_cast<unsigned>(style.background_alpha) & 0xffu);
                    box->SetProperty("background-color", value);
                } else box->RemoveProperty("background-color");
                // The secondary line keeps its own colour so the two never read as one.
                if (i == 0 && style.custom) {
                    std::snprintf(value, sizeof value, "#%06x", style.text_rgb & 0xffffffu);
                    box->SetProperty("color", value);
                } else if (i == 0) box->RemoveProperty("color");
                box->SetClass("tube-sub-outline", style.custom && style.edge == TUBE_SUBTITLE_EDGE_OUTLINE);
                box->SetClass("tube-sub-shadow", style.custom && style.edge == TUBE_SUBTITLE_EDGE_SHADOW);
                if (scale != 100) {
                    std::snprintf(value, sizeof value, "%ddp", std::max(4, base_size[i ? 1 : 0][face] * scale / 100));
                    box->SetProperty("font-size", value);
                } else box->RemoveProperty("font-size");
            }
            if (Rml::Element* layer = m_playback_doc->GetElementById("subtitle-layer")) {
                if (position >= 0) {
                    char value[32];
                    std::snprintf(value, sizeof value, "%ddp", std::min(900, std::max(8, 1080 * position / 1000)));
                    layer->SetProperty("bottom", value);
                } else layer->RemoveProperty("bottom");
            }
            m_frame_dirty = true;
        }
    }
    // End ProsperoTube player extras RML.
    // ProsperoTube video geometry RML.''')
    replace(player + 'assets/rml/playback.rml', '        <!-- Progress Bar Timeline -->',
            '''        <!-- ProsperoTube: chapter title (left) and quality / clock (right) above the timeline -->
        <div id="tube-info-row"><div id="tube-chapter"></div><div id="tube-info"></div></div>
        <!-- Progress Bar Timeline -->''')
    replace(player + 'assets/rml/playback.rml', '</body>', '''    <!-- ProsperoTube: screen dimming and screen off, above everything else -->
    <div id="tube-dim"></div>
</body>''')
    replace(player + 'assets/rml/playback.rcss', '#time-row {', '''/* ProsperoTube: black at the chosen strength over the picture and the controls. */
#tube-dim {
    display: none;
    position: absolute;
    left: 0;
    top: 0;
    width: 1920dp;
    height: 1080dp;
    z-index: 100;
    background-color: #000000;
}

#tube-dim.shown {
    display: block;
}

/* ProsperoTube: chapter title and quality / clock labels. The row sits in the
 * bar's top padding, so the rows below keep their place. */
#tube-info-row {
    display: none;
    position: absolute;
    left: 60dp;
    top: 3dp;
    width: 1800dp;
    flex-direction: row;
    justify-content: space-between;
    align-items: center;
}

#tube-info-row.shown {
    display: flex;
}

#tube-chapter {
    max-width: 1200dp;
    font-size: 15dp;
    font-weight: bold;
    color: #ffffff;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
}

#tube-info {
    font-size: 15dp;
    font-weight: bold;
    color: #a2b0c2;
}

/* ProsperoTube caption edges (SmartTube outline and drop-shadow presets). */
.tube-sub-outline {
    font-effect: outline(2dp #000000);
}

.tube-sub-shadow {
    font-effect: shadow(2dp 2dp #000000);
}

#time-row {''')


def _picker(replace, player):
    picker = player + 'core/src/screens/AudioTrackPickerScreen.cpp'
    header = player + 'core/include/evo/screens/AudioTrackPickerScreen.hpp'
    replace(header, 'KindPlaybackMode, KindVideoNavigation };',
            'KindPlaybackMode, KindVideoNavigation, KindExtraGroup, KindExtraChoice, KindChapterList, KindChapter };')
    replace(header, '    int m_scrollOffset = 0;\n};',
            '    int m_scrollOffset = 0;\n    int m_tubeExtraGroup = -1;      /* -1 = all options, -2 = chapters, else one preference group */\n};')
    replace(picker, '#include <cctype>', '#include "tube_player_extra.h"\n#include <cctype>')
    replace(picker, '    StatefulScreen::onEnter();\n    refreshTracks();',
            '    StatefulScreen::onEnter();\n    m_tubeExtraGroup = -1;\n    refreshTracks();')
    replace(picker, '''    if (!playback) return;

    /*
     * Quality first:''', '''    if (!playback) return;

    // ProsperoTube extra option lists (extracted by host tests).
    if (m_tubeExtraGroup == -2) {
        const int current = tube_extra_chapter_at(playback->getPositionSeconds());
        for (int i = 0; i < tube_extra_chapter_count(); ++i) {
            Entry e; e.kind = KindChapter; e.streamIndex = i;
            char label[512]; tube_extra_chapter_label(i, label, sizeof label);
            e.label = label; e.detail = "Jump to this chapter";
            if (i == current) { e.badge = "PLAYING"; m_activeIndex = i; }
            m_tracks.push_back(std::move(e));
        }
        return;
    }
    if (m_tubeExtraGroup >= 0) {
        const int selected = tube_extra_group_selected(m_tubeExtraGroup);
        for (int i = 0; i < tube_extra_group_count(m_tubeExtraGroup); ++i) {
            Entry e; e.kind = KindExtraChoice; e.streamIndex = i;
            char label[96]; tube_extra_group_label(m_tubeExtraGroup, i, label, sizeof label);
            e.label = label; e.detail = tube_extra_group_detail(m_tubeExtraGroup);
            if (i == selected) { e.badge = "SELECTED"; m_activeIndex = i; }
            m_tracks.push_back(std::move(e));
        }
        return;
    }
    // End ProsperoTube extra option lists.

    /*
     * Quality first:''')
    replace(picker, '    // End ProsperoTube time display rows.', '''    // End ProsperoTube time display rows.
    // ProsperoTube extra option rows (extracted by host tests).
    if (tube_extra_chapter_count() > 0 && playback->isActive()) {
        Entry e; e.kind = KindChapterList; e.label = "Chapters";
        const char* title = tube_extra_chapter_title_at(playback->getPositionSeconds());
        e.detail = *title ? title : "Choose a chapter to jump to";
        e.badge = std::to_string(tube_extra_chapter_count());
        m_tracks.push_back(std::move(e));
    }
    for (int group = 0; group < TUBE_EXTRA_GROUPS; ++group) {
        if (!tube_extra_group_in_player(group)) continue;
        Entry e; e.kind = KindExtraGroup; e.streamIndex = group;
        char value[96];
        tube_extra_group_label(group, tube_extra_group_selected(group), value, sizeof value);
        e.label = tube_extra_group_title(group);
        if (value[0]) e.label += std::string(": ") + value;
        e.detail = tube_extra_group_detail(group);
        m_tracks.push_back(std::move(e));
    }
    // End ProsperoTube extra option rows.''')
    replace(picker, '    // ProsperoTube video geometry selection.', '''    // ProsperoTube extra option selection (extracted by host tests).
    if (e.kind == KindExtraGroup || e.kind == KindChapterList) {
        // Open the choices of this group in place; Circle returns to all options.
        m_tubeExtraGroup = e.kind == KindChapterList ? -2 : e.streamIndex;
        refreshTracks();
        m_selectedIndex = m_activeIndex;
        m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
        return;
    }
    if (e.kind == KindExtraChoice) {
        const int group = m_tubeExtraGroup;
        std::string message = std::string(tube_extra_group_title(group)) + ": " + e.label;
        const bool saved = tube_extra_group_select(group, e.streamIndex) != 0;
        if (saved && group == TUBE_EXTRA_VOLUME && tube_extra_volume() > 100)
            message += " - boosted sound may distort";
        toast("PLAYER", saved ? message.c_str() : "Unable to save this preference");
    } else if (e.kind == KindChapter) {
        const double start = tube_extra_chapter_start(e.streamIndex);
        if (start >= 0 && !playback->isLiveSource() && playback->seekTo(start, playback->isPaused()))
            toast("CHAPTER", e.label.c_str());
        else toast("CHAPTER", "Unable to jump to this chapter");
    } else
    // End ProsperoTube extra option selection.
    // ProsperoTube video geometry selection.''')
    replace(picker, '''    if (pressed & PadButtons::Circle) {
        evo_feedback(EVO_FB_CANCEL);''', '''    if (pressed & PadButtons::Circle) {
        evo_feedback(EVO_FB_CANCEL);
        // ProsperoTube extra option back (extracted by host tests).
        if (m_tubeExtraGroup != -1) {
            const int group = m_tubeExtraGroup;
            m_tubeExtraGroup = -1;
            refreshTracks();
            m_selectedIndex = m_activeIndex;
            for (int i = 0; i < static_cast<int>(m_tracks.size()); ++i)
                if (group == -2 ? m_tracks[i].kind == KindChapterList
                                : (m_tracks[i].kind == KindExtraGroup && m_tracks[i].streamIndex == group)) m_selectedIndex = i;
            m_scrollOffset = m_selectedIndex >= EVO_RMLUI_LIST_ROWS ? m_selectedIndex - EVO_RMLUI_LIST_ROWS + 1 : 0;
            return true;
        }
        // End ProsperoTube extra option back.''')
    replace(picker, '    params.title = "PLAYBACK OPTIONS";',
            '    params.title = m_tubeExtraGroup == -2 ? "CHAPTERS" : m_tubeExtraGroup >= 0 ? tube_extra_group_title(m_tubeExtraGroup) : "PLAYBACK OPTIONS";')


def _speed_lists(replace, player):
    controller = player + 'core/src/services/PlaybackController.cpp'
    replace(controller, '#include "tube_subtitles.h"', '#include "tube_subtitles.h"\n#include "tube_player_extra.h"')
    replace(controller, '    for (int i = 0; i < tube_speed_count(); ++i) if (tube_speed_at(i) == percent) valid = true;',
            '    for (int i = 0; i < tube_extra_speed_count(); ++i) if (tube_extra_speed_at(i) == percent) valid = true;')
    replace(player + 'core/src/screens/AudioTrackPickerScreen.cpp',
            '''        for (int i = 0; i < tube_speed_count(); ++i) {
            Entry e; e.kind = KindSpeed; e.streamIndex = tube_speed_at(i);''',
            '''        for (int i = 0; i < tube_extra_speed_count(); ++i) {
            Entry e; e.kind = KindSpeed; e.streamIndex = tube_extra_speed_at(i);''')


def apply_player_extra_overlay(replace, player):
    replace(player + 'Makefile', '    addons/src/tube_audio_tempo.c',
            '    addons/src/tube_audio_tempo.c \\\n    addons/src/tube_player_extra.c')
    _audio(replace, player)
    _player_screen(replace, player)
    _kit_hud(replace, player)
    _rml_hud(replace, player)
    _picker(replace, player)
    _speed_lists(replace, player)
    # The in-player actions extend the blocks above.
    from player_actions_overlay import apply_player_actions_overlay
    apply_player_actions_overlay(replace, player)
