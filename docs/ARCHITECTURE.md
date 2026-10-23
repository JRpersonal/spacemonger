# SpaceMonger: architecture and current state

State of `master` on 2026-10-23, version **1.4.4** (next release from this
fork: 1.4.5). A short tour for anyone who wants to understand, build or ship
SpaceMonger.

## At a glance

| | |
|---|---|
| What | Native Windows disk-space visualizer: a treemap of a drive, folder sizes at a glance, zoom, open, delete to Recycle Bin |
| Language / UI | C++17, MFC (static), Win32 GDI, Unicode (`UNICODE`/`_UNICODE`) throughout |
| Targets | Windows 10/11/Server, x86, x64, ARM64; Per-Monitor V2 DPI aware, long paths |
| Distribution | One standalone `SpaceMonger-<arch>.exe` (≈2.7 MB x64), no installer, no runtime dependencies |
| Size of code | ≈6,500 lines in 47 source files, ≈2,200 lines in 13 test programs (CTest) |
| License | MIT, © Sean Werkema, Johnny J. Andersen, Jens Roggenfelder ([lineage](../THIRD-PARTY-NOTICES.md)) |

## Where it comes from

```mermaid
flowchart LR
  A["seanofw/spacemonger1<br/>SpaceMonger 1.4.0 (2000)<br/>Sean Werkema · MIT since 2020"]
  B["Daduck/spacemonger<br/>1.4.1 to 1.4.4 (2026)<br/>Johnny J. Andersen<br/>CMake, Unicode, multithreaded scan,<br/>live treemap, x64/ARM64"]
  C["JRpersonal/spacemonger<br/>this fork (2026)<br/>Jens Roggenfelder<br/>review fixes, signed releases,<br/>production CI/CD"]
  A -- fork --> B -- fork --> C
  B -. "weekly upstream-drift issue" .-> C
```

## What the user sees

```text
┌─ SpaceMonger - C:\ ───────────────────────────────────────────────┐
│ [Open][Zoom In][Zoom Out][Zoom Full][Run/Open][Delete][Setup]  Skipped: 3 │
├───────────────────────────────────────────────────────────────────┤
│┌ Windows ─────────────────────┐┌ Users ─────────────────────────┐│
││┌ System32 ────────┐┌ WinSxS ┐││┌ jens ────────────────────────┐││
│││ driverstore      ││        ││││┌ AppData ──────┐┌ Videos ───┐│││
│││                  ││        ││││ Local          ││ holiday.mp4││││
││└──────────────────┘└────────┘│││└───────────────┘└───────────┘│││
│└──────────────────────────────┘│└──────────────────────────────┘││
│┌ Program Files ──────┐┌ pagefile.sys ┐┌ Free space ────────────┐ │
│└─────────────────────┘└──────────────┘└────────────────────────┘ │
└───────────────────────────────────────────────────────────────────┘
  rectangle area ∝ bytes · nesting = folders · colour = depth
```

`Skipped: N` appears only for partial scans and opens the details dialog.
For a partial whole-drive scan an *Unavailable (estimate)* block covers
volume-used space that the scan could not account for.

## Components

