







UAPI_CFLAGS := -std=c90 -Wall -Werror=implicit-function-declaration



UAPI_CFLAGS += $(filter -m32 -m64 --target=%, $(KBUILD_CFLAGS))


UAPI_CFLAGS += $(USERCFLAGS)

override c_flags = $(UAPI_CFLAGS) -Wp,-MMD,$(depfile) -I $(obj) -I $(srctree)/usr/dummy-include







no-header-test += asm/ucontext.h
no-header-test += drm/vmwgfx_drm.h
no-header-test += linux/am437x-vpfe.h
no-header-test += linux/coda.h
no-header-test += linux/cyclades.h
no-header-test += linux/errqueue.h
no-header-test += linux/hdlc/ioctl.h
no-header-test += linux/ivtv.h
no-header-test += linux/matroxfb.h
no-header-test += linux/omap3isp.h
no-header-test += linux/omapfb.h
no-header-test += linux/patchkey.h
no-header-test += linux/phonet.h
no-header-test += linux/sctp.h
no-header-test += linux/sysctl.h
no-header-test += linux/usb/audio.h
no-header-test += linux/v4l2-mediabus.h
no-header-test += linux/v4l2-subdev.h
no-header-test += linux/videodev2.h
no-header-test += linux/vm_sockets.h
no-header-test += sound/asequencer.h
no-header-test += sound/asoc.h
no-header-test += sound/asound.h
no-header-test += sound/compress_offload.h
no-header-test += sound/emu10k1.h
no-header-test += sound/sfnt_info.h
no-header-test += xen/evtchn.h
no-header-test += xen/gntdev.h
no-header-test += xen/privcmd.h



ifeq ($(SRCARCH),arc)
no-header-test += linux/bpf_perf_event.h
endif

ifeq ($(SRCARCH),ia64)
no-header-test += asm/setup.h
no-header-test += asm/sigcontext.h
no-header-test += linux/if_bonding.h
endif

ifeq ($(SRCARCH),powerpc)
no-header-test += linux/bpf_perf_event.h
endif

ifeq ($(SRCARCH),sparc)
no-header-test += asm/uctx.h
no-header-test += asm/fbio.h
endif


no-header-test += asm-generic/%

always-y := $(patsubst $(obj)/%.h,%.hdrtest, $(shell find $(obj) -name '*.h' 2>/dev/null))


quiet_cmd_hdrtest = HDRTEST $<
      cmd_hdrtest = \
		$(CC) $(c_flags) -fsyntax-only -x c /dev/null \
			$(if $(filter-out $(no-header-test), $*.h), -include $< -include $<); \
		$(PERL) $(srctree)/$(src)/headers_check.pl $(obj) $(SRCARCH) $<; \
		touch $@

$(obj)/%.hdrtest: $(obj)/%.h FORCE
	$(call if_changed_dep,hdrtest)



clean-files += $(filter-out Makefile headers_check.pl, $(notdir $(wildcard $(obj)/*)))
