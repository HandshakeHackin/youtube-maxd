"""Extract the shipping host's unplayable scheduling/cancellation blocks."""
from pathlib import Path
import sys
root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
source = (root / 'core/src/screens/ProviderHostScreen.cpp').read_text(encoding='utf-8')
for tag, name in (('schedule','schedule.inc'), ('wait','wait.inc'), ('input cancellation','input.inc'), ('exit cancellation','exit.inc')):
    start = source.index('// ProsperoTube unavailable queue ' + tag)
    end = source.index('// End ProsperoTube unavailable queue ' + tag, start)
    (output / name).write_text(source[start:end],encoding='utf-8')
