
 
 

static const char version[] =
	"smc-ultra.c:v2.02 2/3/98 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/isapnp.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/Space.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "8390.h"

#define DRV_NAME "smc-ultra"

 
static unsigned int ultra_portlist[] __initdata =
{0x200, 0x220, 0x240, 0x280, 0x300, 0x340, 0x380, 0};

static int ultra_probe1(struct net_device *dev, int ioaddr);

#ifdef __ISAPNP__
static int ultra_probe_isapnp(struct net_device *dev);
#endif

static int ultra_open(struct net_device *dev);
static void ultra_reset_8390(struct net_device *dev);
static void ultra_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
						int ring_page);
static void ultra_block_input(struct net_device *dev, int count,
						  struct sk_buff *skb, int ring_offset);
static void ultra_block_output(struct net_device *dev, int count,
							const unsigned char *buf, const int start_page);
static void ultra_pio_get_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
						int ring_page);
static void ultra_pio_input(struct net_device *dev, int count,
						  struct sk_buff *skb, int ring_offset);
static void ultra_pio_output(struct net_device *dev, int count,
							 const unsigned char *buf, const int start_page);
static int ultra_close_card(struct net_device *dev);

#ifdef __ISAPNP__
static struct isapnp_device_id ultra_device_ids[] __initdata = {
        {       ISAPNP_VENDOR('S','M','C'), ISAPNP_FUNCTION(0x8416),
                ISAPNP_VENDOR('S','M','C'), ISAPNP_FUNCTION(0x8416),
                (long) "SMC EtherEZ (8416)" },
        { }	 
};

MODULE_DEVICE_TABLE(isapnp, ultra_device_ids);
#endif

static u32 ultra_msg_enable;

#define START_PG		0x00	 

#define ULTRA_CMDREG	0		 
#define	 ULTRA_RESET	0x80	 
#define	 ULTRA_MEMENB	0x40	 
#define IOPD	0x02			 
#define IOPA	0x07			 
#define ULTRA_NIC_OFFSET  16	 
#define ULTRA_IO_EXTENT 32
#define EN0_ERWCNT		0x08	 

#ifdef CONFIG_NET_POLL_CONTROLLER
static void ultra_poll(struct net_device *dev)
{
	disable_irq(dev->irq);
	ei_interrupt(dev->irq, dev);
	enable_irq(dev->irq);
}
#endif
 

static int __init do_ultra_probe(struct net_device *dev)
{
	int i;
	int base_addr = dev->base_addr;
	int irq = dev->irq;

	if (base_addr > 0x1ff)		 
		return ultra_probe1(dev, base_addr);
	else if (base_addr != 0)	 
		return -ENXIO;

#ifdef __ISAPNP__
	 
	if (isapnp_present() && (ultra_probe_isapnp(dev) == 0))
		return 0;
#endif

	for (i = 0; ultra_portlist[i]; i++) {
		dev->irq = irq;
		if (ultra_probe1(dev, ultra_portlist[i]) == 0)
			return 0;
	}

	return -ENODEV;
}

#ifndef MODULE
struct net_device * __init ultra_probe(int unit)
{
	struct net_device *dev = alloc_ei_netdev();
	int err;

	if (!dev)
		return ERR_PTR(-ENOMEM);

	sprintf(dev->name, "eth%d", unit);
	netdev_boot_setup_check(dev);

	err = do_ultra_probe(dev);
	if (err)
		goto out;
	return dev;
out:
	free_netdev(dev);
	return ERR_PTR(err);
}
#endif

static const struct net_device_ops ultra_netdev_ops = {
	.ndo_open		= ultra_open,
	.ndo_stop		= ultra_close_card,

	.ndo_start_xmit		= ei_start_xmit,
	.ndo_tx_timeout		= ei_tx_timeout,
	.ndo_get_stats		= ei_get_stats,
	.ndo_set_rx_mode	= ei_set_multicast_list,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= eth_mac_addr,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller 	= ultra_poll,
#endif
};

