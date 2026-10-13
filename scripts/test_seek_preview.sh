#!/usr/bin/env bash
# Seek-preview thumbnails: the sheet cache and its player side with FFmpeg's
# real JPEG, WebP and PNG decoders under sanitizers, then the block the player
# controls draw them with, extracted from the generated source.
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
work="$(python3 "${root}/scripts/prepare_native.py")"
destination="${root}/build/tests/seek-preview"
mkdir -p "$destination"
python3 - "$work" "$destination" <<'PY'
from pathlib import Path
import struct
import sys
import zlib
evo = Path(sys.argv[1]) / 'projects/evoplayer'
output = Path(sys.argv[2])
hud = (evo / 'ui_kit/src/evo_hui_osd.cpp').read_text()
start = hud.index('// ProsperoTube seek preview HUI')
end = hud.index('// End ProsperoTube seek preview HUI', start)
(output / 'seek_preview.inc').write_text(hud[start:end])
# Drawn with the seek mark, inside the controls' scrub branch and under their
# opacity; the sheets are released when the player closes; the cache is built.
bottom = hud.index('void OsdScreen::draw_bottom(DrawList &list, const Context &ctx) const')
assert bottom < hud.index('if (scrub_.value > 0.01f && raw_.duration_sec > 0.0)', bottom) < \
    hud.index('list.rounded_rect({sx - 2, track.y - 14, 4, track.h + 28}, 2, kWhite);') < start < \
    hud.index('ui::text(list, fonts.mono, clock_text(raw_.position_sec), track.x, 970, 24, kWhite);') < \
    hud.index('void OsdScreen::draw_centre(DrawList &list, const Context &ctx) const')
assert hud.count('tube_seek_preview_frame(') == 1 and hud.count('#include "tube_seek_sheets.h"') == 1
screen = (evo / 'core/src/screens/PlayerScreen.cpp').read_text()
assert screen.count('void PlayerScreen::onExit() {\n    tube_seek_preview_drop();\n') == 1
makefile = (evo / 'Makefile').read_text()
assert makefile.count('    addons/src/tube_seek_sheets.c') == 1
# One 5 x 5 sheet of 160 x 90 tiles, each tile one colour by its number.
width, height = 800, 450
rows = bytearray()
for y in range(height):
    rows.append(0)
    for x in range(width):
        cell = y // 90 * 5 + x // 160
        rows += bytes((40 + cell * 37 % 200, 40 + cell * 59 % 200, 40 + cell * 83 % 200))
def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
(output / 'sheet.png').write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
                                   chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))
PY
ffmpeg -loglevel error -y -i "$destination/sheet.png" -q:v 2 -frames:v 1 -update 1 "$destination/sheet.jpg"
ffmpeg -loglevel error -y -i "$destination/sheet.png" -c:v libwebp -quality 90 -frames:v 1 "$destination/sheet.webp"
read -ra ffmpeg_flags <<< "$(pkg-config --cflags --libs libavcodec libswscale libavutil)"
flags=(-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer
       -I"${root}/native" -I"${root}/native/vendor/cJSON")
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" -c "${root}/native/tube_player_extra.c" -o "$destination/tube_player_extra.o"
cc -std=c11 "${flags[@]}" -c "${root}/native/vendor/cJSON/cJSON.c" -o "$destination/cJSON.o"
cc -std=c11 -D_DEFAULT_SOURCE -DTUBE_SEEK_SHEETS_NO_NET "${flags[@]}" "${ffmpeg_flags[@]}" -c "${root}/native/tube_seek_sheets.c" -o "$destination/tube_seek_sheets.o"
cc -std=c11 -D_DEFAULT_SOURCE "${flags[@]}" "${root}/tests/test_seek_sheets.c" "$destination/tube_seek_sheets.o" \
    "$destination/tube_player_extra.o" "$destination/cJSON.o" "${ffmpeg_flags[@]}" -lm -o "$destination/sheets"
ASAN_OPTIONS=detect_leaks=1 "$destination/sheets" "$destination/sheet.jpg" "$destination/sheet.webp" "$destination/sheet.png"
g++ -std=c++17 "${flags[@]}" -I"$destination" "${root}/tests/test_seek_preview_hud.cpp" "$destination/tube_seek_sheets.o" \
    "$destination/tube_player_extra.o" "$destination/cJSON.o" "${ffmpeg_flags[@]}" -lm -o "$destination/hud"
ASAN_OPTIONS=detect_leaks=1 "$destination/hud"
