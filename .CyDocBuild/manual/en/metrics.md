---
id: metrics
title: OVR curves and sessions
description: Discover OVR CSV reports, record them on PC and inspect their numeric counters.
section: Tools
order: 50
revision: 1
---

## Record from the Quest {#record}

OVR Metrics Tool must be installed on the headset. Keep the VR application you want to measure running; QuestDev does not launch it for you.

1. Connect the Quest through ADB and open **Metrics / Métriques**.
2. Choose **Record on PC / Enregistrer sur PC**. QuestDev reads the initial CSV mode and enables it if necessary.
3. New or modified reports are copied progressively into a persistent PC session. The first received report opens in the charts; others remain in the report list and local session folder.
4. Choose **Stop / Arrêter**. QuestDev performs a final copy and disables CSV only if it enabled that mode itself. An initially active CSV mode is preserved.

**Session folder** opens `%LOCALAPPDATA%/CyVR/QuestDev/MetricsCaptures`. Each session contains CSV files and `session.json` mapping local files to original reports. UI sessions remain after QuestDev closes. Recordings made through Core jobs have a different [temporary-task lifetime](doc:automation#jobs).

## Discover and follow reports {#reports}

On a connected headset, opening Metrics refreshes reports and opens the newest one if no session is already displayed. You can also refresh manually, choose a report and enable **Follow CSV writes / Suivre les écritures CSV**. Polling occurs each second, while actual updates depend on OVR's writing and flushing cadence. An incomplete final row waits for completion.

Discovery covers the current `Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics` directory under shared storage and the older `OVRMonitorMetricsService/CapturedMetrics`, including four levels of subdirectories. Permission errors and truncated listings are reported.

Some firmware only exposes a completed session report. Enabling CSV therefore does not guarantee live curves, especially in an already running application. If nothing arrives, use **Configure OVR** to review the plugin's CSV mode and selected statistics. QuestDev does not alter the HUD or GPU profiling merely to record CSV. Counters depend on the headset, firmware, app and profiling configuration.

```image
{"id":"ovr-curves","src":"assets/metrics-curves.png","alt":"Three OVR numeric curves sharing the same time range, using simulated measurements","controls":"minimal"}
```

## Read and navigate curves {#curves}

Search and select columns in the left sidebar, or use All/None. All numeric CSV columns are discoverable, beyond the HUD's configurable metric list. Non-numeric fields remain unavailable; numeric negative values are retained.

| Interaction | Result |
| --- | --- |
| Mouse wheel over a curve | Zoom around the pointer |
| Hold middle mouse button | Pan the common time range |
| Left click | Pin a common cursor and inspect each curve's value |
| Double-click / full-view control | Return to the whole session |
| Hover | Nearest sample and visible-range minimum, mean and maximum |

Time is seconds since the first CSV timestamp, interpreted as milliseconds. Each curve has its own vertical scale. Units come explicitly from column names. Missing values interrupt a line; min/max reduction preserves peaks when zoomed out.

## Import, export and limits {#csv}

Open a local CSV without a headset, or export the current CSV with its data. An invalid import reports the error and preserves the previous session. Reports from different applications remain separate; they are not joined onto an invented common timeline.

Supported input: UTF-8, comma separator, quoted CSV fields, up to 256 metrics, 32 MiB per report, 200,000 rows and four million values per loaded session. Recording keeps up to 64 reports. `Time Stamp`, `timestamp` or `timestamp(ms)` is required; backwards timestamps are rejected.

A device change or normal app close stops recording on its original device. A physical disconnection or forced exit may prevent CSV restoration. Reconnect and reread the [OVR plugin state](doc:plugins#ovr) before continuing.
