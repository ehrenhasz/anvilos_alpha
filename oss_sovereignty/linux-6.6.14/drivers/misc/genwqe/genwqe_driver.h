#ifndef __GENWQE_DRIVER_H__
#define __GENWQE_DRIVER_H__
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/scatterlist.h>
#include <linux/iommu.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#include <asm/byteorder.h>
#include <linux/genwqe/genwqe_card.h>
#define DRV_VERSION		"2.0.25"
#define GENWQE_MAX_MINOR	128  
struct genwqe_ddcb_cmd *ddcb_requ_alloc(void);
void ddcb_requ_free(struct genwqe_ddcb_cmd *req);
u32  genwqe_crc32(u8 *buff, size_t len, u32 init);
static inline void genwqe_hexdump(struct pci_dev *pci_dev,
				  const void *buff, unsigned int size)
{
	char prefix[32];
	scnprintf(prefix, sizeof(prefix), "%s %s: ",
		  GENWQE_DEVNAME, pci_name(pci_dev));
	print_hex_dump_debug(prefix, DUMP_PREFIX_OFFSET, 16, 1, buff,
			     size, true);
}
#endif	 
