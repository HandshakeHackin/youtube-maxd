# Browsing while the current player continues

Afterglow playback options offer **Keep playing while browsing** for an
active locally owned YouTube video. The preserved browser/frame route returns
without reopening or pausing that player. A compact Now playing/Paused panel
replaces the clock and shows the current title. **L3** returns to the full
player; pause and stop remain its existing Cross and Circle controls. This is
audio continuation, with no video picture-in-picture or second decoder.

The existing PlayerScreen lifecycle continues off-screen for gain, delay,
SponsorBlock observation, scrub completion and stream state. Hidden decoded
video is intentionally discarded through the existing serial retirement API
only after a successful browser present, preserving any prior picture's GPU
lifetime;
no YUV pointer is read or submitted to the GPU by that path. Leaving the
player already resets the destination rectangle before the next draw.
Browser sounds retain the existing playback preference and 6 dB attenuation.
Attract mode stays off while a background title is present.

At a drained end, background playback pauses before watched/completion/next
state is consumed. Returning to Player runs its existing completion mode and
visible next-video offer; no invisible countdown chooses another video.
Pending next offers, segment questions and navigation prevent background
entry. Automatic recovery that could reopen and navigate waits for the visible
player; a fatal decoder failure stops in the browser. An explicit replacement
keeps the old audio during resolution, then retires its background lease before
the selected stream/worker commits. A failed worker start stops that old audio.

Ownership ends on stop, soft close, Classic switching, source/profile/selection
retirement, phone takeover or returning to any foreground Player route.
Phone takeover clears the lease without stopping the phone's new owner.
Voice recording/transcription, discovery/sign-in, keyboards and the frame's
open questions/text/actions/launch keep priority over L3. Classic controls
are unchanged. No new persistent preference or provider ownership API exists.

Validation: generated entry/ownership/lifecycle/discard/EOF/return sanitizer
checks, existing picker/player dispatch checks, four-look background boards,
and read-only SDK compilation of touched objects. Console audio continuity,
EOF behavior, performance and return transitions still require acceptance.
