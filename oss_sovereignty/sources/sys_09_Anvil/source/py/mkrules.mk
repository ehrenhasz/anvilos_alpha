ifneq ($(MKENV_INCLUDED),1)

THIS_MAKEFILE = $(lastword $(MAKEFILE_LIST))
include $(dir $(THIS_MAKEFILE))mkenv.mk
endif


MICROPY_PREVIEW_VERSION_2 ?= 0

ifeq ($(MICROPY_PREVIEW_VERSION_2),1)
CFLAGS += -DMICROPY_PREVIEW_VERSION_2=1
endif

HELP_BUILD_ERROR ?= "See \033[1;31mhttps://github.com/micropython/micropython/wiki/Build-Troubleshooting\033[0m"
HELP_MPY_LIB_SUBMODULE ?= "\033[1;31mError: micropython-lib submodule is not initialized.\033[0m Run 'make submodules'"


OBJ_EXTRA_ORDER_DEPS =


OBJ_EXTRA_ORDER_DEPS += $(HEADER_BUILD)/moduledefs.h $(HEADER_BUILD)/root_pointers.h

ifeq ($(MICROPY_ROM_TEXT_COMPRESSION),1)

OBJ_EXTRA_ORDER_DEPS += $(HEADER_BUILD)/compressed.data.h

CFLAGS += -DMICROPY_ROM_TEXT_COMPRESSION=1
endif


QSTR_GEN_FLAGS = -DNO_QSTR

QSTR_GEN_CFLAGS := $(CFLAGS)
QSTR_GEN_CFLAGS += $(QSTR_GEN_FLAGS)
QSTR_GEN_CXXFLAGS := $(CXXFLAGS)
QSTR_GEN_CXXFLAGS += $(QSTR_GEN_FLAGS)

















vpath %.S . $(TOP) $(USER_C_MODULES)
$(BUILD)/%.o: %.S
	$(ECHO) "CC $<"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

vpath %.s . $(TOP) $(USER_C_MODULES)
$(BUILD)/%.o: %.s
	$(ECHO) "AS $<"
	$(Q)$(AS) $(AFLAGS) -o $@ $<

define compile_c
$(ECHO) "CC $<"
$(Q)$(CC) $(CFLAGS) -c -MD -MF $(@:.o=.d) -o $@ $< || (echo -e $(HELP_BUILD_ERROR); false)
@
@
@
@$(CP) $(@:.o=.d) $(@:.o=.P); \
  $(SED) -e 's/
      -e '/^$$/ d' -e 's/$$/ :/' < $(@:.o=.d) >> $(@:.o=.P); \
  $(RM) -f $(@:.o=.d)
endef

define compile_cxx
$(ECHO) "CXX $<"
$(Q)$(CXX) $(CXXFLAGS) -c -MD -MF $(@:.o=.d) -o $@ $< || (echo -e $(HELP_BUILD_ERROR); false)
@
@
@
@$(CP) $(@:.o=.d) $(@:.o=.P); \
  $(SED) -e 's/
      -e '/^$$/ d' -e 's/$$/ :/' < $(@:.o=.d) >> $(@:.o=.P); \
  $(RM) -f $(@:.o=.d)
endef

vpath %.c . $(TOP) $(USER_C_MODULES)
$(BUILD)/%.o: %.c
	$(call compile_c)

vpath %.cpp . $(TOP) $(USER_C_MODULES)
$(BUILD)/%.o: %.cpp
	$(call compile_cxx)

$(BUILD)/%.pp: %.c
	$(ECHO) "PreProcess $<"
	$(Q)$(CPP) $(CFLAGS) -Wp,-C,-dD,-dI -o $@ $<


$(BUILD)/%.o: $(BUILD)/%.c
	$(call compile_c)










$(OBJ): | $(HEADER_BUILD)/qstrdefs.generated.h $(HEADER_BUILD)/mpversion.h $(OBJ_EXTRA_ORDER_DEPS)






$(HEADER_BUILD)/qstr.i.last: $(SRC_QSTR) $(QSTR_GLOBAL_DEPENDENCIES) | $(QSTR_GLOBAL_REQUIREMENTS)
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py pp $(CPP) output $(HEADER_BUILD)/qstr.i.last cflags $(QSTR_GEN_CFLAGS) cxxflags $(QSTR_GEN_CXXFLAGS) sources $^ dependencies $(QSTR_GLOBAL_DEPENDENCIES) changed_sources $?

$(HEADER_BUILD)/qstr.split: $(HEADER_BUILD)/qstr.i.last
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py split qstr $< $(HEADER_BUILD)/qstr _
	$(Q)$(TOUCH) $@

$(QSTR_DEFS_COLLECTED): $(HEADER_BUILD)/qstr.split
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py cat qstr _ $(HEADER_BUILD)/qstr $@


$(HEADER_BUILD)/moduledefs.split: $(HEADER_BUILD)/qstr.i.last
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py split module $< $(HEADER_BUILD)/module _
	$(Q)$(TOUCH) $@

$(HEADER_BUILD)/moduledefs.collected: $(HEADER_BUILD)/moduledefs.split
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py cat module _ $(HEADER_BUILD)/module $@


$(HEADER_BUILD)/root_pointers.split: $(HEADER_BUILD)/qstr.i.last
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py split root_pointer $< $(HEADER_BUILD)/root_pointer _
	$(Q)$(TOUCH) $@

