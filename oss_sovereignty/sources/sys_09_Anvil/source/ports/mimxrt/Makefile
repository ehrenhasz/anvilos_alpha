




ifdef BOARD_DIR


BOARD ?= $(notdir $(BOARD_DIR:/=))
else

BOARD ?= TEENSY40
BOARD_DIR ?= boards/$(BOARD)
endif

ifeq ($(wildcard $(BOARD_DIR)/.),)
    $(error Invalid BOARD specified: $(BOARD_DIR))
endif

BUILD ?= build-$(BOARD)
PORT ?= /dev/ttyACM0
CROSS_COMPILE ?= arm-none-eabi-
GIT_SUBMODULES += lib/tinyusb lib/nxp_driver


MICROPY_VFS_LFS2 ?= 1
MICROPY_VFS_FAT ?= 1


QSTR_DEFS += qstrdefsport.h
QSTR_GLOBAL_DEPENDENCIES = $(BOARD_DIR)/mpconfigboard.h


MAKE_PINS = boards/make-pins.py
MAKE_FLEXRAM_LD = boards/make-flexram-config.py


include ../../py/mkenv.mk


include $(BOARD_DIR)/mpconfigboard.mk


MICROPY_ROM_TEXT_COMPRESSION ?= 1


FROZEN_MANIFEST ?= boards/manifest.py


include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk


MCU_DIR = lib/nxp_driver/sdk/devices/$(MCU_SERIES)


LD_FILES = boards/$(MCU_SERIES).ld boards/common.ld


AF_FILE = boards/$(MCU_SERIES)_af.csv
BOARD_PINS = $(BOARD_DIR)/pins.csv
PREFIX_FILE = boards/mimxrt_prefix.c
GEN_FLEXRAM_CONFIG_SRC = $(BUILD)/flexram_config.s
GEN_PINS_HDR = $(HEADER_BUILD)/pins.h
GEN_PINS_SRC = $(BUILD)/pins_gen.c





INC += -I$(BOARD_DIR)
INC += -I$(BUILD)
INC += -I$(TOP)
INC += -I$(TOP)/$(MCU_DIR)
INC += -I$(TOP)/$(MCU_DIR)/drivers
INC += -I$(TOP)/lib/cmsis/inc
INC += -I$(TOP)/lib/oofatfs
INC += -I$(TOP)/lib/tinyusb/hw
INC += -I$(TOP)/lib/tinyusb/hw/bsp/teensy_40
INC += -I$(TOP)/lib/tinyusb/src
INC += -I.
INC += -Ihal


ifeq ($(MICROPY_PY_LWIP),1)
INC += -Ilwip_inc
INC += -Ihal/phy
endif






SRC_TINYUSB_C += \
	lib/tinyusb/src/class/cdc/cdc_device.c \
	lib/tinyusb/src/class/dfu/dfu_rt_device.c \
	lib/tinyusb/src/class/hid/hid_device.c \
	lib/tinyusb/src/class/midi/midi_device.c \
	lib/tinyusb/src/class/msc/msc_device.c \
	lib/tinyusb/src/class/usbtmc/usbtmc_device.c \
	lib/tinyusb/src/class/vendor/vendor_device.c \
	lib/tinyusb/src/common/tusb_fifo.c \
	lib/tinyusb/src/device/usbd.c \
	lib/tinyusb/src/device/usbd_control.c \
	lib/tinyusb/src/portable/chipidea/ci_hs/dcd_ci_hs.c \
	lib/tinyusb/src/tusb.c


ifeq ($(MICROPY_PY_LWIP),1)
SRC_ETH_C += \
	$(MCU_DIR)/drivers/fsl_enet.c \
	hal/phy/device/phydp83825/fsl_phydp83825.c \
	hal/phy/device/phydp83848/fsl_phydp83848.c \
	hal/phy/device/phyksz8081/fsl_phyksz8081.c \
	hal/phy/device/phylan8720/fsl_phylan8720.c \
	hal/phy/device/phyrtl8211f/fsl_phyrtl8211f.c \
	hal/phy/mdio/enet/fsl_enet_mdio.c
