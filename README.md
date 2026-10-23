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

- Treemap of a drive or folder, coloured by depth; zoom in/out, run/open,
  properties, delete to the Recycle Bin, right-click menu.
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
