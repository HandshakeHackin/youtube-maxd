// ProsperoTube - Text engine: font files, the faces in them and which one draws a cluster.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The console font names and the order they are asked in come from the family's other apps
// (ProsperoEden's launcher, ProsperoLichess, ProsperoRadio: gfx/system_fonts.cpp, same author,
// same licence), which read them on real consoles.

#include "tube_text_fonts.hpp"

#include "tube_text_unicode.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ot.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

#if !defined(TUBE_TEXT_NO_DIRENT)
#include <dirent.h>
#endif
#if !defined(TUBE_TEXT_NO_MMAP)
#include <sys/mman.h>
#endif

namespace tube::text
{

namespace
{

// The console's fonts by what they are for; of each line the first file found is used (the same
// font in the weights and generations a system software version may carry).
struct Group
{
    FaceRole role;
    const char *files[3];
};
constexpr Group kGroups[] = {
    {FaceRole::latin, {"SST-Medium.otf", "SST-Roman.otf", nullptr}},
    {FaceRole::japanese, {"SSTJpPro-Regular.otf", "SIE-RDC-Pr5N-M-JPN.otf", "SCE-RDC-R-JPN.otf"}},
    {FaceRole::chinese, {"DFHEI5-SONY.ttf", nullptr, nullptr}},
    {FaceRole::korean, {"YoonGothicProSIE760.otf", "SCEPS4Yoongd-Medium.otf", nullptr}},
    {FaceRole::thai, {"SSTThai-Medium.otf", "SSTThai-Roman.otf", nullptr}},
    {FaceRole::arabic, {"SSTArabic-Medium.otf", "SSTArabic-Roman.otf", nullptr}},
};
constexpr int kMaxFaces = 96;
constexpr std::size_t kMaxTableBytes = 32u << 20;

const char *role_name(FaceRole role)
{
    switch (role)
    {
    case FaceRole::latin:
        return "latin";
    case FaceRole::japanese:
        return "japanese";
    case FaceRole::korean:
        return "korean";
    case FaceRole::chinese:
        return "chinese";
    case FaceRole::thai:
        return "thai";
    case FaceRole::arabic:
        return "arabic";
    case FaceRole::other:
        break;
    }
    return "other";
}

bool exists(const std::string &path)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
        return false;
    std::fclose(file);
    return true;
}

std::string file_name(const std::string &path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool font_extension(const std::string &name)
{
    if (name.size() < 5)
        return false;
    std::string ext = name.substr(name.size() - 4);
    for (char &c : ext)
        c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    return ext == ".otf" || ext == ".ttf" || ext == ".ttc" || ext == ".otc";
}

bool known_name(const std::string &name)
{
    for (const Group &group : kGroups)
        for (const char *file : group.files)
            if (file != nullptr && name == file)
                return true;
    return false;
}

std::uint32_t be32(const std::uint8_t *p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

std::uint16_t be16(const std::uint8_t *p)
{
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

double seconds_since(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

bool is_common(hb_script_t s)
{
    return s == HB_SCRIPT_COMMON || s == HB_SCRIPT_INHERITED || s == HB_SCRIPT_UNKNOWN || s == HB_SCRIPT_INVALID;
}

bool is_cjk(hb_script_t s)
{
    return s == HB_SCRIPT_HAN || s == HB_SCRIPT_HIRAGANA || s == HB_SCRIPT_KATAKANA || s == HB_SCRIPT_HANGUL ||
           s == HB_SCRIPT_BOPOMOFO;
}

// The roles to ask first for a script, in order; returns how many.
int roles_for(hb_script_t s, HanForms han, FaceRole out[3])
{
    switch (s)
    {
    case HB_SCRIPT_ARABIC:
        out[0] = FaceRole::arabic;
        return 1;
    case HB_SCRIPT_THAI:
        out[0] = FaceRole::thai;
        return 1;
    case HB_SCRIPT_HANGUL:
        out[0] = FaceRole::korean;
        return 1;
    case HB_SCRIPT_HIRAGANA:
    case HB_SCRIPT_KATAKANA:
        out[0] = FaceRole::japanese;
        return 1;
    case HB_SCRIPT_BOPOMOFO:
        out[0] = FaceRole::chinese;
        return 1;
    case HB_SCRIPT_HAN:
        if (han == HanForms::japanese)
        {
            out[0] = FaceRole::japanese;
            out[1] = FaceRole::chinese;
            out[2] = FaceRole::korean;
        }
        else if (han == HanForms::korean)
        {
            out[0] = FaceRole::korean;
            out[1] = FaceRole::japanese;
            out[2] = FaceRole::chinese;
        }
        else
        {
            out[0] = FaceRole::chinese;
            out[1] = FaceRole::japanese;
            out[2] = FaceRole::korean;
        }
        return 3;
    default:
        break;
    }
    out[0] = FaceRole::latin;
    return 1;
}

} // namespace

std::vector<std::string> system_font_folders()
{
    std::vector<std::string> folders;
    if (const char *folder = std::getenv("TUBE_TEXT_FONTS"); folder != nullptr && folder[0] != 0)
        folders.emplace_back(folder);
    folders.emplace_back("/preinst/common/font");
    return folders;
}

// ---- files ----

std::shared_ptr<FontFile> FontFile::open(const std::string &path, IoMode mode, std::size_t max_bytes,
                                         std::string *error)
{
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr)
    {
        *error = std::string("cannot be opened (") + std::strerror(errno) + ")";
        return nullptr;
    }
    long size = -1;
    if (std::fseek(file, 0, SEEK_END) == 0)
        size = std::ftell(file);
    if (size < 12)
    {
        std::fclose(file);
        *error = "is not a font file (too small)";
        return nullptr;
    }
    if (static_cast<std::size_t>(size) > max_bytes)
    {
        std::fclose(file);
        *error = "is larger than the limit (" + std::to_string(size) + " bytes)";
        return nullptr;
    }
    std::shared_ptr<FontFile> result(new FontFile());
    result->size_ = static_cast<std::size_t>(size);
#if !defined(TUBE_TEXT_NO_MMAP)
    if (mode != IoMode::stream)
    {
        void *map = mmap(nullptr, result->size_, PROT_READ, MAP_PRIVATE, fileno(file), 0);
        if (map != MAP_FAILED)
        {
            result->map_ = static_cast<const std::uint8_t *>(map);
            std::fclose(file);
            return result;
        }
    }
#endif
    if (mode == IoMode::map)
    {
        std::fclose(file);
        *error = "cannot be mapped into memory";
        return nullptr;
    }
    result->file_ = file;
    return result;
}

FontFile::~FontFile()
{
#if !defined(TUBE_TEXT_NO_MMAP)
    if (map_ != nullptr)
        munmap(const_cast<std::uint8_t *>(map_), size_);
#endif
    if (file_ != nullptr)
        std::fclose(file_);
}

bool FontFile::read(std::size_t offset, void *out, std::size_t count)
{
    if (offset > size_ || count > size_ - offset)
        return false;
    if (count == 0)
        return true;
    if (map_ != nullptr)
    {
        std::memcpy(out, map_ + offset, count);
        return true;
    }
    if (file_ == nullptr || std::fseek(file_, static_cast<long>(offset), SEEK_SET) != 0)
        return false;
    if (std::fread(out, 1, count, file_) != count)
        return false;
    bytes_read_ += count;
    return true;
}

// ---- faces ----

struct Fonts::Face
{
    std::string path;
    int index = 0;
    FaceRole role = FaceRole::other;
    bool bold = false;
    bool discovered = false;
    int family = 0;

    enum class State : std::uint8_t
    {
        untried,
        ready,
        failed,
    };
    State state = State::untried;
    std::string error;
    std::shared_ptr<FontFile> file;
    // Stream mode: where the face's tables are in the file.
    struct Table
    {
        std::uint32_t tag;
        std::uint32_t offset;
        std::uint32_t length;
    };
    std::vector<Table> tables;
    hb_face_t *hb_face = nullptr;
    hb_font_t *hb_font = nullptr;
    FT_Face ft_face = nullptr;
    FT_StreamRec_ *ft_stream = nullptr;
    bool ft_tried = false;
    float upem = 1000.0f;
    float ascent = 0.88f;
    float descent = 0.22f;
    float line_gap = 0.0f;
    float digit = -1.0f; // widest digit in em; -1: not measured

    ~Face()
    {
        if (ft_face != nullptr)
            FT_Done_Face(ft_face);
        delete ft_stream;
        hb_font_destroy(hb_font);
        hb_face_destroy(hb_face);
    }
};

namespace
{

// HarfBuzz asks for a table; stream mode reads just that table from the file.
hb_blob_t *reference_table(hb_face_t *, hb_tag_t tag, void *user)
{
    auto *face = static_cast<Fonts::Face *>(user);
    for (const Fonts::Face::Table &table : face->tables)
    {
        if (table.tag != tag)
            continue;
        if (table.length == 0 || table.length > kMaxTableBytes)
            return nullptr;
        void *data = std::malloc(table.length);
        if (data == nullptr)
            return nullptr;
        if (!face->file->read(table.offset, data, table.length))
        {
            std::free(data);
            return nullptr;
        }
        return hb_blob_create(static_cast<const char *>(data), table.length, HB_MEMORY_MODE_WRITABLE, data, std::free);
    }
    return nullptr;
}

// The table directory of face `index` (a plain font, or one of a collection).
bool read_directory(FontFile &file, int index, std::vector<Fonts::Face::Table> *tables, std::string *error)
{
    std::uint8_t header[12];
    if (!file.read(0, header, sizeof(header)))
    {
        *error = "cannot be read";
        return false;
    }
    std::size_t start = 0;
    if (be32(header) == 0x74746366u) // 'ttcf'
    {
        const std::uint32_t fonts = be32(header + 8);
        if (index < 0 || static_cast<std::uint32_t>(index) >= fonts)
        {
            *error = "has no face " + std::to_string(index);
            return false;
        }
        std::uint8_t offset[4];
        if (!file.read(12 + 4 * static_cast<std::size_t>(index), offset, sizeof(offset)))
        {
            *error = "has a cut collection header";
            return false;
        }
        start = be32(offset);
        if (!file.read(start, header, sizeof(header)))
        {
            *error = "has a face outside the file";
            return false;
        }
    }
    else if (index != 0)
    {
        *error = "has no face " + std::to_string(index);
        return false;
    }
    const std::uint32_t version = be32(header);
    if (version != 0x00010000u && version != 0x4f54544fu && version != 0x74727565u) // 1.0, 'OTTO', 'true'
    {
        *error = "is not a TrueType or OpenType font";
        return false;
    }
    const std::uint16_t count = be16(header + 4);
    if (count == 0 || count > 512)
    {
        *error = "has an implausible table count";
        return false;
    }
    std::vector<std::uint8_t> records(static_cast<std::size_t>(count) * 16);
    if (!file.read(start + 12, records.data(), records.size()))
    {
        *error = "has a cut table directory";
        return false;
    }
    tables->clear();
    for (std::uint16_t i = 0; i < count; ++i)
    {
        const std::uint8_t *record = records.data() + static_cast<std::size_t>(i) * 16;
        Fonts::Face::Table table{be32(record), be32(record + 8), be32(record + 12)};
        if (table.offset > file.size() || table.length > file.size() - table.offset)
            continue; // a table outside the file is as good as absent
        tables->push_back(table);
    }
    return true;
}

unsigned long stream_read(FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count)
{
    auto *file = static_cast<FontFile *>(stream->descriptor.pointer);
    if (count == 0)
        return offset > file->size() ? 1 : 0; // a seek: non-zero is an error
    if (offset >= file->size())
        return 0;
    const std::size_t available = std::min<std::size_t>(count, file->size() - offset);
    return file->read(offset, buffer, available) ? static_cast<unsigned long>(available) : 0;
}

} // namespace

Fonts::Fonts(const Config &config, std::vector<std::string> *messages) : config_(config), messages_(messages)
{
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0)
    {
        messages_->push_back("fonts: FreeType did not start");
        library = nullptr;
    }
    library_ = library;
    buffer_ = hb_buffer_create();
    for (int &primary : primary_)
        primary = -2;

    for (const FaceSpec &spec : config_.faces)
        add(spec.path, spec.index, spec.role, spec.bold, false, spec.family & 15);

    for (const Group &group : kGroups)
    {
        bool found = false;
        for (const std::string &folder : config_.font_dirs)
        {
            for (const char *name : group.files)
            {
                if (name == nullptr || found)
                    break;
                const std::string path = folder + "/" + name;
                if (exists(path))
                {
                    add(path, 0, group.role, false, false, 0);
                    found = true;
                }
            }
        }
        if (!found && !config_.font_dirs.empty())
        {
            std::string names;
            for (const char *name : group.files)
                if (name != nullptr)
                    names += std::string(names.empty() ? "" : ", ") + name;
            messages_->push_back(std::string("fonts: no ") + role_name(group.role) + " face of the console (" + names +
                                 "); other faces are asked");
        }
    }

    if (config_.discover)
    {
#if !defined(TUBE_TEXT_NO_DIRENT)
        for (const std::string &folder : config_.font_dirs)
        {
            DIR *dir = opendir(folder.c_str());
            if (dir == nullptr)
            {
                messages_->push_back("fonts: " + folder + " cannot be listed (" + std::strerror(errno) + ")");
                continue;
            }
            std::vector<std::string> names;
            while (const dirent *entry = readdir(dir))
            {
                const std::string name = entry->d_name;
                if (font_extension(name) && !known_name(name))
                    names.push_back(name);
            }
            closedir(dir);
            std::sort(names.begin(), names.end());
            for (const std::string &name : names)
                add(folder + "/" + name, 0, FaceRole::other, false, true, 0);
        }
#endif
    }
    messages_->push_back("fonts: " + std::to_string(faces_.size()) + " faces known");
}

Fonts::~Fonts()
{
    faces_.clear(); // before the library their FreeType faces belong to
    hb_buffer_destroy(buffer_);
    if (library_ != nullptr)
        FT_Done_FreeType(library_);
}

void Fonts::add(const std::string &path, int index, FaceRole role, bool bold, bool discovered, int family)
{
    if (static_cast<int>(faces_.size()) >= kMaxFaces)
        return;
    for (const auto &face : faces_)
        if (face->path == path && face->index == index && face->family == family)
            return;
    auto face = std::make_unique<Face>();
    face->path = path;
    face->index = index;
    face->role = role;
    face->bold = bold;
    face->discovered = discovered;
    face->family = family;
    faces_.push_back(std::move(face));
}

const std::string &Fonts::path(int face) const
{
    return faces_[static_cast<std::size_t>(face)]->path;
}

FaceRole Fonts::role(int face) const
{
    return faces_[static_cast<std::size_t>(face)]->role;
}

bool Fonts::is_bold(int face) const
{
    return faces_[static_cast<std::size_t>(face)]->bold;
}

bool Fonts::usable(int index)
{
    if (index < 0 || index >= count())
        return false;
    Face &face = *faces_[static_cast<std::size_t>(index)];
    if (face.state != Face::State::untried)
        return face.state == Face::State::ready;
    const auto start = std::chrono::steady_clock::now();
    face.state = Face::State::failed;
    const auto fail = [&](const std::string &why)
    {
        face.error = why;
        // A file nobody named (found in a folder) that is no font is not worth a line.
        if (!face.discovered)
            messages_->push_back("font " + face.path + " " + why + ": skipped");
        open_seconds_ += seconds_since(start);
        return false;
    };

    std::shared_ptr<FontFile> file;
    if (const auto known = files_.find(face.path); known != files_.end())
        file = known->second.lock();
    if (!file)
    {
        std::string error;
        file = FontFile::open(face.path, config_.io, config_.max_font_bytes, &error);
        if (!file)
            return fail(error);
        files_[face.path] = file;
        if (file->mapped() != nullptr)
            bytes_mapped_ += file->size();
    }
    face.file = file;

    if (file->mapped() != nullptr)
    {
        hb_blob_t *blob = hb_blob_create(reinterpret_cast<const char *>(file->mapped()),
                                         static_cast<unsigned>(file->size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        const unsigned faces_in_file = hb_face_count(blob);
        if (static_cast<unsigned>(face.index) >= faces_in_file)
        {
            hb_blob_destroy(blob);
            return fail(faces_in_file == 0 ? "is not a TrueType or OpenType font"
                                           : "has no face " + std::to_string(face.index));
        }
        face.hb_face = hb_face_create(blob, static_cast<unsigned>(face.index));
        hb_blob_destroy(blob);
    }
    else
    {
        std::string error;
        if (!read_directory(*file, face.index, &face.tables, &error))
            return fail(error);
        face.hb_face = hb_face_create_for_tables(reference_table, &face, nullptr);
    }
    const unsigned upem = hb_face_get_upem(face.hb_face);
    if (hb_face_get_glyph_count(face.hb_face) == 0 || upem < 16 || upem > 16384)
        return fail("has no glyphs HarfBuzz can read");
    if (hb_ot_color_has_png(face.hb_face))
        return fail("is a colour bitmap font (no outlines)");
    face.upem = static_cast<float>(upem);
    face.hb_font = hb_font_create(face.hb_face);
    hb_ot_font_set_funcs(face.hb_font);
    hb_font_extents_t extents{};
    if (hb_font_get_h_extents(face.hb_font, &extents) && extents.ascender > 0)
    {
        face.ascent = static_cast<float>(extents.ascender) / face.upem;
        face.descent = static_cast<float>(-extents.descender) / face.upem;
        face.line_gap = static_cast<float>(extents.line_gap) / face.upem;
        if (face.descent < 0.0f)
            face.descent = 0.0f;
        if (face.line_gap < 0.0f)
            face.line_gap = 0.0f;
    }
    face.state = Face::State::ready;
    ++open_count_;
    open_seconds_ += seconds_since(start);
    return true;
}

bool Fonts::has(int index, char32_t c)
{
    if (!usable(index))
        return false;
    hb_codepoint_t glyph = 0;
    return hb_font_get_nominal_glyph(faces_[static_cast<std::size_t>(index)]->hb_font, c, &glyph) != 0;
}

bool Fonts::has_all(int index, const char32_t *cps, std::size_t count)
{
    return covers(index, cps, count);
}

bool Fonts::covers(int index, const char32_t *need, std::size_t count)
{
    if (!usable(index))
        return false;
    hb_font_t *font = faces_[static_cast<std::size_t>(index)]->hb_font;
    for (std::size_t i = 0; i < count; ++i)
    {
        hb_codepoint_t glyph = 0;
        if (!hb_font_get_nominal_glyph(font, need[i], &glyph))
            return false;
    }
    return true;
}

int Fonts::role_face(FaceRole role, int family)
{
    for (const int of : {family, 0})
    {
        for (int i = 0; i < count(); ++i)
        {
            const Face &face = *faces_[static_cast<std::size_t>(i)];
            if (face.role == role && !face.bold && face.family == of && usable(i))
                return i;
        }
    }
    return -1;
}

int Fonts::search(const char32_t *need, std::size_t need_count, const int *preferred, int preferred_count,
                  bool bold_faces, int family)
{
    const auto asked = [&](int face, int of)
    {
        const Face &candidate = *faces_[static_cast<std::size_t>(face)];
        return candidate.bold == bold_faces && candidate.family == of;
    };
    for (int k = 0; k < preferred_count; ++k)
    {
        const int face = preferred[k];
        if (face >= 0 && face < count() && (asked(face, family) || asked(face, 0)) && covers(face, need, need_count))
            return face;
    }
    // The family's own faces, then the faces every text shares; in the order they were named.
    for (int pass = family != 0 ? 0 : 1; pass < 2; ++pass)
        for (int face = 0; face < count(); ++face)
            if (asked(face, pass == 0 ? family : 0) && covers(face, need, need_count))
                return face;
    return -1;
}

int Fonts::pick(const char32_t *cluster, std::size_t cluster_count, hb_script_t own, hb_script_t resolved,
                HanForms han, bool bold, int neighbour, int family)
{
    family &= 15;
    // What the face must have: everything that draws. Joiners, selectors and skin tones do not.
    char32_t need[8];
    std::size_t need_count = 0;
    for (std::size_t i = 0; i < cluster_count && need_count < 8; ++i)
        if (!uni::default_ignorable(cluster[i]) && !uni::emoji_modifier(cluster[i]))
            need[need_count++] = cluster[i];
    if (need_count == 0)
        return neighbour >= 0 ? neighbour : primary(family);

    const bool common = is_common(own);
    const hb_script_t context = common ? resolved : own;
    const bool latin_first = common && is_cjk(resolved);
    const bool by_neighbour = common && !latin_first && neighbour >= 0;
    // One cached answer per character and situation.
    std::uint64_t key = 0;
    if (need_count == 1)
    {
        std::uint64_t situation = 0;
        switch (context)
        {
        case HB_SCRIPT_ARABIC:
            situation = 1;
            break;
        case HB_SCRIPT_THAI:
            situation = 2;
            break;
        case HB_SCRIPT_HANGUL:
            situation = 3;
            break;
        case HB_SCRIPT_HIRAGANA:
        case HB_SCRIPT_KATAKANA:
            situation = 4;
            break;
        case HB_SCRIPT_HAN:
            situation = 5 + static_cast<std::uint64_t>(han);
            break;
        case HB_SCRIPT_BOPOMOFO:
            situation = 8;
            break;
        default:
            break;
        }
        key = need[0] | (static_cast<std::uint64_t>(bold) << 21) | (situation << 22) |
              (static_cast<std::uint64_t>(latin_first) << 26) |
              (static_cast<std::uint64_t>(by_neighbour ? neighbour + 1 : 0) << 27) |
              (static_cast<std::uint64_t>(family) << 40);
        if (const auto found = picked_.find(key); found != picked_.end())
            return found->second;
    }

    int preferred[8];
    int preferred_count = 0;
    const auto prefer_role = [&](FaceRole wanted)
    {
        for (int pass = family != 0 ? 0 : 1; pass < 2; ++pass)
            for (int i = 0; i < count() && preferred_count < 8; ++i)
                if (faces_[static_cast<std::size_t>(i)]->role == wanted &&
                    faces_[static_cast<std::size_t>(i)]->family == (pass == 0 ? family : 0))
                    preferred[preferred_count++] = i;
    };
    if (by_neighbour)
        preferred[preferred_count++] = neighbour;
    if (latin_first)
        prefer_role(FaceRole::latin);
    FaceRole roles[3];
    const int role_count = roles_for(context, han, roles);
    for (int r = 0; r < role_count; ++r)
        prefer_role(roles[r]);

    int chosen = -1;
    for (std::size_t wanted = need_count; chosen < 0; wanted = 1)
    {
        // Bold text: a bold cut first, then a regular face (its outline is widened).
        if (bold)
            chosen = search(need, wanted, preferred, preferred_count, true, family);
        if (chosen < 0)
            chosen = search(need, wanted, preferred, preferred_count, false, family);
        if (wanted == 1)
            break; // no face has the whole cluster: one that has its first character will do
    }
    if (need_count == 1)
    {
        if (picked_.size() > 65536)
            picked_.clear();
        picked_.emplace(key, static_cast<std::int16_t>(chosen));
    }
    return chosen;
}

hb_font_t *Fonts::shaper(int index)
{
    return usable(index) ? faces_[static_cast<std::size_t>(index)]->hb_font : nullptr;
}

float Fonts::units_per_em(int index)
{
    return usable(index) ? faces_[static_cast<std::size_t>(index)]->upem : 1000.0f;
}

FT_FaceRec_ *Fonts::outlines(int index)
{
    if (library_ == nullptr || !usable(index))
        return nullptr;
    Face &face = *faces_[static_cast<std::size_t>(index)];
    if (face.ft_tried)
        return face.ft_face;
    face.ft_tried = true;
    const auto start = std::chrono::steady_clock::now();
    FT_Open_Args args{};
    if (face.file->mapped() != nullptr)
    {
        args.flags = FT_OPEN_MEMORY;
        args.memory_base = face.file->mapped();
        args.memory_size = static_cast<FT_Long>(face.file->size());
    }
    else
    {
        face.ft_stream = new FT_StreamRec_();
        face.ft_stream->size = static_cast<unsigned long>(face.file->size());
        face.ft_stream->descriptor.pointer = face.file.get();
        face.ft_stream->read = stream_read;
        args.flags = FT_OPEN_STREAM;
        args.stream = face.ft_stream;
    }
    FT_Face opened = nullptr;
    const FT_Error error = FT_Open_Face(library_, &args, face.index, &opened);
    if (error != 0 || opened == nullptr)
    {
        messages_->push_back("font " + face.path + " cannot be drawn by FreeType (error " + std::to_string(error) +
                             "): its glyphs stay empty");
        opened = nullptr;
    }
    face.ft_face = opened;
    open_seconds_ += seconds_since(start);
    return face.ft_face;
}

int Fonts::primary(int family)
{
    family &= 15;
    int &primary = primary_[family];
    if (primary != -2)
        return primary;
    primary = role_face(FaceRole::latin, family);
    for (int i = 0; primary < 0 && i < count(); ++i)
        if (!faces_[static_cast<std::size_t>(i)]->bold && faces_[static_cast<std::size_t>(i)]->family == 0 && usable(i))
            primary = i;
    for (int i = 0; primary < 0 && i < count(); ++i)
        if (usable(i))
            primary = i;
    return primary;
}

void Fonts::metrics(int index, float *ascent, float *descent, float *line_gap)
{
    if (!usable(index))
    {
        *ascent = 0.88f;
        *descent = 0.22f;
        *line_gap = 0.1f;
        return;
    }
    const Face &face = *faces_[static_cast<std::size_t>(index)];
    *ascent = face.ascent;
    *descent = face.descent;
    *line_gap = face.line_gap;
}

float Fonts::digit_advance(int index)
{
    if (!usable(index))
        return 0.0f;
    Face &face = *faces_[static_cast<std::size_t>(index)];
    if (face.digit >= 0.0f)
        return face.digit;
    face.digit = 0.0f;
    const hb_feature_t tabular{HB_TAG('t', 'n', 'u', 'm'), 1, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END};
    for (char32_t digit = '0'; digit <= '9'; ++digit)
    {
        hb_codepoint_t glyph = 0;
        if (!hb_font_get_nominal_glyph(face.hb_font, digit, &glyph))
            continue;
        hb_buffer_reset(buffer_);
        const std::uint32_t text = digit;
        hb_buffer_add_utf32(buffer_, &text, 1, 0, 1);
        hb_buffer_set_direction(buffer_, HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer_, HB_SCRIPT_LATIN);
        hb_shape(face.hb_font, buffer_, &tabular, 1);
        unsigned glyphs = 0;
        const hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer_, &glyphs);
        float advance = 0.0f;
        for (unsigned i = 0; i < glyphs; ++i)
            advance += static_cast<float>(positions[i].x_advance);
        face.digit = std::max(face.digit, advance / face.upem);
    }
    return face.digit;
}

std::uint64_t Fonts::bytes_read() const
{
    std::uint64_t total = 0;
    for (const auto &entry : files_)
        if (const auto file = entry.second.lock())
            total += file->bytes_read();
    return total;
}

std::string Fonts::describe() const
{
    std::string text;
    for (const auto &face : faces_)
    {
        if (face->state == Face::State::untried && face->discovered)
            continue;
        if (!text.empty())
            text += ", ";
        text += file_name(face->path);
        if (face->index != 0)
            text += "#" + std::to_string(face->index);
        text += std::string(" (") + role_name(face->role) + (face->bold ? " bold" : "");
        if (face->family != 0)
            text += " family " + std::to_string(face->family);
        if (face->state == Face::State::ready)
        {
            const std::size_t size = face->file ? face->file->size() : 0;
            text += face->file && face->file->mapped() != nullptr ? ", mapped " : ", read on demand ";
            text += std::to_string(size) + " bytes";
        }
        else if (face->state == Face::State::failed)
        {
            text += ", " + face->error;
        }
        else
        {
            text += ", not needed yet";
        }
        text += ")";
    }
    return text.empty() ? "none" : text;
}

} // namespace tube::text
