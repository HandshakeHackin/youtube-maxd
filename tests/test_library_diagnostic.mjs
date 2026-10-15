// Offline check that production failure diagnostics contain only fixed categories.
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
const source = await readFile(new URL('../standalone/js/errors.js', import.meta.url), 'utf8');
const { libraryDiagnostic } = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
assert.equal(libraryDiagnostic(new TypeError('secret token'), 'execute', 'history'),
  'YouTube operation failed: stage=execute action=history category=TypeError');
assert.equal(libraryDiagnostic({ constructor: { name: 'secret token' }, message: 'private response', code: 'secret' }, 'secret', 'private account'),
  'YouTube operation failed: stage=other action=other category=Error');
assert.equal(libraryDiagnostic(null, 'device', 'poll'),
  'YouTube operation failed: stage=device action=poll category=Error');
assert.equal(libraryDiagnostic(new TypeError(), 'execute', 'profiles'),
  'YouTube operation failed: stage=execute action=profiles category=TypeError');
console.log('PASS: bounded library failure diagnostics omit private inputs');
