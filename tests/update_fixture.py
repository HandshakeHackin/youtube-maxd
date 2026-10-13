"""Fixtures for the update tests: release archives, and a loopback server.

    update_fixture.py make DIR            writes DIR/www/*.zip and DIR/www/index.txt
    update_fixture.py serve DIR [CERT KEY]  serves DIR/www on 127.0.0.1, prints "PORT n"

The server answers byte ranges and logs every request (path and headers) as
one JSON line in DIR/requests.log. Path prefixes choose a behaviour:

    /files/NAME          the file; Range answered with 206
    /norange/NAME        the whole file whatever was asked
    /redirect/NAME       302 to /files/NAME (a relative Location)
    /redirect-abs/NAME   302 to the absolute address of /files/NAME
    /flaky/NAME          503 the first time each range is asked, then the file
    /slow/NAME           the file in small pieces with pauses
    /status/CODE/NAME    that status

A file DIR/mode holding "down" makes the server close every connection
without an answer. Nothing here reaches any host but 127.0.0.1.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import hashlib
import http.server
import json
import ssl
import sys
import time
import zipfile

TITLE = "PPSA99142"


def param(version, title=TITLE):
    return json.dumps({"titleId": title, "contentVersion": version, "localizedParameters": {"defaultLanguage": "en-US"}}, indent=2).encode()


def noise(length, seed):
    """Deterministic bytes that deflate only a little."""
    out, block = bytearray(), hashlib.sha256(seed.encode()).digest()
    while len(out) < length:
        block = hashlib.sha256(block).digest()
        out += block + b"padding-that-compresses " * 2
    return bytes(out[:length])


def entry(name, mode=0o100777, method=zipfile.ZIP_DEFLATED):
    info = zipfile.ZipInfo(name, date_time=(2026, 10, 13, 12, 0, 0))
    info.create_system = 3
    info.external_attr = mode << 16
    info.compress_type = method
    return info


def app_files(version, title=TITLE):
    return [(title + "/", None), (title + "/eboot.bin", noise(300000, "eboot" + version)),
            (title + "/lapy.elf", noise(40000, "lapy")), (title + "/sce_sys/", None),
            (title + "/sce_sys/param.json", param(version, title)), (title + "/sce_sys/icon0.png", noise(9000, "icon" + version)),
            (title + "/assets/fonts/a.ttf", noise(70000, "font")), (title + "/assets/new.txt", b"new in " + version.encode() + b"\n"),
            (title + "/assets/deep/er/empty.txt", b"")]


def write_zip(path, files, extra=()):
    with zipfile.ZipFile(path, "w") as archive:
        for name, body in list(files) + list(extra):
            if body is None:
                archive.writestr(entry(name, 0o040777, zipfile.ZIP_STORED), b"")
            elif isinstance(body, tuple):   # (mode, method, bytes)
                archive.writestr(entry(name, body[0], body[1]), body[2])
            else:
                archive.writestr(entry(name, method=zipfile.ZIP_STORED if name.endswith(".elf") else zipfile.ZIP_DEFLATED), body)


def patch(path, find, offset, change):
    data = bytearray(path.read_bytes())
    at = data.index(find)
    data[at + offset] = change(data[at + offset])
    path.write_bytes(data)


def make(root):
    www = root / "www"
    www.mkdir(parents=True, exist_ok=True)
    good = app_files("01.000.010")
    write_zip(www / "good.zip", good)
    write_zip(www / "wrongversion.zip", app_files("01.000.020"))
    write_zip(www / "wrongtitle.zip", app_files("01.000.010", "PPSA00001"))
    write_zip(www / "traversal.zip", good, [(TITLE + "/../evil.txt", b"x")])
    write_zip(www / "absolute.zip", good, [("/" + TITLE + "/evil.txt", b"x")])
    write_zip(www / "outside.zip", good, [("other/file.txt", b"x")])
    write_zip(www / "backslash.zip", good, [(TITLE + "/a\\b.txt", b"x")])
    write_zip(www / "symlink.zip", good, [(TITLE + "/link", (0o120777, zipfile.ZIP_STORED, b"eboot.bin"))])
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter("ignore")
        write_zip(www / "duplicate.zip", good, [(TITLE + "/assets/new.txt", b"again")])
    write_zip(www / "encrypted.zip", good)
    patch(www / "encrypted.zip", b"PK\x01\x02", 8, lambda byte: byte | 1)
    write_zip(www / "corrupt.zip", good)
    patch(www / "corrupt.zip", (TITLE + "/eboot.bin").encode(), 2000, lambda byte: byte ^ 0xFF)
    write_zip(www / "noeboot.zip", [item for item in good if not item[0].endswith("eboot.bin")])
    (www / "notazip.zip").write_bytes(noise(5000, "junk"))
    with (www / "index.txt").open("w") as index:
        for path in sorted(www.glob("*.zip")):
            body = path.read_bytes()
            index.write(f"{path.name} {len(body)} {hashlib.sha256(body).hexdigest()}\n")


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    root = None
    seen = set()

    def log_message(self, *arguments):
        pass

    def reply(self, status, body=b"", headers=()):
        self.send_response(status)
        for name, value in headers:
            self.send_header(name, value)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        with (self.root / "requests.log").open("a") as log:
            log.write(json.dumps({"path": self.path, "headers": {k.lower(): v for k, v in self.headers.items()}}) + "\n")
        mode = self.root / "mode"
        if mode.exists() and mode.read_text().strip() == "down":
            self.close_connection = True
            self.connection.close()
            return
        parts = self.path.split("?")[0].strip("/").split("/")
        kind, name = parts[0], parts[-1]
        if kind == "status" and len(parts) == 3:
            return self.reply(int(parts[1]), b"refused")
        if kind == "redirect":
            return self.reply(302, headers=[("Location", "/files/" + name)])
        if kind == "redirect-abs":
            scheme = "https" if isinstance(self.connection, ssl.SSLSocket) else "http"
            return self.reply(302, headers=[("Location", f"{scheme}://127.0.0.1:{self.server.server_port}/files/{name}")])
        path = self.root / "www" / name
        if kind not in ("files", "norange", "flaky", "slow") or len(parts) != 2 or not path.is_file():
            return self.reply(404, b"not found")
        body, wanted = path.read_bytes(), self.headers.get("Range")
        if kind == "flaky" and (self.path, wanted) not in self.seen:
            self.seen.add((self.path, wanted))
            return self.reply(503, b"try again")
        status, headers, first, last = 200, [("Content-Type", "application/octet-stream")], 0, len(body) - 1
        if wanted and kind != "norange":
            first, _, end = wanted.removeprefix("bytes=").partition("-")
            first = int(first)
            last = min(int(end), len(body) - 1) if end else len(body) - 1
            if first >= len(body):
                return self.reply(416, headers=[("Content-Range", f"bytes */{len(body)}")])
            status = 206
            headers.append(("Content-Range", f"bytes {first}-{last}/{len(body)}"))
        piece = body[first:last + 1]
        if kind != "slow":
            return self.reply(status, piece, headers)
        self.send_response(status)
        for header in headers:
            self.send_header(*header)
        self.send_header("Content-Length", str(len(piece)))
        self.end_headers()
        try:
            for at in range(0, len(piece), 4096):
                self.wfile.write(piece[at:at + 4096])
                self.wfile.flush()
                time.sleep(0.05)
        except OSError:
            self.close_connection = True


def serve(root, certificate=None, key=None):
    Handler.root = root
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    if certificate:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(certificate, key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
    print(f"PORT {server.server_port}", flush=True)
    server.serve_forever()


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "make":
        make(Path(sys.argv[2]))
    elif len(sys.argv) in (3, 5) and sys.argv[1] == "serve":
        serve(Path(sys.argv[2]), *sys.argv[3:5])
    else:
        sys.exit(__doc__)
