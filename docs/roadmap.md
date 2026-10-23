# Roadmap

## 2.0 foundation — implemented in source

- independent ESP8266/ESP32 firmware architecture;
- first-boot captive AP and explicit factory reset;
- primary plus two backup Wi-Fi profiles and recovery AP;
- rotating validated settings records;
- verified HTTPS, bounded cadence, pause, diagnostics;
- fast AJAX local UI and browser-upload OTA;
- expert isolated digital power input and immediate OFF request;
- PlatformIO build profiles, release artifacts, and GitHub Pages installer.

## Before 2.0 is called production-stable

The authoritative hardware acceptance checklist is [test-plan.md](test-plan.md). Until its
matrix is complete, the website and release notes must continue to call version 2 a test build.

- bench-test every released board and record flash/RAM/TLS headroom;
- power-cut tests during both config-slot rotations and OTA;
- AP, DHCP, DNS, router-loss, backup-roaming, and recovery stress tests;
- validate the Svitlobot API response contract with the service owner;
- add signed OTA metadata and ESP32 rollback confirmation;
- harden the existing board-compatibility marker with signed OTA metadata;
- accessibility and mobile layout pass for both web interfaces;
- decide whether ESP-01 512 KiB has enough operating headroom for long-term support.

## Later releases

- Improv Serial provisioning after the first USB flash;
- optional custom API endpoint with a separately managed trust anchor;
- build service for custom defaults without putting Wi-Fi secrets in public artifacts;
- additional isolated sensor types and board templates;
- a dedicated Sonoff Basic R2 battery-and-isolated-sensor DIY design and safety manual;
- import assistant for selected v1 settings, isolated from the v2 storage format.
