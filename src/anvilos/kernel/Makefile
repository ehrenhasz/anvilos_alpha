# Makefile to generate MicroPython Embed Source
# Run with: make -f src/anvilos/kernel/Makefile

# Paths
KERNEL_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
TOP_DIR := $(abspath $(KERNEL_DIR)/../../..)
MICROPYTHON_TOP := $(TOP_DIR)/oss_sovereignty/sys_09_Anvil/source
PACKAGE_DIR := $(KERNEL_DIR)/generated
TOOLCHAIN := $(TOP_DIR)/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc

# Toolchain Definitions
CC := $(TOOLCHAIN)
CPP := $(TOOLCHAIN) -E

# Force PWD for include resolution
CFLAGS += -I$(KERNEL_DIR)

# Include the embed makefile logic
include $(MICROPYTHON_TOP)/ports/embed/embed.mk

$(info DEBUG: CC is $(CC))
$(info DEBUG: CPP is $(CPP))