endif


SRC_HAL_IMX_C += \
	$(MCU_DIR)/drivers/fsl_clock.c \
	$(MCU_DIR)/drivers/fsl_common.c \
	$(MCU_DIR)/drivers/fsl_dmamux.c \
	$(MCU_DIR)/drivers/fsl_edma.c \
	$(MCU_DIR)/drivers/fsl_flexram.c \
	$(MCU_DIR)/drivers/fsl_flexspi.c \
	$(MCU_DIR)/drivers/fsl_gpc.c \
	$(MCU_DIR)/drivers/fsl_gpio.c \
	$(MCU_DIR)/drivers/fsl_gpt.c \
	$(MCU_DIR)/drivers/fsl_lpi2c.c \
	$(MCU_DIR)/drivers/fsl_lpspi.c \
	$(MCU_DIR)/drivers/fsl_lpspi_edma.c \
	$(MCU_DIR)/drivers/fsl_lpuart.c \
	$(MCU_DIR)/drivers/fsl_pit.c \
	$(MCU_DIR)/drivers/fsl_pwm.c \
	$(MCU_DIR)/drivers/fsl_sai.c \
	$(MCU_DIR)/drivers/fsl_snvs_hp.c \
	$(MCU_DIR)/drivers/fsl_snvs_lp.c \
	$(MCU_DIR)/drivers/fsl_wdog.c \
	$(MCU_DIR)/system_$(MCU_SERIES)$(MCU_CORE).c \


ifeq ($(MCU_SERIES), MIMXRT1062)
SRC_HAL_IMX_C += hal/fsl_flexspi_nor_boot.c
else
SRC_HAL_IMX_C += $(MCU_DIR)/xip/fsl_flexspi_nor_boot.c
endif

ifeq ($(MICROPY_HW_SDRAM_AVAIL),1)
SRC_HAL_IMX_C += $(MCU_DIR)/drivers/fsl_semc.c
endif

ifeq ($(MCU_SERIES),$(filter $(MCU_SERIES), MIMXRT1021 MIMXRT1052 MIMXRT1062 MIMXRT1064 MIMXRT1176))
SRC_HAL_IMX_C += $(MCU_DIR)/drivers/fsl_usdhc.c
endif

ifeq ($(MCU_SERIES),$(filter $(MCU_SERIES), MIMXRT1015 MIMXRT1021 MIMXRT1052 MIMXRT1062 MIMXRT1064 MIMXRT1176))
SRC_HAL_IMX_C += \
	$(MCU_DIR)/drivers/fsl_qtmr.c \
	$(MCU_DIR)/drivers/fsl_romapi.c
endif

ifeq ($(MCU_SERIES), MIMXRT1176)
INC += -I$(TOP)/$(MCU_DIR)/drivers/cm7

SRC_HAL_IMX_C += \
	$(MCU_DIR)/drivers/cm7/fsl_cache.c \
	$(MCU_DIR)/drivers/fsl_dcdc.c \
	$(MCU_DIR)/drivers/fsl_pmu.c \
	$(MCU_DIR)/drivers/fsl_common_arm.c \
	$(MCU_DIR)/drivers/fsl_anatop_ai.c \
	$(MCU_DIR)/drivers/fsl_caam.c \
	$(MCU_DIR)/drivers/fsl_lpadc.c \
	$(MCU_DIR)/drivers/fsl_mu.c
else
SRC_HAL_IMX_C += \
	$(MCU_DIR)/drivers/fsl_adc.c \
	$(MCU_DIR)/drivers/fsl_cache.c \
	$(MCU_DIR)/drivers/fsl_trng.c
endif


