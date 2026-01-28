



obj-$(CONFIG_MICROSEMI_PHY) := mscc.o
mscc-objs := mscc_main.o
mscc-objs += mscc_serdes.o

ifdef CONFIG_MACSEC
mscc-objs += mscc_macsec.o
endif

ifdef CONFIG_NETWORK_PHY_TIMESTAMPING
mscc-objs += mscc_ptp.o
endif
