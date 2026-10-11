// Original JSON-lines process boundary. SPDX-License-Identifier: GPL-3.0-or-later
import { Innertube, Log } from 'youtubei.js';
import { StringDecoder } from 'node:string_decoder';
import { Library, LibraryError } from './library.mjs';
import { DeviceAuthorization, createProfileClient } from './device_auth.mjs';
import { proxyFetch, proxySettings } from './proxy_fetch.mjs';

Log.setLevel(Log.Level.NONE);
const allowed = ['youtube.com', 'google.com', 'googleapis.com', 'gstatic.com'];
// A proxy configured by the host carries every request. A malformed setting
// stops the worker instead of connecting directly.
let viaProxy = null;
if (process.env.PROSPEROTUBE_PROXY) {
  try { viaProxy = proxyFetch(proxySettings(process.env.PROSPEROTUBE_PROXY), LibraryError); }
  catch { process.exit(3); }
  delete process.env.PROSPEROTUBE_PROXY;
}
async function scopedFetch(input, init = {}) {
  const request = new Request(input, init);
  const url = new URL(request.url);
  if (url.protocol !== 'https:' || url.username || url.password || url.port ||
      !allowed.some(host => url.hostname === host || url.hostname.endsWith(`.${host}`)))
    throw new LibraryError('unsupported_origin', 'The service returned an unsupported origin.', 502);
  const signal = request.signal ? AbortSignal.any([request.signal, AbortSignal.timeout(15000)]) : AbortSignal.timeout(15000);
  // Return redirects as errors instead of forwarding account headers to a new
  // origin. Ordinary InnerTube operations do not require a cross-origin redirect.
  const response = viaProxy ? await viaProxy(request, signal) : await fetch(request, { signal, redirect: 'manual' });
  if (response.status >= 300 && response.status < 400)
    throw new LibraryError('upstream_redirect', 'The service requested an unsupported redirect.', 502);
  if (!response.body) return response;
  const reader = response.body.getReader();
  let received = 0;
  const body = new ReadableStream({
    async pull(controller) {
      const chunk = await reader.read();
      if (chunk.done) { controller.close(); return; }
      received += chunk.value.byteLength;
      if (received > 8 * 1024 * 1024) {
        await reader.cancel();
        controller.error(new LibraryError('upstream_limit', 'The service response exceeds the size limit.', 502));
      } else controller.enqueue(chunk.value);
    },
    cancel(reason) { return reader.cancel(reason); }
  });
  return new Response(body, { status: response.status, statusText: response.statusText, headers: response.headers });
}
async function createClient(profile, locale) {
  return createProfileClient(profile, Innertube, scopedFetch,
    (id, credentials) => credentialUpdates.set(id, credentials), locale);
}
const credentialUpdates = new Map();
const deviceAuth = new DeviceAuthorization(() => Innertube.create({ client_type: 'TVHTML5',
  retrieve_player: false, retrieve_innertube_config: false, generate_session_locally: true, fetch: scopedFetch }), scopedFetch);
let library = null;
let buffer = '';
const decoder = new StringDecoder('utf8');
for await (const chunk of process.stdin) {
  buffer += decoder.write(chunk);
  if (Buffer.byteLength(buffer) > 1048576) process.exit(2);
  let newline;
  while ((newline = buffer.indexOf('\n')) >= 0) {
    const line = buffer.slice(0, newline); buffer = buffer.slice(newline + 1);
    let id = '';
    let output;
    try {
      const input = JSON.parse(line);
      if (!/^[A-Za-z0-9_-]{1,64}$/.test(input.id ?? '')) throw new LibraryError('invalid_request', 'Invalid request identity.');
      id = input.id;
      if (input.operation === 'init') {
        if (library) throw new LibraryError('already_initialized', 'The worker is already initialized.');
        library = new Library(createClient, input.profiles ?? []);
        output = { ready: true };
      } else {
        if (!library) throw new LibraryError('not_initialized', 'Initialize the worker first.');
        if (Buffer.byteLength(line) > (input.operation === 'device' ? 16384 : 8192)) throw new LibraryError('request_limit', 'The operation exceeds the request limit.');
        if (input.operation === 'device') output = await deviceAuth.execute(input.request);
        else if (input.operation === 'playback') output = await library.playback(input.request);
        else if (input.operation === 'execute') output = await library.execute(input.request);
        else throw new LibraryError('invalid_request', 'The worker operation is invalid.');
      }
      const updates = Array.from(credentialUpdates, ([profile, credentials]) => ({ profile, credentials }));
      const serialized = JSON.stringify({ id, result: output, ...(updates.length ? { credential_updates: updates } : {}) });
      if (Buffer.byteLength(serialized) > 1048576) throw new LibraryError('response_limit', 'The returned operation exceeds the response limit.', 502);
      process.stdout.write(serialized + '\n');
      credentialUpdates.clear();
    } catch (error) {
      // Never serialize upstream exceptions, cookies, endpoints, or stack traces.
      const known = error instanceof LibraryError;
      const updates = Array.from(credentialUpdates, ([profile, credentials]) => ({ profile, credentials }));
      process.stdout.write(JSON.stringify({ id, ...(updates.length ? { credential_updates: updates } : {}), error: {
        code: known ? error.code : 'upstream_failure',
        message: known ? error.message : 'YouTube could not complete this operation.',
        status: known ? error.status : 502
      } }) + '\n');
      credentialUpdates.clear();
    }
  }
}
