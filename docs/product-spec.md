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
- **RX** — small receiver, velcros or magic-arm-mounts to the camera. A 3.5 mm TRRS jack on the RX accepts any PocketWizard-compatible pre-release cable (N10 for Nikon round 10-pin bodies, N3 for Nikon MC-DC2 bodies, and equivalents for other brands). Photographers already carrying PW cables plug straight in. An optional 4-conductor `Laura-N10` cable for 10-pin bodies adds a Ready-pin line for full fire-confirmation.
- **Repeater** — optional bridging unit for venues where no LOS exists between TX and RX (Road America, Lime Rock). Same hardware as the RX; different firmware mode.

All three run on 2×AA (Eneloop NiMH or Linogy Li-ion). Expected battery life is days-to-weeks depending on duty cycle — far exceeding a race weekend.

## Key features

### Certainty

Every shot command is acknowledged end-to-end by the RX. TX has three LEDs:
- **Link** — packet got to the camera (directly or via repeater).
- **Fire** — the RX asserted the trigger contact. When the `Laura-N10` 4-conductor cable is plugged into the RX (Z9, Z8, D5 via round 10-pin), the RX additionally watches the Ready pin for an exposure transition, so this LED green means "shutter actually fired." With a standard PW-compatible 3-conductor cable, the Ready line isn't carried regardless of camera port, so this LED green means "command asserted at the camera" — one hop of certainty short. See Camera compatibility.
- **Repeater** (optional) — your packet went via a repeater rather than directly.

The radio link is the unreliable part of the chain; the ~20 cm of wire between the RX and the camera is not. In practice, link ACK is what matters most, and that's present on every supported body.

### Range

LoRa at 915 MHz with three adaptive **range tiers**: normal shots go at **short-range** (fast, ~50 ms round-trip), falling back automatically to **medium-range** and **long-range** on missed acknowledgment. Long-range adds significant link budget at the cost of a second or two of latency — acceptable for the worst-case shot, and you'll see in the log which tier the shot finally landed on.

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

**Live telemetry from each RX** piggy-backed on every ACK: RX battery voltage, RX internal temperature, Ready-pin state (10-pin bodies with `Laura-N10` cable only), RSSI of signals heard from the TX and repeater. Appears in the TX log against each event. You know the cam-side RX's battery was at 34% and 52 °C *at the moment the shot fired* without walking to the camera.

**Per-shot event log on the RX** too. Even events the TX never sees — keep-alive pulses, orphan packets, Ready-pin state transitions during the session. Dumped via USB-C post-race.

**Session-level statistics** — per-RX drop rate, RSSI trend, link-health snapshot — viewable on the TX OLED. Useful mid-race to notice "turn 1 cam is drifting" before it fully fails.

**Lightroom integration** — a post-session Python tool reads both logs, matches events against RAW files by timestamp + camera serial, and writes XMP sidecars with keywords (`remote-fired`, `cam-flagstand`, `tx-billy`, `via-repeater`) plus custom fields (RSSI, range tier, temp). Filter the Lightroom catalog by remote-triggered vs. handheld shots, or by which camera fired them.

### Clock

GPS module (u-blox MAX-M10) on the TX. Acquires UTC at power-on, runs the RTC from GPS time for the session, powers down. Matches the time reference used by Nikon's own GPS accessories, so TX logs and camera EXIF automatically align — no manual clock sync needed.

Coin-cell backup on the RTC so clock survives battery swaps.

### Pairing

Press and hold a button on the RX while the TX transmits a pair beacon. Network ID, device address, and encryption key exchanged in one handshake. No dipswitches, no menus, no serial numbers to write down.

## Camera compatibility

Compatibility is a function of the *cable* plugged into the RX's TRRS jack, not of the RX hardware. One RX build covers the full lineup.

