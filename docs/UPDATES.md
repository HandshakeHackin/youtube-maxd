# Updates

ProsperoTube uses the same homebrew.page signed catalog and loader-started
helper as the owner's other applications. The GitHub Releases discovery and
in-process replacement engine have been removed.

## What the user sees

Once per launch, after the service starts (or after twenty seconds), a worker
checks the signed catalog. A verified newer version produces a notice directing
the user to Settings > About. Automatic failures stay quiet. Check for updates
runs a fresh check and reports an unavailable catalog or unlisted application.
About also offers notification preferences, the signed release notes, Skip
version, and Update now.

Update now starts the packaged `self-updater.elf` through the console's payload
loader on loopback port 9021, then streams the GitHub release ZIP to it. The
helper verifies and stages the archive. Cancel before applying leaves the
installed files untouched; downloads restart from the beginning on retry.
Restart to finish updating gives the helper permission to apply and closes the
app. Wait for the helper's completion notification before reopening it.
Prepared updates belong to that helper session; they do not survive app exit.
Development builds may check the catalog but cannot install updates.

## Trust and installation

The source of truth is `https://homebrew.page/api/v1/`. The signed catalog's
GitHub Pages mirror is tried if the primary origin cannot provide a verified
answer. Both require the kit's pinned Ed25519 signing keys, a non-decreasing
catalog sequence, and the signed manifest's SHA-256 for the app entry. The
entry's artifact URL, SHA-256, size, version and notes are covered by that
signature. An unsigned GitHub release cannot become an update offer.

The console transport uses independent libcurl handles with the embedded
service's bundled CA roots, mandatory HTTPS certificate and hostname checks,
and its proven console socket setup. Release redirects are restricted to the
kit's GitHub allowlist. No credentials or console identity are sent.

The app stores the accepted catalog sequence in
`/data/prosperotube/update-sequence` and notification/skip preferences in
`/data/prosperotube/update-preferences`. The old updater's download, staging and
journal files are unused; they are not automatically removed or recovered.

The helper validates the archive's title, version, paths, bounds, links, CRCs,
size and SHA-256 before applying. It resolves ShadowMountPlus's `mount.lnk`,
refuses image installs and ambiguous/wrong-version installations, and waits for
the app to close. It swaps top-level entries inside the existing install
folder, preserves entries not mentioned by the release, applies mode 0777,
refreshes registered home-screen metadata and reports the result. See the
pinned upstream [helper documentation](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/blob/b8dc4a3fe2e05e826b037bd68824751c7468a6d7/docs/SELF_UPDATE.md)
for previous-copy handling and failure recovery. This is the shared helper's
recovery model; ProsperoTube no longer counts failed starts or rolls back a
running replacement itself.

## Source and packaging

`native/update/SOURCE.json` pins the public boilerplate revision and SHA-256
of every imported kit, helper and miniz file. Source notices and the miniz
license are retained. The kit is compiled into the application; its console
transport is enabled with the embedded service. `scripts/build_ps5.sh` builds
the payload helper in the selected native work directory, and the packaging
overlay includes it at the app root before ZIP or image generation.

## Before the first release

ProsperoTube needs a public release ZIP and a published signed catalog entry
for title `PPSA99142`. Until then the check reports that the application is
not listed; it does not fall back to unsigned releases or development offers.
Publishing the repository, catalog entry or first release requires the owner's
decision. No credentials are shipped to access a private repository.

## Validation

`bash scripts/test_update.sh` runs the pinned kit's regression against the
real helper over socket pairs, real archives and temporary install folders
under ASan/UBSan. It covers signature refusal, hashes, sequence replay,
origin fallback, download/staging, cancellation, installation refusals,
replacement rollback and previous-copy handling. Adapter checks cover startup
versus manual failures, refusal to install unknown/development offers, About
rows and persisted preferences. `python3 scripts/test_update_host.py` checks
the generated shipping host and source integration after preparation;
`python3 scripts/test_release.py` checks release naming and ZIP permissions.

Shutdown joins an active catalog worker. There are at most three small GETs
per origin and two origins, each with a 20-second total curl timeout (the
10-second connection timeout is included): a configured transfer budget of
120 seconds. This is not a hard wall-clock bound: curl uses `NOSIGNAL`, while
EVO's synchronous system DNS resolver uses console-default timeout/retries
and cannot be interrupted by that curl timeout. Slow DNS can therefore extend
shutdown. The helper also identifies a running title from sandbox directories;
a stale directory can delay or refuse replacement. Both remain console
acceptance concerns; the shared kit has not been modified to work around them.

A full end-to-end update on a console remains necessary: verified catalog
check, helper launch with the payload loader available, a newer signed release,
app exit, file replacement, home-screen refresh and relaunch. Offline helper
tests cannot establish those hardware results.
