// ProsperoTube Afterglow - the host harness: the interface on the PC, as pictures and checks.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Runs the frame with a FixtureSource on the kit's own OpenGL renderer under
// Mesa's surfaceless EGL (llvmpipe), with a fixed 60 Hz clock. Every board of
// tests/afterglow/boards/*.json is brought up from its fixtures, driven by its
// steps, checked, and written as a PNG per look. Nothing here reaches the
// network or a console. scripts/afterglow_host.sh builds and runs it.
//
// usage: afterglow_host <kit dir> <fixtures dir> <boards dir> <output dir> [options]
//   --look <id|all>      afterglow (default), onyx, daybreak, contrast, all
//   --lang <tag>         interface language: a catalog of --catalogs (default en)
//   --catalogs <dir>     native/i18n
//   --size <W>x<H>       picture size (default 1920x1080)
//   --only <text>        boards whose name contains the text
//   --no-pictures        check only
//   --baked              the kit's baked fonts instead of the text engine
//   --reel <file.mp4>    the boards marked "reel", recorded through ffmpeg
//   --sound <file.wav>   stage S: what the interface's port would play over the whole run, to listen to

#include "afterglow/data_fixture.hpp"
#include "afterglow/frame.hpp"
#include "afterglow/sound/recorder.hpp" // stage S
#include "afterglow/text_engine.hpp"
#include "art.hpp"
#include "text_host.hpp"

#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"

#include "cJSON.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace
{

using namespace afterglow;

bool open_context()
{
    auto get_platform_display =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDisplay display = get_platform_display != nullptr
                             ? get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr)
                             : eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0;
    EGLint minor = 0;
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor) || !eglBindAPI(EGL_OPENGL_API))
        return false;
    const EGLint attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
                                 EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    return context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}

bool read_file(const std::string &path, std::string *out)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    char buffer[65536];
    std::size_t got = 0;
    out->clear();
    while ((got = std::fread(buffer, 1, sizeof buffer, file)) > 0)
        out->append(buffer, got);
    std::fclose(file);
    return true;
}

std::string text_of(const cJSON *object, const char *name, const char *fallback = "")
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring != nullptr ? item->valuestring : fallback;
}

double number_of(const cJSON *object, const char *name, double fallback)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

// The interface on the PC: GL textures, pictures painted from arithmetic,
// catalogs read from native/i18n.
class HostPlatform final : public Platform
{
  public:
    HostPlatform(hui::gfx::Renderer &renderer, std::string fixtures) : renderer_(renderer), fixtures_(std::move(fixtures)) {}

