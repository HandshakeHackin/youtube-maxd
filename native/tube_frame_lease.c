/* Copyright (C) 2026 BlackBearReloaded. SPDX-License-Identifier: GPL-3.0-or-later */
#include "tube_frame_lease.h"
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
/* Five resident codec modes today; reserve only a small fixed metadata table. */
static struct pool {uintptr_t base;size_t stride;unsigned count,refs[TUBE_FRAME_POOL_SLOTS];} pools[8];
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static struct pool *find_pool(uintptr_t base) {
    for(unsigned i=0;i<8;++i)if(pools[i].base==base&&base)return &pools[i];
    return NULL;
}
int tube_frame_pool_register(void *base,size_t stride,unsigned count) {
    uintptr_t address=(uintptr_t)base;
    if(!address||!stride||!count||count>TUBE_FRAME_POOL_SLOTS||stride>SIZE_MAX/count||address>UINTPTR_MAX-stride*count)return 0;
    pthread_mutex_lock(&mutex);struct pool *p=find_pool(address);
    if(!p)for(unsigned i=0;i<8;++i)if(!pools[i].base){p=&pools[i];break;}
    int ok=p && !p->base;
    if(ok){p->base=address;p->stride=stride;p->count=count;}
    pthread_mutex_unlock(&mutex);return ok;
}
int tube_frame_pool_claim(void *base,unsigned first) {
    pthread_mutex_lock(&mutex);struct pool *p=find_pool((uintptr_t)base);int result=p?-1:-2;
    if(p)for(unsigned i=0;i<p->count;++i){unsigned slot=(first+i)%p->count;if(!p->refs[slot]){p->refs[slot]=1;result=(int)slot;break;}}
    pthread_mutex_unlock(&mutex);return result;
}
int tube_frame_lease_hold(const void *pointer) {
    uintptr_t address=(uintptr_t)pointer;int result=0;
    pthread_mutex_lock(&mutex);
    for(unsigned i=0;i<8;++i){struct pool *p=&pools[i];
        if(!p->base||address<p->base||address-p->base>=p->stride*p->count)continue;
        size_t offset=address-p->base;unsigned slot=(unsigned)(offset/p->stride);
        result=-1;
        if(offset%p->stride==0&&p->refs[slot]&&p->refs[slot]<UINT_MAX){++p->refs[slot];result=1;}
        break;
    }
    pthread_mutex_unlock(&mutex);return result;
}
void tube_frame_lease_release(const void *pointer) {
    uintptr_t address=(uintptr_t)pointer;
    pthread_mutex_lock(&mutex);
    for(unsigned i=0;i<8;++i){struct pool *p=&pools[i];
        if(!p->base||address<p->base||address-p->base>=p->stride*p->count)continue;
        size_t offset=address-p->base;unsigned slot=(unsigned)(offset/p->stride);
        if(offset%p->stride==0&&p->refs[slot])--p->refs[slot];
        break;
    }
    pthread_mutex_unlock(&mutex);
}
unsigned tube_frame_pool_busy(const void *base) {
    unsigned busy=0;pthread_mutex_lock(&mutex);struct pool*p=find_pool((uintptr_t)base);
    if(p)for(unsigned i=0;i<p->count;++i)busy+=p->refs[i]!=0;
    pthread_mutex_unlock(&mutex);return busy;
}
int tube_frame_pool_unregister(const void *base) {
    int ok=1;pthread_mutex_lock(&mutex);struct pool*p=find_pool((uintptr_t)base);
    if(p){for(unsigned i=0;i<p->count;++i)if(p->refs[i])ok=0;if(ok)memset(p,0,sizeof *p);}
    pthread_mutex_unlock(&mutex);return ok;
}
