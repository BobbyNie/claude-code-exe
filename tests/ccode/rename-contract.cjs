// Shared acceptance oracle: native rename must not split the visible namespace.
const assert = require('node:assert/strict');
const nativeFs = require('node:fs');
const path = require('node:path');

function checkRename(source, fs = nativeFs) {
  const directory = path.dirname(source);
  const destination = path.join(directory, 'renamed 中文 file.txt');
  const original = fs.readFileSync(source);
  assert.equal(fs.existsSync(destination), false, 'rename fixture destination already exists');
  fs.renameSync(source, destination);
  assert.equal(fs.existsSync(source), false, 'rename source remained visible');
  assert.ok(fs.statSync(destination).isFile());
  assert.deepEqual(fs.readFileSync(destination), original);
  const entries = fs.readdirSync(directory);
  assert.ok(!entries.includes(path.basename(source)));
  assert.ok(entries.includes(path.basename(destination)));
  assert.throws(() => fs.statSync(source), error => error.code === 'ENOENT');

  // A missing parent has a deterministic failure without platform-dependent overwrite rules.
  const missingParent = path.join(directory, 'absent-parent');
  assert.equal(fs.existsSync(missingParent), false);
  const invalidDestination = path.join(missingParent, 'file.txt');
  assert.throws(() => fs.renameSync(destination, invalidDestination),
                error => error.code === 'ENOENT');
  assert.deepEqual(fs.readFileSync(destination), original, 'failed rename changed source bytes');
  assert.equal(fs.existsSync(invalidDestination), false);
  assert.ok(fs.readdirSync(directory).includes(path.basename(destination)));
  return destination;
}

module.exports = { checkRename };
