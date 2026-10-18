# YouTube Max’d — next experience build

These additions are developed on `feature/maxd-next-experience`. The published
`v1.2.0-preview.1` binary and pending Homebrew Store submission do not contain
them. This branch is a test candidate, not a hardware-verified release.

| Addition | Use | Practical limit |
| --- | --- | --- |
| Transcript search and bookmarks | R2 → Transcript & bookmarks. Search available captions, select a line to seek, or name the current timestamp. Square deletes a selected bookmark. | Up to 4,096 caption cues, 256 matches per search, 64 bookmarks per video and 512 total. Bookmarks are local and profile-owned; VOD timestamps only. |
| In-app picture-in-picture | Enable Afterglow, then R2 → Picture-in-picture. Browse/search while the same video continues in the bottom-right corner. Tap L3 to return. | Inside YouTube Max’d only; one decoder, not a window over PS5 games. Classic still supports full-screen playback. |
| Push-to-talk playback commands | Hold L3 until Listening, speak, release; Circle cancels. | Local Whisper recognition; microphone routing and latency still need a PS5 test. No always-on microphone. |
| Session builder | YouTube preferences → Session builder. Create a session, choose a source and 15/30/45/60/90/120-minute budget, then Open playlist. | Chooses unwatched VODs closest to the budget without exceeding it. First eight feed pages, at most 128 matching candidates. Network/player delays are outside the runtime budget. |
| Local family profiles | Accounts & library → Local family 1–4; then YouTube preferences → Family profiles. Set a parent PIN, name the profile, approve exact channel IDs and enable limits. | Independent signed-out sessions. No imported Google/Family Link identity, subscription transfer or YouTube Kids safety certification. |
| Party queue | YouTube preferences → Party queue → Start party room → Show invite QR code. Guests join on phones on the same LAN. | PS5 hosts the page at its private IPv4 address on port 8766. Up to eight guests and 64 suggestions; host approval before queueing. |

## Voice commands

- `pause`, `pause video`, `resume`, `play`
- `forward`, `skip forward`, `rewind`, `go back` (ten seconds)
- `forward 30 seconds`, `back 45 seconds` (one to 300 seconds, VOD)
- `captions on`, `captions off` (turning on uses an already loaded caption track;
  choose a language in the subtitle menu first if needed)
- `quality 720p`, `quality 1080p`, `quality 4k` (only an actually available format)
- `add to queue`, `queue this video` (idempotent local addition)

Unrecognized speech remains a search query while browsing. During playback it
shows command help. Quality changes use the existing quality reopen path and
may briefly interrupt playback. Commands do not post Google account changes.

## Family controls

The four local slots have separate signed-out YouTube clients and separate app
history, bookmarks, queue and settings. Existing signed-in app profiles can also
have local restrictions. Profile names are local labels.

The parent PIN is salted and hashed using the app's existing password-record
implementation. Five unsuccessful attempts impose a 30-second pause. A successful
PIN opens a three-minute settings window; use **Lock parental controls now** when
finished. Changing profiles retires that window. Leaving a restricted profile or
opening account/settings screens requires the parent PIN.

An enabled profile can play only videos whose resolved **channel ID** matches
one of its approved IDs. An empty list blocks playback. Unknown channel metadata
is denied, including direct video links. Approve the current video's channel
before enabling restrictions, or enter its exact `UC…` channel ID. Search can be
disabled. Browsing can still display videos from other channels; the allowlist
controls playback, not every thumbnail returned by YouTube.

Viewing time counts active, unpaused playback, including background/PiP. Usage is
saved at ten-second intervals and rounded up when persisted. The daily allowance
uses the PS5's **UTC calendar day**, resets only when the day advances, and stops
playback at the limit. Keep the console clock correct. These are local app
controls on a jailbroken device, not protection against editing its data files.
Party rooms are disabled on restricted profiles. The app never uses a parent's
Google token to impersonate a local child profile.

## Party queue

The room is off at startup. It binds only to the selected private IPv4 interface;
no router setup, companion PC, Google Cloud project or Google credentials are
needed. If automatic address discovery fails, set the PS5 address shown in
Settings → Network → View Connection Status.

Invites expire after ten minutes; a room ends after two hours, on stopping the
room, or on changing app profile. Stop and restart to rotate the invite and
revoke guest sessions. The invite is in the URL fragment; guest session tokens
stay in the phone page's memory. The page exposes only pairing, suggestions,
votes and the party list. It does not expose the app's private account API.
Use it on a trusted LAN: the local page uses HTTP, not TLS.

Each guest can vote once per video. Suggestions are YouTube IDs or supported
YouTube links, never arbitrary media URLs. Guest labels are explicitly unverified.
The host reviews each suggestion on the PS5, approves it into the local play
queue, or rejects it. Open **Play queue** and start a video to play the approved list. Votes order pending approved party entries, with first
suggested winning ties. They do not interrupt the current video. A consumed or
manually removed entry is not silently re-added by later votes. Already approved
local queue entries remain when the room closes.

## Linked Google profiles: research only

Google device-code login currently yields the selected account/channel context.
The official mobile account picker can include supervised child identities that
are not enumerated by this OAuth flow. This branch does not offer an import button,
claim discovery, copy the parent into a child slot, or call a YouTube Kids write
operation to probe for identities. Read-only discovery and consent requirements
need further research and a supported live test environment.

## Validation and acceptance

See [MAXD_NEXT_VALIDATION.md](MAXD_NEXT_VALIDATION.md) for recorded host results.
Before publishing a new installable release, test firmware 11.00 with ShadowMount+:

1. Sign-in/profile changes, persistence, PIN lock on return/restart and account removal.
2. H.264/VP9/HEVC/AV1, HDR/SDR PiP entry/exit, pause, seek, EOF, quality reopen and GPU/source retirement.
3. DualSense microphone selection, hold/release/cancel, accuracy, recognition latency and restored pause state.
4. Caption availability/automatic captions, timestamp jumps, bookmark reload and live-video limits.
5. Family channel denial from search, queue, direct links and phone launches; daily limit in foreground/background/PiP.
6. Phone QR scanning, Wi-Fi isolation/firewall behavior, slow/disconnected guests, votes, approval, queue consumption, room expiry and profile-change revocation.
