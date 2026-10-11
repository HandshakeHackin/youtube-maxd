"""Extract shipping scrub methods, enums and FSM configuration for host execution."""
from pathlib import Path
import sys


def block(source, signature):
    if source.count(signature) != 1:
        raise RuntimeError('Scrub extraction mismatch: ' + signature)
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        if source[end] == '{': depth += 1
        elif source[end] == '}': depth -= 1
        end += 1
    return source[start:end] + '\n'


root, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
source = (root / 'core/src/services/PlaybackController.cpp').read_text()
signatures = ('void PlaybackController::initStateMachine()',
              'double PlaybackController::getSeekIntervalSeconds() const',
              'bool PlaybackController::isPaused() const',
              'bool PlaybackController::isScrubbing() const',
              'double PlaybackController::getPositionSeconds() const',
              'double PlaybackController::getPlaybackPositionSeconds() const',
              'void PlaybackController::saveResumePosition()',
              'void PlaybackController::setPaused(bool paused)',
              'void PlaybackController::beginScrub()',
              'void PlaybackController::moveScrub(double deltaSeconds)',
              'bool PlaybackController::confirmScrub()',
              'void PlaybackController::cancelScrub()',
              'void PlaybackController::resetScrubHold()',
              'void PlaybackController::updateScrubHold(uint32_t heldButtons)',
              'void PlaybackController::tickScrubAutoCommit()',
              'bool PlaybackController::scrubNeedsConfirmation() const',
              'double PlaybackController::clampScrubTarget(double target) const')
(output / 'scrub.inc').write_text(''.join(block(source, s) for s in signatures), encoding='utf-8')
interface = (root / 'core/include/evo/interfaces/IPlaybackController.hpp').read_text()
(output / 'enums.inc').write_text(block(interface, 'enum class PlaybackState') + ';\n' +
                                block(interface, 'enum class PlaybackEvent') + ';\n', encoding='utf-8')
source = (root / 'core/src/screens/PlayerScreen.cpp').read_text()
(output / 'taps.inc').write_text(block(source, 'if (pressed & PadButtons::Left) {') +
                                block(source, 'if (pressed & PadButtons::Right) {'), encoding='utf-8')
start = source.index('// ProsperoTube seek input')
end = source.index('// End ProsperoTube seek input', start)
(output / 'input.inc').write_text(source[start:end], encoding='utf-8')
start = source.index('    const auto chatState = playback->getPlaybackState();')
end = source.index('    playback->tickScrubAutoCommit();', start)
(output / 'chat-clock.inc').write_text(source[start:end], encoding='utf-8')
source = (root / 'core/src/Application.cpp').read_text()
start = source.index('p->position = pb->getPlaybackPositionSeconds();')
end = source.index('\n', start)
(output / 'phone-clock.inc').write_text(source[start:end], encoding='utf-8')

for relative, tags, filename in (
    ('ui_kit/src/evo_hui_osd.cpp', ('Cross hint', 'Circle hint'), 'hui.inc'),
    ('ui_rml/src/evo_rmlui_app.cpp', ('RML hints',), 'rml.inc'),
    ('ui_rml/src/evo_rmlui_app.cpp', ('music hint',), 'music.inc'),
):
    source = (root / relative).read_text()
    fragments = []
    for tag in tags:
        start = source.index('// ProsperoTube seek ' + tag)
        end = source.index('// End ProsperoTube seek ' + tag, start)
        fragments.append(source[start:end])
    (output / filename).write_text('\n'.join(fragments), encoding='utf-8')
assert (root / 'assets/rml/playback.rml').read_text().count('id="label-back"') == 1
