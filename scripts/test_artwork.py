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
        if(std::strcmp(name,"large.webp")==0) assert(width==480 && height==32);
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
        cpp=temp/'test.cpp';cpp.write_text(preamble+state+decoder+'\n#define fwrite artwork_write\n#define fclose artwork_close\n'+download+'\n#undef fwrite\n#undef fclose\n'+request+test)
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','libavformat','libavcodec','libswscale','libavutil'],text=True))
        binary=temp/'test'
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')),'-std=c++20','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),*flags,'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(temp)],check=True)

if __name__=='__main__': main()