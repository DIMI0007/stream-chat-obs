# Stream Chat OBS

Early native OBS Studio dock for Windows x64. The current build displays a chat UI with disabled controls; Twitch and YouTube authentication, chat reading, and sending are still being developed. Do not install this build expecting working chat.

The plugin is based on the official OBS plugin template. It targets OBS Studio 33.0.0 and its matching Qt 6.11.1 dependencies. GitHub Actions builds the Windows DLL, so the streamer does not need development tools on their PC. The installed plugin itself is local and has no backend.

## Build

Run the **Build Stream Chat OBS (Windows)** workflow in GitHub Actions. A successful run uploads `StreamChatOBS-OBS33-Windows-x64`. The source tree contains no OAuth tokens or client secret.

## Current state

- OBS dock scaffold: implemented, pending CI build verification.
- Twitch app registration and Device Code API: verified separately.
- Google OAuth desktop client: configured for one test user.
- Platform adapters and live chat: not implemented yet.

The `cmake/` modules and dependency pinning follow the official OBS plugin template and the already working Stream Notify OBS 33 build.
