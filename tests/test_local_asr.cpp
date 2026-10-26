/* Real-model and lifecycle checks; no network or microphone. GPL-3.0-or-later. */
#include "local_asr.h"
#include "voice_gesture.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <pthread.h>
#include <cerrno>

static bool fail_worker_stack=false;
static unsigned explicit_workers;
extern "C" int __real_pthread_create(pthread_t *,const pthread_attr_t *,void *(*)(void *),void *);
extern "C" int __real_pthread_attr_setstacksize(pthread_attr_t *,size_t);
extern "C" int __wrap_pthread_attr_setstacksize(pthread_attr_t *a,size_t size) {
    if(fail_worker_stack)return EINVAL;
    return __real_pthread_attr_setstacksize(a,size);
}
extern "C" int __wrap_pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*run)(void *),void *data) {
    if(a) {size_t size=0;assert(!pthread_attr_getstacksize(a,&size));assert(size>=4*1024*1024);++explicit_workers;}
    return __real_pthread_create(t,a,run,data);
}

static unsigned le32(const unsigned char *p) { return p[0] | p[1]<<8 | p[2]<<16 | static_cast<unsigned>(p[3])<<24; }
static int wait_for(char *out, size_t capacity) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    int result;
    while ((result = maxd_asr_poll(out, capacity)) == 0) {
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return result;
}
int main(int argc, char **argv) {
    assert(argc == 4);
    const char *model = argv[1], *sample = argv[2], *bad_model = argv[3];
    assert(!maxd_asr_available(nullptr) && !maxd_asr_available(""));
    assert(!maxd_asr_available(std::string(512, 'x').c_str()));
    MaxdVoiceGesture g{};
    assert(maxd_voice_gesture(&g,1,1,0,0,100)==MAXD_VOICE_WAIT);
    assert(maxd_voice_gesture(&g,0,1,0,0,449)==MAXD_VOICE_WAIT);
    assert(maxd_voice_gesture(&g,0,1,0,0,450)==MAXD_VOICE_START);
    assert(!g.armed);
    assert(maxd_voice_gesture(&g,1,1,0,0,500)==MAXD_VOICE_WAIT);
    assert(maxd_voice_gesture(&g,0,0,1,0,600)==MAXD_VOICE_TAP);
    maxd_voice_gesture(&g,1,1,0,0,700);
    assert(maxd_voice_gesture(&g,0,1,0,1,701)==MAXD_VOICE_CANCEL);
    maxd_voice_gesture(&g,1,1,0,0,800);
    assert(maxd_voice_gesture(&g,0,1,0,0,799)==MAXD_VOICE_CANCEL);

    assert(maxd_asr_available(model) && !maxd_asr_available("/missing/model"));
    std::vector<int16_t> silence(96000,0);
    assert(!maxd_asr_start(model,silence.data(),silence.size()));
    assert(std::strstr(maxd_asr_error(),"No speech"));
    std::ifstream input(sample,std::ios::binary);
    std::vector<unsigned char> wav((std::istreambuf_iterator<char>(input)),{});
    assert(wav.size()>44 && !std::memcmp(wav.data(),"RIFF",4) && !std::memcmp(wav.data()+8,"WAVE",4));
    std::vector<int16_t> pcm;
    for(size_t offset=12;offset+8<=wav.size();) {
        unsigned size=le32(wav.data()+offset+4);size_t first=offset+8;
        assert(first+size<=wav.size());
        if(!std::memcmp(wav.data()+offset,"fmt ",4)) {
            assert(size>=16 && wav[first]==1 && wav[first+2]==1 && le32(wav.data()+first+4)==16000 && wav[first+14]==16);
        }
        if(!std::memcmp(wav.data()+offset,"data",4)) {
            for(size_t i=0;i+1<size && pcm.size()<96000;i+=2)
                pcm.push_back(static_cast<int16_t>(wav[first+i] | wav[first+i+1]<<8));
        }
        offset=first+size+(size&1);
    }
    assert(pcm.size()==96000);
    char query[801];
    fail_worker_stack=true;
    assert(!maxd_asr_start(model,pcm.data(),pcm.size()));
    assert(std::strstr(maxd_asr_error(),"worker could not start"));
    fail_worker_stack=false;
    assert(explicit_workers==0);
    assert(maxd_asr_available(bad_model));
    assert(maxd_asr_start(bad_model,pcm.data(),pcm.size()));
    assert(wait_for(query,sizeof query)==-1);
    assert(std::strstr(maxd_asr_error(),"verified"));
    maxd_asr_release();
    assert(maxd_asr_start(model,pcm.data(),pcm.size()));
    assert(!maxd_asr_start(model,pcm.data(),pcm.size()));
    const auto cancel_at=std::chrono::steady_clock::now();
    maxd_asr_cancel();maxd_asr_release();
    assert(std::chrono::steady_clock::now()-cancel_at<std::chrono::milliseconds(100));
    assert(wait_for(query,sizeof query)==-1);maxd_asr_release();
    assert(maxd_asr_start(model,pcm.data(),pcm.size()));
    assert(wait_for(query,sizeof query)==1);
    std::string text=query;
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    std::printf("Real local recognition: %s\n",query);
    assert(text.find("fellow")!=std::string::npos && text.find("american")!=std::string::npos);
    char too_small[2];assert(maxd_asr_poll(too_small,sizeof too_small)==-1);
    assert(maxd_asr_poll(query,sizeof query)==1);
    maxd_asr_release();assert(maxd_asr_poll(query,sizeof query)==-1);
    assert(explicit_workers>=3);
    maxd_asr_shutdown();
    std::puts("PASS: local speech recognition, real model, silence, integrity, hold/tap, cancellation and ownership");
}
