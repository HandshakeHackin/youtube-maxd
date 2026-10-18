#include <assert.h>
#include <stdio.h>
#include "maxd/experience_store.h"
#include "maxd/session.h"
#include "maxd/voice_commands.h"
#include "tube_background_play.h"
#include "tube_video_destination.h"
static void transcript(void) {
    char text[]="WEBVTT\n\nNOTE private metadata\nignore\n\n00:01.250 --> 00:03.000\n<b>Hello</b> &amp; welcome\nsecond line\n\n2\n00:01:02,500 --> 00:01:04,000\nNext moment\n\n00:99:00.000 --> 00:99:01.000\nInvalid clock\n\n";
    MaxdCue rows[8];int n=maxd_parse_cues(text,rows,8);assert(n==2);
    assert(rows[0].seconds==1.25 && !strcmp(rows[0].text,"Hello & welcome second line"));assert(rows[1].seconds==62.5);
    assert(maxd_clock("00:NaN")<0 && maxd_clock("01:60.0")<0 && maxd_clock("999999:00:00")<0 && maxd_clock("01:00garbage")<0);
    assert(maxd_smart_contains("Caf\xc3\xa9 HELLO","hello"));
    g_maxd_mark_count=1;g_maxd_marks[0]=(MaxdBookmark){"guest","abcdefghijk","A moment",12.25};
    cJSON *json=cJSON_CreateObject();maxd_experience_write(json);g_maxd_mark_count=0;maxd_experience_read(json);
    assert(g_maxd_mark_count==1 && g_maxd_marks[0].seconds==12.25);cJSON_Delete(json);(void)g_maxd_transcript;
}
static void sessions(void) {
    int64_t values[]={400,350,250,600,-1,0,999999};unsigned char picked[7];
    assert(maxd_session_pick(values,7,1000,picked)==1000);assert(picked[0] && picked[1] && picked[2]);
    assert(!picked[4] && !picked[5] && !picked[6]);
    /* Compare every result to exhaustive subsets, including ties and duplicates. */
    for(int seed=1;seed<50;++seed) {
        int64_t durations[8];for(int i=0;i<8;++i)durations[i]=(seed*(i+3)*17)%200+1;
        for(int budget=1;budget<601;budget+=17) {
            int best=0;for(unsigned mask=0;mask<256;++mask){int total=0;for(int i=0;i<8;++i)if(mask&(1u<<i))total+=(int)durations[i];if(total<=budget && total>best)best=total;}
            unsigned char choice[8];int got=maxd_session_pick(durations,8,budget,choice),sum=0;
            for(int i=0;i<8;++i)if(choice[i])sum+=(int)durations[i];
            assert(got==best && sum==best);
        }
    }
    assert(maxd_session_pick(values,7,7201,picked)<0);
}
static void voice(void) {
    assert(maxd_voice_command(" Pause video! ").kind==MAXD_CMD_PAUSE);
    assert(maxd_voice_command("resume").kind==MAXD_CMD_RESUME);
    assert(maxd_voice_command("forward 30 seconds").value==30);
    assert(maxd_voice_command("back 45 seconds").value==-45);
    assert(maxd_voice_command("quality 4k").value==2160);
    assert(maxd_voice_command("captions off").kind==MAXD_CMD_CAPTIONS);
    assert(maxd_voice_command("queue this video").kind==MAXD_CMD_QUEUE);
    assert(!maxd_voice_command("forward 999999 seconds").kind);
    assert(!maxd_voice_command("forward 30 seconds cats").kind);
    assert(!maxd_voice_command("pause and delete everything").kind);
    assert(!maxd_voice_command("play relaxing music").kind);
}
static void picture(void) {
    assert(!tube_background_play_picture_active());
    tube_background_play_picture(1);assert(!tube_background_play_picture_active());
    assert(!tube_background_play_begin(0,"guest"));assert(tube_background_play_begin(7,"guest"));
    assert(!tube_background_play_picture_active());
    tube_background_play_picture(1);assert(tube_background_play_picture_active());
    assert(tube_background_play_matches(7,"guest") && !tube_background_play_matches(7,"child"));
    tube_video_destination_set(1392,730,480,270);tube_video_destination d;tube_video_destination_get(1920,1080,&d);
    assert(d.x==1392 && d.y==730 && d.w==480 && d.h==270);
    /* A fresh browsing session must not inherit the previous PiP choice. */
    assert(tube_background_play_begin(8,"child"));assert(!tube_background_play_picture_active());
    tube_background_play_picture(1);assert(tube_background_play_picture_active());
    tube_background_play_picture(0);assert(!tube_background_play_picture_active());
    tube_background_play_picture(1);
    tube_background_play_clear();tube_video_destination_reset();assert(!tube_background_play_picture_active());
    assert(tube_background_play_begin(9,"guest"));assert(!tube_background_play_picture_active());
    tube_background_play_clear();
    tube_video_destination_get(1920,1080,&d);assert(!d.x && d.w==1920);
}
int main(void){transcript();sessions();voice();picture();puts("PASS: transcript parsing/persistence, optimal bounded sessions, strict voice commands and PiP lease/destination");}
