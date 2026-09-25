const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test } = require('node:test');
const { checkRename } = require('./rename-contract.cjs');

function temporary(run) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rename 中文 '));
  try { run(root); } finally { fs.rmSync(root, { recursive: true, force: true }); }
}

test('rename preserves bytes, listing and explicit failure leaves source intact', () => {
  temporary(root => {
    const source = path.join(root, 'original.txt');
    fs.writeFileSync(source, Buffer.from([0, 255, 13, 10, 65]));
    const renamed = checkRename(source);
    assert.notEqual(renamed, source);
    assert.deepEqual(fs.readFileSync(renamed), Buffer.from([0, 255, 13, 10, 65]));
  });
});

test('oracle rejects a rename that reports success without moving the source', () => {
  temporary(root => {
    const source = path.join(root, 'original.txt');
    fs.writeFileSync(source, 'marker');
    assert.throws(() => checkRename(source, { ...fs, renameSync() {} }), /source remained/);
  });
});
