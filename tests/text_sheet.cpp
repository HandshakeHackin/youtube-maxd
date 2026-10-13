// ProsperoTube - Text engine: a contact sheet of video titles in every script, drawn from the atlas.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// usage: text_sheet <output folder> [scale=1] [stream]
// Draws boards of invented titles the way the interface would: the engine lays them out frame by
// frame within its glyph budget, and a CPU copy of the kit's glyph shader (one bilinear sample of
// the atlas page, 128 = the outline, the quad's range) fills the pixels. scale 2 is the 4K
// surface: the same layout units, twice the pixels. PNG files, one per board.

#include "text_host.hpp"
#include "tube_text.hpp"

#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace tube::text;

namespace
{

struct Color
{
    std::uint8_t r, g, b;
};

struct Canvas
{
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> rgb;

    Canvas(int width, int height, Color fill) : w(width), h(height), rgb(static_cast<std::size_t>(width) * height * 3)
    {
        for (std::size_t i = 0; i < rgb.size(); i += 3)
        {
            rgb[i] = fill.r;
            rgb[i + 1] = fill.g;
            rgb[i + 2] = fill.b;
        }
    }
    void blend(int x, int y, Color c, float alpha)
    {
        if (x < 0 || y < 0 || x >= w || y >= h || alpha <= 0.0f)
            return;
        std::uint8_t *p = &rgb[(static_cast<std::size_t>(y) * w + x) * 3];
        p[0] = static_cast<std::uint8_t>(p[0] + (c.r - p[0]) * alpha + 0.5f);
        p[1] = static_cast<std::uint8_t>(p[1] + (c.g - p[1]) * alpha + 0.5f);
        p[2] = static_cast<std::uint8_t>(p[2] + (c.b - p[2]) * alpha + 0.5f);
    }
    void fill(float x0, float y0, float x1, float y1, Color c, float alpha)
    {
        for (int y = static_cast<int>(y0); y < static_cast<int>(y1); ++y)
            for (int x = static_cast<int>(x0); x < static_cast<int>(x1); ++x)
                blend(x, y, c, alpha);
    }
};

void put32(std::vector<std::uint8_t> &out, std::uint32_t value)
{
    out.push_back(static_cast<std::uint8_t>(value >> 24));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value));
}

void chunk(std::vector<std::uint8_t> &out, const char *type, const std::vector<std::uint8_t> &data)
{
    put32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, static_cast<std::uint32_t>(crc32(0, out.data() + start, static_cast<uInt>(out.size() - start))));
}

bool write_png(const std::string &path, const Canvas &canvas)
{
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(canvas.h) * (canvas.w * 3 + 1));
    for (int y = 0; y < canvas.h; ++y)
    {
        raw.push_back(0);
        const std::uint8_t *row = &canvas.rgb[static_cast<std::size_t>(y) * canvas.w * 3];
        raw.insert(raw.end(), row, row + canvas.w * 3);
    }
    uLongf packed_size = compressBound(static_cast<uLong>(raw.size()));
    std::vector<std::uint8_t> packed(packed_size);
    if (compress2(packed.data(), &packed_size, raw.data(), static_cast<uLong>(raw.size()), 6) != Z_OK)
        return false;
    packed.resize(packed_size);
    std::vector<std::uint8_t> out = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<std::uint8_t> header;
    put32(header, static_cast<std::uint32_t>(canvas.w));
    put32(header, static_cast<std::uint32_t>(canvas.h));
    header.insert(header.end(), {8, 2, 0, 0, 0});
    chunk(out, "IHDR", header);
    chunk(out, "IDAT", packed);
    chunk(out, "IEND", {});
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (file == nullptr)
        return false;
    const bool ok = std::fwrite(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);
    return ok;
}

