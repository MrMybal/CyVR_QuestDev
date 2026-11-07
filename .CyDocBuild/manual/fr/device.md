---
id: device
title: Actions de l'appareil et ADB
description: Capturez, inspectez et commandez le casque sélectionné, ou lancez une commande explicite.
section: Outils
order: 30
sourceRevision: 1
---

## Actions de l'appareil {#actions}

Ouvrez **Appareil**. Sa navigation de gauche regroupe les actions, les captures/diffusions, les performances/traces et les réglages runtime/connexion. Actualisez l'état pour consulter les informations Android, batterie, réseau et stockage disponibles. Un état inconnu ne confirme ni l'activation ni la désactivation.

```image
{"id":"device-actions","src":"assets/device-actions.png","alt":"Cartes d'actions avec icônes et interrupteurs utilisant un état de casque simulé","controls":"minimal"}
```

| Outil | Usage et limites |
| --- | --- |
| Capture d'écran | Enregistrer l'écran du casque sélectionné en PNG. |
| Vidéo Android | Enregistrer un MP4 pendant la durée choisie, jusqu'à 180 secondes. Cette fonction est distincte du mirroring et de l'enregistrement MKV. |
| Navigateur | Envoyer une URL au navigateur du casque. |
| Perfetto | Sauvegarder une trace pendant la durée demandée, jusqu'à 120 secondes. |
| Profilage GPU | Demander les contrôles de profilage disponibles ; consulter l'état rapporté et la sortie de tâche. |
| Metrics HUD / CSV | Piloter le service OVR Metrics installé ; configurer les compteurs dans son plugin. |
| Proximité / Guardian | Demander l'état runtime pris en charge. Le firmware peut refuser ou ignorer la commande. |
| Alimentation | Réveiller, mettre en veille ou demander un redémarrage. Vérifier le casque avant de le redémarrer. |

Utilisez le [mirroring](doc:mirroring#start) pour la fenêtre vidéo PC, les [métriques](doc:metrics#record) pour les courbes/sessions CSV et les [plugins](doc:plugins#ovr) pour les réglages propres aux outils. La disponibilité dépend du firmware et des applications Quest installées.

## ADB par Wi-Fi {#wifi}

Depuis une connexion USB autorisée, **Activer TCP 5555** demande le port ADB TCP. Saisissez l'adresse et le port du casque voulu, puis connectez-le. **USB** repasse ADB en mode USB. Le changement de mode peut couper le transport courant ; actualisez puis sélectionnez l'entrée correspondante.

Utilisez l'adresse réellement indiquée pour votre casque sur votre réseau. Le mirroring utilise une connexion ADB déjà établie ; son démarrage ne configure pas le Wi-Fi à votre place.

## Commandes et sorties de tâches {#adb}

Ouvrez **ADB** pour saisir une commande. Choisissez le mode **shell** ou **ADB** voulu et son délai maximal. Le mode shell exécute une commande Android ; le mode ADB utilise l'exécutable ADB du PC. Vérifiez le casque et la commande avant l'exécution, surtout pour les écritures et les changements runtime.

Sélectionnez une tâche de l'historique pour consulter sa sortie, son code de retour et son état. Copiez la sortie utile pour diagnostiquer une erreur. Annulez une tâche en cours si nécessaire ; l'annulation attend l'acquittement du worker natif et ne défait pas forcément les commandes déjà acceptées par Android.

Un processus ADB terminé avec succès ne garantit pas l'état de plus haut niveau d'un outil. Actualisez son état ou consultez la réponse du service. Le [dépannage](doc:troubleshooting#commands) explique les actions refusées et les résultats manquants.
