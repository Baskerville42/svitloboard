# SvitloBoard 2 release test plan

Compilation is necessary but does not prove that radio, flash layout, USB boot mode, or
power-loss recovery work on a physical board. A release can be marked production-stable only
after this checklist is completed for every published target.

## Hardware matrix

| Target | Reference hardware | Flash | Initial install | OTA | Status |
| --- | --- | ---: | --- | --- | --- |
| `esp01_512k` | ESP-01 with verified 512 KiB | 512 KiB | USB–UART | unavailable | Not run |
| `esp01_1m` | ESP-01/ESP-01S with verified 1 MiB | 1 MiB | USB–UART | required | Not run |
| `d1_mini` | WEMOS/LOLIN D1 mini | 4 MiB | onboard USB | required | Not run |
| `nodemcuv2` | NodeMCU DevKit 1.0 | 4 MiB | onboard USB | required | Not run |
| `sonoff_basic_r2` | Sonoff Basic R2 | 1 MiB | isolated USB–UART, 3.3 V | required | Not run |
| `sonoff_basic_r3` | Sonoff Basic R3 | 1 MiB | isolated USB–UART, 3.3 V | required | Not run |
| `esp32dev` | ESP32-WROOM-32 DevKit, 4 MiB | 4 MiB | onboard USB | required | Not run |
| `esp32-c3` | Espressif ESP32-C3-DevKitM-1 | 4 MiB | onboard USB–UART | required | Not run |
| `esp32-s3` | Espressif ESP32-S3-DevKitC-1-N8 | 8 MiB | onboard USB | required | Not run |

Record the exact module marking, flash ID, USB bridge, test date, firmware SHA-256, and result
for every row. A clone is an additional sample, not a replacement for the named reference board.

## Acceptance sequence

Run the following sequence on each row of the matrix. Save the serial log for any failure.

1. **Clean install** — erase the entire flash, install from the published HTTPS browser page,
   reboot without boot straps, and confirm there is no reset loop.
2. **Provisioning AP** — within 15 seconds find `SvitloBoard-XXXXXX`, connect using `svitlobot`,
   confirm captive DNS redirects to `192.168.4.1`, and open the setup page manually as well.
3. **Invalid input** — verify that an empty channel key, overlong SSID, invalid password length,
   and malformed device name are rejected without losing the previous configuration.
4. **First setup** — save a valid primary 2.4 GHz network and channel key. Confirm the HTTP
   response completes before the AP disappears, then confirm STA IP, DNS, NTP, verified TLS,
   and a successful Svitlobot ping.
5. **Persistence** — power-cycle 20 times, including five power cuts during configuration-slot
   rotation. The newest complete CRC-valid record must win; a torn record must not force a reset.
6. **Backup Wi-Fi** — configure two backup networks. Remove the primary, then each backup in
   turn. Confirm bounded attempts, ordered fallback, a stable last-known-good update after
   30 seconds, and no profile switching merely because the Svitlobot API is unavailable.
7. **Recovery AP** — make all saved networks unavailable. Confirm that the AP returns without
   deleting settings, and that correcting a network restores STA mode.
8. **AJAX panel** — exercise every control included by the target profile. Standard profiles cover
   status refresh, network scan, all three Wi-Fi forms, pause/resume, immediate ping, reconnect,
   authentication, diagnostics, and factory-reset confirmation. Compact profiles cover setup,
   status, all three Wi-Fi forms, authentication, and OTA. The minimal ESP-01 profile covers setup,
   authentication, status, and correcting the primary network through recovery AP; verify its
   documented physical factory-reset path separately.
9. **OTA** — install a newer correct binary; reject a binary for another target on the same chip,
   a wrong-chip binary, and a truncated binary; then interrupt one upload. The device must either
   boot the previous image or remain recoverable by serial flash.
10. **Factory reset** — reset from the web panel, reboot, and confirm both SvitloBoard settings
    and SDK-persisted Wi-Fi credentials are gone.
11. **Power-cycle recovery** — configure the board, then interrupt power during each of its first
    eight seconds three times. Leave the fourth start powered and confirm that setup is erased and
    the provisioning AP returns. Configure it again, leave it powered for more than eight seconds,
    and confirm a later power cycle starts a new count. Finally verify that four software restarts,
    four watchdog resets, and an OTA restart do not erase settings.

## Board-specific checks

- **ESP-01 512 KiB:** confirm serial reinstallation instructions are accurate; OTA must not be
  shown or accepted. Observe free heap during at least 100 HTTPS pings.
- **Sonoff Basic R2/R3:** perform serial flashing only while disconnected from mains and load.
  Confirm GPIO12 is driven low from the earliest application startup and the relay never closes.
  Confirm expert input accepts GPIO14 only and rejects the relay, LED, button, UART, and flash pins.
- **ESP32-C3:** test normal automatic flashing and manual `BOOT` + `RESET`. Do not load GPIO2,
  GPIO8, or GPIO9 during reset.
- **ESP32-S3:** verify the N8 module marking and confirm that both first install and OTA use the
  intended partition table despite the board's 8 MiB physical flash.

## Expert isolated input

This is a separate safety test and is not required for the ordinary indicator use case. Use only
a certified isolated low-voltage output; never connect mains directly to a GPIO. Verify allowed
pins, both active levels, pull-up selection, debounce, one queued OFF request per transition, and
recovery after Wi-Fi loss. Boot the board from backup power while the isolated input already
reports missing mains and confirm that one OFF request is sent after debounce. Record the sensing
module and wiring diagram with the result.

## Release evidence

Before creating a `v2.*` tag, attach or link:

- the successful nine-target GitHub Actions run;
- `firmware/scripts/verify_release.py --artifacts <staged-firmware-directory>` output;
- SHA-256 files from the release artifacts;
- the completed hardware matrix and any known limitations;
- one successful browser-install recording for each chip family: ESP8266, ESP32, ESP32-C3,
  and ESP32-S3.
