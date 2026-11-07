---
id: console
title: Journaux par application
description: Séparez les packages, suivez le direct, copiez le RAW et triez les colonnes.
section: Outils
order: 10
sourceRevision: 1
---

## Sélectionner les applications {#packages}

La colonne gauche de **Console** affiche les applications actives, installées ou système. Recherchez un package puis ouvrez-le dans un onglet de journal. L'attribution comprend ses processus et sous-processus, avec un suivi de leur durée de vie. **Toutes les applications** regroupe la capture ; **Système / non attribué** conserve les origines qui ne peuvent pas être affectées de façon fiable. Un UID partagé ne suffit pas à deviner l'identité d'une application.

Activez **Côte à côte** pour comparer des vues indépendantes. Chaque vue conserve ses filtres, sa pause et sa sélection. La capture commence au présent ; les interruptions peuvent laisser des messages manquants.

```image
{"id":"compare-logs","src":"assets/comparison.png","alt":"Deux vues de journaux d'applications avec des messages simulés distincts","controls":"minimal"}
```

## Suivre et filtrer {#follow}

Recherchez dans le texte des messages et utilisez les filtres de niveau, tag, PID, exclusion et les filtres avancés selon les besoins. Remonter dans la vue suspend le suivi. Revenir manuellement en bas le réactive ; **Suivre le direct** rejoint également les derniers messages. La pause fige l'affichage pendant que la capture continue, dans les [limites de rétention](doc:settings#retention).

Effacer le journal local est une action distincte de la pause. Les anciens messages retirés par les limites ne sont plus disponibles pour la copie ou l'export. Les compteurs du bas indiquent les lignes retenues, la mémoire et les messages retirés/perdus.

## Copier le texte RAW original {#raw}

Choisissez **RAW** pour lire les lignes Logcat originales : horodatage, espaces, PID, TID, niveau, tag et message. Sélectionnez du texte à la souris puis appuyez sur **Ctrl+C**. **Ctrl+A** sélectionne la page RAW affichée. Une sélection fige son contenu afin que les nouveaux messages ne déplacent pas les caractères sélectionnés ; **Libérer la sélection** reprend l'actualisation.

Les pages RAW sont limitées à **10 000 lignes ou 2 Mio**. Utilisez Précédentes/Suivantes pour parcourir les pages retenues. **Copier toutes les lignes filtrées** et l'export texte comprennent toute la vue filtrée, même si elle dépasse une page RAW. Le RAW conserve l'ordre de réception.

```image
{"id":"raw-log","src":"assets/console-raw.png","alt":"Lignes Logcat originales sélectionnées dans la vue RAW avec des données de démonstration","controls":"minimal"}
```

## Trier et exporter {#sort}

Dans **Colonnes**, cliquez sur un en-tête pour choisir l'ordre croissant ou décroissant. Maintenez **Maj** pendant le clic pour ajouter une clé de tri. Heure, niveau, PID/TID, application, tag et message sont triables. Les niveaux suivent leur gravité ; PID/TID sont numériques ; les égalités conservent l'ordre de réception.

La copie et l'export du tableau respectent son tri. Le suivi du direct fonctionne avec l'heure locale croissante ; les autres tris restent stables pour l'analyse. Exportez la vue filtrée en texte UTF-8 ou JSON Lines. JSON Lines conserve les champs structurés.

| Raccourci dans la console | Action |
| --- | --- |
| Espace | Mettre en pause/reprendre la vue |
| Ctrl+L | Effacer le journal local |
| Ctrl+S | Exporter la vue filtrée |
| Ctrl+C | Copier la ligne sélectionnée, ou le texte sélectionné en RAW |
| Ctrl+Maj+C | Copier toutes les lignes filtrées |
| F1 | Ouvrir ce guide |

Les éditeurs de texte gardent leurs raccourcis de sélection habituels ; ne supposez pas qu'un raccourci global a été exécuté pendant la saisie. Relisez les journaux avant de les partager, notamment avec le [contexte IA](doc:assistant#context).
