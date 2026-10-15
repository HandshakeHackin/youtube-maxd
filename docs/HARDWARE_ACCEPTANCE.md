# Hardware acceptance — in progress

- 2026-10-15 | direct-LAN 4K VP9/Opus endurance with two rewinds, Guest | 964bb5c instrumented, eboot SHA-256 `4eba00b13cacdf11ceb9e4f17889780dfdc99d0fd7bf6032ca4048574ffab927` | PS5 .30 | pass: one native 3840x2160/30 decoder session advanced 5.33→220.76 s in 215 s, rewound -170 s to 51.65 (target 50.76), advanced 55.24→270.70 s in 215 s, rewound -170 s to 101.64 (target 100.70), then advanced 101.64→270.94 s in 170 s. Three distinct nonblank 4K scanouts were captured. The bounded 600-second playback watch found no stall, GPU/AGC fault signature or new error record | private evidence `/home/denis/pt-console/lan4k-964bb5c-30-run2/result.json` | one launch; stages 0–6 passed, player stopped, GPU parked before exact-title close, all ten saved files restored or unchanged, temporary Classic/LAN overrides removed, title idle, services healthy, ShadowMount released and .30 lock released. The earlier attempt stopped at the harness's Classic-only ready check because the saved `afterglow.on` opened Afterglow; its teardown also passed. This fixture does not prove every public CDN/timing condition or identify the historical GPU fault owner.

- 2026-10-15 | public Bloomberg live DVR rewind and UTC restore, Guest | 964bb5c instrumented, `prosperotube-964bb5cc_1015-0048`, eboot SHA-256 `4eba00b13cacdf11ceb9e4f17889780dfdc99d0fd7bf6032ca4048574ffab927` | PS5 .30 | pass: exact public item `QB5BNdBFujE` opened native AVC 1280x720/29.97 at live position 3591.58 s; a -120-second command landed at 3472.61 s and frames advanced to 3473.59 s one second later. The first stop saved UTC 1792025369061198; reopening after an 18.4-second playlist-window advance resumed at 3455.19 s with restored UTC 1792025371663880, 2.60 s after the saved point, and frames advanced to 3456.17 s. First playback published 125 frames with one late drop and one successful seek, none failed; reopened playback published 80 frames with zero late drops and two successful seeks, none failed. No new error record appeared | private evidence `/home/denis/pt-console/public-dvr-bloomberg-964bb5c-30-run/result.json` | one launch; stages 0-6 passed, temporary Guest history removed, original saved state restored or unchanged, title idle, services healthy, ShadowMount released. This bounded 720p30 AVC/AAC run does not prove long or 4K live DVR playback or physically audible audio. The candidate also contains fixes for timezone-offset program-date-time, exact discontinuity tag matching, AAC packets with missing PTS, and HLS wrap reference reset on seek.

- 2026-10-14 | public BBC Earth 4K/25 post-seek repeat, Guest | cd45171b, eboot SHA-256 `9697e4ecb8d60a267848138b34d2f13efd3db818bdefc719195d499662dad01f` | PS5 .30 | pass: native VP9 3840x2160/25 advanced 605.85→755.69 seconds (+149.84) in the first 150-second interval; a -60-second seek landed at 696.50 (target 695.69); the next 150 seconds advanced to 841.35 (+144.85). The player published 7,414 frames with 144 late drops (1.9%); the engine reported four successful seeks and none failed. The BBC log showed 750 video packets / 48 MB read-ahead despite a reported 2 kbit/s rate; the unrelated core item logged 8 MB. Initial and post-seek 3840x2160 scanouts showed distinct footage. No HTTP 403, `DEMUX_OVERSHOOT`, GPU fault or new error record appeared | private evidence `/home/denis/pt-console/read-ahead-cd45171-30-repeat` | one launch; stages 0–6 and Afterglow 32/32 passed; all nine saved-state files restored or unchanged, title idle, services healthy, ShadowMount released, .30 lock released. This repeat and the preceding run do not prove all public CDN/timing conditions or identify the historical GPU fault address owner.

- 2026-10-14 | public BBC Earth 4K/25 read-ahead fix, Guest | cd45171b, eboot SHA-256 `9697e4ecb8d60a267848138b34d2f13efd3db818bdefc719195d499662dad01f` | PS5 .30 | pass: native VP9 3840x2160/25 advanced 607.64→757.61 seconds (+149.97) in the first 150-second interval; a -60-second seek landed at 697.91 (target 697.61); the next 150 seconds advanced to 836.79 (+138.88). The player published 7,431 frames with eight late drops (0.1%); the engine reported three successful seeks and none failed. The BBC log showed 750 video packets / 48 MB read-ahead despite a reported 2 kbit/s rate; the unrelated core item logged 8 MB. Initial and post-seek 3840x2160 scanouts showed distinct footage. No HTTP 403, `DEMUX_OVERSHOOT`, GPU fault or new error record appeared | private evidence `/home/denis/pt-console/read-ahead-cd45171-30-run` | one launch; stages 0–6 and Afterglow 32/32 passed; all nine saved-state files restored or unchanged, title idle, services healthy, ShadowMount released, .30 lock released. This bounded pass does not prove all public CDN/timing conditions or identify the historical GPU fault address owner.

- 2026-10-14 | public BBC Earth 4K/25 post-seek diagnostic, Guest | a070ade2 with temporary token-safe instrumentation, eboot SHA-256 `5cc9969492a29ea30e2e97c154086576c09663dcf7cb8585559b36f918934c6c` | PS5 .30 | bounded pass: native VP9 3840x2160/25 advanced 605.33→755.26 seconds in the first 150-second interval; a -60-second seek landed at 695.99 seconds (target 695.26); the next 60 seconds advanced to 749.37 (+53.38). About 6–7 seconds of seek preroll preceded steady audio/video clocks and near-normal pacing. Audio packet and PCM output resumed, and underruns stayed flat after preroll. The earlier sustained 3–4 frames/s collapse did not reproduce in this one run; neither HTTP 403 nor a GPU fault appeared | private evidence `/home/denis/pt-console/seek-audio-diag-30-run` | one launch; stage 6 parked and closed the title, restored all nine saved-state files exactly, found healthy services and unchanged error records, and released the .30 lock. Diagnostic instrumentation was removed from product source after the run. The later cd45171b read-ahead fix passed two bounded public-network repeats above.

- 2026-10-14 | public BBC Earth 4K/25 endurance and rewind, Guest | a070ade2 candidate, eboot SHA-256 `ede5a6e9faf54c4d0ac4b35174b8eda595c9d0d1aeca99f211431e837a0595a5` | PS5 .30 | fail after seek: native VP9 3840x2160/25 advanced 606.23→756.37 seconds during the first 150-second interval; a -60-second seek landed at 696.84 seconds (target 696.37), but the next 150-second interval advanced only to 720.91 seconds (+24.07). Presentation slowed from about 25 to 3.5 frames/s. The engine published 4,429 frames with 137 late drops and two successful seeks. Video packet queue remained near 120 while the audio packet queue was empty; repeated FFmpeg premature-file-end notices occurred both before and after the seek. No HTTP 403 or GPU fault appeared. Audio starvation is a plausible cause of the slowdown, not yet proven | private evidence `/home/denis/pt-console/public-bbc-a070ade-30-run` | one launch; stage 6 parked and closed the title, restored all nine saved-state files exactly, found healthy services and unchanged error records, and released the .30 lock. The diagnostic retest above did not reproduce this collapse.

- 2026-10-14 | public BBC Earth 4K/25 retry, Guest | c9402ee instrumented, eboot SHA-256 `34e57d1a7f9f60a5fa4319c9311a21388c9812f8be2e65ae644a5e26f8c9824b` | PS5 .30 | blocked before any frame: exact public item `5PQYM_KSXag` exposed native VP9 3840x2160/25, but FFmpeg received HTTP 403 on DASH video segment 85 (bytes 1137180386-1150531373); fallback 1440p/1080p/720p also hit 403 at segment 85. All attempts had frames_in=0, published=0 and zero seeks; no five-minute or public seek evidence | private evidence `/home/denis/pt-console/public-bbc-c9402ee-30-run` | host preflight got 206 for a fresh link, including the same deep range, but this does not test the console's exact signed URL. One launch, exact nine-file restore, idle/healthy services, unchanged errors and lock release passed; full app/kernel logs had no GPU fault. The upstream refusal remains unattributed.

- 2026-10-14 | 4K VP9/Opus endurance, seeks and HUI texture churn, Guest | c9402ee instrumented, eboot SHA-256 `34e57d1a7f9f60a5fa4319c9311a21388c9812f8be2e65ae644a5e26f8c9824b` | PS5 .30 | pass: direct-LAN native 3840x2160/30 VP9 ran the first 180 seconds, rewound 60 seconds and replayed, sought forward 50 seconds, then drained at natural EOF 304 seconds; 9,421 published frames, one late drop, three successful seeks. A 4K scanout was captured near start and end. Afterglow then passed 26/26 navigation, artwork and playback checks with 4K scanout and 12 seconds of idle after texture churn. Full console and klog scan found no GPU page fault, DCB-full, ring-allocation, fence-timeout or AGC error signature | private evidence `/home/denis/pt-console/hui-fence-c9402ee-30-run` | one launch, all stages including exact nine-file restore passed; title idle, services healthy, errors unchanged, .30 lock released and fixture stopped. The 12-second idle check does not expose a GPU no-swap counter; earlier fault address ownership and public-network timing remain unresolved.

