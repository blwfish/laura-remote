# Laura firmware

Bare-metal STM32WLE5JC firmware, built headlessly — no STM32CubeIDE, no
STM32CubeMX, no ST account. See [../docs/bringup.md](../docs/bringup.md)
("Toolchain" section) for why this exists alongside those GUI tools rather
than replacing their peripheral-configuration convenience.

Currently just a toolchain sanity check (`HAL_Init()`, empty loop) — see
[../docs/bringup.md](../docs/bringup.md) Phase 1 for what's next (radio
echo over the SubGHz peripheral, driven by ST's own radio driver in
`vendor/subghz-phy`, not hand-rolled).

## One-time setup

1. **Get the ARM GNU Toolchain** (compiler + newlib). Not Homebrew's
   `arm-none-eabi-gcc` formula — verified to ship GCC alone with no C
   library, so a build against it fails on a missing `stdint.h`. Instead:

   ```bash
   curl -LO https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz
   tar xf arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi.tar.xz
   xattr -dr com.apple.quarantine arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi
   export PATH="$PWD/arm-gnu-toolchain-14.2.rel1-darwin-arm64-arm-none-eabi/bin:$PATH"
   ```

   (Use the `linux-x86_64` or `mingw-w64-i686` asset instead of `darwin-arm64`
   on other hosts — same page, same no-account download.)

2. **Pull in the vendored ST/ARM sources** (git submodules — HAL driver,
   CMSIS device headers, generic CMSIS-Core, and the SubGHz radio driver):

   ```bash
   git submodule update --init firmware/vendor/cmsis-core \
     firmware/vendor/cmsis-device-wl \
     firmware/vendor/stm32wlxx-hal-driver \
     firmware/vendor/subghz-phy
   ```

## Build

```bash
cd firmware
make
```

Produces `build/laura-fw.elf` and prints a `size` summary. `make clean` to
reset.

## Flashing

Not yet wired up here — the Wio-E5 mini exposes a USB-CDC + ST-Link v2
composite device, so `st-flash`/`openocd` should work, but that needs
actual hardware in hand to verify against, unlike the build itself.
