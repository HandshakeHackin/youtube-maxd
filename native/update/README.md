# Shared update kit

Imported from public ps5-native-app-boilerplate commit
`b8dc4a3fe2e05e826b037bd68824751c7468a6d7`; see SOURCE.json for hashes.
The update kit is byte-identical to that revision and includes the shared
signed catalog mirror and release-note enhancements. Helper source is also
unchanged; only its Makefile source/output paths are adapted for this tree.
Miniz 3.0.2 and its license are preserved under third_party/miniz.

The application adapter lives in native/tube_update*. It supplies the
embedded service's curl trust roots/socket setup and runtime helper paths,
without recompiling the kit's duplicate console libc stand-ins. The regression
in tests/test_update_kit.cpp is upstream's test with include paths adjusted.