static int __init ultra_probe1(struct net_device *dev, int ioaddr)
{
	int i, retval;
	int checksum = 0;
	u8 macaddr[ETH_ALEN];
	const char *model_name;
	unsigned char eeprom_irq = 0;
	static unsigned version_printed;
	 
	unsigned char num_pages, irqreg, addr, piomode;
	unsigned char idreg = inb(ioaddr + 7);
	unsigned char reg4 = inb(ioaddr + 4) & 0x7f;
	struct ei_device *ei_local = netdev_priv(dev);

	if (!request_region(ioaddr, ULTRA_IO_EXTENT, DRV_NAME))
		return -EBUSY;

	 
	if ((idreg & 0xF0) != 0x20 			 
		&& (idreg & 0xF0) != 0x40) {		 
		retval = -ENODEV;
		goto out;
	}

	 
	outb(reg4, ioaddr + 4);

	for (i = 0; i < 8; i++)
		checksum += inb(ioaddr + 8 + i);
	if ((checksum & 0xff) != 0xFF) {
		retval = -ENODEV;
		goto out;
	}

	if ((ultra_msg_enable & NETIF_MSG_DRV) && (version_printed++ == 0))
		netdev_info(dev, version);

	model_name = (idreg & 0xF0) == 0x20 ? "SMC Ultra" : "SMC EtherEZ";

	for (i = 0; i < 6; i++)
		macaddr[i] = inb(ioaddr + 8 + i);
	eth_hw_addr_set(dev, macaddr);

	netdev_info(dev, "%s at %#3x, %pM", model_name,
		    ioaddr, dev->dev_addr);

	 
	outb(0x80 | reg4, ioaddr + 4);

	 
	outb(0x80 | inb(ioaddr + 0x0c), ioaddr + 0x0c);
	piomode = inb(ioaddr + 0x8);
	addr = inb(ioaddr + 0xb);
	irqreg = inb(ioaddr + 0xd);

	 
	outb(reg4, ioaddr + 4);

	if (dev->irq < 2) {
		unsigned char irqmap[] = {0, 9, 3, 5, 7, 10, 11, 15};
		int irq;

		 
		irq = irqmap[((irqreg & 0x40) >> 4) + ((irqreg & 0x0c) >> 2)];

		if (irq == 0) {
			pr_cont(", failed to detect IRQ line.\n");
			retval =  -EAGAIN;
			goto out;
		}
		dev->irq = irq;
		eeprom_irq = 1;
	}

	 
	dev->base_addr = ioaddr+ULTRA_NIC_OFFSET;

	{
		static const int addr_tbl[4] = {
			0x0C0000, 0x0E0000, 0xFC0000, 0xFE0000
		};
		static const short num_pages_tbl[4] = {
			0x20, 0x40, 0x80, 0xff
		};

		dev->mem_start = ((addr & 0x0f) << 13) + addr_tbl[(addr >> 6) & 3] ;
		num_pages = num_pages_tbl[(addr >> 4) & 3];
	}

	ei_status.name = model_name;
	ei_status.word16 = 1;
	ei_status.tx_start_page = START_PG;
	ei_status.rx_start_page = START_PG + TX_PAGES;
	ei_status.stop_page = num_pages;

	ei_status.mem = ioremap(dev->mem_start, (ei_status.stop_page - START_PG)*256);
	if (!ei_status.mem) {
		pr_cont(", failed to ioremap.\n");
		retval =  -ENOMEM;
		goto out;
	}

	dev->mem_end = dev->mem_start + (ei_status.stop_page - START_PG)*256;

	if (piomode) {
		pr_cont(", %s IRQ %d programmed-I/O mode.\n",
			eeprom_irq ? "EEPROM" : "assigned ", dev->irq);
		ei_status.block_input = &ultra_pio_input;
		ei_status.block_output = &ultra_pio_output;
		ei_status.get_8390_hdr = &ultra_pio_get_hdr;
	} else {
		pr_cont(", %s IRQ %d memory %#lx-%#lx.\n",
			eeprom_irq ? "" : "assigned ", dev->irq, dev->mem_start,
			dev->mem_end-1);
		ei_status.block_input = &ultra_block_input;
		ei_status.block_output = &ultra_block_output;
		ei_status.get_8390_hdr = &ultra_get_8390_hdr;
	}
	ei_status.reset_8390 = &ultra_reset_8390;

	dev->netdev_ops = &ultra_netdev_ops;
	NS8390_init(dev, 0);
	ei_local->msg_enable = ultra_msg_enable;

	retval = register_netdev(dev);
	if (retval)
		goto out;
	return 0;
out:
	release_region(ioaddr, ULTRA_IO_EXTENT);
	return retval;
}

