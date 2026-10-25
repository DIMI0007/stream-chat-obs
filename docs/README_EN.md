<div align="center">
<h1>Stream Chat OBS</h1>
<p><strong>Twitch and YouTube chat in one lightweight OBS dock.</strong></p>
<p><a href="../README.md">Русский</a> · <strong>English</strong></p>
<p><img alt="OBS 33.0.0" src="https://img.shields.io/badge/OBS-33.0.0-302E31"> <img alt="Windows x64" src="https://img.shields.io/badge/Windows-x64-0078D4"> <img alt="Native Qt" src="https://img.shields.io/badge/UI-native_Qt-00B88C"></p>
</div>

A native OBS dock for reading chat and sending messages to your own channels. It runs locally without a project-operated backend. Version **0.2.0** introduces the first Twitch adapter; the YouTube adapter is still being developed.

[Windows test builds](https://github.com/DIMI0007/stream-chat-obs/actions/workflows/build-windows.yml)

## Current status

| Area | Status |
| --- | --- |
| OBS dock | The initial scaffold built successfully in Windows CI. |
| Twitch registration | Public client registration verified in the developer console. |
| Twitch sign-in and chat | Device-code sign-in, EventSub messages/moderation and explicit message sending implemented. Build verification is underway; a live account has not been validated. |
| YouTube | Desktop OAuth client registered. Chat adapter not connected yet. |
| Token storage | Windows DPAPI implemented with a dedicated CI test. |
| Code signing | Artifacts are unsigned. Windows Smart App Control acceptance is not guaranteed. |

This is an **alpha build**, not a completed public release. Passing CI does not prove real chat connectivity.

## Lightweight operation

The dock uses native Qt widgets, with no embedded browser UI. History is limited to 500 messages and remains in memory. Small batches update the view; remote avatars and images are not downloaded. Hidden docks stop network connections and timers.

Twitch uses EventSub WebSockets. Message sends with uncertain outcomes are never automatically repeated. Accounts are stored locally with Windows DPAPI protection, and user tokens are never used in CI.

## Install or update

Test configuration: **Windows x64, OBS 33.0.0, Qt 6.11.1**.

1. Close OBS completely.
2. Download the artifact from a successful workflow run and extract it.
3. Copy the complete `stream-chat-obs` directory to `C:\ProgramData\obs-studio\plugins\`.
4. Replace the whole directory when updating; the matching WebSockets and TLS files are required.
5. Start OBS and open the **Stream Chat** dock.

The dock shows its version; `BUILD_INFO.json` identifies the source revision. To uninstall, close OBS and remove only the installed plugin directory. Local settings are stored separately.

## Twitch

Click the Twitch sign-in button. The plugin opens the official Twitch website and displays a device code. Review the scopes before authorizing. The adapter uses the signed-in user's own channel.

The requested scopes are `user:read:chat` and `user:write:chat`. Only the public Client ID is embedded; no application secret is required. Tokens are validated on connection and hourly while active. Disconnect clears the locally saved token; users can also revoke access in Twitch settings.

Check the selected destination before sending. After an uncertain send result, retry only deliberately to avoid duplicate messages.

## YouTube

The Google Desktop OAuth client exists, but sign-in and chat are not connected in this revision. The app remains in Testing. A public release requires OAuth verification and management of the shared YouTube API quota.

## Diagnostics

Include the version, source commit from `BUILD_INFO.json` and the safe error message. Never publish access/refresh tokens, OAuth codes or account files. GitHub Actions handles compilation; streamers do not need development tools installed.

[Documentation standard (Russian)](DOCUMENTATION_STANDARD_RU.md)

Developed by [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
