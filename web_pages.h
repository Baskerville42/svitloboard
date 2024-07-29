#ifndef WEB_PAGES_H
#define WEB_PAGES_H

const char setupPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
%HEAD_PARTITION%
<body>
    <div class="container">
        <h1>Тебе вітає SvitloBoard</h1>
        <h2>Швидке налаштування пристрою</h2>
        <div class="disclaimer">
            Якщо дані мережі були введені неправильно, може знадобитися скидання параметрів пристрою.
        </div>
        <form action="/setup" method="POST">
            <label for="channel-key">Унікальний код каналу</label>
            <input type="text" id="channel-key" name="channel_key" required>
            <label for="main-ssid">Основна WiFi мережа (SSID)</label>
            <input type="text" id="main-ssid" name="ssid" required>
            <label for="main-password">Пароль</label>
            <input type="password" id="main-password" name="password" autocomplete="off" required>
            <button type="submit">Зберегти</button>
        </form>
        %FOOTER_PARTITION%
    </div>
</body>
</html>
)rawliteral";

const char channelKeyPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
%HEAD_PARTITION%
<body>
    <div class="menu">
        <a href="/channel_key" class="active">Налаштування Channel Key</a>
        <a href="/wifi">Налаштування підключення</a>
        <a href="/status">Перевірка статусу</a>
    </div>
    <div class="container">
        <h2>Налаштування Channel Key</h2>
        <form method="POST" action="/channel_key">
            <label for="channel-key">Унікальний код каналу</label>
            <input type="text" id="channel-key" name="channel_key" value="%CHANNEL_KEY%" required>
            <button type="submit">Зберегти</button>
        </form>
        %FOOTER_PARTITION%
    </div>
</body>
</html>
)rawliteral";

const char wifiPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
%HEAD_PARTITION%
<body>
    <div class="menu">
        <a href="/channel_key">Налаштування Channel Key</a>
        <a href="/wifi" class="active">Налаштування підключення</a>
        <a href="/status">Перевірка статусу</a>
    </div>
    <div class="container">
        <h2>Налаштування Wi-Fi</h2>
        <div class="disclaimer">
            Якщо дані мережі були введені неправильно, може знадобитися скидання параметрів пристрою.
        </div>
        <form method="POST" action="/wifi">
            <label for="ssid">Основна WiFi мережа (SSID)</label>
            <input type="text" id="ssid" name="ssid" value="%SSID%" required>
            <label for="password">Пароль</label>
            <input type="password" id="password" name="password" value="%PASSWORD%" autocomplete="off" required>
            <button type="submit">Зберегти</button>
        </form>
        <form method="POST" action="/wifi">
            <label for="backup1-ssid">Резервна WiFi мережа #1 (SSID)</label>
            <input type="text" id="backup1-ssid" name="backup1_ssid" value="%BACKUP_1_SSID%" required>
            <label for="backup1-password">Пароль</label>
            <input type="password" id="backup1-password" name="backup1_password" value="%BACKUP_1_PASSWORD%" autocomplete="off" required>
            <button type="submit">Зберегти</button>
        </form>
        <form method="POST" action="/wifi">
            <label for="backup2-ssid">Резервна WiFi мережа #2 (SSID)</label>
            <input type="text" id="backup2-ssid" name="backup2_ssid" value="%BACKUP_2_SSID%" required>
            <label for="backup2-password">Пароль</label>
            <input type="password" id="backup2-password" name="backup2_password" value="%BACKUP_2_PASSWORD%" autocomplete="off" required>
            <button type="submit">Зберегти</button>
        </form>
        %FOOTER_PARTITION%
    </div>
</body>
</html>
)rawliteral";

const char statusPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
%HEAD_PARTITION%
<body>
    <div class="menu">
        <a href="/channel_key">Налаштування Channel Key</a>
        <a href="/wifi">Налаштування підключення</a>
        <a href="/status" class="active">Перевірка статусу</a>
    </div>
    <div class="container">
        <h2>Перевірка статусу</h2>
        <div class="disclaimer">
            Якщо Channel Key правильний, нижче ви побачите інформацію від СвітлоБот.
        </div>
        <iframe src="https://api.svitlobot.in.ua/status?channel_key=%CHANNEL_KEY%" frameborder="0" scrolling="no"></iframe>
        <div class="network-info">
            Підключено до мережі: <span>%SSID%</span><br>
            IP адреса: <span>%IP_ADDRESS%</span>
        </div>
        <form action="/factory-reset" method="post">
            <button type="submit">Скинути усі налаштування</button>
        </form>
        %FOOTER_PARTITION%
    </div>
