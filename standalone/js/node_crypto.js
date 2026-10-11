// Stand-in for the two `node:crypto` functions the service modules import.
// SPDX-License-Identifier: GPL-3.0-or-later
const host = globalThis.__host;
export const randomUUID = () => globalThis.crypto.randomUUID();

export function createHash(algorithm) {
  if (!['sha1', 'sha256'].includes(algorithm)) throw new Error(`Unsupported digest: ${algorithm}`);
  const parts = [];
  return {
    update(text) { parts.push(String(text)); return this; },
    digest(encoding) {
      if (encoding !== 'hex') throw new Error('Only hex digests are supported.');
      return host[algorithm](parts.join(''));
    }
  };
}
