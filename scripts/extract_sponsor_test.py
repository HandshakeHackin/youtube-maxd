"""Extract the shipping PS5 player's SponsorBlock tick and input paths."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
destination = Path(sys.argv[2])
destination.mkdir(parents=True, exist_ok=True)
tick_start = source.index("    tube_sponsor_skip_observe(")
tick_end = source.index("    /*\n     * Ask the playback engine", tick_start)
input_start = source.index("    if (!m_showStatsForNerds && !playback->isScrubbing() &&")
input_end = source.index("    // Scrub hold update", input_start)
(destination / "tick.inc").write_text(source[tick_start:tick_end])
prefix = source.index('\n', source.index('bool PlayerScreen::handleInput(')) + 1
prefix_end = source.index('    (void)released;', prefix)
(destination / "input.inc").write_text(source[prefix:prefix_end] + source[input_start:input_end])
