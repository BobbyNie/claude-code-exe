# Unified Windows x64 Release readiness — 2026-10-03

**Status: engineering CI green; independent Release not published or approved.**

## Verified candidate

- Repository/branch: `BobbyNie/claude-code-exe`, `codex/unified-sandbox-design`.
- Code revision: `ee64866c94ec523f8e52837da42b5cbca71cfeca`.
- Completed workflow: `37101196950`; all seven jobs passed.
- Actual platform: GitHub-hosted Windows Server 2025 Datacenter, 24H2,
  build 26100, x64 OS/process, administrator token/member. This satisfies the
  explicitly updated hosted-platform scope; it is not Windows11 ordinary-account evidence.
- HTTPS-proxy → HTTPS remains deferred, not passed. Other proxy combinations
  are not implicitly waived by this deferral.

| Engine | Full runtime | Workspace boundary | Real offline | Executable SHA256 |
|---|---|---|---|---|
| 2.1.221 | 111140903797 | 111142649540 | 111142649579 | `81afa4776573f6c869abfe6a17c12e3fb43de256a11879e5b8a033ed1fe6537b` |
| 2.1.282 | 111140903670 | 111142649442 | 111142649418 | `4335f300620be156df853c1f8d8e8ba8ffc7687c465fb68b2e8ca0370d7f7038` |

Cross-version upgrade/rollback job: `111142649415`, passed.

## Evidence inspected, not just job conclusions

Downloaded these artifacts from that exact run and checked archive SHA256 against
GitHub's artifact digest before reading their contents:

| Artifact | ID | Archive SHA256 |
|---|---|---|
| built 2.1.221 | 11266716862 | `598d2a98a2aec52b0e15ef876529b599e5e067b0b931fe7ff9ecaeb7262420a9` |
| built 2.1.282 | 11266761411 | `a4b97b9fba7279468d56fe7f41b6db832772c3ef400bc4e67509b2dbb2d1524b` |
| offline 2.1.221 | 11266596956 | `a22271a604874de0ef8ca5fd09c9b68590a70a0a35995c5feb21f6ad368cfaee` |
| offline 2.1.282 | 11266736848 | `b83992e8549871257fad6d4a54a2d597a2fe032ce249eaaa625e15571091293f` |
| provenance 2.1.221 | 11266766067 | `bb3be58217500796468fb0db7a9f1fa619068c84518f7f6f162bbb1a1ce5c35e` |
| provenance 2.1.282 | 11266201586 | `1385bf4e322884ad8d906f2bd1c9a7c041cace9ebe5224eeb95aff32d379a560` |

Both built archives contain only `ccode.exe`; executable hashes match both offline
and provenance reports. Sizes are 279292416 and 243184128 bytes respectively.
Both reports bind to the exact code revision and expected engine version.

Both offline reports record `acceptance.passed=true`, zero non-loopback default
routes and zero connected non-loopback adapters. All five recorded commands exit
zero. No services or drivers were added or removed; existing service state changes
are recorded (1 for 2.1.221, 10 for 2.1.282), not concealed as identical inventories.
Driver state changes are zero. Controller jobs also passed restoration/cleanup.
The real engine/tools used deterministic loopback responses, not a live enterprise model.

Provenance reports record extracted engine hash/size matching their embedded
manifest, official source URLs/hashes, and exact adapter revision. This supports
payload provenance, not a trusted publisher signature or redistribution approval.

## Release blockers and proof still required

1. **Approved distribution inputs (A02/A04):** actual required notice bytes plus
   independently approved hashes, redistribution authorization and approved
   restricted-name policy. No placeholder notice or inferred approval is acceptable.
2. **Approved signer (A20):** public SPKI and independently approved SHA256 pin,
   with signing performed through an authorized private-key holder. Do not send
   private keys in chat. The current workflow calls `build.ps1` without
   `SignerSpkiPath`/`ApprovedSignerPin`; its downloadable binaries are public-mode
   engineering builds, not the required trusted enterprise candidates.
3. **Complete signed candidate lifecycle (A05/A14/A20):** build against approved
   policy, assemble the allowlisted independent package, sign its manifest, audit
   ZIP/extracted mirror, install/verify signature, and run hosted move/repack/data
   preservation plus signed update/rollback trials. Public-mode CI and engineering
   signature fixtures cannot substitute for this actual candidate.
4. **Remaining scope audit:** reconcile every A01–A20/design item with current
   tests and evidence, including approved endpoint/extensions, fault recovery,
   diagnostics and proxy combinations outside the explicit deferral. A green
   workflow is not proof that unimplemented/unexecuted cases are complete.
5. **Publication:** only after these gates, publish the independent release with
   its exact verified candidate commit, final package hashes, notices and support
   limitations. Main's daily mixed bundle is not this deliverable.

No physical Windows11 gate is reinstated. No new release/tag was created by this
readiness audit. These are retained requirements from the design and acceptance
matrix, not a redefinition of success around the existing public-mode artifact.
