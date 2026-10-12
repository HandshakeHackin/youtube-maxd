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
