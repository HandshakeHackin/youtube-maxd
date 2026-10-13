"""Interface localization: the few places where texts reach the screen ask native/tube_i18n.c.

Applied last by prepare_native.py, so the browser document it marks already
holds what the other overlays added. See docs/LOCALIZATION.md.
"""
from pathlib import Path
import re
import shutil
import subprocess

NAMES = {"ProsperoTube", "YouTube", "SponsorBlock", "DeArrow"}
_TEXT = re.compile(r">([^<>]+)<")


def _runs(rml):
    for match in _TEXT.finditer(rml):
        raw = match.group(1)
        text = " ".join(raw.split())
        if "{{" in raw or not re.search(r"[A-Za-z]{3,}", text) or text in NAMES:
            continue
        if re.search(r'[;={}\[\]\\"]|&&|\|\|', text):      # program text between two comparisons, not a text node
            continue
        yield match, raw, text


def rml_texts(rml):
    """The static texts of a browser document, in order, each once."""
    seen = []
    for _, _, text in _runs(rml):
        if text not in seen:
            seen.append(text)
    return seen


def rml_localize(rml):
    """Each static text becomes an expression the document asks the catalog for.

    The second argument is the language serial: changing the language marks it
    dirty, and RmlUi evaluates every text again.
    """
    out, position = [], 0
    for match, raw, text in _runs(rml):
        lead = raw[:len(raw) - len(raw.lstrip())]
        tail = raw[len(raw.rstrip()):]
        quoted = text.replace("\\", "\\\\").replace("'", "\\'")
        out.append(rml[position:match.start(1)] + lead + "{{ '" + quoted + "' | tr(i18n) }}" + tail)
        position = match.end(1)
    out.append(rml[position:])
    return "".join(out)


RCSS = """/* interface languages */
#tube-footer, #tube-discussion-footer, #tube-chat-footer, #tube-review-footer, #tube-auth-footer, #tube-description-footer, #tube-link-footer { height: auto; min-height: 40dp; }
"""
TOAST = '''    snprintf(prospero_toast_title,   sizeof(prospero_toast_title),   "%s", title);
    snprintf(prospero_toast_message, sizeof(prospero_toast_message), "%s", msg);
'''
STATUS = '''void EvoRmlProviderHost::SetStatus(const std::string& msg, bool error)
{
    m_model.status = Rml::String(msg.c_str());'''
LOADING = '''    if (!status.empty()) {
        m_model.status = Rml::String(status.c_str());'''
ROWS = '''        r.title     = display_text(s.title);
        r.subtitle  = display_text(s.subtitle);
        const char* comment = m_provider_id == "youtube" ? tube_comment_text(s.id) : "";
        r.overview = display_text(*comment ? comment : s.overview);
'''
MODEL = '''    c.Bind("breadcrumb",    &m_model.breadcrumb);'''
TICK = '''void EvoRmlProviderHost::Tick()
{'''
PAGE = '''    m_model.page_info = Rml::String(pbuf);'''
KIT = '''    return list.text(*font.font, font.texture, value, x, baseline, size, color, align, tracking);'''


