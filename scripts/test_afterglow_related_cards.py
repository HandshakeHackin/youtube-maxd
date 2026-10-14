"""Exercise Afterglow's actual in-player suggestion-card mapping on the host."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'native/afterglow/player/app/mount.cpp').read_text()


def block(start):
    offset = source.index(start)
    brace = source.index('{', offset)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[offset:end]


badges = block('unsigned card_badges(')
mapping = block('if (row != nullptr && card.title == row->title && plain_video_id(row->id))')
code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include "afterglow/models.hpp"
namespace afterglow {
struct Row { char id[160], title[160], art_url[384], badges[96]; int64_t duration_sec; int is_live; };
struct Card { std::string id,title,thumb; int duration_s=0; unsigned badges=0; };
bool plain_video_id(const char *id) {
    return id && std::strlen(id)==11 && std::strspn(id,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==11;
}
bool tube_art_url_allowed(const char *url) { return url && std::string(url).rfind("https://i.ytimg.com/",0)==0; }
'''+badges+r'''
void fill(Card &card, const Row *row) {
'''+mapping+r'''
}
}
int main() {
    using namespace afterglow;
    Row row{};
    std::strcpy(row.id,"abcdefghijk"); std::strcpy(row.title,"Video");
    std::strcpy(row.art_url,"https://i.ytimg.com/vi/abcdefghijk/hqdefault.jpg");
    std::strcpy(row.badges,"4k,hdr,cc,unknown"); row.duration_sec=123;
    Card card; card.title="Video"; fill(card,&row);
    assert(card.id==row.id && card.thumb==row.art_url && card.duration_s==123);
    assert(card.badges==(model::kBadge4K|model::kBadgeHdr|model::kBadgeCaptions));
    std::strcpy(row.art_url,"http://invalid.test/art"); row.duration_sec=999999;
    card=Card{}; card.title="Video"; fill(card,&row);
    assert(card.thumb=="https://i.ytimg.com/vi/abcdefghijk/mqdefault.jpg" && card.duration_s==360000);
    row.is_live=1; card=Card{}; card.title="Video"; fill(card,&row);
    assert(card.duration_s==0 && (card.badges & model::kBadgeLive));
    card=Card{}; card.title="Different"; fill(card,&row);
    assert(card.id.empty() && card.thumb.empty() && !card.badges);
}
'''
with tempfile.TemporaryDirectory(prefix='tube-related-card-') as folder:
    binary = Path(folder) / 'test'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    '-I', str(root / 'native'), '-x', 'c++', '-', '-o', str(binary)],
                   input=code, text=True, check=True)
    subprocess.run([str(binary)], check=True)
print('PASS: suggestion art, duration, badge and mismatch mapping')
