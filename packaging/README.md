# Package managers

| Channel | What users type | Updated by |
|---|---|---|
| GitHub Releases | download `SpaceMonger-x64.exe` (stable link: `releases/latest/download/SpaceMonger-x64.exe`) | `release.yml` |
| winget | `winget install JRpersonal.SpaceMonger` (or `winget install spacemonger`) | `release.yml` job `package-managers` (komac) |

## winget

The package `JRpersonal.SpaceMonger` was first submitted with version 1.4.8 in
[microsoft/winget-pkgs#467270](https://github.com/microsoft/winget-pkgs/pull/467270)
(2026-10-31). Once a winget maintainer has merged that pull request, every
real (non-pre-release) release opens the update pull request by itself:
komac runs with `WINGET_TOKEN` (classic PAT with `public_repo`) from the fork
`JRpersonal/winget-pkgs`. The winget maintainers usually merge updates within
a few days.

What the package looks like (see
[the 1.4.8 manifests](https://github.com/microsoft/winget-pkgs/tree/master/manifests/j/JRpersonal/SpaceMonger)):

- `InstallerType: portable`, one installer per architecture (x64, arm64, x86),
  pointing at the signed release assets; winget puts the executable on `PATH`
  as `spacemonger`.
- `Publisher: JRpersonal`, `Author: Jens Roggenfelder`, the same as
  `JRpersonal.STReborn`.
- Manifest schema 1.12.0, `MinimumOSVersion: 10.0.0.0`.

### If a version has to be submitted by hand

For example when the `package-managers` job failed and the release is already
out. With [komac](https://github.com/russellbanks/Komac) installed
(`winget install RussellBanks.Komac`):

```powershell
$v = '1.4.9'
$base = "https://github.com/JRpersonal/spacemonger/releases/download/v$v"
$env:GITHUB_TOKEN = '<classic PAT with public_repo>'
komac update JRpersonal.SpaceMonger `
  --version $v `
  --urls "$base/SpaceMonger-x64.exe" "$base/SpaceMonger-arm64.exe" "$base/SpaceMonger-x86.exe" `
  --release-notes-url "https://github.com/JRpersonal/spacemonger/releases/tag/v$v" `
  --submit
```

Before submitting, check that there is no open pull request for the same
version: `gh search prs --repo microsoft/winget-pkgs JRpersonal.SpaceMonger --state open`.
