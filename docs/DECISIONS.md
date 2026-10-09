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
