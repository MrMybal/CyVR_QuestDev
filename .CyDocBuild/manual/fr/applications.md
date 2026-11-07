---
id: applications
title: Gérer les applications
description: Comptez, filtrez, installez, lancez et arrêtez les packages du Quest.
section: Outils
order: 20
sourceRevision: 1
---

## Retrouver un package {#list}

Ouvrez **Applications**, puis choisissez les applications utilisateur, les applications actives, les packages système ou tous les packages dans sa navigation de gauche. La recherche réduit la catégorie courante. Les compteurs indiquent les applications détectées et correspondantes après filtrage ; ils ne comptent pas des lignes de journal.

Actualisez les packages après une installation réalisée hors de QuestDev. Choisissez le bon casque dans la barre supérieure avant d'effectuer une action.

```image
{"id":"application-list","src":"assets/applications.png","alt":"Tableau des packages Quest avec compteurs par catégorie et actions en mode démonstration","controls":"minimal"}
```

## Installer, lancer et arrêter {#manage}

- **Installer un APK** ouvre un sélecteur de fichier local et installe cet APK sur le Quest sélectionné. Utilisez un APK régulier compatible ; l'installation de bundles APK n'est pas implémentée.
- L'action **lancer** ouvre l'activité de démarrage du package lorsqu'Android en fournit une. Un package sans activité peut être un service plutôt qu'une application lançable directement.
- **Arrêter** demande un arrêt forcé Android pour ce package. Cette action interrompt l'application ; utilisez-la volontairement pendant vos essais.
- **Logs** ouvre ce package dans la [console](doc:console#packages). Un package affiché peut ne pas avoir encore émis de message retenu.

Ces actions dépendent de l'autorisation ADB et des permissions Android. Une commande acceptée ne prouve pas que l'application a réussi son initialisation. Consultez la sortie de la tâche dans [ADB](doc:device#adb) et ses journaux si elle ne démarre pas comme prévu.

Pour configurer les outils installés sur le Quest, utilisez leurs [plugins](doc:plugins#ovr). Installer OVR Metrics ou CyInsightOVR est distinct de l'installation de QuestDev sur le PC.
