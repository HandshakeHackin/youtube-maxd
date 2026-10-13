# Package A (search, results): requests to the frame owner

What the two screens need from files they do not own, what they do meanwhile
(marked `// REQUEST:` in the code), and what they read from the data layer.

## 1. Catalogs (data bridge)

The screens read these ids through `Data::catalog()`; with fixtures each is a
file. `screens/search.hpp` builds the ids.

| Id | What it has to answer | In the provider today |
| --- | --- | --- |
| `search-suggest:<text>` | Completions for the text typed so far: folder rows whose `title` is the completion, at most 15. A row the provider wrote (`interface`: "No suggestions") is skipped. | `search-suggest` completes the last search only; the service action `search_suggestions` already takes any `query`. |
| `search-preview:<text>` | The best matches so far: page 0 of a search that is **not** kept in the history and has no leading rows, videos only. Asked 0.12 s after the last key. | `catalog("search:<text>")` without `g_search_typed` is exactly this. |
| `section:trending` | Shown as "Popular right now" while nothing is typed. | exists |
| `search:<query>` | The submitted search: kept in the profile's history, with channels and playlists among the videos, later pages through `catalog(id, page)`. The provider's two leading rows (`settings:search`, `search-suggest`) may be there; the screen skips them. | `search(query, page)` |
| `search-with:<options>:<query>` | The same with filters. `<options>` holds no colon: `name=value` pairs joined by `,`, only what is set, in this order: `date=today\|week\|month\|year`, `duration=short\|medium\|long`, `type=video\|channel\|playlist\|movie\|shorts`, `features=` any of `4k`, `subtitles`, `live` joined by `+`, `order=view_count`. The values are the provider's own (`g_search_dates`, ...). Example: `search-with:date=month,features=4k+live:aurora`. | set `g_search_options` from the options, then `search(query, page)` |
| `search-history` | As today: `search-history:run:<serial>` rows (title = the query), `search-history:clear:<scope>`, or a notice row. | exists |
| `search-history:forget:<serial>`, `search-history:clear:<scope>` | Asked through `Data::act()`; the answer (what is left, or the notice) becomes the list, then the screen calls `data.forget("search-history")`. | exists |

The search screen also calls `data.forget("search-history")` when it is
re-entered from the results, so a search that ran shows up with its serial.

## 2. Facts read

| Screen | Catalog | Row | Facts |
| --- | --- | --- | --- |
| search | `search-suggest:<text>` | a completion | `detail` (optional: the quiet text at the end of the row; the service has none today) |
| search | `search-history` | the `account:notice` row | `state`: `off` (history is switched off) or `empty`. Without it the provider's title is shown alone, because its subtitle names the classic interface's buttons. |
| search | `search-preview:<text>`, `section:trending` | a video | `badges` (through `model::Video`) |
| results | `search:<query>`, `search-with:...` | the `settings:search` row (or any row of page 0) | `results`: the count as it is to be shown ("1,240"); drawn as "About {0} results" |
| results | same | a video | `views`, `published`, `badges`, `is_upcoming`, `is_short` (through `model::Video`) |
| results | same | a channel (`channel:<id>`) | `handle`, `subscribers`, `subscribed` |
| results | same | a playlist: a folder whose id starts with `playlist:` or `library:playlist:` | `count`, `owner`; `art_url` is its first picture |

Please add to section 5.5: the `results`, `detail` and `state` lines, and
playlists under `search:<query>`.

## 3. Shared code

1. **`data_rows.cpp`, `add_row`**: fill `model::Channel::handle`,
   `subscribed`, `unseen` and (when the subtitle is empty) `subscribers` from
   the row's facts, as section 5.4 says. Meanwhile `results.cpp` reads
   `row.fact("handle")`, `row.fact("subscribers")`, `row.has("subscribed")`.
2. **The look in `update`**: `const Look &look` in `UpdateContext` (or
   `Services`). A kit component draws in `style.theme`; `update` has no look,
   so the keyboard, the search field, the menu, the skeletons and the empty
   state are `mutable` and take the theme at the top of `draw()`.
3. **Hand-over** in `design.hpp`: `struct Handover { Handover(G &g, const
   Phase &phase); ~Handover(); float alpha; };` the scratch design's general
   hand-over (fade and 97.5 % for a section change, 90 px out and 110 px in
   for push and pop, the furniture gone early for a carried picture). The
   stand-in is `search::Handover` in `screens/search.hpp`, used by both
   screens.
4. **What is under a screen**: `virtual bool Nav::back_to(ScreenId)` (pops to
   the nearest such screen below, false when there is none) or `ScreenId
   Nav::below()`. Square "Search again" on the results should return to the
   search screen under it; without this it calls `nav.root(search, query)`,
   which opens a fresh one with a fade.
5. **Actions sheet**: one shared piece for "Touchpad: Actions" on a video
   wherever videos are listed (board 14), for instance `Nav::actions(row id,
   title)` on `Data::menu()`. Until it exists Touchpad on a result opens the
   video page, as Triangle does.

## 4. Boards and harness

1. **`tests/afterglow/boards/reader.json`, board `reader-back`** expects
   `"focus": "placeholder"` on the search screen. Search is built now: the
   focus there is `"key/1"`. That one value has to change (or the expectation
   be dropped); it is the only failure of `scripts/test_afterglow.sh` on this
   branch, once per look.
2. **Pictures are upside down in every board picture**: `host_main.cpp` sets
   `stbi_flip_vertically_on_write(1)` for its own PNGs before the boards run,
   and `HostPlatform::picture_bytes` writes the painted `art://` pictures with
   the same writer. Set the flag only around the board's `stbi_write_png`.
3. **Pictures exist only if asked for in `update`**: a board is drawn once, at
   its end, and `ImageCache::get()` is called from `draw`. Both screens
   therefore ask for the pictures in view (and the ambient one) in `update`
   too. Either the harness draws every frame, or section 6 should say so.
4. **Steps cannot hold a button**: `nav_repeat` and held buttons never occur,
   so the silent refusal on a held direction and the keyboard's held delete
   are not covered by a board.

## 5. Seen on the way (not requests)

- The pinned kit's faces hold no ä, ö, ü, ß and no typographic quotes; the data
  layer drops them ("dreißig" becomes "drei ig"). The long-texts fixtures of
  this package are therefore written without umlauts, and the two texts with
  quotes ("Best matches for ...", "No results for ...") have a second form
  with plain quotes that is used while `covers()` says no.
- `reader.cpp` draws its loading bones with `raised.with_alpha(raised.a * x)`:
  `with_alpha` multiplies, so they are all but invisible.
- The confirm dialog reports the focus `dialog/-1` until a direction is
  pressed.

## 6. Interface texts added

`screens/search.cpp` and `screens/results.cpp` hold 81 `tr()` texts; 52 of
them are not in `native/i18n/template.json` yet and are to be added to the
catalogs centrally (`scripts/i18n_extract.py` lists them). Three are patterns
with one value: "About {0} results", "{0} videos", and "Best matches for ..."
/ "No results for ..." in their two forms.
