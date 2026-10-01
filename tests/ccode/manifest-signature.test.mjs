import {test} from 'node:test';
import assert from 'node:assert/strict';
import {generateKeyPairSync, createHash, sign} from 'node:crypto';
import {verifyManifestSignature} from '../../scripts/ccode/manifest-signature.mjs';

const domain = Buffer.from('ccode-enterprise-manifest-v1\0');
const {publicKey, privateKey} = generateKeyPairSync('ed25519');
const publicKeyDer = publicKey.export({type: 'spki', format: 'der'});
const trustedPin = createHash('sha256').update(publicKeyDer).digest('hex');
const manifest = Buffer.from('{"schemaVersion":1,"platform":"windows","architecture":"x64"}\n');
const signature = sign(null, Buffer.concat([domain, manifest]), privateKey);

test('verifies exact manifest bytes with an independently pinned signer', () => {
  assert.equal(verifyManifestSignature({manifest, signature, publicKeyDer, trustedPin}), true);
});

test('rejects changed bytes, signatures, keys and untrusted pins', () => {
  const other = generateKeyPairSync('ed25519');
  const foreignKey = other.publicKey.export({type: 'spki', format: 'der'});
  const changed = Buffer.from(signature); changed[0] ^= 1;
  for (const mutation of [
    {manifest: Buffer.concat([manifest, Buffer.from(' ')])},
    {signature: changed}, {signature: signature.subarray(1)},
    {publicKeyDer: foreignKey}, {trustedPin: '0'.repeat(64)},
    {trustedPin: trustedPin.toUpperCase()}, {manifest: Buffer.alloc(0)},
    {manifest: Buffer.alloc(1048577)}, {manifest: manifest.toString()},
    {publicKeyDer: Buffer.from('invalid')},
  ]) assert.equal(verifyManifestSignature({manifest, signature, publicKeyDer, trustedPin,
                                         ...mutation}), false);
});

test('rejects signatures without the domain separation and wrong key algorithms', () => {
  assert.equal(verifyManifestSignature({manifest,
    signature: sign(null, manifest, privateKey), publicKeyDer, trustedPin}), false);
  const rsa = generateKeyPairSync('rsa', {modulusLength: 2048});
  const key = rsa.publicKey.export({type: 'spki', format: 'der'});
  assert.equal(verifyManifestSignature({manifest, signature, publicKeyDer: key,
    trustedPin: createHash('sha256').update(key).digest('hex')}), false);
});

test('rejects trailing DER bytes even when their hash is pinned', () => {
  const key = Buffer.concat([publicKeyDer, Buffer.from([0])]);
  assert.equal(verifyManifestSignature({manifest, signature, publicKeyDer: key,
    trustedPin: createHash('sha256').update(key).digest('hex')}), false);
});

import {mkdtempSync, writeFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';

test('public CLI verifies real files and emits only neutral evidence', () => {
  const root = mkdtempSync(join(tmpdir(), 'ccode-signature-'));
  try {
    const paths = ['manifest.json', 'manifest.sig', 'signer.der'].map(p => join(root, p));
    [manifest, signature, publicKeyDer].forEach((bytes, i) => writeFileSync(paths[i], bytes));
    const cli = fileURLToPath(new URL('../../scripts/ccode/verify-manifest-signature.mjs', import.meta.url));
    const result = spawnSync(process.execPath, [cli, ...paths, trustedPin], {encoding: 'utf8'});
    assert.equal(result.status, 0, result.stderr);
    assert.deepEqual(JSON.parse(result.stdout), {schema: 1, status: 'passed',
      manifestSha256: createHash('sha256').update(manifest).digest('hex')});
  } finally { rmSync(root, {recursive: true, force: true}); }
});

test('public CLI fails closed without raw paths, contents or errors', () => {
  const root = mkdtempSync(join(tmpdir(), 'private-signature-path-'));
  try {
    const paths = ['manifest.json', 'manifest.sig', 'signer.der'].map(p => join(root, p));
    [manifest, signature, publicKeyDer].forEach((bytes, i) => writeFileSync(paths[i], bytes));
    const cli = fileURLToPath(new URL('../../scripts/ccode/verify-manifest-signature.mjs', import.meta.url));
    const calls = [[...paths, '0'.repeat(64)], [root, paths[1], paths[2], trustedPin],
                   [join(root, 'missing'), paths[1], paths[2], trustedPin], []];
    writeFileSync(paths[1], Buffer.alloc(65));
    calls.push([...paths, trustedPin]);
    for (const args of calls) {
      const result = spawnSync(process.execPath, [cli, ...args], {encoding: 'utf8'});
      assert.equal(result.status, 2);
      assert.equal(result.stderr, '');
      assert.deepEqual(JSON.parse(result.stdout),
        {schema: 1, status: 'error', code: 'E_MANIFEST_SIGNATURE'});
      assert.equal(result.stdout.includes(root), false);
    }
  } finally { rmSync(root, {recursive: true, force: true}); }
});
