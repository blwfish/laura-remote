# Laura firmware — Phase 1 sanity build, per docs/bringup.md.
#
# Requires the ARM GNU Toolchain (with newlib) on PATH — NOT Homebrew's
# `arm-none-eabi-gcc` formula, which ships GCC alone with no C library
# (verified: its `brew deps` pulls binutils/gmp/isl/libmpc/etc. but no
# newlib, and there is no separate `arm-none-eabi-newlib` formula either).
# Get the complete toolchain directly from ARM, no account needed:
#   https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
# and add its bin/ directory to PATH.
#
# Deliberately not STM32CubeIDE/STM32CubeMX: those are GUI-only tools with
# no headless equivalent, awkward on a machine you're not fully
# authenticated on. This Makefile + the vendored submodules under
# firmware/vendor/ (ST's own HAL, CMSIS device headers, and SubGHz radio
# driver — not hand-rolled) reach the same STM32WLE5JC target without one.

TARGET   = laura-fw
BUILD    = build

VENDOR   = vendor
CMSIS_CORE   = $(VENDOR)/cmsis-core
CMSIS_DEVICE = $(VENDOR)/cmsis-device-wl
HAL_DRIVER   = $(VENDOR)/stm32wlxx-hal-driver

CC       = arm-none-eabi-gcc
SIZE     = arm-none-eabi-size
OBJCOPY  = arm-none-eabi-objcopy

MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft

CFLAGS = $(MCU_FLAGS) -DSTM32WLE5xx -DUSE_HAL_DRIVER \
         -Iinc \
         -I$(CMSIS_CORE)/CMSIS/Core/Include \
         -I$(CMSIS_DEVICE)/Include \
         -I$(HAL_DRIVER)/Inc \
         -Wall -O0 -g -ffunction-sections -fdata-sections \
         -MMD -MP

LDFLAGS = $(MCU_FLAGS) -specs=nano.specs -Tlinker/STM32WLE5XX_FLASH.ld \
          -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

HAL_SRCS = stm32wlxx_hal stm32wlxx_hal_cortex stm32wlxx_hal_rcc \
           stm32wlxx_hal_rcc_ex stm32wlxx_hal_pwr stm32wlxx_hal_pwr_ex \
           stm32wlxx_hal_gpio stm32wlxx_hal_flash stm32wlxx_hal_flash_ex

SRCS = src/main.c src/system_stm32wlxx.c \
       $(addprefix $(HAL_DRIVER)/Src/,$(addsuffix .c,$(HAL_SRCS)))
ASM_SRCS = src/startup_stm32wle5xx.s

OBJS = $(addprefix $(BUILD)/,$(notdir $(SRCS:.c=.o))) \
       $(addprefix $(BUILD)/,$(notdir $(ASM_SRCS:.s=.o)))

vpath %.c $(sort $(dir $(SRCS)))
vpath %.s $(sort $(dir $(ASM_SRCS)))

.PHONY: all clean size

all: $(BUILD)/$(TARGET).elf size

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.s | $(BUILD)
	$(CC) $(MCU_FLAGS) -x assembler-with-cpp -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

size: $(BUILD)/$(TARGET).elf
	$(SIZE) $<

clean:
	rm -rf $(BUILD)

-include $(OBJS:.o=.d)
