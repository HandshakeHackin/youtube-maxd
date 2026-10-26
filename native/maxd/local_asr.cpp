/* YouTube Max'd on-device Whisper worker. SPDX-License-Identifier: GPL-3.0-or-later */
#include "local_asr.h"
#include "maxd_voice_model.h"
#include "tube_search_query.h"
#include "whisper.h"
#include <openssl/evp.h>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <pthread.h>
#include <vector>

namespace {
pthread_t worker{};
bool worker_joinable = false;
// The console's default pthread stack is not an inference stack. The player
// opener also requests an explicit stack instead of relying on that default.
constexpr size_t kWorkerStackSize = 4 * 1024 * 1024;
std::atomic<int> completed{0};
std::atomic<bool> cancelled{false};
bool active = false;
std::vector<float> audio;
std::string model_path;
char transcript[801]{}, failure[160]{};
/* Written and read only by the main-thread preflight/error API. */
char model_failure[160]{};

void wipe(void *memory, size_t bytes) {
    auto *p = static_cast<volatile unsigned char *>(memory);
    while (bytes--) *p++ = 0;
}
void clear_audio() {
    if (!audio.empty()) wipe(audio.data(), audio.size() * sizeof(float));
    audio.clear();
}
void error(const char *message) { std::snprintf(failure, sizeof failure, "%s", message); }
bool abort_inference(void *) { return cancelled.load(std::memory_order_relaxed); }
void quiet_log(enum ggml_log_level, const char *, void *) {}

bool verify_model() {
    std::unique_ptr<FILE, int (*)(FILE *)> file(std::fopen(model_path.c_str(), "rb"), std::fclose);
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!file || !hash || EVP_DigestInit_ex(hash.get(), EVP_sha256(), nullptr) != 1) return false;
    // Keep the 64 KiB streaming buffer off the worker stack. The old frame
    // alone overflowed a guarded 64 KiB pthread before inference began.
    std::vector<unsigned char> block(65536);
    unsigned char digest[EVP_MAX_MD_SIZE];
    size_t total = 0, count;
    while ((count = std::fread(block.data(), 1, block.size(), file.get())) != 0) {
        if (cancelled.load() || total + count > MAXD_VOICE_MODEL_SIZE ||
            EVP_DigestUpdate(hash.get(), block.data(), count) != 1) return false;
        total += count;
    }
    unsigned length = 0;
    if (std::ferror(file.get()) || total != MAXD_VOICE_MODEL_SIZE ||
        EVP_DigestFinal_ex(hash.get(), digest, &length) != 1 || length != 32) return false;
    char hex[65];
    for (unsigned i = 0; i < length; ++i) std::snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    return std::strcmp(hex, MAXD_VOICE_MODEL_SHA256) == 0;
}

void recognize() {
    int result = -1;
    try {
        if (!verify_model()) error("Voice model could not be verified. Reinstall the complete app.");
        else if (!cancelled.load()) {
            whisper_log_set(quiet_log, nullptr);
            auto options = whisper_context_default_params();
            options.use_gpu = false;
            std::unique_ptr<whisper_context, decltype(&whisper_free)> context(
                whisper_init_from_file_with_params(model_path.c_str(), options), whisper_free);
            if (!context) error("Voice model could not be loaded. Try restarting the app.");
            else if (!cancelled.load()) {
                auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
                params.n_threads = 4;
                params.language = "auto";
                params.translate = false;
                params.no_context = true;
                params.no_timestamps = true;
                params.single_segment = true;
                params.print_special = params.print_progress = params.print_realtime = params.print_timestamps = false;
                params.suppress_blank = params.suppress_nst = true;
                params.temperature = 0.0f;
                params.temperature_inc = 0.0f;
                params.greedy.best_of = 1;
                params.max_tokens = 64;
                params.abort_callback = abort_inference;
                params.abort_callback_user_data = nullptr;
                if (whisper_full(context.get(), params, audio.data(), static_cast<int>(audio.size())) == 0 && !cancelled.load()) {
                    std::string text;
                    const int segments = whisper_full_n_segments(context.get());
                    for (int i = 0; i < segments && i < 32; ++i) {
                        if (whisper_full_get_segment_no_speech_prob(context.get(), i) > 0.6f) continue;
                        const char *segment = whisper_full_get_segment_text(context.get(), i);
                        if (segment) text += segment;
                    }
                    std::string clean;
                    bool space = false;
                    for (unsigned char c : text) {
                        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { space = !clean.empty(); continue; }
                        if (space) clean += ' ';
                        space = false;
                        clean += static_cast<char>(c);
                    }
                    if (!clean.empty() && clean.size() < sizeof transcript && tube_search_query_valid(clean.c_str())) {
                        std::memcpy(transcript, clean.c_str(), clean.size() + 1);
                        result = 1;
                    } else error("No clear speech recognized. Try a shorter search in a quiet room.");
                    if (!text.empty()) wipe(text.data(), text.size());
                    if (!clean.empty()) wipe(clean.data(), clean.size());
                } else error("Speech recognition was interrupted. Please try again.");
            }
        }
    } catch (...) { error("Speech recognition could not finish. Please try again."); }
    clear_audio();
    if (cancelled.load()) { wipe(transcript, sizeof transcript); result = -1; }
    completed.store(result, std::memory_order_release);
}
void *recognize_thread(void *) { recognize(); return nullptr; }
}

