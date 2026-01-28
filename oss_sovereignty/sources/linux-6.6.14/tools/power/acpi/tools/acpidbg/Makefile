






include ../../Makefile.config

TOOL = acpidbg
vpath %.c \
	../../../../../drivers/acpi/acpica\
	../../common\
	../../os_specific/service_layers\
	.
CFLAGS += -DACPI_APPLICATION -DACPI_SINGLE_THREAD -DACPI_DEBUGGER\
	-I.
LDFLAGS += -lpthread
TOOL_OBJS = \
	acpidbg.o

include ../../Makefile.rules
