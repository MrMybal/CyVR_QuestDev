---
id: updates
title: Update and restore
description: Select a release channel, verify a download and restore the previous installation.
section: Preferences
parent: settings
order: 81
revision: 1
---

## Find and download a release {#download}

Open **Settings → GitHub updates**. Select **Alpha and prereleases** for Alpha versions; stable mode excludes prereleases. The newest compatible published version is chosen by its version number, without proposing a downgrade. Drafts and incompatible distributions are ignored. Checking at startup is optional and initially off; downloading remains explicit.

The repository is public, so release access does not require signing in, subject to GitHub request limits. **Existing Git connection** can reuse Git Credential Manager without an interactive background prompt. For authenticated access without Git, enter a fine-grained token limited to this repository with **Contents: read** in the masked field. It is kept in Windows Credential Manager. **Forget token** removes only QuestDev's entry, not Git or AI provider accounts.

**Download and verify** retrieves the Windows x64 ZIP, checks its SHA-256 against GitHub metadata and `SHA256SUMS.txt`, then checks the extracted file manifest. API authorization is not forwarded to redirected file-download hosts. Downloads can be cancelled; an incomplete or rejected package is not installed.

```image
{"id":"update-settings","src":"assets/github-updates.png","alt":"GitHub release channels and update actions in demonstration settings","controls":"minimal"}
```

## Install and restart {#install}

Finish or cancel file queues and stop captures, recording and mirroring before installing. **Install and restart** prepares the new installation beside the current folder. A separate updater waits for QuestDev to close normally, saves the old directory as a backup, replaces it, and waits for the new app to confirm that its UI and services started. If startup fails, it attempts to restore and relaunch the backup.

Preferences in `%LOCALAPPDATA%/CyVR/QuestDev` remain. Additional personal files and user plugins in the app directory are preserved. If a shipped file was modified or a personal file conflicts with a new component, automatic installation is refused before replacement.

Automatic installation needs a dedicated writable folder from the portable ZIP or per-user installer. Development builds, Git checkouts, system directories and links/junctions in the replaced tree are refused. No administrator elevation is launched. Large personal folders may require a manual installation into a dedicated location. Windows shortcuts and uninstaller files are preserved for installed distributions.

## Restore a backup {#restore}

**Restore and restart** returns to the backup associated with the current application folder. Backups are not deleted automatically. **Open updates folder** shows downloads and update results; installation backups are in neighbouring directories of the installed application.

Updates are available from QuestDev 0.7 onward. Older versions need an initial manual download. Source checkouts use their development build workflow rather than automatic replacement. The [release page](https://github.com/MrMybal/CyVR_QuestDev/releases) always provides the published installer, portable archive, sources and checksums.
