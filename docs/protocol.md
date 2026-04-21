# Laura — Protocol Spec

Draft 0.1. This is the contract between hardware and firmware; PCB and firmware work downstream reference it.

## Goals

1. **Certainty** — every shot command is acknowledged end-to-end; TX always knows whether the camera fired.
2. **Range** — work reliably at 2,000+ ft LOS at racetracks, through chain-link, and via a single-hop repeater where LOS doesn't exist.
3. **Observability** — every event logged on both sides with enough context to diagnose failures without guessing.
4. **Battery-polite** — RX and repeater duty-cycle via LoRa CAD so 2×AA lasts days.
5. **Coexistence** — multiple photographers on the same venue don't interfere.

## Radio layer

- **Band**: US 902–928 MHz ISM. (EU variants would use 868 MHz with the same protocol.)
- **Modulation**: LoRa.
- **Bandwidth**: 125 kHz.
- **Coding rate**: 4/5.
- **Spreading factor**: adaptive. TX starts at SF7 and falls back to SF10 then SF12 on missed ACK.
- **TX power**: up to +22 dBm (SX1262 max). Legal within US ISM duty-cycle rules at this power/bandwidth combination.
- **Preamble length**: standard (12 symbols) for SF7 packets; extended (long enough to cover RX CAD sniff interval) for commands targeting sleeping RXs (specifically `KA_ON` wake packets). Default sniff interval is 2 s, so wake preambles use a 3-second equivalent.

### Channels

Three channels in the ISM band:

| Channel | Center freq (MHz) |
| --- | --- |
| 0 | 903.9 |
| 1 | 911.9 |
| 2 | 919.9 |

Selected manually via TX mode button, or automatically cycled on repeated failure. Repeaters are pinned to channel 0 by default; TX falls back to channel 0 when it fails to reach the RX directly.

### Spreading factor fallback ladder

| Step | SF | BW | Airtime (16-byte payload) | Sensitivity | Used for |
| --- | --- | --- | --- | --- | --- |
| 1 | SF7 | 125 kHz | ~50 ms | -123 dBm | Normal shots |
| 2 | SF10 | 125 kHz | ~370 ms | -132 dBm | Retry after miss |
| 3 | SF12 | 125 kHz | ~1500 ms | -137 dBm | Last-resort retry |

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
| 0x11 | `REPEATER_ACK` | Repeater → TX | heard_seq# | Immediate "I got it, forwarding" from the repeater. |
| 0x1F | `RESERVED` | — | — | — |

### Outcome codes (returned in `RX_ACK`)

| Code | Meaning |
| --- | --- |
| 0x00 | `OK_FIRED` — shutter asserted and Ready-pin transition observed |
| 0x01 | `OK_KEEPALIVE_STARTED` — keep-alive armed, first pulse sent, camera responded |
| 0x02 | `OK_KEEPALIVE_STOPPED` |
| 0x03 | `OK_PING` |
| 0x10 | `WARN_CMD_SENT_NO_FIRE_DETECTED` — we poked the pin but didn't see a Ready-pin transition (rare; long exposure or camera anomaly) |
| 0x20 | `ERR_NOT_READY` — Ready pin was low before command; camera not ready |
| 0x21 | `ERR_CAMERA_UNRESPONSIVE` — keep-alive pulse issued but Ready pin never rose |
| 0x30 | `ERR_BAD_COMMAND` |
| 0x31 | `ERR_BAD_MIC` |

### Telemetry payload (included with every `RX_ACK`)

8 bytes fixed, extensible in later versions via a length byte:

