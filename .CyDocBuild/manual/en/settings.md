---
id: settings
title: Appearance and log retention
description: Set ADB, themes, density, language and the local log budget.
section: Preferences
order: 80
revision: 1
---

## Adjust the workspace {#appearance}

Open **Settings** and choose the appropriate section in its left navigation. Select `adb.exe` if automatic detection cannot find Android SDK Platform-Tools. Changes apply to the tools using the selected connection.

QuestDev offers five themes, comfortable/compact density and DPI scaling. Language and density preferences are saved. EN/FR localization applies to migrated views and remains partial in the application; this manual's pages are fully translated. Captured logs and command output are not translated.

## Set log limits {#retention}

Open **Log limits / Limites du journal**, or click the retained-line/memory counters at the bottom of the app. Set the maximum retained lines and memory budget. The default is **50,000 lines / 64 MiB**. The available controls allow 1,000–5,000,000 lines and 16–2,048 MiB, with Standard, Extended and Long-session presets.

Both limits apply to the local retained journal. Older entries are discarded when either is reached; increasing a limit later does not restore discarded entries. A view pause does not stop capture. The status bar reports discarded/lost messages, including bounded ingestion losses when applicable.

```image
{"id":"log-budget","src":"assets/log-settings.png","alt":"Configurable retained line count and memory budget in Settings","controls":"minimal"}
```

Choose a budget that fits the PC and session duration. Export useful filtered logs before they leave retention. RAW pagination is a separate display limit, described in [Console](doc:console#raw).

## Stored preferences and other settings {#storage}

User settings live under `%LOCALAPPDATA%/CyVR/QuestDev`, separately from the application installation. Plugin profiles are saved explicitly; file-pane and preparation preferences are remembered, while the transfer list itself lasts only for the session.

Use [Updates](doc:updates#download) for release channels and installation. Provider connection and action policy are in [AI assistant](doc:assistant#connect). Keep the full application directory when moving a portable distribution so its runtime, plugins and local manual remain available.