// The kit's glyph shader on the CPU: GL_LINEAR, clamp to edge, one sample at the pixel centre.
void draw(Canvas &canvas, const Context &context, const Layout &layout, float x, float y, float scale, Color color)
{
    const int page_size = context.page_size();
    for (const Quad &quad : layout.quads)
    {
        const std::uint8_t *page = context.page_pixels(quad.page);
        if (page == nullptr)
            continue;
        const float x0 = (x + quad.x0) * scale;
        const float y0 = (y + quad.y0) * scale;
        const float x1 = (x + quad.x1) * scale;
        const float y1 = (y + quad.y1) * scale;
        const float range = quad.range * scale; // in output pixels
        const auto texel = [&](int tx, int ty)
        {
            tx = std::clamp(tx, 0, page_size - 1);
            ty = std::clamp(ty, 0, page_size - 1);
            return static_cast<float>(page[static_cast<std::size_t>(ty) * page_size + tx]);
        };
        for (int py = static_cast<int>(std::floor(y0)); py < static_cast<int>(std::ceil(y1)); ++py)
        {
            const float cy = static_cast<float>(py) + 0.5f;
            if (cy < y0 || cy > y1)
                continue;
            for (int px = static_cast<int>(std::floor(x0)); px < static_cast<int>(std::ceil(x1)); ++px)
            {
                const float cx = static_cast<float>(px) + 0.5f;
                if (cx < x0 || cx > x1)
                    continue;
                const float u = quad.u0 + (quad.u1 - quad.u0) * (cx - x0) / (x1 - x0);
                const float v = quad.v0 + (quad.v1 - quad.v0) * (cy - y0) / (y1 - y0);
                const float tx = u * static_cast<float>(page_size) - 0.5f;
                const float ty = v * static_cast<float>(page_size) - 0.5f;
                const int ix = static_cast<int>(std::floor(tx));
                const int iy = static_cast<int>(std::floor(ty));
                const float fx = tx - static_cast<float>(ix);
                const float fy = ty - static_cast<float>(iy);
                const float top = texel(ix, iy) * (1.0f - fx) + texel(ix + 1, iy) * fx;
                const float bottom = texel(ix, iy + 1) * (1.0f - fx) + texel(ix + 1, iy + 1) * fx;
                const float sdf = (top * (1.0f - fy) + bottom * fy) / 255.0f;
                const float distance = (sdf - 0.5f) * 2.0f * range;
                canvas.blend(px, py, color, std::clamp(distance + 0.5f, 0.0f, 1.0f));
            }
        }
    }
}

struct Sample
{
    const char *label;
    const char *text;
};

