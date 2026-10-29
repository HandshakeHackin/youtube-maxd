# Update notifications (in development)

The next build checks YouTube Max’d’s public GitHub releases in the background at launch, after the built-in service is ready, and every six hours while open. A newer stable version produces a notice once per version per launch. Releases marked draft or prerelease, preview version tags, and malformed replies do not produce update offers. Publishing a tag alone does not trigger a notice: publish a normal GitHub release with a stable `vMAJOR.MINOR.PATCH` tag and make it the latest release.

Settings → About includes:

- **Check for updates:** checks immediately and reports the result, even if automatic notices are off.
- **Automatic update notices:** on by default; the console-wide choice is saved in the app data directory as `release-notifications`.
- **Download latest release:** displays the project’s release-page URL and a QR code for a phone or computer. Follow that release’s installation instructions on PS5.

The checker uses `https://api.github.com/repos/HandshakeHackin/youtube-maxd/releases/latest` without a GitHub account, API key, or YouTube credentials. Requests verify HTTPS, time out after ten seconds, and accept at most 256 KiB. Automatic failures (including offline or GitHub rate limits) stay quiet and retry at the next interval or launch. Manual failures are shown. It does not download or install an app, and does not enable the inherited upstream catalog updater.

Devices must have this feature installed before they can show the notification. Existing published builds cannot gain it remotely. A release is detected at the next successful check, rather than through a push service. Console and phone QR testing is still required.

Validation: `bash scripts/test_maxd_release_notice.sh` exercises the shipping decision logic and asynchronous checker with deterministic transport; the native support regression checks the actual release-link route and QR payload in both interfaces. PS5 package compilation checks the real curl transport and app integration.
