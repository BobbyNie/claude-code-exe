/** Engineering CLI: manifest path, detached signature path, SPKI DER path,
 * independently approved SHA256 pin. Does not assert candidate or release approval.
 */
import {closeSync, fstatSync, lstatSync, openSync, readSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {verifyManifestSignature} from './manifest-signature.mjs';

function regularBytes(path, maximum) {
  const before = lstatSync(path);
  if (!before.isFile() || before.isSymbolicLink() || before.size > maximum) throw Error();
  const handle = openSync(path, 'r');
  try {
    const opened = fstatSync(handle);
    if (!opened.isFile() || opened.size > maximum || opened.dev !== before.dev ||
        opened.ino !== before.ino) throw Error();
    const bytes = Buffer.alloc(maximum + 1);
    let length = 0;
    while (length < bytes.length) {
      const count = readSync(handle, bytes, length, bytes.length - length, null);
      if (count === 0) break;
      length += count;
    }
    if (length > maximum) throw Error();
    return bytes.subarray(0, length);
  } finally { closeSync(handle); }
}

try {
  const args = process.argv.slice(2);
  if (args.length !== 4) throw Error();
  const [manifestPath, signaturePath, keyPath, trustedPin] = args;
  const manifest = regularBytes(manifestPath, 1048576);
  const signature = regularBytes(signaturePath, 64);
  const publicKeyDer = regularBytes(keyPath, 1024);
  if (!verifyManifestSignature({manifest, signature, publicKeyDer, trustedPin})) throw Error();
  console.log(JSON.stringify({schema: 1, status: 'passed',
    manifestSha256: createHash('sha256').update(manifest).digest('hex')}));
} catch {
  console.log(JSON.stringify({schema: 1, status: 'error', code: 'E_MANIFEST_SIGNATURE'}));
  process.exitCode = 2;
}
