// Interface preferences through the real provider host, the shipping browser
// layout, pinned RmlUi and the CPU rasteriser: every interface scale and video
// grid scale keeps the cards on screen, the paging and the D-pad correct and
// the frame drawn; card text, the title marquee, colour schemes, positions
// saved under another page size and the unchanged default frame.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <time.h>
#include <unistd.h>
#define private public
#include "evo_rmlui_provider.h"
#undef private
#include "evo_rmlui_provider.cpp"
#include "evo_rmlui_render.h"
#include "evo_rmlui_system.h"
extern "C" {
#include "evo_data_path.h"
}

const EvoRmlBundleFile* evo_rmlui_bundle_find(const std::string&) { return nullptr; }
extern "C" int tube_test_clock_gettime(clockid_t, struct timespec* t) { t->tv_sec=100; t->tv_nsec=0; return 0; }
extern "C" int evo_net_request_async(const char*,const char*,const char*,const char**,int,evo_net_cb,void*) { return -1; }
extern "C" void evo_error_set(const char*, ...) {}
extern "C" int evo_failure_reason(char*, size_t) { return 0; }
extern "C" int evo_provider_art_ready(const char*) { return 0; }
extern "C" int evo_provider_art_request(const char*,const char*,char*,size_t) { return -1; }
extern "C" void evo_bundle_poll(void) {}
extern "C" void evo_provider_art_poll(void) {}

struct TestSystem : EvoSystemInterface { double now = 0; double GetElapsedTime() override { return now; } };

static std::vector<evo_provider_item_t> staged;
static int staged_catalog(const char*, int page, evo_provider_items_cb cb, void* ud) {
    const size_t offset = (size_t)page * 24, count = offset < staged.size() ? std::min<size_t>(24, staged.size() - offset) : 0;
    cb(1, count ? staged.data() + offset : nullptr, (int)count, offset + count < staged.size(), ud); return 0;
}
static int chosen_ok;
static void chosen(int ok, const evo_provider_item_t*, int, int, void*) { chosen_ok = ok; }
static void choose(const std::string& id) {
    chosen_ok = 0; evo_provider_youtube.list_catalog(id.c_str(), 0, chosen, nullptr); assert(chosen_ok);
}
static const int kTotal = 60;

