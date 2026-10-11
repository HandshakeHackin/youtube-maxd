// QR display through the real provider host, the shipping RML with the QR
// overlay applied, pinned RmlUi and the CPU rasteriser: the rendered frame is
// sampled module by module and compared with the encoded symbol.
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
#include "tube_about.h"

const EvoRmlBundleFile* evo_rmlui_bundle_find(const std::string&) { return nullptr; }
extern "C" int tube_test_clock_gettime(clockid_t, struct timespec* t) { t->tv_sec=100; t->tv_nsec=0; return 0; }
extern "C" int evo_net_request_async(const char*,const char*,const char*,const char**,int,evo_net_cb,void*) { return -1; }
extern "C" void evo_error_set(const char*, ...) {}
extern "C" int evo_failure_reason(char*, size_t) { return 0; }
extern "C" int evo_provider_art_ready(const char*) { return 0; }
extern "C" int evo_provider_art_request(const char*,const char*,char*,size_t) { return -1; }

/* The catalog under test: whatever rows the case stages, delivered at once. */
static std::vector<evo_provider_item_t> staged;
static int staged_catalog(const char*, int, evo_provider_items_cb cb, void* ud) {
    cb(1, staged.data(), (int)staged.size(), 0, ud); return 0;
}
static evo_provider_item_t row(const char* id, const char* title, const char* hint, const char* overview) {
    evo_provider_item_t item; memset(&item, 0, sizeof item);
    snprintf(item.id, sizeof item.id, "%s", id); snprintf(item.title, sizeof item.title, "%s", title);
    snprintf(item.subtitle, sizeof item.subtitle, "%s", hint); snprintf(item.overview, sizeof item.overview, "%s", overview);
    item.is_folder = 1; item.kind = EVO_MEDIA_VIDEO; return item;
}