extern "C" int maxd_asr_available(const char *model) {
    model_failure[0] = 0;
    if (!model || !*model || std::strlen(model) > 511) {
        std::snprintf(model_failure, sizeof model_failure, "Voice model path is invalid.");
        return 0;
    }
    /* Probe the same file API used by verification and Whisper. A metadata
     * lookup can be refused by console launchers even when the asset opens.
     * This is only a cheap availability check; verify_model() hashes every
     * byte on the worker before inference. Never read 74 MiB on the UI thread. */
    std::unique_ptr<FILE, int (*)(FILE *)> file(std::fopen(model, "rb"), std::fclose);
    if (!file) {
        const int code = errno;
        std::snprintf(model_failure, sizeof model_failure,
                      "Voice model cannot be opened (error %d). Reinstall the complete app assets.", code);
        return 0;
    }
    if (std::fseek(file.get(), 0, SEEK_END) != 0) {
        const int code = errno;
        std::snprintf(model_failure, sizeof model_failure, "Voice model cannot be seeked (error %d).", code);
        return 0;
    }
    const long bytes = std::ftell(file.get());
    if (bytes < 0) {
        const int code = errno;
        std::snprintf(model_failure, sizeof model_failure, "Voice model size cannot be read (error %d).", code);
        return 0;
    }
    if (bytes != static_cast<long>(MAXD_VOICE_MODEL_SIZE)) {
        std::snprintf(model_failure, sizeof model_failure,
                      "Voice model is incomplete: %ld bytes; expected %u. Reinstall the complete app assets.",
                      bytes, MAXD_VOICE_MODEL_SIZE);
        return 0;
    }
    return 1;
}
extern "C" int maxd_asr_start(const char *model, const int16_t *pcm, size_t samples) {
    if (active && completed.load(std::memory_order_acquire) == 0) return 0;
    maxd_asr_release();
    model_failure[0] = 0;
    if (!pcm || samples < 8000 || samples > 96000 || !maxd_asr_available(model)) {
        error("Voice model or recording is unavailable. Reinstall the complete app if needed."); return 0;
    }
    double power = 0.0;
    size_t voiced = 0;
    for (size_t i = 0; i < samples; ++i) {
        const double value = pcm[i] / 32768.0;
        power += value * value;
        if (pcm[i] > 64 || pcm[i] < -64) ++voiced;
    }
    if (power / samples < 0.000001 || voiced < samples / 100) {
        error("No speech captured. Unmute the microphone and speak clearly."); return 0;
    }
    try {
        model_path = model;
        audio.resize(samples);
        for (size_t i = 0; i < samples; ++i) audio[i] = pcm[i] / 32768.0f;
        wipe(transcript, sizeof transcript); failure[0] = 0;
        cancelled.store(false); completed.store(0); active = true;
        pthread_attr_t attributes;
        int rc = pthread_attr_init(&attributes);
        if (!rc) {
            rc = pthread_attr_setstacksize(&attributes, kWorkerStackSize);
            if (!rc) rc = pthread_create(&worker, &attributes, recognize_thread, nullptr);
            pthread_attr_destroy(&attributes);
        }
        if (rc) {
            active = false; clear_audio();
            std::snprintf(failure, sizeof failure, "Speech worker could not start (error %d). Try restarting the app.", rc);
            return 0;
        }
        worker_joinable = true;
        return 1;
    } catch (...) {
        clear_audio(); active = false;
        error("Speech recognition could not start. Please try again."); return 0;
    }
}
extern "C" int maxd_asr_poll(char *query, size_t capacity) {
    if (!active) return -1;
    const int result = completed.load(std::memory_order_acquire);
    if (!result) return 0;
    if (result != 1 || cancelled.load()) return -1;
    if (!query || std::strlen(transcript) >= capacity) return -1;
    std::memcpy(query, transcript, std::strlen(transcript) + 1);
    return 1;
}
extern "C" void maxd_asr_cancel(void) { cancelled.store(true); }
extern "C" void maxd_asr_release(void) {
    if (active && completed.load(std::memory_order_acquire) == 0) return;
    if (worker_joinable) { pthread_join(worker, nullptr); worker_joinable = false; }
    clear_audio(); wipe(transcript, sizeof transcript); active = false;
}
extern "C" const char *maxd_asr_error(void) {
    if (active && completed.load(std::memory_order_acquire) == 0) return "Recognizing speech…";
    if (*model_failure) return model_failure;
    return *failure ? failure : "No speech recognized. Check the microphone and try again.";
}
extern "C" void maxd_asr_shutdown(void) {
    cancelled.store(true);
    if (worker_joinable) { pthread_join(worker, nullptr); worker_joinable = false; }
    clear_audio(); wipe(transcript, sizeof transcript); wipe(failure, sizeof failure);
    wipe(model_failure, sizeof model_failure);
    active = false;
}
