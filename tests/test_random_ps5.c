#define EVO_TARGET_PS5 1
#include "tube_random.h"
#include <assert.h>
#include <stdio.h>
static int calls, fail, short_read, oversized;
int sysctlbyname(const char *name, void *out, size_t *length, const void *input, size_t size) {
    assert(!strcmp(name,"kern.rng_pseudo") && *length==64 && !input && !size); ++calls;
    if (calls==fail) return -1;
    for(size_t i=0;i<*length;++i) ((unsigned char*)out)[i]=(unsigned char)(calls+i);
    if(short_read) *length=8;
    if(oversized) *length=65;
    return 0;
}
int main(void) {
    unsigned char bytes[80];
    assert(!tube_random_bytes(NULL,0) && !calls && tube_random_bytes(NULL,1)<0);
    assert(!tube_random_bytes(bytes,16) && calls==1 && bytes[0]==1 && bytes[15]==16);
    calls=0; assert(!tube_random_bytes(bytes,sizeof bytes) && calls==2 && bytes[64]==2);
    calls=0;fail=2; assert(tube_random_bytes(bytes,sizeof bytes)<0);
    for(size_t i=0;i<sizeof bytes;++i) assert(!bytes[i]);
    fail=0;short_read=1;memset(bytes,1,sizeof bytes);assert(tube_random_bytes(bytes,16)<0);
    for(int i=0;i<16;++i) assert(!bytes[i]);
    short_read=0;oversized=1;assert(tube_random_bytes(bytes,16)<0);
    puts("PS5 kernel RNG: exact name, bounded blocks, partial/error/size rejection and zeroed failures passed");return 0;
}
