"""Seek-preview thumbnails in the player controls: the sheet cache joins the
build, the controls draw the tile under the seek target above the seek bar, and
the sheets are released when the player closes."""

HUD = '''            // ProsperoTube seek preview HUI (extracted by host tests).
            tube_seek_view tube_preview;
            if (tube_seek_preview_frame(raw_.scrub_target, raw_.duration_sec, sx, track.y - 52.0, &tube_preview)) {
                list.push_opacity(scrub_.value);
                /* Neighbours first, the target's tile over them. */
                for (int i = tube_preview.count - 1; i >= 0; --i) {
                    const tube_seek_tile_view &tile = tube_preview.tiles[i];
                    const std::uint32_t texture = ctx.textures.pixels(tile.pixels, tile.sheet_width, tile.sheet_height, 0x7365656bull);
                    if (!texture) continue;
                    const bool target = tile.box.offset == 0;
                    const Rect box{static_cast<float>(tile.box.x), static_cast<float>(tile.box.y),
                                   static_cast<float>(tile.box.w), static_cast<float>(tile.box.h)};
                    const Rect uv{static_cast<float>(tile.x) / static_cast<float>(tile.sheet_width),
                                  static_cast<float>(tile.y) / static_cast<float>(tile.sheet_height),
                                  static_cast<float>(tile.width) / static_cast<float>(tile.sheet_width),
                                  static_cast<float>(tile.height) / static_cast<float>(tile.sheet_height)};
                    list.rounded_rect({box.x - 3, box.y - 3, box.w + 6, box.h + 6}, 11, target ? kWhite : kBlack.with_alpha(0.6f));
                    list.image(texture, box, uv, target ? kWhite : kWhite.with_alpha(0.75f), 8);
                    if (target) {
                        const std::string at = clock_text(raw_.scrub_target);
                        const float w = fonts.mono.measure(at, 22) + 24.0f;
                        list.rounded_rect({box.cx() - w * 0.5f, box.y + box.h - 40, w, 32}, 8, kBlack.with_alpha(0.7f));
                        ui::text(list, fonts.mono, at, box.cx(), box.y + box.h - 17, 22, kWhite, Align::center);
                    }
                }
                list.pop_opacity();
            }
            // End ProsperoTube seek preview HUI.
'''


def apply_seek_preview_overlay(replace, player):
    replace(player + 'Makefile', '    addons/src/tube_backup.c', '    addons/src/tube_backup.c \\\n    addons/src/tube_seek_sheets.c')
    hud = player + 'ui_kit/src/evo_hui_osd.cpp'
    replace(hud, '#include "tube_player_extra.h"', '#include "tube_player_extra.h"\n#include "tube_seek_sheets.h"')
    mark = ('            list.rounded_rect({sx - 2, track.y - 14, 4, track.h + 28}, 2, kWhite);\n'
            '            list.pop_opacity();\n')
    replace(hud, mark, mark + HUD)
    # The sheets belong to the video that was playing: release them with the player.
    screen = player + 'core/src/screens/PlayerScreen.cpp'
    replace(screen, '#include "tube_player_extra.h"', '#include "tube_player_extra.h"\n#include "tube_seek_sheets.h"')
    replace(screen, 'void PlayerScreen::onExit() {\n', 'void PlayerScreen::onExit() {\n    tube_seek_preview_drop();\n')
