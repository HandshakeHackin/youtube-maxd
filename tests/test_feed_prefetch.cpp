// Classic feeds read ahead, through the real provider host and the shipping
// browser document with pinned RmlUi: the next page is asked for once the focus
// is within two rows of the last row that arrived, in the background (no
// loading note), the page and focused card stay put when it lands, a Down at
// the end moves at once (or as soon as the page on its way lands, with no
// second request), the real end asks for nothing more, and a page that brings
// nothing stops the reading ahead. Answers are delivered later, as the network does.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
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
extern "C" int evo_net_callback_error(void) { return 0; }

struct TestSystem : EvoSystemInterface { double now = 0; double GetElapsedTime() override { return now; } };

static std::vector<evo_provider_item_t> staged;
static bool empty_tail;   /* every page after the first is empty but says more follow (an exhausted token) */
struct Pending { int page; evo_provider_items_cb cb; void* ud; };
static std::vector<Pending> pending;
static std::vector<int> asked;
static int deferred_catalog(const char*, int page, evo_provider_items_cb cb, void* ud) {
    asked.push_back(page); pending.push_back({page, cb, ud}); return 0;
}
static void deliver() {
    std::vector<Pending> now; now.swap(pending);
    for (const Pending& p : now) {
        const size_t offset = (size_t)p.page * 24;
        size_t count = offset < staged.size() ? std::min<size_t>(24, staged.size() - offset) : 0;
        bool more = offset + count < staged.size();
        if (empty_tail && p.page > 0) { count = 0; more = true; }
        p.cb(1, count ? staged.data() + offset : nullptr, (int)count, more, p.ud);
    }
}

int main(int argc, char** argv) {
    assert(argc==3); std::filesystem::path source(argv[1]), fonts(argv[2]);
    char directory[]="/tmp/prosperotube-feed-prefetch-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    assert(!evo_provider_youtube.init());
    evo_provider_t provider = evo_provider_youtube; provider.list_catalog = deferred_catalog;
    const int width=1920, height=1080;
    TestSystem system; EvoRenderInterface renderer(width,height);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts/"Lato-Regular.ttf").string())); assert(Rml::LoadFontFace((fonts/"Lato-Bold.ttf").string()));
    EvoRmlProviderHost host; host.m_context=Rml::CreateContext("feed-prefetch",{width,height}); assert(host.m_context);
    host.m_provider_id="youtube"; host.m_provider=&provider;
    host.m_model_name="youtube"; assert(host.RegisterDataModel());
    host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    for (int i=0;i<60;++i) {
        evo_provider_item_t item; memset(&item,0,sizeof item);
        snprintf(item.id,sizeof item.id,"video%02d",i); snprintf(item.title,sizeof item.title,"Video %d",i);
        item.kind=EVO_MEDIA_VIDEO; staged.push_back(item);
    }
    auto pump=[&]() {
        for (int i=0;i<3;++i) {
            host.Tick(); host.m_context->Update();
            if (host.m_needs_initial_focus) { host.SeedFocus(); host.m_context->Update(); }
            host.UpdateSelectedPreview(); host.m_context->Update();
        }
    };
    auto key=[&](EvoRmlProviderHost::Key k) { host.HandleKey(k); pump(); };
    auto absolute=[&]() { return (int)host.m_row_offset+host.FocusedRowIndex(); };
    auto open=[&](const char* parent) {
        host.m_stack={parent}; host.m_crumbs={"Subscriptions"}; host.m_pos_stack.clear(); host.m_page=0; host.RequestPage(parent,0);
        deliver(); pump();
    };
    assert(EVO_PROVIDER_PAGE_SIZE==8);                       /* the default grid: four columns, two rows */

    open("library:subscriptions");
    assert(host.m_all_rows.size()==24 && asked==std::vector<int>({0}) && absolute()==0);
    key(EvoRmlProviderHost::KeyPageDown); assert(host.m_row_offset==8 && pending.empty());   /* far from the end: nothing asked */
    key(EvoRmlProviderHost::KeyPageDown); assert(host.m_row_offset==16 && absolute()==16);
    /* Within two rows of the last row that arrived: page 1 is on its way, with no loading note. */
    if (pending.size()!=1 || pending[0].page!=1) { fprintf(stderr,"no read-ahead at row %d of %zu (%zu asked)\n",absolute(),host.m_all_rows.size(),asked.size()); return 1; }
    assert(!host.m_model.loading);
    key(EvoRmlProviderHost::KeyRight); assert(absolute()==17);
    deliver(); pump();
    assert(host.m_all_rows.size()==48 && host.m_row_offset==16 && absolute()==17);              /* page and focus stay */
    key(EvoRmlProviderHost::KeyDown); assert(absolute()==21);
    key(EvoRmlProviderHost::KeyDown); assert(host.m_row_offset==24 && absolute()==25 && pending.empty()); /* moves at once */

    /* A Down at the end while the next page is on its way moves as soon as it lands, with one request. */
    key(EvoRmlProviderHost::KeyPageDown); key(EvoRmlProviderHost::KeyPageDown);
    assert(host.m_row_offset==40 && pending.size()==1 && pending[0].page==2);
    key(EvoRmlProviderHost::KeyDown); key(EvoRmlProviderHost::KeyDown);
    assert(pending.size()==1 && asked.size()==3);
    deliver(); pump();
    assert(host.m_all_rows.size()==60 && host.m_row_offset==48 && absolute()==49);
    /* The real end: nothing more is asked, however long the list stays up. */
    for (int i=0;i<4;++i) key(EvoRmlProviderHost::KeyPageDown);
    for (int i=0;i<20;++i) pump();
    assert(asked.size()==3 && pending.empty() && !host.m_has_more && host.m_row_offset==56);

    /* An empty page that still says "more" stops the reading ahead instead of asking every frame. */
    empty_tail=true; asked.clear();
    open("library:subscriptions");
    key(EvoRmlProviderHost::KeyPageDown); key(EvoRmlProviderHost::KeyPageDown);
    assert(pending.size()==1); deliver();
    for (int i=0;i<20;++i) pump();
    assert(asked==std::vector<int>({0,1}) && host.m_all_rows.size()==24 && absolute()==16);
    /* Opening the list again reads ahead again. */
    open("library:subscriptions"); key(EvoRmlProviderHost::KeyPageDown); key(EvoRmlProviderHost::KeyPageDown);
    assert(pending.size()==1 && pending[0].page==1);
    deliver(); pump();

    host.m_doc->Close(); Rml::Shutdown();
    std::filesystem::remove_all(directory);
    puts("PASS: Classic feeds read ahead: the next page in the background two rows before the end, focus and page kept, Down without waiting, one request, a clean end and a stop after an empty page");
    return 0;
}
