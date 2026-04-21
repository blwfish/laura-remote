# Laura — Product Spec

## One-liner

A long-range, bidirectional, diagnosable remote trigger and keep-alive system for Nikon cameras, built for motorsports photography at venues where existing PocketWizard-class gear falls short.

## The problem

Motorsports venues are big. Sebring is 2,000 ft from turn 1 to the flag stand; Road America adds real elevation; Lime Rock has a hill right in the middle. Photographers routinely want to fire a remote camera mounted two turns away, often through chain-link fencing, sometimes with no line-of-sight at all.

Current tools (PocketWizard Plus III and similar) have three limitations that bite constantly:

1. **No confirmation.** You press the button and hope. At 2,000 ft through a fence, hoping isn't enough.
2. **Range ceilings at hard track distances.** Sebring turn-1-to-flag-stand is marginal; Road America is worse. No bridging option when geometry makes LOS impossible.
3. **No observability.** When a remote cam "doesn't work," there's no way to know *why* — link problem? camera problem? battery? antenna? You walk over, check, and hope you don't miss the action.

A fourth issue for long auto-capture setups: Nikon meter timeouts turn a camera off after a configurable interval, and a cam set up three hours before the start of a race may or may not still be awake when you want it.

## The solution

A three-part system built around **LoRa** at 915 MHz for range, **bidirectional acknowledgment** for certainty, and **logging on both ends** for diagnosis after the fact:

- **TX** — handheld transmitter, roughly PocketWizard Plus III size, on a lanyard. OLED display, three buttons (shoot, half-press, mode), three status LEDs.
- **RX** — small receiver, velcros or magic-arm-mounts to the camera, with a pigtail to the Nikon 10-pin socket.
- **Repeater** — optional bridging unit for venues where no LOS exists between TX and RX (Road America, Lime Rock). Same hardware as the RX; different firmware mode.

All three run on 2×AA (Eneloop NiMH or Linogy Li-ion). Expected battery life is days-to-weeks depending on duty cycle — far exceeding a race weekend.

## Key features

### Certainty

Every shot command is acknowledged. TX has three LEDs:
- **Link** — packet got to the camera (directly or via repeater).
- **Fire** — camera's Ready pin actually transitioned low-then-high, meaning the shutter really fired. Not just "10-pin contact asserted."
- **Repeater** (optional) — your packet went via a repeater rather than directly.

If you press shoot and see green-green, you know.

### Range

LoRa at 915 MHz with adaptive spreading factor: normal shots go at SF7 (fast, ~50 ms round-trip), falling back automatically to SF10 and SF12 on missed acknowledgment. SF12 adds significant link budget at the cost of a second or two of latency — acceptable for the worst-case shot, and you'll see in the log what fallback was used.

Estimated workable range: comfortable at 2,000+ ft LOS, marginal through dense fencing at max distance. Add a small Yagi on the TX for the corner cases.

### Repeater

For venues where LOS is physically impossible (hills, grandstands), a repeater unit placed at a vantage point that sees both you and the camera closes the gap. One hop only — no mesh routing complexity. The TX shows repeater-ACK and camera-ACK separately so you can tell which leg of the chain broke when something does.

### Keep-alive

Each RX can be set to a keep-alive mode where it pulses the camera's focus pin every N seconds (configurable, default 20 s), preventing the meter from timing out. Critical for oval tracks and long pre-positioning scenarios where a camera sits for hours before the race starts.

The operator arms keep-alive remotely: point the TX at the camera, hit `KA_ON`, walk away. The RX continues autonomously; no further TX presence needed.

### Addressing

Supports up to 254 receivers per photographer, plus group addresses for "fire all flag-stand cams" or "wake all auto-captures." Each photographer has a private 16-bit network ID, so Vic and Billy shooting the same event never interfere with each other.

### Multiple channels

Three channels in the 902–928 MHz ISM band, selectable manually or with automatic fallback on interference. Repeater pins to a known channel.

### Diagnostics — the PW doesn't do this

**Per-shot event log** on the TX. Every command, with: timestamp (UTC from GPS), target camera, outcome, final spreading factor used, retry count, RSSI, link path. Held in flash, survives battery swaps, accessible via OLED during the session or via USB-C dump after.

**Live telemetry from each RX** piggy-backed on every ACK: battery voltage, internal temperature, Ready-pin state, RSSI of signals heard. Appears in the TX log against each event. You know the camera's battery was at 34% and 52 °C *at the moment the shot fired* without walking to the camera.

**Per-shot event log on the RX** too. Even events the TX never sees — keep-alive pulses, orphan packets, Ready-pin state transitions during the session. Dumped via USB-C post-race.

