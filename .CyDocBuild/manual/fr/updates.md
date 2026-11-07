---
id: updates
title: Mettre à jour et restaurer
description: Choisissez un canal, vérifiez le téléchargement et restaurez l'installation précédente.
section: Préférences
parent: settings
order: 81
sourceRevision: 1
---

## Rechercher et télécharger une release {#download}

Ouvrez **Réglages → Mises à jour GitHub**. Choisissez **Alpha et préversions** pour les versions Alpha ; le mode stable ignore les préversions. La version publiée compatible la plus récente est choisie par son numéro, sans proposer de retour en arrière. Les brouillons et distributions incompatibles sont ignorés. La recherche au démarrage est facultative et désactivée initialement ; le téléchargement reste explicite.

Le dépôt est public : l'accès aux releases ne nécessite pas de connexion, sous réserve des limites de requêtes GitHub. **Connexion Git existante** peut réutiliser Git Credential Manager sans demande interactive en arrière-plan. Pour un accès authentifié sans Git, saisissez dans le champ masqué un jeton à accès fin limité à ce dépôt avec **Contents : lecture**. Il est conservé dans le Gestionnaire d'identifiants Windows. **Oublier ce jeton** retire seulement l'entrée QuestDev, sans déconnecter Git ni les fournisseurs IA.

**Télécharger et vérifier** récupère le ZIP Windows x64, vérifie son SHA-256 contre GitHub et `SHA256SUMS.txt`, puis contrôle le manifeste des fichiers extraits. L'autorisation API n'est pas transmise aux serveurs de fichiers lors des redirections. Le téléchargement est annulable ; un paquet incomplet ou refusé n'est pas installé.

```image
{"id":"update-settings","src":"assets/github-updates.png","alt":"Canaux de release GitHub et actions de mise à jour dans les réglages de démonstration","controls":"minimal"}
```

## Installer et redémarrer {#install}

Terminez ou annulez les listes de fichiers et arrêtez captures, enregistrements et mirroring avant l'installation. **Installer et redémarrer** prépare l'installation à côté du dossier courant. Un programme séparé attend la fermeture normale de QuestDev, sauvegarde l'ancien dossier, le remplace puis attend que la nouvelle app confirme le démarrage de son interface et de ses services. Si le démarrage échoue, il tente de restaurer et relancer la sauvegarde.

Les préférences dans `%LOCALAPPDATA%/CyVR/QuestDev` restent en place. Les fichiers personnels supplémentaires et plugins ajoutés dans le dossier de l'app sont conservés. Si un fichier fourni a été modifié ou qu'un fichier personnel entre en conflit avec un nouveau composant, l'installation automatique est refusée avant remplacement.

L'installation automatique nécessite un dossier dédié et modifiable issu du ZIP portable ou de l'installateur par utilisateur. Les builds de développement, copies Git, dossiers système et liens/jonctions dans l'arbre remplacé sont refusés. Aucune élévation administrateur n'est lancée. De gros dossiers personnels peuvent nécessiter une installation manuelle dans un emplacement dédié. Les raccourcis et fichiers de désinstallation Windows sont conservés pour les distributions installées.

## Restaurer une sauvegarde {#restore}

**Restaurer et redémarrer** revient à la sauvegarde associée au dossier courant de l'application. Les sauvegardes ne sont pas supprimées automatiquement. **Ouvrir le dossier des mises à jour** affiche téléchargements et résultats ; les sauvegardes de l'installation résident dans des dossiers voisins de l'application installée.

Les mises à jour sont disponibles à partir de QuestDev 0.7. Les versions antérieures nécessitent un premier téléchargement manuel. Les copies de sources utilisent leur compilation de développement plutôt que le remplacement automatique. La [page des releases](https://github.com/MrMybal/CyVR_QuestDev/releases) fournit l'installateur, le ZIP portable, les sources et les empreintes publiés.