int main(int argc,char**argv) {
    assert(argc==3); std::filesystem::path source(argv[1]),fonts(argv[2]);
    char directory[]="/tmp/prosperotube-qr-ui-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    assert(!evo_provider_youtube.init());
    evo_provider_t provider = evo_provider_youtube; provider.list_catalog = staged_catalog;
    const int width=1920, height=1080;
    EvoSystemInterface system; EvoRenderInterface renderer(width,height);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts/"Lato-Regular.ttf").string())); assert(Rml::LoadFontFace((fonts/"Lato-Bold.ttf").string()));
    EvoRmlProviderHost host; host.m_context=Rml::CreateContext("qr-ui",{width,height}); assert(host.m_context);
    host.m_provider_id="youtube"; host.m_provider=&provider;
    host.m_model_name="youtube"; assert(host.RegisterDataModel()); auto& model=host.m_model;
    host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    auto element=[&](const char* id) { auto found=host.m_doc->GetElementById(id); assert(found); return found; };
    auto pump=[&] {
        host.m_context->Update();
        if(host.m_needs_initial_focus) { host.SeedFocus(); host.m_context->Update(); }
        host.UpdateSelectedPreview(); host.m_context->Update();
    };
    auto show=[&](const char* parent, std::vector<evo_provider_item_t> rows) {
        staged=std::move(rows);
        host.m_stack={parent}; host.m_crumbs={"Page"}; host.m_pos_stack.clear(); host.m_page=0;
        host.RequestPage(parent,0); pump();
        assert(!model.loading && host.m_all_rows.size()==staged.size());
    };
    auto focus=[&](int index) { assert(host.FocusPublishedRow(index)); pump(); assert(host.GetFocusedRow() && host.GetFocusedRow()->index==index); };
    /* Render a frame and read the symbol back from the pixels, quiet zone included. */
    std::vector<uint32_t> frame((size_t)width*height);
    auto rendered_matches=[&](const char* payload) {
        tube_qr_code code; assert(tube_qr_encode_text(payload,&code)==0);
        const int module=tube_qr_module_dp(&code), side=tube_qr_square_dp(&code);
        auto symbol=element("tube-qr-symbol"); auto box=element("tube-qr-box");
        assert(element("tube-qr")->IsVisible(true) && symbol->IsVisible(true));
        const Rml::Vector2f origin=symbol->GetAbsoluteOffset(Rml::BoxArea::Border), size=symbol->GetBox().GetSize(Rml::BoxArea::Border);
        assert(std::abs(size.x-side)<0.01f && std::abs(size.y-side)<0.01f && origin.x==std::floor(origin.x) && origin.y==std::floor(origin.y));
        assert(box->GetAbsoluteOffset(Rml::BoxArea::Border)==origin && std::abs(box->GetBox().GetSize(Rml::BoxArea::Border).x-side)<0.01f);
        assert(origin.x>=0 && origin.y>=0 && origin.x+side<=width && origin.y+side<=height-48);
        std::fill(frame.begin(),frame.end(),0xFF808080u);
        renderer.SetFramebuffer(frame.data()); renderer.SetDimensions(width,height);
        host.m_context->Render();
        for(int y=-TUBE_QR_QUIET_MODULES;y<code.size+TUBE_QR_QUIET_MODULES;++y) for(int x=-TUBE_QR_QUIET_MODULES;x<code.size+TUBE_QR_QUIET_MODULES;++x) {
            /* Centre and the four inner corners of the module: sharp edges, no bleed. */
            for(int corner=0;corner<5;++corner) {
                const int inset=corner==4?module/2:1;
                const int px=(int)origin.x+(x+TUBE_QR_QUIET_MODULES)*module+(corner==4?inset:(corner&1?module-1-inset:inset));
                const int py=(int)origin.y+(y+TUBE_QR_QUIET_MODULES)*module+(corner==4?inset:(corner&2?module-1-inset:inset));
                const uint32_t pixel=frame[(size_t)py*width+px];
                const unsigned red=pixel&255, green=pixel>>8&255, blue=pixel>>16&255;
                const bool dark=red<0x20 && green<0x20 && blue<0x20, light=red>0xE0 && green>0xE0 && blue>0xE0;
                if(!(tube_qr_module(&code,x,y)?dark:light)) { fprintf(stderr,"module %d,%d of '%s' drawn as %08x\n",x,y,payload,pixel); return false; }
            }
        }
        return true;
    };

    /* Sign-in page: the code row yields the activation address. */
    show("auth:session", {row("auth:info:code","ABC-DEF-GHI","Enter this code on your phone or computer",""),
                          row("auth:info:url","https://yt.be/activate","Open this address to authorize YouTube",""),
                          row("auth:cancel","Cancel sign-in","","")});
    assert(model.auth && model.review && !model.link && model.qr && model.qr_text=="https://youtube.com/qr/activate/ABC-DEF-GHI");
    assert(element("tube-auth-footer")->IsVisible(true) && !element("tube-link-footer")->IsVisible(true) && !element("tube-review-footer")->IsVisible(true));
    assert(element("tube-discussion-text")->IsClassSet("with-qr") && rendered_matches("https://youtube.com/qr/activate/ABC-DEF-GHI"));
    /* Moving to a row without a link keeps the page's code, without rebuilding it. */
    auto before=element("tube-qr-symbol"); const int boxes=before->GetNumChildren(); assert(boxes>20);
    focus(1); assert(model.qr && element("tube-qr-symbol")==before && before->GetNumChildren()==boxes);
    focus(2); assert(model.qr && rendered_matches("https://youtube.com/qr/activate/ABC-DEF-GHI"));
    /* The automatic refresh brings a new code, then the signed-in state. */
    show("auth:session", {row("auth:info:code","XYZ-123-QRS","Enter this code on your phone or computer",""),
                          row("auth:info:url","https://yt.be/activate","Open this address to authorize YouTube","")});
    assert(model.qr && model.qr_text=="https://youtube.com/qr/activate/XYZ-123-QRS" && rendered_matches("https://youtube.com/qr/activate/XYZ-123-QRS"));
    show("auth:session", {row("auth:info","Signed in","Account credentials stay on the companion",""), row("auth:use","Use authorized YouTube account","","")});
    assert(model.auth && !model.qr && model.qr_text.empty() && !element("tube-qr")->IsVisible(true) && element("tube-qr-box")->GetNumChildren()==0);
    assert(!element("tube-discussion-text")->IsClassSet("with-qr"));
    show("auth:session", {row("auth:info:code","no code yet","","")});
    assert(!model.qr && !host.QrShown());

    /* Link page built from the About table: the focused link is shown. */
    std::vector<evo_provider_item_t> links;
    for(int i=0;i<tube_about_link_count();++i) {
        const tube_about_link* link=tube_about_link_at(i);
        links.push_back(row((std::string("link:about:")+link->id).c_str(),link->title,link->detail,link->url));
    }
    show("link:about", links);
    assert(model.link && model.review && !model.auth && !model.discussion && model.qr);
    assert(host.QrShown()); /* what keeps screen dimming away from a code being scanned */
    assert(element("tube-link-footer")->IsVisible(true) && !element("tube-review-footer")->IsVisible(true) && !element("tube-auth-footer")->IsVisible(true));
    for(int i=0;i<tube_about_link_count();++i) {
        focus(i);
        assert(model.qr_text==tube_about_link_at(i)->url && model.selected_overview==tube_about_link_at(i)->url);
        assert(rendered_matches(tube_about_link_at(i)->url));
    }
    /* Choosing a link explains what to do instead of opening a folder. */
    focus(0); host.m_pending_activation=host.GetFocusedRow()->id.c_str(); host.ApplyPendingActivation(); pump();
    assert(host.m_stack.size()==1 && host.m_stack.back()=="link:about" && model.status.find("Scan the code")!=std::string::npos && model.qr);

    /* Rows without a usable address fall back to the page's first link; markup stays text. */
    show("link:share", {row("link:note","About this link","","Opens the video on a phone"),
                        row("link:bad","Not a link","","https://example.org/<b id='qr-injection'>x</b>"),
                        row("link:video","Share video","","https://youtu.be/abcdefghijk?t=42"),
                        row("share:other","Other row","","https://example.org/ignored")});
    focus(0); assert(model.qr && model.qr_text=="https://youtu.be/abcdefghijk?t=42" && rendered_matches("https://youtu.be/abcdefghijk?t=42"));
    focus(1); assert(model.qr_text=="https://youtu.be/abcdefghijk?t=42" && !host.m_doc->GetElementById("qr-injection"));
    focus(3); assert(model.qr_text=="https://youtu.be/abcdefghijk?t=42");
    show("link:none", {row("link:note","Nothing to scan","","No address here")});
    assert(model.link && !model.qr && !element("tube-qr")->IsVisible(true));
    host.m_pending_activation="link:note"; host.ApplyPendingActivation(); pump();
    assert(host.m_stack.size()==1 && model.status.find("Type the address")!=std::string::npos);

    /* The largest supported symbol still fits the panel and reads back. */
    std::string longest="https://example.org/"; longest.resize(TUBE_QR_MAX_PAYLOAD,'q');
    show("link:long", {row("link:long","Long link","",longest.c_str())});
    assert(model.qr && model.qr_text==longest.c_str() && rendered_matches(longest.c_str()));
    assert(element("tube-qr-symbol")->GetNumChildren()<1000);

    /* Leaving for an ordinary page removes the code even before its rows arrive. */
    staged={row("settings:quality","Quality","","")};
    host.m_stack={"settings"}; host.m_crumbs={"Preferences"};
    provider.list_catalog=[](const char*, int, evo_provider_items_cb, void*) { return -1; };
    host.RequestPage("settings",0);
    assert(!model.link && !model.review && !model.qr && model.qr_text.empty());
    provider.list_catalog=staged_catalog;
    show("settings", staged);
    assert(!model.qr && !element("tube-qr")->IsVisible(true) && element("tube-qr-box")->GetNumChildren()==0);
    /* A search from a link page must not keep its code either. */
    show("link:about", links); assert(model.qr);
    model.query="cats"; host.UpdateSelectedPreview(); assert(!model.qr); model.query="";

    host.m_doc->Close(); Rml::RemoveContext("qr-ui"); host.m_context=nullptr; host.m_doc=nullptr; host.m_model_handle={};
    Rml::Shutdown(); evo_provider_youtube.shutdown(); unlink(evo_data_path("youtube.json")); rmdir(directory);
    puts("PASS: sign-in and link pages draw their QR code; rendered frames read back module for module");
}
