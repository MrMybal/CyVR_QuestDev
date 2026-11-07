# Dépendances

Le code original de CyVR QuestDev est sous **GNU GPLv3 uniquement** (`GPL-3.0-only`), copyright 2026 Cyberalien. Voir [LICENSE](LICENSE) et [COPYRIGHT](COPYRIGHT). Les notices ci-dessous précisent les licences propres aux composants tiers ; elles sont conservées lors de leur distribution.

## CyUI / CyImgui_Interface

Bibliothèque commune [CyUI 1.2](https://github.com/MrMybal/CyUI), révision `ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3`, épinglée dans `cmake/CyUI.cmake`. Modules Core, Plots, Images, DocumentViews et CodeEditor, avec Code et Canvas comme dépendances. Licence [MIT dans le dépôt CyUI](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE), copyright 2026 Cyberalien. Le code d’hébergement Win32/DX11 est adapté de son exemple. Notice complète incluse dans QuestDev : [licenses/CyUI.txt](licenses/CyUI.txt).

CodeEditor ajoute le backend MIT ImGuiColorTextEdit de Johan A. Goossens : [notice](licenses/CyUI-TextEditor.txt). Code compile les moteurs lexicaux Lexilla et utilise les en-têtes Scintilla et des propriétés SciTE, sous leur licence permissive de Neil Hodgson et contributeurs : [Lexilla](licenses/CyUI-Lexilla.txt), [Scintilla](licenses/CyUI-Scintilla.txt), [SciTE](licenses/CyUI-SciTE.txt). Leurs révisions et correctifs sont ceux du commit CyUI épinglé (`third_party/texteditor/VERSION.json` et `third_party/code-syntax.lock.json`). Aucune interface Scintilla ni application SciTE n’est liée.

## CyDoc

Le manuel local et sa copie web utilisent le lecteur officiel [CyDoc 0.1.0](https://github.com/MrMybal/CyDoc), révision `93f64486d61dae27a94ae1e5a05a20f6edd99914`, sous [MIT](licenses/CyDoc.txt). Le paquet compilateur et son origine sont conservés dans `.CyDocBuild/vendor/` ; seul le module image est activé. Le helper natif `third_party/cydoc/cydoc_link.hpp` provient de l’exemple d’intégration de cette même révision. Les scripts générés utilisent aussi marked, PrismJS et esbuild sous MIT ; leurs textes complets sont inclus dans `Documentation/DocData/licenses/` avec celui de CyDoc. Ces licences ne changent pas celle du contenu du manuel, qui reste GPL-3.0-only.

## CyVR_UI — projet associé

[CyVR_UI](https://github.com/MrMybal/CyVR_UI) est distribué sous licence [MIT](https://github.com/MrMybal/CyVR_UI/blob/main/LICENSE).

## Dear ImGui

Sources et backends Win32/DirectX 11 fournis par le dépôt CyUI. Licence MIT, copyright 2014–2026 Omar Cornut. Notice complète : [licenses/ImGui.txt](licenses/ImGui.txt).

## Installateur Windows

L’installateur utilise [Inno Setup 6.7.4](https://github.com/jrsoftware/issrc/releases/tag/is-6_7_4), copyright 1997–2026 Jordan Russell et portions copyright 2000–2026 Martijn Laan, sous [licence Inno Setup](LICENCES/Inno-Setup.txt). Le compilateur est un outil de build externe ; le programme d’installation et de désinstallation conserve ses notices.

## Android Debug Bridge

Exécutable externe des Android SDK Platform-Tools, détecté sur la machine ou sélectionné dans les réglages. Aucun binaire ADB n’est inclus. [Documentation Android officielle](https://developer.android.com/tools/adb).

## CyToolsCore

[CyToolsCore](https://github.com/MrMybal/CyToolsCore), 0.10.1, licence MIT, copyright 2026 Cyberalien. Le projet distribue sa roue Python officielle et utilise son runtime, ses sessions/jobs, son client HTTP et son adaptateur MCP. Licence : [LICENCES/CyToolsCore-MIT.txt](LICENCES/CyToolsCore-MIT.txt). Référence locale inspectée : commit `d53159087157530b3d4c8e58c09a008a544a7fcb`. SHA-256 de la roue incluse : `5f2f6f0eccdcce960ff7ff9deee36280a4ee1c671cc2ba29b103fa15bb479702`.

## Transports de fournisseurs IA

La bibliothèque portable dans `assistant/providers` provient du module MIT `Bridge/cyue_mcp/providers` de CyUEMCPSDK, copyright 2026 Cyberalien. Copie locale autonome, sans dépendance d’exécution à son dépôt/laboratoire. Le transport Codex est adapté pour les outils dynamiques QuestDev et l’identité de l’application. Licence : [LICENCES/ProviderTransports-MIT.txt](LICENCES/ProviderTransports-MIT.txt).

## JSON for Modern C++

`third_party/json.hpp`, nlohmann/json 3.11.3, MIT, copyright 2013–2023 Niels Lohmann. Licence : [LICENCES/nlohmann-json-MIT.txt](LICENCES/nlohmann-json-MIT.txt).

## Python / MCP

Le runtime privé installe les dépendances de la roue CyToolsCore et le SDK MCP officiel. Versions : [runtime/requirements.lock](runtime/requirements.lock). Notices et licences récupérées dans les distributions installées : [LICENCES/Python-dependencies.txt](LICENCES/Python-dependencies.txt). Dans le dépôt de développement, Python de base et les CLI des fournisseurs restent des dépendances externes installées par l’utilisateur.

Les archives Alpha Windows embarquent Python 3.13.17, distribution officielle sous licences PSF et notices associées, conservées dans `runtime/python/Scripts/LICENSE.txt`. Les licences des distributions Python restent également présentes dans leurs métadonnées. Le runtime C++ de l’application native est lié statiquement.

Les commandes HUD/CSV suivent la [documentation officielle OVR Metrics](https://developers.meta.com/vr/documentation/native/android/ts-ovrmetricstool/). Aucun APK Meta, binaire MQDH ou code propriétaire de MQDH n’est distribué.

Le plugin CyInsightOVR implémente l’interopérabilité avec le protocole de l’application `org.cyinsights.ovr`. La distribution CyInsightOVR (AGPL-3.0-or-later) reste externe : aucun APK, JAR ou code Java du collecteur n’est embarqué dans QuestDev. L’utilisateur sélectionne son JAR officiel pour l’initialisation sur le casque.

## Mirroring scrcpy

Le runtime officiel [scrcpy 5.0.1](https://github.com/Genymobile/scrcpy/tree/v5.0.1), copyright Genymobile et contributeurs, est distribué sans modification dans les archives Windows. Il conserve sa licence [Apache-2.0](LICENCES/scrcpy-Apache-2.0.txt). Les DLL officielles utilisent FFmpeg 9.0.2 (LGPL-2.1-or-later), SDL 3.4.18 (zlib), libusb 1.0.30 (LGPL-2.1-or-later) et dav1d 1.5.4 (BSD). Notices intégrales : [FFmpeg](LICENCES/FFmpeg-LGPL-2.1.txt), [SDL](LICENCES/SDL-zlib.txt), [libusb](LICENCES/libusb-LGPL-2.1.txt), [dav1d](LICENCES/dav1d-BSD.txt), [zlib](LICENCES/zlib.txt) (compression utilisée par FFmpeg, [notice officielle](https://zlib.net/zlib_license.html)).

Leurs archives sources correspondantes, incluant les scripts de compilation du dépôt scrcpy, accompagnent le code source de la même release. Les empreintes et origines sont épinglées dans `tools/setup_mirroring.py` et conservées dans `runtime/mirror/origins.json`. Les bibliothèques LGPL sont chargées dynamiquement et peuvent être remplacées par des versions compatibles. Aucun exécutable ADB provenant du ZIP scrcpy n’est redistribué.
