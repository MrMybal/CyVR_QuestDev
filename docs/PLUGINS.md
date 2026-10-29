# Plugins QuestDev — API 1

## Plugins inclus dans QuestDev 0.4

**Plugins → OVR Metrics Tool** configure le service `com.oculus.ovrmonitormetricsservice` : HUD, suivi de la tête, visibilité des graphiques/valeurs, inclinaison, rotation, échelle, distance, sélection indépendante de 30 métriques et CSV. Les rapports sont accessibles dans l’onglet **Rapports CSV**.

**Lire l’état** lit `LOG_STATE`, assemble le JSON fragmenté et rejette les réponses antérieures à la requête. **État du service → Copier l’état dans le profil** reprend les réglages observés, y compris la visibilité globale. **Appliquer au Quest** envoie le profil puis vérifie sa configuration réelle ; un broadcast accepté ou une valeur limitée par le service ne suffisent pas. Les sélections hors des 30 métriques gérées sont conservées. Leur affichage suit aussi les deux réglages de visibilité globale.

**Plugins → CyInsightOVR** pilote `org.cyinsights.ovr` par son vrai collecteur authentifié `cyinsightovr.bridge/1` et `/2`, via `app_process … org.cyinsights.ovr.ShellClient --request <JSON base64>`.

1. Installer l’APK officiel si nécessaire avec **Installer un APK…**.
2. Dans **Connexion / captures**, cliquer **Initialiser le collecteur…** et choisir `cyinsightovr-bridge.jar` dans la distribution de CyInsightOVR. Le collecteur démarre sous l’utilisateur shell après vérification de l’UID installé. Sa capacité est transférée directement au stockage privé de l’app ; elle ne passe pas dans la sortie QuestDev ou les arguments des processus. Aucun port LAN n’est ouvert.
3. Choisir la taille, l’une des neuf positions, jusqu’à six graphiques et huit valeurs. **Lire le catalogue du casque** vérifie leur disponibilité. **Appliquer au Quest** utilise le HUD Meta contrôlé par CyInsightOVR. **Arrêter / restaurer** restaure la sauvegarde du collecteur.
4. Choisir le package exact du jeu pour une capture GPU de 0,05 à 2 secondes. Le profilage détaillé doit précéder l’ouverture manuelle du jeu. QuestDev ne lance ni ne redémarre le jeu. L’acceptation retourne `capture_id` ; **Lire l’état** doit retrouver cet ID pour confirmer la réception. Les résultats anciens restent identifiés.
5. **Exporter la capture reçue…** écrit un `.cyovr` complet pour CyInsights sur PC. Un diagnostic sans tuiles reste indisponible, sans mesures inventées. Les captures enregistrées par l’app Quest sont également accessibles depuis le navigateur de fichiers.

Le collecteur continue jusqu’au redémarrage du casque. Une initialisation conserve un collecteur déjà actif ; après mise à jour de son APK/JAR, utiliser `Start-CyInsightOVR.ps1 -RestartBridge` de sa distribution. QuestDev ne redistribue ni l’APK ni le JAR. Cette intégration utilise le HUD Meta ; elle n’expose pas la heatmap Vulkan expérimentale.

Les deux plugins pilotent le même HUD Meta. Après un changement d’outil, relire l’état ; l’arrêt CyInsightOVR restaure sa propre sauvegarde.

Les profils sont enregistrés explicitement dans `%LOCALAPPDATA%/CyVR/QuestDev/plugin-settings/<id>.json`. Modifier/enregistrer un profil ne modifie pas le casque. Les jobs utilisent l’appareil sélectionné au départ et invalident leur résultat après un changement d’appareil. Les commandes des plugins sont sérialisées entre elles. Annuler attend la fin du worker mais ne restaure pas les réglages OVR déjà envoyés : relire l’état après interruption. La vérification du service ne confirme pas la visibilité physique dans le casque.

### IA / MCP / SDK

