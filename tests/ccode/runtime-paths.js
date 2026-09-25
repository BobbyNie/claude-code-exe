// Load the production isolation DLL into Bun to exercise its filesystem APIs.
// Official compiled payloads may disable BUN_BE_BUN, so use a pinned test Bun.
const { dlopen } = require('bun:ffi');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

console.log('Loading isolation DLL for filesystem regression');
const kernel = dlopen('kernel32.dll', {
  LoadLibraryW: { args: ['ptr'], returns: 'ptr' },
});
assert.ok(kernel.symbols.LoadLibraryW(Buffer.from(process.argv[2] + '\0', 'utf16le')),
  'production isolation DLL must load successfully');
console.log('Creating the session tasks directory');
const tasks = path.join(process.env.TEMP, 'claude', 'D--tt', 'session', 'tasks');
fs.mkdirSync(tasks, { recursive: true });
fs.mkdirSync(tasks, { recursive: true });
assert.ok(fs.statSync(tasks).isDirectory(), 'created tasks directory must be visible');
const filename = path.join(tasks, 'claude-anthropic-log.txt');
fs.writeFileSync(filename, 'ccode-path-marker');
assert.equal(fs.readFileSync(filename, 'utf8'), 'ccode-path-marker');
assert.ok(fs.readdirSync(tasks).includes(path.basename(filename)));
const child = spawnSync(process.env.ComSpec || 'cmd.exe', ['/d', '/c', 'type', filename], {
  encoding: 'utf8', timeout: 15000,
});
assert.ifError(child.error);
assert.equal(child.status, 0, child.stderr);
assert.match(child.stdout, /ccode-path-marker/);
console.log('ccode runtime path tests passed');

// Bun must be able to respawn the official payload, just as Grep/Glob do.
const payload = path.join(path.dirname(process.argv[2]), 'aa-runtime.exe');
assert.ok(fs.existsSync(payload));
const version = spawnSync(payload, ['--version'], { encoding: 'utf8', timeout: 15000 });
assert.ifError(version.error);
assert.equal(version.status, 0, version.stderr);
assert.match(version.stdout, /Claude Code/);
console.log('ccode runtime respawn test passed');

// Version 2.1.221 selects embedded ripgrep with argv0="rg", not a CLI flag.
const grep = spawnSync(payload, ['--no-config', '--fixed-strings', '--', 'ccode-path-marker', filename], {
  argv0: 'rg', encoding: 'utf8', timeout: 15000,
});
assert.ifError(grep.error);
assert.equal(grep.status, 0, grep.stderr);
assert.match(grep.stdout, /ccode-path-marker/);
const glob = spawnSync(payload, ['--no-config', '--files', '--glob', '*.txt', tasks], {
  argv0: 'rg', encoding: 'utf8', timeout: 15000,
});
assert.ifError(glob.error);
assert.equal(glob.status, 0, glob.stderr);
assert.ok(glob.stdout.includes(path.basename(filename)), glob.stdout);
fs.unlinkSync(filename);
fs.rmdirSync(tasks);
console.log('ccode built-in Grep/Glob backend tests passed');
// Keep the DLL loaded until process exit: its hooks cannot be unloaded safely.
