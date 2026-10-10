"""Bounded, session-owned copies of completely downloaded HLS resources."""
from collections import OrderedDict
from dataclasses import dataclass
import time


@dataclass(frozen=True)
class Buffered:
    url: str
    data: bytes
    mime: str


class HLSBuffer:
    def __init__(self, sessions, *, capacity=64 * 1024 * 1024,
                 item_limit=8 * 1024 * 1024, count_limit=256, staging_limit=4):
        self.sessions = sessions
        self.capacity, self.item_limit = capacity, item_limit
        self.count_limit, self.staging_limit = count_limit, staging_limit
        self.items = OrderedDict()
        self.size = 0
        self.staging = 0

    def _valid(self, token, key, url):
        session = self.sessions.sessions.get(token)
        remote = session.tracks.get(key) if session else None
        return (session is not None and session.deadline > time.monotonic()
                and remote is not None and remote.url == url)

    def prune(self):
        for (token, key), item in list(self.items.items()):
            if not self._valid(token, key, item.url):
                self.size -= len(self.items.pop((token, key)).data)

    def get(self, token, key, remote):
        self.prune()
        identity = (token, key)
        item = self.items.get(identity)
        if item is not None and item.url == remote.url:
            self.items.move_to_end(identity)
            return item
        return None

    def begin(self, token, key, remote, reply):
        # Partial/ranged, encoded and playlist responses cannot establish a
        # complete byte representation. Concurrent staging is bounded too.
        length = reply.headers.get('content-length')
        if (not key.startswith('hls-') or key.endswith('.m3u8')
                or reply.status_code != 200
                or 'content-range' in reply.headers
                or reply.headers.get('content-encoding', 'identity').lower() != 'identity'
                or 'mpegurl' in reply.headers.get('content-type', remote.mime).lower()
                or self.staging >= self.staging_limit):
            return None
        if length is not None and (len(length) > 20 or not length.isascii() or not length.isdigit()
                                   or not 0 < int(length) <= self.item_limit):
            return None
        self.staging += 1
        return Capture(self, token, key, remote.url,
                       reply.headers.get('content-type', remote.mime),
                       int(length) if length is not None else None)

    def store(self, token, key, item):
        self.prune()
        if not self._valid(token, key, item.url) or not 0 < len(item.data) <= self.capacity:
            return
        previous = self.items.pop((token, key), None)
        if previous is not None:
            self.size -= len(previous.data)
        while self.items and (self.size + len(item.data) > self.capacity
                              or len(self.items) >= self.count_limit):
            _, removed = self.items.popitem(last=False)
            self.size -= len(removed.data)
        self.items[(token, key)] = item
        self.size += len(item.data)

    def clear(self):
        self.items.clear()
        self.size = 0


class Capture:
    def __init__(self, owner, token, key, url, mime, expected):
        self.owner, self.token, self.key = owner, token, key
        self.url, self.mime, self.expected = url, mime, expected
        self.data = bytearray()
        self.closed = False

    def feed(self, chunk):
        if self.closed:
            return
        if len(self.data) + len(chunk) > self.owner.item_limit:
            self.close()
        else:
            self.data.extend(chunk)

    def finish(self):
        if not self.closed:
            if self.expected is None or len(self.data) == self.expected:
                self.owner.store(self.token, self.key,
                                 Buffered(self.url, bytes(self.data), self.mime))
            self.close()

    def close(self):
        if not self.closed:
            self.closed = True
            self.data.clear()
            self.owner.staging -= 1
