"""Generate Java-style simple lower(upper(codepoint)) mappings from pinned UCD."""
import hashlib
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_bytes()
digest = '806e9aed65037197f1ec85e12be6e8cd870fc5608b4de0fffd990f689f376a73'
if hashlib.sha256(source).hexdigest() != digest:
    raise SystemExit('Expected Unicode 15.0.0 UnicodeData.txt')
upper, lower = {}, {}
for line in source.decode('utf-8').splitlines():
    fields = line.split(';')
    point = int(fields[0], 16)
    if fields[12]: upper[point] = int(fields[12], 16)
    if fields[13]: lower[point] = int(fields[13], 16)
rows = []
for point in sorted(upper.keys() | lower.keys()):
    mapped = upper.get(point, point)
    mapped = lower.get(mapped, mapped)
    if point != mapped: rows.append(f'{{0x{point:x},0x{mapped:x}}}')
output = ('/* Generated Unicode 15.0.0 simple lower(upper(cp)); see licenses/Unicode-3.0.txt.\n'
          f' * UnicodeData.txt SHA256: {digest} */\n'
          'static const uint32_t tube_name_case[][2] = {\n' +
          ''.join('    ' + ','.join(rows[i:i+6]) + ',\n' for i in range(0, len(rows), 6)) + '};\n')
Path(sys.argv[2]).write_text(output, encoding='utf-8')
