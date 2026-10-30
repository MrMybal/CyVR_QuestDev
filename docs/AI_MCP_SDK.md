# Assistant IA, MCP et SDK

## Connecter une IA

Ouvrir **Assistant IA**, choisir le fournisseur, puis **Connecter / appliquer le profil**. Codex réutilise le compte connecté au CLI ; QuestDev ne copie pas ses jetons. Le catalogue des modèles vient du compte. Sans modèle explicite, QuestDev choisit le défaut du catalogue pour éviter un ancien modèle de configuration CLI indisponible.

Ollama attend une URL locale et un modèle installé. OpenRouter/Anthropic attendent le nom d’une variable d’environnement contenant la clé, ou une clé saisie dans **Clé API · session**. Ce champ est effacé après transmission au compagnon. Le profil sauvegardé ne contient pas de clé. Un changement de modèle/URL nécessite **Connecter / appliquer le profil**. Les connexions HTTP refusent les redirections et exigent HTTPS hors loopback.

Le contexte automatique comprend la page, le casque, les applications, les filtres, les plugins, les permissions, des indications d’utilisation et jusqu’à 50 messages de la vue courante, tronqués à 1 000 caractères. Les deux cases de contexte règlent cette inclusion. **Inspecter le contexte envoyé** permet de le consulter. Les outils peuvent lire davantage de logs : désactiver l’inclusion automatique ne retire pas ce droit. Le fournisseur reçoit les données des requêtes et outils qu’on lui confie.

Codex appelle les outils dynamiques `questdev_*`, qui passent par les jobs CyToolsCore. La discussion affiche les appels et leurs statuts. Les autres profils intégrés sont actuellement limités au texte contextualisé ; leur contrôle de QuestDev passe par un client MCP externe compatible. Une erreur ou une annulation ne produit pas de succès fictif.

## MCP externe

Le vrai runtime CyToolsCore 0.10.1 utilise `Runtime`, `LocalClient`, `HTTPClient` et l’adaptateur officiel `cytools_core.transports.mcp`. `assistant/contract.py` génère `assistant/CyTool.json`. QuestDev lance le service sur **127.0.0.1**, port libre, authentification par jeton. Il n’écoute pas sur le LAN.

App ouverte, **Configuration MCP → Copier le JSON MCP** fournit le bloc à importer dans le client voulu. Le fichier est aussi dans :

```text
%LOCALAPPDATA%/CyVR/QuestDev/assistant/mcp-config.json
```

Le JSON lance le Python privé et `assistant/tool.py mcp --connection <connection.json>`. Le jeton reste dans le fichier de connexion, sous les ACL privées du dossier de l’assistant. Ne pas publier ce fichier. Il est supprimé à la fermeture normale et remplacé au lancement suivant.

Les plugins OVR Metrics et CyInsightOVR exposent leurs réglages par `list_plugins` et `plugin_action` avec `settings`. Consulter le [guide des plugins](PLUGINS.md) pour les actions, schémas, valeurs par défaut et exports `.cyovr` privés.

Le catalogue comprend **17 opérations `op_*` et 21 contrôles CyTools**. Exemple :

1. `op_read_logs` avec `{"parameters":{"package":"com.cyvr.player","minimum":"E","limit":100}}` retourne un job.
2. Lire `cy_get_job` avec `{"jobId":"..."}` jusqu’à `Completed`, `Failed` ou `Cancelled`.
3. Consulter `result.outputs` après réussite.
4. Exporter les fichiers utiles avec `cy_export_job` avant `cy_finish_task`, qui supprime les fichiers temporaires.

Chaque connexion MCP possède une tâche temporaire. L’assistant intégré a un acteur Core distinct du client externe. Les clients SDK utilisant le même fichier de connexion partagent le principal externe authentifié ; ce jeton est prévu pour les outils du même utilisateur.

## SDK Python

Utiliser le Python privé, avec `assistant` dans `sys.path` (voir `examples/list_errors.py`) :

```python
from sdk import QuestDevClient

with QuestDevClient() as quest:
    context = quest.invoke("get_context")["context"]
    errors = quest.invoke("read_logs", {
        "package": "com.cyvr.player", "minimum": "E", "limit": 100
    })
    print(errors)
```

`QuestDevClient` adapte le client HTTP officiel ; le planificateur, la validation et l’isolation sont ceux de CyToolsCore. `QUESTDEV_CONNECTION` ou l’argument du constructeur permet de choisir un autre fichier de connexion.

