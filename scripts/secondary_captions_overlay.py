"""Extend pinned EVO with a separately parsed secondary external caption bank."""
def apply_secondary_captions_overlay(replace, player, work, root):
    engine = player + 'media/src/evo_subtitle.c'
    header = player + 'media/include/evo_subtitle.h'
    source = (work / engine).read_text()
    begin = source.index('static int prospero_subtitle_load_file(')
    end = source.index('\nint prospero_subtitle_load_for_media(', begin)
    original = source[begin:end]
    shared = original.replace('static int prospero_subtitle_load_file(\n    const char *path\n)',
        'static int tube_subtitle_read_file(const char *path, ProsperoSubtitleCue *cues, int *count, char *stored_path, size_t path_size)')
    shared = shared.replace('sizeof(prospero_subtitle_path)', 'path_size')
    shared = shared.replace('prospero_subtitle_count', '(*count)').replace('prospero_subtitle_cues', 'cues').replace('prospero_subtitle_path', 'stored_path')
    wrapper = '''static int prospero_subtitle_load_file(const char *path) {
    return tube_subtitle_read_file(path, prospero_subtitle_cues, &prospero_subtitle_count,
        prospero_subtitle_path, sizeof prospero_subtitle_path);
}
'''
    replace(engine, original, shared + wrapper)
    replace(engine, '#include <ctype.h>', '#include <ctype.h>\n#include <math.h>')
    replace(header, 'void prospero_subtitle_clear(void);', '''void prospero_subtitle_clear(void);
/* Independent external secondary bank; failed parsing preserves active cues. */
#define PROSPERO_SECONDARY_EXTERNAL_BANK (-3)
int tube_secondary_subtitle_load(const char *media_path);
void tube_secondary_subtitle_clear(void);''')
    replace(engine, '/* PROSPERO_SUBTITLE_CONTROLS_START */',
        (root / 'native/tube_secondary_srt.inc').read_text() + '\n/* PROSPERO_SUBTITLE_CONTROLS_START */')
    replace(engine, 'int prospero_secondary_subtitle_active(void) {', '''int prospero_secondary_subtitle_active(void) {
    if (prospero_secondary_use_external == 2) return tube_secondary_current();''')
    replace(engine, 'static void prospero_secondary_drop(void) {', 'static void prospero_secondary_drop(void) {\n    tube_secondary_subtitle_clear();')
    replace(engine, 'void prospero_embedded_subtitle_close(void) {', 'void prospero_embedded_subtitle_close(void) {\n    tube_secondary_subtitle_clear();')
    replace(engine, 'int prospero_secondary_subtitle_open(\n    AVFormatContext *format\n) {',
        'int prospero_secondary_subtitle_open(\n    AVFormatContext *format\n) {\n    tube_secondary_subtitle_clear();')
    replace(engine, '''    if (prospero_secondary_use_external) {
        const ProsperoSubtitleCue *cue =''', '''    if (prospero_secondary_use_external == 2)
        return tube_secondary_text_at(clock_seconds, output, output_size);
    if (prospero_secondary_use_external) {
        const ProsperoSubtitleCue *cue =''')
    replace(engine, '''        if (prospero_secondary_use_external) {
            prospero_secondary_drop();''', '''        if (prospero_secondary_use_external == 1) {
            prospero_secondary_drop();''')
