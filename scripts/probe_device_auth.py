"""Explicit credential-free device-code request; no account authorization or poll."""
import argparse
import asyncio
import json
from pathlib import Path
import tempfile

from prosperotube.device_auth import DeviceAuth, OAuthProfiles
from prosperotube.library import LibraryService
from prosperotube.models import ServiceError


async def probe(node):
    report = {"account_authorized": False, "token_poll_requested": False, "console_traffic": False}
    with tempfile.TemporaryDirectory(prefix="prosperotube-device-probe-") as directory:
        store = OAuthProfiles(directory)
        library = LibraryService(node, oauth_store=store, timeout=45)
        service = DeviceAuth(lambda request: library.execute(request, operation="device"),
                             store, library.add_oauth_profile, lambda: True)
        try:
            started = await service.start("Device-code probe")
            report["start"] = started["state"]
            report["cancel"] = service.cancel(started["session"])["state"]
            report["credentials_stored"] = store.path.exists()
        except ServiceError as error:
            report["error_code"] = error.code
        finally:
            service.close()
            await library.close()
            report["worker_closed"] = library.process is None
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--node", required=True, help="Explicit Node.js executable")
    parser.add_argument("--output", type=Path, required=True, help="Private metadata report; contains no codes or credentials")
    args = parser.parse_args()
    report = asyncio.run(probe(args.node))
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))
    return 0 if report.get("start") == "pending" and report.get("cancel") == "canceled" and not report.get("credentials_stored") else 1


if __name__ == "__main__":
    raise SystemExit(main())
