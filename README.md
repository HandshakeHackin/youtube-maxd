# YouTube Max’d downloads

**[Download the PS5 app ZIP](https://github.com/HandshakeHackin/youtube-maxd/raw/refs/heads/downloads/YouTube-Maxd-PS5.zip)**

Version 1.2.0 experimental preview, targeting firmware 11.00 / ShadowMount+.
The integrated build has not been tested on a PS5. The native build and host
checks passed; see TESTING.txt, TEST_RESULTS.txt and catalog-scan.txt.

`PPSA99991.zip` and `YouTube-Maxd-PS5.zip` contain identical bytes. Extract one
and copy the complete PPSA99991 folder to the location scanned by ShadowMount+,
commonly /data/homebrew/PPSA99991. Retain all assets, helper files and the voice
model. Use your configured console-side ELF loader or compatible Lapy service.
Back up existing app data and avoid title-ID collisions.

SHA256SUMS covers the app aliases, icon, metadata and testing/release documents
in this branch. The sce_sys copies match the packaged app and provide stable
store metadata. They are not a separate install.

[Source, build instructions and credits](https://github.com/HandshakeHackin/youtube-maxd)
are on main; corresponding application revision: 014adc2d901518f47fb4d1fbc99eab15b41b79aa.
The release publisher prepares a complete source archive with pinned dependencies.

Based on ProsperoTube, copyright (C) 2026 BlackBearReloaded, and EVO Player.
GPL-3.0-or-later; dependency notices retained. This branch does not replace source.
