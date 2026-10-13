# First-version implementation decisions

The first version uses EVO Player at
`9715f6ee9bf9b53461d6fdf9a2e11b4c48357783` as its native playback foundation.
ProsperoTube maintains a small, reproducible overlay rather than copying or
rewriting its decoder, audio clock, queues, seeking, or application lifecycle.
Only the YouTube provider is registered in the resulting application.

The first-version service runs on a user-controlled PC/LAN companion using
yt-dlp. This explicitly changes the initial standalone milestone: Android,
WebView proof tokens, and SABR are not ported to PS5. No SmartTube or
MediaServiceCore code is copied. The interface and provider are original work.

The companion resolves media and relays requests using the resolver's headers
and network context. Signed YouTube URLs and cookies remain on the host. The
native client receives short-lived companion session URLs and re-resolves stable
video IDs when necessary. Standard, separate-track DASH is preferred when its
controlled fixture passes; progressive media and live HLS remain alternatives.

ProsperoTube uses independent title and storage identities. GPL-3.0-or-later
applies to the application because it derives from EVO/ProsperoPlayer. Original
copyright notices and upstream licensing are retained in the dependency.

All build and fixture results are host evidence. Console decoding, presentation,
audio synchronization, controller dialogs, and supported firmware require a
hardware acceptance record. Offline fixtures cannot establish current YouTube
service compatibility. Google authorization, server watch-history mutations,
casting, HDR, and a standalone token/SABR runtime remain separate milestones.

## 2026-10-11: the service runs inside the app (supersedes the LAN companion)

The owner made a self-sufficient console application a hard requirement. The
decision above that "the first-version service runs on a user-controlled PC/LAN
companion" no longer holds for releases; the companion stays as a development
tool and as the reference implementation of the HTTP API.

**What replaces it.** The application embeds QuickJS-NG 0.17.0 (interpreter
only) and runs a bundle of YouTube.js 18.1.0 with the companion's own library
modules. A listener on `127.0.0.1` answers the same API the companion
answered, so the native provider changed its address, not its protocol. HTTPS
is libcurl on the OpenSSL the app already linked, verified against an embedded
Mozilla root list. Sources: `standalone/`, `standalone/ps5/`,
`scripts/embedded_service_overlay.py`.

**Why this was possible without the things the earlier decision ruled out.**

- *One client identity.* The service asks YouTube as the `VISIONOS` client.
  Its media links need no signature deciphering, no `n` transform and no proof
  token, and can be read at any offset. `ANDROID_VR` and `IOS` links answered
  403 some tens of megabytes into a file; `WEB` offers SABR only; the TV
  clients were refused. yt-dlp 2026.08.19 uses the same client by default.
- *No player script.* Because nothing is deciphered, YouTube's player script
  is never loaded (its analysis took 10 s and 376 MB in this engine and is
  refused by default).
- *Direct links, no relay.* Manifests name YouTube's servers; the console's
  FFmpeg reads them. Nothing has to stay running on another machine, and no
  media passes through the engine.
- *Seeking.* With the repository's DASH patch (already in the console's
  FFmpeg) a seek is a new ranged request: against a paced local server every
  seek finished in well under a second in MP4 and WebM; live, 1080p60 H.264
  took 1.4 s at the start, 4.9 s in the middle and 0.6 s near the end, VP9
  0.6 / 5.6 / 0.7 s. Without the patch a forward seek reads everything in
  between.
- *Address family.* Links are bound to the address that asked for them; the
  service's client is IPv4-only because the player connects over IPv4 (on the
  console the resolver returns IPv4 addresses only).

**What it costs.** About 4 MB more in `eboot.bin`; a 128 MiB cap on the
engine's heap (26-29 MB seen); accounts, phone casting, voice search and
subscriptions import are not in the built-in service yet.

**Risks accepted.**

- The whole of playback rests on one client identity. If YouTube puts proof
  tokens on `VISIONOS` links, playback stops until the bundle is updated; the
  bundle must become replaceable without a new package.
- Expired or refused links answer 403 in the player, not in the service; the
  player now treats that as a failed input and resolves again (three times at
  most per video).
- The player's TLS (FFmpeg, EVO's HTTP client for thumbnails) does not verify
  certificates; only the API traffic is verified.
- None of it has run on a console: the engine, libcurl under Lapy elevation,
  the loopback listener and direct playback are host evidence only.

Evidence: [VALIDATION.md](VALIDATION.md) (entry of 2026-10-11, standalone),
`standalone/README.md`, `standalone/ps5/README.md`.

