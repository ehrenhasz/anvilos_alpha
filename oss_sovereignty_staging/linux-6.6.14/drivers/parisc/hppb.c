
 

#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/parisc-device.h>

#include "iommu.h"

struct hppb_card {
	unsigned long hpa;
	struct resource mmio_region;
	struct hppb_card *next;
};

static struct hppb_card hppb_card_head = {
	.hpa = 0,
	.next = NULL,
};

#define IO_IO_LOW  offsetof(struct bc_module, io_io_low)
#define IO_IO_HIGH offsetof(struct bc_module, io_io_high)

 
static int __init hppb_probe(struct parisc_device *dev)
{
	int status;
	struct hppb_card *card = &hppb_card_head;

	while(card->next) {
		card = card->next;
	}

	if(card->hpa) {
		card->next = kzalloc(sizeof(struct hppb_card), GFP_KERNEL);
		if(!card->next) {
			printk(KERN_ERR "HP-PB: Unable to allocate memory.\n");
			return 1;
		}
		card = card->next;
	}

	card->hpa = dev->hpa.start;
	card->mmio_region.name = "HP-PB Bus";
	card->mmio_region.flags = IORESOURCE_MEM;

	card->mmio_region.start = gsc_readl(dev->hpa.start + IO_IO_LOW);
	card->mmio_region.end = gsc_readl(dev->hpa.start + IO_IO_HIGH) - 1;

	status = ccio_request_resource(dev, &card->mmio_region);

	pr_info("Found GeckoBoa at %pap, bus space %pR,%s claimed.\n",
			&dev->hpa.start,
			&card->mmio_region,
			(status < 0) ? " not":"" );

        return 0;
}

static const struct parisc_device_id hppb_tbl[] __initconst = {
        { HPHW_BCPORT, HVERSION_REV_ANY_ID, 0x500, 0xc },  
        { HPHW_BCPORT, 0x0, 0x501, 0xc },  
        { HPHW_BCPORT, 0x0, 0x502, 0xc },  
        { HPHW_BCPORT, 0x0, 0x503, 0xc },  
        { 0, }
};

static struct parisc_driver hppb_driver __refdata = {
        .name =         "gecko_boa",
        .id_table =     hppb_tbl,
	.probe =        hppb_probe,
};

 
static int __init hppb_init(void)
{
        return register_parisc_driver(&hppb_driver);
}
arch_initcall(hppb_init);
