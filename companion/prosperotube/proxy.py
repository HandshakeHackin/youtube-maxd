"""One host-chosen web proxy for every outbound connection; its address is never reported."""
import re
from urllib.parse import quote, unquote, urlsplit

MESSAGE = "The proxy must be http://[user:password@]host[:port] or https://[user:password@]host[:port]."


def proxy_url(value):
    """Validate and normalize a proxy address. Errors never repeat the value (it may hold a password).

    SOCKS is refused: the media relay's HTTP client has no SOCKS support here, and
    media addresses are bound to the address that resolved them, so every
    component has to leave through the same proxy.
    """
    try:
        parts = urlsplit(value)
        port = parts.port
        host = (parts.hostname or "").lower()
    except (ValueError, TypeError, AttributeError):
        raise ValueError(MESSAGE) from None
    literal = ":" in host
    if (parts.scheme not in ("http", "https") or len(value) > 2048 or re.search(r"[\x00-\x20\x7f]", value)
            or parts.path not in ("", "/") or parts.query or parts.fragment or "#" in value or "?" in value
            or not (re.fullmatch(r"[0-9a-f:.]{2,45}", host) if literal else
                    re.fullmatch(r"[a-z0-9](?:[a-z0-9.-]{0,251}[a-z0-9])?", host))
            or port == 0):
        raise ValueError(MESSAGE)
    credentials = ""
    if parts.username is not None or parts.password is not None:
        user, password = unquote(parts.username or ""), unquote(parts.password or "")
        if re.search(r"[\x00-\x1f\x7f]", user + password):
            raise ValueError(MESSAGE)
        credentials = quote(user, safe="") + ":" + quote(password, safe="") + "@"
    port = port or (443 if parts.scheme == "https" else 80)
    return f"{parts.scheme}://{credentials}{'[' + host + ']' if literal else host}:{port}"
