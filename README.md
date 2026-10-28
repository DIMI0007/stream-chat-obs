# Stream Chat

**Чаты Twitch и YouTube в одной панели OBS.** Сообщения легко отличать по значку сервиса. Можно отвечать сразу в оба чата или выбрать один.

![Stream Chat](docs/screenshots/chat-ru.png)

[Узкая панель](docs/screenshots/chat-narrow-ru.png) · [Настройки](docs/screenshots/settings-ru.png) · [English](docs/README_EN.md)

## Что удобно

- Читайте оба чата, не выходя из OBS.
- Выбирайте язык и размер **сообщений чата** в настройках.
- Входите в Twitch и YouTube через браузер. Данные входа остаются на вашем компьютере.
- Stream Stats может показать, сколько данных Chat отправил и получил за время работы OBS.

## Как установить

1. Полностью закройте OBS. Скачайте **StreamChatOBS-OBS33-Windows-x64** на странице [последней успешной сборки](https://github.com/DIMI0007/stream-chat-obs/actions/workflows/build-windows.yml) и распакуйте ZIP.
2. Откройте `C:\ProgramData\obs-studio\plugins\`. Если там есть старая папка `stream-chat-obs`, удалите её. Скопируйте сюда папку `stream-chat-obs` из ZIP целиком.
3. Запустите OBS. Откройте **Док-панели → Stream Chat** и войдите в нужные сервисы.

Внутри папки `stream-chat-obs` должны лежать файлы `stream-chat-obs.dll` и `Qt6WebSockets.dll`, а также папка `data`. Настройки входа хранятся отдельно и при замене папки не удаляются. [Подробная инструкция](docs/INSTALL_RU.md).

Для **OBS Studio 33 на Windows x64**. Сборка пока не подписана. YouTube чат и автоматическое восстановление связи ещё требуют проверки во время реального эфира.

## Ещё

[Общий стиль трёх плагинов](docs/VISUAL_STYLE_RU.md) · [Лицензия GPL v2 или новее](LICENSE_NOTICE.md)

Разработчик: [OTMOPOZOK](https://www.youtube.com/channel/UCTAKYR3T2Ud8zZgNShgQfyw/).
## Поддержать проект

Если плагины полезны вам, поддержите проект донатом. Ваша помощь поддерживает развитие плагинов, исправления и будущую разработку.

[Поддержать через DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [Поддержать через DonatePay](https://donatepay.ru/don/OTMOPOZOK)