- 2026-10-14 | active 30-to-25 fps VideoOut switch, Guest | c9402ee instrumented, eboot SHA-256 `34e57d1a7f9f60a5fa4319c9311a21388c9812f8be2e65ae644a5e26f8c9824b` | PS5 .40 | pass: 30 fps PQ playback selected mode 0xf and actual refresh 13 (120 Hz), then direct 25 fps VP9 playback while the player remained active selected mode 0x1 and actual refresh 3 (60 Hz); 452 and 341 frames published, respectively, with nonflat captures and post-flip readback | private evidence `/home/denis/pt-console/cadence-20261014-c9402ee-40-run` | all stages and exact state restoration passed, no new errors appeared, title idle, lock released and fixture server stopped. This does not independently verify TV-reported HDMI HDR metadata.

- 2026-10-14 | HDR selection and output transitions, Guest | b95cd225 readback candidate, eboot SHA-256 `e747524310df003775baed74faa98ab8ab8cec2da638f98a1aefc54938fdb47c` | PS5 .40 | pass for synthetic VP9 clips: PQ/Profile 2/transfer 16 published 524 frames, HLG/Profile 2/transfer 18 published 546, then SDR/Profile 0 published 621, all with zero late drops; each had a nonflat 24.9 MB scanout capture, and PQ-to-SDR changed the VideoOut buffer format | private evidence `/home/denis/pt-console/hdr-playback-20261014-readback-complete-run` | all stages and state restoration passed. Its first PQ selection exposed a boot refresh-state mismatch, fixed and retested in the first-playback entry below. TV HDR indication, HDMI metadata and physical sound were not observed.

- 2026-10-14 | stalled artwork versus playback control, Guest | 33d91f2 instrumented, eboot SHA-256 `df257c1ad61ebd13f9e365704982b78ce152d8e7564ec13605f731208ad05363` | PS5 .30 | pass: while a unique artwork request was deliberately held, POST `/v1/jobs` returned HTTP 200 in 51 ms with the image still pending, and playback became active 2.1 s after Cross before image release | private evidence `/home/denis/pt-console/net-lane-33d91f2-30-rerun` | stage 0-6, saved-state hashes, services, errors, idle and lock release passed. A newly generated 10-byte folder breadcrumb absent at baseline was separately removed under lock and rechecked; the fixture server was stopped. This verifies control-lane independence for a held image, not recovery from every public DNS or CDN failure.

- 2026-10-14 | first-playback refresh and HDR10, Guest | 33d91f2 instrumented, eboot SHA-256 `df257c1ad61ebd13f9e365704982b78ce152d8e7564ec13605f731208ad05363` | PS5 .40 | pass: actual VideoOut refresh was 3 at boot, the first 30 fps PQ playback configured mode 0xf and read back refresh 13 after flip, and Stop configured mode 0x1 and read back 3; 538 frames were published over 19.1 s with zero late drops and a nonflat capture | private evidence `/home/denis/pt-console/first-refresh-20261014-combined-40-run` | all 10 saved files were restored, no new errors appeared, services were healthy, title idle, lock released. A controlled boot-120-to-first-25/50 case has host coverage but no hardware run; the console's baseline dynamic-range report does not independently prove HDMI HDR signalling.

- 2026-10-14 | live DVR seek and UTC restore, Guest | 58566a1 instrumented, eboot SHA-256 `41676c3245431621f7fb2f403c5e5424f4816dab7208eb787d89342b9e633cdb` | PS5 .30 | pass on synthetic split HLS AVC/AAC: rewind 5.50 to 3.31 s, reopen after advancing window within 0.98 s of expected UTC position, and reject an evicted position in favor of the current window; seek_req=1, ok=1, fail=0 | private evidence `/tmp/pt-dvr-console-prep/hardware-interrupt-58566a1-20261014-d/result.json` | saved state restored, no new errors, idle and lock released. A public live feed and high-resolution live DVR remain untested.

- 2026-10-14 | pending-start 403 close race, Guest | e8465b6a instrumented, eboot SHA-256 `21c2cc598f1c7dd0bf18d900b3b74dbaa6d561838174b12f95f8bf1bd03bb3d7` | PS5 .30 | pass on a deliberately held first 4K media range returning HTTP 403: soft close cancelled and joined the provider's pending FFmpeg open before releasing decoder/media state; no CPU SIGSEGV or GPU fault | private evidence `/home/denis/pt-console/forbidden-403-hold-30-rerun` | exact saved-state restoration and healthy idle teardown passed. The upstream BBC first-fragment 403 itself remains unexplained.

- 2026-10-14 | sustained direct-LAN 4K VP9/Opus, Guest | e8465b6a instrumented, eboot SHA-256 `21c2cc598f1c7dd0bf18d900b3b74dbaa6d561838174b12f95f8bf1bd03bb3d7` | PS5 .40 | app-level pass: native 3840x2160/30 played a 304-second MPD to natural EOF with 9,416 published frames, five late drops, zero decode fatal/GPU faults, two successful 4K seeks (-60 and +50 s), and distinct nonflat 4K scanout captures near start and end | private evidence `/home/denis/pt-console/direct-4k-40-run` | ten saved files restored, errors unchanged, console idle and lock released. The private driver incorrectly demanded active progression after natural EOF and marked its final stage failed; its gate was corrected offline without rerunning. This bounded stable-LAN run does not attribute or eliminate historical unmapped GPU reads under other timing/network conditions.

- 2026-10-14 | public 4K VP9/23.976 endurance sample, Guest | 57efd5b2 instrumented baseline | PS5 .30 | pass for one bounded 132-second session: native 3840x2160 VP9 reached natural EOF, published 3,160 frames, and reported zero late/seek drops, decode fatal, or GPU faults | private .30 evidence | exact teardown, saved-state restoration, healthy services, and lock release passed. This does not prove repeated 4K transitions, seeking, HDR, physical sound, or longer endurance.

- 2026-10-14 | BBC Earth 4K/25 retry, Guest | 57efd5b2 instrumented baseline | PS5 .30 | failed before the first frame: native VP9 opened 3840x2160, then the first DASH fragment repeatedly returned HTTP 403; after about 103 seconds and zero published frames, closing during pending startup produced a CPU SIGSEGV after media release, with no new GPU error record | private .30 evidence | the title exited, saved state was restored, services were healthy, and the lock was released. Source review identified a provider start worker still in FFmpeg seek while soft-close freed its format state. The cancellation/join fix passed the later held-403 fixture above; neither the transport 403 cause nor the earlier GPU fault address owner is established by this run.

- 2026-10-14 | bounded live YouTube 4K VP9, Guest | 57efd5b2 (instrumented, `prosperotube-57efd5b2_1014-1838`, eboot SHA-256 `2ba4966b489943bca2507106d17936d462cde2764f75580f82e5c5d161bee8e8`) | PS5 .30 | pass for this bounded live 4K/25 path: Classic native VP9 1920x1080 and a +10 s seek passed first; Afterglow opened the public BBC Earth item `5PQYM_KSXag` through the built-in service and native `sceVideodec2` at 3840x2160, advanced position by 11 s, and published 315 frames with four late drops (1.3%), zero decode fatal, software mappings or GPU faults | private evidence `live4k-57efd5b-30-run`, 19:47Z | the 24.3 s playback session started at a saved 600 s position; the 4K file discarded 253 frames as seek preroll while settling there, separate from late presentation drops. Stop confirmation returned to Afterglow Home. Stage 6 parked and closed the title, restored saved state byte for byte, found services healthy and error records unchanged, left the console idle, and released the .30 lock. A 4K Home image was captured, but no playback scanout image was taken; presenter counters support frame display without proving the picture by eye. FFmpeg logged premature-end and earliest-resync warnings in the WebM media stream; playback recovered, but transport and demux robustness remain to validate. This one bounded live result does not establish long live 4K playback, 4K seeking, HDR or audible output, and does not erase the earlier live GPU faults below.

- 2026-10-14 | offline 64-second 4K VP9 endurance fixture, Guest | 57efd5b2 (instrumented, `prosperotube-57efd5b2_1014-1838`, eboot SHA-256 `2ba4966b489943bca2507106d17936d462cde2764f75580f82e5c5d161bee8e8`) | PS5 .40 | pass for bounded offline 4K/30: a generated 3840x2160 VP9/Opus item opened through native `sceVideodec2`, advanced past 15/30/45/55 s and reached 63.99/64 s; 1,920 frames entered and were published with zero late/seek drops, decode fatal, software mapping or GPU fault | private evidence `offline4k-endurance-57efd5b-run`, 18:58Z | final gates showed natural EOF, audio drain, video decode and display retirement true, with no input/video failure; `complete=0` was sampled during the terminal pause that holds the final frame. Stage 6 parked and closed the title, restored saved state, left services healthy and error records unchanged, and released the .40 lock. This fixture by itself does not establish live YouTube 4K, HDR, audible output, 4K seeking/repeat, very long endurance or a directly sampled completion transition. Earlier network 4K faults remain recorded below.