SRC_C += \
	board_init.c \
	boards/$(MCU_SERIES)_clock_config.c \
	dma_manager.c \
	drivers/bus/softspi.c \
	drivers/dht/dht.c \
	eth.c \
	fatfs_port.c \
	flash.c \
	hal/pwm_backport.c \
	help.c \
	led.c \
	machine_bitstream.c \
	machine_i2c.c \
	machine_led.c \
	machine_pin.c \
	machine_rtc.c \
	machine_sdcard.c \
	machine_spi.c \
	main.c \
	mbedtls/mbedtls_port.c \
	mimxrt_flash.c \
	mimxrt_sdram.c \
	modmimxrt.c \
	mphalport.c \
	mpnetworkport.c \
	network_lan.c \
	pendsv.c \
	pin.c \
	sdcard.c \
	sdio.c \
	systick.c \
	ticks.c \
	tusb_port.c \

SHARED_SRC_C += \
	shared/libc/printf.c \
	shared/libc/string0.c \
	shared/netutils/dhcpserver.c \
	shared/netutils/netutils.c \
	shared/netutils/trace.c \
	shared/readline/readline.c \
	shared/runtime/gchelper_native.c \
	shared/runtime/interrupt_char.c \
	shared/runtime/mpirq.c \
	shared/runtime/pyexec.c \
	shared/runtime/softtimer.c \
	shared/runtime/stdout_helpers.c \
	shared/runtime/sys_stdio_mphal.c \
	shared/timeutils/timeutils.c \


ifeq ($(MICROPY_HW_FLASH_TYPE),$(filter $(MICROPY_HW_FLASH_TYPE),qspi_nor_flash))
    MICROPY_HW_FLASH_BASE = 0x60000000
    FLEXSPI_FLASH_TYPE = $(MICROPY_HW_FLASH_TYPE)
else ifeq ($(MICROPY_HW_FLASH_TYPE),$(filter $(MICROPY_HW_FLASH_TYPE),qspi_hyper_flash))
    MICROPY_HW_FLASH_BASE = 0x60000000
    FLEXSPI_FLASH_TYPE = $(MICROPY_HW_FLASH_TYPE)
else ifeq ($(MICROPY_HW_FLASH_TYPE),$(filter $(MICROPY_HW_FLASH_TYPE),internal))
    
    MICROPY_HW_FLASH_BASE = 0x70000000
    FLEXSPI_FLASH_TYPE = qspi_nor_flash
    CFLAGS += -DMICROPY_HW_FLASH_INTERNAL
else
    $(error Error: Unknown board flash type $(MICROPY_HW_FLASH_TYPE))
endif



SRC_HAL_C += hal/flexspi_$(subst qspi_,,$(FLEXSPI_FLASH_TYPE)).c


ifeq ($(MICROPY_HW_BOARD_FLASH_FILES),1)
    SRC_HAL_C += $(BOARD_DIR)/$(FLEXSPI_FLASH_TYPE)_config.c
else
    SRC_HAL_C += hal/$(FLEXSPI_FLASH_TYPE)_config.c
endif

ifeq ($(MICROPY_PY_BLUETOOTH),1)
SRC_C += mpbthciport.c
endif 

ifeq ($(MICROPY_BLUETOOTH_NIMBLE),1)
SRC_C += mpnimbleport.c
endif



ifeq ($(MICROPY_PY_OPENAMP),1)
SRC_C += mpmetalport.c
$(BUILD)/mpmetalport.o: $(BUILD)/openamp/metal/config.h
ifeq ($(MICROPY_PY_OPENAMP_REMOTEPROC),1)
SRC_C += mpremoteprocport.c
$(BUILD)/mpremoteprocport.o: $(BUILD)/openamp/metal/config.h
endif
endif


ifeq ($(MICROPY_FLOAT_IMPL),double)
    LIBM_SRC_C += $(SRC_LIB_LIBM_DBL_C)
    
    ifeq ($(SUPPORTS_HARDWARE_FP_DOUBLE),1)
        LIBM_SRC_C += $(SRC_LIB_LIBM_DBL_SQRT_HW_C)
    else
        LIBM_SRC_C += $(SRC_LIB_LIBM_DBL_SQRT_SW_C)
    endif
