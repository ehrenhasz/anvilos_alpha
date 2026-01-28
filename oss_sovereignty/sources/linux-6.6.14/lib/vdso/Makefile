

GENERIC_VDSO_MK_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
GENERIC_VDSO_DIR := $(dir $(GENERIC_VDSO_MK_PATH))

c-gettimeofday-$(CONFIG_GENERIC_GETTIMEOFDAY) := $(addprefix $(GENERIC_VDSO_DIR), gettimeofday.c)







quiet_cmd_vdso_check = VDSOCHK $@
      cmd_vdso_check = if $(READELF) -rW $@ | grep -v _NONE | grep -q " R_\w*_"; \
		       then (echo >&2 "$@: dynamic relocations are not supported"; \
			     rm -f $@; /bin/false); fi
