"""Accept WebP artwork through the existing PS5 FFmpeg decoder and disk cache."""


def apply_artwork_overlay(replace, player):
    path = player + 'ui_rml/src/evo_rmlui_provider_art.cpp'
    replace(path, '    return nullptr;   /* WebP and GIF land here: no decoder in this build. */', '''    if (len >= 12 && memcmp(bytes, "RIFF", 4) == 0 &&
        memcmp(bytes + 8, "WEBP", 4) == 0) return ".webp";
    return nullptr;''')
    replace(path, 'const char* const kArtExts[] = { ".png", ".jpg" };',
            'const char* const kArtExts[] = { ".png", ".jpg", ".webp" };')
    replace(path, ''' * sniff_ext(). A poster whose format has no decoder in this build (WebP, GIF)
 * is refused on arrival rather than written and failed later.''', ''' * sniff_ext(). PNG, JPEG and WebP use the enabled PS5 FFmpeg image decoders;
 * other formats are refused before writing a cache entry.''')
    replace(path, '    bool wrote = (put == len) && (fclose(f) == 0);',
            '    int close_result = fclose(f);\n    bool wrote = (put == len) && (close_result == 0);')
