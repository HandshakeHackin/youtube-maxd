// Explicit public TV probe bundling/cleanup. SPDX-License-Identifier: GPL-3.0-or-later
import { build } from 'esbuild';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const [video, report] = process.argv.slice(2);
if (!/^[A-Za-z0-9_-]{11}$/.test(video ?? '') || !report) process.exit(2);
const root = path.dirname(fileURLToPath(import.meta.url));
const directory = await mkdtemp(path.join(tmpdir(), 'prosperotube-tv-probe-'));
try {
  const output = path.join(directory, 'probe.mjs');
  await build({entryPoints:[path.join(root,'../../scripts/probe_tv_player.mjs')],outfile:output,
    bundle:true,platform:'node',format:'esm',target:'node20',nodePaths:[path.join(root,'node_modules')],
    legalComments:'inline',
    banner:{js:'import { createRequire as tubeCreateRequire } from "node:module"; const require = tubeCreateRequire(import.meta.url);'}});
  process.exitCode = await new Promise((resolve,reject) => {
    const child = spawn(process.execPath, [output,video,path.resolve(report)], {stdio:'inherit'});
    const timeout = setTimeout(() => child.kill('SIGKILL'),90000);
    child.once('error', error => {clearTimeout(timeout);reject(error);});
    child.once('close', (code,signal) => {clearTimeout(timeout);resolve(signal ? 1 : code ?? 1);});
  });
} finally {
  if (path.dirname(directory) !== path.resolve(tmpdir()) || !path.basename(directory).startsWith('prosperotube-tv-probe-'))
    throw new Error('Unexpected probe temporary directory');
  await rm(directory,{recursive:true,force:true});
}
