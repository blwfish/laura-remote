# Laura — Bring-up Plan

How we get from "nothing" to "working prototype at a racetrack" without wasting money on PCB spins.

## Philosophy

This is not an ESP32 / Arduino-sketch project. The target MCU is STM32WLE5, used in production commercial LoRa gear; the development style is closer to professional embedded work than maker-scene tinkering:

- Bare-metal C (or C++) against the STM32 HAL/LL drivers.
- STM32CubeMX for peripheral init, a real IDE for the rest.
- SWD flashing and debugging (full breakpoints, register views, etc.), not serial-monitor println debugging.
- A pre-certified radio module so RF performance is predictable.
- No half-working Arduino core abstractions hiding what the radio is actually doing.

The tradeoff is a steeper initial ramp for anyone coming from the ESP32 world, in exchange for: lower power, smaller form factor, predictable behavior, and firmware that's portable unchanged from dev board to production PCB.

## Target hardware for bring-up

All firmware development happens on **Seeed Wio-E5 mini** dev boards. Each one has:

- The **Seeed LoRa-E5 module**, which contains the STM32WLE5JC die plus the RF frontend, crystals, and antenna switch on a pre-certified 12×12 mm package.
- USB-C with an onboard USB-to-UART+SWD bridge (no separate ST-Link needed for flashing or debugging).
- SMA antenna connector.
- All GPIOs broken out to 2.54 mm headers.
- Reset and boot buttons.

~$15 each. Buy 5. The LoRa-E5 module on the dev board is **the same part** that will be soldered onto the production RX and TX PCBs, so firmware developed on a Wio-E5 mini drops onto custom hardware unchanged.

### Why not Nucleo-WL55JC?

Nucleo is ST's official dev board and has its place (Arduino-shield-compatible headers, broader ecosystem reach). But for this project:
- The Nucleo uses a discrete STM32WL chip with ST's own RF layout, not the LoRa-E5 module. Firmware behavior is near-identical but pin maps and antenna-switch wiring differ, which means a small porting cost when we move to production.
- Larger, more expensive, less convenient for breadboarded peripherals.

Wio-E5 mini gets us onto the production silicon path from day one.

## Phased bring-up

### Phase 1 — Radio echo

Two Wio-E5 minis. One sends, one receives. No protocol yet — raw LoRa packets, just proving the radio works and we can set tier/channel/preamble parameters.

- Python CLI on the laptop writes a packet over USB-serial to the TX board, which transmits it.
- RX board receives, writes the payload back over USB-serial to the laptop.
- Both boards echo RSSI and SNR to the laptop.

Goal: confirm we can move bytes through the radio at all three range tiers, confirm CAD sniff works at the RX, measure actual latency per tier on the bench.

Duration: ~1 week.

### Phase 2 — Output stage on RX

Breadboarded TLP175A optos (one each for focus and shutter), wired to a 3.5 mm TRRS jack on the breadboard. PW-compatible pre-release cables (PW N10 for round 10-pin bodies, PW N3 for MC-DC2 bodies) plug from the TRRS jack to the bench camera. GPIOs on the RX Wio-E5 drive the opto LEDs; the opto FETs pull the camera's focus/shutter pins to ground.

- Test on a bench DSLR with standard PW cables first (see Bench cameras below).
- Build a `Laura-N10` 4-conductor cable: cut an aftermarket MC-30-style round 10-pin cable, terminate with a 4-pole TRRS plug, and route the Ready pin out to ring 2. Use this cable with a 10-pin body and verify the Ready-pin input sees the exposure transition on every shot.
- Verify the TRRS jack's 4-pole/3-pole sense contact behaves correctly: a standard 3-conductor plug should leave ring 2 grounded (firmware gates off Ready-pin logic); a 4-conductor `Laura-N10` plug should break that ground and expose Ready to the MCU input (firmware enables Ready-pin logic).
- For MC-DC2: test focus (half-press) and shutter (full-press) pulses only via PW N3; no Ready pin on that port regardless of cable.

Goal: RX board can reliably fire and (with `Laura-N10`) observe fire confirmation, and cable-class detection works.

Duration: ~1.5 weeks (slightly longer than the original estimate because the custom cable build is new).

### Phase 3 — Protocol and state machines

All of [protocol.md](protocol.md) implemented in firmware. Packet format, HMAC, sequence numbers, ACK round-trip, retries with tier escalation, dedupe, CAD sniff, keep-alive mode.

