"""Inventory pinned upstream feature surfaces; inventory is not parity proof."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

PIN = "2c60453007dfc3775ff525c3837c2d1d4e95aab9"
ROOT = Path(__file__).resolve().parents[1]

def audit(source):
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if revision != PIN: raise ValueError(f"SmartTube baseline mismatch: {revision}")
    common = source / "common/src/main"
    labels = {element.attrib["name"]: "".join(element.itertext())
              for element in ET.parse(common / "res/values/strings.xml").getroot().findall("string")}
    package = common / "java/com/liskovsoft/smartyoutubetv2/common"
    features = []
    for file in sorted((package / "app/presenters/settings").glob("*SettingsPresenter.java")):
        text = file.read_text(encoding="utf-8")
        methods = list(re.finditer(r"(?:private|protected|public)\s+(?:static\s+)?void\s+(append\w+)\s*\([^)]*\)\s*\{", text))
        for index, match in enumerate(methods):
            section = text[match.end():methods[index + 1].start() if index + 1 < len(methods) else len(text)]
            names = sorted(set(re.findall(r"R\.string\.(\w+)", section)))
            features.append({"id": file.stem + "." + match[1], "source": file.relative_to(source).as_posix(),
                             "line": text[:match.start()].count("\n") + 1,
                             "labels": {name: labels.get(name, name) for name in names},
                             "ps5_status": "unverified", "evidence": []})
    for directory in (package / "app/presenters", package / "app/models/playback/controllers"):
        for file in sorted(directory.glob("*.java")):
            if file.stem.startswith(("Base", "AppDialog", "Splash")): continue
            features.append({"id": "behavior." + file.stem, "source": file.relative_to(source).as_posix(),
                             "line": 1, "ps5_status": "unverified", "evidence": []})
    return {"baseline_repository": "https://github.com/yuliskov/SmartTube", "baseline_revision": revision,
            "target": "PS5", "meaning": "Feature inventory only; every equivalence claim needs behavior and hardware evidence.",
            "features": features}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "native/parity/smarttube-baseline.json")
    args = parser.parse_args()
    result = audit(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(f"Inventoried {len(result['features'])} upstream feature surfaces at {PIN}; PS5 parity remains unproven.")

if __name__ == "__main__": main()
