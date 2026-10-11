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
static void frame(int first, const char* mode, const char* cursor, int total=24, int replay_position=-1, int page=-1, bool more=false) {
    auto root=cJSON_CreateObject(); auto rows=cJSON_CreateArray();
    cJSON_AddNumberToObject(root,"api_version",1); cJSON_AddStringToObject(root,"kind","live_chat");
    cJSON_AddStringToObject(root,"id","abcdefghijk"); cJSON_AddStringToObject(root,"chat_mode",mode);
    cJSON_AddStringToObject(root,"cursor",cursor); cJSON_AddBoolToObject(root,"has_more",more);
    if(page>=0)cJSON_AddNumberToObject(root,"chat_page",page);
    cJSON_AddBoolToObject(root,"is_replay",replay_position>=0);
    if(replay_position>=0) cJSON_AddNumberToObject(root,"position_ms",replay_position);
    cJSON_AddBoolToObject(root,"ended",0); cJSON_AddNumberToObject(root,"poll_after_ms",2000);
    cJSON_AddItemToObject(root,"items",rows);
    for(int i=first;i<first+total;++i) {
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
    host.m_model_name="youtube";assert(host.RegisterDataModel());auto& model=host.m_model;
    host.m_doc=host.m_context->LoadDocument((source/"youtube.rml").string()); assert(host.m_doc); host.m_doc->Show();
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
    assert(host.GetFocusedRow()->id==host.m_all_rows.back().id);
    assert(host.m_doc->GetElementById("tube-chat-footer")->IsVisible());
    host.SetPageOffset(0,2);pump();
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
    clock_ms+=2000;host.RefreshChat();frame(4,"top","ui-cap-history",256);pump();
    assert(host.m_all_rows.size()==256);
    host.SetPageOffset(48,3);pump();
    selected=model.selected_id.c_str();reader->SetScrollTop(300);host.m_context->Update();scroll=reader->GetScrollTop();
    clock_ms+=2000;host.RefreshChat();frame(5,"top","ui-cap-history-next",256);pump();
    assert(host.GetFocusedRow()->id==selected.c_str() && model.selected_id==selected.c_str());
    assert(host.m_row_offset==48 && std::abs(reader->GetScrollTop()-scroll)<1);
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
    model.discussion=true;model.chat=false;host.m_stack.push_back("discussion:top:abcdefghijk");
    host.m_crumbs.push_back("Top comments");
    assert(host.DiscussionKey(6)==1 && host.m_stack.back()=="chat:top:abcdefghijk");
    assert(pending.size()==1 && pending[0].body.find("\"chat_position_sec\":0.000")!=std::string::npos);
    frame(0,"top","ui-replay-zero",24,0,0,true);pump();
    assert(host.DiscussionKey(6)==1 && pending.size()==1 && pending[0].body.find("\"chat_page\":1")!=std::string::npos);
    assert(host.DiscussionKey(6)==1 && pending.size()==1);
    frame(24,"top","ui-replay-page-one",24,0,1,false);pump();
    assert(host.DiscussionKey(6)==1 && pending.empty());
    assert(model.chat && host.m_all_rows.size()==24);
    clock_ms+=5000;host.RefreshChat();assert(pending.empty());
    assert(host.HandleKey(EvoRmlProviderHost::KeyPageDown));
    assert(pending.size()==1 && pending[0].body.find("\"chat_position_sec\":60.000")!=std::string::npos);
    assert(host.HandleKey(EvoRmlProviderHost::KeyPageDown));
    assert(pending.size()==1 && tube_chat_reader_position(host.m_stack.back().c_str())==60);
    assert(model.status.find("Wait for the replay update")!=std::string::npos);
    frame(60,"top","ui-replay-minute",24,60000);pump();
    assert(host.DiscussionKey(0)==1 && host.m_stack.back()=="chat:live:abcdefghijk");
    assert(pending.size()==2 && pending[0].body.find("chat_close")!=std::string::npos);
    reply("{}");assert(pending[0].body.find("\"chat_position_sec\":60.000")!=std::string::npos);
    frame(60,"live","ui-replay-minute",24,60000);pump();
    assert(host.HandleKey(EvoRmlProviderHost::KeyPageUp));
    assert(pending.size()==1 && pending[0].body.find("ui-replay-minute")!=std::string::npos);
    frame(0,"live","ui-replay-rewind",24,0);pump();
    host.ShowSetupScreen();assert(pending.size()==1);reply("{}");
    /* Settings choosers push a real folder; selection retains its parent level. */
    host.m_stack.push_back("settings"); host.m_crumbs.push_back("Preferences");
    host.RequestPage("settings",0);pump();assert(host.m_has_more);
    host.RequestPage("settings",1);pump();assert(!host.m_has_more);
    assert(std::any_of(host.m_all_rows.begin(),host.m_all_rows.end(),
        [](const EvoProviderRow& row){return row.id=="settings:clear-queue";}));
    for(int enabled=1;enabled>=0;--enabled) {
        host.RequestPage("settings",0);pump();
        const auto depth=host.m_stack.size();
        auto mirror=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),
            [](const EvoProviderRow& row){return row.id=="settings:video-mirror";});
        assert(mirror!=host.m_all_rows.end());
        auto index=static_cast<size_t>(mirror-host.m_all_rows.begin());
        host.SetPageOffset((index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,
                           static_cast<int>(index%EVO_PROVIDER_PAGE_SIZE));pump();
        host.m_pending_activation="settings:video-mirror";host.ApplyPendingActivation();pump();
        assert(tube_player_mirror_enabled()==enabled && host.m_stack.size()==depth);
        host.RequestPage("settings",1);pump();assert(tube_player_mirror_enabled()==enabled);
    }
    for (const auto& chooser : std::vector<std::pair<std::string,std::string>>{
            {"settings:player-ok","settings:player-ok:2"},
            {"settings:playback-mode","settings:playback-mode:4"},
            {"settings:player-hide","settings:player-hide:0"},
            {"settings:player-seek","settings:player-seek:2"},
            {"settings:seek-interval","settings:seek-interval:7"},
            {"settings:quality","settings:quality:240"},
            {"settings:frame-rate","settings:frame-rate:30"},
            {"settings:time-display","settings:time-display:2"},
            {"settings:video-zoom","settings:zoom:100"},
            {"settings:video-aspect","settings:aspect:10"},
            {"settings:video-rotate","settings:rotate:270"},
            {"settings:pixel-ratio","settings:pixel:5"},
            {"settings:sleep-timer","settings:sleep:30"}}) {
        host.RequestPage("settings",0); pump();
        const auto depth=host.m_stack.size();
        auto entry=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),
            [&](const EvoProviderRow& r) { return r.id==chooser.first.c_str(); });
        for(int page=1;entry==host.m_all_rows.end() && host.m_has_more && page<3;++page) {
            host.RequestPage("settings",page);pump();
            entry=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),
                [&](const EvoProviderRow& r) { return r.id==chooser.first.c_str(); });
        }
        assert(entry!=host.m_all_rows.end());
        const auto index=static_cast<size_t>(entry-host.m_all_rows.begin());
        host.SetPageOffset((index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,
                           static_cast<int>(index%EVO_PROVIDER_PAGE_SIZE)); pump();
        host.m_pending_activation=chooser.first; host.ApplyPendingActivation(); pump();
        assert(host.m_stack.size()==depth+1 && host.m_stack.back()==chooser.first);
        auto child=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),
            [&](const EvoProviderRow& r){return r.id==chooser.second.c_str();});
        assert(child!=host.m_all_rows.end());
        const auto child_index=static_cast<size_t>(child-host.m_all_rows.begin());
        host.SetPageOffset((child_index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,
                           static_cast<int>(child_index%EVO_PROVIDER_PAGE_SIZE));pump();
        host.m_pending_activation=chooser.second; host.ApplyPendingActivation(); pump();
        assert(host.m_stack.size()==depth+1 && host.m_stack.back()==chooser.first);
        host.m_stack.pop_back(); host.m_crumbs.pop_back(); host.m_pos_stack.pop_back();
    }
    assert(tube_player_ok_mode()==2 && tube_player_hide_ms()==0 && tube_sleep_minutes()==30);
    assert(tube_player_seek_mode()==2 && tube_player_seek_seconds()==7);
    assert(tube_player_zoom_percent()==100 && tube_player_aspect_mode()==10 && tube_player_pixel_mode()==5 && tube_player_rotation_degrees()==270);
    host.RequestPage("settings:video-zoom",0);pump();assert(host.m_has_more);
    host.RequestPage("settings:video-zoom",1);pump();assert(host.m_has_more);
    host.RequestPage("settings:video-zoom",2);pump();assert(!host.m_has_more);
    assert(std::any_of(host.m_all_rows.begin(),host.m_all_rows.end(),
        [](const EvoProviderRow& row){return row.id=="settings:zoom:300";}));
    assert(pending.empty());
    host.RequestPage("library:playlist:PLabcdefghijk",0);
    reply("{\"job\":\"playlist-ui\",\"state\":\"pending\"}");
    clock_ms+=300;tube_poll();
    reply("{\"state\":\"done\",\"result\":{\"items\":["
          "{\"id\":\"abcdefghijk\",\"title\":\"First occurrence\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":0}},"
          "{\"id\":\"abcdefghijk\",\"title\":\"Repeated occurrence\",\"playback_context\":{\"playlist\":\"PLabcdefghijk\",\"index\":9}}]}}");
    pump();assert(host.m_all_rows.size()==2);
    assert(model.rows[0].id==model.rows[1].id && model.rows[0].selection_key!=model.rows[1].selection_key);
    assert(host.FocusPublishedRow(1));pump();
    assert(host.GetFocusedRow() && host.GetFocusedRow()->playback_index==9);
    auto card=host.m_context->GetFocusElement();assert(card);
    card->DispatchEvent("click",Rml::Dictionary{});
    assert(host.m_pending_activation==model.rows[1].selection_key);
    host.ApplyPendingActivation();
    evo_provider_selection_t selection{};assert(evo_rmlui_provider_take_selection(&selection));
    assert(!strcmp(selection.item_id,"abcdefghijk") && selection.playback_index==9 && selection.playback_scope);
    assert(!strcmp(selection.playback_playlist,"PLabcdefghijk"));
    assert(pending.empty());
    host.m_stack={"favorites"};host.m_crumbs={"Favorites"};host.m_pos_stack.clear();
    host.Search("block fixture");
    const char* block_page="{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"abcdefghijk\",\"title\":\"Block this\",\"channel_id\":\"UCaaaaaaaaaaaaaaaaaaaaaa\"},{\"id\":\"bcdefghijkl\",\"title\":\"Keep this\",\"channel_id\":\"UCbbbbbbbbbbbbbbbbbbbbbb\"}]}}";
    reply("{\"job\":\"block-ui\",\"state\":\"pending\"}");clock_ms+=300;tube_poll();reply(block_page);pump();
    assert(host.FocusPublishedRow(0));pump();assert(host.DiscussionKey(4));pump();
    auto block_action=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),[](const EvoProviderRow& row){return row.title=="Block channel";});
    assert(block_action!=host.m_all_rows.end());const std::string block_id=block_action->id.c_str();
    size_t block_index=block_action-host.m_all_rows.begin();
    host.SetPageOffset((block_index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,block_index%EVO_PROVIDER_PAGE_SIZE);pump();
    host.m_pending_activation=block_id;host.ApplyPendingActivation();
    assert(host.m_stack.size()==1 && host.m_stack.back()=="favorites" && host.m_model.query=="block fixture");
    assert(pending.size()==1 && pending[0].body.find("block fixture")!=std::string::npos);
    reply("{\"job\":\"block-ui-refresh\",\"state\":\"pending\"}");clock_ms+=300;tube_poll();reply(block_page);pump();
    assert(host.m_all_rows.size()==1 && host.m_all_rows[0].id=="bcdefghijkl" && host.GetFocusedRow());
    host.m_model.query="";host.m_stack={"blocked-channels"};host.m_crumbs={"Blocked channels"};host.m_pos_stack.clear();
    host.RequestPage("blocked-channels",0);pump();assert(host.m_all_rows.size()==1);
    host.m_pending_activation=host.m_all_rows[0].id;host.ApplyPendingActivation();pump();
    assert(host.m_stack.size()==1 && host.m_stack.back()=="blocked-channels" && host.m_all_rows.empty() && pending.empty());
    assert(tube_mark_watched("abcdefghijk")==1);
    host.m_stack={"history"};host.m_crumbs={"History"};host.m_pos_stack.clear();host.RequestPage("history",0);pump();
    auto history_video=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),[](const EvoProviderRow& row){return row.id=="abcdefghijk";});
    assert(history_video!=host.m_all_rows.end());const size_t history_index=history_video-host.m_all_rows.begin();
    host.SetPageOffset((history_index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,history_index%EVO_PROVIDER_PAGE_SIZE);pump();
    assert(host.DiscussionKey(4));pump();
    block_action=std::find_if(host.m_all_rows.begin(),host.m_all_rows.end(),[](const EvoProviderRow& row){return row.title=="Block channel";});
    assert(block_action!=host.m_all_rows.end());const std::string history_block_id=block_action->id.c_str();
    block_index=block_action-host.m_all_rows.begin();host.SetPageOffset((block_index/EVO_PROVIDER_PAGE_SIZE)*EVO_PROVIDER_PAGE_SIZE,block_index%EVO_PROVIDER_PAGE_SIZE);pump();
    host.m_pending_activation=history_block_id;host.ApplyPendingActivation();pump();
    assert(host.m_stack.size()==1 && host.m_stack.back()=="history" && host.m_model.query.empty() && pending.empty());
    assert(std::none_of(host.m_all_rows.begin(),host.m_all_rows.end(),[](const EvoProviderRow& row){return row.id=="abcdefghijk";}));
    host.m_stack={"favorites","account:menu:abcdefghijk","local:video:0:block:abcdefghijk"};
    host.m_crumbs={"Favorites","Video actions","Block channel"};host.RequestPage(host.m_stack.back().c_str(),0);pump();
    assert(host.m_stack.size()==3 && !host.m_model.loading && pending.empty());
    host.m_stack={"library:suggestions:abcdefghijk"};host.m_crumbs={"Related videos"};host.m_pos_stack.clear();
    host.RequestPage(host.m_stack.back().c_str(),0);
    reply("{\"job\":\"related-ui\",\"state\":\"pending\"}");clock_ms+=300;tube_poll();
    reply("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"title\":\"From this channel\",\"is_folder\":true}]}}");
    pump();assert(host.m_all_rows.size()==1 && host.FocusPublishedRow(0));pump();
    card=host.m_context->GetFocusElement();assert(card);card->DispatchEvent("click",Rml::Dictionary{});
    host.ApplyPendingActivation();assert(host.m_stack.back()=="library:filter:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
    reply("{\"job\":\"filtered-ui\",\"state\":\"pending\"}");clock_ms+=300;tube_poll();
    reply("{\"state\":\"done\",\"result\":{\"items\":[{\"id\":\"cdefghijklm\",\"title\":\"Related card\",\"playback_context\":{\"group\":\"aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee\",\"index\":2}}]}}");
    pump();assert(host.m_all_rows.size()==1 && host.FocusPublishedRow(0));pump();
    card=host.m_context->GetFocusElement();assert(card);card->DispatchEvent("click",Rml::Dictionary{});
    host.ApplyPendingActivation();assert(evo_rmlui_provider_take_selection(&selection));
    assert(!strcmp(selection.item_id,"cdefghijklm") && selection.playback_index==2 &&
           !strcmp(selection.playback_playlist,"group:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"));
    assert(pending.empty());
    host.m_doc->Close();Rml::RemoveContext("chat-ui");host.m_context=nullptr;host.m_doc=nullptr;host.m_model_handle={};
    Rml::Shutdown();evo_provider_youtube.shutdown();unlink(evo_data_path("youtube.json"));rmdir(directory);
    puts("PASS: actual provider callbacks preserve current focus/scroll across polls, handle removal/mode/setup, and bind literal shipping chat RML");
}
