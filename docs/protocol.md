# Laura — Protocol Spec

Draft 0.1. This is the contract between hardware and firmware; PCB and firmware work downstream reference it.

## Goals

1. **Certainty** — every shot command is acknowledged end-to-end; TX always knows at minimum that the command reached the camera's RX, and for 10-pin bodies, that the shutter actually fired.
2. **Range** — work reliably at 2,000+ ft LOS at racetracks, through chain-link, and via a single-hop repeater where LOS doesn't exist.
3. **Observability** — every event logged on both sides with enough context to diagnose failures without guessing.
4. **Battery-polite** — RX and repeater duty-cycle via LoRa CAD so 2×AA lasts days.
5. **Coexistence** — multiple photographers on the same venue don't interfere.

## Hardware variants

Three roles, one RX hardware build:

| Role | PCB | Camera interface | Notes |
| --- | --- | --- | --- |
| TX | TX PCB | none | Handheld transmitter. GPS, OLED, buttons, LEDs. |
| RX | RX PCB | 3.5 mm TRRS jack | Accepts any PocketWizard-compatible pre-release cable (PW N10 / N3 / equivalents for other brands). An optional `Laura-N10` 4-conductor cable carries the Nikon round 10-pin Ready signal to enable full fire-confirmation. |
| Repeater | RX PCB | none (TRRS jack DNP) | Same PCB as RX. Output stage unpopulated. |

Camera support is determined by the cable, not the RX. The RX detects cable class via a 4-pole/3-pole sense contact in the TRRS jack: a 4-conductor plug (the `Laura-N10`) routes Ready-pin to an MCU input through a protection network, and firmware enables Ready-pin-dependent logic; a 3-conductor plug leaves the Ready input grounded through the sense contact, and firmware gates off Ready-pin logic and reports `OK_CMD_SENT` on shot commands regardless of camera body.

TRRS jack pinout (RX side):

| Conductor | Signal | Direction |
| --- | --- | --- |
| Tip | Shutter (full-press) | RX → camera, open-drain via opto |
| Ring 1 | Focus (half-press) | RX → camera, open-drain via opto |
| Ring 2 | Ready-pin (10-pin bodies only, `Laura-N10` cable only) | Camera → RX, pulled up on the RX side; camera drives the line according to its own Ready state (polarity per Nikon datasheet, firmware-configurable) |
| Sleeve | Ground (camera common) | — |

## Radio layer

- **Band**: US 902–928 MHz ISM. (EU variants would use 868 MHz with the same protocol.)
- **Modulation**: LoRa.
- **Bandwidth**: 125 kHz.
- **Coding rate**: 4/5.
- **Range tier**: adaptive. TX starts at *short* and falls back to *medium* then *long* on missed ACK. (These user-facing labels map to LoRa spreading factors SF7, SF10, and SF12 respectively; see the fallback ladder table below. Throughout the rest of this document, logs, UI text, and firmware APIs, the tier names are canonical and SF values are implementation detail.)
- **TX power**: up to +22 dBm (SX1262 max). Legal within US ISM duty-cycle rules at this power/bandwidth combination.
- **Preamble length**: standard (12 symbols) for short-tier packets; extended (long enough to cover RX CAD sniff interval) for commands targeting sleeping RXs (specifically `KA_ON` wake packets). Default sniff interval is 2 s, so wake preambles use a 3-second equivalent.

### Channels

Three channels in the ISM band:

| Channel | Center freq (MHz) |
| --- | --- |
| 0 | 903.9 |
| 1 | 911.9 |
| 2 | 919.9 |

Selected manually via TX mode button, or automatically cycled on repeated failure. Repeaters are pinned to channel 0 by default; TX falls back to channel 0 when it fails to reach the RX directly.

### Range tier fallback ladder

