# YouTube phone receiver — backend integration

The companion implements YouTube's Lounge / MDX TV-code receiver protocol.
Registration, code generation, command reception and player-state feedback passed
a live temporary-receiver check. **The PS5 pairing screen and native command
execution are not integrated yet. This does not enable phone-to-PS5 playback.**
The existing PS5 playback candidate remains `9ac95e69`.

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

The next native batch must add opt-in pairing/reset settings, persist the stable
non-secret device/screen identity under `/data/prosperotube` through Lapy, scope
callbacks to the current provider/profile/generation, and execute player/queue
commands with acknowledged results and state reporting. Voice capture/search and
disconnect preferences still need implementation. Native reset must stop the
receiver and forget its saved IDs. Real phone and PS5 acceptance remains open.
