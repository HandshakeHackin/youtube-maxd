// Real TextEngine and AGC slot/retirement methods; only GPU allocation is simulated.
#include "afterglow/text_engine.hpp"
#include "afterglow/text.hpp"
#include <cassert>
#include <cstdio>
#include "atlas_backend.hpp"

hui::gfx::Font::Shaper *hui::gfx::Font::shaper = nullptr;
namespace afterglow { void set_reading_face(const Face *) {} }

struct Platform final : afterglow::Platform {
    evo::hui_agc::AgcBatch batch;
    bool coverage = false;
    unsigned updates = 0;
    std::uint32_t create_texture(int w,int h,const std::uint8_t *p) override {return batch.create_texture(w,h,p);}
    bool update_texture(std::uint32_t h,int w,int y,const std::uint8_t *p) override {
        ++updates; return batch.update_texture(h,w,y,p);
    }
    void release_texture(std::uint32_t h) override {batch.release_texture(h);}
    std::uint32_t create_glyph_page(int n,const std::uint8_t *p) override {
        return coverage ? batch.create_texture(n,n,p) : 0;
    }
    bool update_glyph_page(std::uint32_t h,int n,const std::uint8_t *p) override {
        return update_texture(h,n,n,p);
    }
    unsigned fetch(const std::string&,Fetched) override {return 0;}
    void cancel_fetch(unsigned) override {}
    bool read_asset(const std::string&,std::string*) override {return false;}
    void wall_clock(int*,int*) override {}
    const char *translate(const char *s) override {return s;}
    unsigned language_revision() override {return 0;}
    void log(const char*) override {}
};

static void test_engine(const char *font_path, bool coverage) {
    Platform p; p.coverage=coverage;
    hui::gfx::Font font;
    hui::ui::Fonts fonts; fonts.regular.font=&font;
    afterglow::TextSetup setup;setup.regular=font_path;setup.page_size=512;setup.max_pages=2;
    std::string error;
    unsigned char pixel[4]={1,1,1,1};
    auto hole=p.create_texture(1,1,pixel);
    auto engine=afterglow::TextEngine::create(p,setup,&error);assert(engine);
    p.batch.fail=true;engine->bind(fonts);
    std::vector<hui::gfx::GlyphQuad> quads;
    const auto layout=[&](const char *text) {
        quads.clear(); engine->layout(font,text,0,100,24,hui::gfx::Align::left,quads,0);
    };
    for(int i=0;i<3;++i) {engine->begin_frame();layout("1");engine->end_frame();}
    assert(quads.empty()); // failed initial page creation remains retryable
    p.batch.fail=false;
    engine->begin_frame();layout("1");engine->end_frame();assert(quads.size()==1);
    const auto handle=quads[0].texture;
    assert(handle!=hole);
    p.release_texture(hole); // replacement must not migrate to this earlier free slot
    engine->begin_frame();layout("1");engine->end_frame();
    auto old=p.batch.textures_[handle-1]->pixels;
    layout("W");assert(quads.size()==1 && quads[0].texture==handle);
    // A page overlay laid out after upload: begin must not discard its pending pixels.
    engine->begin_frame();layout("W");engine->end_frame();
    assert(quads.size()==1 && quads[0].texture==handle);
    assert(p.batch.textures_[handle-1]->pixels!=old);
    const unsigned uploaded=p.updates;
    engine->end_frame();assert(p.updates==uploaded); // repeated player finishes do not reupload
    engine->begin_frame();layout("Z");assert(quads.size()==1);
    old=p.batch.textures_[handle-1]->pixels;
    const auto *before=p.batch.textures_[handle-1];
    const auto retired=p.batch.pending_free_.size();
    p.batch.fail=true;engine->end_frame();
    assert(p.batch.textures_[handle-1]==before && p.batch.textures_[handle-1]->pixels==old);
    assert(p.batch.pending_free_.size()==retired);
    p.batch.fail=false;engine->begin_frame();layout("Z");engine->end_frame();
    assert(quads[0].texture==handle && p.batch.textures_[handle-1]->pixels!=old);
    // Old allocations remain alive until the actual retirement method sees GPU idle.
    idle=false;for(int i=0;i<10;++i)p.batch.end_frame();assert(!p.batch.pending_free_.empty());
    idle=true;for(int i=0;i<4;++i)p.batch.end_frame();assert(p.batch.pending_free_.empty());
    assert(!p.batch.update_texture(0,1,1,pixel));
    assert(!p.batch.update_texture(99999,1,1,pixel));
    assert(!p.batch.update_texture(hole,1,1,pixel));
}
int main(int argc,char **argv) {
    assert(argc==2);test_engine(argv[1],false);test_engine(argv[1],true);
    std::puts("Afterglow atlas pending uploads, stable handles, allocation retry and GPU retirement: PASS");
}
