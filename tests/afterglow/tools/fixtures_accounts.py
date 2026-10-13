#!/usr/bin/env python3
"""Writes the fixture catalogs of the accounts, sign_in and link_phone screens.

A file that exists is kept unless --force is given.

usage: python3 tests/afterglow/tools/fixtures_accounts.py tests/afterglow/fixtures [--force]
"""
import json
import os
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "tests/afterglow/fixtures"
MARA = "oauth_4d61726100000001"
JONAS = "oauth_4a6f6e6173000002"
LINA = "oauth_6c696e6100000003"
NOTE = "Experimental: tested offline only, not yet on a PS5 or against live YouTube. It may fail or change in a later version."


def name_of(catalog):
    return (re.sub(r"[^A-Za-z0-9_-]", "_", catalog) if catalog else "root") + ".json"


def write(fixture, catalog, rows=None, error=None, more=False):
    folder = os.path.join(ROOT, fixture, "catalog")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name_of(catalog))
    if os.path.exists(path) and "--force" not in sys.argv:
        print("kept", path)
        return
    body = {"error": error} if error is not None else {"items": rows, "has_more": more}
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write("{\n")
        if error is not None:
            out.write(' "error": ' + json.dumps(error, ensure_ascii=False) + "\n")
        else:
            out.write(' "items": [\n')
            for i, row in enumerate(rows):
                out.write("  " + json.dumps(row, ensure_ascii=False) + ("," if i + 1 < len(rows) else "") + "\n")
            out.write(" ],\n")
            out.write(' "has_more": ' + ("true" if more else "false") + "\n")
        out.write("}\n")
    print("wrote", path)


