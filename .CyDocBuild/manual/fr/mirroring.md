---
id: mirroring
title: Mirroring et enregistrement
description: Affichez le Quest sur PC, réglez l'audio et enregistrez une session MKV.
section: Outils
parent: device
order: 31
sourceRevision: 1
---

## Démarrer la fenêtre vidéo {#start}

Connectez et autorisez le Quest, puis ouvrez **Appareil → Mirroring vidéo / audio → Démarrer le mirroring…**. Choisissez la résolution maximale, la fréquence, le débit et le mode audio, puis démarrez la diffusion. Une fenêtre vidéo native dédiée s'ouvre. **Voir la vidéo** la ramène au premier plan ; **F11** bascule le plein écran. La fenêtre est redimensionnable.

Les réglages initiaux sont **1920 px, 60 FPS et 12 Mbit/s**, en H.264 avec décodage matériel automatique. Ce sont des limites demandées, sans garantie de résolution ou de cadence reçue. QuestDev indique les dimensions et la fréquence réelles. Réduisez la résolution ou le débit si la liaison sature. Une demande de 120 FPS ne garantit pas cette cadence.

```image
{"id":"mirror-settings","src":"assets/mirroring-settings.png","alt":"Options vidéo, audio, recadrage et enregistrement du mirroring","controls":"minimal"}
```

La capture montre des réglages de démonstration, sans flux réel du casque. Le moteur fourni est scrcpy 5.0.1. Le mirroring utilise la connexion ADB sélectionnée, y compris en Wi-Fi si elle est déjà établie. Son démarrage ne change pas le réseau, ne lance pas d'application VR et ne modifie pas la veille. Le contrôle à distance et la synchronisation du presse-papiers sont désactivés.

## Audio et recadrage {#audio}

Choisissez l'absence d'audio, sa duplication sur PC en conservant le son du casque, ou une sortie sur PC seulement. La duplication nécessite Android 13+ et une application autorisant la capture audio. Un refus apparaît dans le journal de session ; le silence peut aussi venir de l'application. Aucun microphone n'est capturé.

Le recadrage personnalisé utilise largeur, hauteur et décalages X/Y dans les coordonnées de l'écran du casque. Le rectangle adapté à un œil dépend du modèle et du firmware. Le moteur signale les rectangles refusés dans son journal. Vérifiez le résultat à partir de l'image réellement reçue.

## Enregistrer un MKV {#record}

Avant de démarrer, choisissez un fichier **MKV** dans les réglages. La vidéo et l'audio activé sont enregistrés pendant l'affichage. Le remplacement d'un fichier existant demande confirmation dans le sélecteur Windows. QuestDev réinitialise ce choix au prochain démarrage pour éviter une réécriture involontaire.

**Arrêter et finaliser**, fermer la fenêtre vidéo ou fermer normalement QuestDev finalise le MKV. Un changement de casque ou sa déconnexion arrête également la session. Un arrêt forcé du moteur peut laisser un enregistrement incomplet et reste signalé. Le journal borné se copie dans **Réglages / journal** ; ce panneau donne aussi accès au dernier fichier.

L'enregistrement MKV est indépendant de la [capture Android MP4](doc:device#actions) et n'a pas sa limite de 180 secondes. L'espace disque disponible reste une contrainte. Consultez le [dépannage](doc:troubleshooting#media) en cas d'image ou de son manquant, et l'[automatisation](doc:automation#operations) pour `mirror_start`/`mirror_stop`.
