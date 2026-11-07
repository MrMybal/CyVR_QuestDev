---
id: mirroring
title: Mirroring and recording
description: Display the Quest on your PC, configure audio and save an MKV session.
section: Tools
parent: device
order: 31
revision: 1
---

## Start the video window {#start}

Connect and authorize the Quest, then open **Device → Mirroring vidéo / audio → Démarrer le mirroring…**. Select the maximum resolution, frame rate, bitrate and audio mode, then start the broadcast. A dedicated native video window opens. **See video / Voir la vidéo** brings it forward; **F11** toggles full screen. The window can be resized.

Initial settings are **1920 px, 60 FPS and 12 Mbit/s**, with H.264 and automatic hardware decoding. These are requested limits, not a guarantee of the received resolution or cadence. QuestDev reports the actual dimensions and frame rate. Reduce resolution or bitrate if the connection is saturated. A requested 120 FPS is not guaranteed.

```image
{"id":"mirror-settings","src":"assets/mirroring-settings.png","alt":"Video, audio, cropping and recording options in the mirroring settings","controls":"minimal"}
```

The image shows demonstration settings, not a live headset feed. The bundled engine is scrcpy 5.0.1. Mirroring uses the selected ADB connection, including Wi-Fi when already connected. Starting it does not change network settings, launch a VR app or modify sleep settings. Remote input control and clipboard synchronization are disabled.

## Audio and cropping {#audio}

Choose no audio, duplicate audio to the PC while keeping headset playback, or PC-only audio output. Duplication needs Android 13+ and an application that permits audio capture. Rejected audio appears in the session log; silence may also come from the application. No microphone is captured.

Custom cropping uses width, height and X/Y offsets in headset-screen coordinates. The correct rectangle for one eye depends on the headset model and firmware. A rejected rectangle appears in the engine log. Use the actual received image to check the result.

## Record an MKV {#record}

Before starting, choose an **MKV** file in the settings. Video and enabled audio are recorded while displayed. Replacing an existing file asks for confirmation in the Windows file selector. QuestDev clears this choice on the next start to avoid overwriting the same file accidentally.

**Stop and finalize**, closing the video window, or closing QuestDev normally finalizes the MKV. Changing or disconnecting the headset also stops the session. A forced engine stop can leave an incomplete recording and is reported. The bounded session log can be copied in **Settings / log**; that panel also provides the last file.

MKV recording is independent of [Android MP4 capture](doc:device#actions) and has no 180-second Android recording limit. Available disk space still limits the session. See [troubleshooting](doc:troubleshooting#media) for missing video or sound, and [automation](doc:automation#operations) for `mirror_start`/`mirror_stop`.