#ifdef __ISAPNP__
static int __init ultra_probe_isapnp(struct net_device *dev)
{
        int i;

        for (i = 0; ultra_device_ids[i].vendor != 0; i++) {
		struct pnp_dev *idev = NULL;

                while ((idev = pnp_find_dev(NULL,
                                            ultra_device_ids[i].vendor,
                                            ultra_device_ids[i].function,
                                            idev))) {
                         
                        if (pnp_device_attach(idev) < 0)
				continue;
                        if (pnp_activate_dev(idev) < 0) {
                              __again:
				pnp_device_detach(idev);
				continue;
                        }
			 
			if (!pnp_port_valid(idev, 0) || !pnp_irq_valid(idev, 0))
				goto __again;
                         
			dev->base_addr = pnp_port_start(idev, 0);
			dev->irq = pnp_irq(idev, 0);
			netdev_info(dev,
				    "smc-ultra.c: ISAPnP reports %s at i/o %#lx, irq %d.\n",
				    (char *) ultra_device_ids[i].driver_data,
				    dev->base_addr, dev->irq);
                        if (ultra_probe1(dev, dev->base_addr) != 0) {       
				netdev_err(dev,
					   "smc-ultra.c: Probe of ISAPnP card at %#lx failed.\n",
					   dev->base_addr);
				pnp_device_detach(idev);
				return -ENXIO;
                        }
                        ei_status.priv = (unsigned long)idev;
                        break;
                }
                if (!idev)
                        continue;
                return 0;
        }

        return -ENODEV;
}
#endif

static int
ultra_open(struct net_device *dev)
{
	int retval;
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;  
	unsigned char irq2reg[] = {0, 0, 0x04, 0x08, 0, 0x0C, 0, 0x40,
				   0, 0x04, 0x44, 0x48, 0, 0, 0, 0x4C, };

	retval = request_irq(dev->irq, ei_interrupt, 0, dev->name, dev);
	if (retval)
		return retval;

	outb(0x00, ioaddr);	 
	outb(0x80, ioaddr + 5);
	 
	outb(inb(ioaddr + 4) | 0x80, ioaddr + 4);
	outb((inb(ioaddr + 13) & ~0x4C) | irq2reg[dev->irq], ioaddr + 13);
	outb(inb(ioaddr + 4) & 0x7f, ioaddr + 4);

	if (ei_status.block_input == &ultra_pio_input) {
		outb(0x11, ioaddr + 6);		 
		outb(0x01, ioaddr + 0x19);  	 
	} else
		outb(0x01, ioaddr + 6);		 
	 
	outb_p(E8390_NODMA+E8390_PAGE0, dev->base_addr);
	outb(0xff, dev->base_addr + EN0_ERWCNT);
	ei_open(dev);
	return 0;
}

static void
ultra_reset_8390(struct net_device *dev)
{
	int cmd_port = dev->base_addr - ULTRA_NIC_OFFSET;  
	struct ei_device *ei_local = netdev_priv(dev);

	outb(ULTRA_RESET, cmd_port);
	netif_dbg(ei_local, hw, dev, "resetting Ultra, t=%ld...\n", jiffies);
	ei_status.txing = 0;

	outb(0x00, cmd_port);	 
	outb(0x80, cmd_port + 5);
	if (ei_status.block_input == &ultra_pio_input)
		outb(0x11, cmd_port + 6);		 
	else
		outb(0x01, cmd_port + 6);		 

	netif_dbg(ei_local, hw, dev, "reset done\n");
}

 

static void
ultra_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	void __iomem *hdr_start = ei_status.mem + ((ring_page - START_PG)<<8);

	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);	 
#ifdef __BIG_ENDIAN
	 
	 
	memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
	hdr->count = le16_to_cpu(hdr->count);
#else
	((unsigned int*)hdr)[0] = readl(hdr_start);
#endif
	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET);  
}

 

static void
ultra_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	void __iomem *xfer_start = ei_status.mem + ring_offset - (START_PG<<8);

	 
	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

	if (ring_offset + count > ei_status.stop_page*256) {
		 
		int semi_count = ei_status.stop_page*256 - ring_offset;
		memcpy_fromio(skb->data, xfer_start, semi_count);
		count -= semi_count;
		memcpy_fromio(skb->data + semi_count, ei_status.mem + TX_PAGES * 256, count);
	} else {
		memcpy_fromio(skb->data, xfer_start, count);
	}

	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET);	 
}

