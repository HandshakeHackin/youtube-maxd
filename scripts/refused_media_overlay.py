"""Tell a media request the server refused from the end of the stream.

The built-in service does not relay media: the player's FFmpeg reads YouTube
itself, and a link that expired or was refused answers 403 there. FFmpeg's http
layer says so only in its log; the DASH demuxer then ends the input, which
looks like the end of the video. The in-place retries of the demuxer cannot
help either: they ask for the same links again.

This overlay records the time of FFmpeg's "HTTP error 403/410" report. A read
that fails within fifteen seconds of one is final at once and is classified as
a failed input, never a natural end, so the existing provider recovery runs:
a new resolve (`fresh`) and a reopen at the saved position, three times at
most per video (tube_queue_recovery). No request is repeated here.
SPDX-License-Identifier: GPL-3.0-or-later
"""


def apply_refused_media_overlay(replace, player):
    demux = player + "media/src/evo_demux.c"
    replace(demux, "static _Atomic int tube_input_state;\n", """static _Atomic int tube_input_state;
#include <libavutil/time.h>
/* When FFmpeg last reported a refused media request (0 = never in this
 * playback), on av_gettime_relative()'s clock in milliseconds. */
static _Atomic long long tube_media_refused_ms;
void evo_demux_note_refused(long long now_ms) { atomic_store_explicit(&tube_media_refused_ms, now_ms ? now_ms : 1, memory_order_release); }
void evo_demux_clear_refused(void) { atomic_store_explicit(&tube_media_refused_ms, 0, memory_order_release); }
static int tube_demux_refused(long long now_ms)
{
    const long long at = atomic_load_explicit(&tube_media_refused_ms, memory_order_acquire);
    return at && now_ms >= at && now_ms - at < 15000;
}
""")
    replace(demux, "static int demux_read_is_final(int read_result)\n{\n", """static int demux_read_is_final(int read_result)
{
    /* Refused links are refused again: only a new resolve helps. */
    if (tube_demux_refused(av_gettime_relative() / 1000))
        return 1;
""")
    replace(demux, """            tube_demux_terminal_input(read_result, io_error,
                demux_recover_tries >= DEMUX_RECOVER_TRIES);""", """            tube_demux_terminal_input(read_result, io_error,
                demux_recover_tries >= DEMUX_RECOVER_TRIES ||
                tube_demux_refused(av_gettime_relative() / 1000));""")
    replace(player + "media/include/evo_demux.h", "int evo_demux_natural_eof(void);", """int evo_demux_natural_eof(void);
/* A media request the server refused (HTTP 403/410), as FFmpeg's log reports
 * it; cleared when a playback starts. */
void evo_demux_note_refused(long long now_ms);
void evo_demux_clear_refused(void);""")
    replace(player + "core/src/services/PlaybackController.cpp", "    evo_pb_reset_decode_fatal();\n    evo_demux_reset_eof();",
            "    evo_pb_reset_decode_fatal();\n    evo_demux_reset_eof();\n    evo_demux_clear_refused();")
    app = player + "core/src/Application.cpp"
    replace(app, "static void EvoAvLogCallback(void* avcl, int level, const char* fmt, va_list vl) {", """extern "C" {
int64_t av_gettime_relative(void);
void evo_demux_note_refused(long long now_ms);
}
static void EvoAvLogCallback(void* avcl, int level, const char* fmt, va_list vl) {""")
    replace(app, '    evo_bt("ffmpeg[%d]: %s", level, line);', """    {   /* libavformat/http.c: "HTTP error %d %s" is all that tells a refused link from the end of a file */
        int code = 0;
        if (std::sscanf(line, "HTTP error %d", &code) == 1 && (code == 403 || code == 410))
            evo_demux_note_refused((long long)(av_gettime_relative() / 1000));
    }
    evo_bt("ffmpeg[%d]: %s", level, line);""")
