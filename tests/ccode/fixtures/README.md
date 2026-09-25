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