| Body | Port | Cable | Fire confirmation | Notes |
| --- | --- | --- | --- | --- |
| Z9, Z8, D5 | Round 10-pin | `Laura-N10` (TRRS, 4-conductor) | Yes — Ready-pin transition observed | Full feature set. |
| Z9, Z8, D5 | Round 10-pin | PW N10 or equivalent (3-conductor) | No — Ready not carried. ACK reports `OK_CMD_SENT` | Works with existing PW-owner cables; degraded diagnostic only. |
| Z5, Z6, Z6II, Z6III, Z7, Z7II | MC-DC2 (rectangular 10-pin) | PW N3 or equivalent (3-conductor) | No — port doesn't expose a Ready signal. `OK_CMD_SENT` | Matches PW Plus III capability. |
| D780, D750, D7500, D7200, D5x00, D600/610, D90, Df, P1000 | MC-DC2 | PW N3 or equivalent (3-conductor) | No — same as above | Legacy DSLRs; mostly for reference. |
| Canon, Sony, other brands | Various | PW-compatible cable for that body (3-conductor) | No | Any PW-compatible pre-release cable works for fire/focus. Per-body testing pending. |
| Zf, Zfc, Z30, Z50, Z50II | USB-C only / Bluetooth ML-L7 | **Not supported in v1** | — | Would require a PTP over USB-C stack on the RX. Deferred to v2. |

The RX auto-detects cable type via a 4-pole/3-pole sense contact in the TRRS jack: if a 4-conductor plug is seated, Ready-pin logic is enabled; with a 3-conductor plug it's bypassed and the RX returns `OK_CMD_SENT` on shot commands regardless of camera.

### Dual-camera operation

A passive 3.5 mm Y-cable fans a single RX out to two cameras firing on the same signals (e.g., two Z6IIs on an oval, focus + shutter asserted simultaneously). Modern Nikon bodies present high-impedance inputs on both focus and shutter, so paralleling is safe. Laura sells (or users can build) such a cable; no firmware or RX-PCB changes required.

Caveats with dual-camera Y:
- Ready-pin confirmation can only be monitored for one body (the line from the second camera is left disconnected on the Y), so `OK_FIRED` reflects that camera only.
- Both cameras share the same keep-alive pulse cadence, which is usually the desired behavior.

### Practical note on the fire-confirmation asymmetry

Using a 3-conductor cable (standard PW or equivalent) means the RX can't independently verify "the camera fired," because the Ready-pin signal is simply not carried on those cables. But the short, reliable, physical wire from the RX to the camera is not the part of the system that's prone to failure — the radio link is. If the RX receives the command and asserts the trigger contact, the camera fires. The ACK still tells you unambiguously that the radio delivered the command. The only thing you lose is an independent sanity check for the ~0.1 % of cases where the camera was in an unreportable bad state (card full, mirror lock, firmware hang) — failures that will show up the moment you review footage anyway.

For typical usage patterns:
- **Z5 and Z6II on active remote** (most common per current fleet): `OK_CMD_SENT` ACK is operationally sufficient.
- **Z6III on auto-capture with keep-alive only**: fire confirmation isn't used in this mode anyway (the camera fires on its own interval), so the limitation is invisible.
- **Z8 and Z9 on active remote**: the `Laura-N10` cable gives full `OK_FIRED` confirmation including Ready-pin transition. A standard PW N10 also works but degrades to `OK_CMD_SENT`.

## Scenarios

**Sebring flag stand, turn 1 → flag stand:** TX at turn 1, RX on camera at flag stand, ~2,000 ft, 8-ft chain-link in the path. Direct link, medium-range tier. Green-green on every shot. Optional Yagi on TX for margin.

**Road America turn 1 → turn 14:** no LOS. Repeater on the flag stand (which sees both). Two-hop path. TX shows repeater-ACK + camera-ACK separately.

**Lime Rock uphill:** hill obstructs direct link. Repeater on the hill crest. Same two-hop pattern.

**Daytona oval, pre-race setup:** camera positioned three hours before the start of the race, mounted inside a chain-link perimeter. TX powered off during the wait. Before the race, operator hits `KA_ON`; RX wakes from CAD sniff, starts pulsing focus pin, acks back with camera state and battery. Camera stays alive until `KA_OFF`.