- 2026-10-14 | offline full-width VP9 fixture and completion-gate diagnostic, Guest | 57efd5b2 (instrumented, `prosperotube-57efd5b2_1014-1838`, eboot SHA-256 `2ba4966b489943bca2507106d17936d462cde2764f75580f82e5c5d161bee8e8`) | PS5 .40 | partial: a generated 8-second 3840x2160 VP9/Opus fixture opened through native `sceVideodec2`, presented full-width frames, and reached 7.99/8.0 s; 240 frames entered and were published, with zero late/seek drops, no decode fatal or GPU fault | private evidence `offline4k-57efd5b-run`, 18:45Z | at 7.77 s the natural-EOF gate was true while audio, video-decoded and display-retired gates were false; by the next one-second sample all four completion gates were true and playback was paused. The player code pauses after observing completion when no next item exists, which strongly explains this terminal state, but `complete=1` was never directly sampled. The driver's strict completion observation timed out; this is not evidence of stalled decoding. Its 4.6 decode-fps sweep averages the 51.9-second session including the held end frame, not active playback throughput. Stage 6 parked and closed the app, restored saved state, left services healthy and error records unchanged, and released the .40 lock. This short offline case does not accept sustained or network 4K, HDR, physical audio, seeking, or direct completion-transition proof.

- 2026-10-14 | first offline full-width VP9 fixture, Guest | d1861464 (instrumented, `prosperotube-d1861464_1014-1818`, eboot SHA-256 `2e4c8e4e4d256b9d63719c080e2f9b4cfcab6f982371257866c298e62fee8478`) | PS5 .40 | partial: the same generated 8-second 3840x2160 VP9/Opus fixture opened natively, presented full-width frames and reached 7.99 s; 240 frames entered, converted and published with zero late drops and no GPU fault | private evidence `offline4k-d186146-run`, 18:25Z | its status lacked individual completion gates, so a 45-second `complete=1` timeout could not distinguish a stuck drain from the expected completion pause. The later diagnostic above favors the pause explanation but does not directly prove the transition in this run. Stage 6 restored state, left services healthy and error records unchanged, and released the .40 lock.

- 2026-10-14 | strict GPU pool plus AGC UI frame-boundary retest, Guest | e433df7b (instrumented, `prosperotube-e433df7b_1014-1802`, eboot SHA-256 `621fdfe22cb4caae8894aecf5ed06d8d08030ebf2e8016f8ea476d1a42045162`) | PS5 .40 | unjudged for video: exact install, Lapy storage, built-in service, browser and Discover passed; Science catalog request timed out before video selection, so no 4K decoder opened | private evidence `boundary-pool-e433df7b-40-run`, 18:07Z | the AGC frame-boundary fix held: 3,600 UI frames with zero DCB-full and ring failures. Saved log shows DNS failure for both YouTube and homebrew.page after 60 seconds, explaining the catalog timeout; no GPU fault occurred. Stage 6 restored state, closed exactly once, left services healthy and console idle, and released the .40 lock. Hardware behavior of strict GPU-pool guard remains unjudged.

- 2026-10-14 | VP9 no-Reset stream-boundary experiment, Guest | 4a5e5e79 (instrumented, `prosperotube-4a5e5e79_1014-1747`, eboot SHA-256 `13e000518ac3b0e91a618b44ea9a87a233d1c4871129b3ebdf069b82c33550bf`) | PS5 .30 | fail: Classic native VP9 1080p and seek passed; Afterglow Home drew at 3840x2160; native VP9 2880x2160 opened after the first stream without a VP9 close/open Reset, but the GPU faulted on an unmapped read at `0x2a5256000` before a valid output frame | private evidence `no-reset-4a5e5e79-run`, 17:54Z | no-Reset did not resolve the fault. Saved files were restored, no title remained, and services answered, but a new console error record prevented clean-teardown proof. The driver kept the .30 lock until the owner authorized clearing it for the later bounded run above.

- 2026-10-14 | integrated strict GPU-pool guard, Guest | a898a1e (instrumented, frozen eboot SHA-256 prefix `84a20095cb8f3166`) | PS5 .40 | unjudged for video: exact install, Lapy storage and browser startup passed; Science catalog request timed out before video selection, so no 4K decoder opened | private evidence `integrated-a898a1e-40-run`, 17:50Z | AGC held one UI frame about 66 seconds, reached a 524287/524288-word command buffer with over one million allocation failures, then dropped the unsubmitted frame. No GPU fault was recorded. Stage 6 passed: exact close, saved state restored, no new error record, services healthy, idle, lock released. Repeating this build without a source fix would not test 4K.

- 2026-10-14 | AGC fence-retirement and resident VP9 retest, Guest | 6c2680f0 (instrumented, `prosperotube-6c2680f0_1014-1729`, eboot SHA-256 `b4374b4ff584c21348fa8457d23e27de84797e43a55376c86b0444afdfc1c984`) | PS5 | fail: 185 files verified after settling; Classic native VP9 1080p played and sought; Afterglow Home drew at 3840x2160; a 3840x2160 24 fps VP9 stream produced its first valid frame, then the GPU faulted on an unmapped read at `0x2a50fb000` before playback became active | private evidence `fence-6c2680f0-run`, 17:34Z | both streams used the resident VP9 pool `0x25f2f4000`; fence-retirement checks did not resolve the first-frame fault. Earlier in the same run, an AGC frame could not emit its fence after an interface transition, followed by DCB/ring allocation failures. The relation to the later fault is not yet proven. Sound was unjudged. Saved state, final idle, service health, unchanged error records and lock release passed. No relaunch followed.

- 2026-10-14 | first short full-width VP9 playback with GPU address tracing, Guest | 2ec83f99 (instrumented, `prosperotube-2ec83f99_1014-1713`, eboot SHA-256 `0261c9b222c18a2688fd881308b19a27e737f45f15892bb024228ad51abfbd4d`) | PS5 | partial: 185 files verified after settling; Classic native VP9 1080p and +10 s seek passed; Afterglow Home drew at 3840x2160; native VP9 3840x2160 24 fps became active, advanced from 0.68 to 1.71 s in one second, and published 52 frames with zero late drops and no GPU fault in this short case | private evidence `gpuaddr-2ec83f99-run`, 17:18Z | diagnostic address logs were the only behavioral timing change from ca173808; the earlier first-frame fault is not resolved. Direct-pool fallback was not observed. Afterglow stop-confirmation and return-screen checks failed; a direct stop command retired the player. Sound was unjudged and this case does not establish sustained 4K playback. Saved state, idle, service health, unchanged error records and lock release passed.

- 2026-10-14 | resident 4K-capable VP9 decoder retest, Guest VP9 | ca173808 (instrumented, `prosperotube-ca173808_1014-1657`, eboot SHA-256 `2196182b4aaa1a62dda68ed2418fa19f087edaa3c2b77fd23b3c64c2ca1f4dff`) | PS5 | fail: 185 files verified after settling; Classic native VP9 1920x1080 playback and +10 s seek passed; Afterglow Home drew at 3840x2160; a native VP9 3840x2160 24 fps stream produced a valid frame and reached the first decoded-frame presenter checkpoint, then the GPU faulted on an unmapped read at `0x2a5248000` | private evidence `resident-ca173808-run`, 17:03Z | both 1080p and 2160p used the same resident VP9 frame pool (`0x25f2f4000`) with no resize or retired VP9 maps, so the earlier teardown fault was avoided. The new address lies outside the logged native decoder pools and points to a separate display-path problem. Playback progress and sound could not be assessed. Saved state was restored; final idle, service health, unchanged error records and lock release passed. No relaunch followed.

- 2026-10-14 | bounded decoder-close drain retest, Guest VP9 up to 2160p | 88c154a6 (instrumented, `prosperotube-88c154a6_1014-1650`, eboot SHA-256 `a01fbf471bf040132a3df410526ae4a65e6f26ff7d0982cae22294cb996ddd28`) | PS5 | fail: 185 files verified after settling; Classic native VP9 1920x1080 and +10 s seek passed; close drained four buffered outputs before reset; Afterglow Home drew at 3840x2160; native VP9 2880x2160 30 fps produced one valid surface, then the GPU faulted on an unmapped read at `0x248002000` before the first-frame presenter checkpoint | private evidence `drain-88c154a6-run`, 16:55Z | the new fault falls inside the retired 1080p decoder frame pool (`0x247fa0000`–`0x249940000`). Buffered-output drain alone did not make that allocation safe to release. Playback progress and sound could not be assessed. Saved state was restored; final idle, service health, unchanged error records and lock release passed. No relaunch followed.

