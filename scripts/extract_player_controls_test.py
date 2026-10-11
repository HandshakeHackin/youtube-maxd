"""Extract actual shipping input/visibility blocks, not a second implementation."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
source = (root / 'core/src/screens/PlayerScreen.cpp').read_text()
for tag, name in (('hidden Cross activity', 'activity.inc'), ('controls visibility', 'visibility.inc')):
    start = source.index('// ProsperoTube ' + tag)
    end = source.index('// End ProsperoTube ' + tag, start)
    (output / name).write_text(source[start:end], encoding='utf-8')
start = source.index('    if (pressed & PadButtons::Cross) {')
end = source.index('    if (pressed & PadButtons::Circle) {', start)
(output / 'cross.inc').write_text(source[start:end], encoding='utf-8')