- TX firmware grows the full TX state machine.
- RX firmware grows the full RX state machine including cable-class detection and the Ready-capable vs 3-conductor branch.
- Add a third Wio-E5 as a repeater, test forwarding and two-ACK behavior.
- Exercise via Python CLI from the laptop until the full command set works over the radio.

Duration: ~2–3 weeks.

### Phase 4 — TX peripherals

Add to the TX Wio-E5 on breadboard:
- 128×64 SSD1306 I²C OLED (4 wires).
- Three tactile buttons with pull-ups.
- Three LEDs with current-limit resistors (link, fire, repeater).
- u-blox MAX-M10 GPS breakout board (UART + enable + power).

Firmware gets a real UI: live view, review mode, event log, GPS sync at power-on.

Duration: ~1 week.

### Phase 5 — Power validation

Drop the Wio-E5 mini's onboard 3.3 V regulator, power it from a breadboarded 2×AA + TI TPS61023 eval module instead. Measure current consumption in each state (idle, CAD sniff, continuous RX for repeater role, TX burst, RX with peripherals). Validate against the product-spec battery targets: **1 day floor, 2 days design target, 3 days cap**. Key numbers to confirm: repeater continuous-RX draw ≤ ~5 mA (gives ≥16 days on 2×AA — comfortable 2-day target), RX CAD-sniff average ≤ ~250 µA, TX active-use average under budget for a 12-hour race day + standby overnight.

Duration: ~2 days.

### Phase 6 — Field test at range

Pack two Wio-E5-based prototypes (one acting as TX, one as RX) in sandwich containers and head to a large parking lot or open field. Test:

- Direct link distance, short tier, until packets start dropping.
- Fallback to medium and long tiers; verify the TX UI shows which tier eventually succeeded.
- Repeater placement scenarios: TX + repeater-blocked-by-obstacle + RX.
- Different antennas: whip, half-wave dipole, small Yagi on TX.
- Chain-link fence in the path (home-improvement store sells 8-ft panels if no fence is nearby).

Goal: discover antenna and range issues before they are baked into a custom PCB.

Duration: 1 day + iteration.

### Phase 7 — Custom PCB design

Only now do we design the custom RX and TX PCBs. The LoRa-E5 module footprint drops into KiCad; all the other circuit blocks have been validated on breadboard.

- RX PCB first (single build; TRRS jack with 4-pole sense handles all cable classes, and the repeater role is covered by leaving the output stage / TRRS jack DNP).
- TX PCB second.
- Send both out for fabrication + assembly at JLCPCB or PCBWay.
- Flash with firmware already working on Wio-E5s.

Duration: ~3–4 weeks including fabrication turnaround.

### Phase 8 — Track test

Take the first working custom units to an actual track. Sebring or Lime Rock, one or two cameras, a race weekend. Everything after this is iteration on issues found in the real environment.

## Bench cameras

Never bring up on working cameras. Old D-bodies on eBay are adequate and cost less than a PCB spin.

| Port | Confirmed available | If not, buy |
| --- | --- | --- |
| Round 10-pin | **Vic's D300** | D200 / D300 / D700 on eBay, typically $100–200 |
| MC-DC2 | TBD (ask Vic about D90 / D7000 / D780) | D90 on eBay, typically $50 |

Between Vic's D300 and whatever MC-DC2 body turns up, both cable classes (`Laura-N10` with Ready-pin and standard 3-conductor PW-compatible) can be bench-tested without ever plugging into a current-fleet camera.

## Bring-up BOM

