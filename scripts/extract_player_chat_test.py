"""Extract the shipping player chat renderer blocks for recording tests."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
for relative, tag, name in (
    ('ui_kit/src/evo_hui_osd.cpp', 'draw', 'draw.inc'),
    ('ui_rml/src/evo_rmlui_app.cpp', 'RML', 'rml.inc'),
):
    source = (root / relative).read_text()
    start = source.index('// ProsperoTube player chat ' + tag)
    end = source.index('// End ProsperoTube player chat ' + tag, start)
    (output / name).write_text(source[start:end])
