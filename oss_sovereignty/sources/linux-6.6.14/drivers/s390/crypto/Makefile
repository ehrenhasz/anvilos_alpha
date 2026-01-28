




ap-objs := ap_bus.o ap_card.o ap_queue.o
obj-$(subst m,y,$(CONFIG_ZCRYPT)) += ap.o

zcrypt-objs := zcrypt_api.o zcrypt_card.o zcrypt_queue.o
zcrypt-objs += zcrypt_msgtype6.o zcrypt_msgtype50.o
zcrypt-objs += zcrypt_ccamisc.o zcrypt_ep11misc.o
obj-$(CONFIG_ZCRYPT) += zcrypt.o

obj-$(CONFIG_ZCRYPT) += zcrypt_cex4.o


pkey-objs := pkey_api.o
obj-$(CONFIG_PKEY) += pkey.o


vfio_ap-objs := vfio_ap_drv.o vfio_ap_ops.o
obj-$(CONFIG_VFIO_AP) += vfio_ap.o
