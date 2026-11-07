---
id: plugins
title: OVR Metrics et CyInsightOVR
description: Configurez les outils du casque, appliquez les profils et reliez les applications compagnons.
section: Outils
order: 60
sourceRevision: 1
---

## OVR Metrics Tool {#ovr}

Dans **Plugins → OVR Metrics Tool**, configurez le service installé `com.oculus.ovrmonitormetricsservice` : visibilité HUD, suivi de la tête, graphiques/valeurs, inclinaison, rotation, échelle, distance, sélection indépendante de 30 métriques configurables et CSV. **Rapports CSV → Courbes et enregistrement sur PC** ouvre les [métriques](doc:metrics#record), dont la découverte des colonnes peut dépasser la sélection du HUD.

**Lire l'état** demande le `LOG_STATE` courant du service. **Copier l'état dans le profil** reprend les réglages observés dans le brouillon, visibilité globale comprise. **Appliquer au Quest** envoie le profil puis vérifie la configuration réelle du service. Un broadcast accepté, ou une valeur bornée par le service, n'est pas assimilé à un état appliqué identique. Les sélections hors des 30 métriques gérées sont conservées. Les visibilités globales graphiques/valeurs déterminent aussi l'affichage.

```image
{"id":"ovr-plugin","src":"assets/ovr-metrics.png","alt":"Réglages du plugin OVR Metrics et métriques HUD sélectionnées en mode démonstration","controls":"minimal"}
```

## CyInsightOVR {#cyinsight}

1. Installez au besoin l'APK officiel `org.cyinsights.ovr` sur le Quest.
2. Dans **Connexion / captures**, initialisez le collecteur en choisissant `cyinsightovr-bridge.jar` dans la distribution de cet outil. QuestDev vérifie l'UID installé puis lance le collecteur shell authentifié ; aucun port LAN n'est ouvert.
3. Choisissez la taille, l'une des neuf positions, jusqu'à six graphiques et huit valeurs. **Lire le catalogue du casque** vérifie les métriques disponibles. **Appliquer au Quest** utilise le HUD Meta géré par CyInsightOVR ; **Arrêter / restaurer** reprend la sauvegarde du collecteur.
4. Pour une capture GPU, sélectionnez le package exact du jeu et une durée de 0,05 à 2 secondes. Activez le profilage détaillé avant d'ouvrir manuellement le jeu. QuestDev ne le lance pas et ne le redémarre pas.
5. Une demande de capture retourne `capture_id`. Relisez l'état et vérifiez que la capture reçue porte cet ID. Exportez la capture complète reçue en `.cyovr` pour CyInsights sur PC. Un diagnostic sans tuiles reste indisponible, sans mesures inventées.

Le collecteur prend en charge `cyinsightovr.bridge/1` et `/2` et reste actif jusqu'au redémarrage du casque. L'initialisation conserve un collecteur déjà actif. Après mise à jour de son APK/JAR, utilisez `Start-CyInsightOVR.ps1 -RestartBridge` dans sa distribution. QuestDev ne redistribue pas ces APK/JAR et n'expose pas la heatmap Vulkan expérimentale.

```image
{"id":"cyinsight-plugin","src":"assets/cyinsight-ovr.png","alt":"Réglages d'affichage et de capture du plugin CyInsightOVR en mode démonstration","controls":"minimal"}
```

## Profils et état des actions {#profiles}

Les deux plugins pilotent le même HUD Meta. Relisez l'état en changeant d'outil ; l'arrêt de CyInsightOVR restaure sa propre sauvegarde. Les profils sont sauvegardés explicitement dans `%LOCALAPPDATA%/CyVR/QuestDev/plugin-settings/<id>.json`. Modifier ou enregistrer un profil ne change pas à lui seul le casque.

Les actions gardent leur appareil de départ et rejettent les résultats périmés après un changement de casque. Les commandes des plugins sont sérialisées. Annuler attend le worker mais ne restaure pas les changements OVR déjà envoyés ; relisez l'état après interruption. La vérification du service ne prouve pas la visibilité physique du HUD dans le casque.

## Liaisons compagnons et manifestes {#manifests}

Les manifestes déclaratifs `.cyplugin` sont chargés dans le dossier fourni `plugins/` puis dans `%LOCALAPPDATA%/CyVR/QuestDev/plugins`. Un manifeste utilisateur peut remplacer un ID fourni ; rechargez les plugins pour relire les fichiers et voir les erreurs. Chargez uniquement des manifestes de confiance : leurs actions sont des commandes shell Android explicites.

Le créateur de liaison configure ADB **forward** (accès PC à un service du casque) ou **reverse** (accès casque à un service du PC). La connexion utilise `--no-rebind` pour préserver un mapping existant ; la déconnexion retire le mapping correspondant. Un tunnel actif confirme le mapping ADB, pas une réponse du protocole applicatif. L'application compagnon doit implémenter ce service.

L'API 1 valide package/ID, ports 1–65535, fichiers jusqu'à 128 Kio, jusqu'à 32 actions et délais de 100–300 000 ms. Les substitutions `{package}`, `{local_port}` et `{remote_port}` sont disponibles. Ce format n'est pas une ABI de DLL native. Consultez les actions et schémas découverts par [MCP/SDK](doc:automation#operations) pour automatiser un plugin.
