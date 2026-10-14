# Package F (accounts, sign_in, link_phone): requests to the frame owner

Each request has a stand-in in the screens, marked `// REQUEST:` with its
number here. Nothing in this list blocks the boards; each one closes a
difference to the approved boards or to the app.

## 1. `Data::answer(text)`: the provider's armed prompt

**What.** A way to hand a typed text to the prompt the provider armed
(`prompt_arm`: "Password of this profile", "New password of this profile",
"Current password of this profile") and to get the answer as an action.

**Why.** A protected profile is selected, and a profile's password set, changed
or removed, only through `tube_take_playlist_prompt()` (the ticket) and
`tube_submit_playlist_name(ticket, text, parent, capacity)`: the provider
compares or hashes the text and names the catalog to
list next (`library:profile:<id>`, `settings:lock:new:<ticket>`,
`settings:accounts`). A catalog id cannot carry the text: ids are logged, kept
as cache keys and limited in length, and a password must not be kept.

**Proposed.**

```cpp
// data.hpp, beside act(): the text goes to the provider's armed prompt; the
// action's Page is the listing of the catalog the provider names, or
// Load::failed with tube_prompt_error() ("Wrong password.", "Enter a password
// of 1 to 150 characters."). The text is not stored, logged or cached.
unsigned answer(std::string_view text);
// Source: virtual unsigned answer(std::string_view text, Done done) = 0;
```

**Implemented 2026-10-14.** The account screen calls `data.answer(text)` and
then wipes its temporary copy. This uses the shared typed-answer path and
follows the provider's armed prompt. The account board now verifies the typed
answer was actually delivered; the previous placeholder passed its fixture
without sending the password. Console acceptance remains pending.

## 2. `Nav::confirm` for a destructive answer, and for three answers

**What.** Board 41 ("Remove Mara from this PS5?"): the danger mark, two labels
of the screen's own ("Keep", "Remove"), the focus on the safe answer, the other
in the warning colour. And a question with three stacked answers (the password
of a protected profile: change it, remove it, cancel).

**Why.** `Frame::confirm` builds `{confirm_label (primary, focused), "Cancel"}`
with the question mark. Used for a removal it would open with the focus on the
answer that removes.

**Proposed.**

```cpp
struct Question {
    ui::StatusKind icon = ui::StatusKind::question;
    std::string title, body;
    std::vector<std::string> answers; // one to three
    int danger = -1;                  // the answer drawn in the warning colour
    int safe = 0;                     // where the focus opens
    bool stacked = false;             // answers that are sentences
};
virtual void ask(Question question, std::function<void(int answer)> answered) = 0; // -1: put away
```

**Stand-in.** `acct::Question` keeps a kit `ui::Dialog` in the screen
(accounts: remove, password; link_phone: reset pairing). What differs from the
frame's dialog until then: the rail, the page title, the clock and the hints
are drawn by the frame after the screen, so they are not under the scrim, the
panel frosts the page ground only (not the tiles), and the snapshot's `dialog`
stays 0 (the screens report `question` and the focus `dialog/<n>` themselves).

## 3. The look while updating

**What.** `const Look &look` in `UpdateContext` (or `Services`).

**Why.** A kit component (`ui::ProfilePicker`, `ui::Keyboard`, `ui::TextField`,
`ui::Dialog`) takes its theme through its `style`, which belongs with the rest
of its state in `update`; a screen only meets the look in `draw`, which is
const.

**Stand-in.** The components are `mutable` and dressed at the start of `draw`
when the look or reduced motion changed (`dress()` in `accounts.cpp`,
`accounts_password.cpp`, `Question::draw`).

## 4. A page in a hand-over, as a shared piece

**What.** `acct::Travel` (`accounts_shared.cpp`) in `design.hpp`: an RAII piece
that applies the motion table's "deeper / back out" to a page from its `Phase`
(arriving 110 px from the side of the move with quint-out, leaving 90 px with
cubic-in, a leaving page gone early; `Move::fade` sinks to 97.5 %).

**Why.** The frame moves nothing for a screen; every screen that is pushed or
popped needs the same few lines, and they should not differ between packages.

## 5. The avatar's tint

**What.** `Color avatar_tint(std::string_view name, std::string_view id)` in
`design.hpp` (the top colour `avatar()` uses for a profile without a picture).

