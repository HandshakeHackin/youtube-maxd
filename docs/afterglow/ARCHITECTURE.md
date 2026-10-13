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
| Home (boards 03 to 08 of the proposal) | in progress (frame owner) |
| `ProviderSource`: the data layer on the shipping provider (`scripts/test_afterglow_data.sh`) | done |
| Row facts (section 5.5): the store, and a video's `views`, `published`, `channel_art_url`, `badges` from the service | done |
| Row facts of the other catalogs of section 5.5 | open: with the screen that needs them, through the frame owner |
| The mount in the app, the Interface switch, the console session driver | in progress (frame owner) |
| Every other screen (section 12) | open: one work package each |
| Text engine (`native/text/`): system fonts, shaping, bidi | another worker; seam in section 7 |
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
  images.hpp/.cpp     pictures: fetch, decode, scale, upload, retire
  models.hpp          view models
  data.hpp/.cpp       Source (what the app is asked) and Data (what screens read)
  data_rows.hpp/.cpp  a catalog row as it arrives and what it becomes
  data_fixture.hpp/.cpp  Source from files (tests)
  screen.hpp          Screen, Route, Phase, Nav, contexts, the factories
  frame.hpp/.cpp      the stack, hand-overs, input routing, attract mode
  frame_draw.cpp      page ground, rail, header, hints, snapshot
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
| `search:<query>`, `library:channels`, `subscriptions` | a channel | `subscribers`, `handle`, `subscribed`, `unseen` |
| `channel:<id>` and `channel-tab:<id>:<section>` | first row `channel:<id>` | `handle`, `subscribers`, `banner_url`, `subscribed`, `tabs` (comma separated), `tab`, `sorts`, `sort` |
| `details:<video>` | first row | `views`, `published`, `likes`, `dislikes`, `channel`, `channel_id`, `channel_art_url`, `subscribers`, `subscribed`, `rating` (`like`, `dislike`, ""), `saved`, `chapters` (count) |
| `discussion:<video>`, `discussion:replies:<cursor>` | a comment | `author`, `published`, `likes`, `replies`, `pinned`, `owner`, `member`, `hearted`, `liked`, `disliked`, `author_art_url` |
| `library:notifications` | a notification | `published`, `channel`, `channel_art_url`, `unread` |
| `library:playlists`, `library:playlist:<id>` | a playlist | `count`, `owner`, `updated` |
| `settings` and everything under it | a preference | `value` (the current choice as text), `selected` (the row is the current choice), `kind` (`list`, `switch`, `action`, `text`) |
| `library` and `library:profile:<id>` | a profile | `guest`, `signed_in`, `selected`, `locked` |
| `auth:start`, `auth:session` | the code row | `user_code`, `verification_url`, `expires_in`, `state` |

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
```

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
`measure_block()`, `covers()` and `drawable()` (`text.hpp`) with a `Face`,
which today is one of the kit's baked fonts (`g.f.regular`, `g.f.semibold`,
`g.f.display`, `g.f.mono`). `text_kit.cpp` is the one implementation. The text
engine replaces that file with an implementation of the same functions on its
own measure, wrap and draw interface; a `Face` then names a font role instead
of a baked atlas. The kit's standard components (keyboard, dialog, toasts,
forms) draw through `ui::Fonts` themselves; the engine serves them by adding
fallback faces to those fonts (`gfx::Font::add_fallback`). Do not call
`ui::text`, `ui::paragraph`, `font->measure` or `DrawList::text` in a screen.

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
    virtual std::string title() const;                    // the header; "" for none
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
`touchpad`) and waits in seconds on a fixed 60 Hz clock. `expect` compares the
frame's snapshot: `screen`, `focus` (the focus id), any top-level number
(`rail_open`, `moving`, `attract`, `dialog`), `state` (values of
`Screen::snapshot`), `asked` (catalogs that must have been requested), `played`
(the last video handed to the player). Name boards `<screen>-<state>`.

**What every board is held to, in every look**, without being asked: something
has the focus; its rectangle is on the screen; no glyph crosses the edge of the
screen outside a clip rectangle; the page is not empty; the picture has more
than a handful of colours; the snapshot is valid JSON. The looks themselves are
held to 4.5:1 for every text colour on its ground. A run with `--lang de` or
`--lang ru` fails on an interface text the catalog lacks.

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