int main(int argc,char**argv) {
    assert(argc==3); std::filesystem::path source(argv[1]),fonts(argv[2]);
    char directory[]="/tmp/prosperotube-ui-prefs-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    assert(!evo_provider_youtube.init());
    evo_provider_t provider = evo_provider_youtube; provider.list_catalog = staged_catalog;
    const int width=1920, height=1080;
    TestSystem system; EvoRenderInterface renderer(width,height);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts/"Lato-Regular.ttf").string())); assert(Rml::LoadFontFace((fonts/"Lato-Bold.ttf").string()));
    EvoRmlProviderHost host; host.m_context=Rml::CreateContext("ui-prefs",{width,height}); assert(host.m_context);
    host.m_provider_id="youtube"; host.m_provider=&provider;
    host.m_model_name="youtube"; assert(host.RegisterDataModel());
    host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    for(int i=0;i<kTotal;++i) {
        evo_provider_item_t item; memset(&item,0,sizeof item);
        snprintf(item.id,sizeof item.id,"video%02d",i);
        if(i%2) snprintf(item.title,sizeof item.title,"Short %d",i);
        else snprintf(item.title,sizeof item.title,"A title number %d that is far too long for one line of a card, however large the card is drawn on the screen of the console",i);
        snprintf(item.subtitle,sizeof item.subtitle,"A channel with a long name for card %d, with its views and its date of upload beside it",i);
        item.kind=EVO_MEDIA_VIDEO; staged.push_back(item);
    }
    auto open=[&](const char* parent) {
        host.m_stack={parent}; host.m_crumbs={"List"}; host.m_pos_stack.clear(); host.m_page=0; host.RequestPage(parent,0);
    };
    auto pump=[&]() {
        host.Tick(); host.m_context->Update();
        if(host.m_needs_initial_focus) { host.SeedFocus(); host.m_context->Update(); }
        host.UpdateSelectedPreview(); host.m_context->Update();
    };
    std::vector<uint32_t> plain((size_t)width*height), frame((size_t)width*height);
    auto render=[&](std::vector<uint32_t>& target) {
        pump();
        std::fill(target.begin(),target.end(),0xFF000000u);
        renderer.SetFramebuffer(target.data()); renderer.SetDimensions(width,height);
        host.m_context->Render();
    };
    auto key=[&](EvoRmlProviderHost::Key k) { host.HandleKey(k); pump(); };
    auto box=[](Rml::Element* e, float& x, float& y, float& w, float& h) {
        const Rml::Vector2f at=e->GetAbsoluteOffset(Rml::BoxArea::Border), size=e->GetBox().GetSize(Rml::BoxArea::Border);
        x=at.x; y=at.y; w=size.x; h=size.y;
    };
    auto cards=[&]() {
        Rml::ElementList all, shown; host.m_doc->QuerySelectorAll(all,".tube-card");
        for(Rml::Element* e : all) if(!e->GetAttribute<Rml::String>("rowid","").empty() && e->IsVisible(true)) shown.push_back(e);
        return shown;
    };
    auto layout=[]() { tube_ui_layout_t l; tube_ui_layout(tube_ui_prefs(),&l); return l; };
    auto absolute=[&]() { return (int)host.m_row_offset+host.FocusedRowIndex(); };
    /* The grid as the preferences describe it, all of it on screen. */
    auto check_grid=[&]() {
        const tube_ui_layout_t l=layout();
        const size_t page=(size_t)(l.columns*l.rows);
        assert(EVO_PROVIDER_PAGE_SIZE==page && host.m_row_offset%page==0);
        const size_t expected=std::min(page,host.m_all_rows.size()-host.m_row_offset);
        const Rml::ElementList shown=cards();
        if(host.m_model.rows.size()!=expected || shown.size()!=expected) {
            fprintf(stderr,"page of %zu: %zu rows, %zu cards\n",expected,host.m_model.rows.size(),shown.size()); assert(false);
        }
        float px,py,pw,ph,fx,fy,fw,fh,nx,ny,nw,nh; box(host.m_doc->GetElementById("tube-preview"),px,py,pw,ph);
        box(host.m_doc->GetElementById("tube-footer"),fx,fy,fw,fh);
        box(host.m_doc->GetElementById("tube-selected-overview"),nx,ny,nw,nh);
        /* The key line takes a second line above the first when one does not hold it (a large scale, a
         * long language): it stays on screen, at most two lines high, and clear of the preview's text. */
        const float dp=host.m_context->GetDensityIndependentPixelRatio();
        assert(fy+fh<=height+0.5f && fx+fw<=width+0.5f && fh<=56.0f*dp && ny+nh<=fy+0.5f && py+ph<=fy+fh+0.5f);
        assert(host.m_doc->GetElementById("tube-footer")->GetScrollHeight()<=host.m_doc->GetElementById("tube-footer")->GetClientHeight()+1.0f);
        std::set<int> lefts,tops;
        for(size_t i=0;i<shown.size();++i) {
            float x,y,w,h; box(shown[i],x,y,w,h);
            if(x<0 || y<0 || x+w>width+0.5f || y+h>py+1.0f) {
                fprintf(stderr,"scale %d grid %d: card %zu at %.1f,%.1f %.1fx%.1f leaves its grid (preview at %.1f)\n",
                    tube_ui_prefs()->ui_scale,tube_ui_prefs()->grid_scale,i,x,y,w,h,py); assert(false);
            }
            float x0,y0,w0,h0; box(shown[i%(size_t)l.columns],x0,y0,w0,h0);
            if(std::fabs(x-x0)>=0.5f) {                        /* card i stands in column i % columns */
                fprintf(stderr,"scale %d grid %d lines %d (%dx%d card %.4f): card %zu at %.1f, column at %.1f, width %.2f\n",tube_ui_prefs()->ui_scale,
                    tube_ui_prefs()->grid_scale,tube_ui_prefs()->title_lines,l.columns,l.rows,l.card,i,x,x0,w); assert(false);
            }
            lefts.insert((int)std::lround(x)); tops.insert((int)std::lround(y));
        }
        assert(lefts.size()==std::min<size_t>((size_t)l.columns,expected) && tops.size()==(expected+(size_t)l.columns-1)/(size_t)l.columns);
    };
    auto lit=[&](const std::vector<uint32_t>& target,int x0,int y0,int x1,int y1) {
        long count=0;
        for(int y=y0;y<y1;++y) for(int x=x0;x<x1;++x) {
            const uint32_t p=target[(size_t)y*width+x];
            count+=(p&255)>0x90 && (p>>8&255)>0x90 && (p>>16&255)>0x90;
        }
        return count;
    };

    /* Defaults: the document is left alone, eight cards in four columns. */
    open("history"); render(plain);
    assert(!host.m_tube_styled && tube_ui_plain(tube_ui_prefs()) && EVO_PROVIDER_PAGE_SIZE==8 && host.m_context->GetDensityIndependentPixelRatio()==1.0f);
    check_grid(); assert(host.FocusedRowIndex()==0 && lit(plain,0,0,width,height)>2000);
    /* The body paints the page ground of the theme. The mark is a decoded picture, two rings in the text colour,
     * around a core in the accent. */
    assert((plain[2*width+2]&0xffffffu)==0x06070cu || (plain[2*width+2]&0xffffffu)==0x0c0706u);
    uint32_t mark;
    { float x,y,w,h; box(host.m_doc->GetElementById("tube-mark-core"),x,y,w,h); mark=plain[(size_t)(y+h/2)*width+(size_t)(x+w/2)]; }
    assert((mark&0xffffffu)==0x475affu || (mark&0xffffffu)==0xff5a47u);
    const bool red_low=(mark&255)==0xff;
    { float x,y,w,h; box(host.m_doc->GetElementById("tube-mark"),x,y,w,h);
      const uint32_t ring=plain[(size_t)(y+h/2)*width+(size_t)(x+3)], gap=plain[(size_t)(y+2)*width+(size_t)(x+w/2+4)];
      assert((ring&255)>200 && (ring>>8&255)>200 && (ring>>16&255)>200);          /* nine o'clock: on the outer ring */
      assert((gap&255)<40 && (gap>>8&255)<40 && (gap>>16&255)<40); }               /* past twelve: the ring is open */
    auto rgb=[&](uint32_t p) { return red_low ? (p&255)<<16|(p&0xff00)|(p>>16&255) : p&0xffffffu; };

    /* Every interface scale with every video grid scale. */
    int rendered=0;
    for(int u=0;u<TUBE_UI_SCALES;++u) for(int g=0;g<TUBE_GRID_SCALES;++g) {
        choose("settings:ui:scale:"+std::to_string(u)); choose("settings:ui:grid:"+std::to_string(g)); pump();
        open("history"); pump();
        const tube_ui_layout_t l=layout(); const int page=l.columns*l.rows;
        assert(std::fabs(host.m_context->GetDensityIndependentPixelRatio()-(float)l.scale)<1e-4f);
        assert(host.m_tube_styled || tube_ui_plain(tube_ui_prefs()));
        check_grid(); assert(absolute()==0);
        if(l.columns>1) { key(EvoRmlProviderHost::KeyRight); assert(absolute()==1); key(EvoRmlProviderHost::KeyLeft); assert(absolute()==0); }
        /* Down row by row, then onto the next page in the same column, and back up. */
        if(l.columns>1) key(EvoRmlProviderHost::KeyRight);
        const int column=l.columns>1 ? 1 : 0;
        for(int row=1;row<l.rows;++row) {
            key(EvoRmlProviderHost::KeyDown);
            if(absolute()!=row*l.columns+column) {
                fprintf(stderr,"scale %d grid %d (%dx%d card %.3f): Down to row %d reached %d\n",g_tube_ui_scales[u],g_tube_grid_scales[g],l.columns,l.rows,l.card,row,absolute());
                for(Rml::Element* c : cards()) { float x,y,w,h; box(c,x,y,w,h); fprintf(stderr,"  %.1f,%.1f %.1fx%.1f\n",x,y,w,h); }
                assert(false);
            }
        }
        key(EvoRmlProviderHost::KeyDown);
        if(host.m_row_offset==0) {                              /* the press fetched rows that fill this page: nothing was skipped */
            assert(absolute()==(l.rows-1)*l.columns+column && page>24 && (int)host.m_all_rows.size()>page);
            key(EvoRmlProviderHost::KeyDown);
        }
        if((int)host.m_row_offset!=page || absolute()!=page+column) {
            fprintf(stderr,"scale %d grid %d (%dx%d): Down off the page reached offset %zu row %d of %zu\n",g_tube_ui_scales[u],g_tube_grid_scales[g],
                l.columns,l.rows,host.m_row_offset,absolute(),host.m_all_rows.size()); assert(false);
        }
        check_grid();
        key(EvoRmlProviderHost::KeyUp); assert(host.m_row_offset==0 && absolute()==(l.rows-1)*l.columns+column);
        /* L1 / R1 to the end: every row is shown once, more rows are fetched as the pages need them. */
        std::vector<int> seen(kTotal,0); int guard=0;
        for(;;) {
            assert(++guard<200); check_grid();
            for(size_t i=0;i<host.m_model.rows.size();++i) ++seen[host.m_row_offset+i];
            const size_t before=host.m_row_offset, loaded=host.m_all_rows.size(), shown=host.m_model.rows.size();
            key(EvoRmlProviderHost::KeyPageDown);
            if(host.m_row_offset==before && host.m_all_rows.size()==loaded) break;
            if(host.m_row_offset==before) for(size_t i=0;i<shown;++i) --seen[host.m_row_offset+i]; /* the same page, now fuller */
        }
        assert((int)host.m_all_rows.size()==kTotal && (int)host.m_row_offset==(kTotal-1)/page*page);
        for(int i=0;i<kTotal;++i) assert(seen[i]==1);
        key(EvoRmlProviderHost::KeyPageUp); assert((int)host.m_row_offset==(kTotal-1)/page*page-page || page>=kTotal);
        /* A frame for each scale on its own and for the four corners of the table. */
        const bool corner=(u==0 || u==TUBE_UI_SCALES-1) && (g==0 || g==TUBE_GRID_SCALES-1);
        if(g_tube_ui_scales[u]==100 || g_tube_grid_scales[g]==100 || corner) {
            open("history"); render(frame); ++rendered;
            assert(lit(frame,0,0,width,height)>1500);
            float x,y,w,h; box(cards()[0],x,y,w,h);            /* the focused card wears its white border */
            assert(lit(frame,(int)x,(int)y,(int)(x+w),(int)(y+h))>100);
            box(host.m_doc->GetElementById("tube-footer"),x,y,w,h);
            assert(lit(frame,0,(int)y,width,std::min(height,(int)(y+h)))>50); /* the hint bar is drawn at the bottom */
            assert(rgb(frame[2*width+2])==g_tube_schemes[0].background);     /* the page ground reaches the corner at every scale */
        }
    }
    assert(rendered==TUBE_UI_SCALES+TUBE_GRID_SCALES-1+4);

    /* A position saved under one page size names the same row under another. */
    choose("settings:ui:scale:10"); choose("settings:ui:grid:5"); open("history"); pump();
    key(EvoRmlProviderHost::KeyPageDown); key(EvoRmlProviderHost::KeyDown); key(EvoRmlProviderHost::KeyRight);
    assert(host.m_row_offset==8 && absolute()==13);
    choose("settings:ui:grid:0"); pump();                       /* restyled in place: the focused row stays focused */
    assert(EVO_PROVIDER_PAGE_SIZE==32 && host.m_row_offset==0 && absolute()==13); check_grid();
    choose("settings:ui:grid:5"); pump(); assert(host.m_row_offset==8 && absolute()==13);
    host.m_stack={"history","video13"}; host.m_crumbs={"List","Inside"}; host.m_pos_stack={{8,5}};
    choose("settings:ui:scale:6"); pump();                      /* 0.8x: ten cards on a page */
    assert(EVO_PROVIDER_PAGE_SIZE==10);
    key(EvoRmlProviderHost::KeyBack); assert(host.m_stack.size()==1 && host.m_row_offset==10 && absolute()==13); check_grid();

    /* Back at the defaults the frame is the one drawn before anything changed. */
    choose("settings:ui:scale:10"); open("history"); render(frame);
    assert(tube_ui_plain(tube_ui_prefs()) && host.m_tube_styled && EVO_PROVIDER_PAGE_SIZE==8 && host.m_context->GetDensityIndependentPixelRatio()==1.0f);
    assert(frame==plain);

    /* Lists (comments, chat, review) page by the rows their column holds. */
    for(int u : {10, 6, 18, 0}) {
        choose("settings:ui:scale:"+std::to_string(u)); open("discussion:video00"); pump();
        const tube_ui_layout_t l=layout();
        assert(host.m_model.discussion && (int)EVO_PROVIDER_PAGE_SIZE==l.list_rows && (int)host.m_model.rows.size()==std::min(l.list_rows,24));
        assert(absolute()==0);                                  /* a restyle does not carry the last list's focus over */
        float gx,gy,gw,gh,fx,fy,fw,fh; box(host.m_doc->GetElementById("tube-grid"),gx,gy,gw,gh);
        box(host.m_doc->GetElementById("tube-discussion-footer"),fx,fy,fw,fh);
        assert(gy+gh<=fy+0.5f && fy+fh<=height+0.5f);
        for(Rml::Element* card : cards()) { float x,y,w,h; box(card,x,y,w,h); assert(y>=gy-0.5f && y+h<=gy+gh+0.5f && x+w<=width); }
        float tx,ty,tw,th; box(host.m_doc->GetElementById("tube-discussion-text"),tx,ty,tw,th);
        assert(tx+tw<=width+0.5f && ty+th<=fy+0.5f && tw>100 && th>100);
        /* One column at every scale, the unscaled eight rows included: Down walks every row, then turns
         * the page; Up walks back the same way. (Down from the fifth row used to turn the page.) */
        for(int row=1;row<l.list_rows;++row) {
            key(EvoRmlProviderHost::KeyDown);
            if(absolute()!=row) { fprintf(stderr,"scale index %d: list Down to row %d of %d reached %d (offset %zu, %zu rows)\n",u,row,l.list_rows,absolute(),host.m_row_offset,host.m_model.rows.size()); assert(false); }
        }
        key(EvoRmlProviderHost::KeyDown); assert((int)host.m_row_offset==l.list_rows && absolute()==l.list_rows);
        key(EvoRmlProviderHost::KeyUp); assert(host.m_row_offset==0 && absolute()==l.list_rows-1);
        for(int row=l.list_rows-2;row>=0;--row) { key(EvoRmlProviderHost::KeyUp); assert(host.m_row_offset==0 && absolute()==row); }
        key(EvoRmlProviderHost::KeyUp); assert(host.m_row_offset==0 && absolute()==0);        /* the first row of the first page stays */
    }
    choose("settings:ui:scale:10");

    /* Card text: more title lines and a second subtitle line keep 4 x 2 on screen. */
    open("history"); pump();
    Rml::Element* title=cards()[0]->QuerySelector(".tube-card-title"); Rml::Element* author=cards()[0]->QuerySelector(".tube-card-author");
    const float one_line=title->GetBox().GetSize(Rml::BoxArea::Border).y, one_author=author->GetBox().GetSize(Rml::BoxArea::Content).y;
    assert(std::fabs(one_line-40.0f)<0.5f);
    for(int lines=2;lines<=4;++lines) {
        choose("settings:ui:title-lines:"+std::to_string(lines-1)); open("history"); pump();
        const tube_ui_layout_t l=layout(); check_grid();
        assert(l.columns==4 && l.rows==2 && EVO_PROVIDER_PAGE_SIZE==8);
        title=cards()[0]->QuerySelector(".tube-card-title");
        assert(std::fabs(title->GetBox().GetSize(Rml::BoxArea::Border).y-(float)((12+28*lines)*l.card))<1.0f);
        assert(title->GetScrollWidth()<=title->GetClientWidth()+1.0f);     /* the long title wraps instead of running off */
    }
    choose("settings:ui:subtitle-lines:1"); open("history"); pump(); check_grid();
    author=cards()[0]->QuerySelector(".tube-card-author");
    assert(author->GetBox().GetSize(Rml::BoxArea::Content).y>one_author*1.5f*(float)layout().card);
    render(frame); assert(lit(frame,0,0,width,height)>2000);
    choose("settings:ui:title-lines:0"); choose("settings:ui:subtitle-lines:0");
    /* Square corners: the corner pixel of the focused card's border is lit. */
    open("history"); render(frame);
    { float x,y,w,h; box(cards()[0],x,y,w,h); assert(lit(frame,(int)x,(int)y,(int)x+2,(int)y+2)==0); }
    choose("settings:ui:rounded:0"); open("history"); render(frame);
    { float x,y,w,h; box(cards()[0],x,y,w,h); assert(lit(frame,(int)x,(int)y,(int)x+2,(int)y+2)==4); check_grid(); }
    choose("settings:ui:rounded:1"); open("history"); render(frame); assert(frame==plain);

    /* The title marquee: the focused card's long title travels to its end and starts over. */
    open("history"); system.now=50; pump();
    title=cards()[0]->QuerySelector(".tube-card-title");
    const float overflow=title->GetScrollWidth()-title->GetClientWidth(); assert(overflow>200);
    system.now=55; pump(); assert(title->GetScrollLeft()==0);                 /* off by default */
    choose("settings:ui:title-scroll:1"); system.now=60; host.ClearFrameFlag(); pump();
    assert(title->GetScrollLeft()==0);
    system.now=61; host.ClearFrameFlag(); pump(); assert(title->GetScrollLeft()==0);        /* rests first */
    system.now=62.2; host.ClearFrameFlag(); pump();
    assert(std::fabs(title->GetScrollLeft()-40.0f)<1.0f && host.m_dirty);             /* 40 dp a second, and asks for the frame */
    render(frame); assert(frame!=plain);
    host.ClearFrameFlag(); host.Tick(); assert(!host.m_dirty);                            /* nothing moved: no frame asked for */
    choose("settings:ui:scroll-speed:2"); system.now=62.2; pump();                    /* 2x; the change starts it over */
    system.now=62.2+1.2+1.0; pump(); assert(std::fabs(title->GetScrollLeft()-80.0f)<1.0f);
    system.now=62.2+1.2+overflow/80.0+0.5; pump(); assert(std::fabs(title->GetScrollLeft()-overflow)<1.0f);
    key(EvoRmlProviderHost::KeyRight);                                               /* focus leaves: the title returns */
    assert(cards()[0]->QuerySelector(".tube-card-title")->GetScrollLeft()==0);
    system.now+=30; pump(); assert(cards()[1]->QuerySelector(".tube-card-title")->GetScrollLeft()==0); /* a short title stays */
    key(EvoRmlProviderHost::KeyLeft); system.now+=2.2; pump();
    assert(cards()[0]->QuerySelector(".tube-card-title")->GetScrollLeft()>10);
    choose("settings:ui:title-scroll:0"); pump(); assert(cards()[0]->QuerySelector(".tube-card-title")->GetScrollLeft()==0);
    choose("settings:ui:scroll-speed:0");

    /* Themes and colour schemes: the page takes the background of each, true black and Daybreak's light ground included, and the accent its colour. */
    for(int scheme=1;scheme<TUBE_UI_SCHEMES;++scheme) {
        choose("settings:ui:scheme:"+std::to_string(scheme)); open("history"); render(frame);
        assert(rgb(frame[2*width+2])==g_tube_schemes[scheme].background);
        float x,y,w,h; box(host.m_doc->GetElementById("tube-mark-core"),x,y,w,h);
        assert(rgb(frame[(size_t)(y+h/2)*width+(size_t)(x+w/2)])==g_tube_schemes[scheme].accent);
        check_grid(); assert(EVO_PROVIDER_PAGE_SIZE==8 && lit(frame,0,0,width,height)>2000);
    }
    choose("settings:ui:scheme:0"); open("history"); render(frame); assert(frame==plain);

    host.m_doc->Close(); Rml::RemoveContext("ui-prefs"); host.m_context=nullptr; host.m_doc=nullptr; host.m_model_handle={};
    Rml::Shutdown(); evo_provider_youtube.shutdown(); unlink(evo_data_path("youtube.json")); rmdir(directory);
    puts("PASS: every interface and grid scale keeps cards, paging, D-pad and frame correct; card text, marquee, schemes and the default frame");
}
