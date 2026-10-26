# Stream Chat OBS

**Twitch and YouTube chat in one OBS dock.** The plugin runs locally without a project-operated server. [Русский](../README.md).

![Stream Chat English interface — isolated preview](screenshots/chat-en.png)

*Version 0.3.9 UI preview from an isolated test host. It shows the interface, not a connection to live accounts.* [Russian interface](screenshots/chat-ru.png).

## Verified status

- The plugin loads in OBS 33 on Windows x64. Twitch sign-in and real incoming messages were checked in OBS.
- Google browser sign-in was confirmed. YouTube chat reads and sends during a real broadcast still need validation.
- Version 0.3.9 builds and passes automated tests. Language selection, viewer counts, and automatic stream discovery are implemented and await live broadcast validation.

## Features

- A combined chat with compact platform markers and local message time when the service supplies it. A down button counts new messages while reading older ones.
- Browser sign-in and Windows-protected local account storage.
- Automatic YouTube chat discovery after streaming starts.
- In-stream message and viewer counts. Twitch viewers refresh about every 30 seconds; YouTube about every minute when the API supplies a count.
- Send to selected connected channels. Both platforms are selected by default.
- The ⚙ button opens **Auto / Russian / English** language choices and a text size control with a live preview.

## Install

1. Close OBS.
2. [Download the artifact from the latest successful build](https://github.com/DIMI0007/stream-chat-obs/actions/workflows/build-windows.yml).
3. Copy the entire `stream-chat-obs` directory to `C:\ProgramData\obs-studio\plugins\`. Replace the entire directory when updating.
4. Start OBS and open the **Stream Chat** dock.

Tested target: **OBS 33.0.0 on Windows x64**. The archive is currently unsigned. `BUILD_INFO.json` contains the version and source revision.

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
