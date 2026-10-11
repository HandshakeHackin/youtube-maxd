"""Extract shipping time labels, early-refresh guard and picker integration."""
from pathlib import Path
import sys
root,output=map(Path,sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
for relative,tag,name in [
 ('ui_rml/src/evo_rmlui_app.cpp','refresh','rml.inc'),
 ('ui_kit/src/evo_hui_osd.cpp','HUI','hui.inc'),
 ('core/src/screens/AudioTrackPickerScreen.cpp','rows','rows.inc'),
 ('core/src/screens/AudioTrackPickerScreen.cpp','selection','selection.inc')]:
    source=(root/relative).read_text()
    start=source.index('// ProsperoTube time display '+tag)
    end=source.index('// End ProsperoTube time display '+tag,start)
    if tag=='refresh':assert end<source.index('if (state == m_last_state &&')
    output.joinpath(name).write_text(source[start:end])
