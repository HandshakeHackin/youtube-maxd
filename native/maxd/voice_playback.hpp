/* Included by the app and provider host. GPL-3.0-or-later. */
#ifndef MAXD_VOICE_PLAYBACK_HPP
#define MAXD_VOICE_PLAYBACK_HPP
#include "maxd/voice_commands.h"
#include "tube_provider.h"
#include "tube_phone.h"
#include "evo_subtitle.h"
#include "evo_toast.h"
#include "evo/Application.hpp"
#include "evo/services/PlaybackController.hpp"
#include "evo/screens/ScreenManager.hpp"
#include <string>
#include <cstdlib>
#include <algorithm>
inline unsigned maxd_voice_selection=0;
inline bool maxd_voice_was_paused=true;
inline std::string maxd_voice_profile;
inline void maxd_voice_capture() {
    auto pb=evo::Application::getInstance().getPlaybackController();
    maxd_voice_selection=tube_caption_selection();maxd_voice_profile=tube_browse_profile();
    maxd_voice_was_paused=!pb || !pb->isActive() || pb->isPaused();
    if(pb && pb->isActive())pb->setPaused(true);
}
inline void maxd_voice_restore() {
    auto pb=evo::Application::getInstance().getPlaybackController();
    if(!maxd_voice_was_paused && pb && pb->isActive() && maxd_voice_selection==tube_caption_selection() && maxd_voice_profile==tube_browse_profile())pb->setPaused(false);
    maxd_voice_selection=0;
}
inline bool maxd_voice_apply(const char *text) {
    auto c=maxd_voice_command(text);if(!c.kind)return false;
    auto &app=evo::Application::getInstance();auto pb=app.getPlaybackController();auto sm=app.getScreenManager();
    if(!pb || !pb->isActive() || tube_phone_owns_media() || maxd_voice_selection!=tube_caption_selection() || maxd_voice_profile!=tube_browse_profile()) {
        toast("VOICE CONTROL","No current video to control");return true;
    }
    const char *message="Done";
    if(c.kind==MAXD_CMD_PAUSE){pb->setPaused(true);maxd_voice_was_paused=true;message="Paused";}
    if(c.kind==MAXD_CMD_RESUME){pb->setPaused(false);maxd_voice_was_paused=true;message="Playing";}
    if(c.kind==MAXD_CMD_SEEK) {
        if(pb->isLiveSource())message="Voice seeking is available for videos on demand";
        else pb->seekTo(std::max(0.0,std::min(pb->getDurationSeconds(),pb->getPositionSeconds()+c.value)));
    }
    if(c.kind==MAXD_CMD_QUEUE)message=tube_maxd_queue_current()?"Added to your local queue":"Could not add this video";
    if(c.kind==MAXD_CMD_CAPTIONS) {
        if(!c.value){prospero_subtitle_enabled=0;message="Captions off";}
        else if(prospero_subtitle_count){prospero_subtitle_enabled=1;message="Captions on";}
        else message="Choose a caption language from the subtitle menu first";
    }
    if(c.kind==MAXD_CMD_QUALITY) {
        int match=-1;
        for(int i=0;i<tube_player_quality_count();++i){char label[128];tube_player_quality_label(i,label,sizeof label);if(std::atoi(label)==c.value){match=i;break;}}
        if(match<0)message="That resolution is not available for this video";
        else {
            int result=tube_player_quality_select(match,pb->getPositionSeconds(),pb->isLiveSource(),prospero_subtitle_enabled);
            message=result?"Quality selected":"Quality could not be changed";
            if(result==1){pb->stopPlayback();if(sm)sm->returnFromPlayback();}
        }
    }
    maxd_voice_restore();toast("VOICE CONTROL",message);return true;
}
#endif
