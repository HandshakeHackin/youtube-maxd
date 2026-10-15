"""Physical PS5 HID mapping, batched delivery, hot-plug and IME ownership."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='tube-keyboard-') as directory:
    temp=Path(directory)
    flags=['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'native')]
    subprocess.run([*flags,str(root/'tests/test_physical_keyboard.cpp'),'-o',str(temp/'unit')],check=True)
    subprocess.run([str(temp/'unit')],check=True)
    (temp/'evo_keyboard.h').write_text('int evo_keyboard_is_open(void);\n')
    (temp/'runtime.cpp').write_text(r"""
#include "afterglow/app/keyboard_input.hpp"
#include "afterglow/app/physical_keyboard.hpp"
#include "afterglow/app/afterglow.h"
#include <cassert>
#include <vector>
using afterglow::keyboard::Report;
static bool ime=false,shown=true;static int opens=0,closes=0,reads=0;
static std::vector<Report> pending;
extern "C" int sceSysmoduleLoadModule(std::uint16_t id){assert(id==0x106);return 0;}
extern "C" int sceUserServiceGetInitialUser(std::int32_t *u){*u=1;return 0;}
extern "C" int sceKeyboardInit(){return 0;}
extern "C" int sceKeyboardOpen(std::int32_t u,std::int32_t t,std::int32_t i,const void *p){assert(u==1&&!t&&!p);++opens;return i<2?i:-1;}
extern "C" int sceKeyboardClose(std::int32_t h){assert(h==0||h==1);++closes;return 0;}
extern "C" int sceKeyboardRead(std::int32_t h,void *out,std::int32_t capacity){assert(capacity==16);++reads;if(!h)return 0;int n=0;for(auto r:pending)static_cast<Report*>(out)[n++]=r;pending.clear();return n;}
extern "C" int tube_afterglow_enabled(){return 1;}
extern "C" int tube_afterglow_shown(){return shown;}
extern "C" int evo_keyboard_is_open(){return ime;}
static void report(int key,bool connected=true){Report r{};r.connected=connected;r.keys[0]=key;pending.push_back(r);}
int main(){
 report(4);report(0);report(5);report(0);tube_afterglow_keyboard_poll(1);
 assert(opens==12&&reads==2&&afterglow::keyboard::text()=="ab");
 tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text().empty());
 shown=false;report(6);report(0);int hidden_reads=reads;tube_afterglow_keyboard_poll(1);assert(reads==hidden_reads&&afterglow::keyboard::text().empty());
 shown=true;tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text().empty());
 // Even during the shown() grace period, the Player screen does no reads.
 report(10);report(0);hidden_reads=reads;tube_afterglow_keyboard_poll(0);assert(reads==hidden_reads);
 tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text().empty());
 ime=true;report(7);report(0);int before=reads;tube_afterglow_keyboard_poll(1);assert(reads==before);
 ime=false;tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text().empty());
 report(8);report(0);tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text()=="e");
 report(0,false);tube_afterglow_keyboard_poll(1);report(9);tube_afterglow_keyboard_poll(1);assert(afterglow::keyboard::text()=="f"&&opens==12);
 tube_afterglow_keyboard_close();assert(closes==2&&afterglow::keyboard::text().empty());
}
""")
    subprocess.run([*flags,'-I'+str(temp),str(temp/'runtime.cpp'),str(root/'native/afterglow/app/physical_keyboard.cpp'),'-o',str(temp/'runtime')],check=True)
    subprocess.run([str(temp/'runtime')],check=True)
print('PASS: physical keyboard mapping, ordered press/release batches, bounded repeat, hot-plug, interception and IME suppression')