    bool load_language(const std::string &directory, const std::string &tag)
    {
        strings_.clear();
        ++language_;
        if (tag.empty() || tag == "en")
            return true;
        std::string json;
        if (!read_file(directory + "/" + tag + ".json", &json))
            return false;
        cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
        const cJSON *item = nullptr;
        cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(root, "strings"))
        {
            if (cJSON_IsString(item) && item->string != nullptr)
                strings_[item->string] = item->valuestring;
        }
        cJSON_Delete(root);
        return !strings_.empty();
    }
    // Interface texts asked for that the catalog of the language lacks.
    const std::vector<std::string> &untranslated() const
    {
        return untranslated_;
    }
    void set_clock(int hour, int minute)
    {
        hour_ = hour;
        minute_ = minute;
    }
    void pump()
    {
        for (auto &entry : fetches_)
            --entry.pumps_left;
        while (!fetches_.empty() && fetches_.front().pumps_left <= 0)
        {
            Fetch entry = std::move(fetches_.front());
            fetches_.erase(fetches_.begin());
            std::string bytes;
            const bool ok = picture_bytes(entry.url, &bytes);
            entry.done(ok, std::move(bytes));
        }
    }

    std::uint32_t create_texture(int width, int height, const std::uint8_t *rgba) override
    {
        ++textures_;
        return renderer_.batch().create_texture(width, height, rgba);
    }
    void release_texture(std::uint32_t handle) override
    {
        const GLuint name = handle;
        glDeleteTextures(1, &name);
        --textures_;
    }
    std::uint32_t create_glyph_page(int size, const std::uint8_t *coverage) override
    {
        GLuint name = 0;
        glGenTextures(1, &name);
        glBindTexture(GL_TEXTURE_2D, name);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, coverage);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        return name;
    }
    void update_glyph_page(std::uint32_t handle, int size, const std::uint8_t *coverage) override
    {
        glBindTexture(GL_TEXTURE_2D, handle);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RED, GL_UNSIGNED_BYTE, coverage);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    void release_glyph_page(std::uint32_t handle, bool coverage) override
    {
        (void)coverage;
        const GLuint name = handle;
        glDeleteTextures(1, &name);
    }
    unsigned fetch(const std::string &url, Fetched done) override
    {
        fetches_.push_back({next_ticket_, 2, url, std::move(done)});
        return next_ticket_++;
    }
    void cancel_fetch(unsigned ticket) override
    {
        fetches_.erase(std::remove_if(fetches_.begin(), fetches_.end(), [&](const Fetch &f) { return f.ticket == ticket; }),
                       fetches_.end());
    }
    bool read_asset(const std::string &path, std::string *bytes) override
    {
        return read_file(fixtures_ + "/assets/" + path, bytes);
    }
    void wall_clock(int *hour, int *minute) override
    {
        *hour = hour_;
        *minute = minute_;
    }
    const char *translate(const char *english) override
    {
        if (strings_.empty())
            return english;
        const auto found = strings_.find(english);
        if (found != strings_.end())
            return found->second.c_str();
        if (std::find(untranslated_.begin(), untranslated_.end(), english) == untranslated_.end())
            untranslated_.emplace_back(english);
        return english;
    }
    unsigned language_revision() override
    {
        return language_;
    }
    void log(const char *line) override
    {
        std::fprintf(stderr, "%s\n", line);
    }
    int textures() const
    {
        return textures_;
    }

  private:
    struct Fetch
    {
        unsigned ticket;
        int pumps_left;
        std::string url;
        Fetched done;
    };

    // "art://<scene>/<palette>/<seed>[/<width>x<height>]" is painted here; "file://<name>" is read
    // from the fixtures' pictures folder; "broken://" fails, as a dead address does.
    bool picture_bytes(const std::string &url, std::string *bytes)
    {
        if (url.rfind("file://", 0) == 0)
            return read_file(fixtures_ + "/pictures/" + url.substr(7), bytes);
        if (url.rfind("probe://", 0) == 0)
        {
            // A picture whose orientation can be read back: the top half red, the bottom half blue,
            // with a green left edge.
            constexpr int kW = 320;
            constexpr int kH = 180;
            std::vector<std::uint8_t> probe(static_cast<std::size_t>(kW * kH * 4));
            for (int y = 0; y < kH; ++y)
                for (int x = 0; x < kW; ++x)
                {
                    std::uint8_t *p = &probe[static_cast<std::size_t>(y * kW + x) * 4u];
                    p[0] = x >= 40 && y < kH / 2 ? 255 : 0;
                    p[1] = x < 40 ? 255 : 0;
                    p[2] = x >= 40 && y >= kH / 2 ? 255 : 0;
                    p[3] = 255;
                }
            stbi_write_png_to_func([](void *context, void *data, int size)
                                   { static_cast<std::string *>(context)->append(static_cast<const char *>(data), static_cast<std::size_t>(size)); },
                                   bytes, kW, kH, 4, probe.data(), kW * 4);
            return !bytes->empty();
        }
        if (url.rfind("art://", 0) != 0)
            return false;
        char scene[24] = "";
        int palette = 0;
        unsigned seed = 0;
        int width = 640;
        int height = 360;
        if (std::sscanf(url.c_str() + 6, "%23[a-z]/%d/%u/%dx%d", scene, &palette, &seed, &width, &height) < 3)
            return false;
        width = std::clamp(width, 16, 1920);
        height = std::clamp(height, 16, 1080);
        const std::vector<std::uint8_t> rgba = afterglow_test::paint(scene, seed, palette, width, height);
        if (rgba.empty())
            return false;
        stbi_write_png_to_func([](void *context, void *data, int size)
                               { static_cast<std::string *>(context)->append(static_cast<const char *>(data), static_cast<std::size_t>(size)); },
                               bytes, width, height, 4, rgba.data(), width * 4);
        return !bytes->empty();
    }

    hui::gfx::Renderer &renderer_;
    std::string fixtures_;
    std::map<std::string, std::string> strings_;
    std::vector<std::string> untranslated_;
    std::vector<Fetch> fetches_;
    unsigned next_ticket_ = 1;
    unsigned language_ = 1;
    int hour_ = 21;
    int minute_ = 47;
    int textures_ = 0;
};

struct Options
{
    std::string kit, fixtures, boards, output, catalogs, lang = "en", only, reel;
    std::vector<std::string> looks{"afterglow"};
    int width = 1920;
    int height = 1080;
    bool pictures = true;
    bool baked = false;
    std::string sound; // stage S: --sound
};

