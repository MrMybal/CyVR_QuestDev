---
id: plugins
title: OVR Metrics and CyInsightOVR
description: Configure headset tools, apply profiles and connect companion applications.
section: Tools
order: 60
revision: 1
---

## OVR Metrics Tool {#ovr}

In **Plugins → OVR Metrics Tool**, configure the installed `com.oculus.ovrmonitormetricsservice` service: HUD visibility, head locking, graphs/values, tilt, rotation, scale, distance, independent selection of 30 configurable metrics and CSV. **CSV reports → Curves and recording on PC** opens [Metrics](doc:metrics#record), whose column discovery can cover more fields than the HUD selection.

**Read state / Lire l'état** requests the service's current `LOG_STATE`. **Copy state to profile** copies observed settings into the draft, including global visibility. **Apply to Quest** sends the profile and checks the actual service configuration. A broadcast being accepted, or a service clamping a value, is not treated as a matching applied state. Selections outside the managed 30 metrics are preserved. Global graph/value visibility also controls what appears.

```image
{"id":"ovr-plugin","src":"assets/ovr-metrics.png","alt":"OVR Metrics plugin settings and selected HUD metrics in demonstration mode","controls":"minimal"}
```

## CyInsightOVR {#cyinsight}

1. Install the official `org.cyinsights.ovr` APK on the Quest if needed.
2. In **Connection / captures**, initialize the collector by choosing `cyinsightovr-bridge.jar` from that tool's distribution. QuestDev checks the installed UID and starts the authenticated shell collector; it does not open a LAN port.
3. Choose size, one of nine positions, up to six graphs and eight values. **Read headset catalogue** checks available metrics. **Apply to Quest** uses the Meta HUD managed by CyInsightOVR; **Stop / restore** restores the collector's backup.
4. For a GPU capture, select the exact game package and a duration from 0.05 to 2 seconds. Enable detailed profiling before manually opening the game. QuestDev does not launch or restart it.
5. A capture request returns `capture_id`. Read state and check that the received capture has that same ID. Export the received complete capture as `.cyovr` for CyInsights on PC. A diagnostic without tiles remains unavailable, with no invented measurements.

The collector supports `cyinsightovr.bridge/1` and `/2` and remains active until the headset restarts. Initialization preserves an already active collector. After updating its APK/JAR, use that distribution's `Start-CyInsightOVR.ps1 -RestartBridge`. QuestDev does not redistribute those APK/JAR files or expose the experimental Vulkan heatmap.

```image
{"id":"cyinsight-plugin","src":"assets/cyinsight-ovr.png","alt":"CyInsightOVR plugin overlay and capture settings in demonstration mode","controls":"minimal"}
```

## Profiles and action state {#profiles}

Both plugins control the same Meta HUD. Reread state when switching tools; stopping CyInsightOVR restores its own backup. Profiles are explicitly saved under `%LOCALAPPDATA%/CyVR/QuestDev/plugin-settings/<id>.json`. Editing or saving a profile alone does not modify the headset.

Actions keep their starting device and discard stale results after a device change. Plugin commands run serially. Cancelling waits for the worker but does not restore OVR changes already sent; reread state after interruption. Service verification cannot prove physical HUD visibility inside the headset.

## Companion links and manifests {#manifests}

Declarative `.cyplugin` manifests load from the supplied `plugins/` folder and the user's `%LOCALAPPDATA%/CyVR/QuestDev/plugins`. A user manifest can replace a supplied ID; reload plugins to reread them and see validation errors. Only load trusted manifests: their actions are explicit Android shell commands.

The link creator can configure ADB **forward** (PC access to a headset service) or **reverse** (headset access to a PC service). Connect uses `--no-rebind` to preserve an existing mapping; disconnect removes the matching mapping. An active tunnel verifies the ADB mapping, not an application-protocol response. The companion app must implement that service.

API 1 validates package/ID, ports 1–65535, files up to 128 KiB, up to 32 actions and timeouts 100–300,000 ms. `{package}`, `{local_port}` and `{remote_port}` are supported substitutions. This is not a native DLL ABI. Read the discovered actions and schemas through [MCP/SDK](doc:automation#operations) when automating a plugin.
