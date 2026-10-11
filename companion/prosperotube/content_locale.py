"""Content language/country allowlists; the worker repeats this validation."""
from .models import ServiceError

# Request tag -> YouTube interface language, from the SmartTube reference
# (languages.xml plus its values-* folders). An empty value is accepted but
# keeps the service default: YouTube has no such interface language.
LANGUAGES = {tag: tag for tag in (
    "ar az bg bn ca cs da de el en es et fa fi fr gl hi hr hu hy id it ja ko lt lv nl pl pt-PT ro ru sk sl sq sr sv "
    "te th tr uk vi zh-CN zh-TW").split()}
LANGUAGES.update({"en-NZ": "en-GB", "he": "iw", "mo": "ro", "nb": "no", "pt-BR": "pt", "tjl": ""})
COUNTRIES = frozenset(
    "US GB HR CA UA RU BY KZ AZ AM RS EE BG DE RO ES PL FI LV FR HU NL BE NO CZ IT GR DK LT MD SK SI SE SG NZ NG AU "
    "CH IL SA MA BD BR PT AR MX HK MY JP IN KE TR EG VN CN TW ID KR PK BO CL CO CR EC SV GT HN NI PA PR PY PE DO UY "
    "VE IE PH IQ AT TZ NP ZA ZW DZ TH MK TN LK JO".split())


def valid_locale(hl, gl):
    return (isinstance(hl, str) and isinstance(gl, str) and (not hl or hl in LANGUAGES)
            and (not gl or gl in COUNTRIES))


def check_locale(hl, gl):
    """Pydantic validators raise ValueError; query parameters use locale_options."""
    if not valid_locale(hl, gl):
        raise ValueError("Select a supported content language and country.")


def locale_options(hl="", gl=""):
    """Only non-default values travel on, so earlier extractors keep their call shape."""
    if not valid_locale(hl, gl):
        raise ServiceError("invalid_locale", "Select a supported content language and country.", 400)
    return {key: value for key, value in (("hl", hl), ("gl", gl)) if value}


def extractor_language(hl, supported):
    """yt-dlp accepts only its own case-sensitive list; anything else stays default."""
    language = LANGUAGES.get(hl, "") if isinstance(hl, str) else ""
    return language if language and language in supported else ""