$(HEADER_BUILD)/root_pointers.collected: $(HEADER_BUILD)/root_pointers.split
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py cat root_pointer _ $(HEADER_BUILD)/root_pointer $@


$(HEADER_BUILD)/compressed.split: $(HEADER_BUILD)/qstr.i.last
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py split compress $< $(HEADER_BUILD)/compress _
	$(Q)$(TOUCH) $@

$(HEADER_BUILD)/compressed.collected: $(HEADER_BUILD)/compressed.split
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdefs.py cat compress _ $(HEADER_BUILD)/compress $@






OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
	$(MKDIR) -p $@

$(HEADER_BUILD):
	$(MKDIR) -p $@

ifneq ($(MICROPY_MPYCROSS_DEPENDENCY),)

$(MICROPY_MPYCROSS_DEPENDENCY):
	$(MAKE) -C "$(abspath $(dir $@)..)"
endif

ifneq ($(FROZEN_DIR),)
$(error Support for FROZEN_DIR was removed. Please use manifest.py instead, see https://docs.micropython.org/en/latest/reference/manifest.html)
endif

ifneq ($(FROZEN_MPY_DIR),)
$(error Support for FROZEN_MPY_DIR was removed. Please use manifest.py instead, see https://docs.micropython.org/en/latest/reference/manifest.html)
endif

ifneq ($(FROZEN_MANIFEST),)


ifeq ($(MPY_LIB_DIR),$(MPY_LIB_SUBMODULE_DIR))
GIT_SUBMODULES += lib/micropython-lib
endif


CFLAGS += -DMICROPY_QSTR_EXTRA_POOL=mp_qstr_frozen_const_pool
CFLAGS += -DMICROPY_MODULE_FROZEN_MPY
CFLAGS += -DMICROPY_MODULE_FROZEN_STR




MICROPY_MANIFEST_MPY_LIB_DIR = $(MPY_LIB_DIR)
MICROPY_MANIFEST_PORT_DIR = $(shell pwd)
MICROPY_MANIFEST_BOARD_DIR = $(BOARD_DIR)
MICROPY_MANIFEST_MPY_DIR = $(TOP)


MANIFEST_VARIABLES = $(foreach var,$(filter MICROPY_MANIFEST_%, $(.VARIABLES)),-v "$(subst MICROPY_MANIFEST_,,$(var))=$($(var))")


$(BUILD)/frozen_content.c: FORCE $(BUILD)/genhdr/qstrdefs.generated.h $(BUILD)/genhdr/root_pointers.h | $(MICROPY_MPYCROSS_DEPENDENCY)
	$(Q)test -e "$(MPY_LIB_DIR)/README.md" || (echo -e $(HELP_MPY_LIB_SUBMODULE); false)
	$(Q)$(MAKE_MANIFEST) -o $@ $(MANIFEST_VARIABLES) -b "$(BUILD)" $(if $(MPY_CROSS_FLAGS),-f"$(MPY_CROSS_FLAGS)",) --mpy-tool-flags="$(MPY_TOOL_FLAGS)" $(FROZEN_MANIFEST)
endif

ifneq ($(PROG),)




COMPILER_TARGET := $(shell $(CC) -dumpmachine)
ifneq (,$(findstring mingw,$(COMPILER_TARGET)))
PROG := $(PROG).exe
endif

all: $(BUILD)/$(PROG)

$(BUILD)/$(PROG): $(OBJ)
	$(ECHO) "LINK $@"


	$(Q)$(CC) -o $@ $^ $(LIB) $(LDFLAGS)
ifndef DEBUG
ifdef STRIP
	$(Q)$(STRIP) $(STRIPFLAGS_EXTRA) $@
endif
endif
	$(Q)$(SIZE) $$(find $(BUILD) -path "$(BUILD)/build/frozen*.o") $@

clean: clean-prog
clean-prog:
	$(RM) -f $(BUILD)/$(PROG)
	$(RM) -f $(BUILD)/$(PROG).map

.PHONY: clean-prog
endif

submodules:
	$(ECHO) "Updating submodules: $(GIT_SUBMODULES)"
ifneq ($(GIT_SUBMODULES),)
	$(Q)git submodule sync $(addprefix $(TOP)/,$(GIT_SUBMODULES))
	$(Q)git submodule update --init $(addprefix $(TOP)/,$(GIT_SUBMODULES))
endif
.PHONY: submodules

LIBMICROPYTHON = libmicropython.a






lib $(BUILD)/$(LIBMICROPYTHON): $(OBJ)
	$(Q)$(AR) rcs $(BUILD)/$(LIBMICROPYTHON) $^
	$(LIBMICROPYTHON_EXTRA_CMD)

clean:
	$(RM) -rf $(BUILD) $(CLEAN_EXTRA)
.PHONY: clean

print-cfg:
	$(ECHO) "PY_SRC = $(PY_SRC)"
	$(ECHO) "BUILD  = $(BUILD)"
	$(ECHO) "OBJ    = $(OBJ)"
.PHONY: print-cfg

print-def:
	@$(ECHO) "The following defines are built into the $(CC) compiler"
	$(TOUCH) __empty__.c
	@$(CC) -E -Wp,-dM __empty__.c
	@$(RM) -f __empty__.c

-include $(OBJ:.o=.P)
