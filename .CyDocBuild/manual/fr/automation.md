---
id: automation
title: MCP et SDK Python
description: Connectez les clients externes, invoquez les opérations et conservez les fichiers des jobs.
section: Intégrations
parent: assistant
order: 71
sourceRevision: 1
---

## Connecter un client {#connect}

Gardez QuestDev ouvert. Dans **Configuration MCP**, copiez le JSON MCP dans un client compatible. La configuration utilise le Python privé et `assistant/tool.py mcp --connection <connection.json>`. Elle est aussi enregistrée dans `%LOCALAPPDATA%/CyVR/QuestDev/assistant/mcp-config.json`.

Le service CyToolsCore 0.10.1 écoute en loopback sur un port libre avec authentification par jeton, sans écoute LAN. Le fichier de connexion appartient au dossier privé de l'assistant, est remplacé au lancement et retiré à la fermeture normale. Ne le distribuez pas. L'assistant intégré et les clients externes ont des acteurs Core distincts ; les clients SDK utilisant le même fichier partagent le principal externe authentifié.

En Python, utilisez le runtime fourni et rendez le dossier `assistant` accessible comme dans les exemples des sources. `QuestDevClient` adapte le client HTTP officiel de Core. `QUESTDEV_CONNECTION` ou l'argument du constructeur choisit un autre fichier de connexion.

```python
from sdk import QuestDevClient

with QuestDevClient() as quest:
    context = quest.invoke("get_context")["context"]
    errors = quest.invoke("read_logs", {
        "package": "com.example.demo", "minimum": "E", "limit": 100
    })
    print(errors)
```

Le package de cet exemple est fictif. Remplacez-le par le package exact à examiner.

## Durée de vie des jobs et exports {#jobs}

MCP expose 17 opérations `op_*` et 21 contrôles CyTools. Soumettez une opération, consultez son job jusqu'à `Completed`, `Failed` ou `Cancelled`, puis lisez `result.outputs` après réussite. Par exemple, `op_read_logs` reçoit `{"parameters":{"package":"com.example.demo","minimum":"E","limit":100}}` ; sa réponse identifie le job à consulter avec `cy_get_job`.

Les fichiers générés appartiennent à la tâche temporaire Core et peuvent être supprimés par `FinishTask`. Exportez-les avec `cy_export_job` ou `quest.export_job(job['jobId'], destination)` avant de fermer la tâche. La destination doit être un emplacement absolu et durable, extérieur au runtime. `quest.run(...)` attend puis retourne le job complet, `jobId` compris ; `invoke` est pratique pour obtenir les sorties d'opération.

```python
from pathlib import Path
from sdk import QuestDevClient

with QuestDevClient() as quest:
    job = quest.run("pull_file", {"path": "/sdcard/Download/report.txt"}, timeout=360)
    saved = quest.export_job(job["jobId"], Path.cwd() / "quest-exports")
    print(saved["outputs"]["file"])
```

Core importe un instantané des fichiers fournis aux opérations, en conservant l'original. Les sessions de métriques de l'interface ont leur propre dossier persistant ; un enregistrement Core nécessite toujours un export.

## Catalogue des opérations {#operations}

| Opération | Entrées principales / limites |
| --- | --- |
| `get_context` | Contexte de session, avec appareil choisi et résumés des outils |
| `list_applications` | `query`, `category=all/user/system/running` |
| `read_logs` | `package`, `text`, `minimum=V..F`, `limit=1..2000` ; réponse bornée en octets |
| `export_logs` | Filtres de logs et `format=text/jsonl` ; fichier de sortie privé |
| `adb_execute` | `command`, `mode=shell/adb`, `timeout=1..300` secondes |
| `device_action` | `action`, `url`, `seconds`, réglages `mirror` facultatifs |
| `application_action` | `action=launch/stop`, `package` exact |
| `list_plugins` | Actions, schémas, valeurs par défaut, brouillon/état observé et disponibilité |
| `plugin_action` | ID du plugin, action et `settings` pris en charge |
| `list_files` | `path`, `offset=0..5000`, `limit=1..500`, `nextOffset` |
| `pull_file` | `path` Android absolu ; fichier de sortie et type MIME |
| `push_file` | `sourceFile` local, `destination` Android absolue, `overwrite=false` |
| `file_action` | `action=mkdir/rename/delete`, `path`, `name` facultatif ; suppression des dossiers vides uniquement |
| `navigate` | `page=console/applications/settings/device/adb/plugins/assistant/files/metrics`, `package` facultatif |
| `read_metrics` | Noms exacts de colonnes, jusqu'à 16 métriques et 2 000 échantillons, `start/end` en secondes relatives |
| `metrics_action` | `open_csv/export_csv/refresh_reports/open_report/follow_report/pause/stop_record` |
| `record_metrics` | `seconds=1..300` ; attend la dernière copie et la restauration CSV |

Utilisez le manifeste/schéma découvert pour le contrat complet de chaque action. Les actions appareil comprennent capture d'écran, vidéo Android, Perfetto, HUD/CSV, navigateur, contrôles runtime et `mirror_start`/`mirror_stop`. Le mirroring accepte `maxSize`, `maxFps`, `bitRateMbps`, `audio=off/duplicate/output` et `fullscreen` ; le fichier MKV reste choisi dans l'interface native. Une vidéo de 180 secondes nécessite un délai client supérieur, par exemple 240 secondes.

`read_metrics` retourne `null` pour les valeurs indisponibles. `refresh_reports` retourne jusqu'aux 100 chemins de rapports les plus récents avec compteurs et indication de troncature ; utilisez un chemin retourné pour ouvrir ou suivre un rapport. Les réglages de plugin omis utilisent les valeurs par défaut, indépendamment du brouillon UI. OVR expose `read_state/apply/hide/csv_start/csv_stop` ; CyInsightOVR expose aussi catalogue/état, profilage détaillé et captures.

## Permissions et annulation {#permissions}

Les lectures de fichiers, de logs et de métriques sont distinctes des mutations. `push_file`, `file_action`, l'opération générique `plugin_action`, `metrics_action` et `record_metrics` nécessitent la capacité Core `deviceControl` correspondante ; la politique native s'applique toujours. Les lectures des plugins connus ne demandent pas de confirmation de mutation, mais gardent l'exigence de capacité générique Core.

Le défaut demande confirmation dans QuestDev même pour un client externe. Refus, changement de casque, échec ADB et annulation remontent au job. Annuler attend l'acquittement natif ; le nettoyage d'un casque déconnecté peut échouer et reste signalé. Une annulation ne prouve pas qu'un changement antérieur sur le casque a été défait.