// Invented titles: no real video, channel or person.
const Sample kSamples[] = {
    {"Japanese", "【完全攻略】初心者でも分かる！東京ラーメン食べ歩きガイド 2026年版（前編）"},
    {"Korean", "서울 야경 브이로그 | 한강에서 보내는 완벽한 주말 하루, 그리고 맛집 탐방"},
    {"Chinese (simplified)", "十分钟学会做正宗麻婆豆腐，简单又下饭的家常菜教程（附详细步骤）"},
    {"Chinese (traditional)", "台北夜市美食大挑戰：一千元可以吃到多少東西？結果讓人驚訝！"},
    {"Arabic", "أفضل عشر وجهات سياحية في العالم العربي لعام ٢٠٢٦ – الجزء الأول من السلسلة"},
    {"Hebrew", "המדריך המלא לטיול בצפון: עשרה מקומות שחייבים לבקר בהם לפני סוף הקיץ"},
    {"Hindi", "दिल्ली की सबसे स्वादिष्ट स्ट्रीट फूड यात्रा – पूरी जानकारी हिंदी में, क्या खाएँ और कहाँ"},
    {"Thai", "เที่ยวเชียงใหม่สามวันสองคืน งบไม่เกินห้าพันบาท กินอะไรดี พักที่ไหน น้ำตกที่สวยที่สุด"},
    {"Russian", "Как приготовить настоящий борщ: бабушкин рецепт шаг за шагом, ещё вкуснее на второй день"},
    {"Greek", "Ταξίδι στις Κυκλάδες: οι δέκα ωραιότερες παραλίες για το φετινό καλοκαίρι"},
    {"Vietnamese", "Hướng dẫn nấu phở bò Hà Nội chuẩn vị tại nhà – bí quyết nước dùng trong và ngọt thanh"},
    {"German", "Über den Alpenpaß mit dem Fahrrad: 1.200 Höhenmeter, Käsespätzle & große Gefühle"},
    {"Turkish, Polish", "İstanbul'da bir gün: Boğaz turu, balık ekmek • Zażółć gęślą jaźń, śnieżna łódź"},
    {"Tamil", "தமிழ்நாட்டின் சிறந்த உணவுகள் – ஒரு முழுமையான வழிகாட்டி, எங்கே சாப்பிடலாம்"},
    {"Bengali", "কলকাতার সেরা রাস্তার খাবার: দশটি জায়গা যা আপনাকে অবশ্যই ঘুরে দেখতে হবে"},
    {"Telugu", "హైదరాబాద్ బిర్యానీ ఇంట్లోనే ఎలా తయారు చేయాలి – పూర్తి వివరాలతో సులభమైన పద్ధతి"},
    {"Korean + English", "PS5 리뷰 – is it still worth it in 2026? 최종 결론과 추천 게임 TOP 5"},
    {"Arabic + Latin", "مراجعة هاتف Nova X 17 Pro بعد 30 يوم من الاستخدام: هل يستحق الشراء؟"},
    {"Latin + Arabic", "Top 10 أغاني عربية (Official Video) 2026 – full playlist, 45 min"},
    {"Hebrew + Latin", "שלום World! סרטון מספר 42 (HD) – הפרק האחרון של העונה"},
    {"Japanese + English", "日本語 and English mixed タイトル – Part 3 「最終回」 feat. ゲスト"},
    {"Persian", "آموزش آشپزی ایرانی: قورمه\U0000200Cسبزی خوشمزه در ۴۵ دقیقه، می\U0000200Cتوانید؟"},
    {"Emoji", "🔥 NEW 🔥 Epic fails compilation 😂😂 #shorts – wait for the end 👀"},
    {"Emoji sequences", "My family 👨\U0000200D👩\U0000200D👧\U0000200D👦 trip ✈\U0000FE0F to Japan 🇯🇵 – day 1\U0000FE0F⃣ ❤\U0000FE0F 👍🏽 100% ★★★★☆"},
    {"Marks", "Zalgo-free: é è ê ë ñ ö ű ỹ ṩ ǘ a̐ g̈ n̈ · Việt ngữ · ḍha ṛ ṝ"},
    {"Time code", "1:23:45 / 2:00:00 · 11:11 · 0:07"},
};

