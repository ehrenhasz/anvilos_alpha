






obj-$(CONFIG_FM10K) += fm10k.o

fm10k-y := fm10k_main.o \
	   fm10k_common.o \
	   fm10k_pci.o \
	   fm10k_netdev.o \
	   fm10k_ethtool.o \
	   fm10k_pf.o \
	   fm10k_vf.o \
	   fm10k_mbx.o \
	   fm10k_iov.o \
	   fm10k_tlv.o

fm10k-$(CONFIG_DEBUG_FS) += fm10k_debugfs.o
fm10k-$(CONFIG_DCB) += fm10k_dcbnl.o
