---
id: device
title: Device actions and ADB
description: Capture, inspect and operate the selected headset, or run explicit commands.
section: Tools
order: 30
revision: 1
---

## Device actions {#actions}

Open **Device / Appareil**. Its left navigation groups device actions, captures/broadcasting, performance/traces and runtime/connection controls. Refresh device state to read the available Android, battery, network and storage information. An unknown state is not an enabled or disabled confirmation.

```image
{"id":"device-actions","src":"assets/device-actions.png","alt":"Device action cards with icons and switches using simulated headset state","controls":"minimal"}
```

| Tool | Use and limits |
| --- | --- |
| Screenshot | Save the selected headset's screen as a PNG. |
| Android video | Record an MP4 for the selected duration, up to 180 seconds. This is separate from mirroring/MKV recording. |
| Browser | Send a URL to the headset's browser. |
| Perfetto | Save a trace for the requested duration, up to 120 seconds. |
| GPU profiling | Request supported profiling controls; inspect their reported state and task output. |
| Metrics HUD / CSV | Control the installed OVR Metrics service; configure counters in its plugin. |
| Proximity / Guardian | Request the supported runtime state. Firmware can reject or ignore a command. |
| Power | Wake, sleep or request a reboot. Confirm the intended device before rebooting. |

Use [mirroring](doc:mirroring#start) for a PC video window, [metrics](doc:metrics#record) for curves/CSV sessions, and [plugins](doc:plugins#ovr) for tool-specific settings. Availability depends on firmware and installed Quest applications.

## ADB over Wi-Fi {#wifi}

From an authorized USB connection, **Activate TCP 5555** requests the ADB TCP port. Enter the intended headset's address and port, then connect. **USB** switches ADB back to USB mode. Changing modes can disconnect the current transport; refresh and select the resulting device entry.

Use the address actually reported for your headset on your network. A mirroring session uses an already established ADB connection; starting mirroring does not configure Wi-Fi for you.

## Commands and task output {#adb}

Open **ADB** to enter a command. Choose the intended **shell** or **ADB** mode and its timeout. Shell mode runs an Android command; ADB mode uses the host ADB executable. Review the selected device and the command before execution, especially for writes and runtime changes.

Select a task in the history to inspect its output, exit code and completion state. Copy the relevant output when diagnosing an error. Cancel a running task when needed; cancellation waits for the native worker's acknowledgement and may not undo commands already accepted by Android.

An ADB process returning success does not guarantee a tool's higher-level state. Refresh the state or inspect the service response. See [troubleshooting](doc:troubleshooting#commands) for denied actions and missing results.
