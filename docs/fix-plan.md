# Bilge Monitor — Fix Plan

Status: planned, not started (Oct 2026). Review of `src/main.cpp` at `0cfdcad`.

## What it is

Hat Labs SH-ESP32 reading the bilge pump's power through the opto-isolated input on GPIO35. Publishes pump state, run count and last run duration to SignalK over WiFi every second. It detects **pump runs**, not water level — the separate high-water alarm in the boat's safety plan stays independent of this device.

## Current problems

| # | Problem | Where |
|---|---|---|
| 1 | Not reporting — no `environment.bilge.*` on the SignalK server (nootcore, 2026-10-27) | deployment |
| 2 | Input polarity unverified. Code treats HIGH = pump running; an opto input often reads LOW when energized. If inverted: state is backwards, count increments on stop, duration measures idle time | `main.cpp:67` |
| 3 | A continuously running pump (leak, stuck float) is invisible — duration only reported after the pump stops | `main.cpp:73-84` |
| 4 | No alerts — nothing reaches `notifications.*` | — |
| 5 | Run count lives in RAM, resets on every reboot | `main.cpp:14` |
| 6 | A pump already running at boot is never counted — `DigitalInputChange` reports changes only | `main.cpp:37` |
| 7 | `INPUT_PULLUP` has no effect: GPIO34–39 have no internal pull resistors | `main.cpp:38` |
| 8 | Hostname `nootka_bilgetank_mon` has underscores (invalid in hostnames) | `main.cpp:32` |
| 9 | `platformio.ini` uses `extends = espressif32_base` but no such section exists | `platformio.ini` |
| 10 | README lists wire colors without what each connects to | `README.md` |
| 11 | SensESP 2.7; current is 3.x | `platformio.ini` |

Keep: publishing every second — it doubles as a heartbeat.

## Changes

1. **Port to SensESP 3.x**, using the current Hat Labs SH-ESP32 project layout as reference. Fix `platformio.ini`.
2. **Polarity:** verify on the boat first (run the pump manually, watch the raw input). Add an "invert input" config option rather than hard-coding.
3. **Read initial state at boot** and treat a running pump at boot as a run in progress.
4. **SignalK outputs** (paths under `environment.bilge.main`):

   | Path | Type | Notes |
   |---|---|---|
   | `.state` | bool | pump running (existing) |
   | `.currentRunDuration` | s | 0 when idle, counts up while running — new |
   | `.lastRunDuration` | s | existing |
   | `.runCount` | count | existing; persist or drop (see 6) |

5. **Alerts via SignalK meta zones** on `currentRunDuration` (server raises `notifications.environment.bilge.main.currentRunDuration`). Thresholds: TBD — log normal run times first.
6. **Run count:** either persist to flash, or drop it and derive runs-per-hour from `.state` changes in InfluxDB/Grafana. Preference: derive in Grafana, so the device stays stateless. Decide during implementation.
7. **Staleness:** confirm SignalK or a Grafana alert flags the bilge data going stale (device offline must not look like "pump idle").
8. **Hostname:** `nootka-bilge`.
9. **README:** document what each wire connects to (pump positive sense, ground, etc.) and the board.

## Transport

WiFi → SignalK only (despite the repo name, there is no NMEA 2000 code). Stay WiFi-only: alerts are raised by the SignalK server either way, and the staleness alert covers WiFi loss.

## Optional

- NMEA 2000 output of pump state via PGN 127501 (Binary Switch Bank Status), if an MFD should show it or WiFi coverage at the board is poor. Run durations have no standard PGN and would stay on WiFi. Hold off until the N2K bus anomaly (~200 source addresses) is understood.
- Runs-per-hour alert (rising water ingress) — likely easier in Grafana than on the device.
- If the planned high-water float switch gets a second isolated input, log its state here too. The audible alarm itself stays standalone.

## Test plan

1. Bench: drive the opto input with 12 V, confirm state, duration and boot-while-running behavior.
2. Boat: run the pump manually — verify state, current duration counting, last duration on stop.
3. Hold the pump on past the warning threshold — verify the SignalK notification fires.
4. Power the device off — verify the staleness alert.
