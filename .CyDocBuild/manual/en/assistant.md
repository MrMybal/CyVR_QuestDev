---
id: assistant
title: AI assistant
description: Choose a provider, inspect shared context and control action permissions.
section: Integrations
order: 70
revision: 1
---

## Connect a provider {#connect}

Open **AI assistant / Assistant IA**, choose a provider and model, then **Connect / apply profile**. A provider change, model change or endpoint change needs that explicit connection action.

- **Codex** reuses the account connected to its CLI; QuestDev does not copy its tokens. Models come from the account's catalogue. Without an explicit model, QuestDev uses the catalogue default.
- **Ollama** needs the local endpoint URL and an installed model.
- **OpenRouter / Anthropic** need an environment-variable name containing the key, or a key in the masked **Session API key** field. The field is cleared after passing it to the companion; a saved profile does not contain that key.

Provider services and CLIs are external. HTTP endpoints reject redirects and require HTTPS outside loopback. A failed connection or request is reported as an error, not a completed reply.

```image
{"id":"assistant-setup","src":"assets/assistant.png","alt":"Provider selection and shared-context settings in the AI assistant, with no connected account","controls":"minimal"}
```

## Inspect the context {#context}

The two context switches control automatic inclusion of the QuestDev session context and recent visible logs. Context can include the page, selected headset, application list, filters, plugins, permissions, usage hints and up to 50 current-view messages truncated to 1,000 characters each. **Inspect sent context** lets you review it before a request.

Disabling automatic log inclusion does not remove a tool's permission to read logs. A provider receives the request data and tool results you give it. Review content before sending it, including captured package names and messages.

## Discuss and use tools {#tools}

Conversations display Markdown, code blocks and tables. Replies stream when supported and can be cancelled. The integrated **Codex** profile can call native `questdev_*` tools through CyToolsCore; the conversation shows the calls and their states. Other integrated provider profiles currently offer text with context. Their external control of QuestDev requires a compatible [MCP client](doc:automation#connect).

Choose the app's action policy: **Read only**, **Ask before action** or **Allow actions**. The default asks for confirmation in QuestDev. The integrated assistant and external MCP/SDK clients follow the same native policy. Read-only mode does not grant mutations; an operation can also require its Core capability.

Cancellation requests worker acknowledgement. A command already applied on Android may still need state rereading; cancellation is not a universal Undo. For data export and task lifetime, see [MCP and SDK](doc:automation#jobs). You can use the rest of QuestDev without connecting any provider.