| Step | Tier | SF (impl.) | BW | Airtime (16-byte payload) | Sensitivity | Used for |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | `short` | SF7 | 125 kHz | ~50 ms | -123 dBm | Normal shots |
| 2 | `medium` | SF10 | 125 kHz | ~370 ms | -132 dBm | Retry after miss |
| 3 | `long` | SF12 | 125 kHz | ~1500 ms | -137 dBm | Last-resort retry |

Tier names (`short` / `medium` / `long`) are the canonical form in logs, ACK payloads, config, and UI. SF numbers are used only when directly configuring the LoRa modem.

## Packet format

All fields big-endian. Total header overhead: 8 bytes. Payload variable, 0–32 bytes. MIC: 4 bytes.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Network ID (16)         | Src Addr (8)  | Dst Addr (8)  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Seq#  (16)  |F|R|V|  CMD (5) |            Payload ...        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            Payload (variable)                 |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         MIC (32) — HMAC-SHA256, truncated      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Header fields

| Field | Size | Notes |
| --- | --- | --- |
| Network ID | 16 bits | Per-photographer. Set at pairing. Distinguishes Billy's system from Vic's. |
| Src Addr | 8 bits | Originator's address within the network. TX = 0x01 by convention; RXs = 0x10–0xFE. |
| Dst Addr | 8 bits | Target. 0xFF = broadcast. 0xE0–0xFE reserved for group addresses. |
| Seq# | 16 bits | Monotonically increasing per source. Used for dedupe and gap detection. |
| F (Fwd expected) | 1 bit | 1 = sender expects this packet to be forwarded by a repeater if one exists. Set on TX→RX commands. |
| R (Is-repeat) | 1 bit | 1 = this packet was retransmitted by a repeater. Set by the repeater before rebroadcast; loop prevention. |
| V (Version) | 1 bit | 0 for v1. Reserved for future wire-format breaks. |
| CMD | 5 bits | Command code, see below. |
| MIC | 32 bits | Truncated HMAC-SHA256 over everything preceding, keyed with the per-network shared key. |

### Address space

| Range | Meaning |
| --- | --- |
| 0x00 | Reserved / invalid |
| 0x01 | TX (primary; a second TX, if any, uses 0x02) |
| 0x10–0xDF | Receivers (up to ~200 per network) |
| 0xE0–0xFE | Group addresses (32 groups) |
| 0xFF | Broadcast |

Groups are assigned meaning by the operator at pairing time. Suggested convention: 0xE0 = all auto-captures, 0xE1 = all flag-stand cams, etc.

## Commands

| Code | Name | Direction | Payload | Notes |
| --- | --- | --- | --- | --- |
| 0x01 | `SHOOT` | TX → RX | none | Full-press shutter. Expects `RX_ACK`. |
| 0x02 | `HALF` | TX → RX | duration_ms (16) | Half-press focus pin for given duration. Expects `RX_ACK`. |
| 0x03 | `KA_ON` | TX → RX | interval_sec (8) | Begin autonomous keep-alive with given interval. Long preamble used. |
| 0x04 | `KA_OFF` | TX → RX | none | Cancel keep-alive. |
| 0x05 | `PING` | TX → RX | none | Link check + telemetry request. |
| 0x06 | `STATUS_QUERY` | TX → RX | none | Request extended status dump (more detail than ACK telemetry). |
| 0x07 | `PAIR_BEACON` | TX → broadcast | candidate_net_id, candidate_addr, pairing_nonce | Pairing handshake. See pairing flow. |
| 0x08 | `PAIR_ACCEPT` | RX → TX | confirmed_net_id, confirmed_addr | Pairing response. |
| 0x10 | `RX_ACK` | RX → TX | outcome_code, telemetry (see below) | Authoritative ACK for a TX command. |
| 0x11 | `REPEATER_ACK` | Repeater → TX | heard_seq#, repeater_battery_pct, repeater_battery_mv_scaled | Immediate "I got it, forwarding" from the repeater. Battery fields piggybacked so TX sees repeater battery on any relayed shot without needing a separate query. |
| 0x1F | `RESERVED` | — | — | — |

