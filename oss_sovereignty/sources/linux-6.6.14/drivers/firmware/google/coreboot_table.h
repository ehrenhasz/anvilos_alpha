


#ifndef __COREBOOT_TABLE_H
#define __COREBOOT_TABLE_H

#include <linux/device.h>


struct coreboot_table_header {
	char signature[4];
	u32 header_bytes;
	u32 header_checksum;
	u32 table_bytes;
	u32 table_checksum;
	u32 table_entries;
};



struct coreboot_table_entry {
	u32 tag;
	u32 size;
};


struct lb_cbmem_ref {
	u32 tag;
	u32 size;

	u64 cbmem_addr;
};

#define LB_TAG_CBMEM_ENTRY 0x31


struct lb_cbmem_entry {
	u32 tag;
	u32 size;

	u64 address;
	u32 entry_size;
	u32 id;
};


struct lb_framebuffer {
	u32 tag;
	u32 size;

	u64 physical_address;
	u32 x_resolution;
	u32 y_resolution;
	u32 bytes_per_line;
	u8  bits_per_pixel;
	u8  red_mask_pos;
	u8  red_mask_size;
	u8  green_mask_pos;
	u8  green_mask_size;
	u8  blue_mask_pos;
	u8  blue_mask_size;
	u8  reserved_mask_pos;
	u8  reserved_mask_size;
};


struct coreboot_device {
	struct device dev;
	union {
		struct coreboot_table_entry entry;
		struct lb_cbmem_ref cbmem_ref;
		struct lb_cbmem_entry cbmem_entry;
		struct lb_framebuffer framebuffer;
		DECLARE_FLEX_ARRAY(u8, raw);
	};
};

static inline struct coreboot_device *dev_to_coreboot_device(struct device *dev)
{
	return container_of(dev, struct coreboot_device, dev);
}


struct coreboot_driver {
	int (*probe)(struct coreboot_device *);
	void (*remove)(struct coreboot_device *);
	struct device_driver drv;
	u32 tag;
};


int coreboot_driver_register(struct coreboot_driver *driver);


void coreboot_driver_unregister(struct coreboot_driver *driver);


#define module_coreboot_driver(__coreboot_driver) \
	module_driver(__coreboot_driver, coreboot_driver_register, \
			coreboot_driver_unregister)

#endif 
