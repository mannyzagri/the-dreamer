# comms/ — async agent-to-agent channel (via git)

Same convention as rubber-rhino's comms/: a lightweight message bus so the
Claude sessions on different machines coordinate without the human relaying
every step. The repo IS the transport: pull to read your inbox, push to send.
Not real-time — mailbox, not phone call.

## Agents & identity
Every agent has a short **id** and MUST register itself in `AGENTS.md` (model +
role + machine) the first time it participates. Current ids:
- `mac-opus` — Claude Opus 4.8 (1M), macOS. Analysis, cross-checks, specs,
  Mac-side measurements (no JUCE/MSVC toolchain here).
- `vm-opus`  — Claude Opus 4.8 (1M), Windows VM. MSVC build, validator,
  pluginval, Cubase deploy — the build/ship lane.
- `human`    — Menashe (the coordinator; can post to anyone).

**On first contact, identify yourself:** add/confirm your entry in `AGENTS.md`
and post a short "online" message to `to-human.md`.

## Inboxes
One file per recipient: `to-mac.md`, `to-vm.md`, `to-human.md`. You post by
appending to the RECIPIENT's inbox. You read your OWN inbox on each poll/session.

## Message format (append to the recipient's inbox)
```
## [NEW] 2026-07-22T09:30 · from:<sender-id> → <recipient-id> · <subject>
<body — self-contained: paths, not "the thing we discussed".>
---
```
- **`[NEW]`** = unread/unactioned → flip to **`[DONE]`** when handled (reply in
  the SENDER's inbox if a response is needed); **`[ACK]`** = seen, in progress.
  Never delete messages — flip the marker; the log is the record.

## Rules
- Per-recipient files avoid merge conflicts. Push rejected → `git pull --rebase`.
- **Human-in-the-loop for risk**: deploys, force-pushes, anything hard to
  reverse → ask `human` first.
- House git rules apply: branch → merge --no-ff → push; explicit `git add`
  file lists (comms files only, for comms commits).
- The Mac session is interactive/manual (no cron) — the human prompts it to
  read `to-mac.md`. The VM session checks `to-vm.md` at session start.
