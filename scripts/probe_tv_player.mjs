// Public TV player characterization. No account, tokens, console or media download.
// SPDX-License-Identifier: GPL-3.0-or-later
import { Innertube, Log } from 'youtubei.js';
import { writeFile } from 'node:fs/promises';

Log.setLevel(Log.Level.NONE);
const [video, output] = process.argv.slice(2);
if (!/^[A-Za-z0-9_-]{11}$/.test(video ?? '') || !output) process.exit(2);
let requests = 0;
const fetchTV = async (input, init = {}) => {
  const request = new Request(input, init);
  const url = new URL(request.url);
  if (url.protocol !== 'https:' || url.hostname !== 'www.youtube.com' || url.port || url.username || url.password)
    throw new Error('origin');
  requests++;
  const response = await fetch(request, { signal: AbortSignal.timeout(15000), redirect: 'manual' });
  if (response.status >= 300 && response.status < 400) throw new Error('redirect');
  const reader = response.body?.getReader();
  const chunks = []; let size = 0;
  if (reader) for (;;) {
    const chunk = await reader.read(); if (chunk.done) break;
    size += chunk.value.byteLength;
    if (size > 8 * 1024 * 1024) { await reader.cancel(); throw new Error('limit'); }
    chunks.push(chunk.value);
  }
  return new Response(Buffer.concat(chunks), { status: response.status, headers: response.headers });
};
const report = { video, account_authorized: false, token_poll_requested: false, console_traffic: false };
try {
  const client = await Innertube.create({ client_type: 'TVHTML5', retrieve_player: false,
    retrieve_innertube_config: false, generate_session_locally: true, fetch: fetchTV });
  const info = await client.getBasicInfo(video, { client: 'TV' });
  const formats = [...(info.streaming_data?.formats ?? []), ...(info.streaming_data?.adaptive_formats ?? [])];
  report.result = 'response'; report.status = info.playability_status?.status;
  report.formats = formats.length;
  report.direct_urls = formats.filter(f => f.url).length;
  report.signature_ciphers = formats.filter(f => f.signature_cipher || f.cipher).length;
  report.n_parameters = formats.filter(f => f.url && new URL(f.url).searchParams.has('n')).length;
  report.hls = !!info.streaming_data?.hls_manifest_url;
  report.dash = !!info.streaming_data?.dash_manifest_url;
  report.sabr = !!info.streaming_data?.server_abr_streaming_url;
} catch {
  report.result = 'failed'; report.code = 'tv_player_failed';
}
report.requests = requests;
await writeFile(output, JSON.stringify(report, null, 2) + '\n');
process.stdout.write(JSON.stringify(report) + '\n');
