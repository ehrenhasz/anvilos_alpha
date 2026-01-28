
OBJECT_FILES_NON_STANDARD := y

purgatory-y := purgatory.o stack.o setup-x86_$(BITS).o sha256.o entry64.o string.o

targets += $(purgatory-y)
PURGATORY_OBJS = $(addprefix $(obj)/,$(purgatory-y))

$(obj)/string.o: $(srctree)/arch/x86/boot/compressed/string.c FORCE
	$(call if_changed_rule,cc_o_c)

$(obj)/sha256.o: $(srctree)/lib/crypto/sha256.c FORCE
	$(call if_changed_rule,cc_o_c)

CFLAGS_sha256.o := -D__DISABLE_EXPORTS -D__NO_FORTIFY




KBUILD_CFLAGS := $(filter-out -fprofile-sample-use=% -fprofile-use=%,$(KBUILD_CFLAGS))



KBUILD_CFLAGS := $(filter-out $(CC_FLAGS_LTO),$(KBUILD_CFLAGS))



PURGATORY_LDFLAGS := -e purgatory_start -z nodefaultlib
LDFLAGS_purgatory.ro := -r $(PURGATORY_LDFLAGS)
LDFLAGS_purgatory.chk := $(PURGATORY_LDFLAGS)
targets += purgatory.ro purgatory.chk


GCOV_PROFILE	:= n
KASAN_SANITIZE	:= n
UBSAN_SANITIZE	:= n
KCSAN_SANITIZE	:= n
KMSAN_SANITIZE	:= n
KCOV_INSTRUMENT := n




PURGATORY_CFLAGS_REMOVE := -mcmodel=kernel
PURGATORY_CFLAGS := -mcmodel=large -ffreestanding -fno-zero-initialized-in-bss -g0
PURGATORY_CFLAGS += $(DISABLE_STACKLEAK_PLUGIN) -DDISABLE_BRANCH_PROFILING
PURGATORY_CFLAGS += -fno-stack-protector




ifdef CONFIG_FUNCTION_TRACER
PURGATORY_CFLAGS_REMOVE		+= $(CC_FLAGS_FTRACE)
endif

ifdef CONFIG_STACKPROTECTOR
PURGATORY_CFLAGS_REMOVE		+= -fstack-protector
endif

ifdef CONFIG_STACKPROTECTOR_STRONG
PURGATORY_CFLAGS_REMOVE		+= -fstack-protector-strong
endif

ifdef CONFIG_RETPOLINE
PURGATORY_CFLAGS_REMOVE		+= $(RETPOLINE_CFLAGS)
endif

ifdef CONFIG_CFI_CLANG
PURGATORY_CFLAGS_REMOVE		+= $(CC_FLAGS_CFI)
endif

CFLAGS_REMOVE_purgatory.o	+= $(PURGATORY_CFLAGS_REMOVE)
CFLAGS_purgatory.o		+= $(PURGATORY_CFLAGS)

CFLAGS_REMOVE_sha256.o		+= $(PURGATORY_CFLAGS_REMOVE)
CFLAGS_sha256.o			+= $(PURGATORY_CFLAGS)

CFLAGS_REMOVE_string.o		+= $(PURGATORY_CFLAGS_REMOVE)
CFLAGS_string.o			+= $(PURGATORY_CFLAGS)

asflags-remove-y		+= $(foreach x, -g -gdwarf-4 -gdwarf-5, $(x) -Wa,$(x))

$(obj)/purgatory.ro: $(PURGATORY_OBJS) FORCE
		$(call if_changed,ld)

$(obj)/purgatory.chk: $(obj)/purgatory.ro FORCE
		$(call if_changed,ld)

$(obj)/kexec-purgatory.o: $(obj)/purgatory.ro $(obj)/purgatory.chk

obj-y += kexec-purgatory.o
