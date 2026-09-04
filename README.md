# Laura

A LoRa-based remote trigger and keep-alive system for Nikon cameras, designed for motorsports photography at scale.

The goal, in one sentence: **press the button, know for sure the camera fired — from anywhere at the racetrack.**

## Docs

- [Product spec](docs/product-spec.md) — what it is, who it's for, what it does. Read this first.
- [Protocol spec](docs/protocol.md) — packet format, state machines, log formats. The contract between hardware and firmware.
- [Bring-up plan](docs/bringup.md) — how we get from zero to working prototype; dev board choice, phased approach, BOM, toolchain.

## Firmware

[firmware/](firmware/) — bare-metal STM32WLE5JC firmware, buildable headlessly (no STM32CubeIDE/CubeMX required — see `firmware/README.md`). Currently a toolchain sanity check only; Phase 1's radio echo is next.

## Status

No hardware yet. Firmware toolchain stood up and build-verified; still pre-Phase-1 (see bringup.md) — no protocol code written.
