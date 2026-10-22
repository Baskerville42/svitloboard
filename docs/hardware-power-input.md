# Expert OFF input: hardware and safety

> **Danger: mains voltage can kill, start a fire, and destroy every connected device.** This guide
> describes only the low-voltage side of an isolated, certified sensing module. It is not a wiring
> plan for 230 V. Have all mains-side work selected and installed by a qualified electrician and
> follow local electrical rules.

## What the firmware expects

The board needs a stable digital signal that means “external mains is present.” The ESP board,
router, and Internet uplink must stay powered from a UPS long enough to send `channelPingOff`.
Measuring the ESP supply or assuming that load current equal to zero means an outage is incorrect.

Use a ready-made module with documented galvanic isolation and a logic-level output compatible
with 3.3 V. The output presented to the ESP must never exceed the selected board's GPIO limits.
Do not connect a resistor divider, optocoupler assembled on a breadboard, relay coil, or mains wire
directly to GPIO.

## Low-voltage connection

With the entire installation disconnected from mains, connect only the module's isolated output:

```text
isolated module logic OUT  ─── selected GPIO
isolated module logic GND  ─── ESP GND
isolated module logic VCC  ─── module's documented 3.3 V supply (only if supported)
```

Some modules have an open-collector output. For those, enable `INPUT_PULLUP` only when the module's
documentation explicitly supports it, then choose `LOW` as the “mains present” level if appropriate.
Test the logic state using an isolated bench source before any mains-side installation.

## Firmware setup

1. Open **Expert settings → Power input**.
2. Enable expert hardware and immediate OFF notification.
3. Select the actual GPIO number, not a board silk-screen alias such as `D2`.
4. Select the level that means mains is present and a debounce interval (1.5 s default).
5. Prefer saving while the external mains signal is present. If the board starts from backup power
   while the signal already reports an outage, it deliberately emits one OFF event after debounce.
6. Disconnect only the sensed mains source while the UPS remains active and verify one OFF request
   in diagnostics. Restore mains and verify that a queued stale OFF request is cancelled.

Generic ESP8266 builds permit only GPIO 4, 5, 12, 13, or 14 in this feature. GPIO 0, 2, and 15 are
boot straps; GPIO 1/3 are UART; GPIO 6-11 are flash. Sonoff Basic R2/R3 builds use a stricter
GPIO14-only allowlist: GPIO12 drives the relay, GPIO13 drives the status LED, and GPIO0 is the
button/boot strap. This is a firmware guard, not yet the promised Sonoff DIY wiring design; verify
the exact board revision before soldering. ESP-01/ESP-01S has no recommended supported pin for this
feature. Use D1 mini, NodeMCU, the guarded Sonoff GPIO14 path, or a suitable ESP32 board instead.

ESP32 variants use separate conservative allowlists. They exclude boot straps, flash/PSRAM,
native USB, console UART, and known onboard peripherals for the exact reference target. The local
panel reports the accepted list when it rejects a setting. A pin being accepted by the firmware
does not prove that an arbitrary clone leaves it unconnected; verify the exact board schematic.
