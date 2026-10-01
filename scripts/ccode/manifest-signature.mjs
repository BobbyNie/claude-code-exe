/** Engineering verifier only; not yet a launcher/update acceptance gate.
 * Sign externally over DOMAIN || the exact manifest bytes. Never load private keys.
 * The trust pin must come from independent approved policy, not the candidate.
 */
import {createHash, createPublicKey, verify} from 'node:crypto';

const DOMAIN = Buffer.from('ccode-enterprise-manifest-v1\0');

export function verifyManifestSignature({manifest, signature, publicKeyDer, trustedPin}) {
  try {
    if (!Buffer.isBuffer(manifest) || manifest.length === 0 || manifest.length > 1048576 ||
        !Buffer.isBuffer(signature) || signature.length !== 64 ||
        !Buffer.isBuffer(publicKeyDer) || publicKeyDer.length > 1024 ||
        typeof trustedPin !== 'string' || !/^[0-9a-f]{64}$/.test(trustedPin) ||
        createHash('sha256').update(publicKeyDer).digest('hex') !== trustedPin) return false;
    const key = createPublicKey({key: publicKeyDer, type: 'spki', format: 'der'});
    if (key.asymmetricKeyType !== 'ed25519' ||
        !key.export({type: 'spki', format: 'der'}).equals(publicKeyDer)) return false;
    return verify(null, Buffer.concat([DOMAIN, manifest]), key, signature);
  } catch {
    return false;
  }
}
