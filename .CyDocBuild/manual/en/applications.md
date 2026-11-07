---
id: applications
title: Manage applications
description: Count, filter, install, launch and stop Quest packages.
section: Tools
order: 20
revision: 1
---

## Find a package {#list}

Open **Applications** and choose user applications, running applications, system packages or all packages in its own left navigation. The search narrows the current category. The counters show detected and matching applications after filtering; they are not a count of log lines.

Refresh packages after installing software outside QuestDev. Select the intended headset in the top bar before applying an action.

```image
{"id":"application-list","src":"assets/applications.png","alt":"Quest package table with category counters and package actions in demonstration mode","controls":"minimal"}
```

## Install, launch and stop {#manage}

- **Install an APK / Installer un APK** opens a local file selector and installs that APK on the selected Quest. Use a compatible regular APK; APK bundle installation is not implemented.
- The **launch** action opens the package's launchable activity when Android exposes one. A package without an activity may be a service rather than a directly launchable app.
- **Stop** requests an Android force-stop for that package. It interrupts that application; use it deliberately during a development session.
- **Logs** opens that package in [Console](doc:console#packages). A listed package may have no retained messages yet.

These actions depend on ADB authorization and Android's permissions. An accepted command does not establish that the application's own initialization succeeded. Read the task output in [ADB](doc:device#adb) and the application's logs if it does not start as expected.

To configure Quest-side tools, use their [plugins](doc:plugins#ovr). Installing OVR Metrics or CyInsightOVR is distinct from installing QuestDev on the PC.