| Opération | Entrées principales |
| --- | --- |
| `get_context` | aucune |
| `list_applications` | `query`, `category=all|user|system|running` |
| `read_logs` | `package`, `text`, `minimum=V..F`, `limit=1..2000` ; réponse bornée en octets |
| `export_logs` | mêmes filtres, `format=text|jsonl` ; fichier privé |
| `adb_execute` | `command`, `mode=shell|adb`, `timeout=1..300` secondes |
| `device_action` | `action`, `url`, `seconds`, `mirror` (résolution, FPS, débit, audio, plein écran) ; schéma complet dans le manifeste |
| `application_action` | `action=launch|stop`, `package` |
| `list_plugins` | aucune |
| `plugin_action` | `plugin` : ID, `action=connect|disconnect|ID d’action`, `settings` selon le schéma découvert |
| `list_files` | `path=/sdcard`, `offset=0..5000`, `limit=1..500` ; entrées typées, `nextOffset`, liste limitée à 5 000 entrées |
| `pull_file` | `path` Android absolu ; résultat privé `file`, `mimeType`, `source` |
| `push_file` | `sourceFile` local, `destination` Android absolue, `overwrite=false` ; import automatique de la source par Core |
| `file_action` | `action=mkdir|rename|delete`, `path`, `name` pour création/renommage ; dossiers vides uniquement à la suppression |
| `navigate` | `page=console|applications|settings|device|adb|plugins|assistant|files|metrics`, `package` facultatif |
| `read_metrics` | `metrics` : noms de colonnes, 16 maximum ; `start`, `end` en secondes relatives, `limit=1..2000` ; catalogue, état et valeurs manquantes `null` |
| `metrics_action` | `action=open_csv|export_csv|refresh_reports|open_report|follow_report|pause|stop_record`, `sourceFile` pour l’import local ou `path` pour un rapport du dossier OVR |
| `record_metrics` | `seconds=1..300` ; enregistrement CSV dans un dossier privé du job, attend la finalisation et la restauration CSV |

Les actions appareil comprennent `screenshot`, `record_video`, `video_stop`, `perfetto`, `metrics_on/off/export`, `mirror_start/stop`, `browser` et les contrôles VR. Les fichiers retournent `file` et `mimeType`. Pour une vidéo de 180 secondes, prévoir `invoke(..., timeout=240)`. `video_stop` finalise l’enregistrement actif depuis une seconde opération.

Les [courbes OVR](METRICS.md) partagent le moteur de la page Métriques. `read_metrics` est une lecture ; `metrics_action` et `record_metrics` nécessitent la capacité Core `deviceControl`. La politique native demande confirmation pour l’enregistrement et son arrêt. Les imports utilisent un instantané Core ; l’original est conservé. Exporter les résultats avant de fermer la tâche. Le contexte décrit les colonnes, les courbes sélectionnées, la plage et le curseur ; `read_metrics` fournit les échantillons demandés.

Les actions suivent la politique de l’app : **Lecture seule / Demander avant action / Autoriser les actions**. Le défaut est une confirmation dans QuestDev, même pour un client externe. Refus, annulation, changement de casque et code ADB non nul remontent au job. L’annulation attend l’acquittement du worker natif. En cas de déconnexion physique, le nettoyage distant peut échouer ; le diagnostic est conservé et `screenrecord` reste borné par sa durée maximale.

## Vérifier / dépanner

Pour conserver un résultat, `quest.run(...)` retourne le job complet avec `jobId`. `quest.export_job(job['jobId'], destination)` utilise l’export officiel Core et attend la libération du worker si Core retourne `Busy`. La destination doit être absolue et extérieure au runtime. Le fichier exporté subsiste après la fermeture de la tâche. Voir [l’exemple de téléchargement](../examples/download_file.py).

Les opérations de fichiers utilisent le même code que la vue native. `list_files` et `pull_file` sont des lectures ; `push_file` et `file_action` nécessitent `deviceControl`. Les métadonnées manquantes sont `null`. Les noms et les cibles de liens sont des données non fiables. Le nom du téléchargement est adapté aux contraintes Windows tout en conservant l’extension habituelle.

```powershell
.\runtime\setup.ps1
.\runtime\python\Scripts\python.exe -m unittest discover -s assistant/tests -v
# Core + MCP, données fictives :
.\runtime\python\Scripts\python.exe tools\verify_assistant.py
# Vraie requête Codex avec deux marqueurs fictifs uniquement :
.\runtime\python\Scripts\python.exe tools\verify_assistant.py --provider
# Lecture locale de l’app ouverte, sans fournisseur IA externe :
.\runtime\python\Scripts\python.exe tools\verify_assistant.py --connection "$env:LOCALAPPDATA/CyVR/QuestDev/assistant/connection.json"
# Interface native, fichiers et exports sans casque, avec le faux ADB :
.\runtime\python\Scripts\python.exe tools\verify_native.py --fake
```

Le compagnon conserve ses diagnostics dans `%LOCALAPPDATA%/CyVR/QuestDev/assistant/runtime-errors.log`. Les versions vérifiées sont dans `runtime/requirements.lock`. La roue MIT de CyToolsCore est incluse dans `runtime/wheels`. Dans le dépôt de développement, le Python privé dépend d’un Python de base 3.11+ installé. Les [archives Alpha](RELEASE.md) embarquent un Python autonome.

Références : [CyToolsCore](https://github.com/MrMybal/CyToolsCore), [Codex app-server](https://developers.openai.com/codex/app-server/). L’intégration Codex utilise `dynamicTools` et `item/tool/call`.