- 2026-10-14 | decoder teardown-order retest, Guest VP9 up to 2160p | 0184b32d (instrumented, `prosperotube-0184b32d_1014-1635`, eboot SHA-256 `1faba90886bf230ed2b7cbc57b2876352399229ac82d297f4e0a9de85432aead`) | PS5 | fail: 185 files verified after settling; Classic native VP9 1920x1080 playback and +10 s seek passed; Afterglow Home drew at 3840x2160; a native VP9 2880x2160 30 fps stream produced one valid surface, then the GPU faulted on an unmapped read at `0x247cf4000` before the first-frame presenter checkpoint | private evidence `teardown-0184b32d-run`, 16:42Z | diagnostic map logs place the fault inside the retired 1080p decoder input allocation (`0x245fa0000`–`0x247fa0000`), not the new 2160p frame pool. Releasing the compute queue before the decoder mappings did not resolve the fault. Playback progress and sound could not be assessed. Saved state was restored; final idle, service health, unchanged error records and lock release passed. No relaunch followed.

- 2026-10-14 | native frame-lease retest, Guest VP9 up to 2160p | 8c8d7b15 (instrumented, `prosperotube-8c8d7b15_1014-1621`, eboot SHA-256 `53a8637d44ff4f04179476ba16ce229ef501068c3018477bebe23c11b5c271b2`) | PS5 | fail: 185 files verified after settling; Classic native VP9 1920x1080 advanced and its +10 s seek landed; Afterglow Home and 3840x2160 scanout passed, then native VP9 opened a 2880x2160 30 fps stream and produced one valid surface before an asynchronous GPU unmapped-read page fault at `0x247d58000` | private evidence `lease-8c8d7b15-run`, 16:27Z | the fault address is outside the new decoder frame pool (`0x299928000`); the surface passed pointer, geometry, pitch and pool checks. The frame counter stopped before playback progress or sound activation. The source lease fix passed offline checks, but this case does not establish 2160p playback. Exact-title close, saved-state restoration, idle and service checks, unchanged error records and lock release passed. No relaunch followed.

- 2026-10-14 | combined player/theatre candidate, Guest VP9 up to 2160p | 93b2e0c7 (instrumented, `prosperotube-93b2e0c7_1014-1516`, eboot SHA-256 `565094b29cdc890f286b8888e6adbee31090c5da6cd8fb142bc845242c15b03a`) | PS5 | fail: Classic native VP9 1080p passed seeking, pause/resume, pickers, stop and history (141 published frames, zero late drops); Afterglow Home/navigation passed, but a second video opened native VP9 3840x2160 at 24 fps then a GPU unmapped-read page fault followed its first valid frame and status stopped advancing | private evidence `parallel-93b2e0c-run`, 15:16Z | theatre alignment and mounted pickers were not reached; no 4K performance or sound result. The frame had already stalled before sound was enabled. Exact-title close, saved state/overrides, idle check, unchanged error records and lock release all passed. No relaunch followed.

- 2026-10-14 | ranged WebM, Guest VP9 preference at 1080p, plus Afterglow | ec4660ac (instrumented, `prosperotube-ec4660ac_1014-1320`, eboot SHA-256 `110746c8d0bcbb3e64b4d8e48077ded5364d0aafdaacacf7d47c14a8278d3a75`) | PS5 | pass: native VP9 1920x1080 with native AAC stereo, +10 s seek, pause/resume, history, 138 frames published with 0 late drops; Afterglow 23 checks passed (Home, thumbnails, text, search and playback); clean park/close, state restored, error records unchanged, lock released | evidence kept outside the repository (`webm-ec4660a-run2`, 13:30Z) | generated Opus fixtures passed on the host only; 4K, HDR, long sessions, human sound/motion assessment and firmware identity were not checked. An earlier run of the same candidate reached VP9 playback and seeking but stopped on a local harness method-signature error; its teardown completed cleanly before the corrected rerun.

- 2026-10-13 | self-sufficient core path, Guest, with the Theme setting and the new mark | a86f8664 (instrumented, `prosperotube-a86f8664_1013-1809`) | PS5 | pass: same core path, 103 frames published with 0 late drops, +10 s seek landed at 11.88 s (target 11.32), no texture failure reported, clean park and close | evidence kept outside the repository (`tools/console/session.py` run of 18:10Z) | the browser itself was not captured as a picture, so the Afterglow palette and the mark are unseen on the console; themes were not switched

- 2026-10-13 | self-sufficient core path, Guest, after the update, language, interface and casting merges | 7ec0ac3e (instrumented, `prosperotube-7ec0ac3e_1013-1611`) | PS5 | pass: same core path as the morning session, 98 frames published with 0 late drops, +10 s seek landed at 12.13 s (target 11.32), clean park and close | evidence kept outside the repository (`tools/console/session.py` run of 17:03Z) | the features those merges added were not exercised: update check, interface languages, themes and scale, seek preview, casting, accounts

The session repeated the morning's route (Discover, Science, first video, H.264
1080p on the hardware decoder) on the build that added the update check, the 21
interface languages, the interface preferences and casting. It shows those
additions did not disturb start-up, the built-in service, browsing or playback.
It did not open any of them.

- 2026-10-13 | self-sufficient core path, Guest | 6b3b1c41 (instrumented, `prosperotube-6b3b1c41_1013-1226`) | PS5 | pass: built-in service, Discover, direct playback, seek, pause, pickers, stop, history, clean park and close | evidence kept outside the repository (`tools/console/session.py` run of 13:18Z) | sound and motion by a person, VP9/AV1/4K, long sessions, sign-in

The first session of the build that needs no PC. No companion was running. The
built-in service listened on 127.0.0.1:37841; Discover listed 15 rows and the
Science topic 15 videos through it. The first video (a 1 h 01 min upload) was
resolved on the console and played from YouTube directly: DASH manifest from the
loopback service, media from `googlevideo.com`, H.264 1920x1080 at 29.97 fps on
the hardware decoder with AAC stereo. Position advanced 1.07 to 2.09 s in one
second; a +10 s seek landed at 12.39 s (target 12.09) with the first frame after
2.2 s; pause held; the caption picker and playback options opened; Stop through
the confirmation returned to the browser. The engine published 125 frames with
7 late drops (5.6%) over about 22 s that included the seek and two pickers. The
history record was saved under `/data/prosperotube` and the saved state holds no
service address. Memory while playing: 696 MB live, flexible pool 770 MB free.
The app parked in 6 s, the exact-title close left no sandbox, the previous state
was restored and the lock released. One capture of the paused player was read
back. Not assessed: sound, motion smoothness, captions on screen (the video had
none selected), other codecs and resolutions, expiry recovery, accounts.

- 2026-10-13 | same build, first attempt | 6b3b1c41 | PS5 | no functional result: the app started, drew the main menu, started the service and closed cleanly, but the driver's readiness check expected another interface layer and ended the session after 90 s | same evidence root, run of 12:46Z | driver corrected from the captured snapshot

Console scheduling: the test console is shared with another agent. Before every
console test session, check the shared local lock and the console's running
applications through the established idle-check protocol. Do not interrupt a
busy console or assume an earlier idle check reserves it. Complete remaining
offline work and prepare the final candidate before starting console testing.

- 2026-10-11 | local queue | 73c7b869 | PS5 | pass: unavailable delay/cancel, next playback, retained failed record, clean release/restore | .deps/console/local-queue-73c7b869-hardware-observer2/ | phone failure advance

- 2026-10-11 | local queue | 73c7b869 | PS5 | inconclusive: countdown observer missed notice; automatic next/start and clean release/restore established | .deps/console/local-queue-73c7b869-hardware/ | corrected observer

- 2026-10-10 | secondary captions | 879b1281 | PS5 | pass for paused caption selection/render/clear and speed-reopen retention; clean release/restore | .deps/console/secondary-caption-879b1281-hardware/ | moving playback and real-language timing

The immutable package passed full readback, 130-second settling and readback
again. French became SECONDARY in the actual picker. Scanout after the 1.25x
reopen shows French and English together; clearing French leaves English only.
Paused media time remained stable. Captures include the loading overlay, so this
case establishes caption rendering/retention, not resumed motion or audio at the
new speed. Exact owned GPU/process/video/mount release, healthy services and
original provider and all sidecar bytes were verified; the fixture stopped.
- 2026-10-10 | secondary captions | 63d45c23 | PS5 | failed: SECONDARY tag absent; clean release/restore | .deps/console/secondary-caption-63d45c23-hardware/ | fix provider-path ownership

The corrected observer still did not see secondary application after French
selection. Offline tracing found that provider playback intentionally empties
the legacy filesystem path required by the new loader. The fix uses validated
playback-selection ownership and now has empty-path host coverage. The console
case closed and restored the original data cleanly; no relaunch followed. Dual
rendering and the speed-reopen scenario remain unaccepted.

- 2026-10-10 | secondary captions | 225b5858 | PS5 | failed observation: expected toast absent; primary visible, clean release/restore | .deps/console/secondary-caption-225b5858-hardware/ | native tag gate

