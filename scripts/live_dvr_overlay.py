# Wire checked owner-bound live windows into EVO's existing seek path.
def apply_live_dvr_overlay(replace, player, host):
    interface=player+'core/include/evo/interfaces/IPlaybackController.hpp'
    header=player+'core/include/evo/services/PlaybackController.hpp'
    controller=player+'core/src/services/PlaybackController.cpp'
    replace(interface,'#include <vector>','#include <vector>\n#include "tube_provider.h"')
    replace(interface,'    bool is_live = false;', '    bool is_live = false;\n    tube_live_owner tube_live{};')
    replace(interface,'    virtual double getDurationSeconds() const = 0;', '''    virtual double getDurationSeconds() const = 0;
    virtual bool isSeekable() const { return !isLiveSource(); }
    virtual bool getSeekRange(double *start, double *end) const {
        if (isLiveSource() || getDurationSeconds()<=0) return false;
        if(start)*start=0;if(end)*end=getDurationSeconds();return true;
    }''')
    replace(header,'    double getDurationSeconds() const override { return m_durationSeconds; }', '''    double getDurationSeconds() const override;
    bool isSeekable() const override;
    bool getSeekRange(double *start, double *end) const override;''')
    replace(controller,'#include "tube_seek_timeline.h"','#include "tube_seek_timeline.h"\n#include "tube_live_runtime.h"')
    replace(host,'    g_start_src = src;', '''    if (src.provider=="youtube" && src.is_live)
        tube_live_capture(src.item_id.c_str(),src.url.c_str(),&src.tube_live);
    g_start_src = src;''')
    replace(controller,'    const int find_rc = avformat_find_stream_info(play_fmt, nullptr);', '''    if (source.provider=="youtube" && source.is_live)
        tube_live_context_open(play_fmt,&source.tube_live,filePath.c_str());
    const int find_rc = avformat_find_stream_info(play_fmt, nullptr);''')
    replace(controller,'    if (play_fmt->duration > 0) {', '''    double tubeLiveBase=0.0,tubeLiveRestore=-1.0;
    if (source.provider=="youtube" && source.is_live) {
        if (source.tube_live.token && !tube_live_owner_current(source.tube_live.token)) {
            stopPlayback(); return false;
        }
        tube_live_context_calibrate(play_fmt,&tubeLiveBase,&tubeLiveRestore);
        if (tubeLiveRestore>=0) resumeOffset=tubeLiveRestore;
    }
    if (play_fmt->duration > 0) {''')
    replace(controller,'        avformat_close_input(&play_fmt);', '        tube_live_context_release(play_fmt);\n        avformat_close_input(&play_fmt);',count=4)
    replace(controller,'    const bool wantResume = (resumeOffset > 0.0 && resumeOffset < m_durationSeconds &&', '''    double tubeLiveStart,tubeLiveEnd;
    if (source.is_live && tube_live_range(play_fmt,&tubeLiveStart,&tubeLiveEnd))
        m_durationSeconds=tubeLiveEnd;
    const bool wantResume = ((tubeLiveRestore>=0 || (!source.is_live && resumeOffset>0)) && resumeOffset < m_durationSeconds &&''')
    replace(controller,'        if (video_stream_index >= 0 && seekPos > 3.0)', '        if (!source.is_live && video_stream_index >= 0 && seekPos > 3.0)')
    replace(controller,'    m_resumeBaseOffset = wantResume ? resumeOffset : 0.0;', '    m_resumeBaseOffset = wantResume ? resumeOffset : tubeLiveBase;')
    replace(controller,'    requested_resume_seek_pos = 0.0;', '    requested_resume_seek_pos = 0.0;\n    if (source.is_live && tubeLiveBase>0) resume_base_anchor_pending=tubeLiveBase;')
    replace(controller,'            m_resumeBaseOffset = 0.0;\n            resume_base_offset_seconds = 0.0;', '            m_resumeBaseOffset = tubeLiveBase;\n            resume_base_offset_seconds = tubeLiveBase;')
    replace(controller,'    /* Let the provider know a session started, if it tracks them. */', '    if (source.tube_live.token && !tube_live_owner_current(source.tube_live.token)) { stopPlayback(); return false; }\n    /* Let the provider know a session started, if it tracks them. */')
    replace(controller,'double PlaybackController::getPercentage() const {', 'double PlaybackController::getPercentage() const {\n    if (isLiveSource()) { double start,end;return getSeekRange(&start,&end) && end>start ? std::clamp((getPositionSeconds()-start)/(end-start),0.0,1.0):0.0; }')
    replace(controller,'        if (converted && av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {', '        if (converted && tube_live_seek_prepare(play_fmt,seekPos) && av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {')
    replace(controller,'double PlaybackController::clampScrubTarget(double target) const {', '''bool PlaybackController::isSeekable() const {
    return !isLiveSource() || (isActive() && tube_live_active_range(nullptr,nullptr));
}
bool PlaybackController::getSeekRange(double *start,double *end) const {
    if (isLiveSource()) return isActive() && tube_live_active_range(start,end);
    if (m_durationSeconds<=0) return false;
    if(start)*start=0;if(end)*end=m_durationSeconds;return true;
}
double PlaybackController::getDurationSeconds() const {
    if (!isLiveSource()) return m_durationSeconds;
    double end;return getSeekRange(nullptr,&end)?end:0.0;
}
double PlaybackController::clampScrubTarget(double target) const {
    if (isLiveSource()) {
        double start,end;
        if (!getSeekRange(&start,&end)) return getPlaybackPositionSeconds();
        return std::clamp(target,start,std::max(start,end-.25));
    }''')
    replace(controller,'!isActive() || isLiveSource() || isScrubbing()', '!isActive() || !isSeekable() || isScrubbing()')
    replace(controller,'!std::isfinite(deltaSeconds) || !isActive() || isLiveSource() ||', '!std::isfinite(deltaSeconds) || !isActive() || !isSeekable() ||')
    replace(controller,'void PlaybackController::jumpChapter(int direction) {', '''void PlaybackController::jumpChapter(int direction) {
    if (isLiveSource()) {
        if (isSeekable()) seekTo(clampScrubTarget(getPlaybackPositionSeconds()+(direction>0?30.0:-30.0)),isPaused());
        return;
    }''')
    replace(controller,'    double pos = getPlaybackPositionSeconds();\n\n    /*', '''    double pos = getPlaybackPositionSeconds();
    if (m_source.provider=="youtube" && m_source.is_live)
        tube_live_remember_position(play_fmt,pos);

    /*''')
    demux=player+'media/src/evo_demux.c'
    replace(demux,'#include "tube_seek_timeline.h"','#include "tube_seek_timeline.h"\n#include "tube_live_runtime.h"')
    replace(demux,'    if (media_duration_sec > 1.0) {', '''    if (!tube_live_seek_admit(play_fmt,target_seconds)) return 0;
    if (media_duration_sec > 1.0 && !tube_live_context_matches(play_fmt)) {''')
    replace(demux,'    player_paused = 1;\n    controls_last_used_ms = now_ms();', '''    if (!tube_live_context_matches(play_fmt)) player_paused = 1;
    controls_last_used_ms = now_ms();''')
    replace(demux,'    if (!tube_seek_times(play_fmt, seek_stream, target_seconds,', '    if (!tube_live_seek_prepare(play_fmt,target_seconds) ||\n        !tube_seek_times(play_fmt, seek_stream, target_seconds,')
    replace(demux,'        prebuffer_check(prebuffer_deadline_ms, 0);', '        if (player_paused || pb_scrub_hold) tube_live_refresh_paused(play_fmt);\n        prebuffer_check(prebuffer_deadline_ms, 0);',count=2)
    replace(demux,'        int read_result =', '        if (player_paused || pb_scrub_hold) tube_live_refresh_paused(play_fmt);\n        int read_result =')
    replace(demux,'    /* An audio-only source has no video frame to release a presentation hold. */', '    player_paused = 1;\n    /* An audio-only source has no video frame to release a presentation hold. */')
    screen=player+'core/src/screens/PlayerScreen.cpp'
    replace(screen,'    if (playback->isLiveSource() &&\n        (pressed & (PadButtons::L1', '    if (!playback->isSeekable() &&\n        (pressed & (PadButtons::L1')
    replace(player+'Makefile','    addons/src/tube_provider.c', '    addons/src/tube_provider.c \\\n    addons/src/tube_live_runtime.c')
