
ifdef VARIANT_DIR


VARIANT ?= $(notdir $(VARIANT_DIR:/=))
else

VARIANT ?= standard
VARIANT_DIR ?= variants/$(VARIANT)
endif

ifeq ($(wildcard $(VARIANT_DIR)/.),)
$(error Invalid VARIANT specified: $(VARIANT_DIR))
endif


BUILD ?= build-$(VARIANT)

include ../../py/mkenv.mk
-include mpconfigport.mk
include $(VARIANT_DIR)/mpconfigvariant.mk

FROZEN_MANIFEST ?= variants/manifest.py


PROG ?= micropython


QSTR_DEFS += ../unix/qstrdefsport.h
QSTR_GLOBAL_DEPENDENCIES += $(VARIANT_DIR)/mpconfigvariant.h


include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -I$(VARIANT_DIR)


CFLAGS += $(INC) -Wall -Wpointer-arith -Wdouble-promotion -Werror -std=gnu99 -DUNIX -D__USE_MINGW_ANSI_STDIO=1 $(COPT) $(CFLAGS_EXTRA)
LDFLAGS += -lm -lbcrypt $(LDFLAGS_EXTRA)


ifdef DEBUG
CFLAGS += -g
COPT = -O0
else
COPT = -Os 
endif


SRC_C = \
	shared/libc/printf.c \
	shared/runtime/gchelper_generic.c \
	ports/unix/main.c \
	ports/unix/input.c \
	ports/unix/gccollect.c \
	windows_mphal.c \
	realpath.c \
	init.c \
	fmode.c \
	$(wildcard $(VARIANT_DIR)/*.c)

SHARED_SRC_C += $(addprefix shared/,\
	$(SHARED_SRC_C_EXTRA) \
	)

OBJ = $(PY_O) $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_CXX:.cpp=.o))
OBJ += $(addprefix $(BUILD)/, $(SHARED_SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(LIB_SRC_C:.c=.o))

ifeq ($(MICROPY_USE_READLINE),1)
CFLAGS += -DMICROPY_USE_READLINE=1
SRC_C += shared/readline/readline.c
endif

LIB += -lws2_32


SRC_QSTR += $(SRC_C) $(SRC_CXX) $(SHARED_SRC_C)

ifneq ($(FROZEN_MANIFEST),)
CFLAGS += -DMPZ_DIG_SIZE=16
endif

ifeq ($(shell $(CC) -dumpmachine),i686-w64-mingw32)





CFLAGS += -msse -mfpmath=sse -march=pentium4
endif

CXXFLAGS += $(filter-out -std=gnu99,$(CFLAGS))

include $(TOP)/py/mkrules.mk

.PHONY: test test_full

RUN_TESTS_SKIP += -e math_fun -e float2int_double -e float_parse -e math_domain_special

test: $(BUILD)/$(PROG) $(TOP)/tests/run-tests.py
	$(eval DIRNAME=ports/$(notdir $(CURDIR)))
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) $(PYTHON) ./run-tests.py $(RUN_TESTS_SKIP)

test_full: test
	$(eval DIRNAME=ports/$(notdir $(CURDIR)))
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) $(PYTHON) ./run-tests.py --via-mpy $(RUN_TESTS_MPY_CROSS_FLAGS) $(RUN_TESTS_SKIP) -d basics float micropython

$(BUILD)/$(PROG): $(BUILD)/micropython.res

$(BUILD)/%.res: %.rc
	$(ECHO) "WINDRES $<"
	$(Q)$(WINDRES) $< -O coff -o $@
