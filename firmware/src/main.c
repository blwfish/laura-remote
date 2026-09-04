/*
 * Toolchain sanity check — not yet Phase 1's radio echo (see docs/bringup.md).
 *
 * Confirms the Makefile + ARM GNU Toolchain + vendored ST HAL/CMSIS build
 * pipeline produces a valid image for the STM32WLE5JC before any
 * radio/UART/GPIO code is written against unverified pin assignments.
 * Deliberately touches no peripherals beyond HAL_Init(): that alone
 * exercises the startup file, linker script, and HAL/CMSIS vendoring,
 * without guessing at Wio-E5-mini-specific pin mappings (LED, USB-CDC
 * UART) that haven't been checked against Seeed's actual schematic yet.
 *
 * Next real step: bring in the SubGHz radio driver (firmware/vendor/subghz-phy,
 * ST's own driver — not hand-rolled) and the real Wio-E5-mini GPIO pin
 * mappings, per docs/bringup.md Phase 1.
 */

#include "stm32wlxx_hal.h"

int main(void)
{
    HAL_Init();

    while (1)
    {
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
