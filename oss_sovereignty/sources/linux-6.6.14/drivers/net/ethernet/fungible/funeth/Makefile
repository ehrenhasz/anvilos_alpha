

ccflags-y += -I$(srctree)/$(src)/../funcore -I$(srctree)/$(src)

obj-$(CONFIG_FUN_ETH) += funeth.o

funeth-y := funeth_main.o funeth_rx.o funeth_tx.o funeth_devlink.o \
	    funeth_ethtool.o

funeth-$(CONFIG_TLS_DEVICE) += funeth_ktls.o
