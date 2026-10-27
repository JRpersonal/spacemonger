# Changelog

All notable changes to this fork. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/). The release
workflow publishes the section whose heading matches the release version, so
every release needs a `## [X.Y.Z]` section here first.

## [Unreleased]

## [1.4.8] - 2026-10-26

### Added
- The window stays usable during a scan. The scan no longer runs in a nested
  message loop: a timer in the main window drives it. *Clean Up*, *Settings*
  and *About* keep working while it runs, `Esc` or *Cancel* stops it, and
  the live map is laid out less often on big drives so it never takes more
  than about a ninth of the time.
- Scan cache: the last finished scan of a place is saved under
  `%LOCALAPPDATA%\SpaceMonger\cache` and shown at once (with its date) while
  the new scan runs, so the map can be browsed and zoomed right away. The new
  scan always runs in full and replaces it; nothing is skipped or trusted.
  Scans of more than 3 million entries are not cached. Off with *Settings >
  Keep the last scan...* (or `scan_cache` = 0 under
  `HKCU\Software\65\SpaceMonger\1.3`).
- *Clean Up* works in the background with a progress bar, the location being
  emptied and a *Stop* button, and shows how many locations are measured.
- Right-click > *Copy path and size* puts the full path, size (readable and
  in bytes) with share of the drive, folder contents, last change and the
  computer name on the clipboard.
- The version is in the window title (`SpaceMonger 1.4.7`).

### Fixed
- *Show in Explorer* in the HTML report did nothing unless `/register` had
  been run. The window now registers the `spacemonger:` links for the current
  user whenever it writes a report (and again if the program was moved).

### Changed
- `Ctrl+E` is the *Report* button; `Ctrl+Shift+E` still asks where to save and
  offers CSV. *Export list* is gone from the right-click menu: it did the same
  as *Report*, and the menu is about the item under the pointer.

## [1.4.7] - 2026-10-24

### Added
- HTML report: `/html <file or folder>` (and *Export list*, `Ctrl+E`) writes
  one self-contained page with the map as in the window, hover details, zoom
  by double-click, Explorer on a double-clicked file (as in the window) and
  the biggest folders and files. No network needed; a
  whole drive is a few hundred KB, so it fits into OneDrive or a mail.
- *Report* button in the toolbar, in the accent colour: writes the HTML page
  of the scan to *Documents\SpaceMonger* and opens it in the browser.
- *Show in Explorer* from the HTML page through `spacemonger:` links,
  set up per user with `/register` (`/unregister` removes them). Only plain
  local paths are accepted; a link never deletes or changes anything.

### Fixed
- *By file type*: blocks of files too small to draw one by one were grey
  (*Other*); they now take the colour of their largest file.
- Window title after zooming into a folder scan lacked the backslash after
  the scanned folder (`C:\FileServerUsers\`).

### Changed
- Settings take effect as soon as they are chosen, not only after *OK*;
  *Cancel* puts everything back.
- Toolbar icons in dark mode are drawn with stronger strokes; disabled ones
  are lighter, so both are easy to tell apart from the background.
- The search field is larger and stands out: a rounded field with a
  magnifier, an accent frame while typing, and a hint readable in dark mode.
- README: starts with the problem SpaceMonger solves and what people use it
  for, then the features with screenshots and close-ups; what this fork is
  for, and a rollout guide for IT (per-user HTML reports through Intune or
  a scheduled task, help desk, inventory).

## [1.4.6] - 2026-10-24

### Added
- Command line: `SpaceMonger.exe <path>` scans a drive, folder or share
  (`\\PC01\c$`) directly; `/report <file or folder>` scans without a window,
  writes a CSV report and exits (`/top N`, `/hide-system`, exit codes).
- CSV export of the biggest folders and files (right-click, `Ctrl+E`).
- Scan any folder from the drive dialog (*Folder...*) or by dropping it on
  the window.
- *Free Up Space* dialog: temporary files, caches and Recycle Bin in one
  click; biggest personal files with tips, moved to the Recycle Bin together.
- *Run as administrator* toolbar button; German translation; automatic
  language from Windows; colour schemes including *By file type*.
- Status strip with progress instead of the scan dialog; drive dialog opens
  at start with the first drive selected; richer tooltips with full path.
- Donation links (GitHub Sponsors, Ko-fi, PayPal).
- Path bar between toolbar and map: click a folder to go back to it.
- Search (`Ctrl+F`): matching files and folders keep their colours, the
  rest is washed out; the bar shows the number of matches.
- Colour legend for *By file type*.
- Dark mode (toolbar, path bar, status strip, map background, title bar),
  automatic with Windows or chosen in the settings.
- Folder scans no longer show the drive's free space as a block.

### Changed
- Windows look: Common Controls v6, Fluent icon toolbar, Segoe UI dialogs,
  fonts scale with DPI and Windows text size.
- Live scan: the volume layout matches the final map, folders appear once
  complete, and frames blend briefly instead of jumping.
- System folders are muted and can be shown or hidden (*System* button);
  the *not readable* estimate counts as system content.
- Settings reduced to what people change: detail, colours, hover details,
  highlight, Delete lock, language and theme.
- The right-click menu no longer repeats toolbar commands; the
  `Skipped: N` button is gone (the *not readable* block explains it).

### Fixed
- v1.4.5 did not start (duplicate `dpiAware` in the manifest).
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
