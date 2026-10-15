#!/usr/bin/env python3
"""Exercise the actual Afterglow sign-in response handler with provider row facts."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'native/afterglow/screens/sign_in.cpp').read_text()
def extract(signature):
    a=source.index(signature);b=source.index('{',a)+1;depth=1
    while depth:
        depth+=(source[b]=='{')-(source[b]=='}');b+=1
    return source[a:b]
program=r"""
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <string>
#include <map>
#include <vector>
#include <cstdint>
namespace model {
enum class Load {idle,loading,ready,failed,offline};
struct Row {std::string id,title,subtitle;std::map<std::string,std::string> facts;
 std::string fact(const std::string &key)const{auto p=facts.find(key);return p==facts.end()?"":p->second;}};
}
struct Page {model::Load load=model::Load::ready;std::vector<model::Row> rows;std::string error;};
static const model::Row *find_row(const Page &p,const std::string &id){for(auto &r:p.rows)if(r.id==id)return &r;return nullptr;}
static const model::Row *find_prefix(const Page &p,const std::string &id){for(auto &r:p.rows)if(r.id.compare(0,id.size(),id)==0)return &r;return nullptr;}
static std::string fact_of(const Page &p,const std::string &key){for(auto &r:p.rows){auto s=r.fact(key);if(!s.empty())return s;}return "";}
namespace audio {enum class Cue {notify,saved,error};}
struct UpdateContext {struct {void play(audio::Cue,float=0,float=0,float=0){}void rumble(float,float){}}feedback;};
"""+extract('enum class State : std::uint8_t')+r""";
static constexpr float kPoll=2;
struct SignIn {
 State state_=State::starting;float state_age_=0,code_age_=0,remaining_=0,poll_in_=0;int missed_=0,choice_=0;
 std::string error_,code_,url_,profile_row_,profile_name_;
"""+extract('    void set(State state)')+extract('    void apply(const Page &page, bool first, UpdateContext &context)')+r"""
};
static Page page(std::string state,std::string error="") {
 Page p;p.rows.push_back({"auth:info","","",{{"state",state},{"error",error}}});return p;
}
int main(){
 SignIn screen;UpdateContext context;
 Page pending=page("pending");pending.rows[0].id="auth:info:code";
 pending.rows[0].facts.insert({{"user_code","ABCD-EFGH"},{"verification_url","https://www.google.com/device"},{"expires_in","1800"}});
 screen.apply(pending,true,context);assert(screen.state_==State::code && screen.remaining_==1800);
 pending.rows[0].facts["expires_in"]="899";screen.apply(pending,false,context);assert(screen.remaining_==899);
 auto failed=page("failed","The account could not be written to private storage.");
 screen.apply(failed,false,context);assert(screen.state_==State::failed && screen.error_==failed.rows[0].fact("error"));
 screen.apply(page("denied"),false,context);assert(screen.state_==State::denied && screen.error_.empty());
 screen.apply(page("canceled"),false,context);assert(screen.state_==State::canceled);
 screen.apply(page("expired"),false,context);assert(screen.state_==State::expired);
 // A late pending or authorized answer still revives an expired screen.
 screen.apply(pending,false,context);assert(screen.state_==State::code && screen.remaining_==899);
 Page approved=page("authorized");approved.rows[0].id="library:profile:oauth_0123456789abcdef";
 approved.rows[0].facts["name"]="Living room";screen.apply(approved,false,context);
 assert(screen.state_==State::approved && screen.profile_row_==approved.rows[0].id && screen.profile_name_=="Living room");
}
"""
with tempfile.TemporaryDirectory(prefix='sign-in-state-') as directory:
    temp=Path(directory);(temp/'test.cpp').write_text(program)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(temp/'test.cpp'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
print('PASS actual Afterglow sign-in: authoritative expiry, actionable error, terminal states and late approval')
