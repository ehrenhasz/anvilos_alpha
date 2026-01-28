












.SUFFIXES:




unexport MAKEFLAGS







ifeq ($(JOBS),)
  JOBS := $(shell (getconf _NPROCESSORS_ONLN || grep -E -c '^processor|^CPU[0-9]' /proc/cpuinfo) 2>/dev/null)
  ifeq ($(JOBS),0)
    JOBS := 1
  endif
endif




ifneq ($(O),)
  FULL_O := $(shell cd $(PWD); readlink -f $(O) || echo $(O))
endif




ifeq ("$(origin DEBUG)", "command line")
  ifeq ($(DEBUG),)
    override DEBUG = 0
  else
    SET_DEBUG = "DEBUG=$(DEBUG)"
  endif
else
  override DEBUG = 0
endif

define print_msg
  @printf '  BUILD:   Doing '\''make \033[33m-j'$(JOBS)'\033[m'\'' parallel build\n'
endef

define make
  @$(MAKE) -f Makefile.perf --no-print-directory -j$(JOBS) O=$(FULL_O) $(SET_DEBUG) $@
endef







all tags TAGS:
	$(print_msg)
	$(make)

ifdef MAKECMDGOALS
has_clean := 0
ifneq ($(filter clean,$(MAKECMDGOALS)),)
  has_clean := 1
endif 

ifeq ($(has_clean),1)
  rest := $(filter-out clean,$(MAKECMDGOALS))
  ifneq ($(rest),)
$(rest): clean
  endif 
endif 
endif 




clean:
	$(make) -j1











build-test:
	@$(MAKE) SHUF=1 -f tests/make REUSE_FEATURES_DUMP=1 MK=Makefile SET_PARALLEL=1 --no-print-directory tarpkg make_static make_with_gtk2 out

build-test-tarball:
	@$(MAKE) -f tests/make REUSE_FEATURES_DUMP=1 MK=Makefile SET_PARALLEL=1 --no-print-directory out




%: FORCE
	$(print_msg)
	$(make)

.PHONY: tags TAGS FORCE Makefile
