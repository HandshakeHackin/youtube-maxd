# Afterglow: architecture and the brief for building screens

This is how the Afterglow interface is put together inside ProsperoTube and
what somebody who builds one of its screens has to know. The design itself is
in [design-notes.md](design-notes.md) (grid, type, colour, focus, motion,
controller map), [sound-design.md](sound-design.md) and
[feasibility-report.md](feasibility-report.md); the decision to build it is in
[../DECISIONS.md](../DECISIONS.md).

Read sections 1 to 4 to understand the whole, section 5 before touching data,
and sections 8 to 11 before writing a screen. Section 12 lists what is still
to build and who owns which files.

## 1. What exists and what does not

| Part | State |
| --- | --- |
| UI kit pinned as a build input (`scripts/ui_kit.py`, `scripts/ui_kit.lock.json`) | done |
| Looks, design language, text seam, picture cache, data layer, screen interface, frame | done, on the host |
| Host harness: boards, fixtures, pictures, checks (`scripts/test_afterglow.sh`) | done |
| Placeholder for every screen; the reader as the worked example | done |
| Home and the opening (boards 02 to 08 of the proposal), 22 boards | done, on the host |
| The text engine behind the kit's fonts (`native/text`, `text_engine.cpp`), boards of other scripts | done, on the host |
| `ProviderSource`: the data layer on the shipping provider (`scripts/test_afterglow_data.sh`) | done |
| Row facts (section 5.5): the store, and a video's `views`, `published`, `channel_art_url`, `badges` from the service | done |
| Row facts of the other catalogs of section 5.5 | open: with the screen that needs them, through the frame owner |
| The mount in the app (`app/mount.cpp`, `scripts/afterglow_overlay.py`) and the Interface preference | built for the PS5; never run on a console |
| The console session driver for the new snapshot (`tools/console`) | open (frame owner) |
| Every other screen (section 12) | open: one work package each |
| The player's controls | stay the existing ones in this stage |

Until every screen exists the RmlUi browser stays the interface people get:
**Interface: Classic** is the default and Afterglow is a preview behind the
switch.

Decisions that are taken and not reopened here: the interface is built on the
UI kit in slices; the kit is a pinned build input, never copied into this
repository and never modified for ProsperoTube; Afterglow is the default look
with Onyx, Daybreak and High contrast as the same design in other palettes; the
proposal's controller map stands; the kit's existing sound sets are used until
new recordings exist; attract mode starts after 45 seconds; emoji and scripts
the fonts lack are removed from YouTube's texts until the text engine draws
them; `native/branding/` is not touched.

## 2. Files

```
native/afterglow/
  base.hpp            kit namespaces under short names, tr(), small helpers
  platform.hpp        what the interface needs from whatever runs it
  text.hpp            the text seam: measure, fit, draw, paragraph
  text_kit.cpp        ... on the kit's baked fonts (replaced by the text engine)
  theme.hpp/.cpp      the grid, the type scale, the four looks
  design.hpp/.cpp     icons, panels, buttons, chips, tags, cards, the mark, hints, focus ring
  design_pieces.cpp   hand-overs, bones, the cards beside the video card, the page with nothing to show
  images.hpp/.cpp     pictures: fetch, decode, scale, upload, retire
  models.hpp          view models
  data.hpp/.cpp       Source (what the app is asked) and Data (what screens read)
  data_rows.hpp/.cpp  a catalog row as it arrives and what it becomes
  data_feed.hpp/.cpp  a catalog read page by page (Feed), an action followed to its answer (Act)
  data_fixture.hpp/.cpp  Source from files (tests)
  screen.hpp          Screen, Route, Phase, Nav, contexts, the factories
  frame.hpp/.cpp      the stack, hand-overs, input routing, attract mode
  frame_draw.cpp      page ground, rail, header, hints, snapshot
  sheet.hpp/.cpp      the frame's sheets: a row's actions, a text that is typed
  strings.cpp         the running platform, tr(), screen names
  screens/
    stubs.cpp         a weak placeholder for every screen
    reader.cpp        the worked example
    <name>.cpp        one file per screen
  app/                only in the app, never in the host build
    mount.cpp         the C entry points EVO calls; the app's Platform
    data_provider.cpp Source on the shipping provider
tests/afterglow/
  host_main.cpp       the harness
  art.hpp/.cpp        pictures painted from arithmetic (no real thumbnails anywhere)
  boards/<name>.json  boards: what to bring up, how to drive it, what must hold
  fixtures/<set>/     profile.json and catalog/<catalog>.json
  tools/              the generators of the fixtures and of the settings boards
scripts/
  ui_kit.py, ui_kit.lock.json   the kit pin
  afterglow_host.sh             build and run the harness
  test_afterglow.sh             the check `scripts/test_all.sh` runs
docs/afterglow/       this document and the proposal's three
```

Everything is C++20, as the kit is, in namespace `afterglow`. The host build
takes every `.cpp` under `native/afterglow/` except `app/`; the app's build
takes all of them. Nobody edits a list of files to add one.

## 3. Layers

```
                 the app (EVO)                       the host harness
   mount.cpp: AGC batch, EVO's network,       host_main.cpp: the kit's GL renderer,
   i18n, input; ProviderSource                files, painted pictures; FixtureSource
            \                                           /
             Platform  (textures, fetch, assets, clock, language, log)
             Source    (catalog pages, profile, play)
                              |
   Frame ---- owns ---- Data, ImageCache, the stack of Screens
     |                        |
     |   update(input, dt)    |   Screen::update(input, UpdateContext)
     |   draw(world, ui)      |   Screen::draw(DrawContext, Phase)
     v                        v
   two gfx::DrawList (world, ui) -> the backend draws them
```

- **Platform** (`platform.hpp`) is the only door to the outside: textures,
  picture bytes, packaged assets, the wall clock, interface texts, the log.
- **Source** (`data.hpp`) is the only door to the provider: one page of a
  catalog, the browsing profile, a revision that changes when cached pages are
  stale, and `play`.
- **Frame** (`frame.hpp`) is the whole interface. Whatever runs it calls
  `update(input, dt)` and `draw(world, ui, glass)` once per frame and plays
  `feedback()` (sound cues and rumble).
- A **Screen** (`screen.hpp`) is one page. It reads view models from `Data`,
  pictures from `ImageCache`, draws with the pieces of `design.hpp`, and asks
  the frame for everything that is not inside its page through `Nav`.

A frame is two draw lists. `world` holds the page ground, the colour of the
focused picture and anything a frosted panel should blur; `ui` holds everything
else. On the host the kit's renderer blurs `world` into a glass texture between
the two; on the console EVO's AGC batch has no blur yet, `glass` is 0 and
`glass_panel()` draws the look's solid panel instead. A screen never tests
which backend it is on: it calls `glass_panel()` and gets what there is.

## 4. The frame