static void
ultra_block_output(struct net_device *dev, int count, const unsigned char *buf,
				int start_page)
{
	void __iomem *shmem = ei_status.mem + ((start_page - START_PG)<<8);

	 
	outb(ULTRA_MEMENB, dev->base_addr - ULTRA_NIC_OFFSET);

	memcpy_toio(shmem, buf, count);

	outb(0x00, dev->base_addr - ULTRA_NIC_OFFSET);  
}

 
static void ultra_pio_get_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
						int ring_page)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;  
	outb(0x00, ioaddr + IOPA);	 
	outb(ring_page, ioaddr + IOPA);
	insw(ioaddr + IOPD, hdr, sizeof(struct e8390_pkt_hdr)>>1);
}

static void ultra_pio_input(struct net_device *dev, int count,
						  struct sk_buff *skb, int ring_offset)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;  
    char *buf = skb->data;

	 
	outb(ring_offset, ioaddr + IOPA);	 
	outb(ring_offset >> 8, ioaddr + IOPA);
	 
	insw(ioaddr + IOPD, buf, (count+1)>>1);
}
static void ultra_pio_output(struct net_device *dev, int count,
							const unsigned char *buf, const int start_page)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;  
	outb(0x00, ioaddr + IOPA);	 
	outb(start_page, ioaddr + IOPA);
	 
	outsw(ioaddr + IOPD, buf, (count+1)>>1);
}

static int
ultra_close_card(struct net_device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;  
	struct ei_device *ei_local = netdev_priv(dev);

	netif_stop_queue(dev);

	netif_dbg(ei_local, ifdown, dev, "Shutting down ethercard.\n");

	outb(0x00, ioaddr + 6);		 
	free_irq(dev->irq, dev);

	NS8390_init(dev, 0);

	 

	return 0;
}


#ifdef MODULE
#define MAX_ULTRA_CARDS	4	 
static struct net_device *dev_ultra[MAX_ULTRA_CARDS];
static int io[MAX_ULTRA_CARDS];
static int irq[MAX_ULTRA_CARDS];

module_param_hw_array(io, int, ioport, NULL, 0);
module_param_hw_array(irq, int, irq, NULL, 0);
module_param_named(msg_enable, ultra_msg_enable, uint, 0444);
MODULE_PARM_DESC(io, "I/O base address(es)");
MODULE_PARM_DESC(irq, "IRQ number(s) (assigned)");
MODULE_PARM_DESC(msg_enable, "Debug message level (see linux/netdevice.h for bitmap)");
MODULE_DESCRIPTION("SMC Ultra/EtherEZ ISA/PnP Ethernet driver");
MODULE_LICENSE("GPL");

 
static int __init ultra_init_module(void)
{
	struct net_device *dev;
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_ULTRA_CARDS; this_dev++) {
		if (io[this_dev] == 0)  {
			if (this_dev != 0) break;  
			printk(KERN_NOTICE "smc-ultra.c: Presently autoprobing (not recommended) for a single card.\n");
		}
		dev = alloc_ei_netdev();
		if (!dev)
			break;
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		if (do_ultra_probe(dev) == 0) {
			dev_ultra[found++] = dev;
			continue;
		}
		free_netdev(dev);
		printk(KERN_WARNING "smc-ultra.c: No SMC Ultra card found (i/o = 0x%x).\n", io[this_dev]);
		break;
	}
	if (found)
		return 0;
	return -ENXIO;
}
module_init(ultra_init_module);

static void cleanup_card(struct net_device *dev)
{
	 
#ifdef __ISAPNP__
	struct pnp_dev *idev = (struct pnp_dev *)ei_status.priv;
	if (idev)
		pnp_device_detach(idev);
#endif
	release_region(dev->base_addr - ULTRA_NIC_OFFSET, ULTRA_IO_EXTENT);
	iounmap(ei_status.mem);
}

static void __exit ultra_cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_ULTRA_CARDS; this_dev++) {
		struct net_device *dev = dev_ultra[this_dev];
		if (dev) {
			unregister_netdev(dev);
			cleanup_card(dev);
			free_netdev(dev);
		}
	}
}
module_exit(ultra_cleanup_module);
#endif  