struct Results
{
    int boards = 0;
    int pictures = 0;
    int checks = 0;
    int failures = 0;
};

void fail(Results &results, const std::string &board, const std::string &look, const std::string &what)
{
    ++results.failures;
    std::fprintf(stderr, "FAIL %s [%s]: %s\n", board.c_str(), look.c_str(), what.c_str());
}

std::uint32_t action_of(const std::string &name, Direction *direction)
{
    static const struct
    {
        const char *name;
        Action action;
    } kActions[] = {{"cross", Action::confirm}, {"circle", Action::back}, {"triangle", Action::north},
                    {"square", Action::west},   {"l1", Action::page_prev}, {"r1", Action::page_next},
                    {"l2", Action::jump_prev},  {"r2", Action::jump_next}, {"options", Action::menu},
                    {"touchpad", Action::touch}, {"l3", Action::l3},       {"r3", Action::r3}};
    *direction = name == "up" ? Direction::up : name == "down" ? Direction::down : name == "left" ? Direction::left : name == "right" ? Direction::right : Direction::none;
    for (const auto &entry : kActions)
    {
        if (name == entry.name)
            return hui::action_bit(entry.action);
    }
    return 0;
}

// The looks hold every text colour to 4.5:1 on the grounds it is drawn on.
void check_look(const Look &look, Results &results)
{
    const struct
    {
        const char *name;
        Color ink;
        Color ground;
    } pairs[] = {
        {"ink on base", look.ink, look.base},          {"ink on base2", look.ink, look.base2},
        {"ink2 on base", look.ink2, look.base},        {"ink2 on base2", look.ink2, look.base2},
        {"ink3 on base", look.ink3, look.base},        {"ink3 on base2", look.ink3, look.base2},
        {"ink on panel", look.ink, flatten(look.panel_solid, look.base)},
        {"ink2 on panel", look.ink2, flatten(look.panel_solid, look.base)},
        {"on_plate on plate", look.on_plate, look.plate}, {"on_accent on accent", look.on_accent, look.accent},
    };
    for (const auto &pair : pairs)
    {
        ++results.checks;
        const float ratio = contrast_ratio(pair.ink, pair.ground);
        if (ratio < 4.5f)
        {
            char line[160];
            std::snprintf(line, sizeof line, "%s is %.2f:1, below 4.5:1", pair.name, ratio);
            fail(results, "look", look.id, line);
        }
    }
}

