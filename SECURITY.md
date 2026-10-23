# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| Latest release on [GitHub Releases](https://github.com/JRpersonal/spacemonger/releases/latest) | yes |
| Older releases, upstream builds (andedammen.dk, Daduck/spacemonger) | no, report those upstream |

## Reporting a vulnerability

Please do **not** open a public issue. Use GitHub's private reporting:
[Report a vulnerability](https://github.com/JRpersonal/spacemonger/security/advisories/new).

Include a description, steps to reproduce, the affected version
(`Help > About` or the file properties of `SpaceMonger.exe`) and your
assessment of the impact. SpaceMonger is maintained in spare time; expect a
first answer within a few days. Fixes ship as a new release with a GitHub
security advisory, and reporters are credited unless they prefer otherwise.

## Threat model in one paragraph

SpaceMonger runs with the user's rights, reads directory metadata, and can
open, show properties of, or delete (to the Recycle Bin) the item the user
picks. It has no network code, no updater, no telemetry and no installer. The
relevant attack surface is therefore (1) untrusted file and folder names
(length, Unicode, reparse points) reaching the scanner and the UI, and (2) the
integrity of the executable you download.

## Verifying a download

Releases are built only by [`release.yml`](.github/workflows/release.yml) on
GitHub-hosted runners; nothing is uploaded by hand. Each release has:

- **Authenticode signature**: Certum Open Source code signing certificate,
  subject `Open Source Developer Jens Roggenfelder`, SHA-256 with an RFC 3161
  timestamp. The signing key is held in Certum's SimplySign cloud and is used
  from the maintainer's machine only for the signing step.
- **`SHA256SUMS`** covering every executable.
- **Sigstore build provenance** created after signing, so it covers the exact
  bytes you download.

```powershell
Get-AuthenticodeSignature .\SpaceMonger-x64.exe | Format-List Status, SignerCertificate
Get-FileHash .\SpaceMonger-x64.exe -Algorithm SHA256   # compare with SHA256SUMS
gh attestation verify .\SpaceMonger-x64.exe --repo JRpersonal/spacemonger
```

## Supply chain practices

- `master` is protected: pull requests with passing build, CodeQL and lint
  checks; no force pushes or deletions.
- Every GitHub Action is pinned to a full commit SHA and updated by Dependabot.
- CodeQL (C/C++) on every push, pull request and weekly; OpenSSF Scorecard and
  OSV-Scanner weekly; secret scanning with push protection.
- The self-hosted signing runner only accepts the `sign-windows` job, which
  runs in the `release` environment restricted to `master`; pull requests can
  never reach it.
- Release executables are uploaded to VirusTotal; a broad detection stops the
  release.
