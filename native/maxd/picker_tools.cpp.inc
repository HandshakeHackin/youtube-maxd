void AudioTrackPickerScreen::maxdText(const char* text,void* data) {
    auto self=static_cast<AudioTrackPickerScreen*>(data);
    if(!self || !text || self->m_tubeExtraGroup!=-5 || !self->m_maxdOwner || self->m_maxdOwner!=tube_caption_selection())return;
    if(self->m_maxdNaming) {
        toast("BOOKMARKS",tube_maxd_bookmark_add(self->m_maxdOwner,self->m_maxdPosition,text)?"Saved for this profile":"Could not save this bookmark");
    } else if(maxd_smart_text(text,128,1)) self->m_maxdQuery=text;
    self->refreshTracks();self->m_selectedIndex=self->m_scrollOffset=0;
}
void AudioTrackPickerScreen::maxdTools() {
    Entry e;e.kind=KindMaxdSearch;e.label="Search transcript";e.detail=m_maxdQuery.empty()?"Find words in available captions":m_maxdQuery;m_tracks.push_back(e);
    e.kind=KindMaxdSave;e.label="Save timestamp bookmark";e.detail="Name this moment; saved locally for this profile";m_tracks.push_back(e);
    for(int i=0;i<tube_maxd_bookmark_count();++i) {
        e.kind=KindMaxdBookmark;e.streamIndex=i;e.label=tube_maxd_bookmark_label(i);
        int sec=static_cast<int>(tube_maxd_bookmark_seconds(i));char t[128];std::snprintf(t,sizeof t,"%d:%02d:%02d | Cross jumps; Square deletes",sec/3600,sec/60%60,sec%60);
        e.detail=t;m_tracks.push_back(e);
    }
    m_maxdSeen=tube_maxd_transcript_state();
    if(m_maxdSeen!=1) {
        e.kind=KindTubeNotice;e.label=m_maxdSeen==0?"Loading transcript...":"No transcript available";
        e.detail="Uses the selected caption language, or the first available track";m_tracks.push_back(e);return;
    }
    int hits=0;
    for(int i=0;i<tube_maxd_transcript_count() && hits<256;++i) {
        if(!maxd_smart_contains(tube_maxd_transcript_text(i),m_maxdQuery.c_str()))continue;
        e.kind=KindMaxdCue;e.streamIndex=i;e.label=tube_maxd_transcript_text(i);
        int sec=static_cast<int>(tube_maxd_transcript_seconds(i));char t[96];std::snprintf(t,sizeof t,"%d:%02d:%02d | Jump to this line",sec/3600,sec/60%60,sec%60);
        e.detail=t;m_tracks.push_back(e);++hits;
    }
    e.kind=KindTubeNotice;e.label=hits?"Search to narrow these results":"No matching caption lines";
    e.detail="Up to 256 matching lines per search; automatic captions can contain errors";m_tracks.push_back(e);
}
