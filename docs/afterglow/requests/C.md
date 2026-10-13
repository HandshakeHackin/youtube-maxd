# Package C: requests to the frame owner

Screens: `channel`, `channels`, `subscriptions`. Every stand-in named here is in
`native/afterglow/screens/channel_family.hpp|.cpp` (or the file given) and is
marked `// REQUEST:` in the code. When a request is applied the stand-in goes.

## 1. Shared files

| # | Where | What and why | Proposed | Stand-in today |
| --- | --- | --- | --- | --- |
| 1 | `theme.hpp` | The grid cell of the design notes (section 2) is not in the theme; results, grid, library, channel and subscriptions all lay out the same four columns. | `kCellW = 394`, `kCellH = 222`, `kCellPitchX = 418`, `kCellPitchY = 352`, `kColumns = 4`, `kPageBottom = 968` | `family::kCell*`, `kColumns`, `kPageBottom` |
| 2 | `design.hpp` | The frame hands a screen its `Phase` and moves nothing for it, so every screen needs the hand-over of the motion table (deeper/back out 90 and 110 px, section change sinks to 97.5 %, card to video page: furniture gone in 150 ms, less motion: fades only). | `struct HandOver { HandOver(G &, const Phase &); ~HandOver(); float alpha; };` used as `Enter` is | `family::Travel` |
| 3 | `design.hpp` | Loading bones with one light crossing the page every 1.7 s (motion table: Loading), still with less motion. | `void bone(G &, const Rect &, float radius); void card_bone(G &, const Rect &, bool with_text = true);` | `family::bone`, `family::card_bone` |
| 4 | `design.hpp` | The page with nothing to show (empty, signed out, failed, offline: boards 37 and 38) with one or two ways on that can take the focus. | `struct Notice { void lay_out(const ui::Fonts &, float cx, float top, std::string_view body, const Choice *, int count); void draw(G &, Icon, std::string_view title, std::string_view body, const Choice *, float focus_at, bool focused) const; std::array<Rect, 2> buttons; };` plus `trouble_title(Load)`, `trouble_icon(Load)` | `family::Notice`, `family::Choice`, `family::trouble_*` |
| 5 | `design.hpp` | `button_width()` and `chip_width()` take a `G`, which `update()` has not, but a screen must know its rectangles there to answer `focus()`. | overloads `float button_width(const ui::Fonts &, std::string_view, bool has_icon = true); float chip_width(const ui::Fonts &, std::string_view, float height = 44, bool menu = false);` | `family::button_width`, `family::chip_width` (the same sums) |
| 6 | `design.hpp` | A pressed control that waits for the provider (5.6: "the kit's spinner, a dimmed label"): `button()` and `chip()` have no waiting state. | `button(..., bool busy = false)`: the glyph becomes the kit's spinner | the button breathes between 50 and 90 % opacity |
| 7 | `design.hpp` | Two small pieces every grid screen repeats: the scrim that closes the last 76 px over the hints, and a picture fading in over its placeholder. | `void foot_scrim(G &); void picture_box(G &, const Rect &, float radius, const Picture &, Color tint = kWhite);` | `family::foot_scrim`, `family::picture_box` |
| 8 | `design.hpp` | The focus light of a face that has no picture should take the avatar's own colours; `tint_of()` is private to `design.cpp`. | `Color avatar_tone(std::string_view name, std::string_view id);` | the look's focus colour |
| 9 | `frame` / `Nav` | "Touchpad: Actions" is on every screen of the controller map (board 14), so the sheet belongs to the frame, as the dialog does; so does the step from a provider row to the page that shows it. | `virtual void actions(const std::string &row, std::string_view about) = 0;` (the frame reads `Data::menu()`, lists it, acts through `Data::act()`), and `bool route_of(const model::Row &, Route *)` | `family::Sheet` (a member of each of the three screens) with its own `route_of()` and `icon_of()` |
| 10 | `frame_draw.cpp` | A screen whose header is a picture cannot say so: `draw_header()` writes `route.title` when `Screen::title()` is empty (screen.hpp says "" hides the header), and draws the clock flush at `kRight`, on the banner's corner (board 11 has it 28 px inside the banner; the design notes give the clock a glass plate on a picture). The frame's clock is also drawn over a sheet a screen draws. | `virtual HeaderStyle header() const` with `none`, `title`, `on_picture`; the frame places the clock accordingly | `Channel::title()` returns one space; the banner fades into the page under the clock |
| 11 | `screen.hpp` | `update()` has no `Look`, so a kit component that lives in a screen (`ui::TabBar`) cannot wear the look's theme where it is updated. | `const Look &look;` in `UpdateContext` | `draw()` dresses a copy of the bar; its pace is set by hand (18, 0.9) |
| 12 | `data_rows.cpp`, `models.hpp` | `add_row()` does not fill `model::Channel::handle`, `banner`, `subscribed`, `unseen` (ARCHITECTURE 5.4 says facts do) and `subscribers` from the fact; a `model::Video` has no way back to its row's facts. | fill those four from the facts; add `bool live` to `model::Channel` and `int published_days = -1` to `model::Video` | `family::Feed` keeps every row beside the video or channel it became |
| 13 | `tests/afterglow/host_main.cpp` | (a) `stbi_flip_vertically_on_write(1)` is set before the board loop, so the painted art that `picture_bytes()` encodes is flipped too: every thumbnail and banner in a board picture is upside down. (b) The harness draws only the last frame of a board, so a picture first asked for in `draw()` never arrives. | (a) set the flip only around the final `stbi_write_png`; (b) draw every frame, or say in ARCHITECTURE 6 that screens ask in `update()` | (b) each screen asks for the pictures in view in `update()` (`ask_pictures`); (a) none: the pictures of this package are upside down until it is fixed |
| 14 | text (`text_kit.cpp`, the kit's baked faces) | The pinned kit's faces lack ä ö ü ß and `clean()` removes them: the long-texts boards read "h tten", "N chte". | the `european` set of the proposal's faces, or the text engine | none |

## 2. Catalogs, facts and acting rows this package reads (for the data bridge)

Facts marked **new** are not in the table of ARCHITECTURE 5.5 yet.

| Screen | Catalog | Row | Facts read |
| --- | --- | --- | --- |
| channel | `channel:<id>` | first row `channel:<id>` | `handle`, `subscribers`, `banner_url`, `subscribed`, `tabs`, `tab`; **new** `videos` (the channel's count as YouTube words it, "184 videos"), `notify` (`all`, `personalized`, `none`), `search` (`0` when the channel offers no search) |
| channel | `channel-tab:<id>:<section>` and `...:<order>` | first row `channel:<id>` | `sorts`, `sort` |
| channel | the same | a video | the video facts of 5.5 |
| channel | `channel-tab:<id>:playlists` | a playlist (a folder row) | `count`, `updated` |
| channels, subscriptions | `library:channels` | a channel | `subscribers`, `unseen`; **new** `live` (the channel is live now) |
| channels | `settings:channel-sort` | `settings:channel-sort:<0..2>` | `selected` |
| channels, subscriptions | `subscriptions` | a channel | none (the row itself) |
| subscriptions | `library:subscriptions` | a video | the video facts of 5.5; **new** `published_days` (whole days since it was published, 0 for today: the feed is grouped by it; without it the feed is one group, "Latest") |

Acting rows (`Data::act`), each followed by `Data::forget` of what it changed:

| What | Row | Then |
| --- | --- | --- |
| Subscribe | `account:confirm:subscribe:<UC...>`, then the `account:apply:<n>` it answers with | forget `channel:<id>`, `library:channels`, `library:subscriptions` |
| Unsubscribe (after `nav.confirm`) | `account:confirm:unsubscribe:<UC...>`, then `account:apply:<n>` | the same |
| Notification level | `notify:confirm:level:<level>:<UC...>`, then `account:apply:<n>` | forget `channel:<id>` |
| Order of the channels | `settings:channel-sort:<n>` (its answer, the three rows again, says which is in force) | forget `library:channels`, `settings:channel-sort` |
| Mark all channels as watched | `channels:watched` | forget `library:channels` |
| Anything in a row's sheet | the row chosen from `Data::menu(row)` | forget the menu; the screen forgets its own lists |

Other things the bridge or another package has to know:

- **Search in channel**: Square pushes `Route{ScreenId::search, "channel-search:<UC...>", "Search in <name>"}`. The search screen (package A) has to take that argument as its scope.
- **A playlist** of the Playlists section opens `Route{ScreenId::grid, "<the row's id>", title}`.
- **Mark as watched** on board 28 reads as an action on the tile in focus. The provider has only "all channels" (`channels:watched`), so Triangle is "Mark all as watched". A row `channels:watched:<UC...>` would let it be the one tile.
- **The order of the tiles** is the provider's: the screen never sorts. In a fixture set the list does not change when an order is chosen (one file), the chips do.

## 3. Fixtures

New sets `signed-in`, `empty`, `offline`, `long-texts`, `guest-follows` (profiles: Mara, signed in; `guest-follows` is a Guest with three channels followed on the console) and new catalogs in `guest`. They are written by one generator (kept outside the repository; its data is the proposal's sample content). Two files other packages are likely to add as well: `<set>/profile.json`, and `signed-in/catalog/library_subscriptions.json` (Home's third shelf): this package's version has two pages and the fact `published_days`, and its boards count on its 16 + 8 videos.

## 4. Interface texts

The `tr()` texts of the three screens and of `channel_family.cpp` are not in the catalogs yet (filled centrally): run `scripts/i18n_extract.py` over `native/afterglow/screens/channel*.cpp`, `channels.cpp`, `subscriptions.cpp`. Patterns with a value: `{0} new`, `{0} subscriptions`, `Unsubscribe from {0}?`, `Search in {0}`, `{0} has nothing in this section.`, `Nothing from {0} in your feed yet`, `Nothing from {0} under this filter`.
