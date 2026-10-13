// ProsperoTube - Text engine: font files, the faces in them and which one draws a cluster.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tube_text.hpp"

#include <hb.h>

#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_StreamRec_;

namespace tube::text
{

// One font file: mapped into memory, or read at the places asked for.
class FontFile
{
  public:
    static std::shared_ptr<FontFile> open(const std::string &path, IoMode mode, std::size_t max_bytes,
                                          std::string *error);
    ~FontFile();
    FontFile(const FontFile &) = delete;
    FontFile &operator=(const FontFile &) = delete;

    std::size_t size() const
    {
        return size_;
    }
    // The whole file, or null when it is read on demand.
    const std::uint8_t *mapped() const
    {
        return map_;
    }
    bool read(std::size_t offset, void *out, std::size_t count);
    std::uint64_t bytes_read() const
    {
        return bytes_read_;
    }

  private:
    FontFile() = default;
    std::FILE *file_ = nullptr;
    const std::uint8_t *map_ = nullptr;
    std::size_t size_ = 0;
    std::uint64_t bytes_read_ = 0;
};

// Whose forms the characters Japanese, Chinese and Korean share are drawn in.
enum class HanForms : std::uint8_t
{
    japanese,
    korean,
    chinese,
};

class Fonts
{
  public:
    Fonts(const Config &config, std::vector<std::string> *messages);
    ~Fonts();
    Fonts(const Fonts &) = delete;
    Fonts &operator=(const Fonts &) = delete;

    // False when FreeType did not start (no glyph can be drawn then).
    bool started() const
    {
        return library_ != nullptr;
    }
    int count() const
    {
        return static_cast<int>(faces_.size());
    }
    const std::string &path(int face) const;
    FaceRole role(int face) const;
    bool is_bold(int face) const;

    // Opens the face on first use; false when it cannot be read (said once in the messages).
    bool usable(int face);
    bool has(int face, char32_t c);

    // The face for one cluster: `own` is the script of its first character, `resolved` that of the
    // text around it when its own is none (a space, a digit, a full stop); `neighbour` is the face
    // of the cluster beside it, or -1. Returns -1 when no face has the cluster's first character.
    // `family` is the style's: faces of that family are asked before those of family 0, faces of
    // other families never.
    int pick(const char32_t *cluster, std::size_t count, hb_script_t own, hb_script_t resolved, HanForms han,
             bool bold, int neighbour, int family = 0);
    // The first regular face of that role (of the family, else of family 0), or -1.
    int role_face(FaceRole role, int family = 0);
    // True when the face has every one of these characters.
    bool has_all(int face, const char32_t *cps, std::size_t count);

    hb_font_t *shaper(int face);        // sized in font units
    FT_FaceRec_ *outlines(int face);    // null when FreeType cannot read the face
    float units_per_em(int face);
    // The face line metrics come from: the first usable interface face of the family (else of
    // family 0), else the first usable face.
    int primary(int family = 0);
    // In em; of the face, or plausible values for -1.
    void metrics(int face, float *ascent, float *descent, float *line_gap);
    // The widest digit in em, with tabular figures on when the face has them; 0: no digits.
    float digit_advance(int face);

    std::uint32_t open_count() const
    {
        return open_count_;
    }
    std::uint64_t bytes_mapped() const
    {
        return bytes_mapped_;
    }
    std::uint64_t bytes_read() const;
    double open_seconds() const
    {
        return open_seconds_;
    }
    std::string describe() const;

    struct Face;

  private:
    void add(const std::string &path, int index, FaceRole role, bool bold, bool discovered, int family);
    bool covers(int face, const char32_t *need, std::size_t count);
    int search(const char32_t *need, std::size_t count, const int *preferred, int preferred_count, bool bold_faces,
               int family);

    Config config_;
    std::vector<std::string> *messages_;
    FT_LibraryRec_ *library_ = nullptr;
    std::vector<std::unique_ptr<Face>> faces_;
    std::unordered_map<std::string, std::weak_ptr<FontFile>> files_;
    std::unordered_map<std::uint64_t, std::int16_t> picked_;
    hb_buffer_t *buffer_ = nullptr;
    int primary_[16]; // by family; -2: not looked for yet
    std::uint32_t open_count_ = 0;
    std::uint64_t bytes_mapped_ = 0;
    double open_seconds_ = 0.0;
};

} // namespace tube::text
