# Playback and service reference candidates

These are source references, not evidence that ProsperoTube implements their
features. Platform-specific decoder and presentation contracts must remain
separate. Research was expanded beyond the owner's repositories.

| Reference | Pinned revision | Useful boundary | Reuse decision |
| --- | --- | --- | --- |
| [EVO Player](https://github.com/sainsaji/EVO-PLAYER-PS5) | `9715f6ee9bf9b53461d6fdf9a2e11b4c48357783` | Integrated PS5 media engine, Videodec2/AGC, FFmpeg, TV UI, input and packaging | Existing pinned GPL-compatible foundation; extend its provider boundary |
| [ProsperoTV](https://github.com/blackbearreloaded/ProsperoTV) | `a33031e9e50b4942ca0e2287cc757271345d874f` | PS5 browse/search, bounded media delivery, VP9 WebM and hardware test orchestration | Reference existing PS5 lifecycle and delivery contracts; preserve application/source licensing |
| [ProsperoLight](https://github.com/blackbearreloaded/ProsperoLight) | `b0149a941617c9e92a3be475b78304052c718d7c` | PS5 Videodec2, AGC, HDR, presentation ownership, audio and controller lifecycle | GPL-compatible application source; audit dependencies before integration |
| [PS5 hardware video research](https://github.com/blackbearreloaded/ps5-hardware-video-decoding-research) | `59373c33b8eabd111a3bf4459041bfab5c8d48c5` | AVC/HEVC and VP9 profile 0/2 surface contracts, compound packets, hidden/show-existing frames, pipeline drain | GPL-compatible original examples and measured contracts; validate the same contracts in ProsperoTube |
| [PS5 audio research](https://github.com/blackbearreloaded/ps5-audio-decoding-research) | `26be5a12f92b2cdc67f8a9e082fd69ae0e9a35a2` | Audio codec/output research | Reference; identify applicable public interfaces before reuse |

## ProsperoLight findings relevant to this port

ProsperoLight uses Videodec2 for media decoding and AGC for presentation,
conversion and scaling. This is a hardware decoder plus GPU presentation;
it is not evidence that GPU shader cores decode ordinary YouTube codecs.
Its Moonlight stream orchestration is tightly coupled to its protocol and
presentation state, so it cannot be dropped into the EVO player unchanged.
YouTube playback also needs container demuxing, timestamp ordering, seeking,
buffering and caption/UI composition.

The separate video research records controlled PS5 VP9 profile 0 and profile 2
decoding, including 4K and HDR-target presentation. This materially improves
the PS5 candidate beyond the earlier software-only first-version choice.
It does not establish AV1 support, 8K, or ProsperoTube product acceptance.
Use the exact profile, color transfer, pitch and ownership contracts; do not
infer them from a generic codec name.

The pinned EVO foundation already contains
`projects/evoplayer/media/src/evo_vdec_native.c`: PS5 AVC, HEVC and VP9 hardware
decode, Annex-B filtering, VP9 packet splitting and timestamp reordering.
Its `package-app.sh` includes this backend with secondary decoders enabled by
default. Reuse and validate that boundary before replacing it. Earlier
prototype descriptions of a software-only PS5 path understated the compiled
foundation; inclusion does not prove the active decoder in a ProsperoTube run.
EVO's `scripts/build-ffmpeg.sh` is already the project's PS5 FFmpeg recipe.

ProsperoLight's AGC presenter references embedded resources under `assets/private`.
Asset provenance must be established before copying that presenter or its
binary inputs; a source license alone does not establish every asset's
provenance. Prefer original shaders through an open graphics toolchain.

## Reverse engineering handoff

No external decompilation is currently required: public source already describes
the immediate decoder and audio interfaces. If a concrete capability or ABI
gap remains after source integration and a bounded hardware probe, document
that exact question, expected behavior, target firmware and reproducer first.
An external agent's useful deliverable would be an independently written
interface/behavior contract and validation observations, rather than an
unscoped dump or copied implementation. No decompilation has been delegated.
