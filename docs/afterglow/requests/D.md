# Package D (library, notifications, grid): requests to the frame owner

What the three screens need from shared files. Every stand-in is marked
`// REQUEST:` in `native/afterglow/screens/library.cpp`, `notifications.cpp`
and `grid.cpp`; when a request is applied, the stand-in goes.

## 1. Screen interface and frame

| # | What | Why | Proposed | Stand-in today |
| --- | --- | --- | --- | --- |
| 1.1 | The line under a page title | Library ("Mara · on this PS5 and on YouTube"), Notifications ("3 new"), a playlist ("24 videos · owner · updated") and four boards of other packages have one; it must fade and travel with the title | `virtual std::string Screen::subtitle() const`, drawn by `Frame::draw_header` at 176 / 166 in 22 px, `ink2`, fitted | Each screen draws it itself |
| 1.2 | The Actions sheet of a video | Touchpad: Actions (design notes, section 10) on every list of videos; board 14 | `Nav::actions(const model::Video &video, const Rect &from)`: the frame shows `Data::menu(video.id, title)` as a sheet and runs the chosen row with `Data::act()` | Touchpad opens the video page (`Move::zoom`) in library and grid |
| 1.3 | The look while updating | Kit components (TabBar, Skeleton, EmptyState, Spinner) wear the look's theme, and a screen sees the look only in `draw`: today each component is copied per draw to dress it | `const Look &look` in `UpdateContext`; `wear(ui::ComponentStyle &, const Look &, bool reduced)` in `design.hpp` (it is private to `frame.cpp`) | Copy, set `style.theme`, draw |
| 1.4 | Type sizes | `theme.hpp` has no size for the line under a title (22), the card title `video_card` itself uses (23), and the message block of boards 37 and 38 (36) | `kTypeSub`, `kTypeCard`, `kTypeMessage` | Local constants of the same names |

## 2. Design language (`design.hpp`): pieces now written three times

| # | What | Used by |
| --- | --- | --- |
| 2.1 | `Hand hand_over(const Phase &, bool reduced)`: the motion table's hand-overs (push/pop 90 and 110 px, zoom and shrink furniture gone in 150 ms, fade sinking to 97.5 %) as alpha, dx and scale | all three; every screen needs it |
| 2.2 | `playlist_card()`, `folder_card()` (a section inside a section), `channel_card()` beside `video_card()` | library (playlists), grid (all three); results shows the same cards |
| 2.3 | The message block of the approved empty and offline boards: icon on a disc, title, three lines, one action with its glyph (today `ui::EmptyState` configured identically in three files), and the row of tiles "Kept on this PS5" | all three (tiles: library) |
| 2.4 | A catalog across pages: what has arrived, whether more is coming or was refused, and a number that changes when the rows change (`Feed` in the three files; it hashes row ids every frame because `Page` has no version) | all three; any long list |

## 3. Data layer

| # | What | Why | Proposed |
| --- | --- | --- | --- |
| 3.1 | Facts are not cleaned | `add_row()` passes `title` and `subtitle` through `clean()` but copies `facts` as they come: an owner or a channel name with a letter no face draws shows "?" | `clean()` on every fact value in `add_row()` (the screens call `clean()` on the facts they show meanwhile) |
| 3.2 | `Data::menu()` of a video | `ProviderSource::menu()` asks `tube_folder_menu()`, which knows sections, channels and playlists; for a video it answers "". The library's Remove reads the video's menu for the acting row | `account:menu:<video id>` for a video row |
| 3.3 | Whether history is being kept | Board 25 says "214 videos · history is on"; the screen cannot know | `enum class History { on, paused, off }` in `model::Profile` (local history) and the account's paused state as a fact of `library:history`; the library's status line says "24 videos · on this PS5" today |

## 4. Provider and service: acting rows