**Watkins Glen, two active remotes:** one at the Boot, one on the starter's stand. Both addressed individually. Half-press or shoot targets the selected cam. Group address fires both simultaneously if needed.

**Six-camera event, typical weekend:** 4 handhelds (no remote), 1 active remote (TX-operated), 1 auto-capture (keep-alive only). Single TX handles both remote modes. Post-event, dump logs from TX and both RXs, merge against Lightroom, filter cull by camera.

**Mixed Nikon bodies, current fleet:** A Z5 or Z6II on an RX with a PW N3 cable for the active-remote spot; a Z6III on an RX with a PW N3 cable in auto-capture/keep-alive mode; a Z8 or Z9 on an RX with a `Laura-N10` cable wherever fire-confirmation matters most. Every RX is the same hardware; the cable is the only thing that changes per body. Same TX controls all of them.

## System parts at a glance

| Part | Size | Power | Interfaces | UI |
| --- | --- | --- | --- | --- |
| TX | PW3-ish | 2×AA | SMA, USB-C | 128×64 OLED, 3 buttons, 3 LEDs |
| RX | Small (matchbox) | 2×AA | SMA, USB-C, 3.5 mm TRRS jack (PW-compatible + `Laura-N10`) | 128×32 OLED (optional), 3 LEDs, 1 button |
| Repeater | Same PCB as RX | 2×AA | SMA, USB-C (TRRS jack unpopulated) | 3 LEDs, 1 button |

Shared PCB between RX and repeater, populated differently per role. TX is its own board. All camera compatibility is handled by the choice of cable into the RX's TRRS jack — no RX hardware variants.

## Comparison to PocketWizard Plus III

| Feature | PW Plus III | Laura |
| --- | --- | --- |
| Radio | 344 MHz FHSS | 915 MHz LoRa |
| Range (LOS, typical) | ~1,600 ft | 2,000+ ft, with better penetration and repeater option |
| Confirmation of trigger | None | Bidirectional ACK; Ready-pin verification on 10-pin bodies (with `Laura-N10` cable), command-sent ACK otherwise |
| Repeater support | Yes (up to ~30 chained), but no per-leg ACK — failures are silent | Yes, one-hop, with separate repeater-ACK and camera-ACK so you know which leg broke |
| Camera-side cables | PW-specific | PW-compatible 3.5 mm jack; same PW cables + optional `Laura-N10` for Ready-pin |
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
- Nikon round 10-pin bodies (Z9, Z8, D5, D6, D850, D500, and siblings) via PW-compatible N10 cable (`OK_CMD_SENT`) or `Laura-N10` 4-conductor cable (full `OK_FIRED`).
- Nikon MC-DC2 bodies (Z5, Z6/Z6II/Z6III, Z7/Z7II, plus D780/D750/D7500/etc.) via PW-compatible N3 cable, with `OK_CMD_SENT` confirmation.
- Canon, Sony, and other brands where a PW-compatible pre-release cable already exists — fire/focus only, no Ready signal.
- Dual-camera Y-cable accessory for firing two bodies from a single RX on shared signals.
- Full feature set above regardless of cable choice.

**Not in v1:**
- USB-C / PTP-only bodies (Zf, Zfc, Z30, Z50, Z50II) — requires a PTP stack on the RX, much bigger firmware lift. Deferred to v2.
- Parasitic power from the camera's MC-DC2 +5 V supply. Would require a custom power-pass cable; deferred to v2 so that PW-compatible cables remain the primary interface.
- Flash sync (hot-shoe pickup). Skipped intentionally — EXIF flash metadata pollution isn't worth the marginal fire-confirmation improvement, particularly now that 3-conductor cables are the common case.

## Who it's for

Motorsports photographers who run multi-camera setups at large venues, need reliable remote triggering, and value observability. Assumes a baseline comfort with pairing devices, battery-swapping rechargeables, and occasionally running a Python script on a laptop post-event. Not a mass-market consumer product.

Initial users: Billy and Vic. Scale if it works.
