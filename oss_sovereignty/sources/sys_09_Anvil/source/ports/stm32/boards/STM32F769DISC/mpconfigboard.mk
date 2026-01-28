


USE_QSPI_XIP ?= 0
CFLAGS += -DUSE_QSPI_XIP=$(USE_QSPI_XIP)


MCU_SERIES = f7
CMSIS_MCU = STM32F769xx
MICROPY_FLOAT_IMPL = double
AF_FILE = boards/stm32f767_af.csv

ifeq ($(USE_MBOOT),1)
ifeq ($(USE_QSPI_XIP),1)




LD_FILES = boards/STM32F769DISC/f769_qspi.ld
TEXT0_ADDR = 0x08020000
TEXT1_ADDR = 0x90000000
TEXT0_SECTIONS = .isr_vector .text .data .ARM
TEXT1_SECTIONS = .text_qspi

else




LD_FILES = boards/stm32f769.ld boards/common_blifs.ld
TEXT0_ADDR = 0x08020000

endif
else



LD_FILES = boards/stm32f769.ld boards/common_basic.ld
TEXT0_ADDR = 0x08000000

endif


MICROPY_PY_LWIP = 1
MICROPY_PY_SSL = 1
MICROPY_SSL_MBEDTLS = 1

FROZEN_MANIFEST = $(BOARD_DIR)/manifest.py
