---
id: files
title: Files and transfer batches
description: Browse PC/Quest panes, prepare transfers and handle cancellation or retry.
section: Tools
order: 40
revision: 1
---

## Browse and select {#browse}

Open **Files / Fichiers**. Choose a Quest location from its sidebar or enter a path. Double-click a folder to open it. Enable **Side by side / Côte à côte** for independent PC and Quest panes, each with its own path, filter and selection. Use Ctrl/Shift for multiple selection and Ctrl+A for the current pane. Hidden-file and prepared-list preferences are saved.

Only regular files transfer. Selecting a folder browses it; folder copying and recursive deletion are not implemented. Android's normal access restrictions apply, without root access or automatic link traversal. Each listing is limited to 5,000 entries.

```image
{"id":"file-batch","src":"assets/files-batch.png","alt":"PC and Quest file panes with three prepared transfers using simulated paths","controls":"minimal"}
```

## Prepare and launch a batch {#batch}

1. Enable **Prepare list / Préparer une liste**. Select files and add them from a pane or the multiple-file Windows selector.
2. Review the direction, source, destination, size, device and replacement permission of each entry. Those values are fixed when added; later folder navigation does not change them.
3. Add files from other folders if required, then choose **Start batch / Lancer le lot**. Transfers run sequentially in the background.
4. Choose whether **Stop on error / Arrêt sur erreur** suspends the batch after a failure.
5. To retry, choose **Return to waiting / Remettre en attente…** for failed entries, or failures and cancellations, then launch the batch explicitly.

The list holds up to 2,000 files and lasts for the current app session. It is not automatically restarted when QuestDev opens. A duplicate destination must be removed before being reused. With preparation disabled, newly added transfers start immediately and remain cancellable in the list.

## Cancel and recover {#cancel}

An entry's cross cancels that file. **Cancel batch** cancels waiting entries and requests cancellation of the active transfer. Completed files remain in place. A device change or disconnection suspends the batch; waiting entries stay bound to their original device. Reconnect it and relaunch explicitly.

Sources are revalidated when a file starts. Uploads use a remote temporary file; downloads finalize a local temporary file after success. Cancelled downloads preserve the old local destination. A disconnection can prevent remote temporary-file cleanup; inspect the reported error in [ADB](doc:device#adb).

**Replace** is off initially. Enabling it permits replacement for newly added entries only; an entry's tooltip shows its recorded permission. Finish or cancel transfer lists before an app update/restart.

## Folder actions and limits {#actions}

Create, rename and delete actions show their target before confirmation. Renaming does not replace another entry. A folder must be empty before deletion; deleting a link removes only that link. Names are passed as literal arguments, including spaces and apostrophes. Rename an Android filename incompatible with Windows before downloading it.

Android may deny access or omit a folder; QuestDev does not bypass those permissions. File operations are also available through [MCP and SDK](doc:automation#operations), where output files must be exported before closing their temporary task.