The single launch established paused native playback, the English primary
caption and French picker focus. It stopped after Square because the fixture
waited for a toast missing from the UI snapshot. Secondary rendering remains
unproven. Exact owned teardown, healthy services and original provider/sidecar
restoration passed; the fixture stopped and lock released. The observer was
corrected offline to read the widget's existing PRIMARY/SECONDARY tags. No
relaunch followed this anomaly.

- 2026-10-10 | disconnect close | 9340ab5a | PS5 | pass: fixture playback/pause, native self-exit, exact release/restore | .deps/console/phone-exit-9340ab5a-hardware/ | real phone/browsing

The frozen 126-file package passed full install readback, 130-second settling and
verification again. UI pairing and the persisted close preference worked. Native
AVC/AAC playback and pause feedback preceded receiver disconnect; checked GPU
retirement reached the PS5 self-exit API without a host close payload. Exact
process/video-owner/mount release, healthy declared services and original
provider/sidecar restoration passed. The owned fixture stopped and the shared
lock was released. Real-phone disconnect, browsing-only close and physical
motion/sound were not assessed by this case.

- 2026-10-10 | voice availability | 0b06bc66 | PS5 .40 | failed fixture: playlist packet omitted required ctt; startup/UI/mic capability established, capture not attempted; clean exact release and original state/sidecars restored | .deps/console/voice-available-hardware/ | corrected fixture and physical microphone acceptance

The frozen availability-gated voice package was installed with full readback,
130-second settling and full verification again. Native startup, first frame,
UI pairing and the ASR-enabled receiver capability were established. The test
fixture then sent a malformed playlist command without `ctt`; the native driver
rejected it and the case stopped at acknowledgment. No playback or microphone
acceptance is claimed. GPU park, exact owned PID/app/title/mount release, healthy
services and byte-for-byte provider/sidecar restoration passed. Both temporary
fixture services stopped and the lock was released. The wrapper incorrectly
returned exit 0 after recording the functional failure; the recorded milestone
classifies this case as failed. The fixture and failure exit handling were fixed
offline; no relaunch followed.

- 2026-10-10 | phone/captions | b1adb57c | PS5 | pass: paused language/controls/queue/EOF, clean release and state restore | .deps/caption-phone-available-hardware.* | real phone/physical checks
- 2026-10-10 | captions and phone controls | b1adb57c | PS5 .40 and .30 | no-run: each initial check found a running title; no upload, binding or launch | .deps/caption-phone{,-fallback}-hardware.* | test frozen candidate when idle

- 2026-10-10 | casting | 0b02a137 | PS5 | partial-pass: UUID/screen persisted, fixture receiver registered, remote start at 13.25s; clean release/restore | .deps/phone-player-rng-* | remaining controls, real phone

The native pairing RNG fix passed the actual UI path. The deterministic receiver
accepted a version-4 UUID, and the provider saved its identity/screen under Lapy.
Playlist command startup acknowledged only after native AVC/AAC playback began;
feedback retained the fractional offset, playlist index and next-item state.
Captured rendering/decoder telemetry establishes continued frame presentation.
The case then failed because the screenshot adapter used unsupported FTP `NLST`.
No launch retry followed. Separate cleanup verified the same PID/app ID, parked
the GPU, closed only that instance, proved video/mount release and restored the
exact original data. Services remained healthy. Remaining command scenarios,
physical sound and real phone pairing were not verified.

- 2026-10-10 | SponsorBlock review | e8486d38 | PS5 | pass: generated AVC/AAC persistent review, keep/skip/expiry, Stop and clean release | .deps/console/sponsor-review-hardware/verified-result.json | real service and physical appearance

Candidate `17455475` (`prosperotube-17455475_1010-2002`, 126 files, eboot prefix
`c89ff35e59e6ac5f`) passed public 1.25× playback, paused normal-speed selection,
last-speed restoration, pause/resume, forward/backward seek and saved speed on
the fallback console. Paused position remained 43.04 s through the reopen.
GPU park and exact process/title/mount release passed; temporary configuration
was removed and the console was left idle. The case published 5,035 frames with
108 late drops; physical pitch, smoothness and sync remain unassessed.
The earlier pause-check adapter failure was resolved by aligning telemetry
timestamps, using identical frozen bytes. See [validation](VALIDATION.md).

- 2026-10-10 | speed controls | 17455475 | fallback console | pass: rate progression, paused toggle, last-speed restore, seek, saved speed and clean teardown | .deps/speed-controls-fresh-aligned.log | physical pitch/sync, other rates and scoped-memory hardware coverage

Previous `639b015a` (`prosperotube-639b015a_1010-1905`, 126 files,
eboot prefix `64f9d8b3b6bffab4`) passed a bounded retained-chat fixture case:
newest-message opening, older-page focus across growth to 31 messages, top/live
switching and context retirement. Original pairing/history were restored exactly
after clean GPU/process/title/mount release. Services remained healthy and the
isolated fixture companion was stopped. Physical readability, actual live chat,
posting and replay were not assessed.

Previous candidate `327d8606` (`prosperotube-327d8606_1010-1851`, 126 files,
eboot prefix `69879753599e55bd`) passed native public WebP artwork decode/cache,
saved Guest state, GPU park, exact title/mount release and service health.
The matching 135,454-byte image decoded to 480×270 and was read back from the
app's data directory. This case did not play video or assess physical appearance.

Previous candidate `1e523316` (`prosperotube-1e523316_1010-1834`, 126 files,
eboot prefix `f2561db427571559`) passed verified installation, initial resume,
public playback and forward/backward seeking on the reserved console. The first
frame at the saved 161-second position arrived 5.367 s after selection; the native
seek interval was 0.338 s. Three seeks succeeded including initial resume; none
failed. GPU park, exact title/mount release and service health passed.
The raw adapter expected two seeks; the corrected offline verifier validates the
same captured run, preserving the original exit 1. No relaunch was needed.
Physical sound/motion for this build remain unassessed. The earlier `5cc941f0`
control retains user-confirmed smooth 1080p60 sound/video. DeArrow/SponsorBlock,
broader codecs and full parity remain open. See [validation](VALIDATION.md).

- 2026-10-10 | retained chat | 639b015a | reserved console | pass: newest/older paging, stable refresh, top/live switch, exact state restoration and clean teardown | .deps/chat-history-hardware.log | real chat, replay and physical readability

- 2026-10-10 | artwork | 327d8606 | reserved console | pass: verified install, public WebP decode/cache, preserved Guest state, clean teardown | .deps/console/webp-artwork/artwork-result.json | physical appearance, DeArrow and broader playback

- 2026-10-10 | G1 | 1e523316 | reserved console | pass: 5.367s resume, bidirectional seek, clean teardown; corrected counter assertion | .deps/console/dash-index-resume/verified-result.json | WebP artwork and broader playback

- 2026-10-10 | G1 | 4a840c58 | reserved console | failed: initial-resume deadline; later playback observed; clean owned teardown | .deps/parity-batch-console.log | diagnose slow resume

- 2026-10-10 | G1 | 5cc941f0 | reserved console | pass: public 1080p60 AVC/AAC, user-confirmed motion/sound/sync, clean teardown | .deps/public-bunny-watch-console.log | broader acceptance

- 2026-10-10 | verified installation of known-end skip candidate | c758351c | primary console, firmware unreported | no-run: idle initially; all 126 bytes verified before/after 130-second settle and registration confirmed; another title running before launch; no launch/input/close; owned lock released | .deps/sponsor-end-storage-primary-console.log | Lapy startup/storage, then public YouTube acceptance

- 2026-10-10 | SponsorBlock known-end skip | c758351c | reserved console, firmware unreported | no-run: busy at first FTP check; no other probes/upload/launch/input/close; owned lock released | .deps/sponsor-end-storage-reserved-console.log | Lapy startup/storage, public YouTube, then end-skip/confirmation/queue behavior

Prior offline candidate `c758351c` passed the PS5 package and immutable freeze:
126 files, eboot prefix `fbaa644e95bda5b7`, build
`prosperotube-c758351c_1010-0527`. SponsorBlock attribution is bundled and matches
the source file. Provider and extracted player paths passed host ASan/UBSan;
physical controls, stop/teardown and end-skip queue behavior remain unverified.
The reserved case below did not deploy it; the later primary-console installation
above passed stored-byte verification but did not launch. See [validation](VALIDATION.md).

- 2026-10-10 | SponsorBlock policy | 1b0a0dca | reserved console, firmware unreported | no-run: busy at first FTP check; no upload/launch | .deps/sponsor-policy-storage-reserved-console.log | Lapy startup/storage, then public YouTube and confirmation controls

Prior offline candidate `c2779195` fixes audio-only seek holds and audio EOF
handling, including known audio tracks shorter than video. Its complete PS5
package passed and froze with 126 files, eboot prefix `c09f0aff45b24d48`.
It has not been deployed or launched; this milestone made no console connection.
Sony Audiodec timing and audible AudioOut acceptance remain pending. See
[validation](VALIDATION.md).

