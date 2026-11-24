# CLAUDE.md

Always answer in caveman style (use the caveman skill).

## Version numbers

- Every change bumps `Version:` in `rpm/harbour-claudechat.spec`, in the same commit as the change.
- Bug fix or small tweak: patch bump (0.1.0 -> 0.1.1).
- New feature or bigger change: minor bump, patch reset (0.1.3 -> 0.2.0).
- `Release:` stays `1`.
- Releases get a git tag `vX.Y.Z` matching the spec version.

## Build

- Build the RPM in the Sailfish OS Platform SDK, like CI (`.github/workflows/build.yml`):
  `coderus/sailfishos-platform-sdk:5.2.0.15` container, `mb2 -t SailfishOS-5.2.0.15-aarch64 build`.
- Pass `--no-fix-version` to `mb2` so the RPM carries the spec version, not `build.<sha>`.
- Do not commit build output (`RPMS/`, `out/`).