### Outcome codes (returned in `RX_ACK`)

| Code | Meaning |
| --- | --- |
| 0x00 | `OK_FIRED` — shutter asserted and Ready-pin transition observed (Ready-capable cable only) |
| 0x01 | `OK_KEEPALIVE_STARTED` — keep-alive armed, first pulse sent |
| 0x02 | `OK_KEEPALIVE_STOPPED` |
| 0x03 | `OK_PING` |
| 0x04 | `OK_CMD_SENT` — trigger contact asserted; RX has no Ready-pin feedback to independently confirm fire (3-conductor cable, or any cable on an MC-DC2 body) |
| 0x10 | `WARN_CMD_SENT_NO_FIRE_DETECTED` — Ready-capable cable only; we poked the pin but didn't see a Ready-pin transition (rare; long exposure or camera anomaly) |
| 0x20 | `ERR_NOT_READY` — Ready-capable cable only; Ready pin indicated not-ready before command, so camera was not ready |
| 0x21 | `ERR_CAMERA_UNRESPONSIVE` — Ready-capable cable only; keep-alive pulse issued but Ready state never transitioned to ready |
| 0x30 | `ERR_BAD_COMMAND` |
| 0x31 | `ERR_BAD_MIC` |

An RX with a 3-conductor cable plugged in cannot observe Ready-pin transitions — either because the cable doesn't carry the signal (PW N10 or equivalent into a 10-pin body) or because the camera port doesn't expose the signal in the first place (any cable into an MC-DC2 body). In either case, a successful `SHOOT` returns `OK_CMD_SENT`; the `OK_FIRED` / `ERR_NOT_READY` / `ERR_CAMERA_UNRESPONSIVE` / `WARN_*` codes are never generated. The TX UI maps `OK_CMD_SENT` to amber on the Fire LED (distinguishable from the green of `OK_FIRED`) so the operator can see at a glance which class of confirmation was achieved.

### Telemetry payload (included with every `RX_ACK`)

8 bytes fixed, extensible in later versions via a length byte:

| Byte | Field | Notes |
| --- | --- | --- |
| 0 | Battery % | 0–100. 0xFF = unknown. |
| 1 | Battery mV / 20 | Scaled so 0xFF = 5100 mV. |
| 2 | Temp °C | Signed, -128 to +127. STM32WL internal sensor. |
| 3 | Ready-pin observations | Ready-capable cable: bit 0 = Ready state at time of cmd (1 = ready), bit 1 = transition observed this cmd, bits 2–6 reserved, bit 7 = 0. 3-conductor cable (no Ready signal reaches the RX): bit 7 = 1, other bits undefined. |
| 4 | Last RSSI heard (TX) | Signed, dBm |
| 5 | Last RSSI heard (repeater) | Signed, dBm. 0x7F if no repeater heard recently. |
| 6 | Dedupe count | Number of duplicate packets (from direct + repeated) seen in last 32 events |
| 7 | Seq gap count | Number of sequence numbers the RX noticed missing recently |

## State machines

### TX state machine (per shot)

```
IDLE
  │  button press
  ▼
BUILD_PACKET (seq#, current range tier, MIC)
  │
  ▼
TRANSMIT
  │
  ▼
WAIT_REPEATER_ACK (timeout ~150 ms × tier-scale)
  │                                    │
  │ received                            │ timeout
  ▼                                     ▼
SET_LINK_LED_GREEN             (record: no repeater)
  │                                    │
  ▼                                    ▼
         WAIT_RX_ACK (timeout ~500 ms × tier-scale)
                    │                   │
                    │ received           │ timeout
                    ▼                   ▼
              SET_FIRE_LED        RETRY (escalate tier)
              DISPLAY OUTCOME         │
              LOG EVENT                └─► after 3 retries → LOG+RED
              IDLE
```

