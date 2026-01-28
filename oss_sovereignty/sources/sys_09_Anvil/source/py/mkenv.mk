ifneq ($(lastword a b),b)
$(error These Makefiles require make 3.81 or newer)
endif








THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TOP := $(patsubst %/py/mkenv.mk,%,$(THIS_MAKEFILE))





ifeq ("$(origin V)", "command line")
BUILD_VERBOSE=$(V)
endif
ifndef BUILD_VERBOSE
$(info Use make V=1 or set BUILD_VERBOSE in your environment to increase build verbosity.)
BUILD_VERBOSE = 0
endif
ifeq ($(BUILD_VERBOSE),0)
Q = @
else
Q =
endif



PY_SRC ?= $(TOP)/py
BUILD ?= build

RM = rm
ECHO = @echo
CP = cp
MKDIR = mkdir
SED = sed
CAT = cat
TOUCH = touch
PYTHON = python3
ZIP = zip

AS = $(CROSS_COMPILE)as
CC = $(CROSS_COMPILE)gcc
CPP = $(CC) -E
CXX = $(CROSS_COMPILE)g++
GDB = $(CROSS_COMPILE)gdb
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size
STRIP = $(CROSS_COMPILE)strip
AR = $(CROSS_COMPILE)ar
WINDRES = $(CROSS_COMPILE)windres

MAKE_MANIFEST = $(PYTHON) $(TOP)/tools/makemanifest.py
MAKE_FROZEN = $(PYTHON) $(TOP)/tools/make-frozen.py
MPY_TOOL = $(PYTHON) $(TOP)/tools/mpy-tool.py

MPY_LIB_SUBMODULE_DIR = $(TOP)/lib/micropython-lib
MPY_LIB_DIR = $(MPY_LIB_SUBMODULE_DIR)

ifeq ($(MICROPY_MPYCROSS),)
MICROPY_MPYCROSS = $(TOP)/mpy-cross/build/mpy-cross
MICROPY_MPYCROSS_DEPENDENCY = $(MICROPY_MPYCROSS)
endif

all:
.PHONY: all

.DELETE_ON_ERROR:

MKENV_INCLUDED = 1
