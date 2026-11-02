# Courbes et sessions OVR Metrics

La page **Métriques** trace les colonnes présentes dans les CSV produits par OVR Metrics Tool. Elle est accessible directement dans la barre principale ou depuis **Plugins → OVR Metrics Tool → Rapports CSV**. Le service OVR Metrics doit être installé sur le casque pour l’enregistrement ; son APK reste distribué par Meta.

## Enregistrer et suivre une session

1. Connecter le Quest en ADB et laisser son application VR tourner.
2. Dans **Métriques**, cliquer **Enregistrer sur PC**. QuestDev lit le mode CSV initial et l’active si nécessaire.
3. Les nouveaux rapports et les rapports modifiés sont copiés progressivement dans un dossier de session sur PC. Le premier rapport reçu apparaît dans les courbes ; les autres restent disponibles dans la liste du casque et dans le dossier local.
4. Cliquer **Arrêter**. QuestDev effectue une dernière copie et désactive le CSV s’il l’avait activé. Si ce mode était déjà actif, il le conserve.

**Dossier des sessions** ouvre `%LOCALAPPDATA%/CyVR/QuestDev/MetricsCaptures`. Chaque sous-dossier contient les CSV et `session.json`, qui associe les fichiers locaux aux rapports d’origine. Ces sessions de l’interface persistent après fermeture.

On peut aussi **Actualiser les rapports OVR**, choisir un CSV du casque, puis activer **Suivre les écritures CSV**. Le suivi interroge le dossier chaque seconde, mais la cadence réelle dépend des écritures et du flush d’OVR. Une ligne encore incomplète attend sa fin avant d’être tracée. Un firmware qui ne rend ses CSV disponibles qu’à la fin d’une session ne fournit pas de courbes en direct par cette voie.

La page actualise les rapports à son ouverture sur un casque connecté et ouvre le plus récent si aucune session n’est déjà affichée. La recherche couvre `/sdcard/Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics` et l’ancien `/sdcard/OVRMonitorMetricsService/CapturedMetrics`, avec quatre niveaux de fichiers et sous-dossiers. Les refus d’accès et les listes incomplètes sont signalés comme erreurs.

Si aucun rapport n’arrive, un diagnostic vérifie le mode CSV et la sélection de statistiques dans OVR. **Configurer OVR** ouvre directement le plugin pour choisir les métriques, puis appliquer les réglages. Le CSV est un rapport de session VR ; son activation ne garantit pas qu’un fichier soit immédiatement publié pendant une application déjà ouverte. Le suivi conserve le rapport le plus récent lorsque plusieurs fichiers changent.

QuestDev ne lance pas d’application VR et ne change pas le HUD ou le profilage GPU pour enregistrer. Sans rapport produit par OVR, la session indique qu’aucun CSV n’a été reçu. Certains compteurs peuvent manquer selon le casque, le firmware, l’application et le profilage activé. Les réglages correspondants restent dans **Appareil** et le plugin **OVR Metrics Tool**.

## Lire les courbes

La colonne de gauche découvre les métriques du fichier : recherche, cases individuelles, **Toutes** et **Aucune**. La liste peut dépasser celle des métriques configurables du HUD. Les champs non numériques restent indisponibles ; les valeurs négatives numériques sont conservées.

- **Molette** sur une courbe : zoom autour du pointeur.
- **Bouton central maintenu** : déplacer la plage commune.
- **Clic gauche** : fixer le curseur commun et la valeur de chaque courbe.
- **Double clic** ou bouton de vue entière : retrouver toute la session.
- **Survol** : échantillon le plus proche et minimum, moyenne, maximum de la plage visible.

Le temps est exprimé en secondes depuis le premier timestamp CSV, interprété en millisecondes. Chaque courbe possède sa propre échelle verticale ; les unités reconnues proviennent explicitement du nom de sa colonne. Les valeurs absentes interrompent la ligne. Les minimums et maximums par pixel préservent les pics lors du dézoom.

**Ouvrir un CSV…** recharge une session locale sans casque, et **Exporter le CSV…** conserve ses données. Un import invalide affiche l’erreur et garde la session précédente. Les rapports de plusieurs applications restent des fichiers distincts ; ils ne sont pas concaténés sur une fausse base de temps commune.

Limites : UTF-8, séparateur virgule, guillemets CSV, 256 métriques, 32 Mo par rapport, 200 000 lignes et 4 millions de valeurs par session chargée ; 64 rapports par enregistrement. La colonne `Time Stamp`, `timestamp` ou `timestamp(ms)` est obligatoire. Les timestamps qui reculent sont refusés.

## IA, MCP et SDK

Le contexte indique la source, les colonnes, les unités, le nombre de lignes, l’enregistrement, la plage et le curseur. `read_metrics` lit jusqu’à 16 métriques et 2 000 échantillons par appel, dans une réponse bornée en octets ; une valeur indisponible devient `null`. Les noms doivent correspondre au catalogue retourné.

`metrics_action` avec `action=refresh_reports` retourne les chemins et métadonnées des 100 rapports les plus récents, avec `reportsCount` et `reportsTruncated`. Utiliser ensuite un de ces chemins pour `open_report` ou `follow_report`.

```python
from pathlib import Path
from sdk import QuestDevClient

with QuestDevClient() as quest:
    # Suit la politique de confirmation de QuestDev.
    job = quest.run("record_metrics", {"seconds": 10}, timeout=90)
    print(job["outputs"]["filesSaved"], job["outputs"]["hasSamples"])
    quest.export_job(job["jobId"], Path.cwd() / "session-ovr")
    catalogue = quest.invoke("read_metrics", {"limit": 1})
    print(catalogue["columns"])
```

Les enregistrements et exports déclenchés par Core appartiennent au job et sont supprimés par `FinishTask` : utiliser `ExportJob` avant fermeture pour les conserver. `metrics_action.open_csv` importe automatiquement `sourceFile` en conservant l’original. `record_metrics` accepte 1 à 300 secondes. Son annulation attend la fin des accès natifs et la tentative de restauration CSV avant acquittement.

Un changement d’appareil ou la fermeture normale arrête l’enregistrement sur son appareil d’origine. Une déconnexion physique ou un arrêt forcé peut empêcher la restauration ; l’erreur est visible. Reconnecter le Quest et relire son état dans le plugin OVR avant de reprendre.

Cette collecte utilise les fichiers CSV documentés par Meta. `LOG_STATE` sert uniquement à vérifier la configuration. Sources : [OVR Metrics Tool](https://developers.meta.com/vr/documentation/native/android/ts-ovrmetricstool/), [mesures et bonnes pratiques](https://developers.meta.com/vr/documentation/native/android/ts-ovr-best-practices/).