**The stack.** Screens live on a stack. `Nav::push(route, move)` opens a screen
above the current one, which stays alive underneath with its focus and scroll
position; `Nav::back()` removes the top and re-enters the one below with
`restored == true`; `Nav::root(route)` replaces the whole stack (what the
rail's destinations do). Circle on the last screen sets
`Frame::take_exit_request()`: the app applies the profile's "Circle on the main
menu" rule. The stack holds at most 24 screens.

**Hand-overs.** A `Move` names how a screen arrives: `cut`, `fade`, `push`
(from the right), `zoom` (a card's picture grows into the next screen's
picture; `carried` is the card's rectangle). The way back is the reverse and
the frame chooses it. While a hand-over runs both screens are drawn, each with
its `Phase`: `in` runs 0..1 on the arriving screen, `out` runs 0..1 on the
leaving one. Durations are the frame's (`frame.cpp`, `move_seconds`; the motion
table is section 8 of the design notes). A screen takes input from its first
frame: nothing waits for a transition.

**What the frame draws for a screen.** The page ground and its wash, the rail,
the page title (from `Screen::title()`), the clock, the hint bar (from
`Screen::hints()`), toasts and the dialog. A screen that returns
`shows_rail() == false` gets none of these (opening, sign-in).

**Input routing.** One `hui::InputFrame` per frame goes to exactly one of: the
dialog when one is open; the rail when it is open; the top screen otherwise.
Every other screen is updated with an empty input and `context.top == false`.
A screen opens the rail by calling `nav.open_rail()` when Left is pressed at
its left edge, and leaves by calling `nav.back()` on Circle; the frame does
neither for it.

**Attract mode.** After `kAttractAfter` (45 s) without input the frame calls
`attract(true)` on the top screen; a screen that has an attract mode returns
true and shows it. The next press calls `attract(false)` and is swallowed.

**The page's colour.** A screen calls `nav.ambient(picture_address)` when its
focus moves to another picture. The frame cross-fades a small blurred copy of
that picture over the page ground (`Look::wash` says how strongly; High
contrast has none).

**Questions, sheets and typed text** are the frame's, as toasts are; a screen
asks through `Nav` and the frame gives them the input while they are up, in
this order: the dialog, the text sheet, the actions sheet, the rail, the screen.

- `nav.confirm(title, body, label, answered)`: two answers, the first confirms.
- `nav.ask(Question, answered)`: one to three answers of the screen's own, a
  destructive one in the warning colour, the focus on the safe one, stacked
  when the answers are sentences. `answered(index)`, -1 when put away.
- `nav.ask_text(TextQuestion, answered)`: the kit's keyboard in a sheet (Cross
  types, Square deletes, Triangle is a space, Options takes the text, Circle
  puts the sheet away). The screen hands the text on with
  `data.act(row, text)` and keeps no copy of a secret.
- `nav.actions(row, about, changed)`: the actions of a video, a channel, a
  playlist or a comment as the sheet of board 14 (Touchpad wherever such a
  thing has the focus). The frame reads `data.menu(row, about)`, runs the
  chosen row, asks first where something is taken away, toasts the result and
  opens the page a row names. `changed` runs once after an action went
  through: the screen forgets the lists it shows.
- `nav.back_to(screen)`: leaves for the nearest such screen below.

A screen that has a layer of its own over the whole page (the video page's
More actions and Share) draws it in `Screen::draw_over()`, after the frame's
furniture, and says how much it covers in `over()`; the frame's hint row gives
way to it.

**Picture to player.** `play_video(context, video, rectangle)` (or
`nav.play`) hands the video to the player and lets its picture grow from that
rectangle to the whole screen (0.62 s), where it stands until the player has
its first frame; `Frame::resume()` returns it to where it came from (0.5 s).
`Frame::launch_picture()` is that picture's address, for the player.