def apply_i18n_overlay(replace, player, work, root):
    work, root = Path(work), Path(root)
    # The module and its generated tables.
    for name in ("tube_i18n.c", "tube_i18n.h", "tube_i18n_data.h"):
        shutil.copyfile(root / "native" / name, work / player / ("addons/src" if name.endswith(".c") else "addons/include") / name)
    replace(player + "Makefile", "    addons/src/tube_provider.c", "    addons/src/tube_provider.c \\\n    addons/src/tube_i18n.c")
    # Toasts: every caller passes through toast(); its English classification stays ahead of the translation.
    toast = player + "src/evo_toast.c"
    pin = (work / ".prosperotube-source").read_text().strip()
    source = subprocess.check_output(["git", "-C", str(root / "third_party/evo"), "show", f"{pin}:{toast}"])
    (work / toast).write_bytes(source)
    replace(toast, '#include "evo_toast.h"', '#include "evo_toast.h"\n#include "tube_i18n.h"')
    replace(toast, TOAST, TOAST + '''    if (tube_i18n_field) {
        tube_i18n_field(prospero_toast_title, sizeof(prospero_toast_title), 0);
        tube_i18n_field(prospero_toast_message, sizeof(prospero_toast_message), TUBE_I18N_LABELS);
    }
''')
    # Provider rows made of interface texts carry a mark; rows of YouTube content never do.
    replace(player + "addons/include/evo_provider.h", "    char    now_title[128];",
            "    int     tube_interface;     /* ProsperoTube: built from interface texts, the browser may translate it */\n    char    now_title[128];")
    # The browser: its status line, the page counter and the static texts of the document.
    screen = player + "ui_rml/src/evo_rmlui_provider.cpp"
    replace(screen, '#include "evo_rmlui_provider.h"', '#include "evo_rmlui_provider.h"\n#include "tube_i18n.h"')
    replace(screen, STATUS, '''static std::string tube_i18n_string(const std::string& source, int flags)
{
    char text[512];
    if (!tube_i18n_translate || source.size() >= sizeof text) return source;
    return tube_i18n_translate(source.c_str(), text, sizeof text, flags) ? std::string(text) : source;
}

void EvoRmlProviderHost::SetStatus(const std::string& msg, bool error)
{
    m_model.status = Rml::String(tube_i18n_string(msg, TUBE_I18N_LABELS).c_str());''')
    replace(screen, ROWS, '''        r.title     = display_text(s.title);
        r.subtitle  = display_text(s.subtitle);
        const char* comment = m_provider_id == "youtube" ? tube_comment_text(s.id) : "";
        r.overview = display_text(*comment ? comment : s.overview);
        if (s.tube_interface) {
            /* A row the provider made from its own texts. Its note is matched as a whole only: it may hold content. */
            r.title = Rml::String(tube_i18n_string(r.title.c_str(), TUBE_I18N_LABELS).c_str());
            r.subtitle = Rml::String(tube_i18n_string(r.subtitle.c_str(), TUBE_I18N_LABELS).c_str());
            if (!*comment) r.overview = Rml::String(tube_i18n_string(r.overview.c_str(), 0).c_str());
        }
''')
    replace(screen, LOADING, '''    if (!status.empty()) {
        m_model.status = Rml::String(tube_i18n_string(status, TUBE_I18N_LABELS).c_str());''')
    replace(screen, PAGE, '''    if (m_provider_id == "youtube") m_model.page_info = Rml::String(tube_i18n_string(pbuf, 0).c_str());
    else m_model.page_info = Rml::String(pbuf);''')
    replace(screen, MODEL, MODEL + '''
    c.BindFunc("i18n", [](Rml::Variant& value) { value = (int)(tube_i18n_generation ? tube_i18n_generation() : 0); });
    c.RegisterTransformFunc("tr", [](const Rml::VariantList& arguments) -> Rml::Variant {
        if (arguments.empty()) return Rml::Variant();
        return Rml::Variant(Rml::String(tube_i18n_string(arguments[0].Get<Rml::String>(), 0).c_str()));
    });''')
    replace(screen, TICK, TICK + '''
    static unsigned tube_i18n_seen;
    if (tube_i18n_generation && tube_i18n_seen != tube_i18n_generation() && m_model_handle) {
        tube_i18n_seen = tube_i18n_generation();
        m_model_handle.DirtyVariable("i18n");
        m_dirty = true;
    }''')
    document = work / player / "assets/rml/youtube.rml"
    text = document.read_text(encoding="utf-8")
    if "| tr(i18n) }}" not in text:
        document.write_text(rml_localize(text), encoding="utf-8")
    # A key line can be longer than the page (a long language, a large interface scale): it takes a
    # second line above the first instead of leaving the page. One line keeps the size it had. The
    # rule follows the sheet the interface preferences restyle, so it holds at every scale.
    style = work / player / "assets/rml/youtube.rcss"
    rules = style.read_text(encoding="utf-8")
    if "/* interface languages */" not in rules:
        style.write_text(rules.rstrip("\n") + "\n" + RCSS, encoding="utf-8")
    # The player interface: every line the kit draws passes through ui::text().
    fonts = "third_party/ps5-homebrew-ui/src/ui/fonts.hpp"
    if "tube_i18n_kit" in (work / fonts).read_text(encoding="utf-8"):
        return      # the kit sources are unpacked once and keep the hook
    replace(fonts, KIT, '''    char tube_i18n_text[256];
    if (tube_i18n_kit && value.size() < sizeof tube_i18n_text) {
        const std::string tube_i18n_source(value);
        if (tube_i18n_kit(tube_i18n_source.c_str(), tube_i18n_text, sizeof tube_i18n_text)) {
            // A layout made for the English line keeps its width: a longer translation is drawn smaller.
            const float wanted = font.font->measure(value, size, tracking), actual = font.font->measure(tube_i18n_text, size, tracking);
            const float scale = actual > wanted && actual > 0.0f ? wanted / actual : 1.0f;
            if (scale >= 0.72f)
                return list.text(*font.font, font.texture, tube_i18n_text, x, baseline, size * scale, color, align, tracking * scale);
        }
    }
''' + KIT)
    replace(fonts, "namespace hui::ui\n{", '''extern "C" int tube_i18n_kit(const char *source, char *out, std::size_t capacity) __attribute__((weak));

namespace hui::ui
{''')
