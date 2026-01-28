




compress-y					:= copy
compress-$(CONFIG_INITRAMFS_COMPRESSION_GZIP)	:= gzip
compress-$(CONFIG_INITRAMFS_COMPRESSION_BZIP2)	:= bzip2
compress-$(CONFIG_INITRAMFS_COMPRESSION_LZMA)	:= lzma
compress-$(CONFIG_INITRAMFS_COMPRESSION_XZ)	:= xzmisc
compress-$(CONFIG_INITRAMFS_COMPRESSION_LZO)	:= lzo
compress-$(CONFIG_INITRAMFS_COMPRESSION_LZ4)	:= lz4
compress-$(CONFIG_INITRAMFS_COMPRESSION_ZSTD)	:= zstd

obj-$(CONFIG_BLK_DEV_INITRD) := initramfs_data.o

$(obj)/initramfs_data.o: $(obj)/initramfs_inc_data

ramfs-input := $(CONFIG_INITRAMFS_SOURCE)
cpio-data :=



ifeq ($(ramfs-input),)
ramfs-input := $(srctree)/$(src)/default_cpio_list
endif

ifeq ($(words $(ramfs-input)),1)



ifneq ($(filter %.cpio,$(ramfs-input)),)
cpio-data := $(ramfs-input)
endif



ifeq ($(words $(subst .cpio.,$(space),$(ramfs-input))),2)
cpio-data := $(ramfs-input)
compress-y := copy
endif

endif



ifeq ($(cpio-data),)

cpio-data := $(obj)/initramfs_data.cpio

hostprogs := gen_init_cpio





-include $(obj)/.initramfs_data.cpio.d


$(deps_initramfs): ;

quiet_cmd_initfs = GEN     $@
      cmd_initfs = \
	$(CONFIG_SHELL) $< -o $@ -l $(obj)/.initramfs_data.cpio.d \
	$(if $(CONFIG_INITRAMFS_ROOT_UID), -u $(CONFIG_INITRAMFS_ROOT_UID)) \
	$(if $(CONFIG_INITRAMFS_ROOT_GID), -g $(CONFIG_INITRAMFS_ROOT_GID)) \
	$(if $(KBUILD_BUILD_TIMESTAMP), -d "$(KBUILD_BUILD_TIMESTAMP)") \
	$(ramfs-input)






$(obj)/initramfs_data.cpio: $(src)/gen_initramfs.sh $(obj)/gen_init_cpio $(deps_initramfs) FORCE
	$(call if_changed,initfs)

endif

$(obj)/initramfs_inc_data: $(cpio-data) FORCE
	$(call if_changed,$(compress-y))

targets += initramfs_data.cpio initramfs_inc_data

subdir-$(CONFIG_UAPI_HEADER_TEST) += include