```mermaid
flowchart TB
  subgraph UI["MFC user interface (UI thread)"]
    App["CSpaceMonger<br/>(CWinApp, settings in registry)"]
    Frame["CMainFrame<br/>toolbar, scan notice button"]
    View["CFolderView<br/>(CFreeView) paints the treemap,<br/>hit-testing, zoom, selection"]
    Tip["CTipWnd<br/>info / name tips"]
    Dlg["Dialogs<br/>Drive, Settings, About,<br/>Scan details"]
    Lang["lang_en / lang_fr<br/>UI strings"]
    Doc["CFolderTree<br/>(CFreeDoc) owns the scanned tree,<br/>scan dialog, shell actions"]
  end
  subgraph Core["Core (no MFC, unit tested)"]
    Engine["AsyncScanEngine<br/>worker pool, task queue,<br/>tree mutex, live snapshots"]
    Folder["CFolder + CFolderEntryArrays<br/>names, sizes, times, children"]
    Arena["CStringArena<br/>per-worker name storage"]
    Layout["TreemapEngine<br/>pure layout + hit-test"]
    Disk["DiskUsage / ScanAccounting<br/>allocated size, sparse, cloud placeholders"]
    Path["PathUtil<br/>long-path-safe joins"]
  end
  Win["Win32 / Shell<br/>FindFirstFileW / FindNextFileW, GetCompressedFileSizeW,<br/>IFileOperation (Recycle Bin), ShellExecuteW, GDI"]

  App --> Frame --> View
  View --> Tip
  Frame --> Dlg
  View --> Doc
  Doc --> Engine
  Engine --> Folder --> Arena
  Engine --> Disk
  Engine --> Path
  View --> Layout
  Engine -- "GenerateLiveLayout" --> Layout
  Engine --> Win
  View --> Win
  Frame -.-> Lang
```

| File(s) | Responsibility |
|---|---|
| `SpaceMonger.cpp/.h` | Application object, settings (`CCurrentSettings`), registry persistence |
| `Mainfrm.cpp/.h` | Main window, classic 3D toolbar, partial-scan notice |
| `FolderView.cpp/.h`, `Freeview.cpp`, `xapp.h` | Treemap window: double-buffered GDI painting, rollover, zoom, context menu |
| `FolderTree.cpp/.h`, `Freedoc.cpp` | Document: starts scans, receives the finished tree, shell actions |
| `AsyncScanEngine.cpp/.h` | Multithreaded scanner, cancellation, partial/failed outcomes, live layout snapshots |
| `Folder.cpp/.h`, `FolderEntryArrays.*`, `FolderSort.h`, `StringArena.h` | In-memory tree and its memory management |
| `TreemapLayout.cpp/.h` | Headless treemap layout and hit-testing |
| `DiskUsage.*`, `ScanAccounting.h`, `PathUtil.*` | Size accounting and path handling |
| `TipWnd*`, `UnicodeFontFallback.h` | Tooltips and font fallback for non-Latin names |
| `*Dialog*.cpp`, `SetupDlg.cpp` | Drive picker, settings, About, scan details |
| `CommandPolicy.h`, `ScanNotice.h` | Small pure policies (which commands are allowed, notice text) |
| `SpaceMonger.rc`, `res/` | Dialogs, version info, toolbar bitmaps, icon |
| `legacy/` | Original Visual C++ 6 project files (history only) |

## How a scan works

```mermaid
sequenceDiagram
  autonumber
  actor U as User
  participant V as CFolderView / CMainFrame (UI thread)
  participant T as CFolderTree
  participant E as AsyncScanEngine
  participant W as Worker threads (one per CPU)
  participant FS as NTFS / Win32

  U->>V: Open, pick drive or folder
  V->>T: start scan
  T->>E: StartScan(root, clusterMask, threads)
  E->>W: enqueue root task
  loop each directory
    W->>FS: FindFirstFileW / FindNextFileW
    FS-->>W: entries (name, size, attributes, reparse tag)
    W->>W: allocated size, skip junctions/symlinks, cloud placeholders
    W->>E: add files to CFolder (tree mutex), enqueue subfolders
  end
  loop every 250 ms while scanning (only if changed)
    V->>E: GenerateLiveLayout(w, h)
    E-->>V: self-contained TreemapNode snapshot (names copied)
    V->>V: paint off-screen, blit (no flicker)
  end
  W-->>E: queue drained
  E-->>T: DetachResult() complete, partial or failed
  T-->>V: final tree, Skipped: N notice if partial
  V->>V: TreemapEngine layout and paint
```

Design points worth knowing:

- **Live snapshots own their data.** `GenerateLiveLayout` copies names into the
  snapshot, so painting never touches the tree that workers are changing.
- **Lock cost was measured, not guessed**: on 2 million files the live
  layout cost ≈3 % throughput (`docs/LIVE_SCAN_BENCHMARK.md`, `BACKLOG.md`).
