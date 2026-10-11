"""Extract the shipping phone resolver guards and retry input routing."""
from pathlib import Path
import sys
root, output=map(Path,sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
s=(root/'core/src/screens/ProviderHostScreen.cpp').read_text(encoding='utf-8')
for tag,name in (('schedule','phone-schedule.inc'),('wait','phone-wait.inc'),('input cancellation','phone-input.inc')):
    start=s.index('// ProsperoTube unavailable phone '+tag)
    end=s.index('// End ProsperoTube unavailable phone '+tag,start)
    (output/name).write_text(s[start:end],encoding='utf-8')
