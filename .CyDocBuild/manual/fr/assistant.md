---
id: assistant
title: Assistant IA
description: Choisissez un fournisseur, vérifiez le contexte partagé et réglez les permissions d'action.
section: Intégrations
order: 70
sourceRevision: 1
---

## Connecter un fournisseur {#connect}

Ouvrez **Assistant IA**, choisissez un fournisseur et un modèle, puis **Connecter / appliquer le profil**. Changer de fournisseur, de modèle ou d'adresse de service nécessite cette action de connexion explicite.

- **Codex** réutilise le compte connecté à son CLI ; QuestDev ne copie pas ses jetons. Les modèles viennent du catalogue du compte. Sans modèle explicite, QuestDev utilise le défaut de ce catalogue.
- **Ollama** nécessite l'URL du service local et un modèle installé.
- **OpenRouter / Anthropic** demandent le nom d'une variable d'environnement contenant la clé, ou une clé dans le champ masqué **Clé API · session**. Le champ est effacé après transmission au compagnon ; le profil sauvegardé ne contient pas cette clé.

Les services et CLI des fournisseurs restent externes. Les connexions HTTP refusent les redirections et exigent HTTPS hors loopback. Une connexion ou requête échouée reste une erreur, sans réponse terminée fictive.

```image
{"id":"assistant-setup","src":"assets/assistant.png","alt":"Choix du fournisseur et réglages du contexte partagé dans l'assistant IA, sans compte connecté","controls":"minimal"}
```

## Vérifier le contexte {#context}

Les deux interrupteurs de contexte règlent l'inclusion automatique du contexte QuestDev et des derniers journaux visibles. Le contexte peut inclure la page, le casque, la liste d'applications, les filtres, plugins, permissions, indications d'usage et jusqu'à 50 messages de la vue courante, tronqués à 1 000 caractères chacun. **Inspecter le contexte envoyé** permet de le relire avant une requête.

Désactiver l'inclusion automatique des logs ne retire pas à un outil son droit de les lire. Le fournisseur reçoit les données de requête et les résultats d'outils que vous lui confiez. Relisez le contenu avant de l'envoyer, noms de packages et messages capturés compris.

## Discuter et utiliser les outils {#tools}

La discussion affiche Markdown, blocs de code et tableaux. Les réponses arrivent progressivement lorsque le fournisseur le permet et restent annulables. Le profil **Codex** intégré appelle les outils natifs `questdev_*` par CyToolsCore ; la discussion affiche les appels et leurs états. Les autres profils intégrés proposent actuellement du texte contextualisé. Leur contrôle externe de QuestDev nécessite un [client MCP compatible](doc:automation#connect).

Choisissez la politique de l'app : **Lecture seule**, **Demander avant action** ou **Autoriser les actions**. Le défaut demande confirmation dans QuestDev. L'assistant intégré et les clients externes MCP/SDK suivent la même politique native. Lecture seule n'autorise pas les mutations ; une opération peut aussi demander sa capacité Core.

L'annulation attend l'acquittement du worker. Une commande déjà appliquée sur Android peut nécessiter de relire l'état ; l'annulation n'est pas un Undo universel. Consultez [MCP et SDK](doc:automation#jobs) pour les exports et la durée de vie des tâches. Les autres outils QuestDev restent utilisables sans fournisseur connecté.