else
    LIBM_SRC_C += $(SRC_LIB_LIBM_C)
    ifeq ($(SUPPORTS_HARDWARE_FP_SINGLE),1)
        LIBM_SRC_C += $(SRC_LIB_LIBM_SQRT_HW_C)
    else
        LIBM_SRC_C += $(SRC_LIB_LIBM_SQRT_SW_C)
    endif
endif


SUPPORTS_HARDWARE_FP_SINGLE = 0
SUPPORTS_HARDWARE_FP_DOUBLE = 0


SRC_SS = \
	$(MCU_DIR)/gcc/startup_$(MCU_SERIES)$(MCU_CORE).S \
	hal/resethandler_MIMXRT10xx.S

SRC_S += shared/runtime/gchelper_thumb2.s \






SRC_QSTR += $(SRC_C) $(SHARED_SRC_C) $(GEN_PINS_SRC)





CFLAGS += -g  
ifeq ($(DEBUG),1)
CFLAGS += -Og

MICROPY_ROM_TEXT_COMPRESSION = 0
else
CFLAGS += -Os -DNDEBUG
endif


MICROPY_HW_SDRAM_AVAIL ?= 0
MICROPY_HW_SDRAM_SIZE ?= 0


CFLAGS += \
	$(INC) \
	-D__START=main \
	-D__STARTUP_CLEAR_BSS \
	-D__STARTUP_INITIALIZE_RAMFUNCTION \
	-DBOARD_$(BOARD) \
	-DBOARD_FLASH_SIZE=$(MICROPY_HW_FLASH_SIZE) \
	-DCFG_TUSB_MCU=OPT_MCU_MIMXRT1XXX \
	-DCLOCK_CONFIG_H='<boards/$(MCU_SERIES)_clock_config.h>' \
	-DCPU_$(MCU_SERIES)$(MCU_CORE) \
	-DCPU_$(MCU_VARIANT) \
	-DCPU_HEADER_H='<$(MCU_SERIES)$(MCU_CORE).h>' \
	-DFSL_SDK_ENABLE_DRIVER_CACHE_CONTROL=1 \
	-DI2C_RETRY_TIMES=1000000 \
	-DMICROPY_HW_FLASH_SIZE=$(MICROPY_HW_FLASH_SIZE) \
	-DMICROPY_HW_SDRAM_AVAIL=$(MICROPY_HW_SDRAM_AVAIL) \
	-DMICROPY_HW_SDRAM_SIZE=$(MICROPY_HW_SDRAM_SIZE) \
	-DSPI_RETRY_TIMES=1000000 \
	-DUART_RETRY_TIMES=1000000 \
	-DXIP_BOOT_HEADER_ENABLE=1 \
	-DXIP_EXTERNAL_FLASH=1 \
	-fdata-sections \
	-ffunction-sections \
	-mcpu=cortex-m7 \
	-mthumb \
	-mtune=cortex-m7 \
	-nostdlib \
	-std=c99 \
	-Wall \
	-Wdouble-promotion \
	-Werror \
	-Wfloat-conversion \
	-Wno-error=unused-parameter



CFLAGS += -DBOARD_FLASH_OPS_HEADER_H=\"hal/flexspi_$(subst qspi_,,$(FLEXSPI_FLASH_TYPE)).h\"


ifeq ($(MICROPY_HW_BOARD_FLASH_FILES),1)
    CFLAGS += -DBOARD_FLASH_CONFIG_HEADER_H=\"$(BOARD)_flexspi_flash_config.h\"
else
    CFLAGS += -DBOARD_FLASH_CONFIG_HEADER_H=\"hal/flexspi_flash_config.h\"
endif


ifeq ($(MICROPY_FLOAT_IMPL),single)
    CFLAGS += \
        -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_FLOAT \
        -fsingle-precision-constant \
        -mfloat-abi=softfp \
        -mfpu=fpv5-sp-d16
else ifeq ($(MICROPY_FLOAT_IMPL),double)
    CFLAGS += \
        -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_DOUBLE \
        -mfloat-abi=hard \
        -mfpu=fpv5-d16
