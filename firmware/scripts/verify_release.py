#!/usr/bin/env python3
"""Verify that firmware targets, installer manifests, and CI stay in sync."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

TARGETS = {
    "esp01_512k": ("ESP8266", False, 463_000),
    "esp01_1m": ("ESP8266", False, 479_232),
    "d1_mini": ("ESP8266", False, 600_000),
    "nodemcuv2": ("ESP8266", False, 600_000),
    "sonoff_basic_r2": ("ESP8266", False, 479_232),
    "sonoff_basic_r3": ("ESP8266", False, 479_232),
    "esp32dev": ("ESP32", True, 1_400_000),
    "esp32-c3": ("ESP32-C3", True, 1_400_000),
    "esp32-s3": ("ESP32-S3", True, 1_400_000),
}

OTA_MARKERS = {
    "esp01_1m": "SVB2-OTA:ESP-01S-1M",
    "d1_mini": "SVB2-OTA:D1-mini",
    "nodemcuv2": "SVB2-OTA:NodeMCU-v2",
    "sonoff_basic_r2": "SVB2-OTA:Sonoff-Basic-R2",
    "sonoff_basic_r3": "SVB2-OTA:Sonoff-Basic-R3",
    "esp32dev": "SVB2-OTA:ESP32-DevKit",
    "esp32-c3": "SVB2-OTA:ESP32-C3-DevKitM-1",
    "esp32-s3": "SVB2-OTA:ESP32-S3-DevKitC-1",
}

ESP8266_FLASH_SIZE_CODES = {
    "esp01_512k": 0,
    "esp01_1m": 2,
    "d1_mini": 4,
    "nodemcuv2": 4,
    "sonoff_basic_r2": 2,
    "sonoff_basic_r3": 2,
}

ESP32_CHIP_IDS = {
    "esp32dev": 0,
    "esp32-c3": 5,
    "esp32-s3": 9,
}


class Verification:
    def __init__(self) -> None:
        self.errors: list[str] = []

    def check(self, condition: bool, message: str) -> None:
        if not condition:
            self.errors.append(message)

    def same_set(self, label: str, actual: set[str]) -> None:
        expected = set(TARGETS)
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        self.check(not missing, f"{label}: missing {', '.join(missing)}")
        self.check(not extra, f"{label}: unexpected {', '.join(extra)}")


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def esp8266_crc(data: bytes) -> tuple[int, int, int] | None:
    """Return embedded size, embedded CRC and calculated eboot CRC."""
    if len(data) < 0x1018:
        return None
    mutable = bytearray(data)
    embedded_size, embedded_crc = struct.unpack_from("<II", mutable, 0x1010)
    struct.pack_into("<II", mutable, 0x1010, 0, 0)

    table: list[int] = []
    for value in range(256):
        crc = value << 24
        for _ in range(8):
            crc = ((crc << 1) & 0xFFFFFFFF) ^ (0x04C11DB7 if crc & 0x80000000 else 0)
        table.append(crc)

    calculated = 0xFFFFFFFF
    for value in mutable:
        calculated = ((calculated << 8) & 0xFFFFFFFF) ^ table[
            ((calculated >> 24) ^ value) & 0xFF
        ]
    return embedded_size, embedded_crc, calculated


def verify_esp8266_image(result: Verification, target: str, name: str, data: bytes) -> None:
    result.check(len(data) > 0x1004, f"{name}: truncated ESP8266 combined image")
    if len(data) <= 0x1004:
        return
    result.check(data[0] == 0xE9, f"{name}: missing ESP8266 eboot image header")
    result.check(data[0x1000] == 0xE9, f"{name}: missing ESP8266 application header")
    result.check(data[3] >> 4 == ESP8266_FLASH_SIZE_CODES[target],
                 f"{name}: wrong ESP8266 flash-size header")
    crc = esp8266_crc(data)
    result.check(crc is not None, f"{name}: missing ESP8266 eboot CRC metadata")
    if crc:
        embedded_size, embedded_crc, calculated = crc
        result.check(embedded_size == len(data),
                     f"{name}: embedded ESP8266 image size does not match file size")
        result.check(embedded_crc == calculated,
                     f"{name}: invalid ESP8266 whole-image CRC")


def esp32_chip_id(data: bytes, offset: int = 0) -> int | None:
    if len(data) < offset + 14 or data[offset] != 0xE9:
        return None
    return struct.unpack_from("<H", data, offset + 12)[0]


def verify_esp32_images(result: Verification, target: str, app_name: str, app: bytes,
                        factory_name: str, factory: bytes) -> None:
    expected_chip = ESP32_CHIP_IDS[target]
    result.check(esp32_chip_id(app) == expected_chip,
                 f"{app_name}: wrong or invalid ESP32 chip ID")
    boot_offset = 0x1000 if target == "esp32dev" else 0
    result.check(esp32_chip_id(factory, boot_offset) == expected_chip,
                 f"{factory_name}: wrong or invalid ESP32 bootloader")
    result.check(len(factory) > 0x8002 and factory[0x8000:0x8002] == b"\xAA\x50",
                 f"{factory_name}: partition table is missing at 0x8000")
    result.check(esp32_chip_id(factory, 0x10000) == expected_chip,
                 f"{factory_name}: application is missing or has the wrong chip ID at 0x10000")
    result.check(factory[0x10000:0x10000 + len(app)] == app,
                 f"{factory_name}: embedded application differs from {app_name}")


def verify_targets(result: Verification) -> None:
    result.check(not (ROOT / ".github/workflows/release.yml").exists(),
                 "legacy v1 auto-release workflow must be removed before enabling v2")
    ini = read("firmware/platformio.ini")
    ini_targets = set(re.findall(r"^\[env:([^\]]+)]", ini, re.MULTILINE))
    result.same_set("platformio.ini environments", ini_targets)

    defaults_match = re.search(
        r"^default_envs\s*=\s*(?P<body>.*?)(?=^\S|\Z)",
        ini,
        re.MULTILINE | re.DOTALL,
    )
    result.check(defaults_match is not None, "platformio.ini: cannot locate default_envs")
    if defaults_match:
        default_targets = set(re.findall(r"[A-Za-z0-9_-]+", defaults_match["body"]))
        result.same_set("platformio.ini default environments", default_targets)

    html = read("docs/index.html")
    html_targets = set(re.findall(r'data-board="([^"]+)"', html))
    result.same_set("installer board buttons", html_targets)

    site_js = read("docs/assets/site.js")
    board_block = site_js.split("const options", 1)[0]
    js_targets = {
        quoted or plain
        for quoted, plain in re.findall(
            r'^\s*(?:"([^"]+)"|([A-Za-z0-9_]+)):\s*\{', board_block, re.MULTILINE
        )
    }
    result.same_set("installer board metadata", js_targets)

    workflow = read(".github/workflows/firmware-v2.yml")
    match = re.search(
        r"\n\s+matrix:\s*\n\s+environment:\s*\n(?P<body>(?:\s+- [^\n]+\n)+)",
        workflow,
    )
    result.check(match is not None, "workflow: cannot locate build matrix")
    if match:
        workflow_targets = set(re.findall(r"^\s+-\s+([^\s#]+)", match["body"], re.MULTILINE))
        result.same_set("workflow build matrix", workflow_targets)


def environment_block(ini: str, target: str) -> str:
    match = re.search(
        rf"^\[env:{re.escape(target)}]\s*\n(?P<body>.*?)(?=^\[|\Z)",
        ini,
        re.MULTILINE | re.DOTALL,
    )
    return match["body"] if match else ""


def verify_profile_invariants(result: Verification) -> None:
    ini = read("firmware/platformio.ini")
    result.check(set(OTA_MARKERS) == set(TARGETS) - {"esp01_512k"},
                 "OTA marker table must cover every OTA-capable target")
    minimal = environment_block(ini, "esp01_512k")
    result.check("-D SVB_ENABLE_OTA=0" in minimal and "-D SVB_ENABLE_EXPERT=0" in minimal,
                 "esp01_512k must disable OTA and expert GPIO")
    result.check("eagle.flash.512k32.ld" in minimal,
                 "esp01_512k must use the 512 KiB/32 KiB filesystem linker script")

    esp01_1m = environment_block(ini, "esp01_1m")
    result.check("-D SVB_ENABLE_EXPERT=0" in esp01_1m,
                 "esp01_1m must omit expert GPIO because ESP-01 exposes no supported safe pin")

    for target in ("sonoff_basic_r2", "sonoff_basic_r3"):
        block = environment_block(ini, target)
        result.check("board = esp8285" in block, f"{target}: expected ESP8285 board")
        result.check("eagle.flash.1m64.ld" in block, f"{target}: expected 1 MiB linker script")
        result.check("-D SVB_RELAY_GPIO=12" in block,
                     f"{target}: onboard relay GPIO must be declared")

    main = read("firmware/src/main.cpp")
    power = read("firmware/src/PowerMonitor.cpp")
    result.check(main.count("digitalWrite(SVB_RELAY_GPIO, LOW)") >= 2,
                 "Sonoff relay must be forced low before and after pinMode")
    result.check(main.find("digitalWrite(SVB_RELAY_GPIO, LOW)") < main.find("Serial.begin"),
                 "Sonoff relay must be forced low before Serial and all service initialization")
    result.check("pin == SVB_RELAY_GPIO" in power,
                 "expert GPIO must reject the Sonoff relay pin")
    result.check("Sonoff Basic allows the isolated input only on GPIO 14" in power,
                 "Sonoff expert input must use the conservative GPIO14-only allowlist")
    result.check("stableMainsPresent_ = true" in power and
                 "rawMainsPresent_ = digitalRead" in power,
                 "an initially absent mains input must debounce into a MainsLost event")

    web_ui = (read("firmware/include/WebUi.h") + read("firmware/include/CompactWebUi.h") +
              read("firmware/include/MinimalWebUi.h"))
    result.check("firmware.bin для ESP-01S" not in web_ui,
                 "compact OTA UI must not claim that every compact build is ESP-01S")
    result.check(web_ui.count("x.send(new FormData(e.target))") >= 2,
                 "compact and full OTA forms must send multipart FormData")
    result.check(web_ui.count('name="adminPassword"') >= 3,
                 "every web UI profile must offer an optional admin password during setup")
    result.check("configured&&s.wifi.apActive" in web_ui and
                 "configured?'/api/wifi':'/api/setup'" in web_ui,
                 "minimal UI must allow primary Wi-Fi correction through recovery AP")
    result.check(web_ui.count("renderedGeneration") >= 4 and
                 web_ui.count("document.activeElement.matches('input,select,textarea')") >= 2,
                 "AJAX refresh must not rebuild compact or full forms while the user is editing")
    result.check(web_ui.count('<iframe id=') == 3 and
                 web_ui.count('>Оновити</button>') >= 3,
                 "every web UI profile must include the status iframe and refresh button")
    result.check("statusRefreshAfter=now+5000" in web_ui and
                 "statusRefreshAfter=n+5000" in web_ui and
                 "t=n+5000" in web_ui and
                 web_ui.count("src='/status-frame?refresh='+n") == 3,
                 "status iframe refresh must enforce the agreed five-second cooldown")
    result.check("contentWindow.location.reload" not in web_ui,
                 "cross-origin status iframe must be refreshed by replacing its URL")
    result.check('id="setupNetworks"' in web_ui and
                 'name="setupSsidChoice"' in web_ui and
                 "function chooseSetupWifi(" in web_ui and
                 "Інша або прихована мережа" in web_ui and
                 "api('/api/wifi/scan',{method:'POST'})" in web_ui,
                 "compact setup must render scanned Wi-Fi cards and retain a hidden-network fallback")
    result.check('name="channelKey" maxlength="64" required' not in web_ui and
                 "(!channelKey.isEmpty() && !validChannelKey(channelKey))" in read("firmware/src/WebApp.cpp"),
                 "first setup must allow Wi-Fi provisioning before a channel key is available")
    result.check(web_ui.count('name="reportingBackend"') == 6 and
                 web_ui.count('name="customUrl"') == 6 and
                 web_ui.count("toggleDestination(") >= 9,
                 "every web UI profile must select Svitlobot or a custom URL during and after setup")
    result.check('name="ssidChoice"' in web_ui and
                 "function scanInteractive()" in web_ui and
                 'id="settings"' in web_ui and
                 "api('/api/settings',{method:'POST'" in web_ui,
                 "compact panel must scan backup networks and allow channel settings updates")
    result.check('id="busy"' in web_ui and
                 "function runBusy(" in web_ui and
                 "x.upload.onprogress" in web_ui and
                 '<summary>Логи</summary>' in web_ui and
                 "function setupAccordions()" in web_ui and
                 "content:'📁'" in web_ui and
                 "content:'📂'" in web_ui,
                 "compact UI must block user actions, report OTA progress, and expose accordion logs")
    result.check("max-height:216px" in web_ui and
                 'data-secure="' in web_ui and
                 'name="manualOpen"' in web_ui and
                 "Прихована мережа без пароля" in web_ui and
                 "Відкрита мережа / стерти пароль" not in read("firmware/include/CompactWebUi.h"),
                 "compact Wi-Fi chooser must cap long lists and infer scanned-network security")
    result.check("Ключ показано повністю" not in web_ui and
                 "Під час завантаження панель буде заблокована" not in web_ui,
                 "compact UI must not explain self-evident form and loading behavior")
    result.check('id="buildInfo"' in web_ui and
                 'id="commitLink"' in web_ui and
                 "Alexander Tartmin" in web_ui and
                 "github.com/Baskerville42/svitloboard" in web_ui,
                 "compact UI footer must expose version, authorship, commit, and repository")
    result.check('<summary>Система</summary>' in web_ui and
                 'id="security"' in web_ui and
                 "clearAdminPassword" in web_ui and
                 "action:'factory-reset',confirm:'RESET'" in web_ui,
                 "compact UI must manage panel protection and expose confirmed factory reset")
    result.check(web_ui.count('id="handoff"') == 2 and
                 web_ui.count("IP-адреса плати") == 2 and
                 web_ui.count("watchSetup()") >= 4,
                 "compact and full setup must replace the form with connection handoff status")

    build_config = read("firmware/include/BuildConfig.h")
    version_script = read("firmware/scripts/version.py")
    web_app = read("firmware/src/WebApp.cpp")
    result.check("SVB_GIT_COMMIT" in build_config and
                 "kGitCommit" in build_config and
                 "SVB_GIT_COMMIT" in version_script and
                 '\\"commit\\":\\"' in web_app,
                 "firmware status must expose the source commit injected at build time")
    result.check('server_.on("/api/security"' in web_app and
                 "void WebApp::handleSecurity()" in web_app and
                 "memset(candidate.adminPassword" in web_app and
                 '\\"adminPasswordSet\\"' in web_app,
                 "panel protection must support changing and clearing the admin password")
    boot_recovery = read("firmware/src/BootRecovery.cpp")
    config_store = read("firmware/src/ConfigStore.cpp")
    result.check("SVB_CONFIG_SCHEMA=2" in ini and
                 "LegacyRecordV1" in config_store and
                 "Migrating config schema 1" in config_store,
                 "config schema 1 must migrate to the explicit reporting-backend schema")
    result.check("kFastBootResetThreshold = 4" in build_config and
                 "kFastBootWindowMs = 8000" in build_config,
                 "fast power-cycle recovery contract must remain four boots within 8 seconds")
    result.check("REASON_DEFAULT_RST" in boot_recovery and
                 "ESP_RST_POWERON" in boot_recovery,
                 "fast power-cycle recovery must count only platform power-on reset reasons")
    result.check("count >= kFastBootResetThreshold" in boot_recovery and
                 "store_.markFastBootStable()" in boot_recovery,
                 "fast power-cycle recovery must reset at the threshold and clear after stability")
    result.check("kBootSlotPaths[2]" in config_store and
                 "validBootRecord" in config_store,
                 "fast-boot counter must use redundant CRC-validated records")
    result.check("bootRecovery.begin(config.setupComplete)" in main and
                 "bootRecovery.loop()" in main,
                 "fast power-cycle recovery must be wired into setup and loop")

    web_app = read("firmware/src/WebApp.cpp")
    wifi_manager = read("firmware/src/WifiManager.cpp")
    result.check("kWifiReconfigureDelayMs = 2000" in build_config and
                 "kProvisioningApGraceMs = 60000" in build_config and
                 "kProvisioningStatusMs = 60000" in build_config and
                 "wifiReconfigureAt_ = millis() + kWifiReconfigureDelayMs" in web_app and
                 "apStopAt_ = now + kProvisioningApGraceMs" in wifi_manager,
                 "provisioning AP must remain available long enough to report the DHCP address")
    result.check("provisioningHandoffUntil_ = millis() + kProvisioningStatusMs" in web_app,
                 "status polling must be temporarily available during setup handoff")
    result.check('F(",\\"channelKey\\":\\"")' in web_app and
                 "already assigned to another profile" in web_app,
                 "configuration must expose the requested channel key and reject duplicate Wi-Fi profiles")
    result.check('kOtaCompatibilityMarker[] = "SVB2-OTA:" SVB_BOARD_NAME' in build_config,
                 "each firmware target must embed an OTA board compatibility marker")
    result.check("inspectOtaChunk(upload.buf, upload.currentSize)" in web_app and
                 "!otaMarkerFound_" in web_app,
                 "OTA must reject an image without the current board compatibility marker")
    svitlobot_client = read("firmware/src/SvitlobotClient.cpp")
    result.check('server_.on("/status-frame", HTTP_GET' in web_app and
                 "api_.statusPageUrl()" in web_app,
                 "status iframe route must authenticate and redirect through SvitlobotClient")
    result.check("Ключ каналу не налаштовано" in web_app and
                 "Додайте його в секції «СвітлоБот»" in web_app,
                 "status iframe must explain the valid setup-without-channel-key state")
    result.check('F("/status?channel_key=")' in svitlobot_client and
                 "urlEncode(config_.channelKey)" in svitlobot_client,
                 "status page URL must contain a URL-encoded channel key")


def verify_manifests(result: Verification) -> None:
    manifest_dir = ROOT / "docs/manifests"
    manifest_targets = {path.stem for path in manifest_dir.glob("*.json")}
    result.same_set("installer manifests", manifest_targets)

    for target, (family, factory, _) in TARGETS.items():
        path = manifest_dir / f"{target}.json"
        if not path.is_file():
            continue
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            result.errors.append(f"{path.relative_to(ROOT)}: invalid JSON: {error}")
            continue

        builds = document.get("builds")
        result.check(document.get("name") == "SvitloBoard", f"{target}: wrong manifest name")
        result.check(document.get("new_install_prompt_erase") is True,
                     f"{target}: new installs must offer erase")
        result.check(isinstance(builds, list) and len(builds) == 1,
                     f"{target}: manifest must contain exactly one build")
        if not isinstance(builds, list) or len(builds) != 1:
            continue
        build = builds[0]
        result.check(build.get("chipFamily") == family,
                     f"{target}: expected chipFamily {family!r}")
        parts = build.get("parts")
        result.check(isinstance(parts, list) and len(parts) == 1,
                     f"{target}: manifest must contain exactly one image part")
        if not isinstance(parts, list) or len(parts) != 1:
            continue
        suffix = ".factory.bin" if factory else ".bin"
        expected_path = f"../firmware/svitloboard-{target}{suffix}"
        result.check(parts[0].get("path") == expected_path,
                     f"{target}: expected image path {expected_path!r}")
        result.check(parts[0].get("offset") == 0, f"{target}: factory image offset must be 0")


def verify_partitions(result: Verification) -> None:
    partitions = ROOT / "firmware/partitions/svitloboard_4mb.csv"
    regions: list[tuple[int, int, str]] = []
    for line_number, raw in enumerate(partitions.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = [field.strip() for field in line.split(",")]
        result.check(len(fields) >= 5, f"partition line {line_number}: expected at least 5 fields")
        if len(fields) < 5:
            continue
        try:
            offset = int(fields[3], 0)
            size = int(fields[4], 0)
        except ValueError:
            result.errors.append(f"partition line {line_number}: invalid offset or size")
            continue
        regions.append((offset, offset + size, fields[0]))

    regions.sort()
    for previous, current in zip(regions, regions[1:]):
        result.check(previous[1] <= current[0],
                     f"partitions overlap: {previous[2]} and {current[2]}")
    if regions:
        result.check(regions[-1][1] <= 0x400000, "4 MiB partition table exceeds flash size")
    app_regions = [region for region in regions if region[2] in {"app0", "app1"}]
    result.check(len(app_regions) == 2 and app_regions[0][1] - app_regions[0][0] ==
                 app_regions[1][1] - app_regions[1][0],
                 "ESP32 OTA slots must exist and have equal size")


def verify_security_and_site(result: Verification) -> None:
    source = "\n".join(
        path.read_text(encoding="utf-8")
        for path in (ROOT / "firmware").rglob("*")
        if path.is_file() and ".pio" not in path.parts
    )
    # Split the literals so this verifier does not flag its own source.
    for forbidden in ("set" + "Insecure(", "set" + "Fingerprint("):
        result.check(forbidden not in source, f"TLS verification bypass found: {forbidden}")
    result.check('String(F("https://")) + kApiHost' in source,
                 "Svitlobot API URL is not visibly constructed with HTTPS")
    result.check("-----BEGIN CERTIFICATE-----" in source and
                 "-----END CERTIFICATE-----" in source,
                 "trusted root certificate is missing")
    result.check("static BearSSL::X509List trustAnchor" in source,
                 "ESP8266 must reuse its parsed trust anchor across HTTPS requests")

    result.check((ROOT / "LICENSE").is_file(), "root MIT license is missing")
    result.check(not (ROOT / "firmware/LICENSE").exists() and
                 not (ROOT / "firmware/NOTICE.md").exists(),
                 "firmware must not carry a second license or provenance notice")
    documentation = "\n".join(
        read(path)
        for path in (
            "README.md",
            "firmware/README.md",
            "docs/architecture.md",
            "docs/hardware-power-input.md",
            "docs/roadmap.md",
            "docs/test-plan.md",
        )
    ).lower()
    for forbidden in ("tas" + "mota", "g" + "pl"):
        result.check(forbidden not in documentation,
                     f"removed project reference remains in documentation: {forbidden}")

    html = read("docs/index.html")
    result.check("esp-web-tools@10.4.0/" in html,
                 "ESP Web Tools must be pinned to the reviewed 10.4.0 release")
    result.check("esp-web-tools@10/" not in html,
                 "ESP Web Tools must not float across minor or patch releases")
    result.check("esp-web-tools@latest" not in html,
                 "ESP Web Tools must not use the floating latest tag")

    public_copy = (read("docs/index.html") + "\n" + read("docs/assets/site.js")).lower()
    result.check("telegram-fallback" not in public_copy and "telegram fallback" not in public_copy,
                 "removed Telegram fallback feature is still advertised")
    result.check("експерт" not in public_copy,
                 "public installer must not advertise the deferred expert-mode guide")
    result.check("простий індикатор" in public_copy and "канал, статус і сповіщення" in public_copy,
                 "public installer must describe the firmware as an indicator for Svitlobot")
    result.check("https://svitlobot.in.ua/" in public_copy and
                 "https://t.me/svitloukrainebot" in public_copy,
                 "public installer must link to Svitlobot and its setup bot")
    result.check("«боже, памагі!»" in public_copy,
                 "ESP32-C3 warning lost its agreed short joke")
    result.check("тимошенко" not in public_copy and "юлія" not in public_copy,
                 "ESP32-C3 joke must not contain a person attribution")


def verify_artifacts(result: Verification, artifact_dir: Path) -> None:
    result.check(artifact_dir.is_dir(), f"artifact directory does not exist: {artifact_dir}")
    if not artifact_dir.is_dir():
        return
    for target, (_, factory, maximum_size) in TARGETS.items():
        suffix = ".factory.bin" if factory else ".bin"
        images = [artifact_dir / f"svitloboard-{target}{suffix}"]
        if factory:
            images.append(artifact_dir / f"svitloboard-{target}.bin")
        expected_names = {image.name for image in images}
        image_data: dict[str, bytes] = {}
        for image in images:
            result.check(image.is_file(), f"missing release image: {image.name}")
            if not image.is_file():
                continue
            size_limit = maximum_size + (0x10000 if image.name.endswith(".factory.bin") else 0)
            result.check(image.stat().st_size <= size_limit,
                         f"{image.name}: {image.stat().st_size} bytes exceeds {size_limit}")
            image_data[image.name] = image.read_bytes()
            marker = OTA_MARKERS.get(target)
            if marker:
                result.check(marker.encode("ascii") in image_data[image.name],
                             f"{image.name}: missing OTA compatibility marker {marker!r}")

        if target in ESP8266_FLASH_SIZE_CODES and images[0].name in image_data:
            verify_esp8266_image(result, target, images[0].name, image_data[images[0].name])
        elif factory and all(image.name in image_data for image in images):
            factory_image, application_image = images
            verify_esp32_images(
                result,
                target,
                application_image.name,
                image_data[application_image.name],
                factory_image.name,
                image_data[factory_image.name],
            )

        checksum_path = artifact_dir / f"svitloboard-{target}.sha256"
        result.check(checksum_path.is_file(), f"missing checksum file: {checksum_path.name}")
        if not checksum_path.is_file():
            continue
        observed: dict[str, str] = {}
        for line_number, raw in enumerate(
                checksum_path.read_text(encoding="utf-8").splitlines(), 1):
            fields = raw.split(maxsplit=1)
            if len(fields) != 2 or not re.fullmatch(r"[0-9a-fA-F]{64}", fields[0]):
                result.errors.append(
                    f"{checksum_path.name}:{line_number}: invalid SHA-256 record")
                continue
            filename = fields[1].lstrip("*")
            if Path(filename).name != filename:
                result.errors.append(
                    f"{checksum_path.name}:{line_number}: unsafe artifact filename")
                continue
            observed[filename] = fields[0].lower()
        result.check(set(observed) == expected_names,
                     f"{checksum_path.name}: checksum entries do not match target images")
        for image in images:
            if image.is_file() and image.name in observed:
                digest = hashlib.sha256(image.read_bytes()).hexdigest()
                result.check(observed[image.name] == digest,
                             f"{checksum_path.name}: hash mismatch for {image.name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifacts", type=Path,
                        help="also verify a flat directory of staged release binaries")
    arguments = parser.parse_args()

    result = Verification()
    verify_targets(result)
    verify_profile_invariants(result)
    verify_manifests(result)
    verify_partitions(result)
    verify_security_and_site(result)
    if arguments.artifacts:
        verify_artifacts(result, arguments.artifacts)

    if result.errors:
        for error in result.errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(f"Release contract OK: {len(TARGETS)} targets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
