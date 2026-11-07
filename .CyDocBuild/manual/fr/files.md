---
id: files
title: Fichiers et lots de transferts
description: Parcourez les volets PC/Quest, préparez les transferts, annulez ou relancez-les.
section: Outils
order: 40
sourceRevision: 1
---

## Parcourir et sélectionner {#browse}

Ouvrez **Fichiers**. Choisissez un emplacement du Quest dans la colonne gauche ou saisissez un chemin. Double-cliquez sur un dossier pour l'ouvrir. Activez **Côte à côte** pour obtenir des volets PC et Quest indépendants, chacun avec son chemin, son filtre et sa sélection. Utilisez Ctrl/Maj pour la sélection multiple et Ctrl+A pour le volet courant. Les préférences de fichiers cachés et de liste préparée sont sauvegardées.

Seuls les fichiers réguliers sont transférés. Sélectionner un dossier sert à le parcourir ; la copie de dossiers et la suppression récursive ne sont pas implémentées. Les restrictions Android habituelles s'appliquent, sans root ni suivi automatique des liens. Chaque liste est limitée à 5 000 entrées.

```image
{"id":"file-batch","src":"assets/files-batch.png","alt":"Volets de fichiers PC et Quest avec trois transferts préparés et des chemins simulés","controls":"minimal"}
```

## Préparer puis lancer un lot {#batch}

1. Activez **Préparer une liste**. Sélectionnez les fichiers puis ajoutez-les depuis un volet ou le sélecteur Windows multiple.
2. Vérifiez le sens, la source, la destination, la taille, le casque et l'autorisation de remplacement de chaque entrée. Ces valeurs sont fixées à l'ajout ; parcourir d'autres dossiers ne les modifie pas.
3. Ajoutez au besoin des fichiers d'autres dossiers, puis choisissez **Lancer le lot**. Les transferts s'exécutent dans l'ordre, en arrière-plan.
4. Choisissez si **Arrêt sur erreur** doit suspendre le lot dès un échec.
5. Pour réessayer, utilisez **Remettre en attente…** pour les échecs seuls, ou les échecs et annulations, puis relancez explicitement le lot.

La liste contient jusqu'à 2 000 fichiers et dure le temps de la session. Elle ne redémarre pas automatiquement à l'ouverture de QuestDev. Retirez une destination déjà présente avant de la réutiliser. Quand la préparation est désactivée, les nouveaux transferts démarrent immédiatement et restent annulables dans la liste.

## Annuler et reprendre {#cancel}

La croix d'une entrée annule ce fichier. **Annuler le lot** annule les entrées en attente et demande l'arrêt du transfert actif. Les fichiers déjà terminés restent en place. Changer de casque ou le déconnecter suspend le lot ; les entrées en attente restent liées à leur appareil d'origine. Reconnectez-le et relancez explicitement.

Les sources sont revérifiées au démarrage de chaque fichier. Les envois utilisent un temporaire distant ; les téléchargements finalisent un temporaire local après réussite. Un téléchargement annulé conserve l'ancienne destination locale. Une déconnexion peut empêcher le nettoyage du temporaire distant ; consultez l'erreur dans [ADB](doc:device#adb).

**Remplacer** est désactivé initialement. Son activation autorise le remplacement pour les nouvelles entrées uniquement ; l'infobulle d'une entrée indique sa permission enregistrée. Terminez ou annulez les listes avant une mise à jour ou un redémarrage de l'app.

## Actions sur les dossiers et limites {#actions}

Les créations, renommages et suppressions montrent la cible avant confirmation. Renommer ne remplace pas une autre entrée. Un dossier doit être vide pour être supprimé ; supprimer un lien retire seulement ce lien. Les noms sont transmis comme arguments littéraux, espaces et apostrophes compris. Renommez un fichier Android incompatible avec Windows avant de le télécharger.

Android peut refuser l'accès ou ne pas exposer un dossier ; QuestDev ne contourne pas ses permissions. Les opérations sont aussi accessibles par [MCP et SDK](doc:automation#operations), dont les fichiers de sortie doivent être exportés avant la fermeture de la tâche temporaire.
