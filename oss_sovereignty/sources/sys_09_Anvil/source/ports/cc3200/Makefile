

BOARD ?= WIPY
ifeq ($(wildcard boards/$(BOARD)/.),)
$(error Invalid BOARD specified)
endif


BTYPE ?= release

BTARGET ?= application


PORT ?= /dev/ttyUSB1


BUILD ?= build/$(BOARD)/$(BTYPE)

include ../../py/mkenv.mk
-include ../../localconfig.mk

CROSS_COMPILE ?= arm-none-eabi-

CFLAGS_CORTEX_M4 = -mthumb -mtune=cortex-m4 -march=armv7e-m -mabi=aapcs -mcpu=cortex-m4 -msoft-float -mfloat-abi=soft -fsingle-precision-constant -Wdouble-promotion
CFLAGS += -Wall -Wpointer-arith -Werror -std=gnu99 -nostdlib $(CFLAGS_CORTEX_M4) -Os
CFLAGS += -g -ffunction-sections -fdata-sections -fno-common -fsigned-char -mno-unaligned-access
CFLAGS += -Iboards/$(BOARD)


CFLAGS += -Wno-array-bounds

LDFLAGS += -Wl,-nostdlib -Wl,--gc-sections -Wl,-Map=$@.map

FLASH_SIZE_WIPY = 2M
FLASH_SIZE_LAUNCHXL = 1M

ifeq ($(BTARGET), application)

QSTR_DEFS = qstrdefsport.h


MICROPY_ROM_TEXT_COMPRESSION ?= 1


include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk
include application.mk
else
ifeq ($(BTARGET), bootloader)
include bootmgr/bootloader.mk
else
$(error Invalid BTARGET specified)
endif
endif


include $(TOP)/py/mkrules.mk

erase:
	cc3200tool -p $(PORT) format_flash --size $(FLASH_SIZE_$(BOARD))

deploy:
	cc3200tool -p $(PORT) \
	    write_file bootmgr/build/$(BOARD)/$(BTYPE)/bootloader.bin /sys/mcuimg.bin \
	    write_file build/$(BOARD)/$(BTYPE)/mcuimg.bin /sys/factimg.bin



servicepack:
	cc3200tool -p $(PORT) \
	write_file --file-size=0x20000 --signature ota_1.0.1.6-2.7.0.0.ucf.signed.bin \
	    ota_1.0.1.6-2.7.0.0.ucf /sys/servicepack.ucf
