# SvitloBoard 2 validation record

This record describes evidence collected from the current local working tree on 2026-10-23. It
does not replace the physical-board acceptance matrix in [test-plan.md](test-plan.md).

## Automated evidence

- PlatformIO 6.2.0 compiled all nine declared environments successfully with the pinned ESP8266
  4.2.1 and ESP32 6.10.0 platforms.
- `firmware/scripts/verify_release.py` accepted the target matrix, manifests, partition table,
  security invariants, board feature invariants, staged release images, and their SHA-256 files.
- Staged-image checks validate the ESP8266 combined-image header, declared flash size, embedded
  byte count and eboot CRC; ESP32 checks validate chip IDs, bootloader/partition offsets, and that
  each factory image contains the exact matching OTA application at `0x10000`.
- The embedded JavaScript from all three local web UI profiles passed `node --check`.
- Version injection was verified in application images. In particular, the constrained ESP-01
  512 KiB build with a CI-length `2.0.0-dev.abcdefg` string was 456,288 B, stayed below its
  457,000 B gate, and a subsequent default build removed the override again.
- Every OTA-capable application and factory image contains its target-specific `SVB2-OTA:` marker.
- Sonoff Basic R2/R3 accept only GPIO14 for the optional isolated input; their relay, status LED,
  button/boot strap, UART, and flash pins are excluded by the target-specific allowlist.
- A local Pages assembly served all nine manifests and every manifest-referenced firmware URL with
  HTTP 200; ESP32 manifests resolved to factory images and ESP8266 manifests to application images.
- The public installer was rendered in the in-app browser and its board picker, instructions,
  warnings, and troubleshooting controls were exposed to the accessibility tree.
- The installer dependency is pinned to ESP Web Tools 10.4.0 instead of a floating major tag; its
  exact module URL returned HTTP 200 during validation.

| Target | Application image | Browser factory image |
| --- | ---: | ---: |
| `esp01_512k` | 456,272 B | n/a |
| `esp01_1m` | 462,112 B | n/a |
| `d1_mini` | 493,824 B | n/a |
| `nodemcuv2` | 493,824 B | n/a |
| `sonoff_basic_r2` | 463,264 B | n/a |
| `sonoff_basic_r3` | 463,264 B | n/a |
| `esp32dev` | 1,055,104 B | 1,120,640 B |
| `esp32-c3` | 1,075,168 B | 1,140,704 B |
| `esp32-s3` | 1,004,992 B | 1,070,528 B |

These sizes are development-build evidence, not permanent product specifications. CI enforces the
limits defined in `firmware/scripts/verify_release.py` and `.github/workflows/firmware-v2.yml`.

## Live TLS observation

An `openssl s_client` handshake to `api.svitlobot.in.ua:443` with SNI for the same hostname
returned verification code 0. The observed chain was:

```text
svitlobot.in.ua -> Google Trust Services WE1 -> GTS Root R4
```

The observed leaf certificate was valid from 2026-09-06 through 2026-12-05. Firmware trusts the
long-lived GTS Root R4, not this rotating leaf certificate or a manually entered fingerprint. This
observation proves the selected trust anchor matches the live chain on the test date; physical
ESP8266/ESP32 handshakes still belong in the hardware matrix.

## Evidence still required

- all physical-board rows in [test-plan.md](test-plan.md);
- authenticated calls with a disposable real Svitlobot channel key, including `channelPingOff`;
- power-cut recovery during config rotation and OTA;
- Pages deployment from `main` and a browser flash over its public HTTPS URL;
- ESP8266 OTA headroom and long-running TLS heap measurements, especially on ESP-01 1 MiB and
  Sonoff Basic R2/R3.

## GitHub Actions evidence

Pull request run `39910060064` completed successfully for commit
`b18286f4f98eea23ce9f0c7d353616a981abf71f`. The release-contract job and all nine
PlatformIO matrix jobs passed. GitHub retained one non-expired firmware artifact for every
declared target. The Pages and release jobs were correctly skipped because the event was a pull
request; their deployment paths still require a `main` push and a `v2.*` tag respectively.
