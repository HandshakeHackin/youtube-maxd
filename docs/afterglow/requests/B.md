# Package B (video, comments): requests to the frame owner

Each entry: what, why, what the screens do meanwhile (marked `// REQUEST:` in
`native/afterglow/screens/video*.{hpp,cpp}` and `comments*.{hpp,cpp}`).

## Frame and screen interface

1. **A second draw above the frame's furniture.** `Screen::draw_over(const
   DrawContext &, const Phase &) const`, called after the header, the hint row
   and the rail. Boards 14 and 15 put the More actions sheet and the Share
   panel, with their scrim, over the clock, the hints and the rail. Meanwhile
   both are drawn at the end of `draw()`: the clock stays on top of the sheet,
   the rail and the clock are not dimmed, and `hints()` returns the sheet's own
   row (nothing while Share is up, whose panel carries its hints).
2. **The carried picture's address.** `Phase::picture` (or a field of `Route`)
   beside `Phase::carried`. A page that arrives by `Move::zoom` knows the
   rectangle and not the picture until `details:` answers. Meanwhile the
   travelling rectangle holds the design's resting box until the address is
   known (with the fixtures: three frames).
3. **How a screen arrived.** `enter()` does not say by which `Move`. The motion
   table delays the video page's furniture by 160 ms only behind a carried
   picture; meanwhile it waits 120 ms on every arrival.
4. **Picture to player, and back** (motion table: 0.62 s and 0.5 s): no `Move`
   for it and no request that plays with the picture's rectangle
   (`Nav::play(video, carried)`). Meanwhile `Data::play()` with the `launch` cue
   and the rumble; nothing is animated.
5. **A screen's share of a hand-over.** The frame hands a `Phase` and moves
   nothing, so "deeper / back out" and "section change" of the motion table are
   a function each screen copies (`hand_over()` in `video.hpp` and
   `comments.hpp`: opacity, scale, dx, and the furniture's own fade under a
   carried picture). Proposed: `Motion hand_over(const Phase &, bool reduced)`
   in `design.hpp`, or the frame applying it.
6. **No header title.** The frame shows `route.title` when `Screen::title()` is
   empty. The video page has no header; it is pushed with an empty title, but a
   caller that names the route puts a title over the page's own overline.
   Proposed: `Screen::shows_title()`.
7. **Two hint rows at once.** During a hand-over the frame draws both screens'
   rows at the same place with linear alpha (seen in `video-related-opening`:
   "Choose" under "Details").
8. **The Look while updating.** `UpdateContext` has no `Look`, and a kit
   component needs `style.theme` before its const `draw()`. Meanwhile the
   list, the keyboard, the spinner and the skeleton are copied in `draw()` and
   the copy wears the theme. Proposed: `const Look &look` in `UpdateContext`.
9. **A toast's second line.** The boards' toasts say what and to whom
   ("Subscribed", "Northlight Atlas"); `Nav::toast` takes one text.

## Design language, texts

10. `base.hpp`: `fill(pattern, a, b)` for "{0}" and "{1}", and plural forms
    (`tr_n(pattern, count)`; the catalogs have "one" and "other"). Meanwhile
    `fill()` and `counted()` are local and the two English forms are separate
    `tr()` texts ("{0} reply", "{0} replies").
11. `theme.hpp`: the boards use radii 20 and 24 (a page's picture), 22 (a
    field), 36 to 40 (sheets) and type sizes 19, 21, 22, 23, 25, 34, 38 beside
    the scale; they are named constants at the top of `video.hpp` and
    `comments.hpp`.
12. `design.hpp`: `button_width`, `chip_width` and `tag_width` take a `G`;
    while updating there are fonts only, so the video page measures through a
    `G` on a draw list nobody draws. Proposed: overloads on `const ui::Fonts &`.
13. `design.hpp`: one controller glyph (the Square in the compose box); the
    comments draw it as a hint row with an empty label.

## Data

14. **`Data::menu()` for a video and for a comment.** `ProviderSource::menu`
    asks `tube_folder_menu`, which knows folders: a video row answers "". The
    video's actions are `account:menu:<video>` (the video page falls back to
    that id when `menu()` is empty); a comment's are `comment:menu:<cursor>`
    from `tube_comment_rating(row)` (no fallback: liking and answering a
    comment work with fixtures only until `menu(comment row)` names it).
15. **Text with an action.** `Data::act(row)` carries nothing. A comment's text
    has to travel with `comment:post:<video>` and `comment:reply:<cursor>` (the
    provider takes it from its prompt). Proposed: `act(row, text)`. Meanwhile
    the rows are listed and the text stays on the screen: nothing can be
    posted on a console.
16. **Reserved names never reach a plain row.** `views`, `published`,
    `channel_art_url`, `badges`, `is_upcoming`, `is_short` (and in a fixture
    `is_live`, `duration_sec`, `resume_sec`) go to a video's fields in both
    sources; a row that is not a video loses them. Section 5.5 lists `views`,
    `published`, `channel_art_url` for the first row of `details:` and
    `published` for a comment. Meanwhile the screens read the provider's own
    rows instead: `account:notice:views`, `account:notice:published`, the
    `channel:` row's picture, the first row's second line ("4K | 48:12"), a
    comment's second line ("2 weeks ago | 2.8K likes | 12 replies").
17. **Thread paging beside replies.** The provider continues a discussion only
    while it is the last list opened; opening a comment's replies (or its
    menu) in between ends the thread's next page with "Reload the discussion
    to continue reading." The comments screen shows that message as the
    thread's last row; the data layer has to keep the continuation alive.
18. Section 5.5 names `discussion:<video>`; the provider's catalogs are
    `discussion:top:<video>` and `discussion:newest:<video>`.

## Facts the two screens read

| Catalog | Row | Read |
| --- | --- | --- |
| `details:<video>` | first row | title, overview, `art_url`; facts `views`, `published`, `likes`, `channel`, `channel_id`, `channel_art_url`, `subscribers`, `subscribed`, `rating`, `saved`, `chapters`; **new:** `duration` (seconds), `resume` (seconds, the profile's position), `segments` (SponsorBlock segments the profile skips), `captions` (languages), `fps`, `quality` (`4k`, `hdr`), `live`, `upcoming` |
| `details:<video>` | `channel:<id>` | title, subtitle (subscribers), `art_url` |
| `details:<video>` | `account:notice:views`, `:published`, `:likes` | title (until the facts above arrive) |
| `details:<video>` | `description:<video>`, `library:suggestions:<video>` | that the row is there (the related shelf is that catalog) |
| `library:suggestions:<video>` | a video | as any list of videos |
| `Data::menu(<video>)`, two pages | every row | id and title; ids `account:confirm:<action>:<id>`, `playlist:confirm:...`, `playlist:pick:...`, `reminder:...`, `notify:...`, `local:video:<scope>:<verb>:<video>` (`next`, `restart`, `incognito`, `watched`, `forget`, `block`, `unblock`, `sbexclude`, `sbinclude`), `channel:<id>`, `description:<video>`, `library:playlist:<list>`, `account:notice...` |
| `link:video:<video>` | `link:row:video`, `:channel`, `:playlist` | overview (the address) |
| `discussion:top:<video>`, `discussion:newest:<video>` | first row | `comment:post:<video>` (subtitle: the count) or `account:notice:comments` (title: the count) |
| ... and `discussion:replies:<cursor>` | a comment | id, title (author), overview (text), subtitle; facts `author`, `published`, `likes`, `replies`, `pinned`, `owner`, `hearted`, `liked`, `author_art_url` |
| `Data::menu(<comment row>)` | every row | ids `comment:confirm:like:<cursor>`, `comment:confirm:none:<cursor>`, `comment:reply:<cursor>` |
| any acting row's answer | every row | ids `account:apply:<serial>`, `comment:review:<serial>` (listed next), else the first title (the provider's words) |

Not read: `dislikes`, `member`, `disliked`.

## Harness and boards

19. `tests/afterglow/boards/frame.json` uses `comments` as its page that is not
    built yet. With the comments screen built, `frame-placeholder` and
    `frame-rail-closed-by-circle` can no longer hold (focus `placeholder`).
    `reader.json`'s `reader-back` expects the same of `search` (package A).
20. `host_main.cpp` sets `stbi_flip_vertically_on_write(1)` before the boards
    run, so the pictures it paints for `art://` are encoded upside down: every
    thumbnail in a board picture stands on its head.
21. The harness draws one frame, the last. A picture asked for only in `draw()`
    is never loaded in a board picture (the page ground's wash included).
    Both screens also ask in `update()`, which serves the app as well.
22. A picture is painted once per size class asked (hero, card, ambient): a
    board with a page picture and a shelf takes seconds. The fixtures of this
    package name small pictures (`/800x450`, `/320x180`) to keep that down.