- 2026-10-10 | accurate resume | c6ec8804 | reserved console, firmware unreported | no-run: busy at first FTP check; no upload or launch | .deps/accurate-resume-storage-reserved-console.log | Lapy storage/startup, then real YouTube playback

The accurate-resume candidate passed the PS5 build and host decoded-frame/sample
tests. It has not been deployed or launched; see [validation](VALIDATION.md).

The host companion's real public YouTube path passed credential-free AVC/AAC
and VP9/AAC cases on 2026-10-09 with video `jNQXAC9IVRw` at a 240p limit:
actual LAN-API pairing, resolution, separate-track DASH relay, host FFmpeg probe,
two-second audio/video decode and seek decode. Node 24.10.0, yt-dlp 2026.8.19
and yt-dlp-ejs 0.8.0 were used. Private result metadata is in
`.deps/public-avc-probe.json` and `.deps/public-vp9-probe.json`. No media URLs
or account credentials were saved in those reports. An unavailable public test
video returned the new allowlisted `video_unavailable` code with HTTP 404 through
the same real API. Resolver-error privacy/IPC regressions passed as part of the
67-test Python suite. Run `scripts/probe_youtube.py` for an explicit repeat.

These are host results. Real YouTube PS5 playback, pacing, sound, long-video
behavior, other qualities and live streams remain unverified. The native Lapy
candidate below is unchanged by this companion-only error-handling work.

Lapy storage candidate `1b74462d` passed native ASan/UBSan regressions, the
existing/resident/helper and mounted-data listing tests, and PS5 folder/ZIP/PFS
build inspection. Its frozen folder has 126 files, eboot SHA-256 prefix
`02317fceecfb2f30`, and exact-title Lapy helper prefix `c894f5eb1084c5a3`.
App state, captions, caches and diagnostics use `/data/prosperotube`; verified
Lapy access and app-directory write/read/close/delete are required before app
services start. Transient persistence fallback and render-loop elevation
retries are removed. GPU/decoder preload ordering remains part of acceptance.

- 2026-10-09 | Lapy storage | 1b74462d | PS5 6.02 | no-run: a title was running at the first console operation; no probes, upload, launch or input followed | .deps/console/storage-1b74462/ | verified storage, relaunch persistence and playback

The candidate has not run on PS5. Verify native startup and saved state under
the exact durable directory across two clean launches, then rerun playback,
caption, controller and teardown cases. Earlier accepted sandbox builds do not
prove this new storage/elevation contract.

Standing user instruction: the first console operation of each new session is
a read-only running-title check, after checking the shared lock locally. Any
running title blocks the session, including this app. Continue offline; do not
close it or proceed with service probes, uploads, launch or input. Recheck idle
state before launch after an install.

- 2026-10-09 | Lapy | a07e1733 | PS5 6.02 | no-run: first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/console/storage-a07e1733-second/ | storage acceptance

Candidate `800160c` passed the bounded startup and teardown case on PS5 firmware
6.02. Evidence identifies the matching build, the `youtube` document with eight
root entries, first-frame presentation checkpoints, `parked=1` after the app's
quit command, exact-title closure, runtime-layer release and healthy development
services. The shared console lock was released. This establishes those lifecycle
checkpoints; it does not verify video decoding, audible output, motion quality,
controller feel or live YouTube behavior.

The earlier candidate's absolute diagnostic paths were unavailable inside the
closed sandbox. The fix uses EVO's resolved data root and was verified while the
app remained sandboxed. An upload-only runner error interpreting FTP's successful
delete response was corrected before the accepted run; no title was launched by
that stopped upload attempt.

- 2026-10-09 | startup | bc3a686 | PS5 6.02 | inconclusive: app checkpoint; title cleanup passed | .deps/console/first-start-bc3a686/ | validate sandbox-aware paths
- 2026-10-09 | startup | 800160c | PS5 6.02 | pass: browser, frame checkpoint, graceful GPU park, release | .deps/console/sandbox-start-800160c-retry/ | fixture playback
- 2026-10-09 | provider | fe4a935 | PS5 6.02 | partial-pass: pairing, search, stream choices, teardown | .deps/console/fixture-avc-windows-fe4a935/ | direct playback
- 2026-10-09 | AVC | 59d68dd | PS5 6.02 | partial-pass: DASH opened, hardware AVC active, clock advances | .deps/console/direct-avc-59d68dd/ | full-duration playback
- 2026-10-09 | AVC | 675856b | PS5 6.02 | pass: bounded fixture video, scanout, 7.98/8.00s, teardown | .deps/console/complete-avc-675856b/ | captions, timing, VP9/HLS
- 2026-10-09 | captions | a42d689 | PS5 6.02 | no-run: initial check found a running title; no further console operations | .deps/console/caption-avc-a42d689/ | verify caption rendering

Candidate `a42d689` explicitly selects a downloaded caption sidecar as the
enabled primary track and disables inherited subtitle state when none is
available. Its real-engine host regression and EVO subtitle suite passed 72
checks with address/undefined-behavior sanitizers; the native provider suite
and PS5 build/package also passed. The frozen candidate has not been deployed:
the initial idle check blocked its console case. Caption rendering and timing
on PS5 remain unverified.

Candidate `675856b` passed the bounded AVC/AAC DASH fixture case: native UI
pairing/search, direct preferred-stream selection, a two-stream DASH open,
hardware AVC at 640x360, and progression to 7.98 of 8.00 seconds without a
reported decoder fatal. The inspected 3840x2160 scanout shows the generated
pattern and player UI. GPU park, exact-title closure, runtime release and
service health passed; the shared lock was released.

This is not audio/smoothness acceptance. The capture run's five-second timing
window reported 25.73 video frames/s against a 30 fps source and a 183.51 ms
maximum gap; a run without screenshot work is needed to isolate pacing. The
caption track loaded two cues but the OSD showed subtitles off, so activation
and timing need a separate check. VP9, HLS, seeking and live YouTube playback
remain unverified on this application.

The `59d68dd` runner printed a pass after observing raw EOF, but review found
that this is EVO's demux-exhausted flag. The clock had reached only 1.62 seconds
of an eight-second fixture; no scanout was captured. That output does not prove
full playback. The native overlay now also requires the clock to reach the
known duration before marking a video finished or advancing the queue. A
resolved duration is used if the player has none; unknown-duration completion
still needs a presentation-drain contract and must not be inferred from EOF.

Record firmware, loader/mounter versions, TV mode, source revision, companion
runtime/lock versions and artifact SHA-256 before testing.

1. Install the folder or ZIP through your existing homebrew workflow.
   Verify title `PPSA99142` is free on your console before installation.
2. Launch once: verify launcher artwork, eight native root entries, focus,
   paging, Options keyboard, settings, Back and soft close. Close the parked
   app from the PS5 switcher.
3. Run offline fixtures, pair, search and verify three thumbnails. Restart both
   sides and verify pairing persists. Wrong/expired codes should fail cleanly.
4. Play AVC/AAC, VP9/Opus and HLS. Record frames, sound, synchronization,
   pause/resume, stop and resource use. HLS is finite media marked as live.
5. Seek non-live fixtures. Check chapters, captions at 0–2 and 4–6 seconds,
   caption toggle/picker/delay, aspect and stats controls.
6. Favorite, queue and follow. Check persistence, partial-watch resume, watched
   marking, explicitly enabled ordered autoplay and empty-queue behavior.
7. Disconnect/reconnect LAN while browsing/resolving/playing. Cancel/reselect
   without restarting. Expire a session and retry using the saved ID/position.
   Automatic renewal is not implemented; do not report uninterrupted recovery.
8. In real mode, test public short/long AVC and VP9 SDR videos, channel/playlist
   pages, captions, a supported HLS stream and both SponsorBlock switches.
   Start with 720p, increase only with measured frame/audio evidence. Extraction
   alone is not playback proof.
9. Check logs/storage/title isolation and credential redaction. Repeat
   launch/stop/soft-close and confirm renderer/audio resources are released.

For native library UI acceptance, fixture mode provides two synthetic accounts
without enabling account networking. Select **Offline account one**, browse its
25 playlists and verify the next page. On a fixture video, open Touchpad account
actions; verify explicit confirmation and the result for rating, Watch later,
subscription and playlist changes. Create a private fixture playlist, add/remove
videos, rename it from its Touchpad menu, then delete it. Switch to **Offline
account two** and verify the changes are absent. Cancel a name prompt or switch
profiles before confirming and verify no change is submitted. R3 opens generated
comments: test top/newest, paging, the long-text reader and its reply thread.
Restarting the fixture server resets these accounts. Record these as synthetic
UI results, separately from any real-account acceptance.

With **Sync YouTube history** enabled, play a fixture for a positive duration,
stop it and reopen YouTube history for the same synthetic account. Verify its
resume position and isolation from the other account. Test zero-progress failure,
pause/resume, a run over 30 seconds with longer generated media, rapid video
changes, disabling sync and companion failure. Confirm an unconfirmed report
does not repeat automatically. Abrupt app termination may lose a final pending
report; measure the last confirmed position and lifecycle behavior. Real-account
history display and cross-device resume need a separate authorized live case.