// Structural checks that hold for every board: the focus is on screen and has
// a size, no text leaves the screen unclipped, the page is not blank.
void check_frame(const Frame &frame, const gfx::DrawList &world, const gfx::DrawList &ui, const std::string &board,
                 const std::string &look, Results &results)
{
    const FocusInfo focus = frame.focus();
    ++results.checks;
    if (focus.id.empty())
        fail(results, board, look, "nothing has the focus");
    ++results.checks;
    if (focus.rect.w > 0.0f || focus.rect.h > 0.0f)
    {
        if (focus.rect.x < -0.5f || focus.rect.y < -0.5f || focus.rect.x + focus.rect.w > gfx::kVirtualWidth + 0.5f ||
            focus.rect.y + focus.rect.h > gfx::kVirtualHeight + 0.5f)
        {
            char line[200];
            std::snprintf(line, sizeof line, "the focus '%s' is off the screen: %.0f,%.0f %.0fx%.0f", focus.id.c_str(),
                          focus.rect.x, focus.rect.y, focus.rect.w, focus.rect.h);
            fail(results, board, look, line);
        }
    }
    int stray = 0;
    char where[96] = "";
    for (const gfx::DrawList *list : {&world, &ui})
    {
        for (const gfx::Run &run : list->runs())
        {
            if (run.mesh)
                continue;
            for (std::uint32_t i = run.first; i < run.first + run.count; ++i)
            {
                const gfx::Instance &instance = list->instances()[i];
                if (static_cast<int>(instance.params[3]) != static_cast<int>(gfx::Shape::glyph) || instance.color_top[3] < 0.05f)
                    continue;
                float x0 = instance.rect[0];
                float y0 = instance.rect[1];
                float x1 = x0 + instance.rect[2];
                float y1 = y0 + instance.rect[3];
                if (run.clipped)
                {
                    // What a clip rectangle hides is cut on purpose (a scrolling row, a marquee).
                    x0 = std::max(x0, run.clip.x);
                    y0 = std::max(y0, run.clip.y);
                    x1 = std::min(x1, run.clip.x + run.clip.w);
                    y1 = std::min(y1, run.clip.y + run.clip.h);
                    if (x1 <= x0 || y1 <= y0)
                        continue;
                }
                // A glyph's quad carries a few pixels of distance field around the letter.
                if (x1 < 6.0f || y1 < 6.0f || x0 > gfx::kVirtualWidth - 6.0f || y0 > gfx::kVirtualHeight - 6.0f)
                    continue; // wholly outside: a screen on its way in or out
                if (x0 < -6.0f || y0 < -6.0f || x1 > gfx::kVirtualWidth + 6.0f || y1 > gfx::kVirtualHeight + 6.0f)
                {
                    if (stray++ == 0)
                        std::snprintf(where, sizeof where, " (the first at %.0f,%.0f, %.0f x %.0f)", x0, y0, x1 - x0, y1 - y0);
                }
            }
        }
    }
    ++results.checks;
    if (stray > 0)
        fail(results, board, look, std::to_string(stray) + " glyphs cross the edge of the screen" + where);
    ++results.checks;
    if (world.instances().size() + ui.instances().size() < 8)
        fail(results, board, look, "the page is almost empty");
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: afterglow_host <kit dir> <fixtures dir> <boards dir> <output dir> [options]\n");
        return 2;
    }
    Options options;
    options.kit = argv[1];
    options.fixtures = argv[2];
    options.boards = argv[3];
    options.output = argv[4];
    for (int i = 5; i < argc; ++i)
    {
        const std::string flag = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : "";
        if (flag == "--look")
        {
            options.looks = std::string(value) == "all" ? std::vector<std::string>{"afterglow", "onyx", "daybreak", "contrast"}
                                                        : std::vector<std::string>{value};
            ++i;
        }
        else if (flag == "--lang")
            options.lang = value, ++i;
        else if (flag == "--catalogs")
            options.catalogs = value, ++i;
        else if (flag == "--only")
            options.only = value, ++i;
        else if (flag == "--reel")
            options.reel = value, ++i;
        else if (flag == "--size")
            std::sscanf(value, "%dx%d", &options.width, &options.height), ++i;
        else if (flag == "--no-pictures")
            options.pictures = false;
        else if (flag == "--baked")
            options.baked = true;
        else if (flag == "--sound") // stage S
            options.sound = value, ++i;
        else
        {
            std::fprintf(stderr, "unknown option %s\n", flag.c_str());
            return 2;
        }
    }
    if (!open_context())
    {
        std::fprintf(stderr, "no surfaceless EGL OpenGL 4.5 context\n");
        return 1;
    }
    hui::gfx::set_glsl_prefix("#version 450 core\n");
    hui::gfx::Renderer renderer;
    hui::gfx::Font regular, semibold, display, mono;
    hui::ui::Fonts fonts;
    const auto load_font = [&](const char *name, hui::gfx::Font *font, hui::ui::FontRef *ref)
    {
        std::string data;
        if (!read_file(options.kit + "/assets/fonts/" + name, &data) || !font->load(data))
        {
            std::fprintf(stderr, "font %s: %s\n", name, font->error().c_str());
            return false;
        }
        ref->font = font;
        ref->texture = renderer.batch().create_font_texture(*font);
        return true;
    };
    if (!renderer.init() || !load_font("inter-regular.huifont", &regular, &fonts.regular) ||
        !load_font("inter-semibold.huifont", &semibold, &fonts.semibold) ||
        !load_font("montserrat-medium.huifont", &display, &fonts.display) ||
        !load_font("dejavu-sans-mono.huifont", &mono, &fonts.mono))
    {
        std::fprintf(stderr, "renderer or fonts failed\n");
        return 1;
    }
    fonts.pixel = fonts.mono;
    fonts.hand = fonts.regular;

    GLuint framebuffer = 0;
    GLuint color = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, options.width, options.height);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return 1;
    mkdir(options.output.c_str(), 0755);

    HostPlatform platform(renderer, options.fixtures);
    if (!platform.load_language(options.catalogs, options.lang))
    {
        std::fprintf(stderr, "no catalog for language %s under %s\n", options.lang.c_str(), options.catalogs.c_str());
        return 1;
    }

    // The text engine behind the kit's fonts: the faces of the approved boards for Latin, Greek and
    // Cyrillic, and stand-ins for the console's system fonts for every other script, where this PC has them.
    std::unique_ptr<TextEngine> engine;
    const bool stand_ins = text_host::have_windows();
    if (!options.baked)
    {
        TextSetup setup;
        setup.regular = options.kit + "/third_party/fonts/Inter-Regular.ttf";
        setup.semibold = options.kit + "/third_party/fonts/Inter-SemiBold.ttf";
        setup.display = options.kit + "/third_party/fonts/Montserrat-Medium.ttf";
        setup.mono = options.kit + "/third_party/fonts/DejaVuSansMono.ttf";
        if (stand_ins)
        {
            const tube::text::Config standing = text_host::stand_in(setup.regular);
            setup.faces.assign(standing.faces.begin() + 1, standing.faces.end());
        }
        setup.language = options.lang;
        setup.pixel_scale = static_cast<float>(options.height) / gfx::kVirtualHeight;
        std::string why;
        engine = TextEngine::create(platform, setup, &why);
        if (!engine)
        {
            std::fprintf(stderr, "the text engine did not start: %s\n", why.c_str());
            return 1;
        }
        engine->bind(fonts);
    }
    std::uint32_t most_missing = 0;

    // Every board file, in name order.
    std::vector<std::string> files;
    if (DIR *directory = opendir(options.boards.c_str()))
    {
        while (const dirent *entry = readdir(directory))
        {
            const std::string name = entry->d_name;
            if (name.size() > 5 && name.compare(name.size() - 5, 5, ".json") == 0)
                files.push_back(name);
        }
        closedir(directory);
    }
    std::sort(files.begin(), files.end());

    Results results;
    for (const std::string &id : options.looks)
        check_look(look_by_id(id), results);

    // ---- stage S: sound and vibration ------------------------------------------------------------
    // The recordings come from the pinned kit's sound sets, as the package takes them. Every board runs a
    // Sound with a Recorder: what was heard and felt is written down, never played, and a board may expect it
    // ("cues": the names heard in order, "pulses": how many vibration pulses). A Tape renders the mixer as the
    // console's port would, into --sound's file when one is named.
    sound::Bank recordings;
    {
        const sound::Bank::Report loaded = recordings.load([&](const std::string &file, std::string *bytes)
                                                           { return read_file(options.kit + "/assets/audio/sfx/" + file, bytes); });
        for (const std::string &error : loaded.errors)
            std::fprintf(stderr, "sound: %s\n", error.c_str());
        ++results.checks;
        if (loaded.silent != 0 || !loaded.errors.empty())
            fail(results, "sound", "-", std::to_string(loaded.silent) + " cues of the interface have no recording in the kit");
    }
    sound::Tape tape;
    if (!options.sound.empty() && !tape.open(options.sound))
    {
        std::fprintf(stderr, "cannot write %s\n", options.sound.c_str());
        return 1;
    }
    // ---- end of stage S ---------------------------------------------------------------------------

    constexpr float kDt = 1.0f / 60.0f;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(options.width) * static_cast<std::size_t>(options.height) * 4u);
    std::vector<unsigned char> row(static_cast<std::size_t>(options.width) * 4u);
    std::FILE *reel = nullptr;
    if (!options.reel.empty())
    {
        char command[512];
        std::snprintf(command, sizeof command,
                      "ffmpeg -v error -y -f rawvideo -pix_fmt rgba -s %dx%d -r 60 -i - -c:v libx264 -preset veryfast -crf 18 "
                      "-pix_fmt yuv420p '%s'",
                      options.width, options.height, options.reel.c_str());
        reel = popen(command, "w");
    }

    for (const std::string &file : files)
    {
        std::string json;
        if (!read_file(options.boards + "/" + file, &json))
            continue;
        cJSON *root = cJSON_ParseWithLength(json.data(), json.size());
        if (root == nullptr)
        {
            fail(results, file, "-", "the board file is not JSON");
            continue;
        }
        const cJSON *board = nullptr;
        cJSON_ArrayForEach(board, cJSON_GetObjectItemCaseSensitive(root, "boards"))
        {
            const std::string name = text_of(board, "name");
            if (name.empty() || (!options.only.empty() && name.find(options.only) == std::string::npos))
                continue;
            const bool in_reel = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(board, "reel"));
            if (reel != nullptr && !in_reel)
                continue;
            // A board of other scripts needs the engine and this PC's stand-in fonts.
            if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(board, "scripts")) && (!engine || !stand_ins))
            {
                std::fprintf(stderr, "skipped %s: no text engine or no stand-in fonts on this PC\n", name.c_str());
                continue;
            }
            ++results.boards;
            for (const std::string &look_id : options.looks)
            {
                const Look &look = look_by_id(look_id);
                int scheme = 0;
                for (int i = 0; i < kLookCount; ++i)
                    scheme = &look_for_scheme(i) == &look ? i : scheme;
                FixtureSource source(options.fixtures + "/" + text_of(board, "fixture", "guest"));
                const cJSON *behaviour = cJSON_GetObjectItemCaseSensitive(board, "source");
                source.set_delay(static_cast<int>(number_of(behaviour, "delay", 3)));
                source.set_hold(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(behaviour, "hold")));
                source.set_offline(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(behaviour, "offline")));
                FrameOptions frame_options;
                frame_options.scheme = scheme;
                frame_options.opening = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(board, "opening"));
                frame_options.reduced_motion = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(board, "reduced_motion"));
                frame_options.attract_after = static_cast<float>(number_of(board, "attract_after", kAttractAfter));
                Frame frame(platform, source, fonts, frame_options);
                const cJSON *route = cJSON_GetObjectItemCaseSensitive(board, "route");
                ScreenId screen = ScreenId::home;
                if (cJSON_IsObject(route) && screen_from_name(text_of(route, "screen", "home"), &screen))
                    frame.open(Route{screen, text_of(route, "argument"), text_of(route, "title")});

                // ---- stage S: this board's sound, with what its "sound" object sets of Settings > Interface > Sounds
                sound::Recorder heard;
                sound::Sound sounds(recordings, heard);
                const cJSON *sound_set = cJSON_GetObjectItemCaseSensitive(board, "sound");
                sound::Options sound_options;
                sound_options.reduced_motion = frame_options.reduced_motion;
                sound_options.sounds = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(sound_set, "sounds"));
                sound_options.vibration = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(sound_set, "vibration"));
                sound_options.volume = static_cast<int>(number_of(sound_set, "volume", sound_options.volume));
                sound_options.quiet_from = static_cast<int>(number_of(sound_set, "quiet_from", -1));
                sound_options.quiet_until = static_cast<int>(number_of(sound_set, "quiet_until", -1));
                sounds.set_options(sound_options);
                if (frame_options.opening)
                    sounds.opened();
                const auto hear = [&](bool held)
                {
                    int hour = 0;
                    int minute = 0;
                    platform.wall_clock(&hour, &minute);
                    sound::Step step;
                    step.dt = kDt;
                    step.held = held;
                    step.minute = static_cast<int>(number_of(sound_set, "minute", hour * 60 + minute));
                    step.situation.attract = frame.attracting();
                    sounds.step(frame.feedback(), step);
                    tape.run(sounds.mixer(), static_cast<int>(hui::audio::kSampleRate * kDt));
                };
                // ---- end of stage S

                gfx::DrawList world;
                gfx::DrawList ui;
                const auto present = [&](bool to_reel)
                {
                    world.clear();
                    ui.clear();
                    frame.draw(world, ui, renderer.glass_texture());
                    if (engine)
                    {
                        most_missing = std::max(most_missing, engine->missing());
                        engine->end_frame();
                    }
                    renderer.begin();
                    renderer.draw(world);
                    renderer.glass();
                    renderer.draw(ui);
                    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                    glClearColor(0, 0, 0, 1);
                    glClear(GL_COLOR_BUFFER_BIT);
                    renderer.present(framebuffer, options.width, options.height);
                    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                    glReadPixels(0, 0, options.width, options.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    // OpenGL hands the bottom row first. The capture is turned here, once, so that pixels[] is the
                    // picture as it is seen, top row first: for the PNG, the reel and the pixel checks alike. No
                    // global switch of the PNG writer is used: it would also turn the pictures painted for fixtures.
                    for (int y = 0; y < options.height / 2; ++y)
                    {
                        unsigned char *upper = &pixels[static_cast<std::size_t>(y) * row.size()];
                        unsigned char *lower = &pixels[static_cast<std::size_t>(options.height - 1 - y) * row.size()];
                        std::memcpy(row.data(), upper, row.size());
                        std::memcpy(upper, lower, row.size());
                        std::memcpy(lower, row.data(), row.size());
                    }
                    for (std::size_t i = 3; i < pixels.size(); i += 4)
                        pixels[i] = 255;
                    if (to_reel && reel != nullptr)
                        std::fwrite(pixels.data(), 1, pixels.size(), reel);
                };
                const auto run = [&](float seconds, InputFrame first)
                {
                    const int frames = std::max(1, static_cast<int>(std::lround(seconds / kDt)));
                    for (int i = 0; i < frames; ++i)
                    {
                        platform.pump();
                        if (engine)
                            engine->begin_frame();
                        frame.update(i == 0 ? first : InputFrame{}, kDt);
                        hear(i == 0 && first.nav_repeat); // stage S
                        if (reel != nullptr)
                        {
                            present(true);
                        }
                        else if (i % 4 == 3 || i + 1 == frames)
                        {
                            // The lists are built as the app builds them (pictures are asked for by drawing
                            // them), on every fourth frame: the boards wait whole seconds, the checks read
                            // the last frame, and the run takes a quarter of the time.
                            world.clear();
                            ui.clear();
                            frame.draw(world, ui, renderer.glass_texture());
                            if (engine)
                                engine->end_frame();
                        }
                        else if (engine)
                        {
                            engine->end_frame();
                        }
                    }
                };
                run(static_cast<float>(number_of(board, "settle", 1.5)), InputFrame{});
                const cJSON *step = nullptr;
                cJSON_ArrayForEach(step, cJSON_GetObjectItemCaseSensitive(board, "steps"))
                {
                    InputFrame input;
                    const std::string press = text_of(step, "press");
                    if (!press.empty())
                    {
                        Direction direction = Direction::none;
                        input.pressed = action_of(press, &direction);
                        input.nav = direction;
                        if (input.pressed == 0 && direction == Direction::none)
                            fail(results, name, look_id, "unknown button in a step: " + press);
                    }
                    run(static_cast<float>(number_of(step, "wait", press.empty() ? 0.5 : 0.35)), input);
                }
                // The last frame is drawn twice: glyphs first asked for in it are in the atlas for the second.
                for (int pass = 0; pass < 2; ++pass)
                {
                    if (engine)
                        engine->begin_frame();
                    present(false);
                }
                ++results.checks;
                if (engine && engine->missing() != 0 && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(board, "missing_ok")))
                    fail(results, name, look_id, std::to_string(engine->missing()) + " clusters of text have no glyph in any font");

                check_frame(frame, world, ui, name, look_id, results);
                const std::string snapshot = frame.snapshot_json();
                cJSON *state = cJSON_ParseWithLength(snapshot.data(), snapshot.size());
                ++results.checks;
                if (state == nullptr)
                    fail(results, name, look_id, "the snapshot is not JSON: " + snapshot.substr(0, 120));
                const cJSON *expect = cJSON_GetObjectItemCaseSensitive(board, "expect");
                const cJSON *wanted = nullptr;
                cJSON_ArrayForEach(wanted, expect)
                {
                    const std::string key = wanted->string != nullptr ? wanted->string : "";
                    ++results.checks;
                    if (key == "focus")
                    {
                        const std::string got = text_of(cJSON_GetObjectItemCaseSensitive(state, "focus"), "id");
                        if (got != wanted->valuestring)
                            fail(results, name, look_id, "focus is '" + got + "', expected '" + wanted->valuestring + "'");
                    }
                    else if (key == "state")
                    {
                        const cJSON *value = nullptr;
                        cJSON_ArrayForEach(value, wanted)
                        {
                            const std::string got = text_of(cJSON_GetObjectItemCaseSensitive(state, "state"), value->string, "(none)");
                            if (!cJSON_IsString(value) || got != value->valuestring)
                                fail(results, name, look_id, std::string("state ") + value->string + " is '" + got + "', expected '" +
                                                                 (cJSON_IsString(value) ? value->valuestring : "?") + "'");
                        }
                    }
                    else if (key == "played")
                    {
                        const std::string got = source.played().empty() ? std::string() : source.played().back();
                        if (got != wanted->valuestring)
                            fail(results, name, look_id, "played '" + got + "', expected '" + wanted->valuestring + "'");
                    }
                    else if (key == "pixels")
                    {
                        // The colour of the picture at points given in virtual pixels: [x, y, r, g, b] each.
                        const cJSON *point = nullptr;
                        cJSON_ArrayForEach(point, wanted)
                        {
                            int v[5] = {0, 0, 0, 0, 0};
                            for (int n = 0; n < 5; ++n)
                                v[n] = static_cast<int>(cJSON_GetArrayItem(point, n) != nullptr ? cJSON_GetArrayItem(point, n)->valuedouble : 0);
                            const int px = std::clamp(v[0] * options.width / 1920, 0, options.width - 1);
                            const int py = std::clamp(v[1] * options.height / 1080, 0, options.height - 1);
                            const unsigned char *got = &pixels[(static_cast<std::size_t>(py) * static_cast<std::size_t>(options.width) + static_cast<std::size_t>(px)) * 4u];
                            if (std::abs(got[0] - v[2]) > 70 || std::abs(got[1] - v[3]) > 70 || std::abs(got[2] - v[4]) > 70)
                            {
                                char line[160];
                                std::snprintf(line, sizeof line, "the picture at %d,%d is %d,%d,%d, expected about %d,%d,%d", v[0], v[1], got[0], got[1], got[2], v[2], v[3], v[4]);
                                fail(results, name, look_id, line);
                            }
                        }
                    }
                    else if (key == "asked")
                    {
                        const cJSON *catalog = nullptr;
                        cJSON_ArrayForEach(catalog, wanted)
                        {
                            if (std::find(source.asked().begin(), source.asked().end(), catalog->valuestring) == source.asked().end())
                                fail(results, name, look_id, std::string("catalog '") + catalog->valuestring + "' was never asked for");
                        }
                    }
                    // ---- stage S: what the board was heard and felt to do
                    else if (key == "cues")
                    {
                        if (!cJSON_IsString(wanted) || heard.names() != wanted->valuestring)
                            fail(results, name, look_id, "heard '" + heard.names() + "', expected '" + (cJSON_IsString(wanted) ? wanted->valuestring : "?") + "'");
                    }
                    else if (key == "pulses")
                    {
                        if (static_cast<double>(heard.pulses.size()) != wanted->valuedouble)
                            fail(results, name, look_id, "felt " + std::to_string(heard.pulses.size()) + " pulses, expected " + std::to_string(static_cast<int>(wanted->valuedouble)));
                    }
                    // ---- end of stage S
                    else if (cJSON_IsString(wanted))
                    {
                        const std::string got = text_of(state, key.c_str(), "(none)");
                        if (got != wanted->valuestring)
                            fail(results, name, look_id, key + " is '" + got + "', expected '" + wanted->valuestring + "'");
                    }
                    else if (cJSON_IsNumber(wanted))
                    {
                        const double got = number_of(state, key.c_str(), -1.0);
                        if (got != wanted->valuedouble)
                            fail(results, name, look_id, key + " is " + std::to_string(got) + ", expected " + std::to_string(wanted->valuedouble));
                    }
                }
                cJSON_Delete(state);

                if (options.pictures && reel == nullptr)
                {
                    const std::string folder = options.output + "/" + look_id;
                    mkdir(folder.c_str(), 0755);
                    const std::string path = folder + "/" + name + (options.lang == "en" ? "" : "." + options.lang) + ".png";
                    ++results.checks;
                    if (stbi_write_png(path.c_str(), options.width, options.height, 4, pixels.data(), options.width * 4) == 0)
                        fail(results, name, look_id, "could not write " + path);
                    else
                        ++results.pictures;
                    // The page is drawn: more than a handful of colours came out of the renderer.
                    std::vector<std::uint32_t> seen;
                    for (std::size_t i = 0; i < pixels.size() && seen.size() < 64; i += 4 * 997)
                    {
                        std::uint32_t value = 0;
                        std::memcpy(&value, &pixels[i], 4);
                        if (std::find(seen.begin(), seen.end(), value) == seen.end())
                            seen.push_back(value);
                    }
                    ++results.checks;
                    if (seen.size() < 6)
                        fail(results, name, look_id, "the picture has only " + std::to_string(seen.size()) + " colours");
                    std::fprintf(stderr, "%-28s %-9s %4zu+%-4zu shapes, fill %.2f, GL error 0x%x\n", name.c_str(), look_id.c_str(),
                                 frame.stats().world_instances, frame.stats().ui_instances, frame.stats().fill, glGetError());
                }
            }
        }
        cJSON_Delete(root);
    }
    if (reel != nullptr)
        pclose(reel);
    if (engine)
    {
        const tube::text::Stats stats = engine->stats();
        std::fprintf(stderr, "text engine: %u faces known, %u read, %u glyphs in %u pages, %llu rasterised, %llu evicted, %llu overflows\n", stats.faces,
                     stats.faces_open, stats.glyphs, stats.pages, static_cast<unsigned long long>(stats.rasterised),
                     static_cast<unsigned long long>(stats.evicted), static_cast<unsigned long long>(stats.overflows));
    }
    if (options.lang != "en" && !platform.untranslated().empty())
    {
        for (const std::string &text : platform.untranslated())
            std::fprintf(stderr, "FAIL language %s: no translation for '%s'\n", options.lang.c_str(), text.c_str());
        results.failures += static_cast<int>(platform.untranslated().size());
    }
    std::printf("%s: afterglow host: %d boards, %d pictures, %d checks, %d failed (%s, %s)\n", results.failures == 0 ? "PASS" : "FAIL",
                results.boards, results.pictures, results.checks, results.failures, options.lang.c_str(),
                options.looks.size() == 1 ? options.looks[0].c_str() : "all looks");
    return results.failures == 0 && results.boards > 0 ? 0 : 1;
}
