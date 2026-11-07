---
id: quickstart
title: Installer et connecter
description: Installez l'Alpha Windows et autorisez votre première connexion Quest.
section: Pour commencer
parent: overview
order: 1
sourceRevision: 1
---

## Installer QuestDev {#install}

1. Téléchargez l'installateur Windows x64 depuis les [releases QuestDev](https://github.com/MrMybal/CyVR_QuestDev/releases). La version actuellement publiée de l'application est **0.9.0-alpha.1**.
2. Suivez l'assistant français/anglais. L'installation est propre à l'utilisateur, normalement dans `%LOCALAPPDATA%/Programs/CyVR QuestDev`, avec un raccourci du menu Démarrer et un raccourci Bureau facultatif. Aucun droit administrateur n'est requis. L'installateur n'est pas signé.
3. Vous pouvez aussi extraire le **ZIP Windows-x64 complet** dans un dossier dédié et modifiable, puis lancer `CyVR_QuestDev.exe`. Conservez tous les dossiers fournis à côté de l'exécutable.
4. Installez les [Android SDK Platform-Tools](https://developer.android.com/tools/releases/platform-tools) officiels pour ADB. QuestDev embarque Python, CyToolsCore/MCP, les plugins et scrcpy ; ADB, les CLI/comptes IA et les outils installés sur le Quest restent séparés.

La release fournit aussi les empreintes SHA-256 et les sources correspondantes. Fermez QuestDev avant de le réinstaller ou de le désinstaller. La désinstallation retire les fichiers de l'application et conserve les préférences et captures personnelles. Les [mises à jour](doc:updates#install) préservent les raccourcis et le désinstalleur.

## Connecter le Quest {#connect}

1. Activez le mode développeur du Quest selon la procédure de Meta.
2. Branchez le casque avec un câble USB capable de transférer des données. Portez-le et acceptez l'autorisation de **débogage USB** pour ce PC.
3. Lancez QuestDev. Si ADB n'est pas détecté, sélectionnez `adb.exe` dans **Réglages**.
4. Choisissez le casque en haut à droite. **F5** actualise la liste des appareils. L'état `unauthorized` demande encore une autorisation dans le casque ; une entrée `offline` n'est pas prête à recevoir des commandes.
5. Ouvrez **Applications** pour consulter les packages installés, ou **Console** et sélectionnez un package pour lire ses messages.

ADB Wi-Fi est accessible dans les [actions de l'appareil](doc:device#wifi), après l'établissement de la connexion nécessaire. Choisissez le bon casque avant de démarrer une capture, un transfert ou un lot.

## Une première session utile {#session}

Lancez votre application VR sur le Quest, ouvrez son package dans la [console](doc:console#packages) et gardez un onglet séparé pour un éventuel processus compagnon. Utilisez le [mirroring](doc:mirroring#start) pour obtenir une vue sur PC, ou installez OVR Metrics Tool puis suivez l'[enregistrement OVR](doc:metrics#record) pour les performances.

L'assistant IA peut rester déconnecté. La connexion à un fournisseur est une action distincte et explicite. L'absence de casque n'empêche pas d'ouvrir un CSV OVR local ou de lire le manuel.

En cas d'échec, consultez le [dépannage de connexion](doc:troubleshooting#connection). Évitez de relancer des commandes sur un appareil non autorisé ou déconnecté.
