---
id: console
title: Application logs
description: Separate packages, follow live messages, copy RAW text and sort columns.
section: Tools
order: 10
revision: 1
---

## Select applications {#packages}

The left side of **Console** lists running, installed or system applications. Search for a package and open it to give it a log tab. Attribution includes its processes and subprocesses, with process lifetime tracking. **All applications** combines the capture; **System / unassigned** contains origins that cannot reliably be assigned. Shared UIDs alone are not used to guess an application's identity.

Enable **Side by side / Côte à côte** to compare independent application views. Each view keeps its own filters, pause and selection. Capture starts at the current time; interruptions can leave missing messages.

```image
{"id":"compare-logs","src":"assets/comparison.png","alt":"Two application log views with separate simulated messages","controls":"minimal"}
```

## Follow and filter {#follow}

Search the message text and use severity, tag, PID, exclusion and advanced filters as needed. Scrolling up suspends following. Returning manually to the bottom resumes it; **Follow live / Suivre le direct** also rejoins the latest messages. Pausing freezes the displayed view while capture continues within the [retention budget](doc:settings#retention).

Clearing the local journal is distinct from pausing. Older messages discarded by the retention limits are no longer available for copy or export. The bottom counters show retained lines, memory and discarded/lost messages.

## Copy original RAW text {#raw}

Choose **RAW** to read the original Logcat lines, including timestamp, spaces, PID, TID, level, tag and message. Select text with the mouse and press **Ctrl+C**. **Ctrl+A** selects the displayed RAW page. A text selection freezes its contents so new messages cannot move the selected characters; **Release selection / Libérer la sélection** resumes updates.

RAW pages are limited to **10,000 lines or 2 MiB**. Use Previous/Next to browse retained pages. **Copy all filtered lines** and text export cover the full filtered view, even when it spans more than one RAW page. RAW keeps reception order.

```image
{"id":"raw-log","src":"assets/console-raw.png","alt":"Original Logcat lines selected in the RAW view using demonstration data","controls":"minimal"}
```

## Sort and export {#sort}

In **Columns**, click a header for ascending or descending order. Hold **Shift** while clicking to add another sort key. Time, level, PID/TID, application, tag and message are sortable. Severity uses level order; PID/TID use numeric order; ties keep reception order.

Table copy and export respect its sort. Live following is available with ascending local time; other sorts remain stable for analysis. Export the filtered view as UTF-8 text or JSON Lines. JSON Lines preserves structured fields.

| Shortcut in the console | Action |
| --- | --- |
| Space | Pause/resume the view |
| Ctrl+L | Clear the local journal |
| Ctrl+S | Export the filtered view |
| Ctrl+C | Copy the selected row, or selected text in RAW |
| Ctrl+Shift+C | Copy all filtered lines |
| F1 | Open this guide |

Text editors keep their normal selection shortcuts; avoid assuming a global shortcut ran while an input field was active. Review logs before sharing them, particularly when using [AI context](doc:assistant#context).
