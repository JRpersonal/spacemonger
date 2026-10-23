# Contributing

Thanks for helping. SpaceMonger is a small, native Win32/MFC program; the
goal is careful modernization, not a rewrite (see the decisions in
[BACKLOG.md](BACKLOG.md)).

## Before you start

- Bugs and ideas: open an [issue](https://github.com/JRpersonal/spacemonger/issues/new/choose).
- Security problems: see [SECURITY.md](SECURITY.md), not a public issue.
- Fixes that are not specific to this fork are best sent upstream to
  [Daduck/spacemonger](https://github.com/Daduck/spacemonger) as well; the
  weekly *upstream drift* job shows what moved there.

## Build and test

Visual Studio 2022 with the MFC component and CMake 3.25+ (details in
[BUILDING.md](BUILDING.md)):

```powershell
cmake --preset vs2022-x64
cmake --build --preset vs2022-x64-debug
ctest --preset vs2022-x64-debug --timeout 60
```

## Pull requests

- One topic per pull request, against `master`.
- Commit subjects follow [Conventional Commits](https://www.conventionalcommits.org/)
  (`fix:`, `feat:`, `docs:`, `ci:` ...). `feat`/`fix` subjects appear in the
  release notes, so write them for users.
- Add a line to `## [Unreleased]` in [CHANGELOG.md](CHANGELOG.md) for
  user-visible changes.
- CI must be green: build and tests on x86/x64 (ARM64 build only), CodeQL,
  lint (workflows, PowerShell tooling, version consistency).
- New UI strings go into every language in `lang_en.cpp` / `lang_fr.cpp`.
- Keep the classic look and the single-`.exe` distribution (no installer, no
  extra files, no new runtime dependencies).

## License

By contributing you agree that your contribution is released under the
[MIT License](LICENSE.txt) of this project.