**The header** is the screen's choice (`Screen::header()`): `title` (the page
title, the line under it from `subtitle()`, the clock), `clock` (the clock
alone: the page draws a head of its own), `on_picture` (the clock on a glass
plate, 28 pixels inside the picture that is the page's head).

**Stats.** `Frame::stats()` and the snapshot carry, per frame: milliseconds
spent in update and in draw, the caller's frame time and the worst of the last
120, shapes and runs (draw calls), `fill` (drawn area over screen area: how
many times the screen was covered), pictures ready, pending and their bytes,
data requests pending. The app writes them into its dev status file.

## 5. Data

### 5.1 The rule

A screen reads; it never fetches. It calls `data.home()` or
`data.catalog(id, page)` every frame it needs them and looks at the `Load`
state: `loading` (draw the bones), `ready` (draw the content; it may be empty),
`failed` (the provider's message is in `error`), `offline` (the service cannot
be reached). The first call asks; later calls return what has arrived. Answers
are kept until `Source::revision()` changes (another profile, an account signed
in or out, the library edited, the content language changed) or the screen
calls `data.refresh()`.

### 5.2 Where the data comes from

Everything the classic browser shows comes from the YouTube provider
(`native/tube_provider.c`) as catalogs: a catalog has an id and pages of rows,
and a row has an id that is either a video to play or another catalog to open.
Afterglow uses the same catalogs through the same provider functions, so local
state (history, resume positions, hidden and blocked channels, DeArrow titles,
the profile's order of sections) is already applied when a row arrives. The
provider talks to the built-in service (`POST /v1/jobs` and its polls on the
loopback address; [../PROTOCOL.md](../PROTOCOL.md)); Afterglow never talks to
the service itself.

`ProviderSource` (`app/data_provider.cpp`) calls
`evo_provider_youtube.list_catalog(id, page, ...)` and turns the answer into a
`Page`. It keeps **one request in flight** and queues the rest in the order
they were asked: the provider's paging state belongs to one list at a time, and
its answers arrive on the main thread from `evo_net_poll()`. Home's three
shelves therefore fill one after the other; each shows its bones until its turn
comes.

### 5.3 A page and its rows

```cpp
struct Page {
    model::Load load; std::string error;
    std::vector<model::Row> rows;          // every row, in order
    std::vector<model::Video> videos;      // the rows that are videos, in order
    std::vector<model::Channel> channels;  // ... channels
    std::vector<model::Topic> topics;      // ... topics and sections
    bool more;                             // a further page exists
};
```

`add_row()` in `data_rows.cpp` is the one place that decides what a row is:

| Row | Becomes |
| --- | --- |
| id starts with `channel:` | `model::Channel` (id without the prefix, `row` with it) |
| a folder whose id starts with `section:` or `search:` | `model::Topic` |
| any other folder, and any row the provider wrote itself (`interface`) | `model::Row` only |
| everything else | `model::Video` |

A `model::Row` has `id`, `title`, `subtitle`, `overview`, `thumb`, `folder`,
`interface_text` and **facts**: every other named value the provider knows
about the row (`row.fact("likes")`, `row.has("pinned")`). Rows the provider
wrote are interface texts and arrive in the profile's language.

### 5.4 View models and what fills them

`models.hpp` has the full definitions. "Row" below means the provider's row for
the catalog (`evo_provider_item_t`); "fact" means section 5.5.

**`model::Video`**

| Field | Filled from |
| --- | --- |
| `id` | row `id`: what `Data::play()` and the video page open |
| `video_id` | row `id` when it is an 11-character YouTube id, else "" |
| `title` | row `title` (DeArrow's when the profile uses it) |
| `channel` | row `subtitle` (the service's `subtitle` of a video is the channel name) |
| `channel_id` | row `parent_id` (service `channel_id`) |
| `views` | fact `views` |
| `age` | fact `published` |
| `overview` | row `overview` (service `overview`: the description snippet) |
| `thumb` | row `art_url` (the profile's thumbnail source and DeArrow applied) |
| `avatar` | fact `channel_art_url` |
| `duration_s` | row `duration_sec`; 0 when live |
| `progress` | row `resume_pos_sec / duration_sec` (the profile's local position); else the service's `watch_percent / 100` |
| `watched` | row `played` |
| `badges` | `kBadgeLive` from row `is_live`; `kBadgeUpcoming` from fact `is_upcoming`; `kBadgeShort` from fact `is_short`; `kBadgeNew`, `kBadge4K`, `kBadgeHdr`, `kBadgeCaptions`, `kBadgeMembers` from the fact list `badges` |

**`model::Channel`**

| Field | Filled from |
| --- | --- |
| `id`, `row` | row `id` (`channel:UC...`) |
| `name` | row `title` |
| `subscribers` | row `subtitle` when YouTube wrote it; fact `subscribers` |
| `avatar` | row `art_url` |
| `handle`, `banner` | facts `handle`, `banner_url` (channel page only) |
| `subscribed` | fact `subscribed` |
| `unseen` | fact `unseen` (the provider's new-content marks) |

**`model::Topic`**: `id` and `title` from the row; `icon` from the id
(`topic_icon()`).

**`model::Profile`**: from the provider's selected profile
(`tube_browse_profile()`): `id` (`guest` or the profile's id), `name`, `guest`,
`signed_in` (a YouTube account is signed in on it).

**`model::Home`** (`Data::home()`, `data.cpp`): the profile; `shelves` in
display order, each a `model::Shelf` with its own `Load`; `pinned`, the rail's
channels; `invite_sign_in`.

| Profile | Shelf id | Title | Catalog | Kept |
| --- | --- | --- | --- | --- |
| signed in | `continue` | Continue watching | `history` | videos started and not finished, 12 |
| signed in | `recommended` | Recommended | `library:home` | 16 |
| signed in | `subscriptions` | From your subscriptions | `library:subscriptions` | 16 |
| Guest | `topics` | Browse by topic | `discover` | topics, 16 |
| Guest | `continue` | Continue watching | `history` | as above |
| Guest | `trending` | Trending | `section:trending` | 16 |

`pinned` are the `channel:` rows of the main menu (catalog `""`). A shelf whose
catalog answers with nothing is not shown. The hero is the focused video.

### 5.5 Facts: what the provider has to tell beyond a row

`evo_provider_item_t` cannot hold a view count, a comment's likes or a
channel's banner, and it is EVO's type, not ours to widen. The provider
therefore keeps **facts** beside the rows of the page it last listed
(`native/tube_row_facts.h`): named text values per row id, set while the
provider builds the page and read by `ProviderSource` in the same callback.

```c
void tube_row_facts_begin(void);                                   /* a new page is being built */
void tube_row_fact(const char *row, const char *name, const char *value);
const char *tube_row_fact_get(const char *row, const char *name); /* "" when unknown */
int tube_row_fact_at(const char *row, int index, const char **name, const char **value);
```

In a fixture a fact is simply a further member of the row's JSON object, so a
screen can be built and tested against facts before the provider produces them.
The names are a contract; these are fixed, and a work package that needs more
adds a line to this table in the same change that uses it:

| Catalog | Row | Facts |
| --- | --- | --- |
| any list of videos | a video | `views`, `published`, `channel_art_url`, `badges` (comma separated: `new,4k,hdr,cc,members`), `is_upcoming`, `is_short`, `upcoming_start_ms` |
| any list of videos | a video | also `published_days` (whole days since it was published, 0 for today: the subscriptions feed is grouped by it), `watched` (when it was watched, in `history`) |
| any list | the first row of the first page | `total` (how many rows the whole list has) |
| any list | a folder that is a filter or an order of the list | `chip` (`filter` or `sort`), `selected` |
| `search:<query>`, `search-with:<options>:<query>` | the `settings:search` row (or any row of page 0) | `results` (the count as it is to be shown) |
| `search:<query>`, `library:channels`, `subscriptions` | a channel | `subscribers`, `handle`, `subscribed`, `unseen`, `live` |
| `search:<query>`, `library:playlists`, `channel-tab:<id>:playlists` | a playlist (a folder `playlist:<id>` or `library:playlist:<id>`) | `count`, `owner`, `updated` |
| `search-suggest:<text>` | a completion | `detail` |
| `search-history` | the `account:notice` row | `state` (`off`, `empty`) |
| `channel:<id>` and `channel-tab:<id>:<section>` | first row `channel:<id>` | `handle`, `subscribers`, `banner_url`, `subscribed`, `tabs` (comma separated), `tab`, `sorts`, `sort`, `videos` (the count as YouTube words it), `notify` (`all`, `personalized`, `none`), `search` (`0`: no search in this channel) |
| `details:<video>` | first row | `views`, `published`, `likes`, `dislikes`, `channel`, `channel_id`, `channel_art_url`, `subscribers`, `subscribed`, `rating` (`like`, `dislike`, ""), `saved`, `chapters` (count), `duration`, `resume` (seconds), `segments`, `captions`, `fps`, `quality`, `live`, `upcoming` |
| `discussion:top:<video>`, `discussion:newest:<video>`, `discussion:replies:<cursor>` | a comment | `author`, `published`, `likes`, `replies`, `pinned`, `owner`, `member`, `hearted`, `liked`, `disliked`, `author_art_url` |
| `library:notifications` | a notification | `published`, `channel`, `channel_art_url`, `unread`; the row's `channel_id` |
| `library:playlist:<id>`, `playlist:<id>` | the first row when it is the list itself | `count`, `owner`, `updated` |
| `settings` and everything under it | a preference | the contract is `requests/e.md`, section 6 (`kind`, `label`, `value`, `on`, `selected`, `index`, `count`, ...) |
| `library:profiles` | a profile `library:profile:<id>` | `guest`, `signed_in`, `selected`, `locked`, `subscriptions` |
| `auth:start`, `auth:session`, `auth:cancel` | the first row that has it | `state`, `user_code`, `verification_url`, `expires_in` |
| `settings:remote` and its acting rows | `settings:remote:info`, `:code`, `:toggle`, `:device:<id>` | `state`, `available`, `value`, `selected`, `connected` (`requests/F.md`, 7) |

A video's own values (`views`, `published`, `channel_art_url`, `channel_id`,
`badges`, `is_live`, `is_upcoming`, `is_short`, `duration_sec`, `resume_sec`)
are also facts of its row, whatever the row became: `details:<video>`'s first
row is a folder and has them all the same. Every fact but an address is
cleaned of what no face draws. `model::Video::at` and `model::Channel::at` are
the row's place in `Page::rows`.

Where the service does not send a value yet (`views`, `published`,
`channel_art_url` and `badges` of a video are the first), the same change adds
it to the row the service builds (`companion/innertube/library.mjs`,
`normalizeVideo` and its siblings) and to what the provider reads
(`parse_item`). That bridge is one work package (section 12) so that screen
work never touches the provider.

### 5.6 Actions

Much of what a person does is a row of the provider that acts when it is
listed: a choice of a preference (`settings:quality:3`), an entry of an actions
menu (`account:confirm:like:<video>`), a local one (`local:video:...`). A
screen does not call the provider for these either:

```cpp
unsigned ticket = data.act(row_id);          // lists the acting row once, never from the cache
const Page &state = data.action(ticket);     // loading -> ready (rows: the provider's answer) or failed (error)
data.forget("library:watch_later");          // a catalog the action changed: the next read asks again
std::string menu = data.menu(row_id, title); // the catalog of a row's actions ("" for none): read it with catalog()
unsigned typed = data.act(row_id, text);     // a row that takes a typed text (a comment, an address, a name)
unsigned key = data.answer(text);            // ... or the prompt the provider armed already (a profile's password)
```

`Act` (`data_feed.hpp`) follows an acting row to its end, the provider's
second step included (`account:confirm:...` answers with `account:apply:<n>`,
which is listed in turn): `act.start(data, row)`, then `act.poll(data,
&answer)` every frame. `Feed` reads a catalog page by page into one list and
keeps what was read on screen while the data layer reads it again.

A typed text reaches the provider through the prompt it arms
(`tube_take_playlist_prompt`, `tube_submit_playlist_name`): the row is listed,
the text handed over, and the catalog the provider names next is the answer.
In a fixture set the answer is the row's own catalog file (`prompt:answer` for
the armed prompt), and a board lists what was handed over under `answered`
(`"<row>=<text>"`).

Show the pending state on the control that was pressed (the kit's spinner, a
dimmed label), the result as a toast (`nav.toast`), a refusal with the
provider's message. A finished action is kept for ten seconds; `action()` of a
ticket that is gone has `Load::idle`. In a fixture set an acting row is a
catalog file like any other (its rows are the answer), a row's menu is the
catalog `menu:<row>`, and a board lists the acting rows it expects under
`asked`. Questions with two answers go through `nav.confirm()`.

Playing a video is `data.play(video)`: the app's player takes over and the
frame's `resume()` runs when it closes.

## 6. Pictures and threading

Everything runs on the thread that runs the frame: `Frame::update` and `draw`,
every `Screen` function, every `Source` and `Platform` callback (the app
delivers them from `evo_net_poll()` on the main thread). Nothing in
`native/afterglow` starts a thread or takes a lock, and nothing may block.

`ImageCache::get(address, size)` returns a `Picture`: `texture` is 0 until the
picture is ready, then `age` counts up so a card can fade it in over its
placeholder. `tone` and `glow` are the picture's average and most vivid
colours (focus light, the page's wash). `cover(aspect)` is the texture
rectangle that fills a box without distortion. Sizes: `avatar` (96 pixels on
the long side), `card` (512), `hero` (1280), `ambient` (64 x 36, blurred).

The cache asks the platform for at most 4 addresses at once, most recently
asked first; decodes **one picture per frame** (JPEG and PNG, `stb_image`),
scaled down to its size class; keeps at most 48 MiB of textures, least recently
used first; and gives back a picture nobody asked for in 600 frames. If the
first console session shows the decode in the frame time, the decode moves to
one worker thread behind the same interface; nothing else changes.

A screen calls `get()` for exactly what it draws, every frame it draws it.
That is all the bookkeeping there is.

## 7. Text and languages

**The seam.** Screens and `design.cpp` never call a font. They call
`text()`, `paragraph()` (drawing, with a `G`), `measure()`, `fit()`,
`measure_block()`, `covers()` and `clean()` (`text.hpp`) with a `Face`
(`g.f.regular`, `g.f.semibold`, `g.f.display`, `g.f.mono`). Do not call
`ui::text`, `ui::paragraph`, `font->measure` or `DrawList::text` in a screen.

**The engine.** `native/text` ([TEXT.md](TEXT.md)) shapes every script with
HarfBuzz, FriBidi and FreeType into distance-field quads. The kit draws every
text through `gfx::Font`, whose functions are not virtual, so the builds
compile a patched *copy* of the kit's `gfx/font.hpp` and `gfx/font.cpp`
(`scripts/kit_text_patch.py`; the pinned kit is never touched) in which a font
asks an installed shaper first. `text_engine.cpp` is that shaper. With the
interface's four fonts bound to it, every text of every screen, of the design
language and of the kit's own components (keyboard, dialog, toasts) is
measured, cut, wrapped and laid out by the engine, with no change above
`text.hpp`. Whoever runs the frame owns the `TextEngine`: `begin_frame()`
before the frame's update, `end_frame()` after its draw. If the engine cannot
start, the kit's baked fonts draw as before and `clean()` removes what they
lack.

**Fonts** (a decision taken on the owner's behalf with the approved boards in
view; it is his to overrule): Latin, Greek and Cyrillic are drawn with the
faces the boards used (Inter, Inter SemiBold, Montserrat Medium, DejaVu Sans
Mono, carried as assets), and the console's system fonts
(`/preinst/common/font`) are the fallback chain for every other script; on the
host the stand-ins of the engine's tests take their place. Emoji are drawn
where a face has an outline for them and left out otherwise. A right-to-left
title is ordered and cut correctly but still hangs from the left edge of its
column: the kit aligns a line by its left edge.

**Interface texts** are written in English in the source and shown through
`tr("...")`. The English text is the key of the catalogs in `native/i18n/`.
`scripts/i18n_extract.py` collects every `tr("...")` of `native/afterglow`
into the template; a text that is missing from a catalog fails
`scripts/test_i18n.sh`. Texts with a number in them are built from a
translated pattern and the number (`"{0} videos"`), never by joining pieces.
Button names follow the classic interface: Cross, Circle, Triangle, Square,
Touchpad, Options (never "Select").

**YouTube's texts** (titles, names, counts, comments) are shown as they
arrive, never translated. Until the text engine exists they pass through
`drawable()` once in the data layer, which removes what no face draws.

## 8. The screen interface

```cpp
class Screen {
    virtual ScreenId id() const = 0;
    virtual void enter(const Route &route, UpdateContext &context, bool restored) = 0;
    virtual void leave(UpdateContext &context);
    virtual void update(const InputFrame &input, UpdateContext &context) = 0;
    virtual void draw(const DrawContext &context, const Phase &phase) const = 0;
    virtual void draw_over(const DrawContext &, const Phase &) const; // a layer over the frame's furniture
    virtual float over() const;                           // how much of the page that layer covers, 0..1
    virtual Header header() const;                        // title (default), clock, on_picture
    virtual std::string title() const;                    // the page title; "" shows the route's
    virtual std::string subtitle() const;                 // the line under it
    virtual std::span<const ui::Hint> hints() const;      // the hint bar
    virtual bool shows_rail() const;                      // false: the whole page is the screen's
    virtual bool attract(bool on);                        // true when the screen has an attract mode
    virtual FocusInfo focus() const = 0;                  // what has the focus, and where
    virtual std::vector<std::pair<std::string, std::string>> focusable() const;
    virtual void snapshot(Snapshot &out) const;           // the screen's own state, for tests and the console
};
```

Rules, each of which a check or a reviewer holds a screen to:

1. **`draw` is const and changes nothing.** It may run more than once per
   frame. Everything that moves is advanced in `update` with `context.dt`.
2. **`update` runs every frame the screen is visible**, also while it leaves
   or is covered; then `context.top` is false and the input is empty. Advance
   animations always; act on input only when `context.top`.
3. **`enter(route, context, restored)`.** With `restored == false` the route is
   new: reset everything. With `restored == true` the screen above has left:
   keep the focus and the scroll position, and read the data again (it may
   have changed).
4. **Focus is always somewhere and always visible.** `focus()` returns a stable
   id (`"continue/2"`: the shelf's id and the index; `"action/play"`), the
   text of the focused thing, its rectangle and its position in its row. The
   focused thing wears `focus_ring()` or the look's plate. When data arrives
   under a focus that no longer exists, the focus moves to the nearest thing
   that does.
5. **Edges.** Left at the left edge calls `nav.open_rail()`. A direction with
   nowhere to go plays `audio::Cue::error` once (not on repeats) and moves
   nothing. Circle calls `nav.back()`.
6. **Every state is drawn**: loading (bones in the shape of the content),
   ready, empty (say what is missing and what to do), failed (the provider's
   message), offline. A screen that reads two catalogs handles each one's
   state.
7. **Sound.** Focus moves play `Cue::focus`, a choice `Cue::select`, opening
   and leaving are the frame's. Section 9 of the design notes and
   `sound-design.md` say which cue goes where.
8. **No colours, radii or type sizes of its own.** Colours come from the
   `Look` (`g.k.ink`, `g.k.accent`, ...), sizes from `theme.hpp`, pieces from
   `design.hpp`. A piece that two screens need goes into `design.cpp` by the
   frame owner; ask.
9. **Layout is in virtual pixels**, 1920 x 1080, content between `kLeft` and
   `kRight`, the first line of content at 196, nothing below 968 but the hint
   bar. The renderer scales to 4K.
10. **No text is cut by the screen's edge and none overlaps**: fit every line
    with `fit()` to the width it has, give every paragraph a line limit.
    German and Russian are checked.

A screen never includes a header of EVO, of the provider or of another screen,
never calls the provider, the network, the file system or the clock, and keeps
no global state: two instances of a screen may be on the stack at once.

## 9. Adding a screen: the worked example

`native/afterglow/screens/reader.cpp` is a complete screen in 200 lines: long
text on one page. Read it beside this section; it is the pattern.

1. **The factory.** `screen.hpp` declares `make_reader()`. `stubs.cpp` defines
   every factory weakly as a placeholder page; `reader.cpp` defines
   `make_reader()` normally and the linker takes that one. No list is edited.
   A screen that is not in `ScreenId` yet is added to `screen.hpp`,
   `strings.cpp` and `stubs.cpp` by the frame owner.
2. **`enter`** keeps the route, resets the scroll and the age unless
   `restored`.
3. **`update`** reads `context.services.data.catalog(route_.argument)` every
   frame, lays the text out once when it arrives (measuring with
   `context.services.fonts`), advances the scroll spring, and, when
   `context.top`, turns the input into a scroll target, `nav.open_rail()` or
   `nav.back()`.
4. **`draw`** fades with its `Phase`, draws bones while loading, the message
   when the load failed, and otherwise the paragraphs inside a clip rectangle
   with a scroll thumb.
5. **`title`, `hints`, `focus`, `focusable`, `snapshot`** describe the screen
   to the frame, the tests and the console.
6. **Fixtures**: `tests/afterglow/fixtures/guest/catalog/settings_about_credits.json`
   is the catalog `settings:about:credits` as the provider answers it.
7. **Boards**: `tests/afterglow/boards/reader.json` brings the screen up on
   that fixture, scrolled, at the end of the text, while loading, offline, and
   leaving through the rail, each with what must hold.

Run `bash scripts/test_afterglow.sh`. Pictures of every board in the four
looks are in `build/afterglow/pictures/<look>/<board>.png`: look at them.

## 10. Tests

**Fixtures.** A set is a directory under `tests/afterglow/fixtures/`:
`profile.json` and `catalog/<name>.json`, where `<name>` is the catalog id with
every character that is not a letter, a digit, `-` or `_` written as `_`,
`root` for the main menu, and `.pN` appended for page N > 0. A page is
`{"items":[row...],"has_more":false}`; a row has the service's own names
(`id`, `title`, `subtitle`, `overview`, `art_url`, `channel_id`,
`duration_sec`, `resume_sec`, `watch_percent`, `is_folder`, `is_live`,
`played`, `interface`) and any facts. Pictures are painted, never downloaded:
`"art_url": "art://<scene>/<palette>/<seed>"` with the scenes of
`tests/afterglow/art.hpp` (`ridges`, `city`, `synth`, `space`, `ocean`,
`aurora`, `stage`, `dunes`, `blobs`, `globe`, `desk`, `plate`, `pitch`,
`code`); `broken://x` is an address that fails. Every name, title and number
in a fixture is invented: no real channel, video, song or product.

**Boards.** `tests/afterglow/boards/<file>.json` holds `{"boards":[...]}`.
One board:

```json
{
 "name": "reader-scrolled",
 "fixture": "guest",
 "route": {"screen": "reader", "argument": "settings:about:credits", "title": "Credits"},
 "source": {"delay": 3, "hold": false, "offline": false},
 "opening": false, "reduced_motion": false, "attract_after": 45,
 "settle": 1.5,
 "steps": [{"press": "down"}, {"press": "down", "wait": 0.35}, {"wait": 1.0}],
 "expect": {"screen": "reader", "focus": "text", "rail_open": 0,
            "state": {"scroll": "216"}, "asked": ["settings:about:credits"], "played": ""}
}
```

`steps` are presses (`up`, `down`, `left`, `right`, `cross`, `circle`,
`triangle`, `square`, `l1`, `r1`, `l2`, `r2`, `l3`, `r3`, `options`,
`touchpad`) and waits in seconds on a fixed 60 Hz clock; `{"hold": "left",
"wait": 1.5}` keeps a button down for the wait (a held direction repeats after
0.4 s, every 0.1 s). `expect` compares the frame's snapshot: `screen`,
`argument` (the top route's), `focus` (the focus id), any top-level number
(`rail_open`, `moving`, `attract`, `dialog`, `sheet`, `typing`, `launching`),
`state` (values of `Screen::snapshot`; the frame's sheets add `sheet`,
`sheet_load`, `sheet_level`, `sheet_acting`, `typing`, `typed`), `asked`
(catalogs that must have been requested), `answered` (typed texts handed
over, `"<row>=<text>"`), `played` (the last video handed to the player). A
board may say who is browsing (`"profile": {"id", "name", "guest",
"signed_in"}`) and for how long the harness's player plays before it closes
(`"player"`, seconds, default 1). Name boards `<screen>-<state>`.

**What every board is held to, in every look**, without being asked: something
has the focus; its rectangle is on the screen; no glyph crosses the edge of the
screen outside a clip rectangle; the page is not empty; the picture has more
than a handful of colours; the snapshot is valid JSON. The looks themselves are
held to 4.5:1 for every text colour on its ground. A board may name `pixels`
(points of the output, in virtual pixels, with the colour expected there). A
run with `--lang de` shows an interface text the catalog lacks in English and
counts it (`--strict-lang` fails on it); `--snapshots` writes every board's
snapshot beside its picture.

**The provider itself** is tested in `tests/afterglow/provider/provider_test.cpp`
(`scripts/test_afterglow_data.sh`): `ProviderSource` and `Data` on the real
`native/tube_provider.c`, with the network replaced by documents written in the
test, as `tests/test_native.c` does. A fact that the provider starts to produce
gets an assertion there.

**Commands.**

```bash
bash scripts/test_afterglow.sh                                  # everything, four looks
bash scripts/afterglow_host.sh out --only reader --look daybreak
bash scripts/afterglow_host.sh out --lang de --look afterglow
```

A screen's work is not done until its boards cover: the first state a person
sees, every kind of focus it has, each edge, loading, empty, failed, offline,
and the way in and out; and until its pictures have been looked at in all four
looks and in German.

## 11. Names and ownership

- Files: `screens/<name>.cpp` where `<name>` is the screen's name in
  `screen_name()`. A screen that needs helpers of its own keeps them in an
  anonymous namespace in its file, or in `screens/<name>_<part>.cpp` and
  `screens/<name>.hpp` when the file grows past about 800 lines.
- Types `CamelCase`, functions and variables `snake_case`, members with a
  trailing underscore, constants `kCamelCase`, as the kit writes them.
- Focus ids: `<group>/<index or name>`, lower case, stable across data
  arriving: `continue/2`, `tab/videos`, `action/play`, `row/quality`.
- Snapshot values: lower-case names, values as text.
- Board names: `<screen>-<state>`; fixture sets: who is browsing (`guest`,
  `signed-in`, `empty`, `long-texts`).
- Comments say what the code is for; no names of games, creators or brands
  anywhere, in code, fixtures or pictures.

**A screen's work package owns** its `screens/<name>*.cpp|hpp`, its
`tests/afterglow/boards/<name>*.json` and the fixture catalogs it adds (new
files; an existing fixture file is extended only by adding rows at the end or
facts to rows).

**It does not edit** `frame*.cpp|hpp`, `screen.hpp`, `design.*`, `theme.*`,
`text*`, `images.*`, `data*.{hpp,cpp}`, `models.hpp`, `stubs.cpp`,
`strings.cpp`, anything under `app/`, `tests/afterglow/host_main.cpp`, the
provider, the service, EVO's overlays or the build scripts. When a screen
needs something from one of these (a piece of the design language, a field of
a view model, a fact, a new `Nav` request), the request goes to the frame
owner, who makes the change once for everybody. Interface texts are the one
shared thing a screen adds to: new `tr()` texts go into the 21 catalogs in the
same change (`scripts/i18n_extract.py`, then the catalogs, then
`scripts/i18n_build.py`).

## 12a. In the app: the mount, the preference, the snapshot

`scripts/afterglow_overlay.py` (called by `prepare_native.py`) copies
`native/afterglow` and `native/text` beside EVO's kit screens, adds them to
EVO's Makefile, ships the four faces as assets and hooks EVO in three places:

1. **Drawing.** `EvoRmlProviderHost::Render` asks `evo_hui_render_afterglow()`
   first for the YouTube provider. EVO's kit application (the owner of the AGC
   batch and of the fonts) calls `afterglow::mount_render(batch, fonts, dt,
   width, height)` (`app/mount.cpp`), which runs one step of the frame and
   draws its two lists. When it returns false the classic browser draws.
2. **The pad.** `ProviderHostScreen::handleInput` gives the pad to
   `tube_afterglow_input()` while the interface is shown. A video chosen with
   `Data::play()` is offered as the classic browser offers one
   (`evo_rmlui_provider_offer_selection`) and `startSelected()` resolves and
   plays it: the player and everything in it are the existing ones. Circle on
   the interface's last screen falls through to the classic rule for Circle on
   the main menu.
3. **The report.** `evo_hui_dev_focus_json()` answers with the document name
   `afterglow` and `Frame::snapshot_json()` while the interface is shown.

**The preference.** YouTube preferences > Interface > **Interface**:
**Classic** (the default) or **Afterglow (preview)**; stored per profile as
`interface`. For a console session, a file `afterglow.on` in the app's data
folder (`/data/prosperotube`) turns the interface on whatever the preference
says, and `afterglow.off` keeps the classic browser; both are looked at once a
second, so a session can switch without restarting the app.

**The snapshot** is one JSON object: `interface` (`afterglow`), `look`,
`screen`, `stack` (screens with their arguments), `moving`, `rail_open`,
`attract`, `dialog`, `focus` (`id`, `text`, `index`, `total`, `rect`),
`focusable` (ids and texts), `toasts`, `state` (the screen's own values and
`lists`), `stats` (`frame_ms`, `worst_frame_ms`, `update_ms`, `draw_ms`,
`instances`, `runs`, `fill`, `pictures_ready`, `pictures_pending`,
`picture_bytes`, `data_pending`). Home: `focus.id` is `<shelf>/<index>`,
`state.row` the focused shelf, `state.lists["shelf:<id>"]` the titles.

**What the console has not shown yet**, in the order to look: the frame at all
(the batch drawing two lists a frame, the fonts bound to the shaper); glyph
pages (the engine's atlas goes into ordinary RGBA textures, re-sent whole when
glyphs are added, because the batch has no one-channel update; 1024-pixel
pages keep that at 4 MiB a time); the system fonts being found and read from
`/preinst/common/font`; pictures through EVO's network and `stb_image`, one
decode a frame; panels without the blur (solid, as designed for this backend);
frame time and `fill` at 4K; the clock's time zone; sounds (cues are collected
by the frame and not played yet).

## 12b. The player package

The player stays EVO's `PlayerScreen` with ProsperoTube's overlays: it owns
playback, the pad and every in-player menu. Its picture is already a kit
screen: `evo::kit::OsdScreen` (`ui_kit/src/evo_hui_osd.cpp`), fed through
`evo_hui_update_playback()` and drawn by `evo_hui_render_playback_osd()`, with
ProsperoTube's additions patched into it (sponsor markers, chapters, the seek
preview, player chat, the accent of the theme). The Afterglow player is a
second way to **draw** that same state, not a second player:

- **Mount:** in `evo_hui_render_playback_osd()`, when
  `tube_afterglow_enabled()`, call `afterglow::player_render(batch, fonts, dt,
  width, height, params)` instead of `OsdScreen::draw`; the same for the
  pickers and sheets EVO draws with the kit (`SubtitlePicker`, the dialog, the
  toast). The pad is untouched.
- **View model:** `player/view.hpp`: one `PlayerView` filled from
  `evo_playback_osd_params_t` and the accessors the overlays already use
  (`tube_player_extra.h`, `tube_sponsor_draw.h`, `tube_seek_sheets.h`,
  `tube_caption_track_*`, `tube_speed_*`, the player menu's rows). The first
  task of the package is to read those and write the struct down here.
- **Files it owns:** `native/afterglow/player/*.cpp|hpp`, the boards
  `tests/afterglow/boards/player*.json` and fixtures
  `tests/afterglow/fixtures/<set>/player/*.json` (a `PlayerView` as JSON; the
  harness draws it over a painted frame).
- **It uses** the looks, `design.hpp`, the text seam and the kit's components
  as screens do, and the frame's motion and sound rules (design notes 8 and
  9, boards 16 to 24).
- **It must keep** everything the overlays draw today: sponsor segments on the
  timeline, chapters, the seek preview thumbnail or carousel, the chat column,
  the paid-content notice, screen dimming.
- **Open, for the frame owner:** the hand-over picture from the video page to
  the player (`Move::zoom` ends at the screen's rectangle; the player begins
  with the same picture) needs the frame and the player to share the last
  thumbnail; it follows once package B's video page exists.

## 12c. Decided, and still open

**Home against board 03.** Shelves are named as the board names them
(Continue watching, Recommended for you, New from your subscriptions; Guest:
Browse by topic, Trending now). The hero shows the 4K and CC chips when the
video carries them. Differences that stand until their part exists: Square
opens Search, not Watch later (an account action is a confirm row and then an
apply row of the provider; it arrives with the shared actions sheet); the bell
has no count (the provider does not say how many notifications are unread:
fact `unread` of `library:notifications`); the board's fourth shelf, Live now,
is not built.

**Pictures are stored top row first everywhere**: what the decoder hands over,
what `Platform::create_texture` takes, what `gfx::kFullUv` draws. The host
harness turns its own capture once, where it reads the frame back, and uses no
global switch of the PNG writer (that switch once turned the fixtures' painted
pictures instead). The board `frame-picture-upright` draws a picture whose top
half is red and bottom half blue and reads the colours back from the output.

**Rulings on what the screen packages asked for** (their request files are
`docs/afterglow/requests/<package>.md` on their branches):

| Asked by | What | Ruling |
| --- | --- | --- |
| A, D, F | the look while updating | done: `UpdateContext::look` |
| D, E, F | a second line under the page title | done: `Screen::subtitle()`, drawn by the frame; a route to `sign_in` or `link_phone` carries no title |
| E | a toast with a second line | done: `Nav::toast(text, detail, kind)` |
| F | an avatar's colours | done: `avatar_tint()` |
| D | the actions of a video | done: `Data::menu()` answers `account:menu:<video>` for a video row |
| A, D, F | one hand-over helper, one actions sheet, shared cards (playlist, section, channel), a message block | done: `HandOver` / `hand_over()`, `Nav::actions()`, `playlist_card()`, `folder_card()`, `channel_card()`, `Notice` (section 12d) |
| E, F | typed answers (`Data::act(row, text)`), text entry as a frame service | done: `Nav::ask_text()`, `Data::act(row, text)`, `Data::answer(text)`; the provider's armed prompt takes the text |
| F | a question with three answers, own labels, a destructive one, a safe default | done: `Nav::ask(Question, answered)` |
| A | `Nav::back_to(screen)` | done |
| F | a revision that follows sign-in and the phone link | declined as asked: those change more often than pages go stale, and a revision change empties every page; the screens that show them read their own catalogs again (`data.forget`) |
| A, D, E, F | facts and catalogs the provider lacks (section 5.5 plus `search-suggest:<text>`, `search-preview:<text>`, `search-with:<options>:<query>`, linked-phone rows, subscription counts, `settings:reset:<tile>`, mark-read rows, acting rows for queue and favourite) | accepted as the bridge's list; each gets an assertion in `provider_test.cpp` when the provider produces it |
| E | interface sound, vibration and reduced motion as preferences | accepted for the bridge; they are the frame's options already |
| D | three more type sizes | to be looked at with its pictures at merge; the scale of `theme.hpp` stays the only source |
| D | queue reorder | not in the design notes: out of Afterglow's first release; the classic interface keeps what it has |
| A, D | the harness draws once; steps cannot hold a button; cost per board | done: the lists are built on every fourth simulated frame; `--only <text>` runs one screen's boards (`bash scripts/afterglow_host.sh out --only library`); `{"hold": ...}` keeps a button down; painted pictures are kept across boards |

**Rules that came out of the packages.** In a list of preferences Left and
Right never change a value: Left at the left edge opens the rail everywhere
(section 8, rule 5); a value changes with Cross, and a slider is adjusted
after Cross takes it (Left and Right then, Cross or Circle to leave). The
Accounts tile of Settings opens the `accounts` screen itself. A preference the
provider does not have is not drawn. A screen file past about 800 lines is
split by its parts at merge. Generators of fixtures and boards live in
`tests/afterglow/tools/`.

**Open for the frame owner.** This list is kept current after every
milestone, so that the work can be continued from this document alone.

*Done (I1, the merge).* The six packages are merged (F, E, A, D, B, C, each
with a merge commit); `scripts/test_afterglow.sh` passes 505 boards in the
four looks. What the merge decided:

- Fixture sets shared by several packages keep one `profile.json`: `signed-in`
  and `long-texts` are the accounts screens' (their catalogs name the profile
  id), `empty` is a Guest. A board that shows a set to somebody else says so
  itself: `"profile": {"id": ..., "name": ..., "guest": ..., "signed_in": ...}`.
- `signed-in/library_subscriptions` is the subscriptions screen's list (Home's
  third shelf reads the same file); `guest/section_trending` is Home's.
- Acting rows `account:apply:<n>` are numbered per package: accounts 7,
  channel family 8 to 10, 17, 20 and up, 60 and up; take a free number.
- A board that follows a route into another package's page checks the screen
  and the snapshot's top-level `argument` (the top route's argument), not that
  page's focus.
- The frame's own boards run around the reader, the simplest built page.
- The generators are in `tests/afterglow/tools/` (`fixtures_<family>.py
  tests/afterglow/fixtures`, `boards_settings.py <file>`); each reproduces the
  committed files exactly. Change a fixture there, not by hand. Home's own
  sets, `probe` and `scripts` have no generator.
- The check: painted pictures are kept across boards and the four looks run
  as four processes on one build (about three minutes in all);
  `scripts/afterglow_host.sh --run-only` runs without building. `--lang <tag>`
  shows a text the catalog lacks in English and counts it; `--strict-lang`
  fails on it.

*Next, in order.* The shared pieces of the table above, built once, and every
screen switched from its `// REQUEST:` stand-in; the rules below applied
(preference lists, the Accounts tile, oversized files); Home's Square as Watch
later; `tr()`, `tr_mark()` and `tr_n()` of `native/afterglow` into
`scripts/i18n_extract.py` (the 21 catalogs are filled by a separate worker
from the template); `scripts/test_all.sh`, the PS5 package, pictures of every
screen in the four looks beside their approved boards.

*Not the frame owner's any more (other worktrees):* the console session driver
(`tools/console`), sounds and vibration (`native/afterglow/sound`), the player
on the kit (`native/afterglow/player`). *Still open after this stage:* the
one-channel glyph pages and the blur on the AGC batch.

## 12d. The shared pieces, and the ruling on every request

What the six packages first wrote for themselves exists once. A screen uses
these and keeps no copy.

| Piece | Where | In place of |
| --- | --- | --- |
| `Motion hand_over(phase, reduced)`, `HandOver` (as `Enter` is), `arrive(age, index)` | `design.hpp` | `acct::Travel`, `search::Handover`, `family::Travel`, B's `Motion`, D's `Hand` |
| `bone()`, `card_bone()`, `spinner()`, `foot_scrim()`, `picture_box()`, `glyph()` | `design.hpp` | every package's bones, spinners and scrims |
| `playlist_card()`, `folder_card()`, `channel_card()` with `CardText` | `design.hpp` | D's three cards, A's result cards |
| `Notice`, `Choice`, `trouble_title()`, `trouble_icon()` | `design.hpp` | `family::Notice`, `ui::EmptyState` configured by hand, `acct::message_page` |
| `button(..., busy)`, `button_width(fonts, ...)`, `chip_width(fonts, ...)`, `tag_width(fonts, ...)` | `design.hpp` | measuring through a draw list nobody draws |
| `wear(style, look, reduced)` with `UpdateContext::look` | `design.hpp` | dressing a copy of a kit component in `draw()` |
| `kCell*`, `kColumns`, `kPageTop`, `kPageBottom`, `kRadiusField`, `kRadiusPicture`, `kRadiusSheet`, `kTypePlayer` to `kTypeFine` | `theme.hpp` | constants at the top of screen files |
| `fill()`, `tr_n(one, other, count)`, `starts_with()`, `dotted()`, `leading_number()` | `base.hpp` | local helpers; two `tr()` texts for one counted text |
| `load_name()`, `Data::act(row, text)`, `Data::answer(text)`, facts on every row, `Video::at`, `Video::published_days`, `Channel::live` | `data.hpp`, `models.hpp` | rows kept beside view models, `prompt:answer` |
| `Feed`, `Act`, `Lists` | `data_feed.hpp` | `family::Feed`, D's `Feed`, every package's action follower |
| `Nav::ask()`, `ask_text()`, `actions()`, `back_to()`, `play()`, `push(..., picture)`; `UpdateContext::arriving` | `screen.hpp`, the frame | `acct::Question`, Settings' text sheet, `family::Sheet`, opening the video page on Touchpad |
| `Screen::draw_over()`, `over()`, `header()` | `screen.hpp` | sheets drawn under the clock, a title of one space |
| `refuse()`, `play_video()`, `route_of_row()`, `icon_of_row()` | `screen.hpp` | `family::refuse`, `family::play`, private tables of row ids |

**Every numbered request.** "done" names the piece above; "bridge" is the list
of what the provider and the service have to produce (package G: section 5.5
and the request files are its contract; nothing in the interface waits for it,
the fixtures stand in).

| Request | Ruling |
| --- | --- |
| A 1 (catalogs `search-suggest:<text>`, `search-preview:<text>`, `search-with:<options>:<query>`) | bridge |
| A 2 (facts `results`, `detail`, `state`, playlists among results) | in section 5.5; bridge |
| A 3.1 to 3.5 (channel facts, the look, hand-over, `back_to`, actions) | done |
| A 4.1 to 4.4 (boards and harness) | done |
| A 5 (umlauts, the reader's bones, `dialog/-1`) | done: the text engine draws them and the fixtures are written with them; `bone()` is visible; the dialog reports the button in focus |
| B 1 to 9 (draw above the furniture, the carried picture, how a screen arrived, picture to player, the hand-over, no title, one hint row, the look, a toast's second line) | done |
| B 10 to 13 (`fill`, plurals, radii and sizes, measuring overloads, a glyph) | done; a page's picture has one radius, 24 |
| B 14 (`Data::menu()` of a comment) | done in `ProviderSource::menu()` (`tube_comment_rating`); the assertion in `provider_test.cpp` is open |
| B 15 (text with an action) | done; the comments keep their own writing sheet (board 30) and hand the text over |
| B 16 (reserved names on plain rows) | done |
| B 17 (the thread's next page after replies were opened) | bridge: the provider keeps one list's paging state; the screen's last row says so until then |
| B 18 (`discussion:top:` and `:newest:`) | section 5.5 corrected |
| B 19 to 22 (harness) | done |
| C 1 to 13 | done |
| C 14 (umlauts) | done |
| C section 2 (`videos`, `notify`, `search`, `live`, `published_days`, `channels:watched:<UC...>`) | in section 5.5; bridge. Search in a channel: the search screen takes `channel-search:<UC...>` as its scope |
| D 1.1 to 1.4, 2.1, 2.2, 2.4 | done |
| D 2.3 (the message block; the tiles "Kept on this PS5") | `Notice`; the tiles stay the library's, the only screen that has them |
| D 3.1, 3.2 | done |
| D 3.3 (whether history is kept) | bridge: a fact of `history`'s first row; the status line says what it knows |
| D 4.1 to 4.3 (acting rows for the queue, favourites, read notifications) | bridge |
| D 5 (facts) | in section 5.5 |
| D 6.1 to 6.5 (harness) | done; a set whose local lists must answer while the account's are offline keeps `{"error": ..., "offline": true}` documents |
| D, queue reorder | out of this release (ruled before) |
| E 1, 2, 5 | done |
| E 3 (a path over the title) | the settings pages draw their own head (`header()` is `clock`); the line under a title is `subtitle()` |
| E 4 (the look follows the Theme) | done: the mount reads the profile's scheme every frame and calls `Frame::set_scheme()` |
| E 6 (facts and rows of `settings`) | the contract of the bridge is `requests/e.md`, section 6. A row the provider does not list is not drawn |
| E 8 | done (`FixtureSource` reads the profile once) |
| F 1 to 6 | done; sign-in and the phone page draw their own head (`header()` is `clock`) |
| F 7 (facts) | in section 5.5; bridge |
| F 8 (a revision that follows sign-in and the phone link) | declined (ruled before) |
| F 9 (the password asked at start) | done in the mount |
| F 11 (wording that differs from boards 35 and 41) | stands: the texts say what the app does |
| drv 1 to 7 | done (`stats`, `focus.kind`, `focus.live`, the rail's `focusable`, JSON whatever the texts are) |

## 12. What is left, as work packages

Each package is one screen (or one family that shares a layout), its boards
and its fixtures. The boards of the proposal it implements are in brackets.

| Package | Screens | Files it owns | Reads |
| --- | --- | --- | --- |
| A | `search`, `results` [09 to 12] | `screens/search.cpp`, `screens/results.cpp` | `search-suggest`, `search-history`, `search:<query>`, the kit's keyboard |
| B | `video`, `comments` [15 to 17, 27 to 28] | `screens/video.cpp`, `screens/comments.cpp` | `details:<id>`, `description:<id>`, related list, `discussion:<id>` |
| C | `channel`, `channels`, `subscriptions` [13 to 14, 24 to 26] | `screens/channel.cpp`, `screens/channels.cpp`, `screens/subscriptions.cpp` | `channel:<id>`, `channel-tab:...`, `library:channels`, `subscriptions`, `library:subscriptions` |
| D | `library`, `notifications`, `grid` [23, 25] | `screens/library.cpp`, `screens/notifications.cpp`, `screens/grid.cpp` | `history`, `favorites`, `queue`, `library:playlists`, `library:watch_later`, `library:notifications`, any catalog of videos |
| E | `settings` [29 to 31] | `screens/settings.cpp` | `settings` and everything under it, as rows with `value`, `selected`, `kind` |
| F | `accounts`, `sign_in`, `link_phone` [32 to 35] | `screens/accounts.cpp`, `screens/sign_in.cpp`, `screens/link_phone.cpp` | `library`, `library:profile:<id>`, `auth:start`, `auth:session`, `settings:remote` |
| G | the data bridge | `app/data_provider.cpp`, `native/tube_row_facts.h`, the provider's and the service's rows | section 5.5, for every package's facts |
| H | the player's controls on the kit [18 to 22] | later stage | |

Home, the opening, the frame and the mount are the frame owner's. Loading,
empty and offline are states of each screen, not screens.

## 13. The kit pin and the build

`scripts/ui_kit.py` names one commit of `ps5-homebrew-ui` and
`scripts/ui_kit.lock.json` the SHA-256 of every file the build uses. The kit is
fetched into `.deps/ui-kit/<commit>` from `PROSPEROTUBE_UI_KIT` (a clone or a
git address), from `~/ps5-homebrew-ui` when it exists, or from the kit's
address with whatever credentials git has; a file that differs from the lock
stops the build. Changing the pin is: edit the commit, run
`python3 scripts/ui_kit.py --lock`, build, run the checks, and say in the
commit what the newer kit changes for us.

The host build (`scripts/afterglow_host.sh`) compiles the kit's draw-list
builders, components and OpenGL renderer with the interface and the harness.
The app's build (`scripts/prepare_native.py`) lays the same kit over the copy
EVO pins, so EVO's own kit screens and the interface use one kit, and compiles
`native/afterglow` into the player next to them.
