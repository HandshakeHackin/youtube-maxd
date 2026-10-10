"""Extract the shipping review renderers and accessibility focus path."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
for relative, tag, name in (
    ('ui_kit/src/evo_hui_osd.cpp', 'review draw', 'draw.inc'),
    ('ui_kit/src/evo_hui_osd.cpp', 'review focus', 'focus.inc'),
    ('ui_rml/src/evo_rmlui_app.cpp', 'review RML', 'rml.inc'),
):
    source = (root / relative).read_text()
    start = source.index('// ProsperoTube SponsorBlock ' + tag)
    end = source.index('// End ProsperoTube SponsorBlock ' + tag, start)
    (output / name).write_text(source[start:end])