| Byte | Field | Notes |
| --- | --- | --- |
| 0 | Battery % | 0–100. 0xFF = unknown. |
| 1 | Battery mV / 20 | Scaled so 0xFF = 5100 mV. |
| 2 | Temp °C | Signed, -128 to +127. STM32WL internal sensor. |
| 3 | Ready-pin observations | bit 0: Ready at time of cmd; bit 1: transition observed this cmd; bits 2–7: reserved |
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
BUILD_PACKET (seq#, current SF, MIC)
  │
  ▼
TRANSMIT
  │
  ▼
WAIT_REPEATER_ACK (timeout ~150 ms × SF-scale)
  │                                    │
  │ received                            │ timeout
  ▼                                     ▼
SET_LINK_LED_GREEN             (record: no repeater)
  │                                    │
  ▼                                    ▼
         WAIT_RX_ACK (timeout ~500 ms × SF-scale)
                    │                   │
                    │ received           │ timeout
                    ▼                   ▼
              SET_FIRE_LED        RETRY (escalate SF)
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
        OBSERVE_READY_PIN (timeout ~300 ms for fire detection)
          │
          ▼
        BUILD_RX_ACK (outcome + telemetry)
          │
          ▼
        TRANSMIT_ACK (via same SF as received)
          │
          ▼
        LOG_EVENT
          │
          ▼
        DEEP_SLEEP (or KEEPALIVE_RUN if armed)
```

### Repeater state machine

```
LISTEN (continuous RX or CAD — firmware flag)
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

## Pairing flow

1. User holds PAIR button on RX for >2 s. RX enters pairing mode, LED breathes blue, listens on channel 0, SF10 for 60 s.
2. User presses PAIR on TX. TX sends `PAIR_BEACON` with candidate (network_id, device_addr) plus a 16-byte nonce. Transmitted plaintext (this is the one exception; the MIC is computed with a well-known bootstrap key).
3. RX receives, performs ECDH-lite key derivation: final per-pair key = HMAC-SHA256(bootstrap_key, network_id || device_addr || nonce). Replies with `PAIR_ACCEPT` MIC'd under the new key.
4. TX validates `PAIR_ACCEPT`, both persist (network_id, device_addr, shared_key) to flash. LEDs on both sides flash green.

Simpler than real ECDH but adequate — the bootstrap key is shared across all Laura units, so an attacker within radio range during pairing could theoretically snoop the key. Mitigation: pair indoors, or accept the (very low) risk. Future versions can upgrade the bootstrap to real ECDH if the threat model warrants.

## Security

- **Per-network shared key** (256 bit), derived at pairing, stored in MCU flash.
- **MIC** on every packet: HMAC-SHA256 over (header + payload), truncated to 32 bits. 32-bit MIC is below modern best practice, but packet sizes and airtime budgets make it the reasonable trade-off; the cost of forging a packet that triggers a camera is not high, but casual interference is the actual threat.
- **Replay protection**: RXs track the last 32 seq# per source per network and reject duplicates.
- **No OTA firmware updates** over the radio in v1. Firmware updates happen over USB-C.

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
10      1     Final SF used
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
{"t":"2026-05-17T14:32:11Z","type":"SHOOT","target":"flagstand","seq":4521,"outcome":"OK_FIRED","sf":"SF7","retries":0,"rssi":-82,"rssi_rpt":null,"batt":82,"temp":48,"ready":"high→low→high"}
```

## Timing constants

| Name | Value | Notes |
| --- | --- | --- |
| `CAD_INTERVAL` | 2000 ms | RX sniff period |
| `CAD_DURATION` | 2 ms | Per-channel CAD check |
| `PREAMBLE_STANDARD` | 12 symbols | Normal commands |
| `PREAMBLE_WAKE` | 3000 ms equivalent | `KA_ON` and other commands targeting potentially-sleeping RXs |
| `REPEATER_ACK_TIMEOUT` | 150 ms × SF-scale | TX wait for repeater ack |
| `RX_ACK_TIMEOUT` | 500 ms × SF-scale | TX wait for RX ack |
| `FIRE_DETECTION_WINDOW` | 300 ms | RX waits for Ready-pin transition |
| `KA_DEFAULT_INTERVAL` | 20 s | Default keep-alive pulse interval |
| `KA_PULSE_DURATION` | 100 ms | Focus pin asserted per keep-alive tick |
| `MAX_RETRIES` | 3 | Per TX command |

"SF-scale" = multiply by (airtime at current SF / airtime at SF7).

## Post-session tooling

### `laura-dump`

Python script invoked against a USB-connected TX or RX. Reads the on-unit log and writes a JSONL file to local disk.

```
laura-dump --port /dev/tty.usbmodem123 --out tx-log-2026-04-20.jsonl
```

### `laura-merge`

Python script that takes a TX log, any number of RX logs, and a folder of RAW files. Matches events to files by timestamp + camera serial. Output:

- XMP sidecars next to each matched RAW, with keywords (`remote-fired`, `cam-<name>`, `tx-<name>`, optionally `via-repeater`) and custom fields (RSSI, SF, retry count, RX temp, RX battery).
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
- USB-C / PTP camera support (Z6iii, Z6ii, Z5).
- Canon / Sony release cables.
- Real ECDH pairing.
- OTA firmware updates over radio.

## Open questions

1. **Duration of the `HALF` command default** — is 500 ms of focus-pin assertion appropriate, or should it be user-configurable per shot?
2. **Repeater LISTEN mode default** — continuous RX (better latency, ~5 mA draw) vs. CAD sniff (longer latency, ~200 µA draw)? User-configurable; defaulting to CAD seems right since battery life is the limit.
3. **Does the TX need a piezo buzzer for audible feedback on shots?** Cheap to add if useful; would let you fire without looking at the OLED.
4. **Second TX support** — Vic and Billy both operating identical units within a shared network. Partly answered by separate network IDs, but group fires could benefit from coordination. Defer until both units exist.
