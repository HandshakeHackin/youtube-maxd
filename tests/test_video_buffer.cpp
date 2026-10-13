/* The Video buffer preference in the shipping read-ahead sizing. sizeReadAhead()
 * and its constants come from the generated PlaybackController.cpp
 * (scripts/extract_read_ahead_test.py); the packet queues and the stream are
 * stand-ins. Medium must give exactly the ceilings the player had without the
 * preference; the other choices move the depth and stay inside the ceilings.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_player_extra.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

struct AVCodecParameters { long long bit_rate; };
struct AVStream { AVCodecParameters *codecpar; };
struct AVFormatContext { unsigned nb_streams; AVStream **streams; long long bit_rate; };
struct PacketQueue { size_t ring; };
static PacketQueue video_packet_queue, audio_packet_queue;
static bool ring_available = true;
static int packet_queue_use_ring(PacketQueue *q, size_t bytes) { q->ring = ring_available ? bytes : 0; return ring_available; }
static size_t packet_queue_ring_size(PacketQueue *q) { return q->ring; }
static int video_packet_cap, audio_packet_cap;
static long long video_queue_byte_cap, audio_queue_byte_cap;
static char logged[512];
static void evo_boot_log(const char *format, ...) {
    va_list arguments; va_start(arguments, format); vsnprintf(logged, sizeof logged, format, arguments); va_end(arguments);
}
#include "tube_read_ahead.inc"

struct Caps { int video, audio; long long video_bytes, audio_bytes; };
static bool operator==(const Caps &a, const Caps &b) {
    return a.video == b.video && a.audio == b.audio && a.video_bytes == b.video_bytes && a.audio_bytes == b.audio_bytes;
}
static Caps sized(int buffer, bool network, bool ring, long long bits_per_second, double fps) {
    AVCodecParameters parameters = {bits_per_second}; AVStream stream = {&parameters}; AVStream *streams[] = {&stream};
    AVFormatContext format = {1, streams, 0};
    TubePlayerExtra prefs; tube_player_extra_defaults(&prefs); prefs.video_buffer = buffer; tube_player_extra_select(&prefs);
    assert(tube_extra_buffer() == buffer);
    ring_available = ring; logged[0] = 0;
    video_packet_cap = audio_packet_cap = -1; video_queue_byte_cap = audio_queue_byte_cap = -1;
    sizeReadAhead(network, &format, 0, fps);
    return {video_packet_cap, audio_packet_cap, video_queue_byte_cap, audio_queue_byte_cap};
}
/* What the player sized before the preference existed: the pinned function with its own two constants. */
static Caps original(bool ring, long long bits_per_second, double fps) {
    const double seconds = ring ? kRingReadAheadSeconds : kReadAheadSeconds;
    const long long ceiling = ring ? std::min(kVideoRingMaxBytes, static_cast<long long>(kVideoRingBytes) * 3 / 4) : kVideoReadAheadMaxBytes;
    long long bytes = bits_per_second > 0 ? static_cast<long long>(seconds * static_cast<double>(bits_per_second) / 8.0) : ceiling;
    bytes = std::max(std::min(bytes, ceiling), kVideoReadAheadMinBytes);
    const int packets = std::min(std::max(static_cast<int>(seconds * fps), 96), ring ? kVideoPacketCapRing : kVideoPacketCapMax);
    return {packets, ring ? kAudioPacketCapRing : kAudioPacketCapMax, bytes,
            ring ? std::min(kAudioRingMaxBytes, static_cast<long long>(kAudioRingBytes) * 3 / 4) : kAudioReadAheadMaxBytes};
}

