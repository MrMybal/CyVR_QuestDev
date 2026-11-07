---
id: metrics
title: Courbes et sessions OVR
description: Retrouvez les rapports CSV OVR, enregistrez-les sur PC et examinez leurs compteurs numériques.
section: Outils
order: 50
sourceRevision: 1
---

## Enregistrer depuis le Quest {#record}

OVR Metrics Tool doit être installé sur le casque. Laissez tourner l'application VR à mesurer ; QuestDev ne la lance pas à votre place.

1. Connectez le Quest par ADB et ouvrez **Métriques**.
2. Choisissez **Enregistrer sur PC**. QuestDev lit le mode CSV initial et l'active si nécessaire.
3. Les nouveaux rapports ou rapports modifiés sont copiés progressivement dans une session persistante sur PC. Le premier rapport reçu s'ouvre dans les courbes ; les autres restent dans la liste et le dossier local.
4. Choisissez **Arrêter**. QuestDev effectue une dernière copie et désactive le CSV seulement s'il avait lui-même activé ce mode. Un mode CSV déjà actif est conservé.

**Dossier des sessions** ouvre `%LOCALAPPDATA%/CyVR/QuestDev/MetricsCaptures`. Chaque session contient les CSV et `session.json`, qui associe les fichiers locaux aux rapports d'origine. Les sessions de l'interface persistent après fermeture. Les enregistrements créés par les jobs Core ont une autre [durée de vie temporaire](doc:automation#jobs).

## Retrouver et suivre les rapports {#reports}

Sur un casque connecté, ouvrir Métriques actualise les rapports et ouvre le plus récent si aucune session n'est déjà affichée. Vous pouvez aussi actualiser manuellement, choisir un rapport et activer **Suivre les écritures CSV**. L'interrogation se fait chaque seconde, mais l'actualisation réelle dépend de l'écriture et du flush d'OVR. Une dernière ligne incomplète attend sa fin.

La recherche couvre le dossier courant `Android/data/com.oculus.ovrmonitormetricsservice/files/CapturedMetrics` du stockage partagé et l'ancien `OVRMonitorMetricsService/CapturedMetrics`, avec quatre niveaux de sous-dossiers. Les refus d'accès et listes tronquées sont signalés.

Certains firmwares n'exposent qu'un rapport de session terminé. Activer le CSV ne garantit donc pas des courbes en direct, notamment si une application tourne déjà. Si rien n'arrive, utilisez **Configurer OVR** pour vérifier le mode CSV et les statistiques du plugin. QuestDev ne modifie pas le HUD ou le profilage GPU uniquement pour enregistrer les CSV. Les compteurs dépendent du casque, du firmware, de l'app et du profilage.

```image
{"id":"ovr-curves","src":"assets/metrics-curves.png","alt":"Trois courbes numériques OVR sur une plage de temps commune avec des mesures simulées","controls":"minimal"}
```

## Lire et parcourir les courbes {#curves}

Recherchez et sélectionnez les colonnes dans la barre latérale, ou utilisez Toutes/Aucune. Toutes les colonnes numériques du CSV peuvent être découvertes, au-delà de la liste configurable du HUD. Les champs non numériques restent indisponibles ; les valeurs numériques négatives sont conservées.

| Interaction | Résultat |
| --- | --- |
| Molette sur une courbe | Zoom autour du pointeur |
| Bouton central maintenu | Déplacement de la plage commune |
| Clic gauche | Fixer un curseur commun et lire les valeurs de chaque courbe |
| Double clic / vue entière | Revenir à toute la session |
| Survol | Échantillon le plus proche et minimum, moyenne, maximum de la plage visible |

Le temps est exprimé en secondes depuis le premier timestamp CSV, interprété en millisecondes. Chaque courbe a sa propre échelle verticale. Les unités proviennent explicitement des noms de colonnes. Les valeurs absentes interrompent la ligne ; la réduction min/max préserve les pics lors du dézoom.

## Import, export et limites {#csv}

Ouvrez un CSV local sans casque, ou exportez le CSV courant avec ses données. Un import invalide affiche l'erreur et garde la session précédente. Les rapports d'applications différentes restent distincts ; ils ne sont pas assemblés sur une base de temps inventée.

Entrée prise en charge : UTF-8, virgules, champs entre guillemets, jusqu'à 256 métriques, 32 Mio par rapport, 200 000 lignes et quatre millions de valeurs par session chargée. Un enregistrement conserve jusqu'à 64 rapports. `Time Stamp`, `timestamp` ou `timestamp(ms)` est obligatoire ; les timestamps qui reculent sont refusés.

Un changement de casque ou la fermeture normale arrête l'enregistrement sur son appareil d'origine. Une déconnexion physique ou un arrêt forcé peut empêcher la restauration CSV. Reconnectez le casque et relisez l'[état du plugin OVR](doc:plugins#ovr) avant de poursuivre.
