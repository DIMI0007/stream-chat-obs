# Stream Chat OBS

**Twitch and YouTube chats in one OBS dock.** The plugin runs on your computer without a project-operated server. [Русский](../README.md).

![Stream Chat English interface](screenshots/chat-en.png)

*Version 0.3.15 UI preview from an isolated test host; messages and connections are sample data.* [Russian interface](screenshots/chat-ru.png) · [Narrow dock](screenshots/chat-narrow-ru.png) · [Settings](screenshots/settings-en.png).

## Features

- Read Twitch and YouTube together. Each message has a compact platform marker, subtle color accent, and local time when the service supplies it.
- Scrolling up holds your place. A down button counts new messages and takes you back to the latest ones.
- Send text to Twitch, YouTube, or both connected chats. Both destinations are selected by default.
- YouTube chat discovery starts after the broadcast begins. Viewer counts appear during a stream when the service provides them.
- If the YouTube streaming connection stops delivering data, the plugin switches to the official chat polling API at a limited rate.
- Sign in through a browser without copying tokens. Account data stays on this PC and is protected by Windows.
- Settings include **Auto / Russian / English** and text size with a live preview. Message history is cleared when OBS closes.

## Verified status

- OBS 33 on Windows x64 loads the plugin. Twitch sign-in, real messages, and viewer count were checked during a stream on version 0.3.8.
- Google browser sign-in was confirmed. The YouTube polling fallback still needs validation during a real broadcast.
- Version 0.3.15 is built and UI-tested in CI; live YouTube chat still needs confirmation.

## Install

1. Close OBS and [download the latest successful build](https://github.com/DIMI0007/stream-chat-obs/actions/workflows/build-windows.yml).
2. Copy the entire `stream-chat-obs` folder into `C:\ProgramData\obs-studio\plugins\`. Replace the whole folder when updating.
3. Start OBS, open the **Stream Chat** dock, and sign in to the services you use.

Target: **OBS 33, Windows x64**. `BUILD_INFO.json` in the archive identifies the version and source revision. The build is currently unsigned.

## Support development

[DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [DonatePay](https://donatepay.ru/don/OTMOPOZOK)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
