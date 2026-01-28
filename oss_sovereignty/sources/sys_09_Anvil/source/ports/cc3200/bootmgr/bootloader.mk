BUILD = bootmgr/build/$(BOARD)/$(BTYPE)

BOOT_INC  = -Ibootmgr
BOOT_INC += -Ibootmgr/sl
BOOT_INC += -Ihal
BOOT_INC += -Ihal/inc
BOOT_INC += -I$(TOP)/drivers/cc3100/inc
BOOT_INC += -Imisc
BOOT_INC += -Imods
BOOT_INC += -Isimplelink
BOOT_INC += -Isimplelink/oslib
BOOT_INC += -Iutil
BOOT_INC += -I$(TOP)
BOOT_INC += -I.
BOOT_INC += -I$(BUILD)

BOOT_CPPDEFINES = -Dgcc -DBOOTLOADER -DTARGET_IS_CC3200 -DSL_TINY

BOOT_HAL_SRC_C = $(addprefix hal/,\
	cpu.c \
	interrupt.c \
	gpio.c \
	pin.c \
	prcm.c \
	shamd5.c \
	spi.c \
	startup_gcc.c \
	systick.c \
	utils.c \
	)

BOOT_CC3100_SRC_C = $(addprefix drivers/cc3100/,\
	src/device.c \
	src/driver.c \
	src/flowcont.c \
	src/fs.c \
	src/netapp.c \
	src/netcfg.c \
	src/nonos.c \
	src/socket.c \
	src/spawn.c \
	src/wlan.c \
	)

BOOT_MISC_SRC_C = $(addprefix misc/,\
	antenna.c \
	mperror.c \
	)

BOOT_SL_SRC_C = $(addprefix simplelink/,\
	cc_pal.c \
	)

BOOT_UTIL_SRC_C = $(addprefix util/,\
	cryptohash.c \
	)

BOOT_MAIN_SRC_C = \
	bootmgr/main.c

BOOT_MAIN_SRC_S = \
	bootmgr/runapp.s

BOOT_PY_SRC_C = $(addprefix py/,\
	mpprint.c \
	)

BOOT_SHARED_SRC_C = $(addprefix shared/,\
	libc/printf.c \
	libc/string0.c \
	)

OBJ  = $(addprefix $(BUILD)/, $(BOOT_HAL_SRC_C:.c=.o) $(BOOT_SL_SRC_C:.c=.o) $(BOOT_CC3100_SRC_C:.c=.o) $(BOOT_UTIL_SRC_C:.c=.o) $(BOOT_MISC_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(BOOT_MAIN_SRC_C:.c=.o) $(BOOT_MAIN_SRC_S:.s=.o) $(BOOT_PY_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(BOOT_SHARED_SRC_C:.c=.o))


LINKER_SCRIPT = bootmgr/bootmgr.lds
LDFLAGS += -T $(LINKER_SCRIPT)


CFLAGS += $(BOOT_CPPDEFINES) $(BOOT_INC)


$(BUILD)/drivers/cc3100/src/driver.o: CFLAGS += -fno-strict-aliasing


ifeq ($(BTYPE), release)

CFLAGS += -Os -DNDEBUG
else
ifeq ($(BTYPE), debug)

CFLAGS += -DDEBUG=DEBUG

$(BUILD)/hal/%.o: CFLAGS += -Os
$(BUILD)/misc/%.o: CFLAGS += -Os
$(BUILD)/simplelink/%.o: CFLAGS += -Os
$(BUILD)/drivers/cc3100/%.o: CFLAGS += -Os
$(BUILD)/py/%.o: CFLAGS += -Os
$(BUILD)/ports/stm32/%.o: CFLAGS += -Os
else
$(error Invalid BTYPE specified)
endif
endif

SHELL = bash
BOOT_GEN = bootmgr/bootgen.sh
HEADER_BUILD = $(BUILD)/genhdr

all: $(BUILD)/bootloader.bin

$(BUILD)/bootmgr.axf: $(OBJ) $(LINKER_SCRIPT)
	$(ECHO) "LINK $@"
	$(Q)$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/bootmgr.bin: $(BUILD)/bootmgr.axf
	$(ECHO) "Create $@"
	$(Q)$(OBJCOPY) -O binary $< $@

$(BUILD)/bootloader.bin: $(BUILD)/bootmgr.bin
	$(ECHO) "Create $@"
	$(Q)$(SHELL) $(BOOT_GEN) $(BUILD)


$(HEADER_BUILD)/qstrdefs.generated.h: | $(HEADER_BUILD)
	touch $@


$(HEADER_BUILD)/mpversion.h: | $(HEADER_BUILD)
	touch $@


$(HEADER_BUILD)/moduledefs.h: | $(HEADER_BUILD)
	touch $@


$(HEADER_BUILD)/root_pointers.h: | $(HEADER_BUILD)
	touch $@
