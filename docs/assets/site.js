const boards = {
  esp01_512k: { name: "ESP-01 · 512 КБ", manifest: "manifests/esp01_512k.json", note: "Для ESP-01 із підтвердженими 512 КБ flash. Та сама локальна панель налаштовує три Wi-Fi мережі, показує статус і журнал; OTA недоступне, оновлення встановлюються через USB–UART. Після першого переходу зі старої розмітки 32 КБ налаштуйте плату заново.", warning: "Не орієнтуйтеся лише на колір плати. Обирайте цей файл, тільки якщо 512 КБ підтверджено специфікацією або командою esptool flash-id." },
  esp01_1m: { name: "ESP-01 / ESP-01S · 1 МБ", manifest: "manifests/esp01_1m.json", note: "Для ESP-01 або ESP-01S із підтвердженим 1 МБ flash. Єдина локальна панель налаштовує три Wi-Fi мережі та приймає OTA-оновлення." },
  d1_mini: { name: "D1 mini", manifest: "manifests/d1_mini.json", note: "Для плат із написом WEMOS або LOLIN D1 mini." },
  nodemcuv2: { name: "NodeMCU v2", manifest: "manifests/nodemcuv2.json", note: "Для довгих ESP8266-плат із маркуванням NodeMCU DEVKIT 1.0." },
  sonoff_basic_r2: { name: "Sonoff Basic R2", manifest: "manifests/sonoff_basic_r2.json", note: "Окрема збірка для Sonoff Basic R2 на ESP8285 із 1 МБ flash. Реле прошивка не використовує; локальна панель така сама, як на інших платах.", warning: "НЕБЕЗПЕКА 230 В: перед відкриттям корпусу від’єднайте Sonoff від мережі та навантаження. Прошивайте тільки від USB–UART 3,3 В. Ніколи не підключайте USB–UART і мережеву напругу одночасно." },
  sonoff_basic_r3: { name: "Sonoff Basic R3", manifest: "manifests/sonoff_basic_r3.json", note: "Окрема збірка для Sonoff Basic R3 на ESP8285 із 1 МБ flash. Реле прошивка не використовує; локальна панель така сама, як на інших платах.", warning: "НЕБЕЗПЕКА 230 В: перед відкриттям корпусу від’єднайте Sonoff від мережі та навантаження. Прошивайте тільки від USB–UART 3,3 В. Ніколи не підключайте USB–UART і мережеву напругу одночасно." },
  esp32dev: { name: "ESP32 DevKit", manifest: "manifests/esp32dev.json", note: "Для класичних плат із модулем ESP32-WROOM-32 та 4 МБ flash. Не підходить для ESP32-C3 або ESP32-S3." },
  "esp32-c3": { name: "ESP32-C3 DevKitM-1", manifest: "manifests/esp32-c3.json", note: "Для Espressif ESP32-C3-DevKitM-1 із 4 МБ flash. Інші C3-плати можуть мати іншу USB-схему й потребувати ручного входу в режим прошивки.", warning: "«Боже, памагі!» Якщо серйозно: C3-плати бувають різними. Коли браузер не бачить порт, затисніть BOOT, коротко натисніть RESET, відпустіть BOOT і спробуйте ще раз. GPIO2, GPIO8 та GPIO9 не навантажуйте під час запуску — вони впливають на режим завантаження." },
  "esp32-s3": { name: "ESP32-S3 · 4+ МБ", manifest: "manifests/esp32-s3.json", note: "4-МБ сумісна збірка для ESP32-S3-DevKitC-1 та сумісних плат. На платах із 8 МБ вона використовує перші 4 МБ." }
};

const options = [...document.querySelectorAll(".board-option")];
const button = document.querySelector("#install-button");
const name = document.querySelector("#selected-name");
const note = document.querySelector("#selected-note");
const warning = document.querySelector("#board-warning");

function selectBoard(id) {
  const board = boards[id];
  if (!board) return;
  options.forEach(option => {
    const selected = option.dataset.board === id;
    option.classList.toggle("selected", selected);
    option.setAttribute("aria-checked", String(selected));
  });
  name.textContent = board.name;
  note.textContent = board.note;
  button.manifest = board.manifest;
  warning.textContent = board.warning || "";
  warning.classList.toggle("hidden", !board.warning);
  localStorage.setItem("svb-board", id);
}

options.forEach(option => option.addEventListener("click", () => selectBoard(option.dataset.board)));
selectBoard(boards[localStorage.getItem("svb-board")] ? localStorage.getItem("svb-board") : "d1_mini");
