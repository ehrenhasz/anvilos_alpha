#ifndef _SNET_VDPA_H_
#define _SNET_VDPA_H_
#include <linux/vdpa.h>
#include <linux/pci.h>
#define SNET_NAME_SIZE 256
#define SNET_ERR(pdev, fmt, ...) dev_err(&(pdev)->dev, "%s"fmt, "snet_vdpa: ", ##__VA_ARGS__)
#define SNET_WARN(pdev, fmt, ...) dev_warn(&(pdev)->dev, "%s"fmt, "snet_vdpa: ", ##__VA_ARGS__)
#define SNET_INFO(pdev, fmt, ...) dev_info(&(pdev)->dev, "%s"fmt, "snet_vdpa: ", ##__VA_ARGS__)
#define SNET_DBG(pdev, fmt, ...) dev_dbg(&(pdev)->dev, "%s"fmt, "snet_vdpa: ", ##__VA_ARGS__)
#define SNET_HAS_FEATURE(s, f) ((s)->negotiated_features & BIT_ULL(f))
#define SNET_CFG_VER(snet, ver) ((snet)->psnet->negotiated_cfg_ver >= (ver))
struct snet_vq {
	struct vdpa_callback cb;
	struct vdpa_vq_state vq_state;
	u64 desc_area;
	u64 device_area;
	u64 driver_area;
	u32 num;
	u32 sid;
	bool ready;
	u32 irq;
	u32 irq_idx;
	char irq_name[SNET_NAME_SIZE];
	void __iomem *kick_ptr;
};
struct snet {
	struct vdpa_device vdpa;
	struct vdpa_callback cb;
	struct mutex ctrl_lock;
	spinlock_t ctrl_spinlock;
	struct snet_vq **vqs;
	u64 negotiated_features;
	u32 sid;
	u8 status;
	bool dpu_ready;
	u32 cfg_irq;
	u32 cfg_irq_idx;
	char cfg_irq_name[SNET_NAME_SIZE];
	void __iomem *bar;
	struct pci_dev *pdev;
	struct psnet *psnet;
	struct snet_dev_cfg *cfg;
};
struct snet_dev_cfg {
	u32 virtio_id;
	u32 vq_num;
	u32 vq_size;
	u32 vfid;
	u64 features;
	u32 rsvd[6];
	u32 cfg_size;
	void __iomem *virtio_cfg;
} __packed;
struct snet_cfg {
	u32 key;
	u32 cfg_size;
	u32 cfg_ver;
	u32 vf_num;
	u32 vf_bar;
	u32 host_cfg_off;
	u32 max_size_host_cfg;
	u32 virtio_cfg_off;
	u32 kick_off;
	u32 hwmon_off;
	u32 ctrl_off;
	u32 flags;
	u32 rsvd[6];
	u32 devices_num;
	struct snet_dev_cfg **devs;
} __packed;
struct psnet {
	void __iomem *bars[PCI_STD_NUM_BARS];
	u32 negotiated_cfg_ver;
	u32 next_irq;
	u8 barno;
	spinlock_t lock;
	struct snet_cfg cfg;
	char hwmon_name[SNET_NAME_SIZE];
};
enum snet_cfg_flags {
	SNET_CFG_FLAG_HWMON = BIT(0),
	SNET_CFG_FLAG_IRQ_PF = BIT(1),
};
#define PSNET_FLAG_ON(p, f)	((p)->cfg.flags & (f))
static inline u32 psnet_read32(struct psnet *psnet, u32 off)
{
	return ioread32(psnet->bars[psnet->barno] + off);
}
static inline u32 snet_read32(struct snet *snet, u32 off)
{
	return ioread32(snet->bar + off);
}
static inline void snet_write32(struct snet *snet, u32 off, u32 val)
{
	iowrite32(val, snet->bar + off);
}
static inline u64 psnet_read64(struct psnet *psnet, u32 off)
{
	u64 val;
	val = (u64)psnet_read32(psnet, off);
	val |= ((u64)psnet_read32(psnet, off + 4) << 32);
	return val;
}
static inline void snet_write64(struct snet *snet, u32 off, u64 val)
{
	snet_write32(snet, off, (u32)val);
	snet_write32(snet, off + 4, (u32)(val >> 32));
}
#if IS_ENABLED(CONFIG_HWMON)
void psnet_create_hwmon(struct pci_dev *pdev);
#endif
void snet_ctrl_clear(struct snet *snet);
int snet_destroy_dev(struct snet *snet);
int snet_read_vq_state(struct snet *snet, u16 idx, struct vdpa_vq_state *state);
int snet_suspend_dev(struct snet *snet);
int snet_resume_dev(struct snet *snet);
#endif  