| Item | Qty | Approx cost | Notes |
| --- | --- | --- | --- |
| Seeed Wio-E5 mini dev board | 5 | $75 | Three for TX/RX/repeater roles, two spares |
| Solderless breadboard kit | 1 | $25 | |
| TLP175A opto-isolators | 10 | $15 | Multiple for iteration |
| PW-compatible N3 cable (MC-DC2) | 2 | $30 | PW CM-N3 or aftermarket equivalent; used as-is for MC-DC2 bench testing |
| PW-compatible N10 cable (round 10-pin) | 1 | $25 | PW CM-N10 or aftermarket; used as-is for 10-pin fire/focus testing without Ready |
| MC-30-equivalent aftermarket cable (to cut for `Laura-N10`) | 2 | $50 | Terminate with 4-pole TRRS plug; route Ready pin to ring 2 |
| 3.5 mm TRRS jack with 4-pole sense contact | 5 | $15 | e.g. Switchcraft 35RAPC4BV3 or CUI SJ-43615; breadboard-mountable |
| 4-pole 3.5 mm TRRS plugs (solderable) | 5 | $10 | For `Laura-N10` cable assembly |
| 3.5 mm stereo Y-cable | 2 | $10 | For dual-camera testing |
| u-blox MAX-M10 GPS breakout | 1 | $20 | Sparkfun / Adafruit / Seeed |
| 128×64 SSD1306 OLED (I²C) | 2 | $10 | |
| TPS61023EVM boost eval | 1 | $25 | For phase 5 power validation |
| Used MC-DC2 body | 1 | $0–80 | Confirm with Vic first |
| Used 10-pin body | 0 | $0 | Vic has a D300 |
| 915 MHz SMA whip antennas (¼-wave) | 4 | $20 | |
| 3-element 915 MHz Yagi | 1 | $25 | For phase 6 range testing |
| 2×AA battery holders with leads | 5 | $10 | |
| Eneloop AA NiMH (4×AA pack) | 1 | $20 | Pro-pack ideally |
| Misc: R, C, tactile buttons, LEDs, headers, wire | — | $30 | |

**Total: ~$415–495** depending on MC-DC2 body availability.

One PCB spin (RX + TX, small quantity, assembled) is ~$500 by itself. Bring-up on dev boards pays for itself before the first PCB order.

## Development environment

### Toolchain

- **STM32CubeIDE** (free, ST's official IDE, based on Eclipse) *or* **PlatformIO** with the STM32 platform installed (lighter-weight, VS-Code-based, arguably better DX for anyone already in VS Code).
- Either one supports flash-and-debug via the Wio-E5 mini's onboard USB-to-SWD bridge. No external ST-Link needed.
- **STM32CubeMX** for generating peripheral init code (clocks, GPIO, UART, SPI, I²C, RTC, LoRa SubGHz). Regenerates without overwriting user code regions.

### Language and abstractions

- **C** for the core firmware. C++ is fine if the team prefers it; no strong reason either way.
- **STM32 HAL** (high-level) for most peripherals — good enough and self-documenting.
- **STM32 LL** (low-level) for any tight loops or ISRs where HAL overhead matters (probably just the LoRa IRQ path).
- **No RTOS** in v1. Single cooperative main loop with state machines. The workload doesn't need threading, and an RTOS adds complexity to the power-management story (sleep/wake timing matters a lot when you're counting microamps on 2×AA).

### Flashing and debugging

- Wio-E5 mini appears as a USB CDC serial + ST-Link v2 composite device.
- Set breakpoints in the IDE, inspect registers, read memory live.
- `printf` over USB serial via HAL UART for event logging during bring-up. The ring-buffer log format in [protocol.md](protocol.md) is what ships; printf is just bench convenience.

### What we explicitly avoid

- **Arduino IDE / Arduino-for-STM32.** Works, but hides exactly the things we need to tune (radio timings, sleep modes, clock config). Not worth the short-term ergonomics win.
- **ESP32 + external RFM95W / SX1262.** The option that most hobby LoRa projects take. Rejected because ESP32 idle/sleep current and the whole WiFi/BT infrastructure are overkill for 2×AA battery targets — the project started assuming ESP32-on-Heltec but pivoted to STM32WL specifically for power reasons.
- **LoRaWAN.** LoRa the modulation ≠ LoRaWAN the network protocol. LoRaWAN is designed for sensor networks uploading to gateways, not for point-to-point command-and-control. Using it here would impose unwanted structure and introduce third-party network dependencies. We use raw LoRa packets with our own protocol.
- **OTA firmware updates over the radio.** Out of scope for v1. Firmware updates happen over USB-C, as specified in the protocol spec's roadmap.

## References

- [STM32WLE5JC datasheet](https://www.st.com/resource/en/datasheet/stm32wle5jc.pdf)
- [Seeed LoRa-E5 module wiki](https://wiki.seeedstudio.com/LoRa_E5_STM32WLE5JC_Module/)
- [Seeed Wio-E5 mini dev board wiki](https://wiki.seeedstudio.com/LoRa_E5_mini/)
- [u-blox MAX-M10 datasheet](https://www.u-blox.com/en/product/max-m10-series)
- [TLP175A opto-isolator datasheet](https://toshiba.semicon-storage.com/us/semiconductor/product/photocouplers-photorelays/detail.TLP175A.html)
- [TPS61023 boost converter datasheet](https://www.ti.com/product/TPS61023)