struct Item
{
    std::string text;
    Style style;
    Constraints constraints;
    float x = 0.0f;
    float y = 0.0f; // top of the block
    Color color{236, 238, 242};
    float box_w = 0.0f; // > 0: a frame of this width and box_h is drawn behind
    float box_h = 0.0f;
    int board = 0;
};

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <output folder> [scale=1] [stream]\n", argv[0]);
        return 2;
    }
    const std::string folder = argv[1];
    const float scale = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.0f;
    const bool stream = argc > 3 && std::strcmp(argv[3], "stream") == 0;
    if (!text_host::have_windows())
    {
        std::printf("SKIP text_sheet: the Windows fonts are not in %s\n", text_host::windows_folder().c_str());
        return 0;
    }
    Config config = text_host::stand_in(text_host::windows("segoeui.ttf"));
    config.faces.push_back({text_host::windows("segoeuib.ttf"), 0, FaceRole::latin, true});
    config.pixel_scale = scale;
    config.io = stream ? IoMode::stream : IoMode::automatic;
    std::string error;
    std::unique_ptr<Context> context = Context::create(config, &error);
    if (!context)
    {
        std::fprintf(stderr, "text_sheet: %s\n", error.c_str());
        return 1;
    }

    const float board_w = 1480.0f;
    const float board_h = 1040.0f;
    std::vector<Item> items;
    const Color dim{132, 140, 152};
    const Color accent{255, 122, 64};
    int board = 0;
    float y = 24.0f;
    const auto label = [&](const std::string &text, float at_x, float at_y)
    {
        Item item;
        item.text = text;
        item.style.size = 18.0f;
        item.style.tracking = 1.0f;
        item.x = at_x;
        item.y = at_y;
        item.color = dim;
        item.board = board;
        items.push_back(item);
    };
    for (const Sample &sample : kSamples)
    {
        if (y + 168.0f > board_h)
        {
            ++board;
            y = 24.0f;
        }
        label(sample.label, 24.0f, y);
        // The whole title on one line.
        Item whole;
        whole.text = sample.text;
        whole.style.size = 30.0f;
        whole.x = 24.0f;
        whole.y = y + 26.0f;
        whole.board = board;
        items.push_back(whole);
        // A card: two lines, then the mark.
        Item card;
        card.text = sample.text;
        card.style.size = 28.0f;
        card.style.weight = Weight::bold;
        card.constraints.max_width = 480.0f;
        card.constraints.max_lines = 2;
        card.constraints.ellipsis = Ellipsis::end;
        card.x = 24.0f;
        card.y = y + 72.0f;
        card.box_w = 480.0f;
        card.box_h = 78.0f;
        card.board = board;
        items.push_back(card);
        // A list row: one line, cut at the end.
        Item row = card;
        row.style.size = 24.0f;
        row.style.weight = Weight::regular;
        row.constraints.max_width = 400.0f;
        row.constraints.max_lines = 1;
        row.x = 540.0f;
        row.box_w = 400.0f;
        row.box_h = 34.0f;
        items.push_back(row);
        // The same row cut at the start, and one centred.
        Item tail = row;
        tail.constraints.ellipsis = Ellipsis::start;
        tail.y = y + 112.0f;
        items.push_back(tail);
        Item wrapped = card;
        wrapped.style.size = 20.0f;
        wrapped.style.weight = Weight::regular;
        wrapped.constraints.max_width = 440.0f;
        wrapped.constraints.max_lines = 3;
        wrapped.constraints.align = Align::center;
        wrapped.x = 980.0f;
        wrapped.box_w = 440.0f;
        wrapped.box_h = 82.0f;
        items.push_back(wrapped);
        y += 168.0f;
    }

    // A board of large sizes, weights, alignment and digits.
    ++board;
    y = 24.0f;
    const auto big = [&](const char *name, const char *text, float size, Weight weight)
    {
        label(name, 24.0f, y);
        Item item;
        item.text = text;
        item.style.size = size;
        item.style.weight = weight;
        item.x = 24.0f;
        item.y = y + 24.0f;
        item.board = board;
        items.push_back(item);
        y += size * 1.3f + 22.0f;
    };
    big("Hero, 96", "Afterglow 東京の夜 서울", 96.0f, Weight::regular);
    big("Hero, 96, bold (the Latin face has a bold cut; the others are widened)", "Afterglow 東京の夜 서울", 96.0f, Weight::bold);
    big("Heading, 72", "مرحبا بالعالم · שלום עולם", 72.0f, Weight::regular);
    big("Heading, 72", "नमस्ते दुनिया · สวัสดีชาวโลก ที่นี่", 72.0f, Weight::regular);
    big("Heading, 56, bold", "Ελληνικά · Русский · Tiếng Việt · Größe", 56.0f, Weight::bold);
    {
        label("Clock: proportional, then tabular digits (each pair of lines must end at the same place when tabular)", 24.0f, y);
        const char *clocks[] = {"11:11:11 / 1:11:11", "00:00:00 / 4:44:44"};
        for (int i = 0; i < 2; ++i)
            for (int tabular = 0; tabular < 2; ++tabular)
            {
                Item item;
                item.text = clocks[i];
                item.style.size = 40.0f;
                item.style.tabular_digits = tabular != 0;
                item.x = 24.0f + static_cast<float>(tabular) * 520.0f;
                item.y = y + 24.0f + static_cast<float>(i) * 50.0f;
                item.board = board;
                item.color = tabular != 0 ? accent : Color{236, 238, 242};
                items.push_back(item);
            }
        y += 140.0f;
    }
    {
        label("Paragraph direction: start-aligned in one 700 wide box; tracking 4 on Latin, none inside Arabic", 24.0f, y);
        const char *lines[] = {"English paragraph with עברית inside it.", "פסקה בעברית עם English בתוכה.",
                               "فقرة عربية فيها English و 123 رقم.", "TRACKED CAPS عربي متصل"};
        for (int i = 0; i < 4; ++i)
        {
            Item item;
            item.text = lines[i];
            item.style.size = 30.0f;
            item.style.tracking = i == 3 ? 4.0f : 0.0f;
            item.constraints.max_width = 700.0f;
            item.x = 24.0f;
            item.y = y + 24.0f + static_cast<float>(i) * 42.0f;
            item.box_w = 700.0f;
            item.box_h = 40.0f;
            item.board = board;
            items.push_back(item);
        }
    }
    const int boards = board + 1;

    // Frames: lay everything out until no glyph is left waiting for the budget.
    std::vector<Layout> layouts(items.size());
    int frames = 0;
    const auto start = std::chrono::steady_clock::now();
    std::uint32_t pending = 1;
    std::uint32_t missing = 0;
    while (pending != 0 && frames < 10000)
    {
        context->advance_frame();
        pending = 0;
        missing = 0;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            context->layout(items[i].text, items[i].style, items[i].constraints, &layouts[i]);
            pending += layouts[i].pending;
            missing += layouts[i].missing;
        }
        ++frames;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    const Color background{16, 18, 22};
    const Color frame_color{255, 255, 255};
    for (int b = 0; b < boards; ++b)
    {
        Canvas canvas(static_cast<int>(board_w * scale), static_cast<int>(board_h * scale), background);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const Item &item = items[i];
            if (item.board != b)
                continue;
            if (item.box_w > 0.0f)
                canvas.fill(item.x * scale, item.y * scale, (item.x + item.box_w) * scale, (item.y + item.box_h) * scale,
                            frame_color, 0.06f);
            draw(canvas, *context, layouts[i], item.x, item.y, scale, item.color);
        }
        char name[64];
        std::snprintf(name, sizeof(name), "/titles-%s-%d.png", scale >= 1.5f ? "4k" : "1080p", b + 1);
        if (!write_png(folder + name, canvas))
        {
            std::fprintf(stderr, "text_sheet: cannot write %s%s\n", folder.c_str(), name);
            return 1;
        }
    }
    // The atlas pages themselves.
    for (int page = 0; page < context->page_count(); ++page)
    {
        const int size = context->page_size();
        Canvas canvas(size, size, Color{0, 0, 0});
        const std::uint8_t *pixels = context->page_pixels(page);
        for (int i = 0; i < size * size; ++i)
            canvas.rgb[static_cast<std::size_t>(i) * 3] = canvas.rgb[static_cast<std::size_t>(i) * 3 + 1] =
                canvas.rgb[static_cast<std::size_t>(i) * 3 + 2] = pixels[i];
        char name[64];
        std::snprintf(name, sizeof(name), "/atlas-%s-%d.png", scale >= 1.5f ? "4k" : "1080p", page + 1);
        write_png(folder + name, canvas);
    }

    const Stats stats = context->stats();
    std::printf("text_sheet scale %.0f%s: %d boards, %zu texts, %d frames of %d glyphs at most, %.0f ms in all\n", scale,
                stream ? " (stream)" : "", boards, items.size(), frames, config.glyphs_per_frame, seconds * 1000.0);
    std::printf("  glyphs %u in %u page(s), rasterised %llu in %.1f ms (%.3f ms each), shaped %llu texts in %.1f ms\n",
                stats.glyphs, stats.pages, static_cast<unsigned long long>(stats.rasterised), stats.raster_seconds * 1000.0,
                stats.rasterised != 0 ? stats.raster_seconds * 1000.0 / static_cast<double>(stats.rasterised) : 0.0,
                static_cast<unsigned long long>(stats.layout_misses), stats.shape_seconds * 1000.0);
    std::printf("  fonts opened %u in %.1f ms, mapped %llu bytes, read %llu bytes; clusters without a glyph: %u\n",
                stats.faces_open, stats.font_open_seconds * 1000.0, static_cast<unsigned long long>(stats.font_bytes_mapped),
                static_cast<unsigned long long>(stats.font_bytes_read), missing);
    std::printf("  faces: %s\n", context->describe().c_str());
    for (const std::string &message : context->messages())
        std::printf("  %s\n", message.c_str());
    return 0;
}
