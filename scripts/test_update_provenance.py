"""Check every shipped upstream update source against the recorded pin."""
from pathlib import Path
import hashlib, json
root = Path(__file__).resolve().parents[1]
base = root / "native/update"
source = json.loads((base / "SOURCE.json").read_text())
assert len(source["revision"]) == 40
for name, digest in source["files"].items():
    assert hashlib.sha256((base / name).read_bytes()).hexdigest() == digest, name
assert (base / "third_party/miniz/LICENSE").is_file()
assert not any((root / "native" / name).exists() for name in ("tube_update_zip.c", "tube_update_http.c"))
print("PASS: pinned update kit/helper/miniz provenance and removed self-replacement engine")
