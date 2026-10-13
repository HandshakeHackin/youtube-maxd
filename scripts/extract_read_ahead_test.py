"""Extract the shipping read-ahead sizing for the video buffer test.

From the generated PlaybackController.cpp: the read-ahead constants, the
bitrate helper and sizeReadAhead(), exactly as they are compiled into the app.
"""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
begin = "static constexpr double    kReadAheadSeconds"
end = "static void sizeReadAhead(bool networkSource, const AVFormatContext* fmt,"
if source.count(begin) != 1 or source.count(end) != 1:
    raise RuntimeError("Read-ahead sizing boundary mismatch")
start = source.index(begin)
stop = source.index("{", source.index(end)) + 1
depth = 1
while depth:
    if source[stop] == "{":
        depth += 1
    elif source[stop] == "}":
        depth -= 1
    stop += 1
result = source[start:stop] + "\n"
if "tube_extra_read_ahead_seconds(" not in result:
    raise RuntimeError("The video buffer preference is not applied to the read-ahead")
Path(sys.argv[2]).write_text(result)
