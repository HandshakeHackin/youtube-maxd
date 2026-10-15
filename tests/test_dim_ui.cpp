// Screen dimming through the real provider host, the shipping browser layout,
// pinned RmlUi and the CPU rasteriser: the rendered frame darkens by the
// chosen amount over every part of the screen and returns on input.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include <string>
#include <vector>
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

static std::vector<evo_provider_item_t> staged;
static int staged_catalog(const char*, int, evo_provider_items_cb cb, void* ud) {
    cb(1, staged.data(), (int)staged.size(), 0, ud); return 0;
}

int main(int argc,char**argv) {
    assert(argc==3); std::filesystem::path source(argv[1]),fonts(argv[2]);
    char directory[]="/tmp/prosperotube-dim-ui-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    assert(!evo_provider_youtube.init());
    evo_provider_t provider = evo_provider_youtube; provider.list_catalog = staged_catalog;
    const int width=1920, height=1080;
    EvoSystemInterface system; EvoRenderInterface renderer(width,height);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts/"Lato-Regular.ttf").string())); assert(Rml::LoadFontFace((fonts/"Lato-Bold.ttf").string()));
    EvoRmlProviderHost host; host.m_context=Rml::CreateContext("dim-ui",{width,height}); assert(host.m_context);
    host.m_provider_id="youtube"; host.m_provider=&provider;
    host.m_model_name="youtube"; assert(host.RegisterDataModel());
    host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    for(int i=0;i<12;++i) {
        evo_provider_item_t item; memset(&item,0,sizeof item);
        snprintf(item.id,sizeof item.id,"settings:row%d",i); snprintf(item.title,sizeof item.title,"A bright row of text %d",i);
        snprintf(item.subtitle,sizeof item.subtitle,"Detail"); item.is_folder=1; item.kind=EVO_MEDIA_VIDEO; staged.push_back(item);
    }
    host.m_stack={"settings"}; host.m_crumbs={"Preferences"}; host.RequestPage("settings",0);
    std::vector<uint32_t> plain((size_t)width*height), frame((size_t)width*height);
    auto render=[&](std::vector<uint32_t>& target) {
        host.Tick(); host.m_context->Update();
        if(host.m_needs_initial_focus) { host.SeedFocus(); host.m_context->Update(); }
        host.UpdateSelectedPreview(); host.m_context->Update();
        std::fill(target.begin(),target.end(),0xFF000000u);
        renderer.SetFramebuffer(target.data()); renderer.SetDimensions(width,height);
        host.m_context->Render();
    };
    auto channel=[](uint32_t pixel,int shift) { return (int)(pixel>>shift&255); };
    Rml::Element* dim=host.m_doc->GetElementById("tube-dim"); assert(dim);

    /* Defaults: nothing dims, however long the screen is left alone. */
    tube_extra_dim_reset(0);
    assert(!tube_extra_dim_tick(100000000,TUBE_DIM_BROWSING));
    host.ClearFrameFlag(); render(plain);
    assert(!dim->IsVisible(true) && !dim->IsClassSet("shown"));
    long bright=0;
    for(uint32_t pixel : plain) bright+=channel(pixel,0)>0x90 && channel(pixel,8)>0x90 && channel(pixel,16)>0x90;
    assert(bright>2000);                                   /* there is text to darken */

    /* Half strength after the timeout: every pixel keeps half of each colour. */
    assert(tube_extra_set_dim_timeout(5) && tube_extra_set_dim_amount(50));
    tube_extra_dim_reset(1000);
    assert(!tube_extra_dim_tick(5999,TUBE_DIM_BROWSING) && tube_extra_dim_tick(6000,TUBE_DIM_BROWSING)==50);
    host.ClearFrameFlag(); render(frame);
    assert(host.NeedsFrame());                             /* the change asks for a redraw by itself */
    assert(dim->IsVisible(true) && dim->GetAbsoluteOffset(Rml::BoxArea::Border)==Rml::Vector2f(0,0));
    assert(dim->GetBox().GetSize(Rml::BoxArea::Border)==Rml::Vector2f((float)width,(float)height));
    for(size_t i=0;i<plain.size();i+=7)
        for(int shift=0;shift<24;shift+=8) {
            const int before=channel(plain[i],shift), after=channel(frame[i],shift);
            if(std::abs(after-before/2)>3) { fprintf(stderr,"pixel %zu channel %d: %d -> %d\n",i,shift,before,after); return 1; }
        }
    /* Unchanged level: no further redraw is asked for. */
    host.ClearFrameFlag(); host.Tick(); assert(!host.m_dirty);

    /* Full strength covers the whole screen, corners and text alike. */
    assert(tube_extra_set_dim_amount(100) && tube_extra_dim_tick(6001,TUBE_DIM_BROWSING)==100);
    render(frame);
    for(size_t i=0;i<frame.size();++i) if(frame[i]&0x00ffffffu) { fprintf(stderr,"pixel %zu stays lit: %08x\n",i,frame[i]); return 1; }

    /* Any input brings the picture back exactly as it was. */
    assert(tube_extra_dim_input(6100,1,0)==1 && !tube_extra_dim_tick(6100,TUBE_DIM_BROWSING));
    host.ClearFrameFlag(); render(frame);
    assert(host.NeedsFrame() && !dim->IsVisible(true) && frame==plain);

    host.m_doc->Close(); Rml::RemoveContext("dim-ui"); host.m_context=nullptr; host.m_doc=nullptr; host.m_model_handle={};
    Rml::Shutdown(); evo_provider_youtube.shutdown(); unlink(evo_data_path("youtube.json")); rmdir(directory);
    puts("PASS: the browser screen dims by the chosen amount over the whole frame and returns on input");
}

extern "C" int evo_net_callback_error(void) { return 0; }