else ifeq ($(MICROPY_FLOAT_IMPL),none)
    CFLAGS += \
        -DMICROPY_FLOAT_IMPL=MICROPY_FLOAT_IMPL_NONE
else
    $(error Error: Unknown floating point implementation $(MICROPY_FLOAT_IMPL))
endif



ifeq ($(MICROPY_PY_LWIP),1)
CFLAGS += \
	-DFSL_FEATURE_PHYKSZ8081_USE_RMII50M_MODE=1
endif

ifdef MICROPY_HW_FLASH_CLK
	CFLAGS += -DMICROPY_HW_FLASH_CLK=$(MICROPY_HW_FLASH_CLK)
endif

CFLAGS += $(CFLAGS_EXTRA)

MPY_CROSS_FLAGS += -march=armv7m





LDFLAGS += \
	--cref \
	--gc-sections \
	--print-memory-usage \
	-Map=$@.map




LDDEFINES = \
	-DMICROPY_HW_FLASH_BASE=$(MICROPY_HW_FLASH_BASE) \
	-DMICROPY_HW_FLASH_SIZE=$(MICROPY_HW_FLASH_SIZE)

ifdef MICROPY_HW_FLASH_RESERVED
LDDEFINES += -DMICROPY_HW_FLASH_RESERVED=$(MICROPY_HW_FLASH_RESERVED)
endif

ifdef MICROPY_HW_SDRAM_AVAIL
LDDEFINES += \
	-DMICROPY_HW_SDRAM_AVAIL=$(MICROPY_HW_SDRAM_AVAIL) \
	-DMICROPY_HW_SDRAM_SIZE=$(MICROPY_HW_SDRAM_SIZE)
endif





LIBS = $(shell $(CC) $(CFLAGS) -print-libgcc-file-name)

OBJ += $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(LIBM_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SHARED_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(DRIVERS_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_S:.s=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_SS:.S=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_TINYUSB_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_HAL_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_HAL_IMX_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_ETH_C:.c=.o))
OBJ += $(GEN_PINS_SRC:.c=.o)


$(BUILD)/lib/tinyusb/src/device/usbd.o: CFLAGS += -Wno-missing-braces





all: $(BUILD)/firmware.hex $(BUILD)/firmware.bin



$(BUILD)/firmware.elf: $(OBJ)
	$(ECHO) "PREPROCESS LINK $@"
	$(Q)$(CC) -E -x c $(LDDEFINES) $(LD_FILES) | grep -v '^
	$(ECHO) "LINK $@"
	$(Q)$(LD) -T$(BUILD)/link.ld $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary $^ $@

$(BUILD)/firmware.hex: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O ihex -R .eeprom $< $@






$(OBJ): | $(GEN_PINS_HDR) $(GEN_FLEXRAM_CONFIG_SRC)



$(HEADER_BUILD)/qstrdefs.generated.h: $(BOARD_DIR)/mpconfigboard.h

$(GEN_FLEXRAM_CONFIG_SRC):
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) $(MAKE_FLEXRAM_LD) -d $(TOP)/$(MCU_DIR)/$(MCU_SERIES)$(MCU_CORE).h \
		-f $(TOP)/$(MCU_DIR)/$(MCU_SERIES)$(MCU_CORE)_features.h -l boards/$(MCU_SERIES).ld -c $(MCU_SERIES) > $(GEN_FLEXRAM_CONFIG_SRC)



$(BUILD)/%_gen.c $(HEADER_BUILD)/%.h: $(BOARD_PINS) $(MAKE_PINS) $(AF_FILE) $(PREFIX_FILE) | $(HEADER_BUILD)
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) $(MAKE_PINS) --board-csv $(BOARD_PINS) --af-csv $(AF_FILE) \
	    --prefix $(PREFIX_FILE) --iomux "$(abspath $(TOP)/$(MCU_DIR)/drivers/fsl_iomuxc.h)" \
	    --output-source $(GEN_PINS_SRC) --output-header $(GEN_PINS_HDR)

include $(TOP)/py/mkrules.mk