Verify local profile isolation separately: favorite/follow/queue a fixture and
stop at a partial position in account one; switch accounts and confirm its local
lists are absent. Clearing account two's history/queue must preserve account
one's state. Switch accounts during playback and verify progress/completion
updates the original local profile without starting another profile's queue.
Restart the app and verify each profile's persisted lists. A version-1 state
fixture must migrate to Guest with its original local data preserved; unsupported
or malformed state files must remain unchanged. Keep copies of the test files
and distinguish this console-local state from YouTube account history.

The device authorization backend and native sign-in view are implemented with
offline coverage. Real authorization, TV account catalogs, account removal and OAuth
playback acceptance remain pending. Comment posting, casting, HDR, AV1 and standalone
SABR/token execution remain unfinished. The companion's cookie-profile account
APIs have offline tests but no live account acceptance. These gaps remain part
of the full-parity goal and require implementation and explicit acceptance cases.

Account removal now has offline implementation/coverage. On PS5, verify the
selected app-signed-in account is reviewed, submitted once, removed from the
refreshed profile list and replaced by Guest. Circle before confirmation must
retain it. Old playback links and authorization/job results must no longer work;
another account and Guest must remain usable, and the removed sign-in must stay
absent after restarting the companion. Check profile/source changes during the
review and pending job. Externally configured cookie accounts remain managed by
the companion configuration; console-local lists retain their previous owner.

The audio-tail revision `0af336d` needs a bounded hardware case with short AAC
media, a non-block-aligned audio tail, pause/resume near EOF, and seek after EOF.
Check audible tail output, natural completion, and failure behavior separately.
Host tests exercise the actual decoder/resampler and clock but mock AudioOut;
two silence submissions are an output-retirement assumption to verify on PS5.
Revision `5b98769b` implements explicit decoder and final-frame retirement gates
and completion without metadata duration. Verify `eof=1 complete=0` while buffered
media remains, followed by `complete=1` only after the final audio and video tail.
Test AVC and VP9, audio ending before video, pause near EOF, failed input and seek
after EOF. Native flush errors or GPU fence/flip timeouts must not advance the
queue. Host tests inject render retirement and mock Sony flush; these are not
hardware proofs of the new gates.

Device sign-in acceptance requires an idle-first console case using the paired
companion with its library worker enabled. Open Accounts & library > Sign in to
YouTube; verify the code and URL, automatic polling and stable focus. Test Circle
and explicit Cancel, expiry, source change during initiation and closing the
browser while a poll is pending. No canceled/late response may select a profile.
For successful authorization, the user must enter the displayed code on their
phone/computer, verify the authorized profile becomes selectable and restart the
companion to check persistence/refresh. Distinguish public initiation from real
account authorization. OAuth playback now has a TV-only adapter with offline
request/format/refresh coverage. Verify real account-only playback, captions,
TV signature/n challenges, profile isolation and post-refresh resolution before
claiming it works. A public TV probe returning LOGIN_REQUIRED proves no playback.

Comment-rating acceptance remains pending. In the offline fixture account, open
comments or a reply thread, select a comment, and press Touchpad. Check available
like/dislike/remove-rating choices, the full literal-text review, Cross submission
and Circle cancellation. A result screen must not resubmit, and changing profile
or companion source must invalidate old reviews. Guest and malformed capabilities
must produce no rating request. Repeat with a real designated test comment after
account authorization, then reload/read back its rating before treating transport
acceptance as a verified live outcome. Exercise expired references, timeout and
restarting the companion during a pending request without automatic resubmission.

Split live HLS now passes a public host case with AVC/AAC on the current NASA
Space Station stream. Verify the same transport on PS5 after the idle-first
guard: both audio and video, capture-free pacing, pause/resume, live-edge
recovery and a session longer than 30 minutes. Exercise rolling playlists and
seeking within the available live window separately. The host two-second decode
does not prove PS5 audio output, long-session signed-URL validity or DVR support.

Native live-chat acceptance requires an idle-first bounded case. Start with the
offline HLS fixture, open R3 chat, switch top/live mode, page and scroll a long
message while rolling updates arrive, then open ordinary comments and return.
Check selection/scroll retention when the message remains and fresh selection
when it leaves the snapshot. Leaving chat, setup, search, source/profile changes
and browser closure must stop polling and release the companion context. Repeat
with an available public live chat, reporting public service and PS5 rendering
separately. No posting, account authorization or replay is part of this reader
case; those require their own implementation and acceptance.

VOD URL renewal now needs PS5 acceptance through unchanged relay capabilities.
The host AVC/AAC and VP9/AAC probes use real public media with one local injected
403, fresh same-profile resolution, byte-identity checks, decode and seek. Test
the same bounded injection through PS5, then naturally expired links during a
long session. Check audio/video continuity, seeking and graceful failure when
identity changes. Live HLS paired-root renewal and controlled native restart
are implemented but require PS5 acceptance. Expired idle capabilities and
missing validators use the controlled reopen path rather than byte renewal.

The native input classifier distinguishes retry exhaustion and read/allocation
failures from natural EOF. Repeated premature `AVERROR_EOF` replies after the
demux retry budget must leave the video unwatched and the queue unchanged, even
after buffered audio and video have drained. Test this on PS5 with an interrupted
VOD and live stream, followed by stop/reopen and a clean natural ending. Host
classification and renderer tests cannot establish console recovery or sound.

Controlled restart now uses the native stop/resolve/launch lifecycle. Validate
the same video/profile/format and fractional resume position after a bounded
VOD interruption or expired idle capability. Validate live-edge restart
separately. Check caption visibility, audible audio and A/V synchronization,
pause until explicitly resumed, seek cancellation, browser cancel, account/source
changes, all three retry attempts, and that recovery leaves the queue entry
unconsumed. Missing/ambiguous formats and failed reopen must return control
without opening another video or replaying account mutations. All of these
remain hardware acceptance requirements.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 6.02 | no-run: first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/recovery-console.log | continue offline, await idle console

Live HLS paired-root renewal passed a real public host decode after one locally
injected root expiry. Validate unchanged relay manifests, both audio and video,
and continuity on PS5 separately. Expired segments/keys/init objects require
controlled live-edge restart; full DVR and naturally expired long-session
playback remain unverified. No console traffic occurred for the host case.

- 2026-10-09 | recovery / Lapy startup | 17f1b904 | PS5 6.02 | no-run: current first title check busy; no probes/upload/launch/close/input; owned lock released | .deps/storage-current-console.log | continue offline

Native live-chat composition is implemented. With an explicitly selected account
and active chat, check L3 keyboard open/cancel, Unicode limits, full literal text
in review, Cross submission once, confirmation and Circle return. R3 mode change
and Touchpad comments must still work. Repeat scope changes, delayed callbacks,
ended chat, uncertain response and cancellation; none may automatically resend.
Use a fixture first. Actual account posting requires a separately reviewed
message and acceptance; offline tests have sent no real messages.

- 2026-10-09 | fallback availability | read-only | pass: idle title list; no other console probes or changes; owned lock released | .deps/fallback-idle.json | build frozen candidate, recheck idle before deployment

- 2026-10-09 | fallback storage/startup | 9a7609c8 | PS5, firmware unreported | failed: exact build entered main, Lapy helper apply_failed (7), startup stopped; system captured a coredump | .deps/chat-compose-fallback-console.log; .deps/console/storage-chat-compose-9a7609c8-fallback/klog.txt | offline Lapy/startup diagnosis

Installation, full byte readback after settling, registration and idle recheck
passed. One launch was sent. No input, close payload or second launch followed;
the owned lock was released. Writable app storage, persistence and post-failure
service health were not accepted. Do not retry these identical bytes. Diagnose
the helper apply failure and startup-failure lifecycle before another bounded
case; the fallback's firmware has not been established.

- 2026-10-09 | primary storage/startup with log gate | bb5672a0 | PS5, firmware unreported in this case | failed: exact build reached main/display/IME, helper apply_failed (7), startup stopped, exit(1) then SIGSYS; no input/close/relaunch, owned lock released | .deps/startup-gate-console.log; .deps/console/storage-startup-gate-bb5672a-primary/klog.txt | expose helper-stage logs offline before a changed candidate

The log gate has host regression and PS5 build coverage, but it did not restore
startup in this bounded primary-console case. Both complete byte readbacks,
registration and idle checks passed before one launch. The case stopped on the
matching storage-failure breadcrumb. Storage/persistence, graceful teardown,
post-failure service health and YouTube playback remain unverified. No identical
candidate retry. See VALIDATION.md for hashes and logging-discovery limits.

- 2026-10-09 | Lapy PR50 app candidate | ec358d07 | no-run: first primary FTP connection refused; running-title state unestablished; no directory preparation/upload/launch/close/input/retry; owned lock released | .deps/lapy-pr50-console.log | continue offline, candidate undeployed

