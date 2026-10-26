#!/usr/bin/env python3
"""Production SHA verifier on a guarded 64 KiB stack; no microphone/network.

The old 65,712-byte frame faults here before Whisper can load. Keep the verifier
small even though the recognition worker now requests an explicit larger stack.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import subprocess,tempfile,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
from bootstrap_local_voice import header
import os
os.chdir(ROOT)
s=Path('native/maxd/local_asr.cpp').read_text();a=s.index('bool verify_model()');b=s.index('\nvoid recognize()',a)
pre='''#include <cstdio>
#include <memory>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>
#include <pthread.h>
#include <openssl/evp.h>
#include "maxd_voice_model.h"
static std::string model_path;
static std::atomic<bool> cancelled{false};
'''
post='''
void *run(void *) { return (void *)(verify_model()?1UL:0UL); }
int main(int n,char **v) {if(n!=2)return 3;model_path=v[1];pthread_attr_t a;pthread_attr_init(&a);pthread_attr_setstacksize(&a,65536);pthread_t t;if(pthread_create(&t,&a,run,nullptr))return 4;pthread_attr_destroy(&a);void *r;pthread_join(t,&r);return r==(void*)1?0:5;}
'''
with tempfile.TemporaryDirectory(prefix="maxd-voice-stack-") as directory:
 temp=Path(directory);header(temp/'maxd_voice_model.h')
 (temp/'test.cpp').write_text(pre+s[a:b]+post)
 subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O2','-g','-Werror=frame-larger-than=16384','-I'+str(temp),str(temp/'test.cpp'),'-lcrypto','-pthread','-o',str(temp/'test')],check=True)
 subprocess.run([str(temp/'test'),str(ROOT/'native/maxd/voice/ggml-tiny.bin')],check=True)
 print('PASS: production full-SHA model verification fits a guarded 64 KiB stack')