def profile(fixture, ident, name, guest, signed_in):
    folder = os.path.join(ROOT, fixture)
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "profile.json")
    if os.path.exists(path):
        print("kept", path)
        return
    with open(path, "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps({"id": ident, "name": name, "guest": guest, "signed_in": signed_in}, ensure_ascii=False) + "\n")
    print("wrote", path)


def folder(ident, title, subtitle="", **facts):
    row = {"id": ident, "title": title, "subtitle": subtitle, "is_folder": True, "interface": True}
    row.update(facts)
    return row


def who(ident, name, selected=False, locked=False, subscriptions=None, art=None):
    guest = ident == "guest"
    row = {"id": "library:profile:" + ident, "title": name, "subtitle": "Browse as guest" if guest else "Account signed in on this PS5",
           "is_folder": True, "guest": guest, "signed_in": not guest, "selected": selected, "locked": locked}
    if subscriptions is not None:
        row["subscriptions"] = subscriptions
    if art:
        row["art_url"] = art
    return row


def library(name, account):
    rows = [folder("library:profiles", "Profile: " + name, "Guest, or an account signed in on this PS5"),
            folder("library:home", "Home", "Recommendations for the selected profile"),
            folder("library:subscriptions", "Subscriptions", "Videos from your YouTube subscriptions"),
            folder("library:history", "YouTube history", "History from the selected account"),
            folder("library:watch_later", "Watch later", "Videos saved to your account"),
            folder("library:playlists", "Your playlists", "Playlists from the selected account"),
            folder("library:channels", "Subscribed channels", "Channels you subscribe to"),
            folder("library:notifications", "Notifications", "Videos your account was notified about"),
            folder("auth:start", "Sign in to YouTube", "Authorize on your phone or computer")]
    if account:
        rows.append(folder("account:remove", "Remove selected account", "Review before removing its saved sign-in"))
    return rows


def spaced(code):
    return " ".join(code[i:i + 3] for i in range(0, len(code), 3))


def remote(state, code="", on=None, devices=(), disconnect="keep playing", history=True, available=True, words=None):
    on = state != "off" if on is None else on
    rows = [folder("settings:remote:info", "Phone connection: " + (words or state), "YouTube app TV-code pairing (experimental)",
                   overview=NOTE, state=state, value=words or state, available=available)]
    if code:
        rows.append(folder("settings:remote:code", "TV code: " + spaced(code), "Enter this code in the YouTube app's TV-code pairing screen", value=code))
    rows.append(folder("settings:remote:pair", "Get a new TV code" if code else "Link a phone", "Enable connection and request a TV code", kind="action"))
    rows.append(folder("settings:remote:toggle", "Turn phone connection off" if on else "Turn phone connection on", "Your local library stays available",
                       kind="switch", selected=on))
    rows.append(folder("settings:remote:reset", "Reset phone pairing", "Turn off and forget this TV's pairing identity", kind="action"))
    rows.append(folder("settings:remote:disconnect", "When phone disconnects: " + disconnect,
                       "Close ProsperoTube when a linked phone disconnects, including while browsing" if disconnect == "close the app"
                       else "Stop applies to phone-started playback; disconnections may happen when the phone sleeps", kind="list", value=disconnect))
    rows.append(folder("settings:remote:history", "History for phone playback: " + ("on" if history else "off"),
                       "Off records no local history and sends no account reports for videos started from the phone", kind="switch", selected=history))
    for index, (name, detail) in enumerate(devices):
        rows.append({"id": "settings:remote:device:%d" % (index + 1), "title": name, "subtitle": detail, "is_folder": True, "connected": True})
    return rows


def pending(code, url, expires):
    return [folder("auth:info:code", code, "Enter this code on your phone or computer", state="pending", user_code=code, verification_url=url, expires_in=expires,
                   overview="Open %s on your phone or computer and enter %s. Waiting for authorization; status checks are automatic. The code expires automatically if sign-in is not completed." % (url, code)),
            folder("auth:info:url", url, "Open this address to authorize YouTube"),
            folder("auth:cancel", "Cancel sign-in", "Circle also cancels when leaving sign-in")]


def ended(state, title):
    return [folder("auth:info", title, "Account credentials stay in the app data on this PS5", state=state),
            folder("auth:start", "Start a new sign-in", "Authorize using your phone or computer")]


def authorized(ident, name):
    return [folder("library:profile:" + ident, "Use authorized YouTube account", name, state="authorized", name=name),
            folder("library:profiles", "All profiles", "Account saved in the app data on this PS5")]


PASSWORD = [folder("account:notice", "Enter the password of this profile", "Circle returns without selecting it", prompt="password")]
GOOGLE = "https://www.google.com/device"

# ---- signed-in: Mara is watching; Jonas keeps a password; one phone is linked ----------
profile("signed-in", MARA, "Mara", False, True)
write("signed-in", "library:profiles", [who(MARA, "Mara", selected=True, subscriptions="184"), who(JONAS, "Jonas", locked=True, subscriptions="57"),
                                        who("guest", "Guest")])
write("signed-in", "library", library("Mara", True))
write("signed-in", "library:profile:" + MARA, library("Mara", True))
write("signed-in", "library:profile:" + JONAS, PASSWORD)
write("signed-in", "library:profile:guest", library("Guest", False))
write("signed-in", "prompt:answer", library("Jonas", True))
write("signed-in", "account:remove", [folder("account:apply:7", "Remove selected account", "Cross confirms; Circle returns without removing it",
                                             overview="Profile: Mara. Remove this account and its saved sign-in from ProsperoTube. Continue as Guest; other accounts remain available.")])
write("signed-in", "account:apply:7", [folder("account:notice", "Account removed", "Select another profile or continue as Guest")])
for verb in ("set", "change", "remove"):
    write("signed-in", "settings:lock:" + verb, [folder("account:notice", "Enter the password", "Circle returns to the account settings without changing it", prompt="password")])
PHONE = [("Mara's phone", "Linked on 12 September · last used today")]
write("signed-in", "settings:remote", remote("ready", "482913057261", devices=PHONE))
write("signed-in", "settings:remote:pair", remote("ready", "905117362448", devices=PHONE))
write("signed-in", "settings:remote:toggle", remote("off"))
write("signed-in", "settings:remote:disconnect", remote("ready", "482913057261", devices=PHONE, disconnect="stop playback"))
write("signed-in", "settings:remote:history", remote("ready", "482913057261", devices=PHONE, history=False))
write("signed-in", "settings:remote:reset", remote("off"))
write("signed-in", "auth:start", pending("QDRK-VXNP", GOOGLE, 812))
write("signed-in", "auth:session", pending("QDRK-VXNP", GOOGLE, 810))
write("signed-in", "auth:cancel", ended("canceled", "Sign-in canceled"))

# ---- guest: nobody has signed in; the phone connection is off -------------------------------
write("guest", "library:profiles", [who("guest", "Guest", selected=True)])
write("guest", "library", library("Guest", False))
write("guest", "library:profile:guest", library("Guest", False))
write("guest", "settings:remote", remote("off"))
write("guest", "settings:remote:pair", remote("connecting"))
write("guest", "settings:remote:toggle", remote("connecting"))
write("guest", "auth:start", pending("QDRK-VXNP", GOOGLE, 900))
write("guest", "auth:session", pending("QDRK-VXNP", GOOGLE, 898))
write("guest", "auth:cancel", ended("canceled", "Sign-in canceled"))

# ---- guest-only: a build whose service neither signs in nor links phones -------------------------
profile("guest-only", "guest", "", True, False)
write("guest-only", "library:profiles", [who("guest", "Guest", selected=True)])
write("guest-only", "library", [folder("account:notice", "Sign-in to YouTube arrives in a later version", "This version browses and plays as Guest, without an account"),
                                 folder("account:notice", "Your library on this PS5 needs no account", "History, favorites, the play queue and followed channels are in the main menu"),
                                 folder("library:home", "Home", "What YouTube recommends without an account")])
write("guest-only", "settings:remote", remote("off", available=False))

# ---- empty: the list of profiles answers with nothing -----------------------------------------------
profile("empty", "guest", "", True, False)
write("empty", "library:profiles", [])
write("empty", "library", library("Guest", False))
write("empty", "settings:remote", remote("off"))

# ---- long-texts: long names, big numbers, more profiles than fit, a long code -------------------------
LONG = "Maximilian-Alexander von Hohenstein-Wittgenstein"
profile("long-texts", MARA, LONG, False, True)
write("long-texts", "library:profiles", [
    who(MARA, LONG, selected=True, locked=True, subscriptions="12,480"),
    who(JONAS, "Wohnzimmer-Fernsehgemeinschaft Nordstadt", subscriptions="1"),
    who(LINA, "Anastasia Konstantinopoulou-Papadimitriou", locked=True),
    who("oauth_6b696400000000a4", "Kinderprofil (Wochenende und Ferien)", subscriptions="3,206"),
    who("oauth_6f6d6100000000a5", "Oma Hildegard", subscriptions="9"),
    who("guest", "Guest")])
write("long-texts", "library", library(LONG, True))
write("long-texts", "settings:remote", remote("ready", "482913057", words="ready", devices=[
    ("Maximilian-Alexanders Arbeitstelefon im Wohnzimmer (zweites Gerät)", "Verknüpft am 12. September · zuletzt heute Nachmittag verwendet"),
    ("Tablet im Kinderzimmer", ""), ("Jonas' phone", "")], disconnect="close the app"))
write("long-texts", "auth:start", pending("ABCD-EFGH-JKLM", "https://www.youtube.com/activate", 59))
write("long-texts", "auth:session", pending("ABCD-EFGH-JKLM", "https://www.youtube.com/activate", 57))
write("long-texts", "auth:cancel", ended("canceled", "Sign-in canceled"))

# ---- the ways a sign-in ends: the code is shown first, the next look says how it went -----------------
write("sign-in-approved", "auth:start", pending("QDRK-VXNP", GOOGLE, 812))
write("sign-in-approved", "auth:session", authorized(LINA, "Lina"))
write("sign-in-approved", "library:profile:" + LINA, library("Lina", True))
for state, title in (("denied", "YouTube authorization denied"), ("expired", "Sign-in code expired"), ("canceled", "Sign-in canceled")):
    write("sign-in-" + state, "auth:start", pending("QDRK-VXNP", GOOGLE, 812))
    write("sign-in-" + state, "auth:session", ended(state, title))
write("sign-in-failed", "auth:start", error="The sign-in response was invalid.")

# ---- phone-linking: the connection is on and YouTube has not answered with a code yet ------------------
profile("phone-linking", MARA, "Mara", False, True)
write("phone-linking", "settings:remote", remote("connecting"))

# ---- long-texts keeps a password it does not accept: what a wrong one looks like -------------------------
write("long-texts", "settings:lock:change", [folder("account:notice", "Enter the password", "Circle returns to the account settings without changing it", prompt="password")])
write("long-texts", "prompt:answer", error="Wrong password.")

# ---- protected: whoever is watching keeps a password ------------------------------------------------------
profile("protected", JONAS, "Jonas", False, True)
write("protected", "library:profiles", [who(JONAS, "Jonas", selected=True, locked=True), who("guest", "Guest")])
write("protected", "library", library("Jonas", True))
write("protected", "settings:remote", remote("off"))
for verb in ("change", "remove"):
    write("protected", "settings:lock:" + verb, [folder("account:notice", "Enter the password", "Circle returns to the account settings without changing it", prompt="password")])
write("protected", "prompt:answer", [folder("account:notice", "Enter the password", "Circle returns to the account settings without changing it")])

# ---- broken: nothing answers (every catalog fails) -------------------------------------------------------
profile("broken", "guest", "", True, False)
