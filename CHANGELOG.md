# Changelog

All notable changes to this fork. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/). The release
workflow publishes the section whose heading matches the release version, so
every release needs a `## [X.Y.Z]` section here first.

## [Unreleased]

## [1.4.5] - 2026-10-23

First release of this fork, now code signed.

### Changed
- The About dialog, file properties and `README.TXT` point to this repository
  (`JRpersonal/spacemonger`) for downloads and bug reports, and name both
  maintainers.
- Release executables are named `SpaceMonger-x86.exe`, `SpaceMonger-x64.exe`
  and `SpaceMonger-arm64.exe`, matching the upstream release names.

### Added
- Signed releases: Authenticode signature (Certum Open Source certificate via
  SimplySign), Sigstore provenance after signing, `SHA256SUMS`, VirusTotal
  scan with a release gate, post-publish asset verification, and winget
  updates.
- `tools/version.ps1` keeps the version in `CMakeLists.txt`, `SpaceMonger.rc`
  and `README.TXT` in sync; CI checks it on every pull request.
- Project documentation: architecture with diagrams, release runbook,
  third-party notices, security policy, contributing guide, issue templates.

### Fixed
- Info-tip icon handle leak (`HICON`), stray NUL byte in a class-name
  initialiser, narrowing conversions, and hardening of the tip window, the
  settings dialog and scan cancellation.

## [1.4.4] - 2026-09-15

Upstream release by Johnny J. Andersen
([Daduck/spacemonger v1.4.4](https://github.com/Daduck/spacemonger/releases/tag/v1.4.4)).

### Added
- Partial scans show a clickable `Skipped: N` notice that opens a details
  dialog with the skipped-folder count, the first Windows error and the
  accessible skipped paths.
- Partial whole-drive scans can show an `Unavailable (estimate)` treemap block
  when volume-used space exceeds scanned file space.

## [1.4.3] - 2026-08-15

Upstream release ([Daduck/spacemonger v1.4.3](https://github.com/Daduck/spacemonger/releases/tag/v1.4.3)).

### Added
- Multithreaded scan engine and a live treemap that builds while scanning.
- Double-buffered, flicker-free rendering; Per-Monitor V2 DPI awareness.

### Fixed
- Scan hang at completion, missing cloud-placeholder folders, a name
  corruption bug, slow cancellation on network drives, out-of-memory handling.

## [1.4.2] and [1.4.1] - 2026

Upstream modernization: CMake/VS2022 build, Unicode scanner with long-path
support, reparse-point handling, arena allocator, safer delete policy.

## [1.4.0] - 2000

Original release by Sean Werkema
([seanofw/spacemonger1](https://github.com/seanofw/spacemonger1)). Older
history is in `README.TXT`.
