// Real generated provider callbacks/focus with the shipping RML and C reader.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cassert>
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
#include "cJSON.h"
#include "evo_data_path.h"
}
const EvoRmlBundleFile* evo_rmlui_bundle_find(const std::string&) { return nullptr; }
static long long clock_ms=100000;
extern "C" int tube_test_clock_gettime(clockid_t id, struct timespec* t) {
    assert(id==CLOCK_MONOTONIC); t->tv_sec=clock_ms/1000; t->tv_nsec=clock_ms%1000*1000000; return 0;
}
struct Pending { std::string method,url,body; evo_net_cb callback; void* data; };
static std::vector<Pending> pending;
extern "C" int evo_net_request_async(const char* method,const char* url,const char* body,const char**,int,evo_net_cb callback,void* data) {
    pending.push_back({method,url,body?body:"",callback,data}); return 0;
}
extern "C" void evo_error_set(const char*, ...) {}
extern "C" int evo_failure_reason(char*, size_t) { return 0; }
extern "C" int evo_provider_art_ready(const char*) { return 0; }
extern "C" int evo_provider_art_request(const char*,const char*,char*,size_t) { return -1; }
static void reply(const char* value) { assert(!pending.empty()); auto p=pending.front(); pending.erase(pending.begin()); p.callback(1,200,value,strlen(value)+1,p.data); }
static void frame(int first, const char* mode, const char* cursor) {
    auto root=cJSON_CreateObject(); auto rows=cJSON_CreateArray();
    cJSON_AddNumberToObject(root,"api_version",1); cJSON_AddStringToObject(root,"kind","live_chat");
    cJSON_AddStringToObject(root,"id","abcdefghijk"); cJSON_AddStringToObject(root,"chat_mode",mode);
    cJSON_AddStringToObject(root,"cursor",cursor); cJSON_AddBoolToObject(root,"has_more",0);
    cJSON_AddBoolToObject(root,"is_replay",0); cJSON_AddBoolToObject(root,"ended",0); cJSON_AddNumberToObject(root,"poll_after_ms",2000);
    cJSON_AddItemToObject(root,"items",rows);
    for(int i=first;i<first+24;++i) {
        auto row=cJSON_CreateObject(); std::string id="message-"+std::to_string(i), body="Literal <b id='chat-injection'>text</b>.\n";
        for(int line=0;line<100;++line) body += "Readable message line "+std::to_string(line)+"\n";
        cJSON_AddStringToObject(row,"id",id.c_str()); cJSON_AddStringToObject(row,"author","Viewer");
        cJSON_AddStringToObject(row,"content",body.c_str()); cJSON_AddStringToObject(row,"published","now"); cJSON_AddItemToArray(rows,row);
    }
    char* text=cJSON_PrintUnformatted(root); reply(text); free(text); cJSON_Delete(root);
}
int main(int argc,char**argv) {
    assert(argc==3); std::filesystem::path source(argv[1]),fonts(argv[2]);
    char directory[]="/tmp/prosperotube-chat-ui-XXXXXX"; assert(mkdtemp(directory)); assert(!setenv("EVO_DATA_DIR_OVERRIDE",directory,1));
    assert(!evo_provider_youtube.init()); assert(!evo_provider_youtube.set_source("http://127.0.0.1:8765#123456"));
    evo_provider_youtube.list_catalog("library:profile:one",0,
        [](int ok,const evo_provider_item_t*,int,int,void*) { assert(ok); },nullptr);
    EvoSystemInterface system; EvoRenderInterface renderer(1920,1080);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer); assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts/"Lato-Regular.ttf").string())); assert(Rml::LoadFontFace((fonts/"Lato-Bold.ttf").string()));
    EvoRmlProviderHost host; host.m_context=Rml::CreateContext("chat-ui",{1920,1080}); assert(host.m_context);
    host.m_provider_id="youtube"; host.m_provider=&evo_provider_youtube;
    auto constructor=host.m_context->CreateDataModel("youtube"); auto row=constructor.RegisterStruct<EvoProviderRow>();
#define ROW(name) row.RegisterMember(#name,&EvoProviderRow::name)
    ROW(id);ROW(title);ROW(subtitle);ROW(art);ROW(initial);ROW(duration);ROW(is_folder);ROW(is_live);ROW(progress);ROW(progress_w);ROW(played);
#undef ROW
    constructor.RegisterArray<std::vector<EvoProviderRow>>(); auto& model=host.m_model;
#define BIND(name) constructor.Bind(#name,&model.name)
    BIND(provider_name);BIND(breadcrumb);BIND(page_info);BIND(status);BIND(has_error);BIND(loading);BIND(empty);BIND(rows);
    BIND(has_selected);BIND(selected_title);BIND(selected_subtitle);BIND(selected_resume);BIND(selected_overview);
    BIND(discussion);BIND(chat);BIND(review);BIND(auth);BIND(count);BIND(in_folder);BIND(is_folder_level);BIND(has_multiple_pages);
