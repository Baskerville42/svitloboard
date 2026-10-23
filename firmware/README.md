# SvitloBoard 2 firmware

This directory contains the complete SvitloBoard 2 firmware. Version 1 source is not a build
dependency and remains available only in Git history and earlier releases.

## Current behavior

- first boot and explicit factory reset start `SvitloBoard-XXXXXX` access point;
- the access point password is `svitlobot`;
- the captive web UI stores a primary Wi-Fi network, channel key, and optional local
  administrator password;
- three Wi-Fi profiles are tried immediately, last-known-good first, with bounded attempts,
  exponential retry backoff, and a recovery AP when all profiles are unavailable;
- API availability never causes Wi-Fi profile churn;
- settings use two CRC-checked, generation-numbered LittleFS slots;
- four genuine power-on starts, each interrupted within eight seconds, erase a completed setup;
  software restarts, OTA restarts, and watchdog resets do not increment this recovery counter;
- HTTPS uses hostname verification, current time from NTP, and the GTS Root R4 trust anchor;
- normal pings are limited to 30 seconds or slower (60 seconds by default);
- OTA rejects binaries that lack the current board target marker; this prevents common accidental
  cross-board uploads but is not a cryptographic signature;
- AJAX UI, diagnostics, and an opt-in isolated digital mains input are available when enabled by
  the build profile.

## Interface profiles

Flash capacity determines the local-panel feature set; network failover, validated settings, and
verified Svitlobot HTTPS remain firmware behavior rather than browser-page behavior.

| Profile | Targets | Local panel |
| --- | --- | --- |
| Minimal | ESP-01 512 KiB | first setup, live status, recovery Wi-Fi correction; no OTA or expert GPIO |
| Compact | ESP-01/ESP-01S 1 MiB, Sonoff Basic R2/R3 | first setup, three Wi-Fi profiles, status, OTA |
| Standard | D1 mini, NodeMCU v2, ESP32 targets | full settings, actions, diagnostics, OTA, expert input |

The compact and minimal profiles intentionally omit nonessential controls to preserve application
and OTA headroom. Factory reset by four short physical power cycles remains available on every
target after setup. ESP-01/ESP-01S builds also omit expert GPIO entirely because their exposed
GPIO0/GPIO2 pins affect boot mode and are not accepted for this feature.

## Build

Install PlatformIO Core and run the default build. It compiles all nine release targets:

```sh
pio run -d firmware
```

Useful individual environments:

```sh
pio run -d firmware -e esp01_512k
pio run -d firmware -e esp01_1m
pio run -d firmware -e d1_mini
pio run -d firmware -e nodemcuv2
pio run -d firmware -e sonoff_basic_r2
pio run -d firmware -e sonoff_basic_r3
pio run -d firmware -e esp32dev
pio run -d firmware -e esp32-c3
pio run -d firmware -e esp32-s3
```

`esp01_512k` is a deliberately reduced, serial-install profile without OTA or expert GPIO.
Whether it remains a supported release target is determined by the CI size gate and hardware
tests; TLS is not removed to make an oversized build appear successful.

`sonoff_basic_r2` and `sonoff_basic_r3` target the stock ESP8285 with 1 MiB flash. SvitloBoard
forces the onboard relay off and does not expose switching controls; these targets currently
behave only as presence indicators. Any future R2 battery and isolated mains-sensor conversion
requires a separate hardware design and safety guide.

## Safety

The expert mains input is a **digital low-voltage input only**. Never connect 230 V, a resistor
divider, or any non-isolated circuit directly to an ESP board. Use a certified isolated sensing
module, put the router/ONT and board on backup power, and have mains wiring installed by a
qualified electrician. `channelPingOff` reports an outage; it does not switch or disconnect mains.

See [docs/architecture.md](../docs/architecture.md),
[docs/hardware-power-input.md](../docs/hardware-power-input.md), and
[docs/roadmap.md](../docs/roadmap.md). The release acceptance procedure is in
[docs/test-plan.md](../docs/test-plan.md), with current automated evidence in
[docs/validation.md](../docs/validation.md).
