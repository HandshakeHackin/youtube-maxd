"""Run shipping artwork sniff/cache/decode paths with host FFmpeg and sanitizers."""
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get('PROSPEROTUBE_NATIVE_WORK', str(Path.home()/'.cache/prosperotube/native')))


def main():
    subprocess.run(['python3',str(ROOT/'scripts/prepare_native.py')],check=True,stdout=subprocess.DEVNULL)
    source=(WORK/'projects/evoplayer/ui_rml/src/evo_rmlui_provider_art.cpp').read_text()
    state=source.split('namespace {',1)[1].split('void evict_to_budget()',1)[0]
    decoder=source.split('#else\n',1)[1].split('#endif /* EVO_PROVIDER_ART_NO_DECODE */',1)[0]
    download=source[source.index('struct FetchCtx {'):source.index('} /* namespace */')]
    request=source[source.index('extern "C" int evo_provider_art_request'):source.index('extern "C" int evo_provider_art_ready')]
    preamble=r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iterator>
#include <filesystem>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
/* swscale writes from uninstrumented code, out of AddressSanitizer's sight:
 * convert into a guarded copy and fail on any byte past the destination rows. */
static int checked_sws_scale(SwsContext* c,const uint8_t* const src[],const int src_stride[],int y,int h,
                             uint8_t* const dst[],const int dst_stride[]) {
    std::vector<uint8_t> scratch((size_t)dst_stride[0]*481u+4096u,0xA5);
    uint8_t* planes[4]={scratch.data(),nullptr,nullptr,nullptr};
    const int rows=sws_scale(c,src,src_stride,y,h,planes,dst_stride);
    const size_t used=rows>0?(size_t)rows*(size_t)dst_stride[0]:0;
    for(size_t i=used;i<used+4096u;++i) assert(scratch[i]==0xA5 && "swscale wrote past the destination rows");
    std::memcpy(dst[0],scratch.data(),used);
    return rows;
}
#define sws_scale checked_sws_scale
#define EVO_BUNDLE_MAX_TEXTURE_BYTES (32*1024*1024)
#define PROV_LOG(...) ((void)0)
static std::string cache_root;
static int requests;
static int art_allowed=1;
static int tube_art_url_allowed(const char*) { return art_allowed; }
static void (*complete)(int,int,const char*,size_t,void*);
static void* context;
static bool fail_write;
static int closed;
static size_t artwork_write(const void* p,size_t n,size_t count,FILE* file) {
    return fail_write ? 0 : fwrite(p,n,count,file);
}
static int artwork_close(FILE* file) { ++closed; return fclose(file); }
static int evo_bundle_path(const char*,const char* relative,char* output,size_t size) {
    return std::snprintf(output,size,"%s/%s",cache_root.c_str(),relative)>=(int)size?-1:0;
}
static int evo_bundle_ensure_parent_dirs(const char* path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path()); return 0;
}
static int evo_net_request_async(const char*,const char*,const char*,const char*,int,
                                void (*cb)(int,int,const char*,size_t,void*),void* ud) {
    ++requests; complete=cb; context=ud; return 0;
}
'''
    test=r'''
