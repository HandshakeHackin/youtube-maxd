# YouTube's "Ask" about a video

A tester asked for YouTube's **Ask** button: a Gemini chat about the video on
the watch page, with suggested questions and free-text questions. This page
records whether ProsperoTube can offer it (study of 2026-10-16).

## Verdict

**Not possible yet.** The protocol that is known to work is the **web**
client's, and it needs a signed-in **web cookie session**. ProsperoTube's
accounts are TV device-code sign-ins (OAuth), and YouTube accepts those
only on the TV client. YouTube is also testing Ask on TV apps, but nothing
public describes the TV form, and no anonymous answer carries it. Building
chips, an answer view and a request path against a shape nobody has seen
would ship a feature that cannot be checked. So nothing was built for the
screens. The service now reports one fact that settles the TV question for a
signed-in account (see [What was added](#what-was-added)).

## How Ask works on the web

Source: the open-source macOS client Kaset
([sozercan/kaset](https://github.com/sozercan/kaset), `docs/api-discovery.md`,
section "YouTube Ask Gemini / YouChat investigation"). Its authors captured and
replayed the flow with a signed-in web session in July and August 2026 and use
it in their app. Ad blockers and patch sets that hide the button name the same
panel (`PAyouchat` in YouTube-Enhancer, Morphe patches, yt-reader-mode).

- **Advertisement:** the signed-in `youtubei/v1/next` answer (client `WEB`)
  holds an engagement panel with `targetId: "PAyouchat"` (also seen:
  `engagement-panel-youchat`, `PAai_companion`). The action bar shows the Ask
  button (`you-chat-entrypoint-button` in the page). The panel carries
  suggestion chips (`chipData.continuation`) and, in its footer,
  `chatInputViewModel.sendUserQueryCommand` for free text. YouTube.js 18.0.0
  added `VideoDescriptionYouchatSectionView`, the Ask section of the
  description panel.
- **Requests:** everything goes to `youtubei/v1/get_panel` with `context`,
  the server-issued `continuation` and
  `formData.inputComposerFormData` = `clientMessageId` (`youchat-<epoch ms>`),
  `playerOffsetMs` (string) and, for free text, `userInputText`. A suggestion
  chip sends its continuation, not its text. `streaming_panel` answered HTTP 400.
- **Answer:** `onResponseReceivedCommand.listMutationCommand`; the text and
  follow-up chips are `youChatItemViewModel` items (`text.content`,
  `chipsData`) under `insertItemSectionContent`. Kaset reads only the text and
  the chips and ignores other result objects (`videoResultsData`, `webData`);
  whether answers carry timestamp links is not documented.
- **Authentication:** cookies of a signed-in web session (SAPISID hash). Kaset's
  first runs with a session YouTube treated as signed out got no panel at all.
  No proof token or BotGuard attestation is mentioned for these calls.
- **Who gets it:** adults (18+) on some videos, in rolling countries and
  languages (first English in the US, Canada, New Zealand and India; then
  more). YouTube decides per account and video.

## What a TV sign-in can do

- YouTube.js states that OAuth sign-in "now only works with the TV Innertube
  client" ([authentication guide](https://ytjs.dev/guide/authentication)).
  ProsperoTube already uses it that way (`LIBRARY_SERVICE.md`, "OAuth
  sessions use the TVHTML5 client"). A TV token cannot open the web flow
  above, and web cookies would mean a web sign-in that this app does not
  offer (passwords and browser cookies stay out of the console by design).
- YouTube began a small test of Ask in its TV apps in February 2026 (adults,
  English, Hindi, Spanish, Portuguese and Korean; not all videos; press
  coverage: [9to5Google](https://9to5google.com/?p=704952),
  [PCWorld](https://www.pcworld.com/article/3067091/google-is-cramming-gemini-ai-chat-into-youtube-on-smart-tvs.html)).
  That is the only route open to our accounts. No client, issue tracker or
  write-up describes its requests. TV watch-next answers do carry engagement
  panels now, so the TV form may well be the same `PAyouchat` panel and
  `get_panel` flow, but that has not been seen.
- Not found in FreeTube, SmartTube, NewPipe, Invidious or yt-dlp (searched
  2026-10-16): none of them implements Ask.

## Requests made for this study

Four anonymous, read-only `youtubei/v1/next` requests from the PC (plus one
session setup), for a public 19-minute video, 2026-10-16, through the pinned
YouTube.js of `companion/innertube`, no account, no cookies:

| Client | Engagement panels | YouChat / Ask markers |
|---|---|---|
| `WEB` 2.20261016.01.00 | comments, ads, search preview, chapters, structured description, transcript | none |
| `MWEB` 2.20260205.04.01 | comments, chapters, description | none |
| `TV` | comments, chapters, description (by identifier) | none |

As expected for a signed-out session, nothing advertised Ask.

## What was added

`video_details` (the details screen's service action) now carries
`"ask_advertised": true` when the watch-next answer it already fetched for the
selected profile advertises Ask: a panel whose target or identifier names
`youchat`, or the description's Ask section
(`companion/innertube/video_description.mjs`, `askAdvertised`). No extra
request is made, nothing from the panel is copied, and no screen shows it.
Tests: `companion/innertube/third_batch.test.mjs` ("video details report Ask
only when the watch page advertises it").

## What would make it possible

1. **Evidence from an eligible account.** With the built-in service on the PC
   (`standalone/build/tube_js serve --state-dir DIR`), sign in through the
   device code (`/v1/auth/start`), then ask
   `POST /v1/library {"action":"video_details","profile":"<profile id>","id":"<video id>"}`
   for videos where the YouTube app on a TV shows Ask (adult account, a
   supported language and country). The same sign-in on the console gives the
   same answer, because the app uses the same TV session.
2. **If `ask_advertised` comes back true**, record that watch-next answer's
   panel (with continuations and tracking values removed) as a fixture and
   build the feature on it: an Ask entry on the video page and in the player
   details only when advertised, YouTube's suggestions as chips, free text
   through the typed-answer entry, the answer as text with timestamps as seek
   links if present, loading, error and unavailable states, and the note that
   answers come from YouTube. The requests would be `get_panel` as above, sent
   by the account session, once per question, without retries.
3. **If it stays absent** while the TV app shows Ask for the same account, the
   TV app uses another form; a capture from a TV app would be needed. A web
   cookie sign-in is the other way in, and it is deliberately not supported.
