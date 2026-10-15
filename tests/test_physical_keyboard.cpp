#include "afterglow/app/keyboard_input.hpp"
#include <cassert>
using namespace afterglow::keyboard;
int main()
{
    assert(character(4,0,0)=='a' && character(4,2,0)=='A');
    assert(character(4,0,2)=='A' && character(4,2,2)=='a');
    assert(character(30,2,0)=='!' && character(52,2,0)=='"');
    assert(!character(4,1,0) && !character(4,64,0));
    assert(character(40,0,0)=='\r' && character(88,0,0)=='\r');
    assert(character(42,0,0)=='\b' && character(41,0,0)=='\x1b');
    assert(character(89,0,1)=='1' && !character(89,0,0));
    Input input; Report r{}; r.connected=1;r.length=1;r.keys[0]=4;
    std::string text;
    input.sample(r,true,100,text);assert(text=="a");
    input.sample(r,true,200,text);assert(text=="a");
    input.repeat(true,399999,text);assert(text=="a");
    input.repeat(true,400100,text);assert(text=="aa");
    input.repeat(true,1000000,text);assert(text=="aaa"); // one repeat, no catch-up
    r.keys[1]=4;input.sample(r,true,1000001,text);assert(text=="aaa");
    r.keys[0]=r.keys[1]=0;input.sample(r,true,1000002,text);
    r.keys[0]=5;input.sample(r,true,1000003,text);assert(text=="aaab");
    r.intercepted=1;input.sample(r,true,1000004,text);input.repeat(true,2000000,text);assert(text=="aaab");
    r.intercepted=0;r.keys[0]=6;input.sample(r,true,3000000,text);
    assert(text=="aaab"); // system-owned key held on return is not a new press
    input.sample(r,false,3000000,text);
    input.sample(r,true,3000001,text);input.repeat(true,4000000,text);assert(text=="aaab");
    r.connected=0;input.sample(r,true,4000001,text);
    r.connected=1;input.sample(r,true,4000002,text);assert(text=="aaabc");
    r.keys[0]=40;input.sample(r,true,4000003,text);input.repeat(true,5000000,text);assert(text=="aaabc\r");
    text.assign(256,'x');r.keys[0]=7;input.sample(r,true,5000001,text);assert(text.size()==256);
}
