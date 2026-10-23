# Third-party notices and project lineage

SpaceMonger is released under the [MIT License](LICENSE.txt). This file
records where the code comes from, what the shipped executable contains, and
which third-party tools build and publish it.

## Project lineage

| Project | Role | License | Copyright |
|---|---|---|---|
| [seanofw/spacemonger1](https://github.com/seanofw/spacemonger1) | Original SpaceMonger 1.4.0 source (1998–2000), published 2020 for digital preservation | MIT | © 1998–2020 Sean Werkema |
| [Daduck/spacemonger](https://github.com/Daduck/spacemonger) | Modernization: CMake/VS2022, Unicode, long paths, multithreaded scanner, live treemap, x64/ARM64 (releases 1.4.1–1.4.4) | MIT (inherited) | © 2026 Johnny J. Andersen |
| [JRpersonal/spacemonger](https://github.com/JRpersonal/spacemonger) (this repository) | Fork: review fixes, hardened CI/CD, signed releases | MIT (inherited) | © 2026 Jens Roggenfelder |

All three use the same, unmodified MIT text. Each fork kept the original
notice and added its own copyright line; nothing was relicensed. When you
redistribute SpaceMonger or code from it, keep [LICENSE.txt](LICENSE.txt)
with it.

The `legacy/` folder holds the original Visual C++ 6 project files
(`SpaceMonger.dsp`, `SpaceMonger.dsw`) from Sean Werkema's source release,
under the same MIT license. The bitmaps and icon in `res/` are original
SpaceMonger artwork, also from that release.

The original 1.4.0 `README.TXT` described SpaceMonger as freeware that may be
distributed freely; the MIT license added to the source in 2020 supersedes
that wording for the code.

## What the executable contains

`SpaceMonger.exe` is a single, statically linked Win32 binary. Apart from
SpaceMonger's own code it contains:

| Component | Origin | License |
|---|---|---|
| Microsoft Foundation Classes (MFC), static | Visual Studio 2022 (v143 toolset) | Microsoft Visual Studio license terms; MFC static libraries are "Distributable Code" that may be linked into and redistributed with applications |
| Microsoft C/C++ runtime and C++ Standard Library (static, `/MT`) | Visual Studio 2022 | Microsoft Visual Studio license terms (Distributable Code). The MSVC STL is also open source under Apache-2.0 WITH LLVM-exception |

There are no other bundled libraries, no package-manager dependencies and no
downloaded components. At run time the program imports only Windows system
DLLs that ship with every Windows installation (x64 build of 2026-10-23:
`kernel32`, `user32`, `gdi32`, `gdiplus`, `advapi32`, `shell32`, `shlwapi`,
`comctl32`, `uxtheme`, `ole32`, `oleaut32`, `oleacc`, `imm32`, `msimg32`,
`winmm`; most of them are pulled in by static MFC). The treemap typeface ("Small
Fonts", with a Unicode fallback font for other scripts) is a font installed
with Windows and is not embedded.

## Build and test tools (not redistributed)

| Tool | Used for | License |
|---|---|---|
| [CMake](https://cmake.org) ≥ 3.25 | Build system, CTest test runner | BSD-3-Clause |
| Visual Studio 2022 / MSVC v143, Windows SDK | Compiler, resource compiler, `signtool.exe` | Microsoft license terms |
| Certum SimplySign Desktop | Access to the cloud-held code signing key | Certum (Asseco Data Systems S.A.) terms |

The unit tests in `tests/` are plain C++ with no test framework.

## CI/CD actions (GitHub Actions, not redistributed)

Every action is pinned to a full commit SHA; Dependabot keeps the pins current.

| Action | Used in | License |
|---|---|---|
| [actions/checkout](https://github.com/actions/checkout) | all workflows | MIT |
| [actions/upload-artifact](https://github.com/actions/upload-artifact), [actions/download-artifact](https://github.com/actions/download-artifact) | build, release | MIT |
| [actions/attest-build-provenance](https://github.com/actions/attest-build-provenance) | release (Sigstore provenance) | MIT |
| [github/codeql-action](https://github.com/github/codeql-action) | CodeQL, Scorecard upload | MIT |
| [ossf/scorecard-action](https://github.com/ossf/scorecard-action) | OpenSSF Scorecard | Apache-2.0 |
| [google/osv-scanner-action](https://github.com/google/osv-scanner-action) | OSV scan | Apache-2.0 |
| [softprops/action-gh-release](https://github.com/softprops/action-gh-release) | release publishing | MIT |
| [dependabot/fetch-metadata](https://github.com/dependabot/fetch-metadata) | Dependabot auto-merge | MIT |
| [crazy-max/ghaction-virustotal](https://github.com/crazy-max/ghaction-virustotal) | release VirusTotal upload | MIT |
| [rhysd/actionlint](https://github.com/rhysd/actionlint) (container) | workflow lint | MIT |
| [Komac](https://github.com/russellbanks/Komac) (CLI, run in CI only) | winget manifest updates | GPL-3.0 (tool only; does not affect SpaceMonger's license) |

## Trademarks

Windows, Visual Studio and MFC are trademarks of Microsoft Corporation. Certum
and SimplySign are trademarks of Asseco Data Systems S.A. Their use here is
descriptive only.
