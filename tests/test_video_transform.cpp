#include "tube_video_transform.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
struct vec2 {float x,y;vec2(float a,float b):x(a),y(b){} vec2 operator-()const{return {-x,-y};}};
#include "transform.inc"
int main() {
    for(int width:{5,8,1920})for(int height:{7,12})for(unsigned code=0;code<8;++code) {
        int lw,lh;tube_video_transform_canvas(code,width,height,&lw,&lh);
        std::vector<std::vector<int>> image(lh,std::vector<int>(lw));
        for(int y=0;y<lh;++y)for(int x=0;x<lw;++x)image[y][x]=y*lw+x;
        if(code&4)for(auto& row:image)std::reverse(row.begin(),row.end());
        for(unsigned i=0;i<(code&3);++i) {
            std::vector<std::vector<int>> rotated(image[0].size(),std::vector<int>(image.size()));
            for(size_t y=0;y<image.size();++y)for(size_t x=0;x<image[y].size();++x)
                rotated[x][image.size()-1-y]=image[y][x];
            image=std::move(rotated);
        }
        assert(image.size()==(size_t)height && image[0].size()==(size_t)width);
        for(int edge=0;edge<2;++edge) {
            const int ox=edge?0:1,oy=edge?0:2,ow=edge?lw:3,oh=edge?lh:2;
            int x=ox,y=oy,w=ow,h=oh;tube_video_transform_rect(code,width,height,&x,&y,&w,&h);
            assert(x>=0 && y>=0 && x+w<=width && y+h<=height);
            int minx=width,miny=height,maxx=-1,maxy=-1;
            for(int py=0;py<height;++py)for(int px=0;px<width;++px) {
                int id=image[py][px],sx=id%lw,sy=id/lw;
                if(sx>=ox && sx<ox+ow && sy>=oy && sy<oy+oh) {
                    minx=std::min(minx,px);maxx=std::max(maxx,px);
                    miny=std::min(miny,py);maxy=std::max(maxy,py);
                }
            }
            assert(x==minx && y==miny && w==maxx-minx+1 && h==maxy-miny+1);
            for(int sy=0;sy<oh;++sy)for(int sx=0;sx<ow;++sx) {
                vec2 p=tube_transform(vec2(2.f*(sx+.5f)/ow-1,1-2.f*(sy+.5f)/oh),(float)code);
                float px=x+(p.x+1)*w*.5f-.5f,py=y+(1-p.y)*h*.5f-.5f;
                int ix=(int)std::lround(px),iy=(int)std::lround(py);
                assert(std::fabs(px-ix)<.001f && std::fabs(py-iy)<.001f);
                assert(image[iy][ix]==(oy+sy)*lw+ox+sx);
            }
        }
    }
    puts("PASS: staged GLSL transform and final viewport mapping match labeled-image rotation/mirroring");
}
