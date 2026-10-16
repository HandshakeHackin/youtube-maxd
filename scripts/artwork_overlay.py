"""Accept WebP artwork through the existing PS5 FFmpeg decoder and disk cache."""

# A Classic card draws its picture in a fixed 16:9 box (native/youtube.rcss
# `.tube-image img`), so any other shape was stretched: the 4:3 sddefault and
# portrait oardefault/frame0 thumbnails of Subscriptions most of all. The decoder
# keeps every picture at 16:9 instead: a letterboxed 4:3 picture loses its bars,
# anything else is centred on a transparent 16:9 ground.
CARD_FRAME = r'''/* The 16:9 frame a Classic card shows a w x h picture in. */
struct TubeCardFrame { int width, height, src_y, src_h, dst_x, dst_y; };
static bool tube_card_frame(int w, int h, TubeCardFrame& f)
{
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384) return false;
    const long long wide = (long long)w * 9, tall = (long long)h * 16;   /* w/h against 16/9 */
    f = TubeCardFrame{ w, h, 0, h, 0, 0 };
    if (wide * 100 >= tall * 97 && wide * 100 <= tall * 103) return true;
    if (wide < tall && (long long)w * 4 >= (long long)h * 5) {
        f.src_h = f.height = (int)((long long)w * 9 / 16);              /* 4:3 down to 5:4: crop the bars */
        f.src_y = (h - f.src_h) / 2;
    } else if (wide < tall) {
        f.width = (int)(((long long)h * 16 + 8) / 9);                   /* portrait or square: pillarbox */
        f.dst_x = (f.width - w) / 2;
    } else {
        f.height = (int)(((long long)w * 9 + 8) / 16);                  /* wider than 16:9: letterbox */
        f.dst_y = (f.height - h) / 2;
    }
    return f.width > 0 && f.height > 0;
}

bool decode_image(const char* path, std::vector<uint32_t>& out, int& ow, int& oh)
{
    AVFormatContext* fmt = nullptr;'''

SCALED = r'''        int tw = frame->width, th = frame->height;
        TubeCardFrame plan;
        if (!tube_card_frame(tw, th, plan)) goto done;
        const int longest = plan.width > plan.height ? plan.width : plan.height;
        if (longest > kMaxDim) {   /* the framed card, not the source, stays under kMaxDim */
            double sc = (double)kMaxDim / (double)longest;
            tw = (int)(tw * sc + 0.5);
            th = (int)(th * sc + 0.5);
            if (tw < 1) tw = 1;
            if (th < 1) th = 1;
        }
        if (!tube_card_frame(tw, th, plan)) goto done;'''

CONVERTED = r'''        /* swscale's x86 converters write whole blocks of 8 pixels: a tight row
         * whose width is no multiple of 8 (the 202, 203 and 268-pixel portrait
         * thumbnails) runs past its end, and the last one past the buffer.
         * Convert into 64-byte rows, then copy the card frame out of them. */
        const int pitch = (tw * 4 + 63) & ~63;
        std::vector<uint32_t> rows((size_t)(pitch / 4) * (size_t)th, 0);
        uint8_t* dst[4] = { (uint8_t*)rows.data(), nullptr, nullptr, nullptr };
        int dst_ls[4] = { pitch, 0, 0, 0 };
        if (sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, dst_ls) != th) goto done;
        out.assign((size_t)plan.width * (size_t)plan.height, 0);
        for (int y = 0; y < plan.src_h; ++y)
            memcpy(out.data() + (size_t)(y + plan.dst_y) * (size_t)plan.width + (size_t)plan.dst_x,
                   rows.data() + (size_t)(y + plan.src_y) * (size_t)(pitch / 4), (size_t)tw * sizeof(uint32_t));'''


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
    replace(path, '''#else

bool decode_image(const char* path, std::vector<uint32_t>& out, int& ow, int& oh)
{
    AVFormatContext* fmt = nullptr;''', '#else\n\n' + CARD_FRAME)
    replace(path, '''        int tw = frame->width, th = frame->height;
        if (tw > kMaxDim || th > kMaxDim) {
            double sc = (double)kMaxDim / (double)(tw > th ? tw : th);
            tw = (int)(tw * sc);
            th = (int)(th * sc);
            if (tw < 1) tw = 1;
            if (th < 1) th = 1;
        }''', SCALED)
    replace(path, '''        out.assign((size_t)tw * (size_t)th, 0);
        uint8_t* dst[4] = { (uint8_t*)out.data(), nullptr, nullptr, nullptr };
        int dst_ls[4] = { tw * 4, 0, 0, 0 };
        sws_scale(sws, frame->data, frame->linesize, 0, frame->height, dst, dst_ls);''', CONVERTED)
    replace(path, '''        ow = tw;
        oh = th;
        ok = true;''', '''        ow = plan.width;
        oh = plan.height;
        ok = true;''')
