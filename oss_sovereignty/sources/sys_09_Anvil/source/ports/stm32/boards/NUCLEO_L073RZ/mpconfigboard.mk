MCU_SERIES = l0
CMSIS_MCU = STM32L073xx
AF_FILE = boards/stm32l072_af.csv
LD_FILES = boards/stm32l072xz.ld boards/common_basic.ld


MICROPY_VFS_FAT = 0


FROZEN_MANIFEST ?=


LTO ?= 1
