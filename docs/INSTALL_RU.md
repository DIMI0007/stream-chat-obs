# Установка Stream Chat для OBS 33

1. Полностью закройте OBS Studio.
2. В `C:\ProgramData\obs-studio\plugins\` удалите старую папку `stream-chat-obs`. Настройки аккаунтов хранятся отдельно в профиле пользователя OBS.
3. Из установочного ZIP скопируйте туда всю новую папку `stream-chat-obs`.
4. Проверьте структуру: `stream-chat-obs\stream-chat-obs.dll`, `stream-chat-obs\Qt6WebSockets.dll`, `stream-chat-obs\data\...`. Папки `bin\64bit` в новом пакете нет.
5. Запустите OBS и откройте док-панель Stream Chat.
6. Для версии 0.4.0 создайте личные приложения Twitch, YouTube и Kick по [инструкции](REGISTER_APPS_RU.md). Введите ключи в шестерёнке, сохраните и выполните вход. Kick включается отдельно; если он не нужен, оставьте его выключенным.

Сборка рассчитана на Windows x64 и OBS 33. Если Windows Application Control снова блокирует загрузку, откройте новый журнал в `%APPDATA%\obs-studio\logs` и проверьте события CodeIntegrity. Не отключайте защиту Windows ради установки плагина; пришлите строки ошибки для диагностики.
