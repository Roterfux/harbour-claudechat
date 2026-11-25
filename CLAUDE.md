# CLAUDE.md

Always answer in caveman style (use the caveman skill).

## Version numbers

- Every change bumps `Version:` in `rpm/harbour-claudechat.spec`, in the same commit as the change.
- Bug fix or small tweak: patch bump (0.1.0 -> 0.1.1).
- New feature or bigger change: minor bump, patch reset (0.1.3 -> 0.2.0).
- `Release:` stays `1`.
- Releases: CI creates release `vX.Y.Z` (tag + RPMs) on the first push to main with a new spec version,
  and deletes merged `claude/*` branches. No manual tagging needed.

## Build

- Build only in WSL on the user's PC, never in a cloud session or container.
  Repo on the PC: `C:\Users\jgrei\PycharmProjects\PythonProject\harbour-claudechat`.
- Command (as root in WSL, Sailfish Platform SDK chroot in `/srv/sailfishos`):
  `wsl -d FedoraLinux-44 -- sh /mnt/c/Users/jgrei/PycharmProjects/PythonProject/harbour-claudechat/tools/build-rpm.sh`
- `tools/build-rpm.sh` runs `tools/qml-check.sh` first (QML must compile with the phone's Qt 5.6),
  then `mb2 build`, and copies the RPM to `RPMS/` in the repo.
- Do not commit build output (`RPMS/`, `out/`).
