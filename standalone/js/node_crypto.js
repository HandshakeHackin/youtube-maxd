// Stand-in for the two `node:crypto` functions the service modules import.
// SPDX-License-Identifier: GPL-3.0-or-later
const host = globalThis.__host;
export const randomUUID = () => globalThis.crypto.randomUUID();

const K = new Uint32Array(64);
{ // SHA-256 round constants: fractional parts of the cube roots of the first 64 primes.
  let count = 0;
  for (let candidate = 2; count < 64; candidate++) {
    let prime = true;
    for (let d = 2; d * d <= candidate; d++) if (candidate % d === 0) { prime = false; break; }
    if (prime) K[count++] = Math.floor((Math.cbrt(candidate) % 1) * 4294967296);
  }
}
function sha256(bytes) {
  const h = Uint32Array.of(0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19);
  const total = ((bytes.length + 8 >> 6) + 1) << 6;
  const data = new Uint8Array(total);
  data.set(bytes); data[bytes.length] = 0x80;
  const view = new DataView(data.buffer);
  view.setUint32(total - 8, Math.floor(bytes.length / 536870912)); view.setUint32(total - 4, bytes.length * 8 >>> 0);
  const w = new Uint32Array(64), rot = (v, n) => v >>> n | v << 32 - n;
  for (let offset = 0; offset < total; offset += 64) {
    for (let i = 0; i < 16; i++) w[i] = view.getUint32(offset + i * 4);
    for (let i = 16; i < 64; i++)
      w[i] = (rot(w[i - 2], 17) ^ rot(w[i - 2], 19) ^ w[i - 2] >>> 10) + w[i - 7] + (rot(w[i - 15], 7) ^ rot(w[i - 15], 18) ^ w[i - 15] >>> 3) + w[i - 16];
    let [a, b, c, d, e, f, g, hh] = h;
    for (let i = 0; i < 64; i++) {
      const t1 = hh + (rot(e, 6) ^ rot(e, 11) ^ rot(e, 25)) + (e & f ^ ~e & g) + K[i] + w[i] >>> 0;
      const t2 = (rot(a, 2) ^ rot(a, 13) ^ rot(a, 22)) + (a & b ^ a & c ^ b & c) >>> 0;
      hh = g; g = f; f = e; e = d + t1 >>> 0; d = c; c = b; b = a; a = t1 + t2 >>> 0;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  return Array.from(h, word => word.toString(16).padStart(8, '0')).join('');
}

export function createHash(algorithm) {
  if (!['sha1', 'sha256'].includes(algorithm)) throw new Error(`Unsupported digest: ${algorithm}`);
  const parts = [];
  return {
    update(text) { parts.push(String(text)); return this; },
    digest(encoding) {
      if (encoding !== 'hex') throw new Error('Only hex digests are supported.');
      const text = parts.join('');
      return algorithm === 'sha1' ? host.sha1(text) : sha256(new Uint8Array(host.utf8Encode(text)));
    }
  };
}