</body>
</html>
)rawliteral";

const char messagePage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
%HEAD_PARTITION%
<body>
    <div class="menu">
        <a href="/channel_key">Налаштування Channel Key</a>
        <a href="/wifi">Налаштування підключення</a>
        <a href="/status">Перевірка статусу</a>
    </div>
    <div class="container">
        <h2>Повідомлення</h2>
        <div class="disclaimer">
            %MESSAGE_TEXT%
        </div>
        %FOOTER_PARTITION%
    </div>
</body>
</html>
)rawliteral";

const char headPartition[] PROGMEM = R"rawliteral(
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Налаштування Svitloboard</title>
  <style>
    body {
        font-family: Arial, sans-serif;
        margin: 0;
        padding: 0;
        display: flex;
        flex-direction: column;
        align-items: center;
        min-height: 100vh;
        background-color: #f0f8ff;
        color: #333;
    }
    .menu {
        display: flex;
        justify-content: center;
        background-color: rgba(255, 255, 255, 0.8);
        border: 2px solid #ffeb3b;
        border-radius: 10px;
        margin-top: 20px;
        padding: 10px;
        width: auto;
        flex-wrap: wrap;
    }
    .menu a {
        padding: 15px 20px;
        text-decoration: none;
        color: #333;
        border-right: 1px solid #ffeb3b;
        transition: background-color 0.3s;
    }
    .menu a:last-child {
        border-right: none;
    }
    .menu a.active, .menu a:hover {
        background-color: #ffeb3b;
        color: #333;
    }
    @media (max-width: 700px) {
        .menu {
            flex-direction: column;
            align-items: stretch;
        }
        .menu a {
            border-right: none;
            border-bottom: 1px solid #ffeb3b;
        }
        .menu a:last-child {
            border-bottom: none;
        }
    }
    .container {
        text-align: center;
        padding: 20px;
        background-color: #fff;
        border: 2px solid #ffeb3b;
        border-radius: 10px;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        margin-top: 20px;
        width: 80%;
        max-width: 600px;
        margin-bottom: 20px;
    }
    h2 {
        color: #00bcd4;
        margin-bottom: 10px;
    }
    .disclaimer {
        margin-bottom: 20px;
        font-size: 14px;
        color: #666;
    }
    iframe {
        width: 100%;
        min-height: 440px;
        border: none;
        border-radius: 5px;
        background-color: #4c95d9;
        box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
        color: #eee;
    }
    .network-info {
        margin-top: 20px;
        margin-bottom: 20px;
        font-size: 16px;
        color: #666;
    }
    .network-info span {
        font-weight: bold;
        color: #333;
    }
    form {
        display: flex;
        flex-direction: column;
        align-items: center;
        margin-bottom: 20px;
        border-top: 2px solid #ffeb3b;
        padding-top: 20px;
    }
    form:first-of-type {
        border-top: none;
        padding-top: 0;
    }
    label {
        margin: 10px 0 5px;
    }
    input {
        padding: 10px;
        margin-bottom: 15px;
        border: 1px solid #ccc;
        border-radius: 5px;
        width: 80%;
        max-width: 300px;
    }
    button {
        padding: 10px 20px;
        background-color: #ffeb3b;
        border: none;
        border-radius: 5px;
        color: #333;
        font-size: 16px;
        cursor: pointer;
        transition: background-color 0.3s;
    }
    button:hover {
        background-color: #fdd835;
    }
    button {
        padding: 10px 20px;
        background-color: #ffeb3b;
        border: none;
        border-radius: 5px;
        color: #333;
        font-size: 16px;
        cursor: pointer;
        transition: background-color 0.3s;
    }
    button:hover {
        background-color: #fdd835;
    }
    .footer {
        margin-top: 20px;
        font-size: 12px;
        color: #aaa;
    }
    .footer a {
        color: #00bcd4;
        text-decoration: none;
    }
    .footer a:hover {
        text-decoration: underline;
    }
    @media (max-width: 700px) {
        input {
            width: 100%;
        }
    }
  </style>
</head>
)rawliteral";

const char footerPartition[] PROGMEM = R"rawliteral(
<div class="footer">
    &copy; 2024 Alexander Tartmin. Версія 1.0. <a href="https://github.com/baskerville42/svitloboard" target="_blank">GitHub</a>
</div>
)rawliteral";

#endif  // WEB_PAGES_H
