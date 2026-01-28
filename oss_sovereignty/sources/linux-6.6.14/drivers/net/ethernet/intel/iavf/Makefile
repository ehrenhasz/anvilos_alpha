







ccflags-y += -I$(src)
subdir-ccflags-y += -I$(src)

obj-$(CONFIG_IAVF) += iavf.o

iavf-objs := iavf_main.o iavf_ethtool.o iavf_virtchnl.o iavf_fdir.o \
	     iavf_adv_rss.o \
	     iavf_txrx.o iavf_common.o iavf_adminq.o iavf_client.o
