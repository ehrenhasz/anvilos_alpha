
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


QSTR_DEFS += qstrdefsport.h
QSTR_GLOBAL_DEPENDENCIES += $(VARIANT_DIR)/mpconfigvariant.h


UNAME_S := $(shell uname -s)


include $(TOP)/py/py.mk
include $(TOP)/extmod/extmod.mk

GIT_SUBMODULES += lib/berkeley-db-1.xx

INC +=  -I.
INC +=  -I$(TOP)
INC += -I$(BUILD)


CWARN = -Wall -Werror
CWARN += -Wextra -Wno-unused-parameter -Wpointer-arith -Wdouble-promotion -Wfloat-conversion
CFLAGS += $(INC) $(CWARN) -std=gnu99 -DUNIX $(COPT) -I$(VARIANT_DIR) $(CFLAGS_EXTRA)


ifdef DEBUG
COPT ?= -Og
else
COPT ?= -Os
COPT += -DNDEBUG
endif


COPT += -fdata-sections -ffunction-sections



CFLAGS += -g

ifndef DEBUG
























CFLAGS += -U _FORTIFY_SOURCE
endif





ifeq ($(UNAME_S),Darwin)
ifeq ($(MICROPY_FORCE_32BIT),1)
CC = clang -m32
else
CC = clang
endif

LDFLAGS_ARCH = -Wl,-map,$@.map -Wl,-dead_strip
else

LDFLAGS_ARCH = -Wl,-Map=$@.map,--cref -Wl,--gc-sections
endif
LDFLAGS += $(LDFLAGS_MOD) $(LDFLAGS_ARCH) -lm $(LDFLAGS_EXTRA)


LIBPTHREAD = -lpthread

ifeq ($(MICROPY_FORCE_32BIT),1)


ifeq ($(MICROPY_PY_FFI),1)
ifeq ($(UNAME_S),Linux)
CFLAGS += -I/usr/include/i686-linux-gnu
endif
endif
endif

ifeq ($(MICROPY_USE_READLINE),1)
INC += -I$(TOP)/shared/readline
CFLAGS += -DMICROPY_USE_READLINE=1
SHARED_SRC_C_EXTRA += readline/readline.c
endif
ifeq ($(MICROPY_PY_TERMIOS),1)
CFLAGS += -DMICROPY_PY_TERMIOS=1
endif
ifeq ($(MICROPY_PY_SOCKET),1)
CFLAGS += -DMICROPY_PY_SOCKET=1
endif
ifeq ($(MICROPY_PY_THREAD),1)
CFLAGS += -DMICROPY_PY_THREAD=1 -DMICROPY_PY_THREAD_GIL=0
LDFLAGS += $(LIBPTHREAD)
endif

ifeq ($(MICROPY_PY_SSL),1)
ifeq ($(MICROPY_SSL_AXTLS),1)

endif
endif


ifeq ($(MICROPY_PY_BLUETOOTH),1)
ifeq ($(MICROPY_BLUETOOTH_BTSTACK),1)
HAVE_LIBUSB := $(shell (which pkg-config > /dev/null && pkg-config --exists libusb-1.0) 2>/dev/null && echo '1')


ifeq ($(HAVE_LIBUSB),1)

MICROPY_BLUETOOTH_BTSTACK_USB ?= 1
else

MICROPY_BLUETOOTH_BTSTACK_H4 ?= 1
endif

SRC_BTSTACK_C += lib/btstack/platform/embedded/btstack_run_loop_embedded.c
endif
endif

ifeq ($(MICROPY_PY_FFI),1)

ifeq ($(MICROPY_STANDALONE),1)

GIT_SUBMODULES += lib/libffi
DEPLIBS += libffi
LIBFFI_CFLAGS := -I$(shell ls -1d $(BUILD)/lib/libffi/out/lib/libffi-*/include)
 ifeq ($(MICROPY_FORCE_32BIT),1)
  LIBFFI_LDFLAGS = $(BUILD)/lib/libffi/out/lib32/libffi.a
 else
  LIBFFI_LDFLAGS = $(BUILD)/lib/libffi/out/lib/libffi.a
 endif
else

LIBFFI_CFLAGS := $(shell pkg-config --cflags libffi)
LIBFFI_LDFLAGS := $(shell pkg-config --libs libffi)
endif

ifeq ($(UNAME_S),Linux)
LIBFFI_LDFLAGS += -ldl
endif

