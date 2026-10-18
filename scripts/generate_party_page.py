"""Regenerate the self-contained LAN page for the native HTTP server."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
source=root/'native/maxd/party_page.html'
output=root/'native/maxd/party_page.h'
output.write_text('/* Generated from party_page.html; scripts/generate_party_page.py. GPL-3.0-or-later. */\nstatic const char maxd_party_page[]=\n'+'\n'.join(json.dumps(line+'\n',ensure_ascii=True) for line in source.read_text().splitlines())+';\n')
