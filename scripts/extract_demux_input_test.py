"""Extract exact shipping input classification for the real completion test."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
begin = "static _Atomic int tube_input_state;"
end = "static void tube_demux_terminal_input(int read_result, int io_error, int exhausted)"
if source.count(begin) != 1 or source.count(end) != 1:
    raise RuntimeError("Demux input classification boundary mismatch")
start = source.index(begin)
brace = source.index("{", source.index(end))
depth = 1
stop = brace + 1
while depth:
    if source[stop] == "{":
        depth += 1
    elif source[stop] == "}":
        depth -= 1
    stop += 1
result = source[start:stop] + "\n"
name = "static int demux_read_is_final(int read_result)"
if source.count(name) != 1:
    raise RuntimeError("Demux retry classification boundary mismatch")
start = source.index(name)
stop = source.index("{", start) + 1
depth = 1
while depth:
    if source[stop] == "{":
        depth += 1
    elif source[stop] == "}":
        depth -= 1
    stop += 1
limits = re.findall(r"^#define DEMUX_RECOVER_TRIES (\d+)$", source, re.M)
if len(limits) != 1:
    raise RuntimeError("Demux retry limit mismatch")
result += "#define DEMUX_RECOVER_TRIES " + limits[0] + "\n" + source[start:stop] + "\n"
Path(sys.argv[2]).write_text(result)