static std::vector<char> bytes(const std::string& path) {
    std::ifstream file(path,std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(file),{});
}
int main(int argc,char** argv) {
    assert(argc==2); av_log_set_level(AV_LOG_QUIET);
    std::string root=argv[1]; cache_root=root+"/cache";
    auto webp=bytes(root+"/alpha.webp"); assert(webp.size()>12);
    assert(std::strcmp(sniff_ext(webp.data(),webp.size()),".webp")==0);
    for(size_t n=0;n<12;n++) assert(!sniff_ext(webp.data(),n));
    char riff[12]={'R','I','F','F',0,0,0,0,'W','A','V','E'};
    assert(!sniff_ext(riff,sizeof riff));
    char key[128];
    assert(evo_provider_art_request("youtube","file:///private",key,sizeof key)==-1);
    art_allowed=0; /* an address the provider refuses is never fetched */
    assert(evo_provider_art_request("youtube","https://refused.example/fixture",key,sizeof key)==-1 && requests==0);
    art_allowed=1;
    assert(evo_provider_art_request("youtube","https://i.ytimg.com/fixture",key,sizeof key)==0);
    assert(requests==1 && complete && context);
    complete(1,200,webp.data(),webp.size(),context);
    assert(S().arrived.size()==1 && S().in_flight==0);
    std::string cached=S().by_key[key].file; assert(cached.ends_with(".webp"));
    S()=ArtState{};
    assert(evo_provider_art_request("youtube","https://i.ytimg.com/fixture",key,sizeof key)==0);
    assert(requests==1 && S().arrived.size()==1 && S().by_key[key].file==cached);
    for(const char* name:{"alpha.webp","lossy.webp","large.webp","opaque.png","opaque.jpg"}) {
        std::vector<uint32_t> pixels; int width=0,height=0;
        assert(decode_image((root+"/"+name).c_str(),pixels,width,height));
        assert(width>0 && height>0 && width<=480 && height<=480);
        assert(pixels.size()==size_t(width)*height);
        uint32_t color=pixels[pixels.size()/2];
        if(std::strcmp(name,"alpha.webp")==0 || std::strcmp(name,"lossy.webp")==0) {
            assert((color>>24)==128 && (color&255)>=122 && (color&255)<=130);
            assert(((color>>8)&255)<5 && ((color>>16)&255)<5);
        } else {
            assert((color>>24)==255 && (color&255)>240 && ((color>>16)&255)<10);
        }
        if(std::strcmp(name,"large.webp")==0) assert(width==480 && height==270); /* letterboxed on a 16:9 card */
    }
    /* Subscriptions thumbnails: every card picture is 16:9 (the Classic card box), the
     * 4:3 one without its bars, the portrait ones of odd widths whole and pillarboxed. */
    for(const char* name:{"letterboxed.jpg","portrait203.jpg","portrait268.jpg","portrait202.webp","portrait151.jpg"}) {
        std::vector<uint32_t> pixels; int width=0,height=0;
        assert(decode_image((root+"/"+name).c_str(),pixels,width,height));
        assert(width==480 && height==270 && pixels.size()==size_t(width)*height);
        if(std::strcmp(name,"portrait151.jpg")==0) {
            /* Converted at its own size: swscale's 8-pixel blocks used to leave the last
             * 7 columns of a tight 151-pixel row unconverted (transparent). */
            const uint32_t right=pixels[size_t(height/2)*width+(480-151)/2+150];
            assert((right>>24)==255 && (right&255)>200);
        }
        const uint32_t middle=pixels[size_t(height/2)*width+width/2],corner=pixels[0],side=pixels[size_t(height/2)*width];
        assert((middle>>24)==255 && (middle&255)>200 && ((middle>>8)&255)<60);
        if(std::strcmp(name,"letterboxed.jpg")==0) {                                            /* the bars are cropped */
            const uint32_t inside=pixels[size_t(4)*width+4];
            assert((inside>>24)==255 && (inside&255)>200);
        }
        else assert(side==0 && corner==0);                                                     /* transparent pillars */
    }
    { std::vector<uint32_t> pixels;int w=0,h=0;assert(decode_image(cached.c_str(),pixels,w,h)); }
    std::ofstream broken(root+"/broken.webp",std::ios::binary);
    broken.write(webp.data(),12);broken.close();
    std::vector<uint32_t> pixels; int w=0,h=0;
    assert(!decode_image((root+"/broken.webp").c_str(),pixels,w,h));
    fail_write=true; int before_close=closed;
    assert(evo_provider_art_request("youtube","https://i.ytimg.com/write-failure",key,sizeof key)==0);
    complete(1,200,webp.data(),webp.size(),context);
    assert(S().by_key[key].failed && S().in_flight==0 && closed==before_close+1);
    assert(!std::filesystem::exists(S().by_key[key].file));
    puts("PASS: shipping WebP sniff/cache reload, lossless/lossy alpha, scaling, PNG/JPEG, malformed input and cache-write failure");
}
'''
    with tempfile.TemporaryDirectory(prefix='prosperotube-art-') as directory:
        temp=Path(directory)
        for name,width,height,alpha,codec,extra in (
            ('alpha.webp',64,32,128,'libwebp',['-lossless','1']),
            ('lossy.webp',64,32,128,'libwebp',['-quality','70']),
            ('large.webp',960,64,255,'libwebp',['-lossless','1']),
            ('opaque.png',64,32,255,'png',[]),('opaque.jpg',64,32,255,'mjpeg',[])):
            raw=temp/'input.rgba';raw.write_bytes(bytes([255,0,0,alpha])*(width*height))
            subprocess.run(['ffmpeg','-v','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',f'{width}x{height}',
                            '-i',str(raw),'-frames:v','1','-c:v',codec,*extra,str(temp/name)],check=True)
        # The shapes YouTube's TV Subscriptions tiles carry (checked on 2026-10-16): sddefault
        # 640x480 with 60-pixel bars; oardefault 203x360 and 202x360; frame0 268x480; 4:2:0 JPEG.
        for name,width,height,bars,codec in (('letterboxed.jpg',640,480,60,'mjpeg'),('portrait203.jpg',203,360,0,'mjpeg'),
                                             ('portrait268.jpg',268,480,0,'mjpeg'),('portrait202.webp',202,360,0,'libwebp'),
                                             ('portrait151.jpg',151,270,0,'mjpeg')):
            row=bytes([230,20,20,255])*width
            raw=temp/'input.rgba';raw.write_bytes(bytes(4*width)*bars+row*(height-2*bars)+bytes(4*width)*bars)
            subprocess.run(['ffmpeg','-v','error','-y','-f','rawvideo','-pixel_format','rgba','-video_size',f'{width}x{height}',
                            '-i',str(raw),'-frames:v','1','-c:v',codec,*(['-pix_fmt','yuvj420p'] if codec=='mjpeg' else []),str(temp/name)],check=True)
        cpp=temp/'test.cpp';cpp.write_text(preamble+state+decoder+'\n#define fwrite artwork_write\n#define fclose artwork_close\n'+download+'\n#undef fwrite\n#undef fclose\n'+request+test)
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','libavformat','libavcodec','libswscale','libavutil'],text=True))
        binary=temp/'test'
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++20','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),*flags,'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(temp)],check=True)

if __name__=='__main__': main()