/* Included at the end of tube_provider.c. Main-thread ownership only. GPL-3.0-or-later. */
void tube_maxd_transcript_clear(void) {
    free(g_maxd_transcript.cues);g_maxd_transcript.cues=NULL;g_maxd_transcript.count=0;
    g_maxd_transcript.state=-1;g_maxd_transcript.selection=0;++g_maxd_transcript.serial;
}
static int maxd_transcript_current(void) {
    return caption_scope() && g_maxd_transcript.selection==g_selection_serial &&
        !strcmp(g_maxd_transcript.profile,g_library_profile) && !strcmp(g_maxd_transcript.video,g_active_id);
}
static void maxd_transcript_response(int ok,int status,const char *body,size_t size,void *data) {
    if((unsigned)(size_t)data!=g_maxd_transcript.serial || !maxd_transcript_current())return;
    g_maxd_transcript.state=-1;
    if(!ok || status!=200 || !body || size>2*1024*1024)return;
    if(size && !body[size-1])--size;
    if(!size || memchr(body,0,size))return;
    char *copy_body=malloc(size+1);MaxdCue *cues=calloc(MAXD_CUES,sizeof *cues);
    if(!copy_body || !cues){free(copy_body);free(cues);return;}
    memcpy(copy_body,body,size);copy_body[size]=0;
    int count=maxd_parse_cues(copy_body,cues,MAXD_CUES);free(copy_body);
    free(g_maxd_transcript.cues);g_maxd_transcript.cues=cues;g_maxd_transcript.count=count;
    g_maxd_transcript.state=count?1:-1;
}
int tube_maxd_transcript_begin(void) {
    tube_maxd_transcript_clear();if(!caption_scope() || !g_caption_track_count)return 0;
    g_maxd_transcript.selection=g_selection_serial;g_maxd_transcript.state=0;g_maxd_transcript.deadline=now_ms()+15000;
    copy(g_maxd_transcript.profile,sizeof g_maxd_transcript.profile,g_library_profile);
    copy(g_maxd_transcript.video,sizeof g_maxd_transcript.video,g_active_id);
    int track=0;for(int i=0;i<g_caption_track_count;++i)if(!strcmp(g_caption_tracks[i].language,g_caption_language)){track=i;break;}
    if(evo_net_request_async("GET",g_caption_tracks[track].url,NULL,NULL,0,maxd_transcript_response,
                            (void *)(size_t)g_maxd_transcript.serial)){tube_maxd_transcript_clear();return 0;}
    return 1;
}
int tube_maxd_transcript_state(void) {
    if(!maxd_transcript_current() || (!g_maxd_transcript.state && now_ms()>g_maxd_transcript.deadline))return -1;
    return g_maxd_transcript.state;
}
int tube_maxd_transcript_count(void) {return tube_maxd_transcript_state()==1?g_maxd_transcript.count:0;}
const char *tube_maxd_transcript_text(int i) {return i>=0 && i<tube_maxd_transcript_count()?g_maxd_transcript.cues[i].text:"";}
double tube_maxd_transcript_seconds(int i) {return i>=0 && i<tube_maxd_transcript_count()?g_maxd_transcript.cues[i].seconds:-1;}
static int maxd_bookmark_at(int index) {
    if(!caption_scope())return -1;
    for(int i=0;i<g_maxd_mark_count;++i)if(!strcmp(g_maxd_marks[i].profile,g_library_profile) && !strcmp(g_maxd_marks[i].video,g_active_id))
        if(index--==0)return i;
    return -1;
}
int tube_maxd_bookmark_count(void) {int n=0;while(maxd_bookmark_at(n)>=0)++n;return n;}
static const char *g_maxd_bookmark_error="";
const char *tube_maxd_bookmark_error(void) {return g_maxd_bookmark_error;}
static int maxd_bookmark_saved(void) {
    g_maxd_bookmark_error=g_storage_notice?"Bookmark kept for this session only. Check free space and save it again.":"";
    return !g_storage_notice;
}
int tube_maxd_bookmark_add(unsigned selection,double seconds,const char *label) {
    g_maxd_bookmark_error="This video changed. Return to playback and try again.";
    if(!caption_scope() || selection!=g_selection_serial)return 0;
    g_maxd_bookmark_error="Choose a valid position in this video.";
    if(!isfinite(seconds)||seconds<0||seconds>604800)return 0;
    g_maxd_bookmark_error="Enter a bookmark name of 1 to 120 UTF-8 bytes.";
    if(!label)return 0;
    while(*label==' ')++label;
    size_t length=strlen(label);while(length && label[length-1]==' ')--length;
    if(!length || length>120)return 0;
    char name[121];memcpy(name,label,length);name[length]=0;
    if(!maxd_smart_text(name,120,0))return 0;
    /* Repeated saves of the same named moment succeed without duplicates,
     * including when the per-video limit is already reached. */
    for(int i=0;i<g_maxd_mark_count;++i) {
        const MaxdBookmark *b=&g_maxd_marks[i];
        if(!strcmp(b->profile,g_library_profile) && !strcmp(b->video,g_active_id) &&
            !strcmp(b->label,name) && fabs(b->seconds-seconds)<=0.5) {if(g_storage_notice)save();return maxd_bookmark_saved();}
    }
    g_maxd_bookmark_error="This video has 64 bookmarks. Remove one before saving another.";
    if(tube_maxd_bookmark_count()>=64)return 0;
    g_maxd_bookmark_error="Bookmark storage is full. Remove an old bookmark and try again.";
    if(g_maxd_mark_count>=MAXD_BOOKMARKS)return 0;
    MaxdBookmark *b=&g_maxd_marks[g_maxd_mark_count++];memset(b,0,sizeof *b);
    copy(b->profile,sizeof b->profile,g_library_profile);copy(b->video,sizeof b->video,g_active_id);
    copy(b->label,sizeof b->label,name);b->seconds=seconds;save();return maxd_bookmark_saved();
}
const char *tube_maxd_bookmark_label(int i) {int at=maxd_bookmark_at(i);return at>=0?g_maxd_marks[at].label:"";}
double tube_maxd_bookmark_seconds(int i) {int at=maxd_bookmark_at(i);return at>=0?g_maxd_marks[at].seconds:-1;}
int tube_maxd_bookmark_remove(int i) {
    int at=maxd_bookmark_at(i);if(at<0)return 0;
    memmove(g_maxd_marks+at,g_maxd_marks+at+1,(size_t)(--g_maxd_mark_count-at)*sizeof g_maxd_marks[0]);save();return 1;
}

int tube_maxd_queue_current(void) {
    if(!valid_id(g_active_id) || strcmp(g_active_profile,g_library_profile))return 0;
    Record *r=record(g_active_id,1);if(!r)return 0;
    if(!r->queued){r->queued=1;r->queue_order=++g_queue_order;r->queued_music=0;save();}return 1;
}
