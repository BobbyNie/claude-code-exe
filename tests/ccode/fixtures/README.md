# Public TLS test fixtures

`untrusted-test-cert.pem` and `untrusted-test-key.pem` are a self-signed certificate
and intentionally public private key used only by the loopback gateway fixture.
They are not credentials and must never be used for deployment or added to a
machine/engine trust store. The certificate has an IP SAN for `127.0.0.1` so the
negative test exercises untrusted issuer rejection rather than a hostname mismatch.

The fixture unit test first requires a default-trust client to reject the TLS
handshake, then uses an explicitly scoped client trust context to confirm the
server can actually complete TLS and return HTTP. The engine acceptance test does
not add this trust: it requires a rejected handshake, zero HTTP requests, nonzero
exit, a TLS-specific neutral diagnostic, and no workspace writes or terminal
credential/prompt disclosure. Local fixture success is not engine acceptance.

Certificate validity: 2026-09-25 21:35:06 UTC through 2126-09-01 21:35:06 UTC.

## Expiration-only rejection

`expired-test-ca.pem` is a separate, valid, test-only CA with critical CA and
keyCertSign/cRLSign extensions. `expired-test-cert.pem` is its leaf certificate,
with serverAuth, CA:FALSE, IP SAN 127.0.0.1 and validity from January 1, 2020
through January 1, 2021 UTC. The leaf reuses the intentionally public fixture key; its CA has a separate public key.
Never use these files or key for deployment or install them in a machine store.

The expiration test scopes NODE_EXTRA_CA_CERTS to this public CA for the single
fixture subprocess; it never disables TLS verification. The unit test uses a
strict trust context and requires verification code 10 (expired), not an issuer,
hostname or malformed-chain failure. The actual-engine acceptance independently
requires failed TLS handshake evidence, zero HTTP, nonzero exit, E_GATEWAY_TLS
and the strict neutral failure report. Trusting this test CA is not production
signer approval, enterprise gateway trust or permission to bypass certificate
validity checks. Original untrusted-issuer acceptance remains unchanged.

### Independent-key positive control (2026-10-03)

The expiration CA uses an independent RSA key and is valid from January 1, 2019
through January 1, 2126. `valid-test-cert.pem` is a serverAuth leaf issued by that
same CA, with the same loopback IP SAN and public server key as the expired leaf,
but valid from January 1, 2020 through January 1, 2125. The negative leaf remains
expired in 2021. Neither issuer trust nor hostname checks are disabled.

The old CA reused the server key; native Windows policy reported untrusted root
and expiration together (0x800b0109 / chain 0x21), so it did not prove an
expiration-only rejection. The new key-separation invariant and positive leaf
are required test controls, not deployment credentials or release signer trust.
