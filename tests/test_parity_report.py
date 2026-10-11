"""The parity tracker covers the whole inventory and its rendered page is current."""
from pathlib import Path

import parity_report


def test_every_inventory_entry_has_a_status_and_the_page_is_current():
    areas = parity_report.load()
    assert set(areas) == set(parity_report.AREAS)
    page = Path(parity_report.ROOT / "docs/PARITY.md").read_text(encoding="utf-8")
    assert page == parity_report.render(areas)
