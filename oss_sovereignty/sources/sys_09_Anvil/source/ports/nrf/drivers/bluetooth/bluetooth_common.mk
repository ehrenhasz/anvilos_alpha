
SOFTDEV_HEX_NAME ?=
SOFTDEV_HEX_PATH ?=

ifeq ($(SD), s110)
	INC += -Idrivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_API/include
	CFLAGS += -DBLUETOOTH_SD_DEBUG=1
	CFLAGS += -DBLUETOOTH_SD=110

else ifeq ($(SD), s132)
	INC += -Idrivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_API/include
	INC += -Idrivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_API/include/$(MCU_VARIANT)
	CFLAGS += -DBLUETOOTH_SD_DEBUG=1
	CFLAGS += -DBLUETOOTH_SD=132

else ifeq ($(SD), s140)
	INC += -Idrivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_API/include
	INC += -Idrivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_API/include/$(MCU_VARIANT)
	CFLAGS += -DBLUETOOTH_SD_DEBUG=1
	CFLAGS += -DBLUETOOTH_SD=140
else
	$(error Incorrect softdevice set flag)
endif

SOFTDEV_HEX_NAME = $(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)_softdevice.hex
SOFTDEV_HEX_PATH = drivers/bluetooth/$(SD)_$(MCU_VARIANT)_$(SOFTDEV_VERSION)

define STACK_MISSING_ERROR













endef


SOFTDEV_HEX = $(SOFTDEV_HEX_PATH)/$(SOFTDEV_HEX_NAME)

ifeq ($(shell test ! -e $(SOFTDEV_HEX) && echo -n no),no)
    $(error $(STACK_MISSING_ERROR))
endif
