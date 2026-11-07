---
id: settings
title: Apparence et rétention des logs
description: Réglez ADB, les thèmes, la densité, la langue et les limites du journal local.
section: Préférences
order: 80
sourceRevision: 1
---

## Adapter l'espace de travail {#appearance}

Ouvrez **Réglages** et choisissez la section voulue dans sa navigation de gauche. Sélectionnez `adb.exe` si la détection automatique ne trouve pas les Android SDK Platform-Tools. Les changements s'appliquent aux outils utilisant la connexion sélectionnée.

QuestDev propose cinq thèmes, une densité confortable/compacte et la mise à l'échelle DPI. Les préférences de langue et de densité sont sauvegardées. La localisation EN/FR concerne les vues migrées et reste partielle dans l'application ; les pages de ce manuel sont entièrement traduites. Les logs et sorties de commandes gardent leur langue d'origine.

## Choisir les limites du journal {#retention}

Ouvrez **Limites du journal**, ou cliquez sur les compteurs de lignes/mémoire au bas de l'app. Réglez le maximum de lignes retenues et le budget mémoire. Le défaut est **50 000 lignes / 64 Mio**. Les contrôles permettent 1 000–5 000 000 lignes et 16–2 048 Mio, avec les préréglages Standard, Étendu et Longue session.

Les deux limites s'appliquent au journal local conservé. Les plus anciennes entrées sont retirées dès qu'une limite est atteinte ; l'augmenter ensuite ne les restaure pas. Une pause d'affichage ne suspend pas la capture. La barre d'état indique les messages retirés/perdus, y compris les pertes d'ingestion bornée lorsqu'elles surviennent.

```image
{"id":"log-budget","src":"assets/log-settings.png","alt":"Réglages du nombre de lignes retenues et du budget mémoire du journal","controls":"minimal"}
```

Choisissez un budget adapté au PC et à la durée de session. Exportez les journaux utiles avant qu'ils sortent de la rétention. La pagination RAW est une autre limite, propre à l'affichage, décrite dans [Console](doc:console#raw).

## Préférences enregistrées et autres réglages {#storage}

Les préférences utilisateur résident dans `%LOCALAPPDATA%/CyVR/QuestDev`, séparément de l'installation. Les profils de plugins s'enregistrent explicitement ; les préférences des volets de fichiers et de préparation sont mémorisées, tandis que la liste de transferts ne dure que le temps de la session.

Consultez [Mises à jour](doc:updates#download) pour les canaux et l'installation. La connexion fournisseur et la politique d'action sont dans [Assistant IA](doc:assistant#connect). Conservez tout le dossier d'une distribution portable pour garder le runtime, les plugins et le manuel local disponibles.
