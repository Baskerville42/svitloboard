# SvitloBoard 2 architecture

SvitloBoard 2 is a new firmware with its own behavior contract and module boundaries. Version 1
behavior informed the product requirements but its source is not a dependency and remains only in
Git history. Tasmota is an engineering reference: proven algorithms may be adapted with
provenance, but the project is not a Tasmota fork or skin.

## Runtime flow

```text
boot
  ├─ ConfigStore: mount LittleFS, validate both records, choose newest generation
  ├─ BootRecovery: count only fast physical power cycles; clear after eight stable seconds
  ├─ WifiManager: try last-known-good, primary, then backups
  │    └─ all unavailable → AP+STA recovery mode with bounded exponential retry
  ├─ WebApp: captive setup and local AJAX API
  ├─ SvitlobotClient: NTP → verified TLS → bounded ping cadence
  └─ PowerMonitor: optional isolated digital input → debounced OFF event,
       including an already-absent input at boot
```

All periodic work is driven by `loop()` and rollover-safe deadlines. A failed Svitlobot request
does not cause Wi-Fi roaming. Wi-Fi association, service reachability, TLS, and HTTP status are
separate diagnostic states.

## Configuration durability

Each record contains a magic value, schema, byte size, generation, fixed-width payload, and CRC32.
Writes always target the inactive slot, are read back and validated, and only then replace that
slot. The last valid generation remains readable after an interrupted write. Heartbeats and status
polling never write flash; only explicit configuration changes and a changed last-known-good Wi-Fi
profile do.

The v1 EEPROM struct is not read. A first v2 boot starts setup without interpreting erased or
arbitrary bytes as a C struct. This eliminates the v1 factory-reset loop instead of migrating its
unsafe layout into the new architecture.

The recovery counter is separate from configuration and also uses two generation-numbered,
CRC-checked LittleFS records. Only a platform-reported power-on reset increments it. Four starts
that are each interrupted before eight seconds trigger a factory reset; eight seconds of stable
uptime clears the counter. OTA, software, exception, and watchdog resets are deliberately ignored
so an update or crash loop cannot erase credentials.

## Wi-Fi policy

- primary plus two backup profiles;
- an immediate first attempt during boot;
- 12-second bounded association attempts;
- last-known-good first, then each enabled profile once;
- recovery AP while retrying after a complete failed cycle;
- exponential retry delay capped at five minutes;
- no reconnect merely because DNS, TLS, or the API is down;
- candidate settings are length-validated before they can replace a stored record.

A future hardware test suite must still measure association, DHCP, router loss, and failback on
real ESP-01, ESP8266 dev boards, and every released ESP32 target.

## HTTPS and API cadence

The client verifies the hostname and the Google Trust Services R4 root, and waits for NTP before
TLS validation. There is no leaf-certificate fingerprint and no `setInsecure()` fallback. Requests
have a 6.5-second bound. Normal pings default to 60 seconds and cannot be manually forced more
often than every 30 seconds. OFF retry starts at 30 seconds and backs off to five minutes.

The root certificate expires in 2036, but certificate-chain behavior is checked in CI/release
maintenance before then. A CA update is firmware maintenance, not an expert UI setting.

## Local web security

The setup portal exposes no stored passwords or channel key. After setup, an optional HTTP Basic
administrator password protects the page and APIs. Mutating calls also require a custom request
header, preventing ordinary cross-origin HTML forms from triggering configuration or reset.

This protects a trusted local network; it is not equivalent to local HTTPS. Users should set the
administrator password and avoid hostile shared networks.

## OTA model

ESP32 uses two 1.5 MiB application partitions. ESP8266 OTA is enabled only when the selected flash
layout leaves enough staging space. ESP-01 512 KiB is serial-install only. Browser factory images
and local OTA application images are distinct artifacts on ESP32.

Current OTA validates the platform image using the Arduino core and requires a SvitloBoard target
marker embedded in the uploaded binary. This prevents common accidental cross-board uploads, such
as installing a D1 mini image on a Sonoff, but it is not a cryptographic signature and is not a
security boundary. Signed release verification and ESP32 boot rollback are release blockers before
declaring 2.0 production-stable; see the roadmap.
