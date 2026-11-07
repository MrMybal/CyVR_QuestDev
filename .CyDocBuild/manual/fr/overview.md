---
id: overview
title: CyVR QuestDev
description: Retrouvez les outils utiles à votre session de développement Meta Quest.
section: Pour commencer
order: 0
sourceRevision: 1
---

## Votre espace de travail {#workspace}

CyVR QuestDev est une application Windows pour les développeurs Meta Quest. Suivez les journaux de chaque application, gérez les packages installés, utilisez les commandes du casque, transférez des fichiers, affichez son écran et analysez les rapports de performances OVR depuis un même espace.

L'application est une **Alpha**. Certaines commandes et certains compteurs dépendent du firmware et d'applications Quest externes. Windows 10/11 x64 est pris en charge ; aucune application Linux ou macOS n'est fournie. L'assistant IA est facultatif pour les usages ADB habituels.

```image
{"id":"console-overview","src":"assets/console.png","alt":"Console QuestDev avec une application sélectionnée et des journaux simulés","controls":"minimal"}
```

Les captures de ce manuel proviennent de la véritable application en mode démonstration. Les packages, messages et mesures sont simulés. Certaines captures précèdent les composants communs CyUI ; elles illustrent les usages, sans connexion réelle à un casque.

## Choisir une tâche {#tasks}

| Tâche | Guide |
| --- | --- |
| Installer et connecter le casque | [Première connexion](doc:quickstart#connect) |
| Isoler les messages et copier les journaux originaux | [Console](doc:console#raw) |
| Installer, lancer ou arrêter une application | [Applications](doc:applications#manage) |
| Capturer ou commander le casque | [Appareil et ADB](doc:device#actions) |
| Afficher et enregistrer le casque sur PC | [Mirroring](doc:mirroring#start) |
| Préparer un lot de transferts PC/Quest | [Fichiers](doc:files#batch) |
| Tracer et enregistrer les rapports OVR | [Métriques](doc:metrics#record) |
| Configurer OVR Metrics ou CyInsightOVR | [Plugins](doc:plugins#ovr) |
| Connecter une IA ou un outil externe | [Assistant](doc:assistant#connect), [MCP et SDK](doc:automation#connect) |
| Régler l'apparence, la rétention ou les mises à jour | [Réglages](doc:settings#retention), [Mises à jour](doc:updates#download) |

## Utiliser ce manuel {#manual}

Ouvrez **Aide / F1** dans QuestDev pour consulter la page correspondant à l'outil courant. Le manuel installé et le site utilisent le même lecteur généré. La lecture de la copie installée ne nécessite ni serveur ni connexion. Les téléchargements et les fournisseurs IA ont besoin de leurs propres connexions.

Aide/F1 est inclus dans les compilations des sources actuelles. Les binaires Alpha 0.9.0 déjà publiés précèdent cette intégration ; utilisez ce manuel en ligne avec ces binaires.

Choisissez English ou Français dans le lecteur, parcourez l'arborescence et utilisez **Ctrl+K** pour la recherche globale. Les adresses des pages et sections restent stables entre les langues. Les images s'agrandissent dans le lecteur ; **Échap** les réduit. **Mon espace** conserve des favoris et des notes lorsque le stockage du navigateur est disponible ; sa persistance dépend de la politique appliquée aux fichiers locaux ou au site.

La traduction de l'interface QuestDev reste partielle. Les packages, commandes, journaux capturés et libellés littéraux gardent leur écriture d'origine. Ce manuel est entièrement disponible en français et en anglais.

Consultez le [dépannage](doc:troubleshooting#connection) si un appareil ou un rapport manque, et la [licence](doc:licensing#license) pour les conditions de distribution.
