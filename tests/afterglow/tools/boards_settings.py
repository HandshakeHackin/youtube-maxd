#!/usr/bin/env python3
"""Writes tests/afterglow/boards/settings.json: one board per state of Settings worth a picture.

usage: python3 tests/afterglow/tools/boards_settings.py tests/afterglow/boards/settings.json
"""
import json
import sys

SEP = "\u001f"
# The "empty" set is a Guest's; these boards show it to a signed-in person who has nothing yet.
MARA = {"id": "mara", "name": "Mara", "guest": False, "signed_in": True}


def P(*keys):
    return [{"press": k} for k in keys]


def W(seconds):
    return [{"wait": seconds}]


def board(name, fixture="signed-in", argument="", title="", steps=None, expect=None, **more):
    route = {"screen": "settings"}
    if argument:
        route["argument"] = argument
    if title:
        route["title"] = title
    out = {"name": name, "fixture": fixture, "route": route}
    out.update(more)
    if steps:
        out["steps"] = steps
    out["expect"] = expect or {}
    out["expect"].setdefault("screen", "settings")
    return out


CAPTIONS = "tile:captions"
MENU = "Interface" + SEP + "Video actions menu"
DOWN5 = P("down", "down", "down", "down", "down")
boards = [
    # ---- the tiles (board 31) ----
    board("settings-home", steps=P("right"),
          expect={"focus": "tile/captions", "rail_open": 0, "state": {"page": "home", "load": "ready", "tiles": "11"}, "asked": ["settings"]}),
    board("settings-home-guest", fixture="guest", expect={"focus": "tile/player", "state": {"page": "home", "load": "ready", "tiles": "11"}}),
    board("settings-home-loading", source={"hold": True}, expect={"focus": "tile/player", "state": {"load": "loading"}}),
    board("settings-home-arriving", source={"delay": 30}, settle=0.62, expect={"focus": "tile/player", "state": {"load": "ready"}}),
    board("settings-home-offline", source={"offline": True}, steps=P("down", "right"), expect={"focus": "tile/sponsorblock", "state": {"load": "offline"}}),
    board("settings-home-empty", fixture="empty", profile=MARA, expect={"focus": "tile/player", "state": {"load": "ready", "tiles": "11"}}),
    board("settings-home-long", fixture="long-texts", steps=P("down", "right", "right"), expect={"focus": "tile/dearrow", "state": {"tiles": "11"}}),
    board("settings-home-edges", steps=P("up", "right", "right", "right", "right", "down", "down", "down"),
          expect={"focus": "tile/about", "rail_open": 0}),
    board("settings-home-rail", steps=P("left"), expect={"focus": "rail/settings", "rail_open": 1}),
    board("settings-home-more", fixture="newer-provider", steps=P("down", "down", "right", "right", "right") + W(0.6),
          expect={"focus": "tile/more", "state": {"tiles": "12", "scroll": "0"}}),
    board("settings-home-reset", steps=P("right", "options"), expect={"dialog": 1}),
    board("settings-home-reset-done", steps=P("right", "options", "cross") + W(0.8),
          expect={"dialog": 0, "focus": "tile/captions", "state": {"applied": "1", "last_applied": "settings:reset:captions"}, "asked": ["settings:reset:captions"]}),
    # ---- from a tile to its page and back ----
    board("settings-open-tile", steps=P("right", "cross") + W(0.7),
          expect={"focus": "row/captions", "moving": 0, "state": {"page": CAPTIONS, "load": "ready", "rows": "8"}, "asked": ["settings:extra:captions"]}),
    board("settings-open-tile-midway", steps=P("right") + [{"press": "cross", "wait": 0.15}], expect={"moving": 1, "state": {"page": CAPTIONS}}),
    board("settings-back-to-tiles", steps=P("right", "cross") + W(0.7) + P("down", "circle") + W(0.7),
          expect={"focus": "tile/captions", "moving": 0, "state": {"page": "home"}}),
    board("settings-phone-link", steps=P("down", "down", "cross") + W(0.7), expect={"screen": "link_phone"}),
    board("settings-accounts", argument="tile:accounts", expect={"focus": "row/link", "state": {"rows": "4", "kind": "link"}, "asked": ["settings:accounts"]}),
    board("settings-accounts-link", argument="tile:accounts", steps=P("cross") + W(0.7), expect={"screen": "accounts"}),
    board("settings-accounts-guest", fixture="guest", argument="tile:accounts", steps=P("down", "down", "down", "down"),
          expect={"focus": "row/account:notice", "state": {"rows": "5", "kind": "info"}}),
    # A Guest finds signing in first on the Accounts page, and it opens the sign-in screen.
    board("settings-accounts-guest-sign-in", fixture="guest", argument="tile:accounts",
          expect={"focus": "row/auth:start", "state": {"rows": "5", "kind": "link"}, "asked": ["settings:accounts"]}),
    board("settings-accounts-guest-sign-in-open", fixture="guest", argument="tile:accounts", steps=P("cross") + W(0.7), expect={"screen": "sign_in"}),
    # ---- the form (board 32) ----
    board("settings-captions", argument=CAPTIONS, steps=DOWN5,
          expect={"focus": "row/subtitle-size", "state": {"kind": "slider", "value": "120 %", "preview": "captions", "defaults": "1", "rows": "8"}}),
    board("settings-captions-size", argument=CAPTIONS, steps=DOWN5 + P("right") + W(0.6),
          expect={"focus": "row/subtitle-size", "state": {"value": "130 %", "applied": "1", "waiting": "0"}, "asked": ["settings:subtitle-size", "settings:subtitle-size:12"]}),
    board("settings-captions-size-smaller", argument=CAPTIONS, steps=DOWN5 + P("left") + W(0.6),
          expect={"state": {"value": "110 %", "applied": "1"}, "asked": ["settings:subtitle-size:10"]}),
    board("settings-captions-off", argument=CAPTIONS, steps=P("cross") + W(0.8),
          expect={"focus": "row/captions", "state": {"kind": "switch", "value": "off", "applied": "1"}, "asked": ["settings:captions"]}),
    board("settings-captions-waiting", argument=CAPTIONS, source={"delay": 45}, settle=3, steps=[{"press": "cross", "wait": 0.4}],
          expect={"state": {"value": "off", "waiting": "1", "applied": "0"}}),
    board("settings-captions-refused", argument=CAPTIONS, steps=P("down", "down", "cross") + W(0.5),
          expect={"focus": "row/automatic", "state": {"value": "on", "refused": "1", "last_refused": "settings:automatic", "waiting": "0"}}),
    board("settings-captions-defaults", argument=CAPTIONS, steps=P("triangle"), expect={"dialog": 1}),
    board("settings-captions-defaults-done", argument=CAPTIONS, steps=P("triangle", "cross") + W(0.8),
          expect={"dialog": 0, "focus": "row/captions", "state": {"value": "off", "applied": "1"}, "asked": ["settings:reset:captions"]}),
    board("settings-captions-edges", argument=CAPTIONS, steps=P("up", "left") + DOWN5 + P("down", "down", "down", "down"),
          expect={"focus": "row/subtitle-position", "rail_open": 0, "state": {"value": "Bottom"}}),
    board("settings-captions-long", fixture="long-texts", argument=CAPTIONS, steps=DOWN5,
          expect={"focus": "row/subtitle-size", "state": {"rows": "9", "preview": "captions"}}),
    board("settings-captions-newer", fixture="newer-provider", argument=CAPTIONS, steps=P("r2", "r2"),
          expect={"focus": "row/caption-dictionary", "state": {"rows": "10"}}),
    board("settings-captions-reduced", argument=CAPTIONS, reduced_motion=True, steps=DOWN5 + P("right") + W(0.4),
          expect={"focus": "row/subtitle-size", "state": {"value": "130 %"}}),
    # ---- a row's choices as a page ----
    board("settings-style-list", argument=CAPTIONS, steps=DOWN5 + P("down", "cross") + W(0.7),
          expect={"focus": "row/subtitle-style:2", "state": {"page": "settings:subtitle-style", "kind": "choice", "value": "on", "preview": "captions"}}),
    board("settings-style-try", argument=CAPTIONS, steps=DOWN5 + P("down", "cross") + W(0.7) + P("down", "down"),
          expect={"focus": "row/subtitle-style:4", "state": {"value": "off", "preview": "captions"}}),
    board("settings-style-chosen", argument=CAPTIONS, steps=DOWN5 + P("down", "cross") + W(0.7) + P("down", "cross") + W(1.3),
          expect={"focus": "row/subtitle-style", "moving": 0, "state": {"page": CAPTIONS}, "asked": ["settings:subtitle-style:3"]}),

    board("settings-country-more", argument="settings:content-country", title="Content & language" + SEP + "Content country",
          steps=P("r2", "r2", "r2") + W(0.5) + P("r2", "r2", "r2") + W(0.5), expect={"focus": "row/content-country:MA", "state": {"rows": "48", "scroll": "2872"}}),
    # A change on the second part of a long list: the row keeps the focus and the page its scroll while the list is read again.
    board("settings-buttons-more", argument="settings:extra:buttons", title="Player" + SEP + "Player buttons", steps=P("r2") + W(0.5) + P("r2") + W(0.5),
          expect={"focus": "row/button-next", "state": {"rows": "21", "scroll": "664"}}),
    board("settings-buttons-changed", argument="settings:extra:buttons", title="Player" + SEP + "Player buttons",
          steps=P("r2") + W(0.5) + P("r2") + W(0.5) + P("cross") + W(1.2),
          expect={"focus": "row/button-next", "moving": 0, "state": {"rows": "21", "scroll": "664", "applied": "1", "waiting": "0"}, "asked": ["settings:button-next"]}),
    board("settings-languages-long", fixture="long-texts", argument="settings:languages", title="Untertitel" + SEP + "Sprache der Untertitel", steps=P("down"),
          expect={"focus": "row/lang:nl-BE", "state": {"rows": "4"}}),
    # ---- the previews ----
    board("settings-interface", argument="tile:interface", expect={"focus": "row/ui:scheme", "state": {"preview": "theme", "value": "Afterglow"}}),
    board("settings-interface-theme", argument="tile:interface", steps=P("right") + W(0.6),
          expect={"state": {"preview": "theme", "value": "Onyx", "applied": "1"}, "asked": ["settings:ui:scheme:1"]}),
    board("settings-interface-card", argument="tile:interface", steps=P("down", "down", "right", "right") + W(0.6),
          expect={"focus": "row/ui:grid", "state": {"preview": "card", "value": "1.2x", "applied": "2"}}),
    board("settings-interface-corners", argument="tile:interface", steps=P("down", "down", "down", "right", "down", "down", "down", "down", "cross") + W(0.6),
          expect={"focus": "row/ui:rounded", "state": {"preview": "card", "value": "off"}}),
    board("settings-theme-list", argument="settings:ui:scheme", title="Interface" + SEP + "Theme", steps=P("down", "down"),
          expect={"focus": "row/ui:scheme:2", "state": {"preview": "theme", "rows": "5"}}),
    board("settings-theme-classic", argument="settings:ui:scheme:classic", title="Interface" + SEP + "Theme" + SEP + "Classic colours", steps=P("down"),
          expect={"focus": "row/ui:scheme:5", "state": {"preview": "theme", "rows": "9"}}),
    # ---- lists that can be arranged ----
    board("settings-order", argument="settings:menu", title=MENU, steps=P("down", "cross"),
          expect={"focus": "row/menu:dislike", "state": {"moving": "1", "kind": "order", "defaults": "1"}}),
    board("settings-order-moved", argument="settings:menu", title=MENU, steps=P("down", "cross", "up") + W(0.6),
          expect={"focus": "row/menu:dislike", "state": {"moving": "1", "applied": "1"}, "asked": ["settings:menu:dislike:up"]}),
    board("settings-order-refused", argument="settings:menu", title=MENU, steps=P("down", "down", "cross", "up") + W(0.6) + P("cross"),
          expect={"focus": "row/menu:unrate", "state": {"moving": "0", "refused": "1"}}),
    board("settings-order-hidden", argument="settings:menu", title=MENU, steps=P("down", "square") + W(0.6),
          expect={"focus": "row/menu:dislike", "state": {"value": "off", "applied": "1"}, "asked": ["settings:menu:dislike:toggle"]}),
    board("settings-sections", argument="settings:sections", title="Interface" + SEP + "Menu sections", steps=P("r2", "cross"),
          expect={"focus": "row/section:9", "state": {"moving": "1", "rows": "16"}}),
    # ---- every other kind of row ----
    board("settings-sponsorblock", argument="tile:sponsorblock", steps=P("down", "down", "cross") + W(0.6),
          expect={"focus": "row/sb:action:intro", "state": {"kind": "cycle", "value": "Skip silently", "applied": "1"}, "asked": ["settings:sb"]}),
    board("settings-text", argument="tile:sponsorblock", steps=P("r2", "r2", "cross") + W(0.5),
          expect={"focus": "sheet/keys", "state": {"typing": "1", "typed": "https://"}}),
    board("settings-text-typed", argument="tile:sponsorblock", steps=P("r2", "r2", "cross") + W(0.5) + P("cross") + W(0.1),
          expect={"focus": "sheet/keys", "state": {"typing": "1", "typed": "https://1"}}),
    board("settings-text-saved", argument="tile:sponsorblock", steps=P("r2", "r2", "cross") + W(0.5) + P("cross", "options") + W(0.6),
          expect={"focus": "row/sb:server", "state": {"typing": "0", "value": "sb.example.org", "applied": "1"}, "asked": ["settings:sb:server"], "answered": ["settings:sb:server=https://1"]}),
    board("settings-backup", argument="tile:backup", steps=P("cross") + W(0.7),
          expect={"focus": "row/backup:create", "state": {"applied": "1", "rows": "6"}, "asked": ["settings:backup:create"]}),
    board("settings-backup-list", argument="settings:backup:list", title="Backup" + SEP + "Restore or delete a backup",
          expect={"focus": "row/backup:entry:backup-20261013-0912", "state": {"rows": "3", "kind": "page"}}),
    board("settings-backup-entry", argument="settings:backup:entry:backup-20261013-0912", title="Backup" + SEP + "Today, 09:12", steps=P("down", "cross"),
          expect={"dialog": 1}),
    board("settings-clear-queue", argument="tile:content", steps=P("r2", "down", "down", "cross"), expect={"dialog": 1}),
    board("settings-clear-queue-done", argument="tile:content", steps=P("r2", "down", "cross", "cross") + W(0.8),
          expect={"dialog": 0, "focus": "row/clear-history", "state": {"applied": "1", "kind": "action"}, "asked": ["settings:clear-history"]}),
    board("settings-about", argument="tile:about", steps=P("down", "down"), expect={"focus": "row/about:credits", "state": {"rows": "4", "kind": "link"}}),
    board("settings-about-credits", argument="tile:about", steps=P("down", "down", "cross") + W(0.7), expect={"screen": "reader", "argument": "settings:about:credits"}),
    board("settings-audio-disabled", argument="tile:audio", steps=P("r2", "cross"), expect={"focus": "row/audio-delay-enabled", "state": {"value": "off", "waiting": "0"}}),
    board("settings-player", argument="tile:player", steps=P("r2", "r2", "r2"), expect={"state": {"load": "ready"}}),
    board("settings-hide", argument="settings:hide", title="Content & language" + SEP + "Hide content", steps=P("down", "down", "down", "cross") + W(0.3),
          expect={"focus": "row/hide:watched_home", "state": {"rows": "12", "defaults": "1", "applied": "1", "value": "off"}}),
    board("settings-more", fixture="newer-provider", argument="tile:more", expect={"focus": "row/picture-in-picture", "state": {"rows": "2"}}),
    # ---- nothing to show ----
    board("settings-page-loading", argument=CAPTIONS, source={"hold": True}, expect={"focus": "loading", "state": {"load": "loading"}}),
    board("settings-page-offline", argument=CAPTIONS, source={"offline": True}, expect={"focus": "action/retry", "state": {"load": "offline"}}),
    board("settings-page-failed", argument="settings:backup:import", title="Backup" + SEP + "Import subscriptions", steps=P("cross") + W(0.4),
          expect={"focus": "action/retry", "state": {"load": "failed"}}),
    board("settings-page-empty", fixture="empty", profile=MARA, argument="settings:hide", title="Content & language" + SEP + "Hide content",
          expect={"focus": "action/back", "state": {"load": "ready", "rows": "0"}}),
]

about = ("Settings: the eleven tiles (board 31), a tile's page as a form with its preview (board 32), the lists behind a row, "
         "changes on their way to the provider, confirmed and refused, and every state without rows.")
with open(sys.argv[1], "w", encoding="utf-8", newline="\n") as out:
    out.write('{\n "about": %s,\n "boards": [\n' % json.dumps(about))
    out.write(",\n".join("  " + json.dumps(b) for b in boards))
    out.write("\n ]\n}\n")
print(len(boards), "boards")
