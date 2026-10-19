/* UI-thread controls for the paired LAN remote. GPL-3.0-or-later. */
#ifndef MAXD_PHONE_REMOTE_PLAYBACK_HPP
#define MAXD_PHONE_REMOTE_PLAYBACK_HPP
#include "maxd/phone_remote.h"
#include "maxd/diagnostics.h"
#include "tube_provider.h"
#include "tube_phone.h"
#include "tube_afterglow.h"
#include "tube_background_play.h"
#include "tube_voice.h"
#include "evo_keyboard.h"
#include "evo_data_path.h"
#include "evo_demux.h"
extern "C" {
#include <libavformat/avformat.h>
extern AVFormatContext *play_fmt;
extern volatile int pb_prebuffer_hold;
}
#include <algorithm>
#include <cmath>
inline std::string maxd_phone_search;
inline bool maxd_phone_busy(evo::Application &app) {
    auto sm=app.getScreenManager();
    return !sm || evo::ProviderHostScreen::playbackStartupBusy() ||
        evo_keyboard_is_open() || tube_voice_state()!=TUBE_VOICE_IDLE || tube_phone_owns_media() ||
        (sm->getCurrentScreenId()!=evo::ScreenId::EmbyBrowse && sm->getCurrentScreenId()!=evo::ScreenId::Player);
}
inline void maxd_phone_snapshot(void *context,MaxdRemoteState *s) {
    auto &app=*static_cast<evo::Application*>(context);auto pb=app.getPlaybackController();
    char video[12];
    s->busy=maxd_phone_busy(app);
    s->active=!evo::ProviderHostScreen::playbackStartupBusy() && pb && pb->isActive() && tube_phone_video(pb->getCurrentFilePath().c_str(),video);
    s->selection=s->active?tube_caption_selection():0;
    if(s->active){s->paused=pb->isPaused();s->live=pb->isLiveSource();s->position=pb->getPositionSeconds();s->duration=pb->getDurationSeconds();}
    tube_maxd_remote_queue_state(s);
}
inline int maxd_phone_apply(void *context,const MaxdRemoteCommand *c) {
    auto &app=*static_cast<evo::Application*>(context);auto pb=app.getPlaybackController();auto sm=app.getScreenManager();
    if(!tube_maxd_remote_scope() || maxd_phone_busy(app))return 0;
    if(c->kind>=MAXD_REMOTE_ADD)return tube_maxd_remote_queue(c->kind,c->text);
    if(c->kind==MAXD_REMOTE_SEARCH) {
        tube_next_offer_t nextOffer;
        if(tube_next_offer(&nextOffer) || tube_manual_navigation_pending() || tube_playlist_eof_pending() ||
           (pb && pb->isActive() && tube_sponsor_prompt(pb->getPositionSeconds())))return 0;
        if(sm->getCurrentScreenId()==evo::ScreenId::Player) {
            /* Use the ordinary browser return, with PiP still off by default. */
            if(tube_afterglow_enabled() && pb && pb->isActive() && !pb->isScrubbing() &&
               tube_background_play_begin(tube_caption_selection(),tube_browse_profile())) {
                tube_background_play_status(pb->getDisplayTitle().c_str(),pb->isPaused());
            } else if(pb)pb->stopPlayback();
            maxd_phone_search=c->text;sm->returnFromPlayback();return 1;
        }
        auto host=static_cast<evo::ProviderHostScreen*>(sm->getScreen(evo::ScreenId::EmbyBrowse));
        return host && host->submitPhoneSearch(c->text);
    }
    if(c->kind==MAXD_REMOTE_KEY) {
        uint32_t key=!std::strcmp(c->text,"UP")?evo::PadButtons::Up:!std::strcmp(c->text,"DOWN")?evo::PadButtons::Down:
            !std::strcmp(c->text,"LEFT")?evo::PadButtons::Left:!std::strcmp(c->text,"RIGHT")?evo::PadButtons::Right:
            !std::strcmp(c->text,"ENTER")?evo::PadButtons::Cross:evo::PadButtons::Circle;
        sm->handleInput(key,key,0);sm->handleInput(0,0,key);return 1;
    }
    MaxdRemoteState current{};maxd_phone_snapshot(context,&current);
    if(!current.active || c->selection!=current.selection || pb->isScrubbing())return 0;
    if(c->kind==MAXD_REMOTE_PAUSE || c->kind==MAXD_REMOTE_RESUME) {
        pb->setPaused(c->kind==MAXD_REMOTE_PAUSE);return pb->isPaused()==(c->kind==MAXD_REMOTE_PAUSE);
    }
    if(c->kind==MAXD_REMOTE_SEEK && !current.live && pb->isSeekable()) {
        double duration=pb->getDurationSeconds();if(!std::isfinite(duration) || duration<=0)return 0;
        return pb->seekTo(std::max(0.0,std::min(duration-0.05,c->seconds)),pb->isPaused());
    }
    return 0;
}
inline int maxd_phone_capture(void *) {return maxd_report_capture(evo_data_path("bug-report.json"));}
inline const char *maxd_phone_report(void *) {return maxd_report_json();}
inline void maxd_phone_frame(evo::Application &app,int64_t now) {
    auto pb=app.getPlaybackController();auto sm=app.getScreenManager();
    const bool busy=evo::ProviderHostScreen::playbackStartupBusy();
    /* Samples contain only typed counters and enums. No stream strings enter a report. */
    MaxdDiagnostics d{};char video[12];
    d.active=!busy && pb && pb->isActive() && tube_phone_video(pb->getCurrentFilePath().c_str(),video);
    d.afterglow=tube_afterglow_enabled();d.pip=tube_background_play_picture_active();
    d.auto_max=tube_maxd_auto_enabled();tube_player_quality(&d.height_limit,&d.fps_limit,&d.codec_preference);
    if(d.active) {
        d.paused=pb->isPaused();d.live=pb->isLiveSource();d.position=pb->getPositionSeconds();d.duration=pb->getDurationSeconds();
        d.captions=prospero_subtitle_enabled;d.hardware=evo_pb_active_backend()==EVO_VDEC_BACKEND_NATIVE;d.fps=evo_pb_video_fps();
        d.buffering=pb_prebuffer_hold || g_pp_pb.seek_discarding || evo_demux_input_failed();
        d.frames=__atomic_load_n(&g_pp_pb.stats.frames_in,__ATOMIC_RELAXED);
        d.dropped=__atomic_load_n(&g_pp_pb.stats.frames_late_dropped,__ATOMIC_RELAXED);
        if(play_fmt && video_stream_index>=0 && video_stream_index<static_cast<int>(play_fmt->nb_streams)) {
            auto stream=play_fmt->streams[video_stream_index];
            if(stream && stream->codecpar) {
            d.width=stream->codecpar->width;d.height=stream->codecpar->height;
            switch(stream->codecpar->codec_id) {
            case AV_CODEC_ID_H264:d.codec=1;break;case AV_CODEC_ID_VP9:d.codec=2;break;
            case AV_CODEC_ID_AV1:d.codec=3;break;case AV_CODEC_ID_HEVC:d.codec=4;break;default:break;
            }
            }
        }
    }
    maxd_report_sample(&d);
    if(!tube_maxd_remote_scope()){maxd_phone_search.clear();return;}
    if(!maxd_phone_search.empty() && !maxd_phone_busy(app) && sm && sm->getCurrentScreenId()==evo::ScreenId::EmbyBrowse) {
        auto host=static_cast<evo::ProviderHostScreen*>(sm->getScreen(evo::ScreenId::EmbyBrowse));
        if(!host || !host->submitPhoneSearch(maxd_phone_search.c_str()))toast("PHONE REMOTE","Return to browsing on the TV, then search again");
        maxd_phone_search.clear();
    }
    MaxdRemoteControls controls{&app,maxd_phone_snapshot,maxd_phone_apply,maxd_phone_capture,maxd_phone_report};
    maxd_remote_tick(now,&controls);
}
#endif
