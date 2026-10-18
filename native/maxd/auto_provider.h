/* Included by the native provider after its scoped reopen implementation. */
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
    maxd_auto_changed(&g_maxd_auto.samples);
    return reason;
}
