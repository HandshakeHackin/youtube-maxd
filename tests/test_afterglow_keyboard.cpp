// Exercise actual focused screen and shared text-sheet editing, without a GPU.
#include "afterglow/frame.hpp"
#include "afterglow/data_fixture.hpp"
#include "afterglow/screens/accounts.hpp"
#include <algorithm>
#include <cassert>
using namespace afterglow;
struct TestPlatform final : Platform
{
    std::uint32_t create_texture(int,int,const std::uint8_t*) override { return 0; }
    void release_texture(std::uint32_t) override {}
    unsigned fetch(const std::string&,Fetched) override { return 0; }
    void cancel_fetch(unsigned) override {}
    bool read_asset(const std::string&,std::string*) override { return false; }
    void wall_clock(int *h,int *m) override { *h=*m=0; }
    const char *translate(const char *s) override { return s; }
    unsigned language_revision() override { return 0; }
    void log(const char*) override {}
};
struct TestNav final : Nav
{
    void push(const Route&,Move,const Rect&) override {}
    void back() override {}
    void root(const Route&) override {}
    void open_rail() override {}
    void toast(std::string_view,ToastKind) override {}
    void toast(std::string_view,std::string_view,ToastKind) override {}
    void confirm(std::string_view,std::string_view,std::string_view,std::function<void(bool)>) override {}
    void ambient(std::string_view) override {}
};
int main(int argc,char **argv)
{
    assert(argc==2);
    TestPlatform platform; ui::Fonts fonts{}; FixtureSource source(argv[1]);
    source.set_delay(0);
    FrameOptions options;options.opening=false;options.reduced_motion=true;
    Frame frame(platform,source,fonts,options);
    frame.open({ScreenId::search,{},{}});
    frame.update({},1.f/60,"ab\bcd");
    assert(frame.snapshot_json().find("\"acd\"")!=std::string::npos);
    for(int i=0;i<60;++i)frame.update({},1.f/60);
    assert(std::find(source.asked().begin(),source.asked().end(),"search-suggest:acd")!=source.asked().end());
    assert(std::find(source.asked().begin(),source.asked().end(),"search-preview:acd")!=source.asked().end());
    frame.update({},1.f/60,"\rignored");
    assert(frame.screen()==ScreenId::results);
    frame.open({ScreenId::search,{},{}});
    frame.update({},1.f/60,std::string(150,'z'));
    assert(frame.snapshot_json().find(std::string(121,'z'))==std::string::npos);
    // A non-text page discards typing. Reopening Search cannot replay it.
    frame.open({ScreenId::home,{},{}});frame.update({},1.f/60,"hidden");
    frame.open({ScreenId::search,{},{}});frame.update({},1.f/60);
    assert(frame.snapshot_json().find("hidden")==std::string::npos);
    // Real shared text sheet: UTF-8 backspace, Enter and cancellation.
    Data data(source); ImageCache images(platform);Services services{data,images,platform,fonts};
    TestNav nav;Feedback feedback;UpdateContext context{services,nav,feedback};
    TextSheet sheet; bool answered=false,taken=false;std::string answer;
    auto callback=[&](bool ok,std::string text){answered=true;taken=ok;answer=std::move(text);};
    TextQuestion question;question.initial="caf\xc3\xa9";
    sheet.show(question,callback,context);context.typed="\bE\rignored";sheet.handle({},context);
    assert(answered&&taken&&answer=="cafE"&&!sheet.open());
    answered=false;question.initial="";sheet.show(question,callback,context);
    context.typed="\r";sheet.handle({},context);assert(!answered&&sheet.open());
    context.typed="secret\x1b";sheet.handle({},context);assert(answered&&!taken&&answer.empty()&&!sheet.open());
    answered=false;question.secret=true;question.initial="must not survive";
    sheet.show(question,callback,context);context.typed="abc";sheet.handle({},context);
    context.typed={};InputFrame pad;pad.pressed=hui::action_bit(Action::menu);sheet.handle(pad,context);
    assert(answered&&taken&&answer=="abc"); // controller submission still works
    acct::PasswordSheet password;
    password.open("Password", "Password", true, fonts, feedback);
    context.typed="ab\bCd\rignored";
    assert(password.update({},context)==1 && password.take()=="aCd" && password.take().empty());
    context.typed="\r";assert(password.update({},context)==0); // empty rejected
    const std::string long_password(170,'p');context.typed=long_password;
    assert(password.update({},context)==0 && password.take().size()==150);
    context.typed="old";password.update({},context);
    password.set_busy(true);context.typed="new\r";
    assert(password.update({},context)==0 && password.take()=="old");
    context.typed="\x1b";assert(password.update({},context)==-1 && !password.is_open());
    password.open("Password", "Password", true, fonts, feedback);
    context.top=false;context.typed="hidden";password.update({},context);
    assert(password.take().empty()); // covered sheets never consume edits
}