- **Honest results**: failed roots are errors; access-denied or >128-level
  subtrees make the result *partial* and are listed in the details dialog.
- **Hard links are counted per link** (known limitation, see `README.TXT`).

## Dependencies

```mermaid
flowchart LR
  subgraph Ship["Shipped inside SpaceMonger.exe"]
    MFC["MFC (static)"]
    CRT["MSVC CRT + STL (static, /MT)"]
  end
  subgraph OS["Provided by Windows"]
    DLL["kernel32 · user32 · gdi32 · gdiplus · advapi32<br/>shell32 · shlwapi · comctl32 · uxtheme<br/>ole32 · oleaut32 · oleacc · imm32 · msimg32 · winmm"]
  end
  subgraph Build["Build only"]
    VS["Visual Studio 2022 v143 + MFC component"]
    CM["CMake ≥ 3.25 / CTest"]
    SDK["Windows SDK (rc, signtool)"]
  end
  EXE(["SpaceMonger.exe"])
  MFC --> EXE
  CRT --> EXE
  EXE --> DLL
  Build --> EXE
```

No third-party libraries, no package manager, nothing downloaded at build or
run time. Licenses of everything involved, including the CI actions, are in
[THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md).

## Build, test and release pipeline

```mermaid
flowchart LR
  PR["Pull request"] --> CI
  subgraph CI["build.yml (every PR and push)"]
    L["lint: actionlint,<br/>PSScriptAnalyzer,<br/>version check"]
    B["build matrix<br/>x86 · x64 · ARM64<br/>Debug + Release"]
    TST["CTest (12 programs)<br/>x86 + x64"]
    B --> TST
  end
  PR --> CQL["CodeQL C/C++"]
  CI --> M["master (protected)"]
  CQL --> M
  M -- "gh workflow run release.yml -f version=vX.Y.Z" --> R
  subgraph R["release.yml"]
    VS1["verify-source<br/>tag, version, CHANGELOG,<br/>build + test"]
    BLD["build x86/x64/ARM64<br/>(GitHub runners)"]
    SGN["sign-windows<br/>self-hosted 'signing' runner<br/>Certum SimplySign · signtool"]
    PUB["publish<br/>SHA256SUMS · Sigstore<br/>VirusTotal gate · release"]
    WG["winget PR (komac)"]
    VS1 --> BLD --> SGN --> PUB --> WG
  end
  W1["weekly: Scorecard · OSV · CodeQL<br/>release dry-run · upstream drift"]
```

Signing in detail, and what to do when it fails: [RELEASING.md](RELEASING.md).

## Quality gates

| Gate | Where | Blocks merge / release |
|---|---|---|
| Build x86/x64/ARM64, Debug + Release, `/W3` | `build.yml` | merge |
| 12 CTest programs (scanner incl. Unicode and access-denied fixtures, treemap, accounting, allocation failure) | `build.yml` | merge |
| CodeQL C/C++ | `codeql.yml` | merge |
| actionlint, PSScriptAnalyzer, version consistency | `build.yml` (lint) | merge |
| Version = tag, CHANGELOG entry, tag unused | `release.yml` verify-source | release |
| Authenticode signature present and valid | `release.yml` sign-windows / publish | release |
| VirusTotal broad detection (≥10 engines or ≥15 %) | `release.yml` publish | release |
| Published assets match `SHA256SUMS` | `release.yml` publish | release (post-check) |
| OpenSSF Scorecard, OSV-Scanner | weekly | report only |

Not automated: interactive checks of scan dialogs, mixed-DPI rendering and
shell operations, and ARM64 test execution (build only).

## Open points

From [BACKLOG.md](../BACKLOG.md): x64 conversion warnings in `DriveDialog.cpp`
and `Freedoc.cpp`, interactive verification of partial-scan dialogs and DPI,
incremental live-size propagation (deferred after benchmarking), contiguous
`CFolder` layout, dark mode, folder/UNC picker, filters.
