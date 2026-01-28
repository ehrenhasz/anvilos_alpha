





$(BUILD)/openamp: $(BUILD)
	$(MKDIR) -p $@

$(BUILD)/openamp/metal: $(BUILD)/openamp
	$(MKDIR) -p $@

$(BUILD)/openamp/metal/config.h: $(BUILD)/openamp/metal $(TOP)/$(LIBMETAL_DIR)/lib/config.h
	@$(ECHO) "GEN $@"
	@for file in $(TOP)/$(LIBMETAL_DIR)/lib/*.c $(TOP)/$(LIBMETAL_DIR)/lib/*.h; do $(SED) -e "s/@PROJECT_SYSTEM@/micropython/g" -e "s/@PROJECT_PROCESSOR@/arm/g" $$file > $(BUILD)/openamp/metal/$$(basename $$file); done
	$(MKDIR) -p $(BUILD)/openamp/metal/processor/arm
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/processor/arm/*.h $(BUILD)/openamp/metal/processor/arm
	$(MKDIR) -p $(BUILD)/openamp/metal/compiler/gcc
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/compiler/gcc/*.h $(BUILD)/openamp/metal/compiler/gcc
	$(MKDIR) -p $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/$(LIBMETAL_DIR)/lib/system/generic/*.c $(TOP)/$(LIBMETAL_DIR)/lib/system/generic/*.h $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/extmod/libmetal/metal/system/micropython/* $(BUILD)/openamp/metal/system/micropython
	@$(CP) $(TOP)/extmod/libmetal/metal/config.h $(BUILD)/openamp/metal/config.h


SRC_LIBMETAL_C := $(addprefix $(BUILD)/openamp/metal/,\
	device.c \
	dma.c \
	init.c \
	io.c \
	irq.c \
	log.c \
	shmem.c \
	softirq.c \
	version.c \
	device.c \
	system/micropython/condition.c \
	system/micropython/device.c \
	system/micropython/io.c \
	system/micropython/irq.c \
	system/micropython/shmem.c \
	system/micropython/time.c \
	)




$(SRC_LIBMETAL_C): $(BUILD)/openamp/metal/config.h


QSTR_GLOBAL_REQUIREMENTS += $(BUILD)/openamp/metal/config.h
