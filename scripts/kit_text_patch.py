#!/usr/bin/env python3
"""Give a copy of the kit's font a door for the text engine.

The UI kit draws every text through gfx::Font, whose functions are not
virtual. ProsperoTube's builds therefore compile a patched copy of the kit's
gfx/font.hpp and gfx/font.cpp (never the pinned checkout itself): with a
shaper installed, a font the shaper knows is measured, cut, wrapped and laid
out by it; every other font, and every font while no shaper is installed,
behaves exactly as the kit's. native/afterglow/text_engine.cpp is the shaper.

`python3 scripts/kit_text_patch.py <kit directory> <output directory>` writes
<output>/gfx/font.hpp and <output>/gfx/font.cpp; a build puts <output> ahead
of the kit's src on its include path and compiles that font.cpp instead of
the kit's. scripts/prepare_native.py patches the app's copy in place.
"""
import pathlib
import sys

DOOR = '''class Font
{
  public:
    // ProsperoTube: a shaper, when one is installed, measures and lays out the
    // text of the fonts it knows (native/afterglow/text_engine.cpp). The baked
    // font below draws whenever there is none, or it does not know the font.
    class Shaper
    {
      public:
        virtual ~Shaper() = default;
        virtual bool knows(const Font &font) const = 0;
        virtual float measure(const Font &font, std::string_view text, float size, float tracking) = 0;
        virtual std::string fit(const Font &font, std::string_view text, float size, float max_width, float tracking) = 0;
        virtual float layout(const Font &font, std::string_view text, float x, float y, float size, Align align,
                             std::vector<GlyphQuad> &quads, float tracking) = 0;
        virtual std::vector<std::string> wrap(const Font &font, std::string_view text, float size, float max_width) = 0;
        virtual bool has_glyph(const Font &font, std::uint32_t codepoint) = 0;
    };
    static Shaper *shaper;

'''

HOOKS = (
    ("std::string Font::fit(std::string_view text, float size, float max_width, float tracking) const\n{\n",
     "    if (shaper != nullptr && shaper->knows(*this))\n        return shaper->fit(*this, text, size, max_width, tracking);\n"),
    ("bool Font::has_glyph(std::uint32_t codepoint) const\n{\n",
     "    if (shaper != nullptr && shaper->knows(*this))\n        return shaper->has_glyph(*this, codepoint);\n"),
    ("float Font::measure(std::string_view text, float size, float tracking) const\n{\n",
     "    if (shaper != nullptr && shaper->knows(*this))\n        return shaper->measure(*this, text, size, tracking);\n"),
    ("float Font::layout(std::string_view text, float x, float y, float size, Align align,\n"
     "                   std::vector<GlyphQuad> &quads, float tracking) const\n{\n",
     "    if (shaper != nullptr && shaper->knows(*this))\n        return shaper->layout(*this, text, x, y, size, align, quads, tracking);\n"),
    ("std::vector<std::string> Font::wrap(std::string_view text, float size, float max_width) const\n{\n",
     "    if (shaper != nullptr && shaper->knows(*this))\n        return shaper->wrap(*this, text, size, max_width);\n"),
)


def once(text, old, new, what):
    if text.count(old) != 1:
        raise RuntimeError(f"UI kit font patch: '{what}' was expected once and found {text.count(old)} times; the pinned kit changed")
    return text.replace(old, new)


def patch(header, source):
    """The patched texts of gfx/font.hpp and gfx/font.cpp. Idempotent."""
    if "class Shaper" not in header:
        header = once(header, "class Font\n{\n  public:\n", DOOR, "class Font")
    if "Font::Shaper *Font::shaper" not in source:
        for signature, hook in HOOKS:
            source = once(source, signature, signature + hook, signature.split("(")[0])
        source = once(source, "bool Font::load(std::string_view data)\n{\n",
                      "Font::Shaper *Font::shaper = nullptr;\n\nbool Font::load(std::string_view data)\n{\n", "Font::load")
    return header, source


def stage(kit, output):
    kit = pathlib.Path(kit)
    output = pathlib.Path(output) / "gfx"
    output.mkdir(parents=True, exist_ok=True)
    header, source = patch((kit / "src/gfx/font.hpp").read_text(encoding="utf-8"), (kit / "src/gfx/font.cpp").read_text(encoding="utf-8"))
    for name, text in (("font.hpp", header), ("font.cpp", source)):
        target = output / name
        if not target.exists() or target.read_text(encoding="utf-8") != text:
            target.write_text(text, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    stage(sys.argv[1], sys.argv[2])