### RX state machine

```
DEEP_SLEEP
  │  CAD wake (every ~2s)
  ▼
CAD_CHECK (~2 ms)
  │                       │
  │ preamble found          │ nothing
  ▼                       ▼
RECEIVE                  DEEP_SLEEP
  │
  ▼
VALIDATE (net_id, MIC, dedupe)
  │       │
  │ bad    │ dup → drop silently
  ▼       ▼
DROP    PROCESS_CMD
          │
          ▼
        EXECUTE (opto pulse / arm KA / respond to ping)
          │
          ▼
        IF Ready-capable cable detected (4-pole plug, Ready line wired):
            OBSERVE_READY_PIN (timeout ~300 ms for fire detection)
            outcome = OK_FIRED | WARN_CMD_SENT_NO_FIRE_DETECTED | ERR_NOT_READY
        ELSE (3-pole plug, or Ready line not wired in cable):
            outcome = OK_CMD_SENT
          │
          ▼
        BUILD_RX_ACK (outcome + telemetry)
          │
          ▼
        TRANSMIT_ACK (via same range tier as received)
          │
          ▼
        LOG_EVENT
          │
          ▼
        DEEP_SLEEP (or KEEPALIVE_RUN if armed)
```

### Repeater state machine

```
LISTEN (continuous RX — default; CAD sniff available as a power-save option but not recommended for active use, see note below)
  │
  ▼
RECEIVE
  │
  ▼
VALIDATE (net_id we know? MIC good?)
  │            │
  │ no          │ yes
  ▼            ▼
DROP         IS_REPEAT_BIT_SET?
               │          │
               │ yes       │ no
               ▼          ▼
             DROP (loop) REBROADCAST with R=1
                         │
                         ▼
                       IF cmd expects ACK: send REPEATER_ACK back to src
                         │
                         ▼
                       LOG_EVENT
                         │
                         ▼
                       LISTEN
```

