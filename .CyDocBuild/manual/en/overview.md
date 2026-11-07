---
id: overview
title: CyVR QuestDev
description: Find the tools for your Meta Quest development session.
section: Start here
order: 0
revision: 1
---

## Your workspace {#workspace}

CyVR QuestDev is a Windows desktop application for Meta Quest developers. Follow each application's logs, manage installed packages, operate the headset, transfer files, mirror its display and inspect OVR performance reports from one workspace.

The application is an **Alpha**. Some device commands and counters depend on headset firmware and external Quest applications. Windows 10/11 x64 is supported; no Linux or macOS application build is supplied. The optional AI assistant is not required for ordinary ADB workflows.

```image
{"id":"console-overview","src":"assets/console.png","alt":"QuestDev console with an application selected and simulated logs","controls":"minimal"}
```

The captures in this manual come from the real application in demonstration mode. Packages, messages and measurements are simulated. Some captures precede the shared CyUI widgets; they illustrate the workflows, rather than a live headset connection.

## Choose a task {#tasks}

| Task | Guide |
| --- | --- |
| Install and connect the headset | [First connection](doc:quickstart#connect) |
| Isolate messages and copy original logs | [Console](doc:console#raw) |
| Install, start or stop an application | [Applications](doc:applications#manage) |
| Capture or control the headset | [Device and ADB](doc:device#actions) |
| Display and record the headset on PC | [Mirroring](doc:mirroring#start) |
| Prepare a PC/Quest transfer batch | [Files](doc:files#batch) |
| Plot and record OVR reports | [Metrics](doc:metrics#record) |
| Configure OVR Metrics or CyInsightOVR | [Plugins](doc:plugins#ovr) |
| Connect an AI or an external tool | [Assistant](doc:assistant#connect), [MCP and SDK](doc:automation#connect) |
| Adjust appearance, retention or updates | [Settings](doc:settings#retention), [Updates](doc:updates#download) |

## Use this manual {#manual}

Open **Help / F1** in QuestDev for the page corresponding to the current tool. The installed manual and the website use the same generated reader. Reading the installed copy does not require a server or an internet connection. Download links and AI providers do require their own connections.

Help/F1 is included in builds from the current source. The previously published 0.9.0 Alpha binaries predate this integration; use this online manual with those binaries.

Use the reader's language selector for English or French, its page tree to navigate, and **Ctrl+K** for global search. Page and section addresses stay stable across languages. Images enlarge inside the reader; **Escape** reduces them. **My space** keeps personal bookmarks and notes where browser storage is available; persistence depends on the browser's local-file or site storage policy.

QuestDev's own interface translation remains partial. Package names, commands, captured logs and literal UI labels keep their original spelling. This manual is fully available in English and French.

See [troubleshooting](doc:troubleshooting#connection) if a device or report does not appear, and [licensing](doc:licensing#license) for distribution terms.