| # | Row | Why |
| --- | --- | --- |
| 4.1 | `local:video:<scope>:queue:<id>` in a video's menu (toggles as `tube_toggle_queue()` does) | Library, Queue tab: Square removes. The provider changes the queue only through the C function today |
| 4.2 | `local:video:<scope>:favorite:<id>` (as `tube_toggle_favorite()`) | Library, Favourites tab: Square removes |
| 4.3 | `notifications:read:<row>` and `notifications:read:all` | Notifications: opening a row marks it read, Square marks all. The service keeps YouTube's own "clicked" call private; until a row exists, read is remembered by the page only (`mark_read()` in `notifications.cpp`) |

Used as they are: `local:video:<scope>:forget:<id>` (History), `account:confirm:watch_later_remove:<id>` then
the `account:apply:<n>` it answers with (Watch later), `account:confirm:unrate:<id>` then `account:apply:<n>` (Liked).

## 5. Facts the three screens read

| Screen | Catalog | Row | Facts | State |
| --- | --- | --- | --- | --- |
| library | `history` | a video | `watched` (when it was watched, worded in the profile's language: "Today, 20:14") | new; without it the card shows views and age |
| library, grid | `history`, `favorites`, `queue`, `library:watch_later`, `library:playlist:<id>` | the first row of the first page | `total` (how many rows the whole list has) | new; without it the count shows once every page was read, and a tab's badge stays empty for a long list |
| library, grid | `library:playlists`, any list | a playlist (`library:playlist:<id>`, `playlist:<id>`) | `count`, `owner` | in section 5.5 |
| grid | `library:playlist:<id>`, `playlist:<id>` | the first row when it is the list itself (id equal to the catalog) | `count`, `owner`, `updated` | new: the line under the title |
| grid | any list | a folder that is a filter or an order of the list | `chip` (`filter` or `sort`), `selected` | new; `library:filter:<cursor>` rows are taken as filters without it |
| grid | any list | a channel | `handle`, `subscribers`, `subscribed` | in section 5.5 for `search:` |
| notifications | `library:notifications` | a notification | `channel`, `published`, `channel_art_url`, `unread`; the row's `channel_id` | in section 5.5; `channel_id` is new. Without `channel` the sentence is shown whole and Triangle (Open channel) refuses |

## 6. Harness and boards

| # | What |
| --- | --- |
| 6.1 | `tests/afterglow/boards/frame.json`, `frame-rail-destination`: it expects the Library to be the placeholder (`"focus": "placeholder"`). With the library built the focus there is `tab/history`, so the board fails in the four looks until it expects `"screen": "library"` alone |
| 6.2 | `host_main.cpp`: `stbi_flip_vertically_on_write(1)` also flips the `art://` pictures that `picture_bytes()` encodes, so every painted picture is upside down in the boards |
| 6.3 | `host_main.cpp` draws once, after the last step. A picture is asked for when it is drawn, so a board never has pictures unless the screen also asks in `update`. The three screens do (a row ahead of the page, and the focused picture's ambient copy): keep that in mind when the harness changes |
| 6.4 | `FixtureSource::set_offline()` answers `Load::offline` for the lists the provider serves from the console too (`history`, `favorites`, `queue`, `subscriptions`). The offline boards of this package use the set `d-offline`, whose account catalogs are `{"error": ..., "offline": true}` documents |

## 7. Routes these screens open (for the other packages)

- `Route{ScreenId::video, <video row id>, <title>}` with `Move::zoom` and the card's rectangle.
- `Route{ScreenId::channel, <"UC..." id>, <name>}` with `Move::push`.
- `Route{ScreenId::channels, "subscriptions", "Followed channels"}` from the library's tile of that name
  (package C: say if another argument names the channels followed on the console).
- `Route{ScreenId::sign_in}` from every sign-in invitation.
- `Route{ScreenId::library, <tab>}` with `history`, `watch_later`, `playlists`, `liked`, `queue` or `favorites`;
  `Route{ScreenId::grid, <catalog id>, <title>}` for any list of videos.
