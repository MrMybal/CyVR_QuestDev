---
id: troubleshooting
title: Troubleshooting
description: Diagnose device authorization, missing logs, OVR reports and media failures.
section: Support
order: 90
revision: 1
---

## No usable headset {#connection}

Check that the USB cable carries data, developer mode is enabled and the headset has authorized this PC's USB debugging request. Verify the selected `adb.exe`, refresh with F5 and select the intended device. `unauthorized` needs headset approval; `offline` is not a usable command connection. For Wi-Fi, check the actual current address and transport before retrying.

Do not confuse demonstration mode with a real device. Demo data is simulated and actions may be disabled. See [first connection](doc:quickstart#connect).

## Empty or unexpected logs {#logs}

Start the target application and check the exact package, running/installed/system category and active filters. Look in System/unassigned when an origin cannot be established. Capture starts at the current time; a quiet app can have no messages. Messages discarded by retention or missed during a disconnection are not reconstructed.

If following stopped, return to the bottom or enable Follow live. In RAW, release a frozen text selection before expecting updates. Non-time column sorts intentionally keep a stable analysis order. See [Console](doc:console#follow).

## No OVR curves {#metrics}

Install OVR Metrics Tool, keep a VR app running, refresh reports and inspect the CSV mode/statistic selection in the OVR plugin. Check access errors rather than treating an empty list as success. Firmware may publish a CSV only after the VR session ends; enabling CSV does not guarantee live output. A local CSV lets you check the plotting workflow independently of headset collection.

Do not expect missing counters to be synthesized. Check CSV format, timestamp order and [import limits](doc:metrics#csv). Reconnect and reread plugin state after a recording was interrupted so you can verify the CSV configuration.

## No picture, sound or complete capture {#media}

For mirroring, inspect its session log, the chosen ADB device and any rejected crop. Reduce bitrate or resolution for a saturated link. Audio duplication requires Android 13+ and an app permitting capture; silence may be application-specific. A forced stop can leave an incomplete MKV. Android MP4 and mirroring MKV are separate recording modes.

For CyInsightOVR, confirm collector initialization, the exact target package and matching `capture_id`. A diagnostic without tiles is not an exportable measured capture. Reread state after switching HUD tools. See [mirroring](doc:mirroring#audio) and [plugins](doc:plugins#cyinsight).

## Denied commands, transfers or AI actions {#commands}

Read the selected task's output and exit code in ADB. Android permissions and firmware capabilities still apply. A cancelled action may already have changed state; reread it. For files, inspect replacement permission, destination collisions, regular-file selection and Windows filename constraints. Reconnect the original headset before restarting a suspended batch.

For AI, explicitly apply the provider profile after changing model or URL. Check the required CLI/account or local model, shared context and action policy. For external tools, keep QuestDev open and regenerate MCP configuration after restarting it. Export job files before task cleanup.

When reporting a reproducible issue, include the QuestDev version, headset/firmware, steps and relevant errors in the [issue tracker](https://github.com/MrMybal/CyVR_QuestDev/issues). Review logs and remove account information or credentials before sharing them.