## 2026-10-13: accounts in the built-in service

Sign-in, account lists and account changes are now answered by the service
inside the app, with the same HTTP contract the companion had
([library service](LIBRARY_SERVICE.md)). Everything here was tested offline
against canned answers; **no real account has signed in with this tree**.

**Where the credentials live.** One file,
`/data/prosperotube/accounts/oauth-profiles.json` (mode 0600 in a 0700
directory), in the companion's format: version 1, at most eight profiles, each
with an ID `oauth_<16 hex>`, the name, and Google's access token, refresh
token, expiry and the public TV client identity. It is written to a temporary
file, flushed, then renamed over the old one, and read back before the write
counts. A file that cannot be understood is never overwritten: accounts are
then unavailable (`auth_storage`), Guest keeps working, and the file has to be
removed by hand.

**The console offers no encrypted storage.** The tokens are plain text in the
app's data directory. Anyone who can read that directory (FTP or a file
manager on a console running homebrew, a copied backup of `/data`) can use the
account's YouTube access until the sign-in is revoked at
<https://myaccount.google.com/permissions>. The scope is the TV device scope
(YouTube account access), not the Google password. What limits the exposure:
the file is separate from `youtube.json`, so it never enters the app's own
backups; tokens are never logged, never returned by the API and never sent to
anything but Google's and YouTube's API hosts over verified HTTPS; removing
the account in the app deletes them from the file. Removing an account does
not revoke it at Google (as in SmartTube); the README says how to do that.

**What survives a restart of the app.** The accounts and their tokens. Not
kept: sign-in sessions in progress (a code shown before the app closed is
dead), list cursors, and the one-attempt records of account changes.

**Backups do not hold sign-ins, and there is no option to add them.** A local
backup (YouTube preferences > Backup and restore) is a copy of `youtube.json`:
the settings and the library kept on the console, for every profile. The
credential file is not copied, by the automatic backup either, and the screen
says so when the built-in service is in use. An "include sign-ins" choice was
considered and left out: the backup folder is read by whoever can read the
data directory, every further copy of a refresh token is one more thing that
stays valid until it is revoked at Google, a backup would become a pair of
files to validate, rotate, delete and restore together, and restored tokens
would have to replace the accounts of the running service (stop it, which
retires its playback sessions, sign-in sessions and one-attempt records, and
start it again). A new sign-in costs a minute and none of that.

A restore therefore neither adds nor removes an account. What a backup holds
for an account profile (its history, favorites, queue and followed channels,
and its own preferences when settings are separate per account) is filed
under the profile ID, so it is used again only while that sign-in is still on
the console. An account that signs in again is a new profile with a new ID:
it starts with an empty library and default preferences, and the earlier
profile's library rows stay in the file unused. If the restored file selects
a profile that is no longer signed in, the next profile list moves the
selection to Guest (`tests/test_embedded_service.c`). Guest's library and the
settings shared by all profiles always return.

**One-attempt guards in one process.** The companion kept them in Python so
they outlived the Node worker. Here the library object is rebuilt whenever an
account is added or removed, so the same records sit above it
(`standalone/js/accounts.js`, `Guards`): a change that was sent and not
confirmed is never sent again under the same confirmation, a confirmed one
answers from the record, and open watch-history sessions are closed by a
rebuild. They are in memory for thirty minutes and are **not** written to
disk: a confirmation belongs to one screen of the running app, the app
forgets it when it closes, and a repeat after a restart is a new, explicit
action by the user, as it was after a companion restart. A change interrupted
by the app closing may or may not have reached YouTube; the lists show the
truth when they are loaded again.

**Signed-in playback: every profile resolves as Guest.** The device-code
sign-in authorizes the TV client only. Its player answers carry signature
ciphers that need YouTube's player script, which this engine refuses by
default (ten seconds and hundreds of megabytes), and the companion never
proved the TV script's challenges against a real account either. The Guest
path (`VISIONOS`, no credentials) was measured live. So:

- A signed-in profile resolves videos exactly as Guest does. The bearer token
  is not part of any player request.
- Watch history is reported separately through the account session
  (`watch_progress`: tracking data, one history entry, position updates), with
  the provider's 30-second throttle and no retry after an unconfirmed report.
- Videos that need the account (age-restricted, private, members-only,
  purchased) answer `authentication_required`, as for Guest.
- The path that asks the account session for such a video exists
  (`standalone/js/account_playback.js`) behind `account_playback` in
  `tube_service_options`, **off**. Health reports `account_playback: false`.
  It is to be switched on only after it worked with the owner's account, and
  ciphered answers additionally need `allow_player_script`.
