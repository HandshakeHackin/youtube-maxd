"""Connect native microphone/transcription to the real provider search UI."""

def apply_voice_controls_overlay(replace, player, host):
    header = player + "core/include/evo/screens/ProviderHostScreen.hpp"
    replace(header, "    static bool playbackStartupBusy();", "    static bool playbackStartupBusy();\n    bool submitVoiceQuery(const char* query);")
    replace(header, "    void startSelected();", "    bool m_voiceHeld = false;\n    void startSelected();")
    replace(host, '#include "tube_provider.h"', '#include "tube_provider.h"\n#include "tube_voice.h"')
    replace(host, "void ProviderHostScreen::onExit()\n{", """void ProviderHostScreen::onExit()
{
    m_voiceHeld = false;
    if (m_providerId == "youtube") tube_voice_cancel();""")
    replace(host, "    (void)held;\n    (void)released;", """    (void)held;
    if (m_providerId == "youtube") {
        const int voice = tube_voice_state();
        if (voice == TUBE_VOICE_RECORDING || voice == TUBE_VOICE_TRANSCRIBING) {
            if (pressed & PadButtons::Circle) {
                m_voiceHeld = false; tube_voice_cancel();
                toast("VOICE SEARCH", "Cancelled"); return true;
            }
            if (m_voiceHeld && (released & PadButtons::L3)) {
                m_voiceHeld = false; tube_voice_finish();
                toast("VOICE SEARCH", "Recognizing… Circle cancels");
            }
            return true;
        }
    }""")
    replace(host, "    if (pressed & PadButtons::Square) {\n        openSearch();", """    if (m_providerId == "youtube" && (pressed & PadButtons::L3)) {
        const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (tube_voice_begin(0, time)) {
            m_voiceHeld = true;
            if (auto pb = Application::getInstance().getPlaybackController())
                if (pb->isActive()) pb->setPaused(true);
            toast("VOICE SEARCH", "Speak while holding L3; release to search. Circle cancels.");
        } else toast("VOICE SEARCH", "Pair your companion and enable local speech recognition first.");
        return true;
    }
    if (pressed & PadButtons::Square) {
        openSearch();""")
    replace(host, "void ProviderHostScreen::openSearch()", """bool ProviderHostScreen::submitVoiceQuery(const char* query)
{
    if (m_providerId != "youtube" || !m_opened || m_picking || m_web ||
        m_resolving || m_streamPick || !query || !*query || evo_keyboard_is_open()) return false;
    m_voiceHeld = false;
    OnSearchSubmitted(query, this);
    return true;
}

void ProviderHostScreen::openSearch()""")
    replace(host, 'evo_keyboard_open("Search YouTube videos...", m_searchQuery.c_str(), 64,',
            'evo_keyboard_open("Search YouTube videos...", m_searchQuery.c_str(), 800,')
    app = player + "core/src/Application.cpp"
    replace(app, '#include "tube_phone.h"', '#include "tube_phone.h"\n#include "tube_voice.h"')
    replace(app, "        tube_phone_sync_scope(); /* retire changed profiles before resolver callbacks */", """        tube_phone_sync_scope(); /* retire changed profiles before resolver callbacks */
        {
            char source[512]{}, token[128]{}, profile[33]{};
            tube_voice_scope(source, sizeof source, token, sizeof token, profile, sizeof profile);
            tube_voice_configure(source, token, profile);
            volatile char* clear = token;
            for (size_t i = 0; i < sizeof token; ++i) clear[i] = 0;
        }""")
    replace(app, "        tube_poll();", """        tube_poll();
        const auto voiceTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        tube_voice_tick(voiceTime);
        tube_remote_voice_available(tube_voice_available() == 1);
        if (tube_voice_state() == TUBE_VOICE_FAILED) {
            toast("VOICE SEARCH", "No speech recognized. Check the microphone and companion, then retry.");
            tube_voice_cancel();
        } else if (tube_voice_state() == TUBE_VOICE_READY && m_screenManager &&
                   m_screenManager->getCurrentScreenId() == ScreenId::EmbyBrowse) {
            auto host = static_cast<ProviderHostScreen*>(m_screenManager->getCurrentScreen());
            if (host && host->submitVoiceQuery(tube_voice_result())) tube_voice_cancel();
        }""")
    replace(app, "            phone.captions_status =", """            phone.voice = [](void* ctx, int started, unsigned owner) -> int {
                if (!started) {
                    if (tube_voice_owner() == owner) tube_voice_cancel();
                    return 1;
                }
                if (tube_voice_owner() == owner &&
                    (tube_voice_state() == TUBE_VOICE_RECORDING || tube_voice_state() == TUBE_VOICE_TRANSCRIBING)) return 1;
                auto app = static_cast<Application*>(ctx);
                auto sm = app->getScreenManager();
                if (!sm || evo_keyboard_is_open()) return 0;
                ProviderHostScreen::setPendingProvider("youtube");
                if (sm->getCurrentScreenId() == ScreenId::Player) sm->returnFromPlayback();
                if (sm->getCurrentScreenId() != ScreenId::EmbyBrowse) sm->navigateTo(ScreenId::EmbyBrowse);
                const auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                if (!tube_voice_begin(owner, time)) return 0;
                if (auto pb = app->getPlaybackController()) if (pb->isActive()) pb->setPaused(true);
                toast("VOICE SEARCH", "Speak now. Recording up to six seconds; Circle cancels.");
                return 1;
            };
            phone.captions_status =""")
    replace(app, "    evo_provider_mgr_shutdown();", "    tube_voice_shutdown();\n    evo_provider_mgr_shutdown();")
