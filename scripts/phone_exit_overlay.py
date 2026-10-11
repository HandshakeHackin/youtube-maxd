"""Reuse the native PS5 self-exit API only after media release and GPU park."""

def apply_phone_exit_overlay(replace, player):
    app = player + 'core/src/Application.cpp'
    header = player + 'core/include/evo/Application.hpp'
    replace(header, '    void requestSoftClose();', '    void requestSoftClose();\n    void requestAutoClose();')
    replace(header, '    bool m_softClosed = false;', '    bool m_exitAfterPark = false;\n    bool m_softClosed = false;')
    replace(app, 'void Application::requestSoftClose() {', '''void Application::requestAutoClose() {
    m_exitAfterPark = true;
    requestSoftClose();
}

void Application::requestSoftClose() {''')
    replace(app, '#include "tube_voice.h"', '''#include "tube_voice.h"
extern "C" int sceSystemServiceLoadExec(const char*, const char**);''')
    replace(app, '        if (m_softClosed) {', '''        if (m_softClosed) {
            if (m_exitAfterPark) {
                m_exitAfterPark = false; /* One attempt; a refusal remains parked. */
                if (evo_agc_runtime_exit_ready(500)) {
                    evo_usb_remote_mark_parked();
                    evo_boot_log("phone close: GPU idle; requesting PS5 self exit");
                    evo_boot_log_flush();
                    std::fflush(nullptr);
                    const int refused = sceSystemServiceLoadExec("exit", nullptr);
                    evo_boot_log("phone close: PS5 self exit returned rc=0x%x; staying parked", static_cast<unsigned>(refused));
                } else evo_boot_log("phone close: GPU still busy; staying parked");
                evo_boot_log_flush();
            }''')
    replace(app, '                pp_playback_shutdown(&g_pp_pb);', '''                pp_playback_shutdown(&g_pp_pb);
                tube_voice_shutdown();
                tube_caption_switch_cancel();
                tube_auth_cancel();
                tube_cancel();
                tube_phone_cancel();
                tube_remote_shutdown();''')
    replace(app, '                evo_agc_runtime_wait_idle(500);', '                const bool gpuIdle = evo_agc_runtime_exit_ready(500) != 0;')
    replace(app, '''                evo_boot_log("soft close: parked - GPU idle, display latched, "
                             "safe to close from the switcher");''', '''                if (gpuIdle) evo_boot_log("soft close: parked - GPU idle, display latched, safe to close from the switcher");
                else evo_boot_log("soft close: rendering parked; GPU retirement unconfirmed - no safe-close checkpoint");''')
    replace(app, '                evo_boot_log_flush();\n                evo_usb_remote_mark_parked();\n                continue;',
            '                evo_boot_log_flush();\n                if (gpuIdle) evo_usb_remote_mark_parked();\n                continue;')
    replace(app, '        tube_phone_sync_scope(); /* retire changed profiles before resolver callbacks */',
            '        if (!m_softClose) tube_phone_sync_scope(); /* retire changed profiles before resolver callbacks */')
    replace(app, '        {\n            char source[512]{}, token[128]{}, profile[33]{};',
            '        if (!m_softClose) {\n            char source[512]{}, token[128]{}, profile[33]{};')
    replace(app, '        tube_poll();', '        if (!m_softClose) tube_poll();')
    replace(app, '        tube_voice_tick(voiceTime);', '        if (!m_softClose) tube_voice_tick(voiceTime);')
    replace(app, '            tube_phone_tick(&phone);', '            if (!m_softClose) tube_phone_tick(&phone);')
    replace(app, '            phone.stop_on_disconnect = tube_phone_stop_on_disconnect();', '''            phone.stop_on_disconnect = tube_phone_stop_on_disconnect();
            phone.close_on_disconnect = tube_phone_close_on_disconnect();
            phone.close_app = [](void* ctx) -> int {
                static_cast<Application*>(ctx)->requestAutoClose(); return 1;
            };''')
    agc = player + 'media/src/evo_agc_runtime.c'
    replace(agc, 'static void agc_wait_gpu_idle(unsigned timeout_ms)', 'static int agc_wait_gpu_idle(unsigned timeout_ms)')
    replace(agc, '    if (!g_agc_dev.initialized)\n        return;\n\n    int waited_any = 0;',
            '    if (!g_agc_dev.initialized)\n        return 1;\n\n    int idle = 1;\n    int waited_any = 0;')
    replace(agc, '        if (!g_agc_dev.fence_expect[slot] || !g_agc_dev.fences[slot])\n            continue;',
            '        if (!g_agc_dev.fence_expect[slot]) continue;\n        if (!g_agc_dev.fences[slot]) { idle = 0; continue; }')
    replace(agc, '"STILL BUSY - releasing anyway"', '"STILL BUSY"')
    replace(agc, '        g_agc_dev.fence_expect[slot] = 0;\n    }\n    if (!waited_any)',
            '        if (waited >= timeout_ms) idle = 0;\n        else g_agc_dev.fence_expect[slot] = 0;\n    }\n    if (!waited_any)')
    replace(agc, '        evo_boot_log("agc drain: no submits outstanding");\n    evo_boot_log_flush();\n}',
            '        evo_boot_log("agc drain: no submits outstanding");\n    evo_boot_log_flush();\n    return idle;\n}')
    replace(agc, 'void evo_agc_runtime_wait_idle(unsigned timeout_ms)', '''int evo_agc_runtime_exit_ready(unsigned timeout_ms) {
    return agc_wait_gpu_idle(timeout_ms);
}

void evo_agc_runtime_wait_idle(unsigned timeout_ms)''')
    replace(player + 'media/include/evo_agc_runtime.h', 'void evo_agc_runtime_wait_idle(unsigned timeout_ms);',
            'void evo_agc_runtime_wait_idle(unsigned timeout_ms);\nint evo_agc_runtime_exit_ready(unsigned timeout_ms);')