`list_plugins` expose les actions, `settingsSchema`, `defaults`, le brouillon `draft`, la réponse `observed`, les métriques disponibles, `installed`, `busy` et les erreurs. Brouillon et état observé sont distincts.

```python
from sdk import QuestDevClient

with QuestDevClient() as quest:
    state = quest.invoke('plugin_action', {
        'plugin': 'meta.ovr-metrics', 'action': 'read_state'
    })
    # Suit la politique d’autorisation des actions de QuestDev :
    result = quest.invoke('plugin_action', {
        'plugin': 'cyinsights.ovr', 'action': 'apply',
        'settings': {'size': 'small', 'corner': 'upper_right',
                     'graphs': ['average_frame_rate'], 'stats': []}
    })
```

OVR : `read_state`, `apply`, `hide`, `csv_start`, `csv_stop`. CyInsightOVR : `read_state`, `catalog`, `status`, `apply`, `hide`, `enable_details`, `capture`. Les réglages omis utilisent les **defaults**, indépendamment du brouillon UI. Les types, bornes, choix et métriques sont validés côté natif. Seules `apply`, `capture` et `enable_details` acceptent `settings`.

Les lectures des contrôleurs connus ne demandent pas de confirmation UI ; les mutations suivent Lecture seule / Demander / Autoriser. Core exige toujours le droit `deviceControl` sur l’opération générique `plugin_action`.

Quand une capture complète existe, `read_state` CyInsightOVR fournit `file` et `mimeType`. Ce fichier appartient au job Core : l’exporter avec `quest.export_job(job['jobId'], destination)` avant de fermer la tâche. Comparer `outputs.capture.capture_id` à l’ID demandé. Le contexte IA conserve un résumé sans géométrie.

Sources : [commandes publiques OVR Metrics](https://developers.meta.com/vr/documentation/native/android/ts-ovrmetricstool/) et le contrat de la distribution locale `CyInsights_plugin/Quest/CyInsightOVR`.

## Créer un manifeste

Manifestes déclaratifs `.cyplugin`, chargés près de l’EXE (`plugins/`) puis dans `%LOCALAPPDATA%/CyVR/QuestDev/plugins`. Un manifeste utilisateur peut remplacer un ID fourni. **Recharger les plugins** relit les fichiers ; l’app affiche leurs erreurs.

```ini
[plugin]
api=1
id=my.companion
name=Mon compagnon Quest
version=1.0
description=Service de mon application sur le Quest.
package=com.example.companion

[link]
direction=forward
local_port=8787
remote_port=9797

[action.inspect]
label=Inspecter le package
shell=dumpsys package {package}
timeout_ms=30000
```

Ports : 1–65535. Le package, l’ID et l’API sont validés ; fichier limité à 128 Ko, 32 actions, délai de 100–300 000 ms. Les substitutions `{package}`, `{local_port}`, `{remote_port}` sont disponibles. Le package est échappé pour le shell Android.

**Relier l’application** exécute `adb forward --no-rebind tcp:LOCAL tcp:REMOTE` ou `reverse`. Forward expose sur le PC un service du casque ; reverse expose sur le casque un service du PC. Le mode sans remplacement évite d’écraser un tunnel existant. **Déconnecter** retire le mapping correspondant. « Tunnel ADB actif » confirme le mapping, pas une réponse du protocole applicatif : le service doit être implémenté dans l’application cible.

Le créateur de liaison écrit un manifeste utilisateur. `plugins/companion-template.cyplugin.example` est un modèle inactif à adapter. `plugins/cyvr-player.cyplugin` fournit des commandes Android pour `com.cyvr.player`, sans service TCP fictif.

Les actions sont des commandes shell Android explicites sur le casque sélectionné. Charger des manifestes de confiance. Ce format n’est pas une ABI de DLL native. L’IA/MCP réutilise ces manifestes via `list_plugins` et `plugin_action`, sous la politique d’action de QuestDev.
