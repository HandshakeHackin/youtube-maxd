// Original offline test bundling/cleanup. SPDX-License-Identifier: GPL-3.0-or-later
import { build } from 'esbuild';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const root = path.dirname(fileURLToPath(import.meta.url));
const directory = await mkdtemp(path.join(tmpdir(), 'prosperotube-library-tests-'));
try {
  const output = path.join(directory, 'tests.mjs');
  await build({ entryPoints: [path.join(root, 'worker.test.mjs')], outfile: output,
    bundle: true, platform: 'node', format: 'esm', target: 'node20', legalComments: 'inline',
    banner: { js: 'import { createRequire as tubeCreateRequire } from "node:module"; const require = tubeCreateRequire(import.meta.url);' } });
  const result = await new Promise((resolve, reject) => {
    const child = spawn(process.execPath, ['--test', output], { stdio: 'inherit' });
    child.once('error', reject);
    child.once('exit', (code, signal) => resolve(signal ? 1 : code ?? 1));
  });
  process.exitCode = result;
} finally {
  await rm(directory, { recursive: true, force: true });
}
