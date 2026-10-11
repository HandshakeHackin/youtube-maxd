"""Extract the shipping section-menu, refresh, results-level and startup blocks."""
from pathlib import Path
import sys

root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
ui = (root / 'ui_rml/src/evo_rmlui_provider.cpp').read_text(encoding='utf-8')
host = (root / 'core/src/screens/ProviderHostScreen.cpp').read_text(encoding='utf-8')
for source, tag, name in ((ui, 'refresh section', 'refresh.inc'), (ui, 'search results level', 'results.inc'),
                          (ui, 'section menu', 'menu.inc'), (ui, 'startup folder', 'startup_folder.inc'),
                          (ui, 'startup fallback', 'fallback.inc'), (host, 'startup section', 'startup.inc')):
    start = source.index('// ProsperoTube ' + tag + '\n')
    end = source.index('// End ProsperoTube ' + tag + '\n', start)
    (output / name).write_text(source[start:end], encoding='utf-8')
# The settings rule must keep the two real levels out of the in-place reload.
rule = ui[ui.index('id.rfind("settings:", 0) == 0 &&'):]
rule = rule[:rule.index('{')]
for level in ('settings:search', 'settings:sections'):
    if f'id != "{level}"' not in rule:
        raise SystemExit(level + ' would reload in place')
if 'm_search_depth = m_stack.size();' not in ui:
    raise SystemExit('typed results are not tracked')
if 'std::strncmp(parent_id, "description:", 12) == 0' not in ui or 'c.Bind("description", &m_model.description);' not in ui:
    raise SystemExit('description reader is not wired')
