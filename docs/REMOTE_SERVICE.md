# YouTube phone receiver — backend integration

The companion implements YouTube's Lounge / MDX TV-code receiver protocol.
Registration, code generation, command reception and player-state feedback passed
a live temporary-receiver check. Native pairing settings and identity storage are
integrated and host-tested. **Player command execution remains unconnected.
This does not enable phone-to-PS5 playback.**
The existing PS5 playback candidate remains `9ac95e69`.

The native `tube_remote` driver is now included in generated PS5 build inputs.
It has passed ASan/UBSan command/scope/feedback tests and a PS5 SDK object compile.
It provides opt-in asynchronous registration/pair/poll/report/stop, typed commands,
owner-tagged acknowledgments and stable device/screen identity getters.
The provider now calls the driver; player execution remains pending.

## Native pairing settings

YouTube preferences > Phone pairing provides Link a phone, connection On/Off,
and Reset phone pairing. The screen refreshes when the receiver returns its code
and formats it into groups of three digits. Opening preferences does not enable
the receiver. Linking explicitly enables it and requests a code; an enabled saved
receiver starts again with the application. Turning it off closes the receiver
while retaining its identity; reset closes it and forgets both IDs.

The native device UUID is created before registration using the PS5 C library's
random source. Enable/device/screen fields live in the atomic `youtube.json` under
`/data/prosperotube`, through the existing Lapy-backed data path. The UUID must be
saved before advertising; a returned screen ID is saved before pairing continues.
Failed writes stop advertisement and display a local-data warning. Neither the
TV code nor opaque receiver session is persisted. Corrupt/future storage remains
protected. Identities survive restart; late registration after reset is closed
without restoring the forgotten IDs. Browser cancellation keeps an explicitly
enabled receiver; provider shutdown closes it.

The provider workflow passed mock-transport ASan/UBSan tests, including blocked
atomic save, reset, restart and late cleanup. The provider, driver and RML host
objects compiled with the PS5 SDK. A full package, console pairing, random-source
runtime behavior and real phone acceptance still need verification.

This is original Python implementation based on the pinned SmartTube service
[Lounge interface](https://github.com/yuliskov/MediaServiceCore/blob/4d9767733aa92269b9d24f24844a7fa1bb5196c1/youtubeapi/src/main/java/com/liskovsoft/youtubeapi/lounge/LoungeService.java)
and its command models. Application framing follows
[Closure ChannelRequest](https://github.com/google/closure-library/blob/master/closure/goog/net/channelrequest.js):
decimal length, newline, JSON, with lengths measured in UTF-16 code units.
No Android or MediaServiceCore source is incorporated.

## Authenticated API

POST `/v1/remote` uses the companion Bearer token and bounded JSON requests.
There is no receiver, pairing request or upstream connection at companion startup.
Only an explicit `start` enables the receiver. The Lounge transport runs separately
from the library's serialized Node worker.

| Action | Input | Result / behavior |
| --- | --- | --- |
| `start` | Optional stable `device_id` UUID, saved `screen_id`, display `name` | Opaque `session`, `device_id`, `screen_id`, name and listening status |
| `pair` | `session` | Nine- or twelve-digit `pairing_code` for the YouTube phone application's Link with TV code screen |
| `poll` | `session`, `ack` (initially zero) | At most 32 ordered typed commands; unacknowledged commands remain available on retries |
| `update` | `session`, increasing positive `update_id`, `player` | Sends now-playing, playback clock/state, volume/mute, previous/next and captions to the phone |
| `stop` | `session` | Cancels the owned stream, clears commands/secrets and attempts bounded channel termination |

`player` contains `video_id`, numeric `state`, `position_sec`, `duration_sec`,
`playlist`, `index`, opaque `ctt` returned by setPlaylist, `volume` (0–100),
`muted`, `has_previous`, `has_next`, caption `language` and `vss_id`.
State values follow the reference: playing 1, paused 2, idle 3, ready 4;
undetected 0 and undefined -1 are accepted. Repeating an accepted update ID with
identical data returns its previous success without sending another packet.
Older IDs and different data under the same ID are rejected.

Incoming commands include set/update playlist, play/pause, next/previous, seek,
volume, stop, now-playing query, phone connection events, device list, D-pad,
voice initiation/cancellation and caption selection. Unknown command names are
ignored. Known commands expose only validated fields, including a queue of at
most 128 video IDs. Phone state does not prove native playback occurred.

The native integration must apply each command on the main thread before
acknowledging its sequence. Polling does not consume commands. Acknowledgments
cannot exceed the highest delivered sequence or move backwards. Wire IDs are
deduplicated; missing IDs, malformed frames and queue overflow fail the channel
without silently dropping a playback command.

## Lifetime and native integration gate

There are at most four active receiver contexts, each with a 120-second renewable
native lease and a 128-command queue. Normal long-poll completion opens another
back-channel request. Transport errors require explicit restart; uncertain state
reports are not automatically replayed. Failure clears pending commands and
stops the stream, so delayed callbacks cannot revive the receiver. Stop and
companion shutdown cancel owned tasks and clear private protocol state. Channel
termination is best effort with a two-second deadline.

Lounge tokens, SID and gsessionid remain private to the companion; they are never
returned through the API, formatted in errors or included in receiver repr.
Upstream URLs are fixed to YouTube; no request can supply an arbitrary host.

The native player adapter now executes playlist launch through EVO's existing
resolver/fallback/worker path. It acknowledges only after successful startup,
keeps fractional offsets, rejects retired owners and reports the actual selected
profile/source player. Play/pause, pause-preserving seek admission, stop,
next/previous, queue replacement, PCM volume, D-pad and current-track caption
enable/disable are connected. A separate ephemeral phone queue preserves the
saved local queue and advances after proven presentation EOF. Playlist windows
retain the sender's global index and click-tracking parameters.

Volume scales the final S16 output with an atomic main/audio-thread gain; mute
does not change the media clock or retirement accounting. Unknown caption
languages and voice capture are not silently acknowledged. Voice capture/search,
caption-language switching, disconnect preferences and playlist continuation
beyond the supplied 128-ID window still need implementation. Real phone and PS5
acceptance remains open.

The driver keeps receiving/renewing the lease while commands wait for application.
It validates an entire response before changing its command queue. If local
execution advances acknowledgment during an in-flight poll, the returned applied
prefix is discarded and the outstanding prefix must remain identical. A command
owner changes with the provider/profile/identity scope, so a delayed launch result
cannot acknowledge a same-number command from a new receiver. Late successful
registration after scope change is closed through its original source/token;
new registration waits for owned cleanup callbacks to complete. Pairing identity
and code revisions can be used by the provider's persistence/UI integration.
