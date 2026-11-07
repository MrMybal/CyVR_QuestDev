---
id: quickstart
title: Install and connect
description: Install the Windows Alpha and authorize your first Quest connection.
section: Start here
parent: overview
order: 1
revision: 1
---

## Install QuestDev {#install}

1. Download the Windows x64 installer from the [QuestDev releases](https://github.com/MrMybal/CyVR_QuestDev/releases). The current published application version is **0.9.0-alpha.1**.
2. Run the French/English wizard. Installation is per user, normally under `%LOCALAPPDATA%/Programs/CyVR QuestDev`, with a Start menu shortcut and optional desktop shortcut. Administrator rights are not required. The installer is unsigned.
3. Alternatively, extract the **complete Windows-x64 ZIP** into a dedicated writable folder, then run `CyVR_QuestDev.exe`. Keep all supplied directories beside it.
4. Install the official [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools) for ADB. QuestDev includes Python, CyToolsCore/MCP, plugins and scrcpy; ADB, AI provider CLIs/accounts and Quest-side tools remain separate.

The release also provides SHA-256 checksums and matching sources. Close QuestDev before reinstalling or uninstalling. Uninstallation removes application files and preserves user preferences and personal captures. [Updates](doc:updates#install) preserve the installation's shortcuts and uninstaller.

## Connect the Quest {#connect}

1. Enable developer mode for your Quest using Meta's developer setup.
2. Connect the headset with a data-capable USB cable. Put it on and accept the **USB debugging** authorization for this PC.
3. Start QuestDev. If ADB is not detected, choose `adb.exe` in **Settings**.
4. Select the headset in the top-right device selector. **F5** refreshes the device list. A device marked `unauthorized` still needs approval in the headset; an `offline` entry is not ready for commands.
5. Open **Applications** to inspect installed packages, or **Console** and select a package to read its messages.

ADB Wi-Fi is available through [Device actions](doc:device#wifi) after the required connection is established. Choose the intended headset before starting a capture, transfer or batch.

## First useful session {#session}

Start your VR application on the Quest, open its package in [Console](doc:console#packages), and keep a separate tab for any companion process you are investigating. Use [mirroring](doc:mirroring#start) if you need a PC view, or install OVR Metrics Tool and follow [OVR recording](doc:metrics#record) for performance data.

The AI assistant can stay disconnected. Connecting a provider is a separate explicit action. A missing headset does not prevent opening a local OVR CSV or reading the manual.

If the connection fails, use [connection troubleshooting](doc:troubleshooting#connection). Do not repeatedly run commands against an unauthorized or disconnected device.
