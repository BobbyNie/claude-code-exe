// Run inside the injected official payload with BUN_BE_BUN=1; no API calls.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

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
fs.unlinkSync(filename);
fs.rmdirSync(tasks);
console.log('ccode runtime path tests passed');
