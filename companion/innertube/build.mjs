// Original worker bundling recipe. SPDX-License-Identifier: GPL-3.0-or-later
import { build } from 'esbuild';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const root = path.dirname(fileURLToPath(import.meta.url));
await build({ entryPoints: [path.join(root, 'worker.mjs')], outfile: path.join(root, 'worker.bundle.mjs'),
  bundle: true, platform: 'node', format: 'esm', target: 'node20',
  banner: { js: 'import { createRequire as tubeCreateRequire } from "node:module"; const require = tubeCreateRequire(import.meta.url);' },
  legalComments: 'inline' });
