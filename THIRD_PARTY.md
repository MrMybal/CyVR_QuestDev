# Dépendances

## CyUI / CyImgui_Interface

Bibliothèque de widgets et thèmes, liée depuis le dossier indiqué par `CYUI_ROOT` ou détecté près du projet. Licence MIT, copyright 2026 Cyberalien. Le code d’hébergement Win32/DX11 est adapté de son exemple. Notice complète : [licenses/CyUI.txt](licenses/CyUI.txt).

## Dear ImGui

Sources et backends Win32/DirectX 11 fournis par le dépôt CyUI. Licence MIT, copyright 2014–2026 Omar Cornut. Notice complète : [licenses/ImGui.txt](licenses/ImGui.txt).

## Android Debug Bridge

Exécutable externe des Android SDK Platform-Tools, détecté sur la machine ou sélectionné dans les réglages. Aucun binaire ADB n’est inclus. [Documentation Android officielle](https://developer.android.com/tools/adb).

## CyToolsCore

[CyToolsCore](https://github.com/MrMybal/CyToolsCore), 0.10.1, licence MIT, copyright 2026 Cyberalien. Le projet distribue sa roue Python officielle et utilise son runtime, ses sessions/jobs, son client HTTP et son adaptateur MCP. Licence : [LICENCES/CyToolsCore-MIT.txt](LICENCES/CyToolsCore-MIT.txt). Référence locale inspectée : commit `d53159087157530b3d4c8e58c09a008a544a7fcb`. SHA-256 de la roue incluse : `5f2f6f0eccdcce960ff7ff9deee36280a4ee1c671cc2ba29b103fa15bb479702`.

## Transports de fournisseurs IA

La bibliothèque portable dans `assistant/providers` provient du module MIT `Bridge/cyue_mcp/providers` de CyUEMCPSDK, copyright 2026 Cyberalien. Copie locale autonome, sans dépendance d’exécution à son dépôt/laboratoire. Le transport Codex est adapté pour les outils dynamiques QuestDev et l’identité de l’application. Licence : [LICENCES/ProviderTransports-MIT.txt](LICENCES/ProviderTransports-MIT.txt).

## JSON for Modern C++

`third_party/json.hpp`, nlohmann/json 3.11.3, MIT, copyright 2013–2023 Niels Lohmann. Licence : [LICENCES/nlohmann-json-MIT.txt](LICENCES/nlohmann-json-MIT.txt).

## Python / MCP

Le runtime privé installe les dépendances de la roue CyToolsCore et le SDK MCP officiel. Versions : [runtime/requirements.lock](runtime/requirements.lock). Notices et licences récupérées dans les distributions installées : [LICENCES/Python-dependencies.txt](LICENCES/Python-dependencies.txt). Python de base et CLI des fournisseurs restent des dépendances externes installées par l’utilisateur.

Les commandes HUD/CSV suivent la [documentation officielle OVR Metrics](https://developers.meta.com/vr/documentation/native/android/ts-ovrmetricstool/). Aucun APK Meta, binaire MQDH ou code propriétaire de MQDH n’est distribué.

Le plugin CyInsightOVR implémente l’interopérabilité avec le protocole de l’application `org.cyinsights.ovr`. La distribution CyInsightOVR (AGPL-3.0-or-later) reste externe : aucun APK, JAR ou code Java du collecteur n’est embarqué dans QuestDev. L’utilisateur sélectionne son JAR officiel pour l’initialisation sur le casque.
