"""Extract the actual UI-kit and fallback marker rendering blocks."""
from pathlib import Path
import sys

root, output = Path(sys.argv[1]), Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
for relative, tag, name in (("ui_kit/src/evo_hui_osd.cpp", "marker draw", "kit.inc"),
                            ("ui_rml/src/evo_rmlui_app.cpp", "marker RML", "rml.inc")):
    source = (root / relative).read_text()
    start = source.index("// ProsperoTube SponsorBlock " + tag)
    end = source.index("// End ProsperoTube SponsorBlock " + tag + ".", start)
    (output / name).write_text(source[start:end])
