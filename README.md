# harbour-claudechat

Unofficial native Sailfish OS client (Silica/QML + C++) for claude.ai chats.
No WebView, no web login, no API key: the app uses the `sessionKey` cookie of a
claude.ai login, stored once, for every request.

## Setup

1. Log in to claude.ai in a desktop browser.
2. Developer tools (F12) > Storage/Application > Cookies > `https://claude.ai`,
   copy the value of `sessionKey` (starts with `sk-ant-sid`).
3. First app start: paste it and accept. The app stays logged in until the key
   expires; then it asks for a new one.

## Features

- Chat list of the account (pull down: new chat, refresh, log out).
- Open a chat, read its messages (current branch), continue it.
- New chats; claude.ai generates the title.
- Answers stream in; stop button while streaming.
- Markdown in answers: code blocks, inline code, headings, bold/italic, lists, quotes, tables, links.
- Delete a chat (long press), copy a message (long press).
- Attach files to a message (paperclip): text/code files are sent inline; images and PDFs
  are uploaded to claude.ai (`POST /api/{org}/upload`, unverified).
- Claude | Code switch: Claude Code sessions (claude.ai/code): list, read messages and tool calls
  (refreshed while open), send a message into a session, open it in the browser.
- Usage page (pull down): plan limits with percent used and reset time (5-hour session,
  weekly, weekly per model); 5-hour and weekly percent also in the start page header.
- Notifications: Claude answered (app in the background); a Code session stopped running
  (checked every 30 s while the app runs).

Uses the private API of the claude.ai web client; claude.ai can change it at any time.

| Feature | Request |
| --- | --- |
| Account | `GET /api/organizations` |
| Chat list | `GET /api/organizations/{org}/chat_conversations` |
| Messages | `GET …/chat_conversations/{id}?tree=True&rendering_mode=messages` |
| New chat | `POST …/chat_conversations` |
| Send (SSE stream) | `POST …/chat_conversations/{id}/completion` |
| Title | `POST …/chat_conversations/{id}/title` |
| Delete | `DELETE …/chat_conversations/{id}` |
| Usage | `GET /api/organizations/{org}/usage` (unverified) |
| Code sessions | `GET /v1/sessions`, `GET /v1/sessions/{id}`, `GET/POST /v1/sessions/{id}/events` (unverified) |

Not for Harbour (private web API). Distribute via GitHub releases, OpenRepos or Chum.

## Files on the phone

- `sessionKey`: `~/.config/org.roterfux/harbour-claudechat/settings.conf` (mode 600).

## Build

In WSL on the PC with the Sailfish Platform SDK chroot (see `CLAUDE.md`):

```bash
wsl -d FedoraLinux-44 -- sh /mnt/c/Users/jgrei/PycharmProjects/PythonProject/harbour-claudechat/tools/build-rpm.sh
```

The RPM lands in `RPMS/`.