#undef BIND
    constructor.BindEventCallback("activate",[](Rml::DataModelHandle,Rml::Event&,const Rml::VariantList&){});
    constructor.BindEventCallback("setup_url",[](Rml::DataModelHandle,Rml::Event&,const Rml::VariantList&){});
    host.m_model_handle=constructor.GetModelHandle(); host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
    host.m_stack.push_back("chat:top:abcdefghijk");host.m_crumbs.push_back("Top chat"); host.RequestPage(host.m_stack.back().c_str(),0);
    reply("{\"token\":\"abcdefghijklmnopqrstuvwxabcdefghijkl\"}"); frame(0,"top","ui-cap-one");
    auto pump=[&] {
        host.m_context->Update();
        if(host.m_needs_initial_focus) { host.SeedFocus();host.m_context->Update(); }
        if(host.m_chat_restore_scroll) {
            host.UpdateSelectedPreview();host.m_context->Update();
            if(model.selected_id==host.m_chat_restore_id.c_str()) host.m_doc->GetElementById("tube-discussion-text")->SetScrollTop(host.m_chat_scroll);
            host.m_context->Update();host.m_chat_restore_scroll=false;
        }
        host.UpdateSelectedPreview();host.m_context->Update();
    };
    pump(); assert(model.chat && model.discussion && host.m_all_rows.size()==24);
    assert(host.m_doc->GetElementById("tube-chat-footer")->IsVisible());
    assert(host.FocusPublishedRow(2));host.UpdateSelectedPreview();host.m_context->Update();
    auto reader=host.m_doc->GetElementById("tube-discussion-text");assert(reader && reader->GetScrollHeight()>reader->GetClientHeight());
    reader->SetScrollTop(400);host.m_context->Update();
    clock_ms+=2000;host.RefreshChat();assert(pending.size()==1 && host.m_chat_refresh && !model.loading);
    /* Move while the HTTP update is pending: preserve completion-time focus. */
    assert(host.FocusPublishedRow(3));host.UpdateSelectedPreview();host.m_context->Update(); reader->SetScrollTop(420);host.m_context->Update();
    std::string selected=model.selected_id.c_str();float scroll=reader->GetScrollTop();assert(scroll>0);
    frame(1,"top","ui-cap-two");pump();
    assert(model.selected_id==selected.c_str() && host.GetFocusedRow()->id==selected.c_str());
    assert(std::abs(reader->GetScrollTop()-scroll)<1 && !host.m_doc->GetElementById("chat-injection"));
    assert(host.m_all_rows.size()==24 && !host.m_chat_refresh && !model.loading);
    clock_ms+=2000;host.RefreshChat();frame(4,"top","ui-cap-three");pump();
    assert(model.selected_id!=selected.c_str() && host.GetFocusedRow() && reader->GetScrollTop()==0);
    assert(host.DiscussionKey(0)==1 && host.m_stack.back()=="chat:live:abcdefghijk");
    assert(pending.size()==2 && pending[0].body.find("chat_close")!=std::string::npos);reply("{}");frame(4,"live","ui-cap-live");pump();
    assert(model.chat && host.m_all_rows.size()==24);
    assert(host.DiscussionKey(5)==1 && host.m_stack.back()=="message:compose");pump();
    assert(!model.chat && pending.empty());
    tube_chat_prompt_t prompt; assert(tube_take_chat_prompt(&prompt));
    char review[80]; assert(!tube_submit_chat_message(prompt.ticket,"Literal <b id='send-injection'>draft</b> & café",review,sizeof review));
    host.ReplaceFolder(review,"Review chat message");pump();
    assert(model.review && !model.chat && host.GetFocusedRow());
    assert(std::string(tube_comment_text(host.GetFocusedRow()->id.c_str())).find("Literal <b")!=std::string::npos);
    assert(!host.m_doc->GetElementById("send-injection") && pending.empty());
    host.ShowSetupScreen();assert(!model.chat && !model.discussion && !pending.empty());reply("{}");
    assert(pending.empty() && !tube_chat_due("chat:live:abcdefghijk"));
    host.m_doc->Close();Rml::RemoveContext("chat-ui");host.m_context=nullptr;host.m_doc=nullptr;host.m_model_handle={};
    Rml::Shutdown();evo_provider_youtube.shutdown();unlink(evo_data_path("youtube.json"));rmdir(directory);
    puts("PASS: actual provider callbacks preserve current focus/scroll across polls, handle removal/mode/setup, and bind literal shipping chat RML");
}
