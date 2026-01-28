
PY_BUILD = $(BUILD)/py


HEADER_BUILD = $(BUILD)/genhdr


PY_QSTR_DEFS = $(PY_SRC)/qstrdefs.h



ifneq ($(QSTR_AUTOGEN_DISABLE),1)
QSTR_DEFS_COLLECTED = $(HEADER_BUILD)/qstrdefs.collected.h
endif



QSTR_GLOBAL_DEPENDENCIES += $(PY_SRC)/mpconfig.h mpconfigport.h
QSTR_GLOBAL_REQUIREMENTS += $(HEADER_BUILD)/mpversion.h


CSUPEROPT = -O3


ifeq ($(MICROPY_FORCE_32BIT),1)
CC += -m32
CXX += -m32
LD += -m32
endif


ifneq ($(USER_C_MODULES),)




SRC_USERMOD_C :=
SRC_USERMOD_CXX :=

SRC_USERMOD_LIB_C :=
SRC_USERMOD_LIB_CXX :=

CFLAGS_USERMOD :=
CXXFLAGS_USERMOD :=
LDFLAGS_USERMOD :=



SRC_USERMOD :=

$(foreach module, $(wildcard $(USER_C_MODULES)/*/micropython.mk), \
    $(eval USERMOD_DIR = $(patsubst %/,%,$(dir $(module))))\
    $(info Including User C Module from $(USERMOD_DIR))\
	$(eval include $(module))\
)

SRC_USERMOD_C += $(SRC_USERMOD)

SRC_USERMOD_PATHFIX_C += $(patsubst $(USER_C_MODULES)/%.c,%.c,$(SRC_USERMOD_C))
SRC_USERMOD_PATHFIX_CXX += $(patsubst $(USER_C_MODULES)/%.cpp,%.cpp,$(SRC_USERMOD_CXX))
SRC_USERMOD_PATHFIX_LIB_C += $(patsubst $(USER_C_MODULES)/%.c,%.c,$(SRC_USERMOD_LIB_C))
SRC_USERMOD_PATHFIX_LIB_CXX += $(patsubst $(USER_C_MODULES)/%.cpp,%.cpp,$(SRC_USERMOD_LIB_CXX))

CFLAGS += $(CFLAGS_USERMOD)
CXXFLAGS += $(CXXFLAGS_USERMOD)
LDFLAGS += $(LDFLAGS_USERMOD)

SRC_QSTR += $(SRC_USERMOD_PATHFIX_C) $(SRC_USERMOD_PATHFIX_CXX)
PY_O += $(addprefix $(BUILD)/, $(SRC_USERMOD_PATHFIX_C:.c=.o))
PY_O += $(addprefix $(BUILD)/, $(SRC_USERMOD_PATHFIX_CXX:.cpp=.o))
PY_O += $(addprefix $(BUILD)/, $(SRC_USERMOD_PATHFIX_LIB_C:.c=.o))
PY_O += $(addprefix $(BUILD)/, $(SRC_USERMOD_PATHFIX_LIB_CXX:.cpp=.o))
endif


PY_CORE_O_BASENAME = $(addprefix py/,\
	mpstate.o \
	nlr.o \
	nlrx86.o \
	nlrx64.o \
	nlrthumb.o \
	nlraarch64.o \
	nlrmips.o \
	nlrpowerpc.o \
	nlrxtensa.o \
	nlrsetjmp.o \
	malloc.o \
	gc.o \
	pystack.o \
	qstr.o \
	vstr.o \
	mpprint.o \
	unicode.o \
	mpz.o \
	reader.o \
	lexer.o \
	parse.o \
	scope.o \
	compile.o \
	emitcommon.o \
	emitbc.o \
	asmbase.o \
	asmx64.o \
	emitnx64.o \
	asmx86.o \
	emitnx86.o \
	asmthumb.o \
	emitnthumb.o \
	emitinlinethumb.o \
	asmarm.o \
	emitnarm.o \
	asmxtensa.o \
	emitnxtensa.o \
	emitinlinextensa.o \
	emitnxtensawin.o \
	formatfloat.o \
	parsenumbase.o \
	parsenum.o \
	emitglue.o \
	persistentcode.o \
	runtime.o \
	runtime_utils.o \
	scheduler.o \
	nativeglue.o \
	pairheap.o \
	ringbuf.o \
	stackctrl.o \
	argcheck.o \
	warning.o \
	profile.o \
	map.o \
	obj.o \
	objarray.o \
	objattrtuple.o \
	objbool.o \
	objboundmeth.o \
	objcell.o \
	objclosure.o \
	objcomplex.o \
	objdeque.o \
	objdict.o \
	objenumerate.o \
	objexcept.o \
	objfilter.o \
	objfloat.o \
	objfun.o \
	objgenerator.o \
	objgetitemiter.o \
	objint.o \
	objint_longlong.o \
	objint_mpz.o \
	objlist.o \
	objmap.o \
	objmodule.o \
	objobject.o \
	objpolyiter.o \
	objproperty.o \
	objnone.o \
	objnamedtuple.o \
	objrange.o \
	objreversed.o \
	objset.o \
	objsingleton.o \
	objslice.o \
	objstr.o \
	objstrunicode.o \
	objstringio.o \
	objtuple.o \
	objtype.o \
	objzip.o \
	opmethods.o \
	sequence.o \
	stream.o \
	binary.o \
	builtinimport.o \
	builtinevex.o \
	builtinhelp.o \
	modarray.o \
	modbuiltins.o \
	modcollections.o \
	modgc.o \
	modio.o \
	modmath.o \
	modcmath.o \
	modmicropython.o \
	modstruct.o \
	modsys.o \
	moderrno.o \
	modthread.o \
	vm.o \
	bc.o \
	showbc.o \
	repl.o \
	smallint.o \
	frozenmod.o \
	)


PY_CORE_O = $(addprefix $(BUILD)/, $(PY_CORE_O_BASENAME))


PY_O += $(PY_CORE_O)


ifneq ($(FROZEN_MANIFEST),)
PY_O += $(BUILD)/frozen_content.o
endif


SRC_QSTR_IGNORE = py/nlr%
SRC_QSTR += $(filter-out $(SRC_QSTR_IGNORE),$(PY_CORE_O_BASENAME:.o=.c))


FORCE:
.PHONY: FORCE

$(HEADER_BUILD)/mpversion.h: FORCE | $(HEADER_BUILD)
	$(Q)$(PYTHON) $(PY_SRC)/makeversionhdr.py $@



MPCONFIGPORT_MK = $(wildcard mpconfigport.mk)







$(HEADER_BUILD)/qstrdefs.generated.h: $(PY_QSTR_DEFS) $(QSTR_DEFS) $(QSTR_DEFS_COLLECTED) $(PY_SRC)/makeqstrdata.py mpconfigport.h $(MPCONFIGPORT_MK) $(PY_SRC)/mpconfig.h | $(HEADER_BUILD)
	$(ECHO) "GEN $@"
	$(Q)$(CAT) $(PY_QSTR_DEFS) $(QSTR_DEFS) $(QSTR_DEFS_COLLECTED) | $(SED) 's/^Q(.*)/"&"/' | $(CPP) $(CFLAGS) - | $(SED) 's/^\"\(Q(.*)\)\"/\1/' > $(HEADER_BUILD)/qstrdefs.preprocessed.h
	$(Q)$(PYTHON) $(PY_SRC)/makeqstrdata.py $(HEADER_BUILD)/qstrdefs.preprocessed.h > $@

$(HEADER_BUILD)/compressed.data.h: $(HEADER_BUILD)/compressed.collected
	$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makecompresseddata.py $< > $@


$(HEADER_BUILD)/moduledefs.h: $(HEADER_BUILD)/moduledefs.collected
	@$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/makemoduledefs.py $< > $@


$(HEADER_BUILD)/root_pointers.h: $(HEADER_BUILD)/root_pointers.collected $(PY_SRC)/make_root_pointers.py
	@$(ECHO) "GEN $@"
	$(Q)$(PYTHON) $(PY_SRC)/make_root_pointers.py $< > $@



CFLAGS_BUILTIN ?= -ffreestanding -fno-builtin -fno-lto
$(BUILD)/shared/libc/string0.o: CFLAGS += $(CFLAGS_BUILTIN)



$(PY_BUILD)/nlr%.o: CFLAGS += -Os


$(PY_BUILD)/gc.o: CFLAGS += $(CSUPEROPT)


$(PY_BUILD)/vm.o: CFLAGS += $(CSUPEROPT)








