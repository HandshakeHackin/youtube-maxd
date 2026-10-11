// The youtubei.js platform for the embedded engine. SPDX-License-Identifier: GPL-3.0-or-later
import './host_shims.js';
import './url_shim.js';
import { fetch, Request, Response, Headers } from './fetch_shim.js';
// Utils.js first, exactly as youtubei.js' own platform entries do: its modules
// import each other in cycles and only this starting point orders them right.
import { Platform } from 'youtubei-utils';

const host = globalThis.__host;
export const cacheDirectory = () => host.getenv('TUBE_JS_CACHE_DIR') ||
  `${host.getenv('HOME') || '.'}/.cache/prosperotube/tube_js`;

// One file per key under the cache directory (player analysis, session data,
// later the account credentials).
class Cache {
  #directory;
  constructor(_persistent = false, directory) { this.#directory = directory || cacheDirectory(); }
  get cache_dir() { return this.#directory; }
  #path(key) { return `${this.#directory}/${String(key).replace(/[^A-Za-z0-9_.-]/g, '_')}`; }
  async get(key) { return host.readFile(this.#path(key)); }
  async set(key, value) { host.writeFile(this.#path(key), value); }
  async remove(key) { host.removeFile(this.#path(key)); }
}

const unsupported = name => class { constructor() { throw new Error(`${name} is not available in the embedded engine.`); } };

Platform.load({
  runtime: 'unknown',
  server: true,          // send our own User-Agent and Origin, as the Node platform does
  info: { version: host.engine },
  Cache,
  sha1Hash: async text => host.sha1(text),
  uuidv4: () => globalThis.crypto.randomUUID(),
  // The extracted player functions run in a fresh context of the same engine:
  // code that came from the network never sees `__host`.
  eval: async data => JSON.parse(host.evalIsolated(`JSON.stringify((function(){${data.output}\n})())`)),
  fetch, Request, Response, Headers,
  FormData: unsupported('FormData'),
  File: unsupported('File'),
  ReadableStream: unsupported('ReadableStream'),
  CustomEvent: globalThis.CustomEvent
});