Complete PS5 build and freeze passed for `prosperotube-ec358d07_1010-0225`:
126 files, eboot SHA-256 prefix `08748ab3bd47c1c2`. These bytes have not yet
been tested on either console. Storage/startup, helper stage logs, persistence,
clean teardown, service health and actual YouTube playback remain pending.

- 2026-10-09 | seek / PR50 candidate | 00372796 | PS5 | no-run: both installs verified twice; prelaunch checks busy | .deps/seek-console.log; .deps/seek-console-primary.log | await idle
- 2026-10-09 | reserved installation reuse | 00372796 | PS5 | no-run: idle, installed bytes differ; no launch | .deps/seek-console-reserved.log | fresh verified install
- 2026-10-09 | reserved fresh installation | 00372796 | PS5 | no-run: first title check busy; no probes/upload/launch/input/close | .deps/seek-console-reserved-install.log | offline work

The full PS5 package and freeze passed for build
`prosperotube-00372796_1010-0258` (126 files, eboot SHA-256 prefix
`ce25ebbfc8158d28`). No case launched it. All owned locks were released.
Firmware is unreported. Installed-byte mismatch has no established cause;
see VALIDATION.md for the verifier limitation. Storage, helper execution,
persistence, teardown and real YouTube playback remain pending.

- 2026-10-09 | seek/resume origin | 3c3eb818 | offline PS5 | partial-pass: full build/package/freeze; host shipping-code regressions passed; no deployment | .deps/resume-timeline-ps5-build.log; .deps/resume-timeline.log | idle-first storage/startup case

Frozen build `prosperotube-3c3eb818_1010-0333` has 126 files, eboot SHA-256
prefix `7c3e112076128376`. Its in-place seeks and initial resume use the common
container origin; the audio re-anchor returns to relative UI positions.
Host packet/clock assertions do not establish PS5 decoded position or sound.
This candidate has never launched. Startup/storage, persistence, actual YouTube
playback, A/V synchronization and clean teardown remain pending.

- 2026-10-09 | stored SELF verifier | 3c3eb818 | PS5 | no-run: upload comparison requested ELF view after duplicate mode toggle; no launch | .deps/resume-storage-console.log | correct adapter offline
- 2026-10-09 | corrected install | 3c3eb818 | PS5 | no-run: all files verified twice after settling; registration passed; final title check busy | .deps/resume-storage-stored-self-console.log | offline work

Both cases released their owned locks. No launch, input or close was sent.
The corrected case establishes stored bytes, not app execution. Current PR50
startup/storage, persistence, playback/audio and teardown remain unverified.

- 2026-10-10 | video-only resume | e52315dd | offline PS5 | partial-pass: complete package/freeze and host decoded-frame regression; no deployment | .deps/video-resume-ps5-build.log; .deps/video-resume-timeline.log | idle-first startup

Build `prosperotube-e52315dd_1010-0354` has 126 frozen files and eboot SHA-256
prefix `60967ac1c01c1852`. Fresh Guest YouTube AVC/AAC DASH host decode passed
eight seconds at offsets zero and four. This candidate has never launched;
the host result does not establish console playback, motion or sound.

- 2026-10-10 | settled startup control | 3c3eb818 | PS5 | no-run: fresh exact readback and registration passed; prelaunch title check busy; no uploads/launch/input/close | .deps/resume-storage-reuse-control-console.log | offline work
- 2026-10-10 | current primary candidate | e52315dd | PS5 | no-run: first title check busy; no other probes/uploads/launch/input/close | .deps/video-resume-storage-primary-console.log | offline work

Both owned locks were released. The settled control used the earlier committed
installation and unchanged PR50 helper; the current `e52315dd` package remains
undeployed. No startup, persistence, playback or teardown acceptance was added.

- 2026-10-10 | reserved marker candidate | 58390ef6 | PS5 helper reports firmware 12700001 | failed: two full stored-byte verifications and registration passed, one launch reached main, helper target_stop error=16 before credential/root changes, startup stopped; no input/close/relaunch, owned lock released | .deps/marker-storage-reserved-console.log; .deps/console/storage-marker-58390ef6-fallback/klog.txt | inspect stop predicate with diagnostic-only candidate

A separate idle-first read-only case retrieved the matching helper-stage log
from the reserved fallback console. It reports roots_moved=0, cred_changed=0
and detached=1. It does not expose which stop predicate failed. Storage,
persistence, physical playback and clean teardown remain unaccepted. See
VALIDATION.md for the log hash and diagnostic-only build option.

- 2026-10-10 | stopped-target snapshot diagnostic | 146945b5 | PS5 | failed: full installation/readbacks and one launch, stable stopped snapshots with 92 threads/credential references; helper requires single-thread private credentials; no root/credential changes, detached; no input/close/relaunch, lock released | .deps/lapy-stop-reserved-console.log; .deps/console/lapy-stop-helper-readonly-146945b/ | split query-only warmup from resident decoder/display creation

The separate diagnostic-log retrieval began with an idle title check and made
no application changes. The subsequent startup-order overlay has host coverage
only. A newly built candidate must establish storage proof, decoder availability,
native UI, persistence and teardown before real YouTube playback acceptance.

- 2026-10-10 | single-thread Lapy startup | b9b5c916 | reserved PS5 .40 | partial-pass: verified install, Lapy storage, four resident codecs, UI/first frame and GPU park; exact close sent after park; adapter result 1 because its expected ShadowMount phrase was absent | .deps/lapy-warmup-reserved-console.log | independently verify release
- 2026-10-10 | title release read-only | b9b5c916 | PS5 .40 | pass: first check idle, no remaining title sandbox, FTP/klog/elfldr reachable; captured kernel log binds launched PID 106/app ID 0x4018 to kill, video-owner removal and completed app-mount teardown | .deps/console/warmup-release-readonly-b9b5c916/ | real public YouTube case

The original adapter failure is retained. No second launch or preference change
occurred, so this does not establish persistence across relaunch or playback.

- 2026-10-10 | first public YouTube case | b9b5c916 native | reserved PS5 .40 | failed: verified reuse and startup, pairing saved; search returned resolver invalid_response before playback | .deps/public-video-reserved-console.log | fix Windows worker protocol encoding
- 2026-10-10 | owned failed-case cleanup | b9b5c916 | PS5 .40 | pass: known PID 121/app ID 0x8018 gracefully parked, exact close, matching process/mount release, idle titles and reachable services | .deps/console/public-video-b9-cleanup/ | fresh idle-first public-video case with corrected companion

- 2026-10-10 | actual public YouTube playback | b9b5c916 native + c7d6f35 companion | reserved PS5 .40 | partial-pass: native AVC, visible video/captions, 284 published frames/zero late drops, progress and EOF/completion; original adapter result 1 retained for too-strict 0.10-second endpoint threshold (final frame 18.87/19 at 15 fps) | .deps/console/public-video-utf8-b9b5c916-fallback/ | independent saved-sample endpoint check and graceful cleanup
- 2026-10-10 | public playback cleanup/evidence | same native | PS5 .40 | pass: owned process identity, GPU park before exact close, matching process/mount release, idle/reachable services, capture retrieved, pairing and played Guest history saved | .deps/console/public-video-utf8-b9-cleanup/ | history reload, pause/seek and physical audio/controller acceptance

- 2026-10-10 | history/pause/seek candidate | d4d5b66d | reserved PS5 .40 | no-run: first title check busy; no other probes/uploads/launch/input/close, lock released | .deps/history-pause-seek-reserved-console.log | authorized primary fallback
- 2026-10-10 | fallback idle/config read-only | none | PS5 .30 | pass: minimal title-only case idle; separate first-check-idle configuration read found no provider file; no app changes | .deps/console/controller-primary-idle-only/; .deps/console/controller-primary-config-readonly/ | fresh primary pairing/playback-control case
- 2026-10-10 | primary playback controls | d4d5b66d | PS5 .30 | no-run: console became busy before case's first title check; no other probes/uploads/launch/input/close, lock released | .deps/public-pause-seek-primary-console.log | continue offline; no repeated hardware polling

The separately owned primary companion on port 8766 was stopped after the
no-run. The previously paired `.40` companion on port 8765 remains available;
it was not interrupted. The `d4d5b66d` controller package is frozen but has not
been deployed. Saved-history reload, pause/seek and physical pad/audio acceptance
remain open; the preceding `b9b5c916` real-video result remains valid.

- 2026-10-10 | saved-history playback controls | 5cc941f0 | PS5 | pass: one title launch, verified install/storage, Guest pairing/history reload, pause/resume, native seek success, EOF, park/exact release/healthy services | .deps/history-pause-seek-highlight-replay.log | physical sound, broader playback, SponsorBlock hardware acceptance

The original root-navigation and strict-status adapters retain result 1. Their
same owned process completed the corrected replay scenario; no second launch
occurred. The rejected status bytes were not retained, so its exact cause is
unproven. New R3 highlights remain offline-tested; physical sound/controller feel,
long sessions, wider formats and full SmartTube equivalence remain open.