**Repeater LISTEN mode.** The repeater defaults to continuous RX (~5 mA draw, ~16 days on 2×AA). CAD sniff is available as a firmware option but imposes a ~3 s wake preamble on every forwarded packet (to guarantee the TX's transmission spans at least one sniff window). That penalty compounds with tier escalation — a shot that retries through all three range tiers could take 8+ seconds under CAD — which is incompatible with "press the shutter while the car is in the corner" use. Continuous RX is the right default; CAD is reserved for future standalone scenarios (unattended repeater left out for weeks).

## Pairing flow

1. User holds PAIR button on RX for >2 s. RX enters pairing mode, LED breathes blue, listens on channel 0, medium-range tier for 60 s.
2. User presses PAIR on TX. TX sends `PAIR_BEACON` with candidate (network_id, device_addr) plus a 16-byte nonce. Transmitted plaintext (this is the one exception; the MIC is computed with a well-known bootstrap key).
3. RX receives, performs ECDH-lite key derivation: final per-pair key = HMAC-SHA256(bootstrap_key, network_id || device_addr || nonce). Replies with `PAIR_ACCEPT` MIC'd under the new key.
4. TX validates `PAIR_ACCEPT`, both persist (network_id, device_addr, shared_key) to flash. LEDs on both sides flash green.

Simpler than real ECDH but adequate — the bootstrap key is shared across all Laura units, so an attacker within radio range during pairing could theoretically snoop the key. Mitigation: pair indoors, or accept the (very low) risk. Future versions can upgrade the bootstrap to real ECDH if the threat model warrants.

## Security

- **Per-network shared key** (256 bit), derived at pairing, stored in MCU flash.
- **MIC** on every packet: HMAC-SHA256 over (header + payload), truncated to 32 bits. 32-bit MIC is below modern crypto best practice, but is more than adequate here: the effective key space is channel × SF × sync word × 16-bit network ID × 32-bit MIC, and the actual threat model is accidental RF collision with another photographer's gear, not targeted packet forgery.
- **Replay protection**: RXs track the last 32 seq# per source per network and reject duplicates.
- **No OTA firmware updates** over the radio in v1. Firmware updates happen over USB-C.

## Channel hygiene

Every TX, RX, and repeater maintains a **non-Laura channel activity counter** — a running count of CAD hits (preamble-detect or energy-above-threshold events) that did *not* result in a successful Laura packet demod on the current channel. This surfaces non-Laura traffic competing for the band (LoRaWAN gateways, other ISM users, co-located industrial equipment).

- Counter resets at session start (detected by GPS-sync-acquired on the TX, or by boot on RX/repeater).
- RX and repeater include their counter in extended STATUS_QUERY responses.
- TX displays its own counter plus the maximum across all paired devices on the session-stats OLED screen.
- Events above a configurable rate (default: 10 non-Laura hits per minute sustained) are logged to the event log with a `WARN_NOISY_CHANNEL` event type.

Laura-vs-Laura detection (another photographer's Laura system on the same channel) is not surfaced as a counter, because coordinating photographers agree channels in advance and collision between unassociated Laura users is a non-issue in practice. If it becomes one, the existing MIC-fail and foreign-network-ID cases can be enumerated from the packet path trivially.

## Log format

### On-unit ring buffer (both TX and RX)

Fixed 20-byte records, flash-resident, newest-overwrites-oldest. TX buffer: 256 records (~5 KB). RX buffer: 256 records.

```
Offset  Size  Field
0       4     Unix UTC seconds (TX only; RX uses local monotonic counter with rollover mark)
4       2     Event type / command code
6       1     Counterparty address (target on TX, source on RX)
7       2     Sequence number
9       1     Outcome code
10      1     Final range tier used (0=short, 1=medium, 2=long)
11      1     Retry count
12      1     RSSI (signed dBm) of last heard return traffic
13      1     RSSI from repeater (signed dBm); 0x7F if N/A
14      1     RX battery % (from telemetry; TX's own battery for RX-side logs)
15      1     RX temp °C (signed)
16      1     Ready-pin state bits at time of event
17      3     Reserved
```

### USB-C dump format

JSONL (one JSON object per line). Each record self-describing, including field names; field-value dictionary drops the fixed 20-byte layout for readability.

```
{"t":"2026-05-17T14:32:11Z","type":"SHOOT","target":"flagstand","seq":4521,"outcome":"OK_FIRED","range":"short","retries":0,"rssi":-82,"rssi_rpt":null,"batt":82,"temp":48,"ready":"high→low→high"}
```

## Timing constants

| Name | Value | Notes |
| --- | --- | --- |
| `CAD_INTERVAL` | 2000 ms | RX sniff period |
| `CAD_DURATION` | 2 ms | Per-channel CAD check |
| `PREAMBLE_STANDARD` | 12 symbols | Normal commands |
| `PREAMBLE_WAKE` | 3000 ms equivalent | `KA_ON` and other commands targeting potentially-sleeping RXs |
| `REPEATER_ACK_TIMEOUT` | 150 ms × tier-scale | TX wait for repeater ack |
| `RX_ACK_TIMEOUT` | 500 ms × tier-scale | TX wait for RX ack |
| `FIRE_DETECTION_WINDOW` | 300 ms | RX waits for Ready-pin transition |
| `KA_DEFAULT_INTERVAL` | 20 s | Default keep-alive pulse interval |
| `KA_PULSE_DURATION` | 100 ms | Focus pin asserted per keep-alive tick |
| `MAX_RETRIES` | 3 | Per TX command |

"tier-scale" = multiply by (airtime at current tier / airtime at short tier).

## Post-session tooling

### `laura-dump`

Python script invoked against a USB-connected TX or RX. Reads the on-unit log and writes a JSONL file to local disk.

```
laura-dump --port /dev/tty.usbmodem123 --out tx-log-2026-04-20.jsonl
```

### `laura-merge`

Python script that takes a TX log, any number of RX logs, and a folder of RAW files. Matches events to files by timestamp + camera serial. Output:

- XMP sidecars next to each matched RAW, with keywords (`remote-fired`, `cam-<name>`, `tx-<name>`, optionally `via-repeater`) and custom fields (RSSI, range tier, retry count, RX temp, RX battery).
- A session report (`laura-report.txt`) listing: total shots commanded, fired, missed; orphan RAW files (no matching TX event); per-RX drop rate; RSSI trend per RX; session-level notes from the RX log (Ready-pin gaps, keep-alive anomalies).

### `laura-config`

Python script for on-device configuration over USB-C serial: set clock manually, list/add/remove paired RXs, assign human-readable names to addresses, change channel, dump/reset event log.

## Roadmap

**v1 (MVP, hardware + firmware):**
- TX + RX + repeater, fully featured per this spec.
- Post-session tooling (`laura-dump`, `laura-merge`, `laura-config`).

**Possibly v1.1:**
- Group-fire across multiple cameras.
- On-TX re-assignment of RX names without USB.

**Deferred to v2:**
- USB-C / PTP camera support (Zf, Zfc, Z30, Z50, Z50II — bodies with no wired remote port).
- Canon / Sony release cables.
- Real ECDH pairing.
- OTA firmware updates over radio.

**Speculative v3, pending viability testing:**
- **NX Field-like camera control over LoRa** — inventory, property get/set (ISO, shutter, aperture, WB, focus mode), remote trigger, thumbnail + EXIF download. Built on top of the v2 USB-C/PTP stack: the RX gains a USB-C host port to the camera and becomes a translator that turns LoRa command packets into PTP transactions. Not "IP over LoRa" — a Laura-protocol extension with new command codes, no network stack, no LoRaWAN, no tunneling. A laptop-side tool drives it over USB from the TX.
- **Prerequisite: viability test against a real NX Field deployment.** NX Field was designed around 1000base-T Ethernet; WiFi support was added late and reluctantly. It is not at all clear that NX Field's control-plane operations function on their own when live view is disabled — there may be hard throughput assumptions in discovery, heartbeat, or session management that make it unusable on a ~400 byte/sec sustained link. Before any firmware work, confirm empirically on an NX Field setup that inventory / config / trigger operations work standalone with live view off and a deliberately rate-limited network path (shape traffic to match LoRa's budget). If NX Field falls over without live view's bandwidth headroom, this direction is a dead end, and the alternative is a Laura-native PTP-over-LoRa protocol that bypasses the NX Field layer entirely.

## Open questions

1. **Duration of the `HALF` command default** — is 500 ms of focus-pin assertion appropriate, or should it be user-configurable per shot?
2. **Second TX support** — Vic and Billy both operating identical units within a shared network. Partly answered by separate network IDs, but group fires could benefit from coordination. Defer until both units exist.

## Resolved decisions

- **Repeater LISTEN mode** → continuous RX (not CAD). CAD's battery savings are real but the ~3 s wake-preamble penalty per shot (and its compounding under tier escalation) makes CAD incompatible with shooting racing where timing of the shutter press is precisely what matters. Continuous RX still gives ≥2 days on 2×AA, which exceeds the design target (see Battery targets, product-spec).
- **TX piezo buzzer** → not included. Track environments (unmuffled race cars + in-ear scanner) have no remaining audio attention; an audible cue would be inaudible and therefore misleading. Rely on LEDs and OLED only.
- **32-bit MIC** → kept at 32 bits. Combinatorial space (channel × SF × sync word × 16-bit network ID × 32-bit MIC) already makes casual-interference collision statistically negligible. Longer MIC would cost ~15 % airtime at long-tier for no practical gain against the actual threat model (nearby-photographer RF, not targeted attack).
