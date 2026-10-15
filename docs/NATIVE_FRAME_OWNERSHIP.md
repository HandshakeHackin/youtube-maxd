# Native caller-frame ownership

The native decoder's eight caller-owned output slots now have explicit
references. The earlier cyclic `au_ring % FRAME_POOL_SLOTS` selection could
reuse a published/presented slot after eight outputs without consulting GPU
completion. Its comment assumed twelve slots and timely rendering. A generated
host regression held the first presenter frame across pool wrap and failed
when the decoder selected that same slot.

The fixed, allocation-free metadata table distinguishes the native decode-call
write claim, a borrowed reorder/receive reference, pp's current publication and
each returned GPU snapshot. Decoder selection scans for an unowned slot. It
waits outside pp/pool mutexes using the existing native decode watchdog's
monotonic deadline; expiration fails rather than reusing a busy slot. Pipeline
depth, codec/profile negotiation, packet filtering and rendering settings are
unchanged. This is backpressure for ownership, not a decoder performance mode.

Publication takes its reference before replacing/releasing the old picture.
A pp snapshot takes an independent reference under the publication mutex, then
returns unlocked. No pp mutex spans a GPU submit or wait. The application
transfers a drawn snapshot to the pending presentation, releases unused
snapshots immediately and releases submitted sources only at the existing
successful GPU-fence/flip retirement gate. Hidden background video uses the
same successful-present boundary for intentional discard.

A failed present retains its source reference. It does not authorize reuse.
Native close releases decoder/reorder references; pp publication is cleared
after the decode thread joins and before native decoder teardown. Close or
resize waits for external references using the existing deadline. If they
remain, it marks the slot unavailable, logs unsafe teardown/restart required,
and retains the decoder and memory mappings. It must not free memory under a
possibly active GPU. This conservative failed-present retention requires a
process restart; there is no detached cleanup worker or timeout-based recycle.

The first valid decoder output emits one diagnostic with returned pointer,
pool base, slot index/size, coded and visible geometry, both pitches, configured
profile, accepted/picture/error flags. The existing strict output/pool checks
still precede pixel reads and presentation.

The local hardware-video-decoding research's implementation guide and
`examples/common/memory_pipeline.cpp` require caller-slot pointer identity and
retention through completed flip. VP9 display policy is retained with each
input's unique attached tag. Returned public picture metadata selects the
matching pending input: hidden completions arrive much later than shown
pictures and cannot be paired by coded-AU FIFO position. A shown completion
must match the oldest pending shown input, preserving order and the original
PTS even when equal or missing. Unknown identities fail closed. Metadata reads
hold the surface lease and validate/cache-invalidate its bounded trailer;
frame publication and GPU retirement keep their existing references.

The ownership change addresses the confirmed pool reuse race; completion
identity addresses the separately confirmed stale-picture association bug.
Neither establishes the cause or fix of the observed first-valid 4K VP9 GPU
unmapped-read fault. That fault lacks a logged
source/pool/descriptor mapping in the failed case. Direct AGC descriptor
alignment, pitch and visible crop remain unchanged pending concrete evidence.

Validation covers generated native claim/teardown decisions, real pp
publication/snapshot ownership, concurrent producer/presenter data stability,
failed-present retention, stop ordering and existing drain/seek paths under
ASan/UBSan. Native decoder hardware, rendered 4K pixels and failure recovery
still need a frozen console acceptance case. No console run was performed for
this source change.
