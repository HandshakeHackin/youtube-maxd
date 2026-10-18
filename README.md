# YouTube Max’d downloads

[Download the PS5 app ZIP](https://github.com/HandshakeHackin/youtube-maxd/raw/refs/heads/downloads/YouTube-Maxd-PS5.zip)

This is the verified 1.2.0 experimental build targeting firmware 11.00 and ShadowMount+. Native compilation and host checks passed; console acceptance is still pending. A bundled local voice model is included.

Extract `YouTube-Maxd-PS5.zip`, then copy the complete `PPSA99991` folder to your homebrew location, commonly `/data/homebrew/PPSA99991`. Close the old app and back up its folder before replacement. Retain all packaged assets and helper files. The content version is 01.002.000.

`SHA256SUMS` verifies the app and launcher icon in this branch.

## Source and licenses

The complete project, build instructions, notices and pinned dependency references are on [main](https://github.com/HandshakeHackin/youtube-maxd/tree/main). Clone with its nested submodules:

```sh
git clone --recurse-submodules --branch main https://github.com/HandshakeHackin/youtube-maxd.git
```

See the [build guide](https://github.com/HandshakeHackin/youtube-maxd/blob/main/docs/BUILD.md) for the verified dependency recipes and additional pinned UI-kit input. Based on the credited ProsperoTube/EVO sources, GPL-3.0-or-later, with dependency licenses retained. This downloads branch stores an unchanged compiled ZIP; it does not replace the source on main.
