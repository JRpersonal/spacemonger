# Package managers

| Channel | What users type | Updated by |
|---|---|---|
| GitHub Releases | download `SpaceMonger-x64.exe` (stable link: `releases/latest/download/SpaceMonger-x64.exe`) | `release.yml` |
| winget | `winget install JRpersonal.SpaceMonger` | `release.yml` job `package-managers` (komac), after the first manual submission |

There is no SpaceMonger package in `microsoft/winget-pkgs` yet, so the first
version is submitted once by hand; every later release then opens the update
pull request by itself.

## First winget submission (after the first signed release of this fork)

Requires the fork `JRpersonal/winget-pkgs` (exists) and a classic PAT with
`public_repo` (same token as `WINGET_TOKEN`). With
[komac](https://github.com/russellbanks/Komac) installed
(`winget install RussellBanks.Komac`):

```powershell
$v = '1.4.5'
$base = "https://github.com/JRpersonal/spacemonger/releases/download/v$v"
$env:GITHUB_TOKEN = '<classic PAT with public_repo>'
komac new JRpersonal.SpaceMonger `
  --version $v `
  --urls "$base/SpaceMonger-x64.exe" "$base/SpaceMonger-arm64.exe" "$base/SpaceMonger-x86.exe" `
  --package-locale en-US `
  --publisher "Jens Roggenfelder" `
  --publisher-url "https://github.com/JRpersonal" `
  --package-name "SpaceMonger" `
  --package-url "https://github.com/JRpersonal/spacemonger" `
  --license "MIT" `
  --license-url "https://github.com/JRpersonal/spacemonger/blob/master/LICENSE.txt" `
  --copyright "(c) 1998-2020 Sean Werkema, (c) 2026 Johnny J. Andersen, Jens Roggenfelder" `
  --short-description "Disk space visualizer that shows your drive as a treemap" `
  --moniker spacemonger `
  --release-notes-url "https://github.com/JRpersonal/spacemonger/releases/tag/v$v" `
  --submit
```

komac asks for anything it cannot infer (installer type is `portable`, one
installer per architecture). The winget maintainers review the first pull
request manually; once it is merged, `WINGET_TOKEN` lets the release workflow
handle every later version.
