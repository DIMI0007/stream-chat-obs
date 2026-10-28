# Установка Stream Chat 0.3.15 для OBS 33

1. Полностью закройте OBS Studio.
2. В `C:\ProgramData\obs-studio\plugins\` удалите старую папку `stream-chat-obs`. Настройки аккаунтов хранятся отдельно в профиле пользователя OBS.
3. Из установочного ZIP скопируйте туда всю новую папку `stream-chat-obs`.
4. Проверьте структуру: `stream-chat-obs\stream-chat-obs.dll`, `stream-chat-obs\Qt6WebSockets.dll`, `stream-chat-obs\data\...`. Папки `bin\64bit` в новом пакете нет.
5. Запустите OBS и откройте док-панель Stream Chat.

Сборка рассчитана на Windows x64 и OBS 33. Если Windows Application Control снова блокирует загрузку, откройте новый журнал в `%APPDATA%\obs-studio\logs` и проверьте события CodeIntegrity. Не отключайте защиту Windows ради установки плагина; пришлите строки ошибки для диагностики.
