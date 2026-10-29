/* Included by the native provider after its scoped reopen implementation. */
#include "playback_health.h"
static MaxdPlaybackHealth g_maxd_health;
static unsigned g_maxd_health_selection;
static struct {
    MaxdAutoMax samples;
    uint64_t failed_cost;
    int failed_height;
    unsigned selection;
} g_maxd_auto;
static void maxd_auto_video_reset(void) { memset(&g_maxd_auto,0,sizeof g_maxd_auto);maxd_auto_reset(&g_maxd_auto.samples); }
int tube_maxd_auto_enabled(void) { return g_auto_max; }
int tube_maxd_auto_sample(uint64_t now,uint64_t frames,uint64_t dropped,int suppressed,int buffering,
                         double position,int live,int subtitles) {
    if(g_maxd_auto.selection!=g_selection_serial) { maxd_auto_video_reset();g_maxd_auto.selection=g_selection_serial; }
    if(g_maxd_health_selection!=g_selection_serial) {memset(&g_maxd_health,0,sizeof g_maxd_health);g_maxd_health_selection=g_selection_serial;}
    maxd_health_sample(&g_maxd_health,now,frames,dropped,suppressed || live || !streams_scope() || g_recovery.phase,buffering);
    suppressed=suppressed || !g_auto_max || g_ask_stream || live || !streams_scope() || g_recovery.phase || g_streams.pinned>=0;
    int reason=maxd_auto_tick(&g_maxd_auto.samples,now,frames,dropped,suppressed,buffering);
    if(!reason)return 0;
    MaxdAutoFormat formats[CHOICES];int current=-1;
    for(int i=0;i<g_streams.count;++i) {
        const evo_stream_choice_t *c=&g_streams.choices[i];
        formats[i]=(MaxdAutoFormat){c->height,isfinite(c->fps)?(int)(c->fps+0.5):0,player_codec(c->video_codec),c->bitrate_bps};
        if(c->height==g_recovery.choice.height && c->fps==g_recovery.choice.fps && !strcmp(c->video_codec,g_recovery.choice.video_codec) &&
           !strcmp(c->video_range,g_recovery.choice.video_range))current=i;
    }
    if(current<0)return 0;
    if(reason==MAXD_AUTO_DROP) {
        uint64_t cost=maxd_auto_cost(formats[current]);
        if(!g_maxd_auto.failed_cost || cost<g_maxd_auto.failed_cost)g_maxd_auto.failed_cost=cost;
    } else if(reason==MAXD_AUTO_BUFFER) {
        int height=formats[current].height;
        if(!g_maxd_auto.failed_height || height<g_maxd_auto.failed_height)g_maxd_auto.failed_height=height;
    }
    int selected=maxd_auto_pick(formats,g_streams.count,current,reason,g_height,g_max_fps,g_codec,
                                g_maxd_auto.failed_cost,g_maxd_auto.failed_height);
    /* Even at the lowest available format, wait before another decision. */
    g_maxd_auto.samples.cooldown=30000;g_maxd_auto.samples.stable=0;
    if(selected<0 || !companion_url(g_streams.choices[selected].url) ||
       !streams_reopen(&g_streams.choices[selected],position,live,subtitles))return 0;
    g_maxd_health.last_action=reason;
    maxd_auto_changed(&g_maxd_auto.samples);
    return reason;
}

int tube_maxd_health_summary(char *out,size_t size) {
    if(!out || !size)return 0;
    if(!streams_scope()){snprintf(out,size,"No active video for this profile");return 0;}
    int state=g_maxd_health_selection==g_selection_serial?g_maxd_health.state:MAXD_HEALTH_LEARNING;
    const char *status=state==MAXD_HEALTH_BUFFERING?"Recent sustained buffering":
        state==MAXD_HEALTH_DROPPING?"Recent dropped frames":state==MAXD_HEALTH_STABLE?"Recent playback looks stable":"Collecting playback samples";
    const evo_stream_choice_t *c=&g_recovery.choice;
    snprintf(out,size,"%s | %dp %.0f fps %.16s | Auto Max %s",status,c->height,c->fps,c->video_codec,g_auto_max?"on":"off");return 1;
}
const char *tube_maxd_health_action(void) {
    if(!streams_scope() || g_maxd_health_selection!=g_selection_serial)return "No adaptation recorded for this video";
    return g_maxd_health.last_action==MAXD_AUTO_BUFFER?"Auto Max lowered quality after buffering":
        g_maxd_health.last_action==MAXD_AUTO_DROP?"Auto Max changed format after dropped frames":
        g_maxd_health.last_action==MAXD_AUTO_UP?"Auto Max tried a higher quality after stable playback":
        g_maxd_health.last_action==4?"You selected a lower-quality rescue for this video":
        g_maxd_health.last_action==5?"You selected an AVC compatibility rescue for this video":"No adaptation recorded for this video";
}
/* Explicit, one-video rescue. Never changes the profile's saved format limits.
 * Uses only resolved choices, the existing audio selection and scoped reopen. */
int tube_maxd_health_repair(unsigned owner,int safe,double position,int live,int subtitles) {
    if(!owner || owner!=g_selection_serial || live || !streams_scope() || g_recovery.phase || (tube_phone_owns_media && tube_phone_owns_media()))return 0;
    const evo_stream_choice_t *old=&g_recovery.choice;int best=-1;
    for(int i=0;i<g_streams.count;++i) {
        const evo_stream_choice_t *c=&g_streams.choices[i];int codec=player_codec(c->video_codec);
        if(c->is_live || c->height<144 || c->height>g_height || !isfinite(c->fps) || c->fps<=0 || c->fps>120 ||
            !companion_url(c->url) || same_picture(c,old))continue;
        if(safe ? codec!=TUBE_CODEC_AVC || c->height>1080 || c->fps>30 || strcmp(c->video_range,"SDR") :
            c->height>=old->height || (g_max_fps && c->fps>g_max_fps) || (g_codec && codec!=g_codec))continue;
        if(best<0 || c->height>g_streams.choices[best].height ||
            (c->height==g_streams.choices[best].height && c->fps>g_streams.choices[best].fps))best=i;
    }
    if(best<0 || !streams_reopen(&g_streams.choices[best],position,live,subtitles))return 0;
    /* A manual rescue is a deliberate choice; Auto Max must not undo it. */
    g_maxd_health_selection=g_selection_serial;g_maxd_health.last_action=safe?5:4;
    g_streams.pinned=0;
    for(int i=0;i<g_streams.qualities;++i)if(same_picture(&g_streams.choices[g_streams.quality[i]],&g_streams.choices[best]))g_streams.pinned=i;
    return 1;
}
