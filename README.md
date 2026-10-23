# SpaceMonger

[![build](https://github.com/JRpersonal/spacemonger/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/JRpersonal/spacemonger/actions/workflows/build.yml)
[![CodeQL](https://github.com/JRpersonal/spacemonger/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/JRpersonal/spacemonger/actions/workflows/codeql.yml)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/JRpersonal/spacemonger/badge)](https://scorecard.dev/viewer/?uri=github.com/JRpersonal/spacemonger)
[![Latest release](https://img.shields.io/github/v/release/JRpersonal/spacemonger?sort=semver&display_name=tag)](https://github.com/JRpersonal/spacemonger/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/JRpersonal/spacemonger/total)](https://github.com/JRpersonal/spacemonger/releases)
[![License: MIT](https://img.shields.io/github/license/JRpersonal/spacemonger)](LICENSE.txt)

[![Windows 10 | 11](https://img.shields.io/badge/Windows-10%20%7C%2011%20%7C%20Server-0078D4?logo=windows)](#download)
[![x86 | x64 | ARM64](https://img.shields.io/badge/arch-x86%20%7C%20x64%20%7C%20ARM64-555)](#download)
[![C++17 / MFC](https://img.shields.io/badge/C%2B%2B17-MFC%20static-00599C?logo=cplusplus)](BUILDING.md)
[![Code signed: Certum](https://img.shields.io/badge/code%20signed-Certum%20Open%20Source-2ea44f)](SECURITY.md#verifying-a-download)
[![Provenance: Sigstore](https://img.shields.io/badge/provenance-Sigstore-6f42c1?logo=sigstore)](SECURITY.md#verifying-a-download)
[![Fork of Daduck/spacemonger](https://img.shields.io/badge/fork%20of-Daduck%2Fspacemonger-lightgrey?logo=github)](https://github.com/Daduck/spacemonger)

[![GitHub Sponsors](https://img.shields.io/github/sponsors/JRpersonal?label=Sponsor%20on%20GitHub&logo=GitHub&color=ea4aaa)](https://github.com/sponsors/JRpersonal)
[![Ko-fi](https://img.shields.io/badge/Ko--fi-support-FF5E5B?logo=ko-fi&logoColor=white)](https://ko-fi.com/streborn)
[![PayPal](https://img.shields.io/badge/PayPal-donate-00457C?logo=paypal&logoColor=white)](https://paypal.me/JR31337)

SpaceMonger is a native Windows disk-space visualizer with a classic MFC
interface, background scanning, and a live treemap: your whole drive as one
picture, where the larger the rectangle, the more space it takes.

## Download

| Windows | File |
|---|---|
| 64-bit (most PCs) | [SpaceMonger-x64.exe](https://github.com/JRpersonal/spacemonger/releases/latest/download/SpaceMonger-x64.exe) |
| ARM64 (Snapdragon, Surface Pro X) | [SpaceMonger-arm64.exe](https://github.com/JRpersonal/spacemonger/releases/latest/download/SpaceMonger-arm64.exe) |
| 32-bit (legacy) | [SpaceMonger-x86.exe](https://github.com/JRpersonal/spacemonger/releases/latest/download/SpaceMonger-x86.exe) |

One standalone executable, no installer, no companion files. Releases are
Authenticode-signed (Certum Open Source certificate), listed in `SHA256SUMS`
and carry Sigstore build provenance. [How to verify](SECURITY.md#verifying-a-download).

## Features

- Treemap of a drive, folder or network share (`\\PC01\c$`), coloured by
  depth, muted or by file type; zoom with the mouse wheel and keys, show in
  Explorer, properties, delete to the Recycle Bin. Drag a folder onto the
  window to scan it.
- *Free Up Space*: one click removes temporary files, browser caches and the
  Recycle Bin; your biggest files are listed with tips and can go to the
  Recycle Bin together.
- System content (Windows, Program Files, ...) is shown muted and can be
  hidden; *Run as administrator* restarts elevated to read the rest.
- CSV export (right-click > *Export list*, `Ctrl+E`) and a headless report
  mode for company rollouts, see [Command line](#command-line).
- English, German and French, following the Windows display language.
- Multithreaded scanner; the treemap builds live while the scan runs
  (at most every 250 ms, only when something changed).
- Unicode names and long paths end to end; junctions, symlinks, sparse files
  and cloud placeholders (OneDrive and others) are accounted for correctly.
- Honest results: failed roots are errors, unreadable or too-deep subtrees
  give a clickable `Skipped: N` notice with details, and partial whole-drive
  scans show an *Unavailable (estimate)* block.
- Per-Monitor V2 DPI awareness with the original 1990s look.

Known limitation: hard links are counted once per link. Details in
[README.TXT](README.TXT).

## Command line

```text
SpaceMonger.exe [path] [/report <file or folder>] [/top N] [/hide-system]
```

| Example | |
|---|---|
| `SpaceMonger.exe D:\` | opens the window and scans D: right away (no drive dialog) |
| `SpaceMonger.exe \\PC01\c$` | an admin scans a remote PC over its admin share |
| `SpaceMonger.exe /report \\srv\reports\` | no window: scans the Windows drive, writes `\\srv\reports\<COMPUTER>_<yyyy-mm-dd>.csv` and exits |
| `SpaceMonger.exe \\PC01\c$ /report pc01.csv /top 50 /hide-system` | remote report with the 50 biggest folders and files, without system content |

The report lists drive size, used, free and *not readable* space, then the
largest folders and files with size, share, file count and date. It is UTF-8
with the list separator of the user's locale, so Excel opens it in columns.
Exit codes: `0` written, `1` invalid arguments (`/?` prints the usage
to the calling console), `2` scan failed, `3` report not written. For a
rollout, ship the signed exe and a shortcut with the path for users, or run
the `/report` line from a logon script or scheduled task.

## Documentation

| Topic | |
|---|---|
| [Architecture and current state](docs/ARCHITECTURE.md) | components, scan flow, dependencies, pipeline (with diagrams) |
| [Build and test](BUILDING.md) | Visual Studio 2022, CMake presets for x86/x64/ARM64 |
| [Releasing](docs/RELEASING.md) | signed release runbook, one-time setup, troubleshooting |
| [User guide](README.TXT) | usage, disk accounting, partial scans, version history |
| [Backlog and decisions](BACKLOG.md) | priorities, distribution and UI constraints |
| [Changelog](CHANGELOG.md) | release notes source |
| [Contributing](CONTRIBUTING.md) · [Code of Conduct](CODE_OF_CONDUCT.md) · [Security](SECURITY.md) | |

Historical implementation records:
[treemap design](docs/superpowers/specs/2026-08-15-treemap-layout-engine-design.md) /
[plan](docs/superpowers/plans/2026-08-15-treemap-layout-engine.md),
[live-scan design](docs/superpowers/specs/2026-08-15-live-progressive-treemap-scan-design.md) /
[plan](docs/superpowers/plans/2026-08-15-live-progressive-treemap-scan.md).
They describe the original work; implementation-status notes record later decisions.

## Support the project

SpaceMonger is free and funded by donations:
[GitHub Sponsors](https://github.com/sponsors/JRpersonal) ·
[Ko-fi](https://ko-fi.com/streborn) ·
[PayPal](https://paypal.me/JR31337).

## Open source

SpaceMonger is free software under the [MIT License](LICENSE.txt).

- **Original author:** Sean Werkema, SpaceMonger 1.0 to 1.4.0 (1998–2000),
  source published as [seanofw/spacemonger1](https://github.com/seanofw/spacemonger1).
- **Modernization:** Johnny J. Andersen, releases 1.4.1 to 1.4.4 at
  [Daduck/spacemonger](https://github.com/Daduck/spacemonger).
- **This fork:** Jens Roggenfelder, review fixes, signed releases and
  production CI/CD. Changes that are not fork-specific are offered upstream.

The executable statically links MFC and the MSVC runtime and has no other
dependencies. Licenses of everything involved are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