int main() {
    const long long megabyte = 1024 * 1024;
    /* The mapping itself: SmartTube's 5, 30, 50 and 100 seconds; Medium is whatever the player has. */
    assert(tube_buffer_seconds(TUBE_BUFFER_LOW, 30.0, 1) == 5.0 && tube_buffer_seconds(TUBE_BUFFER_MEDIUM, 30.0, 1) == 30.0);
    assert(tube_buffer_seconds(TUBE_BUFFER_HIGH, 30.0, 1) == 50.0 && tube_buffer_seconds(TUBE_BUFFER_HIGHEST, 30.0, 1) == 100.0);
    assert(tube_buffer_seconds(TUBE_BUFFER_MEDIUM, 12.5, 1) == 12.5 && tube_buffer_seconds(TUBE_BUFFER_MEDIUM, 10.0, 0) == 10.0);
    /* Without the packet ring the queued packets live in the scarce pool: nothing reads further ahead than the player did. */
    assert(tube_buffer_seconds(TUBE_BUFFER_LOW, 10.0, 0) == 5.0 && tube_buffer_seconds(TUBE_BUFFER_HIGH, 10.0, 0) == 10.0);
    assert(tube_buffer_seconds(TUBE_BUFFER_HIGHEST, 10.0, 0) == 10.0 && tube_buffer_seconds(TUBE_BUFFER_LOW, 3.0, 0) == 3.0);
    assert(tube_buffer_seconds(-1, 30.0, 1) == 30.0 && tube_buffer_seconds(4, 30.0, 1) == 30.0 && tube_buffer_seconds(TUBE_BUFFER_HIGH, 0.0, 1) == 0.0);
    assert(kRingReadAheadSeconds == 30.0 && kReadAheadSeconds == 10.0);   /* what Medium has meant so far */

    /* Medium, the default: exactly the player's ceilings, for every kind of stream, with and without the ring. */
    TubePlayerExtra defaults; tube_player_extra_defaults(&defaults);
    assert(defaults.video_buffer == TUBE_BUFFER_MEDIUM);
    static const long long rates[] = {0, 300000, 1100000, 3248000, 17174000, 54000000, 400000000};
    static const double frames[] = {0.0, 24.0, 30.0, 60.0, 120.0, 5000.0};
    for (const long long rate : rates) for (const double fps : frames) for (const bool ring : {true, false}) {
        const double safe = fps > 1.0 && fps < 1000.0 ? fps : 30.0;
        assert(sized(TUBE_BUFFER_MEDIUM, true, ring, rate, fps) == original(ring, rate, safe));
    }
    /* A local file keeps its flat 96 packets whatever the choice. */
    for (int buffer = TUBE_BUFFER_LOW; buffer <= TUBE_BUFFER_HIGHEST; ++buffer)
        assert((sized(buffer, false, true, 3248000, 60.0) == Caps{96, 96, 0, 0}));

    /* 1080p60 at 3.2 Mbit/s with the ring: seconds times the frame rate; the byte ceiling follows, with its 8 MiB floor. */
    assert((sized(TUBE_BUFFER_LOW, true, true, 3248000, 60.0) == Caps{300, kAudioPacketCapRing, 8 * megabyte, 64 * megabyte}));
    assert(strstr(logged, "read-ahead 5s (ring)"));
    assert((sized(TUBE_BUFFER_MEDIUM, true, true, 3248000, 60.0) == Caps{1800, kAudioPacketCapRing, 12180000, 64 * megabyte}));
    assert(strstr(logged, "read-ahead 30s (ring)"));
    assert((sized(TUBE_BUFFER_HIGH, true, true, 3248000, 60.0) == Caps{3000, kAudioPacketCapRing, 20300000, 64 * megabyte}));
    assert(strstr(logged, "read-ahead 50s (ring)"));
    assert((sized(TUBE_BUFFER_HIGHEST, true, true, 3248000, 60.0) == Caps{6000, kAudioPacketCapRing, 40600000, 64 * megabyte}));
    assert(strstr(logged, "read-ahead 100s (ring)"));
    /* 2160p60 VP9 at 17 Mbit/s: 100 s is 205 MiB, inside the ring's 384 MiB. A 54 Mbit/s stream meets that ceiling. */
    assert(sized(TUBE_BUFFER_HIGHEST, true, true, 17174000, 60.0).video_bytes == 214675000);
    assert(sized(TUBE_BUFFER_HIGHEST, true, true, 54000000, 24.0).video_bytes == 384 * megabyte);
    /* Never more packets than the ring has slots for, never fewer than the player's floor. */
    assert(sized(TUBE_BUFFER_HIGHEST, true, true, 3248000, 120.0).video == kVideoPacketCapRing);
    assert(sized(TUBE_BUFFER_LOW, true, true, 3248000, 12.0).video == 96);
    /* Without the ring: Low reads five seconds ahead, the rest stay at the player's ten. */
    assert((sized(TUBE_BUFFER_LOW, true, false, 17174000, 30.0) == Caps{150, kAudioPacketCapMax, 10733750, kAudioReadAheadMaxBytes}));
    assert(strstr(logged, "read-ahead 5s ->"));
    for (int buffer = TUBE_BUFFER_MEDIUM; buffer <= TUBE_BUFFER_HIGHEST; ++buffer) {
        assert(sized(buffer, true, false, 17174000, 30.0) == original(false, 17174000, 30.0));
        assert(strstr(logged, "read-ahead 10s ->"));
    }
    tube_player_extra_select(nullptr);
    puts("PASS: video buffer choices in the shipping read-ahead sizing, Medium unchanged, ring and pool ceilings kept");
    return 0;
}
