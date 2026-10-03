# Hosted Windows acceptance snapshot — 2026-10-03

- Repository: BobbyNie/claude-code-exe
- Workflow run: 37090296989, completed / success.
- Tested code: `3321a2e3c22f9a1376e61fb697cb38e6fa540053`.
- Jobs: test2.1.221 (111109007836), test2.1.282 (111109007893),
  workspace-boundary2.1.282 (111110972076), workspace-boundary2.1.221
  (111110972082), cross-version (111110972142): all success.
- Reports copied byte-for-byte from artifacts11262516993 (2.1.221) and
  11261933067 (2.1.282), both named ccode-hosted-provenance-<version>.
- Actual environment: Windows build26100, product type3 (Server), x64,
  64-bit verifier, administrator token. Not Windows11 ordinary-account evidence.
- Both logs confirm actual native HTTP/HTTPS forwarding, concurrency, streaming,
  cancellation, trust rejection and provenance checks; both full engine jobs and
  the independent cross-version/workspace gates pass. Annotation observed on the
  2.1.282 job is Node20 deprecation, not a failing check or billing error.

## Candidate executable SHA256

| Engine | Executable SHA256 |
|---|---|
| 2.1.221 | `9ff16b1ae9f2608385954af362cd0ed1fd03d81d6994ee38435c097c6c695323` |
| 2.1.282 | `f65675911b3547b424c199f03d4de9f35cec15d9b57e58647c3c290be7fc8390` |

The JSON reports also bind extracted official engine bytes, official source
manifest digest and adapter SHA. They are provenance evidence, not signatures.

## Release decision: not yet released

This is a successful engineering matrix, NOT a completed full-design audit.
Earlier intermittent subagent E_PROTOCOL_ORDER and interrupted-snapshot WinError32
have not reproduced in this run. No observed diagnostic establishes their root
cause; successful runs must not be presented as conclusive fixes.

The following release work remains, independent of hosted-platform approval:

1. Finish approved transport parity (proxy configuration and response trailers),
   and validate it against actual Windows requests without disabling TLS checks.
2. Obtain isolated/offline hosted evidence; an online loopback test does not
   establish no non-loopback networking or absence of installation dependencies.
3. Assemble and exercise the complete signed standalone candidate, including
   external data separation, move/repack, update and compatible data rollback.
   Existing public-binary jobs are not this signed-candidate acceptance.
4. Collect the remaining failure-matrix evidence, including data-safety fault
   cases and per-case diagnostics. Old historical table rows are not a current
   exhaustive completion proof.
5. Obtain external approved signer SPKI/public pin, required notice bytes/hashes,
   redistribution approval and restricted-name policy; never substitute fixture
   credentials or self-authorized approval. No private key is needed in chat.
6. Publish the independent Release only after those gates, binding its actual
   attachments to the tested candidate and hashes. The daily main mixed bundle
   does not satisfy this delivery.
