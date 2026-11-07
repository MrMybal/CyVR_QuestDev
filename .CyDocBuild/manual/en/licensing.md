---
id: licensing
title: License and third-party components
description: Understand the application's GPL terms and the separate licenses of its components.
section: Support
order: 100
revision: 1
---

## Application license {#license}

CyVR QuestDev is **GNU GPL version 3 only (`GPL-3.0-only`)**, copyright 2026 Cyberalien. Its original application code and documentation follow those terms. The corresponding Source archive accompanies each application release. See the complete [GPL license](https://github.com/MrMybal/CyVR_QuestDev/blob/main/LICENSE) and [copyright scope](https://github.com/MrMybal/CyVR_QuestDev/blob/main/COPYRIGHT); their texts are also included with the installed application.

The app is supplied without warranty under the license. Alpha status describes its development stage and does not change these terms.

## Components keep their own terms {#components}

CyUI, Dear ImGui, CyToolsCore, nlohmann/json and the selected text-editor components carry their own MIT or permissive notices. CPython and its dependencies keep their respective licenses. The bundled scrcpy engine is Apache-2.0; its replaceable FFmpeg/libusb shared libraries retain LGPL terms and corresponding-source requirements. Other mirroring components retain their own notices. ADB and Quest-side OVR/CyInsightOVR applications are external.

This manual uses the **CyDoc reader**, under MIT. Only its image module is enabled for the application captures. CyDoc's MIT license does not change QuestDev's GPL license. Reader component notices are supplied alongside its generated files.

Keep the complete notices in the application distribution, including `THIRD_PARTY.md`, `LICENCES/` and `licenses/`. The [third-party inventory](https://github.com/MrMybal/CyVR_QuestDev/blob/main/THIRD_PARTY.md) identifies components and source links; a short list here does not replace those texts.