**Why.** The focused tile's light and ring are "in the content's own colour"
(board 33: Mara's warm ring). With a picture the tile uses `Picture::glow`;
without one it has no way to learn the colour `avatar()` draws, and falls back
to the look's focus colour (a white ring and light).

## 6. A page head of two lines

**What.** Sign-in and the phone page have a head of their own (boards 34 and
35): the family in accent capitals at y 96 and the title at y 156, where the
frame draws a single title at y 128. Either `Screen` says so
(`virtual std::string overline() const`, drawn by the frame above a lowered
title), or the frame does not fall back to `route.title` for a screen whose
`title()` is empty.

**Stand-in.** Both screens return an empty `title()` and draw their head
themselves (`acct::page_head`). **A route to `sign_in` or `link_phone` must
carry an empty `title`** until then, or the frame draws it across the head.

## 7. Facts: exactly what the three screens read

Fixtures use these today; the provider side is asked for here.

| Catalog (read or acted on) | Row | Read |
| --- | --- | --- |
| `library:profiles` (read; further pages when `has_more`) | `library:profile:<id>` | `title` (the name; Guest is recognised by the fact or the id `guest` and shown through `tr("Guest")`), `art_url` (the account's picture, optional), facts `guest`, `signed_in`, `selected`, `locked`, `subscriptions` (a count as text, optional) |
| `library` (read) | `auth:start` | only whether the row exists: the build signs in to YouTube |
| `library:profile:<id>` (acted on: selects the profile) | first row | id `account:notice` with fact `prompt` = `password` when the profile's password prompt was armed (today inferred from the tile's `locked` and the row id) |
| `account:remove` (acted on) | `account:apply:<n>` | its id, which is then acted on once |
| `settings:lock:set`, `:change`, `:remove` (acted on) | | only success or the provider's message |
| `auth:start`, `auth:session`, `auth:cancel` (acted on) | the first row that has it | `state` (`starting`, `pending`, `authorized`, `canceled`, `expired`, `denied`, `failed`), `user_code`, `verification_url`, `expires_in` (seconds left) |
| ... when authorized | `library:profile:<id>` | its id; fact `name` (else the row's `subtitle`) |
| `settings:remote` (read), `settings:remote:pair`, `:toggle`, `:disconnect`, `:history`, `:reset` (acted on; each answers with the catalog as it is afterwards) | `settings:remote:info` | `state` (`off`, `connecting`, `ready`, `stopped`; any other state is shown in the words of `value`), `available` (`0` when the service lists no `remote` feature) |
| | `settings:remote:code` | `value` (the digits; else the digits of the title) |
| | `settings:remote:toggle` | `selected` (the connection is on) |
| | `settings:remote:disconnect`, `:history`, `:reset` | `title` and `subtitle` as the provider writes them |
| | `settings:remote:device:<id>` | `title` (the phone's name), `subtitle` (since when, last used), `connected` |

Notes for the bridge:

- Section 5.5 names `library` and `library:profile:<id>` as the catalogs with
  profile rows; the list of profiles is `library:profiles` in the provider.
- Without the `state` fact the sign-in page reads the rows themselves
  (`auth:info:code` and `auth:info:url` titles, a `library:profile:` row), and
  the phone page infers `ready` / `connecting` / `off` from the code and
  `selected`. `expires_in` has no fallback (the page then counts from 15:00).
- `settings:remote` has to answer in a build without the phone receiver too
  (today only the `settings` menu hides the entry): with `available` = `0` the
  page and the "Link a phone" action show "Not available in this build".
- The provider keeps no list of linked phones today (`TUBE_REMOTE_CONNECTED`,
  `TUBE_REMOTE_DEVICES` carry names). Without `settings:remote:device:` rows
  the panel says "No phone is linked yet" beside the connection's state.
- A subscription count per profile does not exist in the service yet; without
  it the focused tile says "Password protected" or "Watching now" instead.

## 8. Changes nobody asked for (optional)

`auth:session` changes when the other device approves, `settings:remote` when
YouTube answers with the TV code. Neither moves `Source::revision()`. The
screens look again every two seconds (`data.act("auth:session")`;
`data.forget("settings:remote")` while the state is `connecting`). A revision
that also follows `tube_auth_revision()` and `tube_phone_revision()` would make
both immediate; the polling can stay as it is.

## 9. The password asked at start

When the provider's startup section is a protected profile
(`tube_boot_section()` = `library:profile:<id>`), the mount should open
`Route{ScreenId::accounts, "library:profile:<id>"}`: the accounts screen then
asks for that profile's password as soon as the list is there.

## 10. Interface texts

121 `tr()` texts in the package, 30 of them already in the catalogs (the
classic interface's own wording was reused wherever it exists), 91 new;
`scripts/i18n_extract.py` lists them. Three are the YouTube phone app's own
menu words and should be translated as that app writes them: "Settings",
"Watch on TV", "Enter TV code".

Snapshot values are tokens, not interface words, so the boards hold in any
language once the catalogs are filled.

## 11. What the app does that the approved boards word differently

- Board 41's body says history, favourites and the play queue of the profile
  are removed with the account. `account_removed()` / `profile_forget()` remove
  the sign-in, the profile's password, channel marks, caption memory and recent
  playlists, and continue as Guest. The question says what happens: "The
  sign-in is deleted from this console. You continue as Guest; other accounts
  stay available."
- Board 35's head says "Phone & casting"; the app labels the feature
  "Phone pairing (experimental)", and that label is the page's family line.
  The experimental note of the app's rows is on the page and on the password
  sheet.
- The provider keeps a password for the profile that is watching only
  (`settings:lock:*` act on the selected profile), and removes the selected
  account only: Triangle on another profile says "Watch as ... first", and
  removing another profile selects it first (asking for its password when it
  has one).
