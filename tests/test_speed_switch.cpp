/* Executes the generated controller's speed reopen with an observed open boundary. */
#include <cassert>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstdint>

static bool complete, seeking, pb_prebuffer_hold;
static struct { bool seek_discarding; } g_pp_pb;
static int saved_rate=100, engine_rate=100, audio_stream_index=3;
static int prospero_subtitle_enabled=1, prospero_subtitle_use_external=0;
static int prospero_embedded_subtitle_stream_index=4, prospero_subtitle_requested_stream=-2;
static int prospero_subtitle_delay_ms=450, prospero_secondary_delay_ms=-300;
static double prospero_subtitle_time_scale=1.25;
static int evo_pb_is_complete() { return complete; }
static int evo_demux_is_seeking() { return seeking; }
static double evo_audio_rate() { return engine_rate/100.0; }
static int tube_speed_count() { return 3; }
static int tube_speed_at(int i) { const int rates[]={25,100,125}; return rates[i]; }
static int tube_remember_playback_speed(const char *id, int percent) { assert(std::string(id)=="abcdefghijk"); saved_rate=percent; return 1; }
static int provider_stops;
enum { EVO_PROVIDER_CAP_PROGRESS=1, EVO_PROVIDER_PLAY_STOP=3 };
static void stopped(const char* id,int64_t pos,int64_t dur,int reason) {
    assert(std::string(id)=="abcdefghijk" && pos==83 && dur==200 && reason==EVO_PROVIDER_PLAY_STOP);
    ++provider_stops;
}
struct evo_provider_t { int caps=1; decltype(&stopped) report_progress=stopped; } provider;
static const evo_provider_t* evo_provider_find(const char*) { return &provider; }
struct PlaybackSource {
    std::string provider="youtube",url="signed-url",title="Video",item_id="abcdefghijk";
    bool is_live=false;
    bool isProvider() const { return provider=="youtube"; }
};
struct PlaybackController {
    PlaybackSource m_source;
    std::string m_currentFilePath="signed-url";
    int m_requestedAudioStream=-1,m_tubeRequestedSpeed=-1;
    bool scrubbing=false,paused=false,open_ok=true;
    bool music=false;
    double position=83.125;
    double m_durationSeconds=200;
    int opens=0,pauses=0,observed_audio=-1,observed_sub=-1,observed_rate=-1;
    double observed_position=-1;
    PlaybackSource observed_source;
    bool isScrubbing() const { return scrubbing; }
    bool isPaused() const { return paused; }
    bool isMusicMode() const { return music; }
    double getPositionSeconds() const { return position; }
    double getPlaybackPositionSeconds() const { return position; }
    void setPaused(bool value) { paused=value; ++pauses; }
    void reportStop() {
#include "stop-progress.inc"
    }
    bool startPlaybackSource(const PlaybackSource &source,double pos) {
        reportStop();
        ++opens;observed_source=source;observed_position=pos;
        observed_audio=m_requestedAudioStream;observed_sub=prospero_subtitle_requested_stream;
        observed_rate=m_tubeRequestedSpeed;
        /* The real open destroys source state; this catches aliasing and captures
         * exactly what reaches the stopped-worker boundary. */
        m_source={};m_currentFilePath.clear();paused=false;
        prospero_subtitle_enabled=0;prospero_subtitle_use_external=0;
        prospero_embedded_subtitle_stream_index=-1;
        prospero_subtitle_delay_ms=prospero_secondary_delay_ms=0;prospero_subtitle_time_scale=1;
        if(open_ok) { m_source=source;m_currentFilePath=source.url;engine_rate=m_tubeRequestedSpeed; }
        return open_ok;
    }
    bool canChangePlaybackSpeed() const;
    bool canShowLiveChat() const;
    int playbackSpeedPercent() const;
    bool changePlaybackSpeed(int);
};
#include "speed-switch.inc"

static void reset() {
    complete=seeking=pb_prebuffer_hold=g_pp_pb.seek_discarding=false;
    saved_rate=engine_rate=100;audio_stream_index=3;
    provider_stops=0;
    prospero_subtitle_enabled=1;prospero_subtitle_use_external=0;
    prospero_embedded_subtitle_stream_index=4;prospero_subtitle_requested_stream=-2;
    prospero_subtitle_delay_ms=450;prospero_secondary_delay_ms=-300;prospero_subtitle_time_scale=1.25;
}
int main() {
    PlaybackController chat;
    assert(chat.canShowLiveChat());
    chat.m_source.is_live=true;assert(chat.canShowLiveChat());
    chat.music=true;assert(!chat.canShowLiveChat());chat.music=false;
    chat.m_source.provider="file";assert(!chat.canShowLiveChat());
    chat.m_source.provider="youtube";chat.m_source.url.clear();assert(!chat.canShowLiveChat());
    for(bool paused:{false,true}) for(bool external:{false,true}) {
        reset();PlaybackController c;c.paused=paused;prospero_subtitle_use_external=external;
        assert(c.changePlaybackSpeed(125));
        assert(c.opens==1 && c.observed_position==83.125 && c.observed_rate==125);
        assert(c.observed_source.url=="signed-url" && c.observed_source.title=="Video" && c.observed_source.item_id=="abcdefghijk");
        assert(c.observed_audio==3 && c.observed_sub==(external?-2:4));
        assert(c.paused==paused && c.pauses==(paused?1:0));
        assert(prospero_subtitle_enabled==1 && prospero_subtitle_use_external==external);
        assert(prospero_subtitle_delay_ms==450 && prospero_secondary_delay_ms==-300 && prospero_subtitle_time_scale==1.25);
        if(!external) assert(prospero_embedded_subtitle_stream_index==4);
        assert(saved_rate==125 && c.playbackSpeedPercent()==125);
        assert(provider_stops==0);
        assert(c.m_tubeRequestedSpeed==-1 && c.m_requestedAudioStream==-1 && prospero_subtitle_requested_stream==-2);
        assert(c.changePlaybackSpeed(125) && c.opens==1); // No-op avoids reopening.
        c.reportStop();assert(provider_stops==1); // A later user Stop still closes the session.
    }
    reset();PlaybackController failed;failed.open_ok=false;
    assert(!failed.changePlaybackSpeed(25) && failed.opens==1 && saved_rate==100);
    assert(provider_stops==1);
    assert(failed.m_tubeRequestedSpeed==-1 && failed.m_requestedAudioStream==-1 && prospero_subtitle_requested_stream==-2);
    for(int gate=0;gate<12;++gate) {
        reset();PlaybackController c;
        switch(gate) {
            case 0:complete=true;break;case 1:seeking=true;break;
            case 2:pb_prebuffer_hold=true;break;case 3:g_pp_pb.seek_discarding=true;break;
            case 4:c.scrubbing=true;break;case 5:c.m_source.is_live=true;break;
            case 6:c.m_source.provider="file";break;case 7:c.m_source.url.clear();break;
            case 8:c.m_currentFilePath.clear();break;case 9:c.position=NAN;break;
            case 10:c.position=200;break;case 11:c.m_durationSeconds=0;break;
        }
        assert(!c.changePlaybackSpeed(125) && c.opens==0 && saved_rate==100);
    }
    reset();PlaybackController c;
    assert(!c.changePlaybackSpeed(126) && !c.changePlaybackSpeed(0) && c.opens==0);
    puts("PASS: generated speed reopen preserves source/position/pause/tracks, clears intent and rejects competing work");
}