CFLAGS += $(LIBFFI_CFLAGS) -DMICROPY_PY_FFI=1
LDFLAGS += $(LIBFFI_LDFLAGS)
endif

ifeq ($(MICROPY_PY_JNI),1)

CFLAGS += -I/usr/lib/jvm/java-7-openjdk-amd64/include -DMICROPY_PY_JNI=1
endif


SRC_C += \
	main.c \
	gccollect.c \
	unix_mphal.c \
	mpthreadport.c \
	input.c \
	alloc.c \
	fatfs_port.c \
	mpbthciport.c \
	mpbtstackport_common.c \
	mpbtstackport_h4.c \
	mpbtstackport_usb.c \
	mpnimbleport.c \
	modtermios.c \
	modsocket.c \
	modffi.c \
	modjni.c \
	$(wildcard $(VARIANT_DIR)/*.c)

SHARED_SRC_C += $(addprefix shared/,\
	runtime/gchelper_generic.c \
	timeutils/timeutils.c \
	$(SHARED_SRC_C_EXTRA) \
	)

SRC_CXX += \

OBJ = $(PY_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_CXX:.cpp=.o))
OBJ += $(addprefix $(BUILD)/, $(SHARED_SRC_C:.c=.o))


SRC_QSTR += $(SRC_C) $(SRC_CXX) $(SHARED_SRC_C)

ifneq ($(FROZEN_MANIFEST),)
CFLAGS += -DMPZ_DIG_SIZE=16 
endif

CXXFLAGS += $(filter-out -Wmissing-prototypes -Wold-style-definition -std=gnu99,$(CFLAGS) $(CXXFLAGS_MOD))

ifeq ($(MICROPY_FORCE_32BIT),1)
RUN_TESTS_MPY_CROSS_FLAGS = --mpy-cross-flags='-march=x86'
endif

ifeq ($(CROSS_COMPILE),arm-linux-gnueabi-)






$(info Detected arm-linux-gnueabi-gcc. Disabling error message compression.)
MICROPY_ROM_TEXT_COMPRESSION = 0
endif

include $(TOP)/py/mkrules.mk

.PHONY: test test_full

test: $(BUILD)/$(PROG) $(TOP)/tests/run-tests.py
	$(eval DIRNAME=ports/$(notdir $(CURDIR)))
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py

test_full: $(BUILD)/$(PROG) $(TOP)/tests/run-tests.py
	$(eval DIRNAME=ports/$(notdir $(CURDIR)))
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py -d thread
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py --emit native
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py --via-mpy $(RUN_TESTS_MPY_CROSS_FLAGS) -d basics float micropython
	cd $(TOP)/tests && MICROPY_MICROPYTHON=../$(DIRNAME)/$(BUILD)/$(PROG) ./run-tests.py --via-mpy $(RUN_TESTS_MPY_CROSS_FLAGS) --emit native -d basics float micropython
	cat $(TOP)/tests/basics/0prelim.py | ./$(BUILD)/$(PROG) | grep -q 'abc'

test_gcov: test_full
	gcov -o $(BUILD)/py $(TOP)/py/*.c
	gcov -o $(BUILD)/extmod $(TOP)/extmod/*.c



ifneq ($(CROSS_COMPILE),)
CROSS_COMPILE_HOST = --host=$(patsubst %-,%,$(CROSS_COMPILE))
else
CROSS_COMPILE_HOST =
endif

deplibs: $(DEPLIBS)

libffi: $(BUILD)/lib/libffi/include/ffi.h

$(TOP)/lib/libffi/configure: $(TOP)/lib/libffi/autogen.sh
	cd $(TOP)/lib/libffi; ./autogen.sh



$(BUILD)/lib/libffi/include/ffi.h: $(TOP)/lib/libffi/configure
	mkdir -p $(BUILD)/lib/libffi; cd $(BUILD)/lib/libffi; \
	$(abspath $(TOP))/lib/libffi/configure $(CROSS_COMPILE_HOST) --prefix=$$PWD/out --disable-shared --disable-structs CC="$(CC)" CXX="$(CXX)" LD="$(LD)" CFLAGS="-Os -fomit-frame-pointer -fstrict-aliasing -ffast-math -fno-exceptions"; \
	$(MAKE) install-exec-recursive; $(MAKE) -C include install-data-am

PREFIX = /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

install: $(BUILD)/$(PROG)
	install -d $(BINDIR)
	install $(BUILD)/$(PROG) $(BINDIR)/$(PROG)

uninstall:
	-rm $(BINDIR)/$(PROG)
