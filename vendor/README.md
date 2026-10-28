# Qt6WebSockets runtime for Windows packaging

This 64-bit Qt 6.11.1 DLL is the exact copy distributed in the verified Stream Notify 1.0.10 OBS 33 build (commit `64e2344`, CI run `40648534796`). SHA-256: `A158B90C70D8B6DD3C651028A7C3D93BD751440598F551CD763FED78E31D8008`.

The Stream Chat CI build still compiles and tests against the matching Qt WebSockets source. The package uses this same-version ABI build because Windows Application Control blocked a separately built runtime on the user's machine, while the Stream Notify runtime loaded successfully. Qt is licensed under LGPLv3; source code: https://code.qt.io/cgit/qt/qtwebsockets.git/tag/?h=v6.11.1
