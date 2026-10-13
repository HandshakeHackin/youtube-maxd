# Package E (Settings): requests to the frame owner and the data bridge

Settings is built on what the frame and the data layer have today, with a
local stand-in wherever something shared is missing. Each stand-in is marked
`// REQUEST:` in `native/afterglow/screens/settings*.cpp` and is listed here
with the exact thing asked for. Section 6 is the contract for package G: every
row of `settings` and the catalogs under it, with the facts this screen reads.

## 1. `Data::act()` with a typed value

- **What:** `unsigned Data::act(const std::string &row, std::string_view text)`
  (and `Source::act` under it): the row is carried out with a typed value.
- **Why:** three kinds of row take text: the alternative SponsorBlock server
  (`settings:sb:server`), the profile password (`settings:lock:set`,
  `:change`, `:remove`) and a pinned section's name
  (`settings:pin:rename:<id>`). Today listing such a row arms EVO's own text
  prompt, which the Afterglow frame does not show.
- **Stand-in:** the sheet (request 2) takes the text and then calls
  `data.act(row)` without it. In a fixture the answer file supplies the value.

## 2. Text entry as a service of the frame

- **What:** `Nav::ask_text(std::string_view title, std::string_view detail,
  std::string_view initial, bool secret, std::function<void(bool, std::string)> answered)`:
  a sheet with a field and the kit's keyboard, as `Nav::confirm()` is a dialog.
- **Why:** Settings, Search (A), the reply to a comment (B) and the profile
  password (F) all need typing; one sheet keeps the keys, the bindings
  (Square deletes, Options saves) and the look the same.
