
 

#include <linux/memblock.h>
#include <linux/blkdev.h>
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/efi.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/acpi.h>
#include <linux/iscsi_ibft.h>

#include <asm/mmzone.h>

 
phys_addr_t ibft_phys_addr;
EXPORT_SYMBOL_GPL(ibft_phys_addr);

static const struct {
	char *sign;
} ibft_signs[] = {
	{ "iBFT" },
	{ "BIFT" },	 
};

#define IBFT_SIGN_LEN 4
#define VGA_MEM 0xA0000  
#define VGA_SIZE 0x20000  

 
void __init reserve_ibft_region(void)
{
	unsigned long pos, virt_pos = 0;
	unsigned int len = 0;
	void *virt = NULL;
	int i;

	ibft_phys_addr = 0;

	 
	if (efi_enabled(EFI_BOOT))
		return;

	for (pos = IBFT_START; pos < IBFT_END; pos += 16) {
		 
		if (pos == VGA_MEM)
			pos += VGA_SIZE;

		 
		if (offset_in_page(pos) == 0) {
			if (virt)
				early_memunmap(virt, PAGE_SIZE);
			virt = early_memremap_ro(pos, PAGE_SIZE);
			virt_pos = pos;
		}

		for (i = 0; i < ARRAY_SIZE(ibft_signs); i++) {
			if (memcmp(virt + (pos - virt_pos), ibft_signs[i].sign,
				   IBFT_SIGN_LEN) == 0) {
				unsigned long *addr =
				    (unsigned long *)(virt + pos - virt_pos + 4);
				len = *addr;
				 
				if (pos + len <= (IBFT_END-1)) {
					ibft_phys_addr = pos;
					memblock_reserve(ibft_phys_addr, PAGE_ALIGN(len));
					pr_info("iBFT found at %pa.\n", &ibft_phys_addr);
					goto out;
				}
			}
		}
	}

out:
	early_memunmap(virt, PAGE_SIZE);
}
