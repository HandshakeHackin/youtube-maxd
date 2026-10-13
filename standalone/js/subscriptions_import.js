// Subscription exports of NewPipe, PocketTube and GrayJay as a bounded channel
// list: companion/prosperotube/subscriptions_import.py, ported. Pure parsing;
// nothing is stored and no account is touched.
// SPDX-License-Identifier: GPL-3.0-or-later
import { ServiceError, text } from './errors.js';

export const MAX_BYTES = 1024 * 1024, MAX_CHANNELS = 2048;
const CHANNEL = /^UC[A-Za-z0-9_-]{22}$/;
const invalid = () => new ServiceError('invalid_import', 'This is not a NewPipe, PocketTube or GrayJay subscriptions export.', 400);
const record = value => value !== null && typeof value === 'object' && !Array.isArray(value);

// Strict UTF-8 (no overlong forms, no surrogates), as Python's decoder.
function validUtf8(bytes) {
  for (let i = 0; i < bytes.length;) {
    const lead = bytes[i];
    if (lead < 0x80) { i++; continue; }
    const more = lead >= 0xC2 && lead <= 0xDF ? 1 : lead >= 0xE0 && lead <= 0xEF ? 2 : lead >= 0xF0 && lead <= 0xF4 ? 3 : 0;
    if (!more || i + more >= bytes.length) return false;
    const second = bytes[i + 1];
    const low = lead === 0xE0 ? 0xA0 : lead === 0xF0 ? 0x90 : 0x80, high = lead === 0xED ? 0x9F : lead === 0xF4 ? 0x8F : 0xBF;
    if (!(second >= low && second <= high)) return false;
    for (let k = 2; k <= more; k++) if ((bytes[i + k] & 0xC0) !== 0x80) return false;
    i += more + 1;
  }
  return true;
}

// A channel ID, or the ID inside a youtube.com/channel/UC... address; handles
// and user names are not resolved.
export function channelId(value) {
  if (typeof value !== 'string' || (value.length > 2048 && Array.from(value).length > 2048)) return '';
  if (CHANNEL.test(value)) return value;
  // What urlsplit() reads: scheme, "//" authority, path up to "?" or "#".
  const parts = /^(https?):\/\/([^/?#]*)([^?#]*)/i.exec(value.trim().replace(/[\t\r\n]/g, ''));
  if (!parts || /[[\]]/.test(parts[2])) return '';
  const hostname = parts[2].slice(parts[2].lastIndexOf('@') + 1).split(':')[0].toLowerCase();
  const match = /^\/channel\/(UC[A-Za-z0-9_-]{22})\/?$/.exec(parts[3]);
  return (hostname === 'youtube.com' || hostname.endsWith('.youtube.com')) && match ? match[1] : '';
}

// {"subscriptions": [{"service_id": 0, "url": ".../channel/UC...", "name": "..."}]}; service 0 is YouTube.
function newpipe(data) {
  const rows = record(data) ? data.subscriptions : null;
  if (!Array.isArray(rows)) return null;
  return rows.map(row => {
    const service = record(row) ? row.service_id : null;
    return [record(row) && (service === undefined || service === 0 || service === false) ? row.url : null, record(row) ? row.name : '', ''];
  });
}

// A list of channel addresses, or of subscription records (objects or JSON
// strings) with a channel address.
function grayjay(data) {
  if (!Array.isArray(data)) return null;
  return data.map(row => {
    if (typeof row === 'string' && row.trimStart().startsWith('{') && row.length <= 65536) {
      try { row = JSON.parse(row); } catch { row = null; }
    }
    if (!record(row)) return [row, '', ''];
    const channel = record(row.channel) ? row.channel : row;
    return [channel.url, channel.name, ''];
  });
}

// {"Group name": ["UC...", ...], ...}; `ysc_*` keys hold the extension's own settings.
function pockettube(data) {
  if (!record(data)) return null;
  const groups = Object.keys(data).filter(key => !key.startsWith('ysc_'));
  if (!groups.length || !groups.every(key => Array.isArray(data[key]))) return null;
  return groups.flatMap(group => data[group].map(entry => [entry, '', group]));
}

// The export file's bytes -> {"format", "channels": [{"channel_id", "title", "group"?}], "skipped", "truncated"}.
export function parseImport(raw) {
  let bytes = new Uint8Array(raw);
  if (bytes.length > MAX_BYTES) throw new ServiceError('import_too_large', 'The subscriptions file exceeds one mebibyte.', 413);
  if (!validUtf8(bytes)) throw invalid();
  if (bytes.length >= 3 && bytes[0] === 0xEF && bytes[1] === 0xBB && bytes[2] === 0xBF) bytes = bytes.subarray(3);
  // One byte-order mark is skipped (Python's utf-8-sig); a second one is not JSON.
  if (bytes.length >= 3 && bytes[0] === 0xEF && bytes[1] === 0xBB && bytes[2] === 0xBF) throw invalid();
  let data;
  try { data = JSON.parse(globalThis.__host.utf8Decode(bytes)); } catch { throw invalid(); }
  let name = '', rows = null;
  for (const [format, reader] of [['newpipe', newpipe], ['grayjay', grayjay], ['pockettube', pockettube]]) {
    rows = reader(data);
    if (rows !== null) { name = format; break; }
  }
  if (rows === null) throw invalid();
  const channels = [], seen = new Set();
  let skipped = 0;
  for (const [address, title, group] of rows) {
    const identifier = channelId(address);
    if (!identifier) skipped++;
    else if (!seen.has(identifier)) {
      seen.add(identifier);
      channels.push({ channel_id: identifier, title: typeof title === 'string' ? text(title).trim() : '', ...(group ? { group: text(group, 64) } : {}) });
    }
  }
  if (!channels.length && name === 'pockettube') throw invalid();   // structural detection alone is too weak
  return { api_version: 1, kind: 'subscriptions_import', format: name, channels: channels.slice(0, MAX_CHANNELS), skipped,
    truncated: channels.length > MAX_CHANNELS };
}
