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