**Session-level statistics** — per-RX drop rate, RSSI trend, link-health snapshot — viewable on the TX OLED. Useful mid-race to notice "turn 1 cam is drifting" before it fully fails.

**Lightroom integration** — a post-session Python tool reads both logs, matches events against RAW files by timestamp + camera serial, and writes XMP sidecars with keywords (`remote-fired`, `cam-flagstand`, `tx-billy`, `via-repeater`) plus custom fields (RSSI, SF, temp). Filter the Lightroom catalog by remote-triggered vs. handheld shots, or by which camera fired them.

### Clock

GPS module (u-blox MAX-M10) on the TX. Acquires UTC at power-on, runs the RTC from GPS time for the session, powers down. Matches the time reference used by Nikon's own GPS accessories, so TX logs and camera EXIF automatically align — no manual clock sync needed.

Coin-cell backup on the RTC so clock survives battery swaps.

### Pairing

Press and hold a button on the RX while the TX transmits a pair beacon. Network ID, device address, and encryption key exchanged in one handshake. No dipswitches, no menus, no serial numbers to write down.

## Scenarios

**Sebring flag stand, turn 1 → flag stand:** TX at turn 1, RX on camera at flag stand, ~2,000 ft, 8-ft chain-link in the path. Direct link at SF10. Green-green on every shot. Optional Yagi on TX for margin.

**Road America turn 1 → turn 14:** no LOS. Repeater on the flag stand (which sees both). Two-hop path. TX shows repeater-ACK + camera-ACK separately.

**Lime Rock uphill:** hill obstructs direct link. Repeater on the hill crest. Same two-hop pattern.

**Daytona oval, pre-race setup:** camera positioned three hours before the start of the race, mounted inside a chain-link perimeter. TX powered off during the wait. Before the race, operator hits `KA_ON`; RX wakes from CAD sniff, starts pulsing focus pin, acks back with camera state and battery. Camera stays alive until `KA_OFF`.

**Watkins Glen, two active remotes:** one at the Boot, one on the starter's stand. Both addressed individually. Half-press or shoot targets the selected cam. Group address fires both simultaneously if needed.

**Six-camera event, typical weekend:** 4 handhelds (no remote), 1 active remote (TX-operated), 1 auto-capture (keep-alive only). Single TX handles both remote modes. Post-event, dump logs from TX and both RXs, merge against Lightroom, filter cull by camera.

## System parts at a glance

| Part | Size | Power | Interfaces | UI |
| --- | --- | --- | --- | --- |
| TX | PW3-ish | 2×AA | SMA, USB-C | 128×64 OLED, 3 buttons, 3 LEDs |
| RX | Small (matchbox) | 2×AA | SMA, USB-C, Nikon 10-pin pigtail | 128×32 OLED (optional), 3 LEDs, 1 button |
| Repeater | Same PCB as RX | 2×AA | SMA, USB-C | 3 LEDs, 1 button |

Shared PCB between RX and repeater, populated differently per role. TX is its own board.

## Comparison to PocketWizard Plus III

| Feature | PW Plus III | Laura |
| --- | --- | --- |
| Radio | 344 MHz FHSS | 915 MHz LoRa |
| Range (LOS, typical) | ~1,600 ft | 2,000+ ft, with better penetration and repeater option |
| Confirmation of trigger | None | Bidirectional ACK with Ready-pin verification |
| Repeater support | No | Yes, one-hop |
| Keep-alive | No | Yes, autonomous at RX |
| Event log | No | On both TX and RX, flash-backed, USB dump |
| Clock / timestamps | No | GPS UTC, matches Nikon GPS accessories |
| Telemetry from camera side | None | Battery, temp, RSSI, Ready state in every ACK |
| Lightroom integration | None | Post-session XMP sidecar tool |
| Addressing | 4 channels | 256 addresses × multi-network |
| Batteries | 2×AA | 2×AA |
| Form factor (TX) | ~115×60×32 mm | comparable target |

## Scope boundaries

**In v1:**
- Nikon 10-pin support (Z9, Z8, D5, and any other Nikon with the 10-pin socket)
- Full feature set above

**Not in v1:**
- USB-C tethered cameras (Z6iii, Z6ii, Z5) — requires PTP stack on the RX, much bigger lift. Possible v2.
- Canon, Sony, or other bodies — possible v2 with appropriate release cable.
- Flash sync (hot-shoe pickup). Skipped intentionally — EXIF flash metadata pollution isn't worth the marginal fire-confirmation improvement.

## Who it's for

Motorsports photographers who run multi-camera setups at large venues, need reliable remote triggering, and value observability. Assumes a baseline comfort with pairing devices, battery-swapping rechargeables, and occasionally running a Python script on a laptop post-event. Not a mass-market consumer product.

Initial users: Billy and Vic. Scale if it works.
