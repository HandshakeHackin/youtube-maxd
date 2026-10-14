# Updates

ProsperoTube can tell its user that a newer version exists and replace itself
with it. YouTube changes its service regularly; this is how a fix reaches an
installed copy without a computer.

**Status.** Host-tested only. Nothing on this page has run on a console, and
the check has never met GitHub: the repository is private, and an anonymous
request for its releases is answered with 404 (see
[Before the first public release](#before-the-first-public-release)).

## What the user sees

1. Some seconds after start, once the built-in service is up, the app asks for
   the list of releases, at most once a day. When a newer version is listed, a
   notice says so: *"Version 01.000.010 is available. Open Settings, About to
   read what is new and update."* It appears once per start.
2. **Settings > About** lists:
   - **Check for updates**, with the installed and the latest version. Selecting
     it asks again at once and reports the outcome, also when nothing is new or
     the server could not be reached. The menu of a main-menu section
     (Touchpad on its row) has the same row.
   - **Notify about updates: on / off** (SmartTube's switch). Off silences the
     notice at start; the daily check and the rows stay.
   - **What is new in ...**: the release's notes in the long-text reader.
   - **Update to ... now**, then a progress row (select it to pause; a paused
     download continues where it stopped), then **Restart to finish updating**.
   - **Skip version ...**: no notice for that version.
3. **Restart to finish updating** replaces the app's files and ends
   ProsperoTube with the PS5's own exit (if the console refuses that, the app
   parks and is closed from the PS5 switcher). Opening it again starts the new
   version; after ten seconds of
   running it says *"ProsperoTube was updated to version ..."* and the previous
   version's files are deleted.

A copy that cannot update itself still lists the newer version and says why:
a development build, a copy that does not run from
`/data/homebrew/PPSA99142` (a USB drive, another folder), or an image install.

## Where the mechanism comes from

The owner's other PS5 apps share one update kit, kept in
[ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
(`docs/UPDATE_CHECK.md`, `docs/SELF_UPDATE.md`) and used by ProsperoLight,
ProsperoAI, ProsperoEden, ProsperoTV, ProsperoRadio and ProsperoPuzzles:

- the app asks **homebrew.page** for its catalog entry, signed with Ed25519,
  with a sequence number against replay, and compares `contentVersion` values;
- the user is offered *Update now / What's new / Skip*;
- the app downloads the release ZIP from GitHub with libcurl and streams it to
  a **helper** (`self-updater.elf`) that the console's payload loader starts
  (port 9021). The helper checks size and SHA-256, validates and unpacks the
  archive beside the app, waits until the app has closed, swaps the top-level
  entries of the install folder by rename, refreshes the home screen's copies
  of `sce_sys` and posts a notification;
- lessons recorded there: unpacked files must be mode 0777 or the console
  refuses to start the app; the install folder itself must stay in place so
  the console's mount keeps pointing at it; a sandbox folder left behind by an
  app that ended badly must not count as "still running" (the helper waited
  two minutes for nothing); ShadowMountPlus's `mount.lnk` names the real
  install folder and `mount_img.lnk` an image that cannot be updated.

ProsperoTube keeps those rules and differs in three places, each for a reason:

| | The kit | ProsperoTube |
| --- | --- | --- |
| Source of truth | homebrew.page's signed catalog | The GitHub releases of this repository (`TUBE_UPDATE_MANIFEST_URL`, one constant in `native/tube_update.h`): the app is not listed in the catalog, and a fix must be offered the moment it is released |
| HTTPS | libcurl handle of the kit | The app's own client (`standalone/ps5/tube_js_http`), certificate verification always on, in a client of its own on the update worker: it works when the JavaScript service does not |
| Who replaces the files | A loader-started helper, after the app has closed | The app itself. It is already elevated through Lapy and writes under `/data`; no payload loader has to be running. In exchange the replacement is journaled and the previous version is kept, so a failed start is undone (the kit has neither) |

What the GitHub source does not give: a signature independent of GitHub. The
release's own SHA-256 (the `digest` GitHub records for each uploaded file)
arrives over the same verified HTTPS connection as the address; whoever
controls the repository controls the update, as with any release download.

## The sequence and its recovery states

Everything lives in `/data/prosperotube/update`:

| File | Holds |
| --- | --- |
| `check.json` | The last answer, when it was asked, the notify switch, the skipped version |
| `download.zip`, `download.json` | The part downloaded so far and which release it belongs to |
| `stage/` | The unpacked new version |
| `previous/` | The replaced entries of the installed version |
| `state.json` | The journal: `staged`, `swapping` (with every entry's name and whether it existed) or `applied` (with the count of starts) |

1. **Check.** `GET` the release list (nothing but a generic `User-Agent` and
   an `Accept` header). The highest version tag wins; drafts and tags that are
   not versions are passed over, pre-releases count. The release must carry
   `PPSA99142.zip` with a size, an address under this repository's
   `releases/download/` and a `sha256:` digest; otherwise it is named but not
   installable.
2. **Download.** Ranges of 4 MiB, each a request of its own, appended to
   `download.zip` and hashed. Redirects are followed by the engine, to
   GitHub's hosts over HTTPS only. A failed piece is retried three times; a
   failure, a cancel or a power loss leaves the part file, and the next try
   continues after it. Size and SHA-256 must match the release; a mismatch
   deletes the part.
3. **Stage.** The archive's directory is validated before anything is written
   (one app folder, relative paths, no links, special files, encryption,
   duplicates or ZIP64, bounded sizes), unpacked into `stage/` with mode 0777,
   every file checked against its CRC-32 while written and again read back
   from the disk. `stage/sce_sys/param.json` must name this title and the
   listed version. Then `state.json` says `staged`. Nothing installed has been
   touched; a staged update survives a restart of the app.
4. **Replace** (on *Restart to finish updating*). `state.json` says
   `swapping` with the entry list, then for each top-level entry of `stage/`:
   the installed one moves to `previous/`, the new one moves in (renames on
   the same drive). Entries the release does not name stay. Then `applied`.
   The home screen's copies of `sce_sys` are refreshed and the app closes.
5. **Confirm.** The next start counts itself in the journal. Ten seconds of
   running confirm the version: `previous/`, `stage/` and the journal go.

| Found at start | Meaning | What happens |
| --- | --- | --- |
| No journal | Nothing in progress | Leftover `stage/` and `previous/` are removed; a part download stays |
| `staged` | Ready from an earlier run | Offered again, unless this copy may no longer update itself |
| `swapping` | Power was lost during the replacement | Every moved entry is put back from `previous/`; "An interrupted update was undone" |
| `applied`, this version, fewer than two unconfirmed starts | First start of the new version | Counted; confirmed after ten seconds |
| `applied`, this version, two unconfirmed starts already | The new version starts and dies | The previous version is put back; the user is told to close and reopen; that version is no longer announced at start |
| `applied`, another version running | The replacement is not what the console started | Put back |
| An unreadable journal | | Ignored and removed with the leftovers |

A step that fails while the app runs (a rename, the journal) is undone on the
spot. What no journal can cover: a new version that cannot be loaded at all
(it never reaches the code that counts starts). Then the previous version is
still in `/data/prosperotube/update/previous`, to be copied back by hand.

Not enough space: on a PC build the free space is checked before the download
(four times the ZIP) and before unpacking. **On the console the app's runtime
has no `statvfs`**, so there the condition shows as a write that fails while
downloading or unpacking; both happen before anything installed is touched.

## Copies that never update themselves

- **A development build** (the instrumented package, `build_ps5.sh
  --usb-remote`): it lists newer versions and says it is a development build.
- **Not in `/data/homebrew/PPSA99142`:** ShadowMountPlus's record
  `/user/app/PPSA99142/mount.lnk` names another folder, or that folder holds
  no copy, another title, another version or another `eboot.bin` than the one
  running.
- **An image install** (`mount_img.lnk`).

## Trying it on a console before a release exists

Only a development build reads `/data/prosperotube/update-offer.txt`, and with
that file present it may run the whole sequence (the kit's "development
offer"):

```text
01.000.010
ProsperoTube 0.2.0 test
https://github.com/<owner>/<repository>/releases/download/<tag>/PPSA99142.zip
<sha256 of that ZIP>
<size in bytes>
## Notes
- any further lines are the release notes
```

1. Build the *new* version: raise the content version
   ([release procedure](BUILD.md#releasing)), build, attach `PPSA99142.zip` to
   a pre-release of a temporary public repository, note its size and SHA-256.
2. Build the *old* version as an instrumented package and install it; put the
   five lines above in `/data/prosperotube/update-offer.txt`.
3. Start it: the notice names the offered version, About lists it, **Update
   now** downloads, **Restart to finish updating** replaces the files and
   closes the app. Start it again: About shows the new version, and ten
   seconds later the update is confirmed.

A release build ignores the file. The address must be on `github.com`.

## Before the first public release

- **Make the repository public.** The check is an anonymous request to
  `https://api.github.com/repos/blackbearreloaded/ProsperoTube/releases?per_page=10`.
  While the repository is private GitHub answers 404 and the app says nothing
  (a check asked for by hand says "No releases were found at the update
  address"). The app carries no token and must not.
- **Release through the workflow** ([release procedure](BUILD.md#releasing)):
  the tag `v<content version>`, the asset name `PPSA99142.zip` and the
  `sha256:` digest GitHub records at upload are what the check requires.
- Anonymous API requests are limited to 60 an hour per address; the app asks
  once a day and on request.
- If the project is listed on homebrew.page later, the kit's signed catalog
  can become a second source; nothing in the app depends on it today.

## Tests

`bash scripts/test_all.sh update release` runs:

- `scripts/test_update.sh`: the engine with the app's HTTPS client against a
  loopback server (`tests/update_fixture.py`), plain HTTP and HTTPS with a
  certificate made for the run: versions; release metadata (malformed, no
  asset, bad digest, wrong address or size); the daily limit, the stored
  answer, silence on automatic failures; the headers sent; ranges, redirects,
  a server without ranges, retries; resume after a failure, a power loss and a
  damaged part; cancel between pieces and in flight; space; refused archives;
  a failure and a power loss injected at every step of staging and of the
  replacement; rollback after two failed starts; development, location and
  image refusals. The same cases run a second time through the folder
  functions the console build uses. Then About's rows through the shipping
  provider with a canned network.
- `scripts/test_update_host.py`: the generated host code that shows the
  notice, redraws About and closes the app.
- `scripts/test_release.py`: the release tag, the notes, the ZIP's
  permissions and the workflow's triggers.

## What only a console can show

- The check and the download through the app's HTTPS client in a client of its
  own, beside the service's, under Lapy elevation; GitHub's redirect to its
  file host; download speed.
- Writing, renaming and deleting under `/data/homebrew/PPSA99142` and
  `/data/prosperotube/update` from the elevated app, and whether large writes
  are slowed as they are for a sandboxed app.
- Replacing `eboot.bin` and the assets of the running app, the exit
  afterwards (media released, GPU parked, `sceSystemServiceLoadExec("exit")`), and that the next launch runs the new files without a remount.
- The refresh of `/user/appmeta/PPSA99142` and `/user/app/PPSA99142/sce_sys`,
  and reading `mount.lnk`.
- The running folder the app finds (`/app0`, `/system_ex/app/PPSA99142`).
- A full disk, and a power loss during the replacement.
- The notice on screen, the redraw of the progress row and the reader page.
