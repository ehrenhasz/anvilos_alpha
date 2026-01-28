


obj-$(CONFIG_EISA)	        += eisa-bus.o
obj-${CONFIG_EISA_PCI_EISA}     += pci_eisa.o



obj-${CONFIG_EISA_VIRTUAL_ROOT} += virtual_root.o



DEVICE_NAME_SIZE = 50

$(obj)/eisa-bus.o: $(obj)/devlist.h

quiet_cmd_eisaid = GEN     $@
      cmd_eisaid = sed -e '/^\

clean-files := devlist.h
$(obj)/devlist.h: $(src)/eisa.ids include/linux/device.h
	$(call cmd,eisaid)
