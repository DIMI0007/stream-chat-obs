# Stream Chat OBS

Native OBS Studio dock for Windows x64. Version 0.2 implements a first Twitch adapter: browser/device-code sign-in for the user's own channel, EventSub chat messages and moderation events, and Send Chat Message API delivery. The adapter still needs Windows CI and real-account validation. YouTube is not implemented in this revision.

The plugin is based on the official OBS plugin template. It targets OBS Studio 33.0.0 and its matching Qt 6.11.1 dependencies. GitHub Actions builds the Windows DLL, so the streamer does not need development tools on their PC. The installed plugin itself is local and has no backend. Tokens are encrypted with Windows DPAPI in the OBS plugin configuration directory. Message history stays in memory and is limited to 500 entries; avatars are not downloaded. Hidden docks stop network sessions and timers.

## Build

Run the **Build Stream Chat OBS (Windows)** workflow in GitHub Actions. A successful run uploads `StreamChatOBS-OBS33-Windows-x64`. The source tree contains no OAuth tokens or client secret.

## Current state

- OBS dock scaffold: built successfully before the adapter changes.
- Twitch public client registration: verified in the developer console. The plugin embeds only the public Client ID, not an application secret.
- Twitch adapter: implemented, CI and live-account verification pending. It validates tokens at startup and hourly while active, rotates refresh tokens, reconnects EventSub sessions, bounds event deduplication, and does not automatically retry message sends.
- Google OAuth desktop client: configured for one test user.
- YouTube adapter: next implementation step. It must use streaming chat rather than frequent API polling.

## Installation and alpha limitations

Close OBS and copy the complete `stream-chat-obs` directory from the artifact into `C:\ProgramData\obs-studio\plugins\`. Do not copy only the main DLL: the package also includes the matching WebSockets and TLS runtime libraries. The dock appears under View/Docks (OBS language dependent).

These artifacts are unsigned alpha builds. A successful CI build is not proof that Windows Smart App Control will allow them, or that live chat has been tested. No release is represented as signed until every executable and DLL has a valid signature.

Sign-in and message sending are explicit user actions. Credentials and message contents must not be logged or sent to CI. The privacy-sensitive `user:read:chat` and `user:write:chat` scopes apply to the account the user authorizes. Disconnect clears the locally saved token; users can also revoke app access in Twitch settings.

The `cmake/` modules and dependency pinning follow the official OBS plugin template and the already working Stream Notify OBS 33 build.

