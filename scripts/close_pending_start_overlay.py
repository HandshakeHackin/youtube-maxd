"""Order provider start cancellation before soft-close frees its media."""
def apply(replace, player, work):
    base = player + "media/"
    replace(base + "include/evo_stream_io.h",
            "void evo_stream_io_abort(evo_stream_io_ctx_t *ctx);",
            """void evo_stream_io_abort(evo_stream_io_ctx_t *ctx);
/* One provider activation at a time. Set before joining its worker on close. */
void evo_stream_io_reset_start_cancel(void);
void evo_stream_io_cancel_pending_start(void);
int evo_stream_io_start_cancelled(void);""")
    replace(base + "src/evo_stream_io.c",
            "#define EVO_STREAM_DEFAULT_RING_SIZE (8 * 1024 * 1024) /* 8 MiB */",
            """#define EVO_STREAM_DEFAULT_RING_SIZE (8 * 1024 * 1024) /* 8 MiB */
static int sio_start_cancelled; /* atomic operations below; checked by FFmpeg's interrupt callback */""")
    replace(base + "src/evo_stream_io.c",
            "    if (ctx && ctx->aborted)\n        return 1;",
            """    if (__atomic_load_n(&sio_start_cancelled, __ATOMIC_ACQUIRE) || (ctx && ctx->aborted))
        return 1;""")
    replace(base + "src/evo_stream_io.c",
            "void evo_stream_io_abort(evo_stream_io_ctx_t *ctx)\n{",
            """void evo_stream_io_reset_start_cancel(void)
{
    __atomic_store_n(&sio_start_cancelled, 0, __ATOMIC_RELEASE);
}

void evo_stream_io_cancel_pending_start(void)
{
    __atomic_store_n(&sio_start_cancelled, 1, __ATOMIC_RELEASE);
}

int evo_stream_io_start_cancelled(void)
{
    return __atomic_load_n(&sio_start_cancelled, __ATOMIC_ACQUIRE);
}

void evo_stream_io_abort(evo_stream_io_ctx_t *ctx)
{""")
    header = player + "core/include/evo/screens/ProviderHostScreen.hpp"
    replace(header, "    void onExit() override;",
            """    void onExit() override;
    /* Quit must retire the asynchronous opener before playback frees its context. */
    void cancelPendingStart();""")
    provider = player + "core/src/screens/ProviderHostScreen.cpp"
    replace(provider,
            "    g_start_src = src;\n",
            """    evo_stream_io_reset_start_cancel();
    g_start_src = src;""")
    replace(provider, "void ProviderHostScreen::onExit()\n{",
            """void ProviderHostScreen::cancelPendingStart()
{
    if (!g_start_running) return;
    evo_stream_io_cancel_pending_start();
    evo_bt("provider: cancelling pending playback open before media release");
    pthread_join(g_start_thread, nullptr);
    g_start_running = false;
    evo_stream_io_reset_start_cancel(); /* later direct or Classic playback starts clean */
}

void ProviderHostScreen::onExit()
{""")
    replace(provider, "    if (g_start_running) {\n        pthread_join(g_start_thread, nullptr);\n        g_start_running = false;\n    }\n    m_tunePending = false;", """    const bool hadPendingStart = g_start_running;
    cancelPendingStart();
    if (hadPendingStart && !m_navigatingToPlayer) {
        if (auto *pb = Application::getInstance().getPlaybackController())
            pb->stopPlayback();
    }
    m_tunePending = false;""")
    # The phone overlay already stops a successful pending worker on Back. The
    # generic post-join cleanup above now covers that case and failed starts.
    if "joined_phone_worker" in (work / provider).read_text():
        replace(provider, "    const bool joined_phone_worker = g_phone_ticket && g_start_running;\n", "")
        replace(provider,
                "        if (joined_phone_worker && g_start_success) {\n"
                "            if (auto pb = Application::getInstance().getPlaybackController()) pb->stopPlayback();\n"
                "        }\n", "")
    app = player + "core/src/Application.cpp"
    replace(app,
            "                if (m_playbackController)  m_playbackController->stopPlayback();",
            """                if (m_screenManager) {
                    auto *provider = static_cast<ProviderHostScreen *>(
                        m_screenManager->getScreen(ScreenId::EmbyBrowse));
                    if (provider) provider->cancelPendingStart();
                }
                if (m_playbackController)  m_playbackController->stopPlayback();""")
    controller = player + "core/src/services/PlaybackController.cpp"
    generated = (work / controller).read_text()
    with_live_dvr = (
        "        if (converted && tube_live_seek_prepare(play_fmt,seekPos) && "
        "av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {"
    )
    without_live_dvr = (
        "        if (av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) >= 0) {"
    )
    if with_live_dvr in generated:
        anchor = with_live_dvr
        prepared = "converted && tube_live_seek_prepare(play_fmt, seekPos)"
    elif without_live_dvr in generated:
        anchor = without_live_dvr
        prepared = "true"
    else:
        raise RuntimeError("resume seek anchor changed")
    replace(controller, anchor,
            f"""        const bool seekPrepared = {prepared};
        const bool networkSeek = filePath.rfind("http://", 0) == 0 ||
                                 filePath.rfind("https://", 0) == 0;
        if (networkSeek && seekPrepared) evo_stream_io_set_deadline(m_streamIo, 12.0);
        const int resumeSeekRc = seekPrepared
            ? av_seek_frame(play_fmt, seekStream, seekTs, AVSEEK_FLAG_BACKWARD) : -1;
        if (networkSeek && seekPrepared) evo_stream_io_set_deadline(m_streamIo, 0.0);
        if (evo_stream_io_start_cancelled()) return false; /* quit joins us, then releases media */
        if (networkSeek && seekPrepared && evo_stream_io_deadline_expired(m_streamIo)) {{
            evo_boot_log("PlaybackController: resume seek timed out after 12 s");
            stopPlayback();
            toast("STREAM FAIL", "Timed out seeking this stream");
            return false;
        }}
        if (resumeSeekRc >= 0) {{""")