- **Stand-in:** `Settings::TextSheet` in `settings.cpp`: the kit's
  `ui::Keyboard` in a frosted sheet. Because `UpdateContext` has no `Look`, the
  keyboard takes the look's theme from a copy made while drawing; with
  `UpdateContext::look` (or the frame's sheet) that copy goes away.

## 3. The header: a path over the title, and a line under it

- **What:** `virtual std::vector<std::string> Screen::path() const` (names from
  the root down; the frame draws them as the overline "SETTINGS > CAPTIONS" at
  baseline 84 with the last one in the accent, and sets the title at baseline
  146 instead of 128, as board 32 has it) and
  `virtual std::string Screen::subtitle() const` (drawn at `kLeft`, baseline
  166, 22 px, `ink2`, as boards 31 and 33 have it).
- **Why:** both are parts of the approved boards; the accounts page (F) has the
  same line under its title.
- **Stand-in:** on a page `Settings::title()` returns `" "` so the frame draws
  no title, and `draw_path()` draws the path and the name; the tiles draw their
  line under the frame's title themselves.

## 4. The look follows the Theme when it is confirmed

- **What:** the frame takes the Theme on by itself: `Source::scheme()` read in
  `Data::pump()` (or the mount calling `Frame::set_scheme()` when
  `settings:ui:scheme:<n>` was carried out), so that the whole page changes
  look the moment the provider confirms the choice.
- **Why:** board text: "theme (the page itself changes look)". A screen cannot
  change the frame's look.
- **Stand-in:** the page shows the chosen theme in its preview (a small page in
  that look's roles); the frame's own look stays as it was started.

## 5. A toast with a second line

- **What:** `Nav::toast(std::string_view title, std::string_view detail, ToastKind kind)`.
- **Why:** the provider's refusals are sentences ("Automatic captions could not
  be switched: the settings file on this PS5 could not be written."); one toast
  line ends them in an ellipsis.
- **Stand-in:** the refusal is also shown in full in the help panel beside the
  row, with the danger edge, for as long as the focus stays on that row.

## 6. Row facts of `settings` and everything under it

ARCHITECTURE 5.5 lists `value`, `selected`, `kind`. Settings reads the facts
below. They replace that line of the table:

| Fact | On | Meaning |
| --- | --- | --- |
| `kind` | every row | `switch`, `list`, `slider`, `cycle`, `choice`, `action`, `text`, `page`, `link`, `info`, `order`, `reset` (below) |
| `label` | every row | the row's name without its value ("Show captions", not "Captions: on"), in the profile's language |
| `value` | rows that have one | the current value as shown ("Up to 2160p", "English"); on a `page` row a one-line summary of what is inside ("Skipping 3 categories") |
| `on` | `switch`, `order` | `1` when on (shown), `0` when off (hidden) |
| `selected` | `choice` | `1` on the choice that stands |
| `index`, `count` | `list`, `slider` | the place of the current choice among its choices and how many there are (a slider's fill; Left and Right before the list is read) |
| `number` | rows and choices that feed a preview | the value as a number: percent for sizes and scales, per mille from the bottom for the caption position (`-1`: the player's own), lines for title lines |
| `section` | rows of a list with headings | the heading the row stands under ("Main menu", "Discover", "Pinned"), in the profile's language |
| `tag` | any | a short mark after the name ("Experimental") |
| `attention` | any | `1`: an accent dot beside the row (and on its tile): a version is ready |
| `disabled` | any | `1`: shown dimmed and not changeable now; the row's subtitle says why |
| `danger` | `action` | `1`: drawn in the danger colour |
| `confirm`, `confirm_label` | `action`, `reset` | the question asked first through `nav.confirm()` and the word on its button |
| `done` | `action`, `reset` | what the toast says when it worked ("Backup created") |
| `secret` | `text` | `1`: a password: shown as dots, never read back |
| `text` | `text` | what the field starts with ("https://") |
| `up`, `down`, `toggle` | `order` | the ids of the rows that move it one place and show or hide it; `""` where it cannot move |
| `screen`, `argument` | `link` | the screen it opens (`screen_name()`: `reader`, `accounts`, `link_phone`, ...) and its argument (default: the row's id for the reader) |
| `preview` | rows and choices | which part of a preview the row feeds: `caption.on`, `caption.size`, `caption.style`, `caption.position`, `theme`, `card.scale`, `card.grid`, `card.title_lines`, `card.subtitle_lines`, `card.corners` |
| `preview_art_url` | one row of a page with a preview | a picture to show captions and the sample card on (the newest video of the profile's history, or a packaged picture) |
| `text_rgb`, `background_alpha`, `edge` | `caption.style` rows and choices | the style as `tube_subtitle_style_t` has it: `ffff00`, `0`..`255`, `none` / `outline` / `shadow` |
| `look`, `background`, `accent` | `theme` rows and choices | the look's id (`afterglow`, `onyx`, `daybreak`, `contrast`), or for a classic colour scheme `""` and its two colours as `rrggbb` |
| `update` | `settings:about` in the main list | the Updates tile's line ("0.2.0 is ready to install", "0.1.0 is the latest version") |

The interface texts among them (`label`, `value`, `section`, `tag`,
`confirm`, `confirm_label`, `done`) are wanted in the profile's language, as a
row's title and subtitle arrive today (`add_row()` passes those through `tr()`;
facts are copied as they are).

A row without `kind` still shows: a notice (`account:notice...`) as `info`, a
row with `selected` as `choice`, `link:`, `description:` and `settings:about:`
rows as `link` to the reader, anything else as `page` (Cross opens the list of
its id, which is what the classic browser does with it).

### The kinds

| Kind | Control | Cross | Left / Right | Carried out by |
| --- | --- | --- | --- | --- |
| `switch` | a switch | flips | off / on | `act(row id)` |
| `list` | `< value >` | opens the catalog of the row's id as a page of choices | the choice before / after, round | `act(choice id)` |
| `slider` | a track and the value | as `list` | one step, stops at the ends | `act(choice id)` |
| `cycle` | `value >` | the next value | Right: the next value | `act(row id)` |
| `choice` | a row with a mark | chooses it; the page closes when the provider confirmed | | `act(row id)` |
| `action` | a row | asks (`confirm`) and does it | | `act(row id)` |
| `text` | the value and `>` | opens the text sheet | | `act(row id, text)` (request 1) |
| `page` | summary and `>` | opens the catalog of the row's id as a page | | |
| `link` | summary and `>` | opens `screen` | | |
| `info` | name and value | | | |
| `order` | name, Shown / Hidden, a grip | takes the row: Up and Down move it, Cross puts it down | | `act(up)`, `act(down)`; Square `act(toggle)`; Triangle opens the catalog of the row's id |
| `reset` | not a row: Triangle "Defaults" on its page, Options on its tile | | | `act(row id)` after `confirm` |

What `Data::action(ticket).rows` holds is taken as the list of the page as it
now stands when at least six in ten of its ids are that list's; otherwise the
row takes the choice the answer marks (or what was asked) and the page's lists
are forgotten (`data.forget()`) and read again. So an acting row may answer
with its own list, with the main list or with a notice, as the provider does
today, and nothing has to change there.

### The main list `settings`

"Tile" is where the grouping table (`settings_model.cpp`, `kRules`) shows the
row; a row it does not know is shown on the tile "More". Rows marked
*container* stand for a catalog a tile's page reads: they are not listed, only
their `value` is used for the tile's line.

| Row | Kind | Proposed `label` | Facts beside `label`, `value` | Tile |
| --- | --- | --- | --- | --- |
| `settings:quality` | `list` | Maximum quality | `index`, `count` (8) | Player |
| `settings:frame-rate` | `list` | Frame rate | `index`, `count` (3); value "Automatic", "30 fps", "60 fps" | Player |
| `settings:codec` | `cycle` | Video codec | value "Automatic", "AVC", "VP9", "AV1", "HEVC" | Player |
| `settings:ask-stream` | `switch` | Choose the stream before playback | `on` | Player |
| `settings:playback-mode` | `list` | At the end of a video | `index`, `count` (7) | Player |
| `settings:queue-respects-mode` | `switch` | Queue follows the playback mode | `on` | Player |
| `settings:sleep-timer` | `slider` | Sleep timer | `index`, `count` (21) | Player |
| `settings:speed` | `slider` | Speed | `index`, `count` (the profile's speed list) | Player |
| `settings:speed-mode` | `list` | Remember speed for | `index`, `count` (4) | Player |
| `settings:player-ok` | `list` | Cross while the controls are hidden | `index`, `count` (4) | Player |
| `settings:player-hide` | `slider` | Hide the controls after | `index`, `count` (16) | Player |
| `settings:seek-interval` | `slider` | Seek step | `index`, `count` (10) | Player |
| `settings:player-seek` | `list` | Seeking | `index`, `count` (3) | Player |
| `settings:time-display` | `list` | Time shown | `index`, `count` (3) | Player |
| `settings:player-chat` | `cycle` | Live chat beside the video | value "Off", "Left", "Right" | Player |
| `settings:video-rotate` | `list` | Rotation | `index`, `count` (4) | Player |
| `settings:video-mirror` | `switch` | Mirror the picture | `on` | Player |
| `settings:video-aspect` | `list` | Aspect ratio | `index`, `count` (11) | Player |
| `settings:pixel-ratio` | `list` | Display pixel ratio | `index`, `count` (6) | Player |
| `settings:video-zoom` | `list` | Sizing and zoom | `index`, `count` | Player |
| `settings:extra:player` | *container* | | | Player |
| `settings:captions` | `switch` | Show captions | `on`, `preview` = `caption.on` | Captions |
| `settings:languages` | `list` | Language | `index`, `count` | Captions |
| `settings:automatic` | `switch` | Automatic captions | `on` | Captions |
| `settings:extra:captions` | *container* | | value "120 % · box" | Captions |
| `settings:audio-languages` | `list` | Audio language | `index`, `count` | Audio |
| `settings:extra:audio` | *container* | | value "Auto volume on · 100 %" | Audio |
| `settings:interface-language` | `list` | Interface language | `index`, `count` | Content & language |
| `settings:content-language` | `list` | Content language | `index`, `count` | Content & language |
| `settings:content-country` | `list` | Content country | `index`, `count` (paged list) | Content & language |
| `settings:hide` | `page` | Hide content | value: what is hidden, in a line | Content & language |
| `settings:thumbnails` | `list` | Thumbnails | `index`, `count` (4) | Content & language |
| `settings:channel-sort` | `list` | Order of subscribed channels | `index`, `count` (3) | Content & language |
| `settings:search` | `page` | Search options | value: the filters that are set | Content & language |
| `settings:history` | `list` | Watch history | `index`, `count` (3) | Content & language |
| `settings:search-history` | `switch` | Search history | `on` | Content & language |
| `settings:clear-history` | `action` | Clear local history | `danger`, `confirm`, `confirm_label`, `done` | Content & language |
| `settings:clear-queue` | `action` | Clear play queue | `confirm`, `confirm_label`, `done` | Content & language |
| `settings:ui` | *container* | | value "Afterglow · sounds on" | Interface |
| `settings:sections` | `page` | Menu sections | | Interface |
| `settings:boot-section` | `list` | Start in | `index`, `count` (17) | Interface |
| `settings:sponsor` | `switch` | Skip segments | `on` | SponsorBlock |
| `settings:sb` | *container* | | value "Skipping 3 categories" | SponsorBlock |
| `settings:dearrow` | *container* | | value "Titles on · thumbnails off" | DeArrow |
| `settings:accounts` | *container* | | value "Mara · 2 profiles" | Accounts |
| `settings:account-on-boot` | `switch` | Choose account at startup | `on` | Accounts |
| `settings:remote` | *linked* (package F) | | value "Mara's phone is linked" | Phone & casting |
| `settings:backup` | *container* | | value "Last backup today, 09:12" | Backup |
| `settings:about` | *container* | | value "ProsperoTube 0.1.0"; `update`, `attention` | Updates, About |
| `settings:experimental` | `page` | Experimental features | `tag` | About |

### Rows asked for that the provider does not list yet

| Row | Kind | Why |
| --- | --- | --- |
| `settings:reset:player`, `:captions`, `:audio`, `:content`, `:interface`, `:sponsorblock`, `:dearrow` (main list) | `reset` with `confirm` | boards 31 and 32: "Reset a section" (Options on a tile) and "Defaults" (Triangle on a page). Each puts the preferences of that tile back to their defaults for the profile in use. Without the row the hint is not shown and the button answers as the end of a list does |
| `settings:interface:sounds` (`switch`), `settings:interface:volume` (`slider`, 0 to 10), `settings:interface:playback-sounds` (`list`), `settings:interface:quiet-hours` (`list`), `settings:interface:vibration` (`switch`), in `settings:ui` | | sound-design.md section 6 and design-notes.md section 9; board 31 shows "Afterglow · sounds on" |
| `settings:interface:reduce-motion` (`switch`), `settings:interface:attract` (`list`: Off, 45 s, ...), in `settings:ui` | | design-notes.md sections 8 and 12 |
| `settings:interface` (`cycle` or `switch`): Classic / Afterglow, in `settings:ui` | | the Interface switch of ARCHITECTURE section 1; the table already places it first under "Look" |

### The catalogs under it

Every row of a list's catalog (`settings:quality`, `settings:languages`,
`settings:ui:scheme`, ...) is a `choice` with `label` and `selected`; where
the list feeds a preview the choices carry `preview` and the facts it reads.

| Catalog | Rows | Kind and facts |
| --- | --- | --- |
| `settings:extra:captions` | `settings:subtitle-size` | `slider`, `index`, `count` (20), `number` (percent), `preview` = `caption.size`, `preview_art_url` |
| | `settings:subtitle-style` | `list`, `index`, `count` (7), `preview` = `caption.style`, `text_rgb`, `background_alpha`, `edge`; short labels are wanted ("Box", "Yellow, shadow") |
| | `settings:subtitle-position` | `slider`, `index`, `count` (42), `number` (per mille, `-1` for the player's own), `preview` = `caption.position` |
| | `settings:more-subtitles`, `settings:caption-memory` | `switch`, `on` |
| `settings:subtitle-size`, `-style`, `-position` | `...:<n>` | `choice`, `selected`, the same `preview`, `number` or style facts per choice |
| `settings:extra:audio` | `settings:player-volume` (`slider`, 61), `settings:auto-volume`, `settings:player-mute` (`switch`), `settings:audio-delay` (`slider`, 29), `settings:audio-delay-enabled` (`switch`; `disabled` while the delay is 0) | |
| `settings:extra:player` | `settings:player-clock`, `:quality-info`, `:quality-bitrate`, `:loop-shorts`, `:remember-short-position`, `:player-suggestions`, `:chapter-notification`, `:error-fixer`, `:remember-quality`, `settings:use-section-playlist`, `settings:dislikes` | `switch`, `on` |
| | `settings:time-format`, `:player-exit`, `:keys-left-right`, `:keys-up-down`, `:seek-preview`, `:speed-list`, `:video-buffer` | `list`, `index`, `count` |
| | `settings:extra:buttons`, `settings:extra:dimming` | `page`, value as a summary ("18 of 21 shown") |
| `settings:extra:buttons` | `settings:button-<name>` | `switch`, `on` |
| `settings:extra:dimming` | `settings:dim-timeout`, `:dim-amount`, `:screen-off-timeout`, `:screen-off-amount` | `slider`, `index`, `count` |
| `settings:ui` | `settings:ui:scheme` | `list`, `index`, `count`, `preview` = `theme`, `look` (or `background`, `accent`) |
| | `settings:ui:scale`, `settings:ui:grid` | `slider`, `number` (percent), `preview` = `card.scale`, `card.grid`; one of them `preview_art_url` |
| | `settings:ui:title-lines` | `slider`, `number` (1 to 4), `preview` = `card.title_lines` |
| | `settings:ui:subtitle-lines` | `switch`, `on` (two lines), `preview` = `card.subtitle_lines` |
| | `settings:ui:rounded` | `switch`, `on`, `preview` = `card.corners` |
| | `settings:ui:title-scroll` (`switch`), `settings:ui:scroll-speed` (`slider`), `settings:ui:exit` (`list`), `settings:menu` (`page`) | |
| `settings:ui:scheme` | `settings:ui:scheme:<n>` | `choice`, `selected`, `preview` = `theme`, `look`; `settings:ui:scheme:classic` is a `page` |
| `settings:menu` | `settings:menu:<key>` | `order`, `on` (shown), `up` = `settings:menu:<key>:up`, `down`, `toggle`; `settings:menu:reset` is the page's `reset` |
| `settings:sections` | `settings:section:<n>` | `order`, `on`, `section` ("Main menu", "Discover"), `up`, `down`, `toggle` |
| | `settings:pin:<id>` | `order`, `section` ("Pinned"), `up` = `settings:pin:up:<id>`, `down`, `toggle` = `""` (Triangle opens its page: unpin, rename) |
| `settings:hide` | `settings:hide:<flag>` | `switch`, `on`; `settings:hide:reset` is the page's `reset` |
| `settings:search` | `settings:search:date`, `:duration`, `:type`, `:order`, `:feature:<name>` | `cycle`; `settings:search:cards` `switch`; `settings:search:reset` `reset`; `settings:ui:search-exit` `list` |
| `settings:sb` | `settings:sb:action:<category>` | `cycle`, value "Skip silently", "Skip with message", "Ask before skipping", "Do nothing" |
| | `settings:sb:minimum` | `slider`, `index`, `count` (41) |
| | `settings:sb:repeat`, `settings:sb:paid`, `settings:sb:alt` | `switch`, `on` |
| | `settings:sb:exclusions`, `settings:sb:colors` | `page`, value as a summary |
| | `settings:sb:server` | `text`, value (the host), `text` (the address to edit) |
| | `settings:sb:about`, `settings:sb:status` | `info`, value (the address) |
| `settings:sb:exclusions`, `settings:sb:colors` | `settings:sb:exclude:<channel>`, `settings:sb:color:<category>` | `switch`, `on` |
| `settings:dearrow` | `settings:dearrow:titles`, `:thumbnails` | `switch`, `on` |
| | `settings:dearrow`, `:about`, `:status` | `info`, value |
| `settings:accounts` | `settings:separate-settings` | `switch`, `on` |
| | `settings:lock:set`, `:change`, `:remove` | `text`, `secret`, `tag` |
| | `account:notice` | `info` |
| `settings:backup` | `settings:backup:create` | `action`, `done` |
| | `settings:backup:list`, `settings:backup:import` | `page`, value ("3 kept") |
| | `settings:backup:auto:<days>` | `cycle`, value "Off", "Daily", "Weekly", "Monthly" |
| | `account:notice:backup`, `account:notice:backup-accounts` | `info` |
| `settings:backup:list` | `settings:backup:entry:<name>` | `page`, value (its size) |
| `settings:backup:entry:<name>` | `settings:backup:restore:<ticket>:<name>`, `settings:backup:delete:...` | `action`, `confirm`, `danger` on delete, `done` |
| `settings:about` | `settings:update:check`, `:install`, `:cancel`, `:apply`, `:discard`, `:skip` | `action`, value (the state in a few words), `attention` on the one to take, `done` |
| | `settings:update:notify` | `switch`, `on` |
| | `settings:update:blocked` | `info` |
| | `description:update`, `settings:about:credits`, `settings:about:changelog`, `link:about`, `link:feedback` | `link`, `screen` = `reader` |
| | `account:notice:version` | `info` |
| `settings:experimental` | `account:notice:experimental:<n>` | `info` |

## 7. Interface texts

Settings shows 89 texts through `tr()`, 77 of them new to the catalogs: the
names of the tiles and the headings (plain literals in the tables of
`settings_model.cpp`, shown through `tr()` where they are drawn, as `frame.cpp`
keeps its rail entries), the hints, the states and the sample texts of the
previews. `--lang de` and `ru` fail on them until the catalogs are filled.
Board 43 has the German of the twelve tiles: Wiedergabe, Untertitel, Audio,
Inhalte und Sprache, Benutzeroberfläche, SponsorBlock, DeArrow, Konten,
Smartphone und Streaming, Sicherung und Wiederherstellung, Aktualisierungen,
Info.

## 8. Seen while testing, not this package's to change

- The pinned kit's baked faces have no ä, ö, ü, ß: German texts draw those
  letters as "?" in the host pictures (`settings-home-long`,
  `settings-captions-long`) until the text engine replaces `text_kit.cpp`.
- `FixtureSource::profile()` reads `profile.json` from the disk on every call,
  and the frame's rail asks every frame. Settings asks once, in `enter()`.
