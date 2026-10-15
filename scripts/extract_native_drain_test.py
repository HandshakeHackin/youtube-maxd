"""Compile exact generated native drain code without starting Sony services."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
destination = Path(sys.argv[2])
definitions = []
for name in ("EVO_VDEC_REORDER_DEPTH", "FRAME_POOL_SLOTS", "RO_SLOTS", "PIPELINE_BUFFER_COUNT", "PTS_POOL", "NWD_DECODE_MS"):
    matches = re.findall(r"^#define " + name + r"\s+([^\n]+)", source, re.M)
    if len(matches) != 1:
        raise RuntimeError(f"Expected one native constant: {name}")
    definitions.append(f"#define {name} {matches[0]}")
destination.joinpath("tube_native_drain_constants.inc").write_text("\n".join(definitions) + "\n")
functions = []
begin = "static int tube_native_surface_valid("
end = "static void ro_harvest("
assert source.count(begin) == 1 and source.count(end) == 1
destination.joinpath("tube_native_surface.inc").write_text(begin + source.split(begin, 1)[1].split(end, 1)[0])
harvest = source.split(end, 1)[1].split("/*\n * HEVC NAL", 1)[0]
assert harvest.index("tube_native_surface_valid") < harvest.index("blend_fields") < harvest.index("memcpy")
assert "n->fatal = 1;" in harvest
for begin, end in (
    ("static void drain_decoder(evo_vdec_native *n)", "/* (Re)create the Annex-B"),
    ("int evo_vdec_native_is_drained(const evo_vdec_native *v)\n{", "void evo_vdec_native_flush(evo_vdec_native *v)   /* seek */"),
):
    if source.count(begin) != 1 or source.count(end) != 1:
        raise RuntimeError("Native drain test source boundary mismatch")
    functions.append(begin + source.split(begin, 1)[1].split(end, 1)[0])
destination.joinpath("tube_native_drain_functions.inc").write_text("\n".join(functions))
