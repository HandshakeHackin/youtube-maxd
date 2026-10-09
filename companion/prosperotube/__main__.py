import argparse
import json
import os
import re
from pathlib import Path
import uvicorn
from .app import create_app
from .extractor import Extractor


def main():
    parser = argparse.ArgumentParser(description="ProsperoTube user-controlled LAN companion")
    parser.add_argument("--host", default="127.0.0.1", help="Use 0.0.0.0 to allow the PS5 on your LAN")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--public-base", help="Reachable LAN address, e.g. http://192.168.1.20:8765")
    parser.add_argument("--state-dir", type=Path, default=Path.home() / ".local/share/prosperotube")
    parser.add_argument("--cookies", type=Path, help="Explicitly supplied test-account cookie file; never reads browser cookies")
    parser.add_argument("--extractor-args", type=Path, help="Optional JSON yt-dlp extractor arguments, including a configured token provider")
    parser.add_argument("--sponsorblock", action="store_true", help="Enable optional hashed-prefix SponsorBlock queries")
    parser.add_argument("--js-runtime", choices=("deno", "node", "bun", "quickjs"), help="Explicit yt-dlp JavaScript runtime (default: deno)")
    parser.add_argument("--js-runtime-path", help="Optional path to the chosen JavaScript executable")
    parser.add_argument("--fixtures", type=Path, help="Explicit offline mode using media from scripts/generate_fixtures.py")
    parser.add_argument("--library-node", help="Enable library/comments support with this Node.js executable")
    parser.add_argument("--library-profiles", type=Path, help="Explicit local account-profile configuration; credentials stay on this host")
    args = parser.parse_args()
    if args.library_profiles and not args.library_node:
        parser.error("--library-profiles requires --library-node")
    if args.fixtures and (args.library_node or args.library_profiles):
        parser.error("Library/account networking is disabled in offline fixture mode.")
    args.state_dir.mkdir(parents=True, exist_ok=True)
    os.chmod(args.state_dir, 0o700)
    token_file = args.state_dir / "token"
    if token_file.exists():
        token = token_file.read_text().strip()
        if not re.fullmatch(r"[A-Za-z0-9_-]{32,128}", token):
            parser.error("The companion token file is invalid. Remove it and pair again.")
        os.chmod(token_file, 0o600)
    else:
        import secrets
        token = secrets.token_urlsafe(32)
        fd = os.open(token_file, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as f:
            f.write(token + "\n")
    extra = json.loads(args.extractor_args.read_text()) if args.extractor_args else {}
    if args.js_runtime_path and not args.js_runtime:
        parser.error("--js-runtime-path requires --js-runtime")
    runtimes = {args.js_runtime: {"path": args.js_runtime_path} if args.js_runtime_path else {}} if args.js_runtime else None
    extractor = Extractor(cookies=str(args.cookies) if args.cookies else None, extractor_args=extra, js_runtimes=runtimes)
    if args.fixtures:
        from .demo import create_demo_app
        app = create_demo_app(args.fixtures, token=token, public_base=args.public_base)
        print("OFFLINE FIXTURE MODE: generated media only; YouTube is not contacted.")
    else:
        library = None
        if args.library_node:
            from .library import LibraryService
            try:
                library = LibraryService(args.library_node, profile_config=args.library_profiles)
            except (ValueError, OSError, KeyError):
                parser.error("The library profile configuration is invalid. Check profile identifiers and cookie files.")
        app = create_app(extractor=extractor, token=token, public_base=args.public_base, sponsorblock=args.sponsorblock, library=library)
    print("ProsperoTube companion 0.1.0")
    print(f"Pairing code (valid for 10 minutes): {app.state.pairing_code}")
    print("On the PS5 enter the companion LAN address followed by # and that code.")
    print("Example: http://192.168.1.20:8765#123456")
    if args.host == "127.0.0.1":
        print("Loopback mode: add --host 0.0.0.0 --public-base http://YOUR-LAN-IP:8765 for PS5 access.")
    # Paths contain short-lived media capabilities; access logs stay disabled.
    uvicorn.run(app, host=args.host, port=args.port, access_log=False, log_level="warning")


if __name__ == "__main__":
    main()