- Search and channel pages opened through the catalog jobs are read with the
  Guest session in every profile (public data, the measured path); playlists
  and every library action use the account session.

**What is not verified until a real sign-in.** That Google accepts the TV
client identity read from YouTube's TV page inside this engine; that the
pinned YouTube.js parsers understand what the TV client returns for the
account lists (the offline answers are hand-made in the WEB shapes those
parsers expect); every account change; history reports. A list that fails
does not affect Guest browsing or playback.

**Also decided.** `playlist_delete` now posts `playlist/delete` itself: the
pinned library's own method builds an endpoint without an API path and throws
before sending (found by the offline tests). Subscriptions import stays out of
the built-in service: it needs a larger request limit in the listener and a
file path on the console, which is more than this step.

## 2026-10-13: subscriptions import and the phone receiver in the built-in service; voice search left out

Later the same day the two remaining companion-only features moved into the
app, and the third was weighed and left out. Offline only: no phone was linked
and nothing was sent to YouTube ([validation](VALIDATION.md)).

**Subscriptions import.** The parser is ported as it is. The listener keeps
its 8 KiB body limit and lets this one route carry 1 MiB, one such body at a
time, received into a block that goes to the engine without a second copy.
The file path is the provider's, as before: it reads the user's file from the
import folder and posts it.

**The phone receiver, and how its back channel is read.** YouTube's Lounge
service answers the back-channel request with one long-lived chunked response
(`CI=0`), and the service's HTTPS client delivered whole bodies. Two ways:

- ask for the buffered form (`CI=1`), in which the server ends the response
  after each batch of frames, and keep the client as it was; or
- teach the client to hand a response over in pieces.

The second was chosen. The buffered form is a different server path than the
one the Python receiver was checked against live, nothing in this repository
shows YouTube's phone service honouring it, and this step may not ask YouTube.
A streaming request keeps the wire identical to the checked one and costs
little: one flag on a request, one call that hands over what arrived, the
request's time limit read as its longest silence, the same completion and
cancellation as before, in the C that already runs on host and console
(`standalone/ps5/tube_js_http.*`). Its use is confined to the receiver.

**Identity stays with the provider.** It creates and saves the receiver's
device and screen identifiers and passes them with every start; the service
stores nothing about the receiver, so that "Reset phone pairing" forgets
everything there is to forget. Tokens and sessions of the phone service exist
in memory only.

**Offered by the feature list.** The service lists `subscriptions_import` and
`remote`; the provider shows the rows unless a service it has asked does not
list them, the rule accounts already follow.

**Not done at exit.** Stopping the service on the first soft-close frame does
not wait for a request that ends the channel upstream; the phone learns of the
exit when YouTube notices the closed connection. The exit path was accepted on
hardware and is not lengthened for this.

**Voice search is left out.** A recogniser on the console means whisper.cpp
and a 31 to 75 MiB model as new third-party code and data, with memory and
time that nobody has measured there; YouTube.js has no speech endpoint and
YouTube's own recogniser is a cloud service. With a phone linked, the YouTube
app's voice search does the job from the phone. [Assessment](VOICE_SEARCH.md).

## 2026-10-13: the Afterglow interface

The owner approved the Afterglow proposal on 2026-10-13
([design notes](afterglow/design-notes.md), [sound](afterglow/sound-design.md),
[feasibility and plan](afterglow/feasibility-report.md)) and decided that the
first release carries all of it: no version is published until every proposed
screen is built and validated on a console.

- The interface is built on the UI kit, the route the feasibility report calls
  A, in slices: the frame and Home first, then one screen after another. The
  RmlUi browser stays as the classic interface and the fallback until every
  screen exists.
- Afterglow is the default look; Onyx, Daybreak and High contrast are the same
  design in other palettes. The proposal's controller map stands. The kit's
  existing sound sets are used until new recordings exist. Attract mode starts
  after 45 seconds. The in-app mark is the one shown in the proposal; the
  home-screen icon and launch pictures under `native/branding/` stay.
- What transfers to the RmlUi browser is already in it: the **Theme**
  preference (the four palettes ahead of the nine classic colour schemes,
  expressed through the palette tokens of `native/youtube.rcss`) and the mark
  as a decoded picture in the header and on About. A profile saved before the
  themes carries `default`, which names no scheme any more, and so opens in
  Afterglow; a classic scheme that was chosen stays chosen.
