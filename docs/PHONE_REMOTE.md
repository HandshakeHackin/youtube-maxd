# Phone remote and bug-report export

Available in YouTube Max’d **v1.4.0**. Both run entirely on the PS5. No phone
app, PC companion, Google Cloud project or paid service is required.

## Pair a phone

1. Connect your phone and PS5 to the same trusted home network.
2. Open **Settings → Phone remote** in Afterglow, or **Phone remote** in the
   Classic settings list. Select **Start Phone remote**, then **Show pairing QR code**.
3. Scan the QR with your phone camera and open the link. The page is served by
   your PS5 on TCP **8767**. Use the phone's **Back** button to leave the TV's
   QR/settings page before submitting a search.
4. Type a search on the phone. Results open on the TV. Use the navigation pad
   and Select to browse/play them. Pause/Play and −10/+30-second seek controls
   operate on the current video. Seeking is for videos on demand.
5. Paste a YouTube video link or 11-character ID to add it to your local queue.
   **Play next** moves it to the front of upcoming entries; **Remove** removes
   only the queue entry. Select a video on the TV to start it immediately.

The remote exposes up to 64 queued entries. Adding an existing entry is
idempotent. Search text is always a literal search, including words like
“pause”; it is never interpreted as a voice command. Keep dialogs and keyboard
entry on the TV closed when using remote controls. Searches from playback
return to browsing; Afterglow can retain the current video in background mode,
with **PiP off** unless explicitly enabled separately.

Pair one controlling phone at a time. The one-use QR expires after **five
minutes**. Its paired session expires after **two hours**, even if you keep
using it. Stop/restart Phone remote to revoke access and create a new QR.
Profile/source changes and app exit also disconnect it. Remote access never
starts automatically, and restricted family profiles cannot host it. Existing
YouTube-app casting and party guest voting use separate sessions; party guests
do not gain full remote controls.

If pairing fails, stop/start the remote and scan the new QR. If automatic
address discovery fails, enter the PS5's **private IPv4 address** in the remote
settings. Find it in the console's Network → View Connection Status. Guest Wi-Fi,
client isolation or a VPN may prevent phones from reaching the PS5.

This is **local HTTP**, with a random QR secret and authenticated control
requests. Use a trusted LAN; do not forward port 8767 to the Internet. The QR
secret is removed from the browser URL after opening. The phone keeps its
session in tab-scoped storage so a refresh can reconnect; clearing/closing the
tab or session revocation requires pairing again. No Google credentials are
entered on the remote page.

## Export a bug report

On the paired phone, select **Capture & preview report**. Review the JSON, then
select **Download report (.json)**. The PS5 also writes a single report to
`/data/youtube-maxd/bug-report.json`. New captures overwrite that file.

On the PS5, **Settings → About → Bug-report export** (or the Classic settings
list) offers **Capture**, **Review** and **Delete captured report**. The phone
remote need not be enabled to capture locally. A phone download still requires
a paired remote session. Only reports captured during this app session can be
downloaded; a prior file is never loaded or served automatically.

The report contains a fixed technical schema: app version, interface/player
flags, selected quality limits, codec, dimensions, frame rate, position/duration
and decoded/dropped frame counters. Buffering reflects seek/input-failure state
at capture; this is a snapshot rather than a complete rebuffer/crash trace.
Capture while the problem is visible, before stopping playback, for useful
codec and counter information.

It excludes account/profile names and IDs, tokens, cookies, IP addresses, video
IDs and titles, search terms, media URLs and raw logs. It does not read your
configuration or console logs. The app never uploads reports automatically.
You decide whether to attach the reviewed file to a
[GitHub issue](https://github.com/HandshakeHackin/youtube-maxd/issues/new).
Include a written description and the PS5 firmware/runtime you tested; those
are not detected or assumed by the report. Check screenshots you attach for
personal information too.

## Validation boundary

HTTP authentication, pairing/expiry, request framing, command deduplication,
player-selection guards, queue/profile isolation, the report schema and file
export are covered by host tests. UI screenshots use fixtures. This release
still needs an actual firmware 11.00 / ShadowMount+ PS5 test for LAN binding,
phone-browser controls/downloads, background playback and player behavior.
