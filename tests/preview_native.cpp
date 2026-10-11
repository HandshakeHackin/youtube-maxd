// Render the shipping RML through EVO's CPU backend and verify its focus/events.
// SPDX-License-Identifier: GPL-3.0-or-later
#include <RmlUi/Core.h>
#include "evo_rmlui_render.h"
#include "evo_rmlui_system.h"
#include "evo_rmlui_provider.h"
#include "evo_rmlui_bundle.h"
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

const EvoRmlBundleFile* evo_rmlui_bundle_find(const std::string&) { return nullptr; }

static void capture(Rml::Context* context, EvoRenderInterface& renderer,
                    std::vector<uint32_t>& pixels, const std::filesystem::path& file) {
    std::fill(pixels.begin(), pixels.end(), 0xff161110);
    renderer.SetFramebuffer(pixels.data());
    context->Update(); context->Render();
    std::ofstream out(file, std::ios::binary);
    out << "P6\n1920 1080\n255\n";
    for (uint32_t p : pixels) {
        const char rgb[] = {(char)p, (char)(p >> 8), (char)(p >> 16)};
        out.write(rgb, 3);
    }
}

int main(int argc, char** argv) {
    assert(argc == 4);
    std::filesystem::path source(argv[1]), fonts(argv[2]), output(argv[3]);
    std::filesystem::create_directories(output);
    EvoSystemInterface system;
    EvoRenderInterface renderer(1920, 1080);
    Rml::SetSystemInterface(&system); Rml::SetRenderInterface(&renderer);
    assert(Rml::Initialise());
    assert(Rml::LoadFontFace((fonts / "Lato-Regular.ttf").string()));
    assert(Rml::LoadFontFace((fonts / "Lato-Bold.ttf").string()));
    assert(Rml::LoadFontFace((fonts / "DejaVuSans.ttf").string(), true));
    Rml::Context* context = Rml::CreateContext("tube-preview", {1920, 1080});
    assert(context);
    EvoProviderModel model;
    auto constructor = context->CreateDataModel("youtube");
    assert(constructor);
    auto row = constructor.RegisterStruct<EvoProviderRow>();
    row.RegisterMember("id", &EvoProviderRow::id);
    row.RegisterMember("title", &EvoProviderRow::title);
    row.RegisterMember("subtitle", &EvoProviderRow::subtitle);
    row.RegisterMember("art", &EvoProviderRow::art);
    row.RegisterMember("initial", &EvoProviderRow::initial);
    row.RegisterMember("duration", &EvoProviderRow::duration);
    row.RegisterMember("is_folder", &EvoProviderRow::is_folder);
    row.RegisterMember("is_live", &EvoProviderRow::is_live);
    row.RegisterMember("progress", &EvoProviderRow::progress);
    row.RegisterMember("progress_w", &EvoProviderRow::progress_w);
    row.RegisterMember("played", &EvoProviderRow::played);
    row.RegisterMember("selection_key", &EvoProviderRow::selection_key);
    constructor.RegisterArray<std::vector<EvoProviderRow>>();
#define BIND(name) constructor.Bind(#name, &model.name)
    BIND(provider_name); BIND(breadcrumb); BIND(page_info); BIND(status); BIND(has_error);
    BIND(loading); BIND(empty); BIND(rows); BIND(has_selected); BIND(selected_title);
    BIND(selected_subtitle); BIND(selected_resume); BIND(selected_overview);
    BIND(discussion); BIND(review); BIND(auth); BIND(chat); BIND(description);
#undef BIND
    Rml::String activated;
    constructor.BindEventCallback("activate", [&activated](Rml::DataModelHandle, Rml::Event& event, const Rml::VariantList&) {
        activated = event.GetCurrentElement()->GetAttribute<Rml::String>("rowid", "");
    });
    constructor.BindEventCallback("setup_url", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {});
    auto handle = constructor.GetModelHandle();
    Rml::ElementDocument* doc = context->LoadDocument((source / "youtube.rml").string());
    assert(doc); doc->Show();
    std::vector<uint32_t> pixels(1920 * 1080);
    const char* titles[] = {"A new perspective on our solar system", "Building a quiet gaming setup",
                           "Music for a late evening", "The science of everyday things",
                           "Inside a remarkable old city", "A very long title that should remain inside its card without breaking navigation",
                           "Cooking something simple", "Designing a controller-friendly app"};
    model.provider_name = "ProsperoTube"; model.breadcrumb = "Search / science";
    model.page_info = "1 / 3";
    for (int i = 0; i < 8; ++i) {
        EvoProviderRow r;
        r.id = "fixture-" + std::to_string(i); r.title = titles[i]; r.subtitle = "Fixture channel · offline preview";
        r.initial = "P"; r.duration = "12:34"; r.is_folder = false;
        r.art = "evo:mem/fixture-" + std::to_string(i);
        std::vector<uint32_t> art(412 * 226);
        for (int y = 0; y < 226; ++y) for (int x = 0; x < 412; ++x) {
            int red = 40 + (x + i * 25) % 120, green = 35 + (y + i * 30) % 110, blue = 65 + (x / 3 + i * 20) % 120;
            art[y * 412 + x] = 0xff000000u | (unsigned)red | (unsigned)green << 8 | (unsigned)blue << 16;
        }
        renderer.SetMemoryTexture(r.art.c_str(), art.data(), 412, 226);
        model.rows.push_back(r);
    }
    model.rows[0].progress = 37; model.rows[0].progress_w = "37%";
    model.rows[4].played = true;
    model.has_selected = true; model.selected_title = titles[0];
    model.selected_subtitle = "Fixture channel"; model.selected_resume = "Resume from 4:38";
    model.selected_overview = "Offline preview generated with the shipping RML, fonts, and CPU renderer. No service or hardware claims.";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    Rml::ElementList cards; doc->GetElementsByClassName(cards, "tube-card");
    std::vector<Rml::Element*> visible;
    for (auto* card : cards)
        if (!card->GetAttribute<Rml::String>("rowid", "").empty() && card->IsVisible()) visible.push_back(card);
    assert(visible.size() == 8);
    visible[0]->Focus(); context->Update();
    for (auto* card : visible) {
        auto offset = card->GetAbsoluteOffset(Rml::BoxArea::Border);
        auto size = card->GetBox().GetSize(Rml::BoxArea::Border);
        assert(offset.x >= 0 && offset.y >= 0 && offset.x + size.x <= 1920 && offset.y + size.y <= 1000);
    }
    capture(context, renderer, pixels, output / "search.ppm");
    context->ProcessKeyDown(Rml::Input::KI_RIGHT, 0); context->ProcessKeyUp(Rml::Input::KI_RIGHT, 0);
    context->Update();
    assert(context->GetFocusElement() != visible[0]);
    Rml::Dictionary params; params["button"] = 0;
    context->GetFocusElement()->DispatchEvent(Rml::EventId::Click, params);
    assert(activated == "fixture-1");
    capture(context, renderer, pixels, output / "focus.ppm");
    model.rows.clear(); model.breadcrumb = "Search / fixture / YouTube account actions";
    model.page_info = "1 / 1"; model.selected_title = "Like video";
    model.selected_subtitle = "YouTube profile: fixture"; model.selected_resume = "";
    model.selected_overview = "Choose a change for the selected video. A confirmation follows.";
    const char* actions[] = {"Like video", "Dislike video", "Remove video rating", "Add to YouTube Watch later",
        "Remove from YouTube Watch later", "Subscribe on YouTube", "Unsubscribe on YouTube"};
    for (int i = 0; i < 7; ++i) {
        EvoProviderRow row; row.id = "account:confirm:fixture-" + std::to_string(i);
        row.title = actions[i]; row.subtitle = "YouTube profile: fixture"; row.initial = "Y"; row.is_folder = true;
        model.rows.push_back(row);
    }
    handle.DirtyAllVariables(); context->Update(); context->Update();
    capture(context, renderer, pixels, output / "account-actions.ppm");
    model.rows.resize(1); model.rows[0].id = "account:apply:1";
    model.rows[0].subtitle = "Confirm for YouTube profile: fixture";
    model.selected_subtitle = model.rows[0].subtitle;
    model.selected_overview = "Selected video: Offline fixture. Cross applies the change to YouTube; Circle returns without submitting it.";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    cards.clear(); visible.clear(); doc->GetElementsByClassName(cards, "tube-card");
    for (auto* card : cards)
        if (!card->GetAttribute<Rml::String>("rowid", "").empty() && card->IsVisible()) visible.push_back(card);
    assert(visible.size() == 1);
    visible[0]->Focus(); visible[0]->DispatchEvent(Rml::EventId::Click, params);
    assert(activated == "account:apply:1");
    capture(context, renderer, pixels, output / "account-confirmation.ppm");
    model.review = true; model.breadcrumb = "Accounts / Confirm playlist change";
    model.rows[0].title = model.selected_title = "Create private YouTube playlist";
    model.selected_overview = "Name: A new private playlist with a long name\nVideo: abcdefghijk\nCross confirms; Circle returns without submitting it.";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(doc->GetElementById("tube-review-footer")->IsVisible());
    assert(!doc->GetElementById("tube-footer")->IsVisible());
    assert(doc->GetElementById("tube-discussion-text")->IsVisible());
    capture(context, renderer, pixels, output / "playlist-confirmation.ppm");
    model.breadcrumb = "Accounts / Remove selected account";
    model.rows[0].title = model.selected_title = "Remove selected account";
    model.selected_subtitle = "Cross confirms; Circle returns without removing it";
    model.rows[0].subtitle = model.selected_subtitle;
    model.selected_overview = "Profile: Living room. Remove this account and its saved sign-in from ProsperoTube. Continue as Guest; other accounts remain available.";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(doc->GetElementById("tube-review-footer")->IsVisible());
    assert(doc->GetElementById("tube-discussion-text")->IsVisible());
    capture(context, renderer, pixels, output / "account-removal.ppm");
    model.auth = true; model.breadcrumb = "Accounts / YouTube sign-in";
    model.rows.clear();
    const char* auth_titles[] = {"ABCD-EFGH", "https://www.google.com/device", "Cancel sign-in"};
    const char* auth_ids[] = {"auth:info:code", "auth:info:url", "auth:cancel"};
    for (int i = 0; i < 3; ++i) {
        EvoProviderRow row; row.id = auth_ids[i]; row.title = auth_titles[i];
        row.subtitle = "Authorize using your phone or computer"; row.is_folder = true;
        model.rows.push_back(row);
    }
    model.selected_title = auth_titles[0]; model.selected_subtitle = "Enter this code on your phone or computer";
    model.selected_overview = "Open https://www.google.com/device on your phone or computer and enter ABCD-EFGH. Waiting for authorization; status checks are automatic. The code expires automatically if sign-in is not completed.";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(doc->GetElementById("tube-auth-footer")->IsVisible());
    assert(!doc->GetElementById("tube-review-footer")->IsVisible());
    assert(doc->GetElementById("tube-discussion-text")->IsVisible());
    cards.clear(); visible.clear(); doc->GetElementsByClassName(cards, "tube-card");
    for (auto* card : cards)
        if (!card->GetAttribute<Rml::String>("rowid", "").empty() && card->IsVisible()) visible.push_back(card);
    assert(visible.size() == 3);
    visible[2]->Focus(); visible[2]->DispatchEvent(Rml::EventId::Click, params);
    assert(activated == "auth:cancel");
    capture(context, renderer, pixels, output / "device-sign-in.ppm");
    model.auth = false;
    model.review = false;
    model.rows.clear(); model.empty = true; model.has_selected = false; model.status = "";
    handle.DirtyAllVariables(); context->Update();
    doc->GetElementById("tv-card-setup-url")->Focus();
    capture(context, renderer, pixels, output / "empty.ppm");
    model.loading = true; model.status = "Resolving a video through your LAN companion";
    handle.DirtyAllVariables(); capture(context, renderer, pixels, output / "loading.ppm");
    model.loading = false; model.has_error = true; model.status = "Playback information expired. Select the video again to refresh it.";
    handle.DirtyAllVariables(); capture(context, renderer, pixels, output / "error.ppm");
    model.discussion = true; model.empty = false; model.has_error = false; model.status = "";
    model.breadcrumb = "Search / fixture / Top comments"; model.page_info = "1 / 2";
    model.has_selected = true; model.selected_title = "A comment author";
    model.selected_subtitle = "Today | 12 likes | 3 replies | Pinned | Creator";
    model.selected_resume = "";
    model.selected_overview = "Literal <img id='comment-injection' src='evil'> & comment text.\n";
    for (int i = 0; i < 45; ++i)
        model.selected_overview += "Line " + std::to_string(i + 1) + ": This long comment remains available in the scrollable reader.\n";
    for (int i = 0; i < 8; ++i) {
        EvoProviderRow row;
        row.id = "discussion:replies:fixture-" + std::to_string(i);
        row.title = "Author " + std::to_string(i + 1); row.subtitle = "Today | 12 likes | 3 replies";
        row.is_folder = true; model.rows.push_back(row);
    }
    handle.DirtyAllVariables(); context->Update(); context->Update();
    auto* reader = doc->GetElementById("tube-discussion-text");
    assert(reader && reader->IsVisible());
    assert(reader->GetClientWidth() > 900);
    assert(!doc->GetElementById("comment-injection"));
    assert(reader->GetScrollHeight() > reader->GetClientHeight());
    cards.clear(); visible.clear(); doc->GetElementsByClassName(cards, "tube-card");
    for (auto* card : cards)
        if (!card->GetAttribute<Rml::String>("rowid", "").empty() && card->IsVisible()) visible.push_back(card);
    assert(visible.size() == 8);
    visible[0]->Focus(); context->Update();
    context->ProcessKeyDown(Rml::Input::KI_DOWN, 0); context->ProcessKeyUp(Rml::Input::KI_DOWN, 0);
    context->Update();
    assert(context->GetFocusElement() == visible[1]);
    context->GetFocusElement()->DispatchEvent(Rml::EventId::Click, params);
    assert(activated == "discussion:replies:fixture-1");
    capture(context, renderer, pixels, output / "discussion.ppm");
    reader->SetScrollTop(500); context->Update();
    assert(reader->GetScrollTop() > 0);
    capture(context, renderer, pixels, output / "discussion-scroll.ppm");
    model.chat = true; model.breadcrumb = "Live video / Top chat";
    model.selected_title = "Supporter"; model.selected_subtitle = "Now | Super Chat | $5.00 | Moderator";
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(doc->GetElementById("tube-chat-footer")->IsVisible());
    assert(!doc->GetElementById("tube-discussion-footer")->IsVisible());
    assert(!doc->GetElementById("comment-injection"));
    capture(context, renderer, pixels, output / "live-chat.ppm");
    model.chat = false;
    model.discussion = false; model.review = true; model.rows.clear();
    model.breadcrumb = "Top comments / Comment rating / Confirm";
    model.page_info = "1 / 1"; model.selected_title = "Like comment";
    model.selected_subtitle = "Cross confirms for the selected YouTube profile; Circle cancels";
    model.selected_overview = "Profile: Living room\nComment:\nLiteral <b>markup</b> & café.\nCross confirms; Circle returns without submitting it.";
    EvoProviderRow rating_row; rating_row.id = "account:apply:rating-preview";
    rating_row.title = model.selected_title; rating_row.subtitle = model.selected_subtitle;
    rating_row.is_folder = true; model.rows.push_back(rating_row);
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(doc->GetElementById("tube-review-footer")->IsVisible());
    assert(!doc->GetElementById("tube-discussion-footer")->IsVisible());
    capture(context, renderer, pixels, output / "comment-rating.ppm");
    model.review = false; model.discussion = true;
    model.rows.clear(); model.empty = true; model.has_selected = false;
    handle.DirtyAllVariables(); context->Update(); context->Update();
    assert(!doc->GetElementById("tv-card-setup-url")->IsVisible());
    capture(context, renderer, pixels, output / "discussion-empty.ppm");
    doc->Close(); Rml::RemoveContext("tube-preview"); Rml::Shutdown();
    std::cout << "PASS: RML data binding, eight-card bounds, D-pad focus, stable row activation, empty/loading/error screens, discussion reader scrolling and literal text\n";
}
