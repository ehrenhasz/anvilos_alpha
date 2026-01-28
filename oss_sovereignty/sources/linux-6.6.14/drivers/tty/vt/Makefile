



FONTMAPFILE = cp437.uni

obj-$(CONFIG_VT)			+= vt_ioctl.o vc_screen.o \
					   selection.o keyboard.o
obj-$(CONFIG_CONSOLE_TRANSLATIONS)	+= consolemap.o consolemap_deftbl.o
obj-$(CONFIG_HW_CONSOLE)		+= vt.o defkeymap.o


clean-files := consolemap_deftbl.c defkeymap.c

hostprogs += conmakehash

quiet_cmd_conmk = CONMK   $@
      cmd_conmk = $(obj)/conmakehash $< > $@

$(obj)/consolemap_deftbl.c: $(src)/$(FONTMAPFILE) $(obj)/conmakehash
	$(call cmd,conmk)

$(obj)/defkeymap.o:  $(obj)/defkeymap.c






ifdef GENERATE_KEYMAP

$(obj)/defkeymap.c: $(obj)/%.c: $(src)/%.map
	loadkeys --mktable --unicode $< > $@

endif
