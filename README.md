# Flatch

[Русский](#русский) · [English](#english)

<img width="1127" height="838" alt="Flatch" src="https://github.com/user-attachments/assets/3254d560-fa88-486c-8d14-5d9a7493c5c0" />

## Русский

**Flatch** — менеджер сниппетов для Windows и Linux, который живёт в системном трее. Хранит часто используемые тексты, форматированный текст, файлы и команды запуска приложений в локальной базе SQLite и по щелчку копирует их в буфер обмена или запускает нужную программу.

### Что делает

- Держит категории и сниппеты под рукой — в меню иконки в трее.
- Копирует в буфер обмена:
  - обычный текст;
  - форматированный текст (HTML с текстовым запасным вариантом);
  - файлы как файловые объекты.
- Запускает приложения одним щелчком.
- Позволяет включать шифрование для сниппетов с логинами и паролями.
- Умеет запускаться при входе в систему.
- Работает полностью локально: без сети, синхронизации и телеметрии.

### Кому полезно

- Тем, кто часто вставляет одни и те же тексты: шаблоны писем, ответы поддержки, реквизиты, команды, фрагменты кода.
- Тем, кто хранит под рукой набор файлов или ссылок для быстрой вставки.
- Тем, кто хочет запускать нужные программы из трея без поиска по меню.
- Тем, кому важно, чтобы данные оставались на компьютере.

### Платформы

- Windows 10/11
- Linux (нужен рабочий системный трей; в GNOME может понадобиться расширение)

### Сборка

Проект собирается через **qmake** с **Qt 6.11.1** (проверено с MinGW 13.1.0 на Windows).

Для Linux нужны модули Qt: **Core, Gui, Widgets, Sql, Network** и драйвер **SQLite** (обычно пакет `libqt6sql6-sqlite` в Debian/Ubuntu).

### Автор

**BoDun**

- GitHub: <https://github.com/bodun56>
- Дзен: <https://dzen.ru/room_bodun>
- Boosty: <https://boosty.to/room_bodun>
- Поддержать: [Дзен](https://dzen.ru/room_bodun?donate=true) · [Boosty](https://boosty.to/room_bodun/donate)

## English

**Flatch** is a snippet manager for Windows and Linux that lives in the system tray. It keeps frequently used texts, rich text, files and application launch commands in a local SQLite database and copies them to the clipboard or launches the required program with a single click.

### What it does

- Keeps categories and snippets at hand — in the tray icon menu.
- Copies to the clipboard:
  - plain text;
  - rich text (HTML with a plain-text fallback);
  - files as file objects.
- Launches applications with a single click.
- Lets you enable encryption for snippets with logins and passwords.
- Can start automatically when you sign in.
- Works fully locally: no network, sync or telemetry.

### Who it's for

- Those who often paste the same texts: email templates, support replies, bank details, commands, code fragments.
- Those who keep a set of files or links at hand for quick pasting.
- Those who want to launch the needed programs from the tray without searching through menus.
- Those who care that their data stays on their computer.

### Platforms

- Windows 10/11
- Linux (a working system tray is required; GNOME may need an extension)

### Building

The project is built with **qmake** and **Qt 6.11.1** (tested with MinGW 13.1.0 on Windows).

For Linux you need the Qt modules **Core, Gui, Widgets, Sql, Network** and the **SQLite** driver (usually the `libqt6sql6-sqlite` package on Debian/Ubuntu).

### Author

**BoDun**

- GitHub: <https://github.com/bodun56>
- Dzen: <https://dzen.ru/room_bodun>
- Boosty: <https://boosty.to/room_bodun>
- Support: [Dzen](https://dzen.ru/room_bodun?donate=true) · [Boosty](https://boosty.to/room_bodun/donate)
