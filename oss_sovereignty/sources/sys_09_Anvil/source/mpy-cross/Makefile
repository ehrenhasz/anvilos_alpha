include ../py/mkenv.mk


PROG = mpy-cross


QSTR_DEFS = qstrdefsport.h


UNAME_S := $(shell uname -s)


include $(TOP)/py/py.mk

INC += -I.
INC += -I$(BUILD)
INC += -I$(TOP)


CWARN = -Wall -Werror
CWARN += -Wextra -Wno-unused-parameter -Wpointer-arith
CFLAGS += $(INC) $(CWARN) -std=gnu99 $(COPT) $(CFLAGS_EXTRA)
CFLAGS += -fdata-sections -ffunction-sections -fno-asynchronous-unwind-tables


ifdef DEBUG
CFLAGS += -g
COPT = -O0
else
COPT = -Os 
endif





ifeq ($(UNAME_S),Darwin)
CC = clang

LDFLAGS_ARCH = -Wl,-map,$@.map -Wl,-dead_strip
else

LDFLAGS_ARCH = -Wl,-Map=$@.map,--cref -Wl,--gc-sections
endif
LDFLAGS += $(LDFLAGS_MOD) $(LDFLAGS_ARCH) -lm $(LDFLAGS_EXTRA)


SRC_C = \
	main.c \
	gccollect.c \
	shared/runtime/gchelper_generic.c \


COMPILER_TARGET := $(shell $(CC) -dumpmachine)
ifneq (,$(findstring mingw,$(COMPILER_TARGET)))
	SRC_C += ports/windows/fmode.c
endif

OBJ = $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))

include $(TOP)/py/mkrules.mk
