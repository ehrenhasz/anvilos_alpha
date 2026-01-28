#ifndef __ISST_IF_COMMON_H
#define __ISST_IF_COMMON_H
#define PCI_DEVICE_ID_INTEL_RAPL_PRIO_DEVID_0	0x3451
#define PCI_DEVICE_ID_INTEL_CFG_MBOX_DEVID_0	0x3459
#define PCI_DEVICE_ID_INTEL_RAPL_PRIO_DEVID_1	0x3251
#define PCI_DEVICE_ID_INTEL_CFG_MBOX_DEVID_1	0x3259
#define ISST_IF_CMD_LIMIT	64
#define ISST_IF_API_VERSION	0x01
#define ISST_IF_DRIVER_VERSION	0x01
#define ISST_IF_DEV_MBOX	0
#define ISST_IF_DEV_MMIO	1
#define ISST_IF_DEV_TPMI	2
#define ISST_IF_DEV_MAX		3
struct isst_if_cmd_cb {
	int registered;
	int cmd_size;
	int offset;
	int api_version;
	struct module *owner;
	long (*cmd_callback)(u8 *ptr, int *write_only, int resume);
	long (*def_ioctl)(struct file *file, unsigned int cmd, unsigned long arg);
};
int isst_if_cdev_register(int type, struct isst_if_cmd_cb *cb);
void isst_if_cdev_unregister(int type);
struct pci_dev *isst_if_get_pci_dev(int cpu, int bus, int dev, int fn);
bool isst_if_mbox_cmd_set_req(struct isst_if_mbox_cmd *mbox_cmd);
bool isst_if_mbox_cmd_invalid(struct isst_if_mbox_cmd *cmd);
int isst_store_cmd(int cmd, int sub_command, u32 cpu, int mbox_cmd,
		   u32 param, u64 data);
void isst_resume_common(void);
#endif
