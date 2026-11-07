---
id: automation
title: MCP and Python SDK
description: Connect external clients, invoke public operations and preserve job outputs.
section: Integrations
parent: assistant
order: 71
revision: 1
---

## Connect a client {#connect}

Keep QuestDev open. In **MCP configuration**, copy the MCP JSON into a compatible client. The generated configuration uses the private Python runtime and `assistant/tool.py mcp --connection <connection.json>`. It is also saved under `%LOCALAPPDATA%/CyVR/QuestDev/assistant/mcp-config.json`.

The CyToolsCore 0.10.1 service listens on loopback at a free port with token authentication, not on the LAN. The connection file belongs to the user's private assistant directory, is replaced at startup and removed on normal shutdown. Do not distribute it. The integrated assistant and external clients use distinct Core actors; SDK clients using the same connection file share the authenticated external principal.

For Python, use the supplied runtime and make the `assistant` package directory available as in the source examples. `QuestDevClient` wraps the official Core HTTP client. `QUESTDEV_CONNECTION` or its constructor argument selects a different connection file.

```python
from sdk import QuestDevClient

with QuestDevClient() as quest:
    context = quest.invoke("get_context")["context"]
    errors = quest.invoke("read_logs", {
        "package": "com.example.demo", "minimum": "E", "limit": 100
    })
    print(errors)
```

The package in this example is fictional. Replace it with the exact package you intend to inspect.

## Job lifecycle and exports {#jobs}

MCP exposes 17 `op_*` operations and 21 CyTools controls. Submit an operation, read its job until `Completed`, `Failed` or `Cancelled`, and inspect `result.outputs` after success. For example, `op_read_logs` receives `{"parameters":{"package":"com.example.demo","minimum":"E","limit":100}}`; its response identifies the job for `cy_get_job`.

Generated files belong to the temporary Core task and can be removed by `FinishTask`. Export them with `cy_export_job` or `quest.export_job(job['jobId'], destination)` before closing the task. The destination must be an absolute durable location outside the runtime. `quest.run(...)` waits and returns the complete job, including `jobId`; `invoke` is convenient for operation outputs.

```python
from pathlib import Path
from sdk import QuestDevClient

with QuestDevClient() as quest:
    job = quest.run("pull_file", {"path": "/sdcard/Download/report.txt"}, timeout=360)
    saved = quest.export_job(job["jobId"], Path.cwd() / "quest-exports")
    print(saved["outputs"]["file"])
```

Source files supplied to import operations are snapshotted by Core, preserving the original. UI metrics sessions have their own persistent folder; a Core recording still needs export.

## Operation catalogue {#operations}

| Operation | Main inputs / limits |
| --- | --- |
| `get_context` | Session context, including selected device and tool summaries |
| `list_applications` | `query`, `category=all/user/system/running` |
| `read_logs` | `package`, `text`, `minimum=V..F`, `limit=1..2000`; byte-bounded response |
| `export_logs` | Log filters and `format=text/jsonl`; private output file |
| `adb_execute` | `command`, `mode=shell/adb`, `timeout=1..300` seconds |
| `device_action` | `action`, `url`, `seconds`, optional `mirror` settings |
| `application_action` | `action=launch/stop`, exact `package` |
| `list_plugins` | Actions, schemas, defaults, draft/observed state and availability |
| `plugin_action` | Plugin ID, action and supported `settings` |
| `list_files` | `path`, `offset=0..5000`, `limit=1..500`, `nextOffset` |
| `pull_file` | Absolute Android `path`; output file and MIME type |
| `push_file` | Local `sourceFile`, absolute Android `destination`, `overwrite=false` |
| `file_action` | `action=mkdir/rename/delete`, `path`, optional `name`; empty folders only for deletion |
| `navigate` | `page=console/applications/settings/device/adb/plugins/assistant/files/metrics`, optional `package` |
| `read_metrics` | Exact column names, up to 16 metrics and 2,000 samples, `start/end` in relative seconds |
| `metrics_action` | `open_csv/export_csv/refresh_reports/open_report/follow_report/pause/stop_record` |
| `record_metrics` | `seconds=1..300`; waits for final copy and CSV restoration |

Use the discovered manifest/schema for the complete action-specific contract. Device actions include screenshot, Android video, Perfetto, HUD/CSV, browser, runtime controls and `mirror_start`/`mirror_stop`. Mirroring accepts `maxSize`, `maxFps`, `bitRateMbps`, `audio=off/duplicate/output` and `fullscreen`; the MKV file choice remains in the native UI. Video calls lasting 180 seconds need a longer client timeout, such as 240 seconds.

`read_metrics` returns unavailable values as `null`. `refresh_reports` returns up to 100 newest report paths with count/truncation metadata; use a returned path to open or follow a report. Plugin settings omitted from automation use defaults, independently of the UI draft. OVR exposes `read_state/apply/hide/csv_start/csv_stop`; CyInsightOVR also exposes catalogue/status, detailed profiling and capture actions.

## Permissions and cancellation {#permissions}

File reads and log/metric reads are separate from mutations. `push_file`, `file_action`, generic `plugin_action`, `metrics_action` and `record_metrics` require the relevant Core `deviceControl` capability; native action policy still applies. Known plugin read actions do not show mutation confirmation, but the generic Core capability remains required.

Default policy asks in QuestDev even for an external client. Rejection, device change, ADB failure and cancellation propagate to the job. Cancellation waits for native acknowledgement; disconnected-device cleanup may fail and is reported. Do not treat cancellation as proof that an earlier headset change was undone.
