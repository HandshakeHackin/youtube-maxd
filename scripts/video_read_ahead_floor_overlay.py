"""Keep 4K read-ahead useful when DASH/WebM reports a bogus bitrate."""


def apply(replace, player):
    path = player + "core/src/services/PlaybackController.cpp"
    replace(path,
            "    const double safeFps = (fps > 1.0 && fps < 1000.0) ? fps : 30.0;",
            """    const double safeFps = (fps > 1.0 && fps < 1000.0) ? fps : 30.0;
    const AVCodecParameters* videoParams = fmt && videoIndex >= 0 &&
        videoIndex < static_cast<int>(fmt->nb_streams) && fmt->streams[videoIndex]
        ? fmt->streams[videoIndex]->codecpar : nullptr;
    const bool ultraHd = videoParams && videoParams->width > 0 && videoParams->height > 0 &&
        static_cast<long long>(videoParams->width) * videoParams->height >= 3840ll * 2160;""")
    replace(path,
            "        if (rvbytes < kVideoReadAheadMinBytes)     rvbytes = kVideoReadAheadMinBytes;\n        video_queue_byte_cap = rvbytes;",
            """        if (rvbytes < kVideoReadAheadMinBytes)     rvbytes = kVideoReadAheadMinBytes;
        /* DASH/WebM may claim 2 kbit/s for a 4K track. At the normal or
         * deeper buffer settings, keep its video queue above the 8 MiB floor;
         * Low remains the user's deliberate small-buffer choice. */
        if (ultraHd && rvbps > 0 && rvbps < 1000000 &&
            tubeRingSeconds >= kRingReadAheadSeconds && rvbytes < kVideoReadAheadMaxBytes)
            rvbytes = std::min(kVideoReadAheadMaxBytes, vmax);
        video_queue_byte_cap = rvbytes;""")
    replace(path,
            "    if (vbytes < kVideoReadAheadMinBytes) vbytes = kVideoReadAheadMinBytes;\n    video_queue_byte_cap = vbytes;",
            """    if (vbytes < kVideoReadAheadMinBytes) vbytes = kVideoReadAheadMinBytes;
    if (ultraHd && vbps > 0 && vbps < 1000000 &&
        tubePoolSeconds >= kReadAheadSeconds && vbytes < kVideoReadAheadMaxBytes)
        vbytes = kVideoReadAheadMaxBytes;
    video_queue_byte_cap = vbytes;""")
