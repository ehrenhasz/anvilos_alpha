 

#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/sizes.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/aer.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/ntb.h>

#include "ntb_hw_idt.h"

#define NTB_NAME	"ntb_hw_idt"
#define NTB_DESC	"IDT PCI-E Non-Transparent Bridge Driver"
#define NTB_VER		"2.0"
#define NTB_IRQNAME	"ntb_irq_idt"

MODULE_DESCRIPTION(NTB_DESC);
MODULE_VERSION(NTB_VER);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("T-platforms");

 
static const struct idt_ntb_regs ntdata_tbl = {
	{ {IDT_NT_BARSETUP0,	IDT_NT_BARLIMIT0,
	   IDT_NT_BARLTBASE0,	IDT_NT_BARUTBASE0},
	  {IDT_NT_BARSETUP1,	IDT_NT_BARLIMIT1,
	   IDT_NT_BARLTBASE1,	IDT_NT_BARUTBASE1},
	  {IDT_NT_BARSETUP2,	IDT_NT_BARLIMIT2,
	   IDT_NT_BARLTBASE2,	IDT_NT_BARUTBASE2},
	  {IDT_NT_BARSETUP3,	IDT_NT_BARLIMIT3,
	   IDT_NT_BARLTBASE3,	IDT_NT_BARUTBASE3},
	  {IDT_NT_BARSETUP4,	IDT_NT_BARLIMIT4,
	   IDT_NT_BARLTBASE4,	IDT_NT_BARUTBASE4},
	  {IDT_NT_BARSETUP5,	IDT_NT_BARLIMIT5,
	   IDT_NT_BARLTBASE5,	IDT_NT_BARUTBASE5} },
	{ {IDT_NT_INMSG0,	IDT_NT_OUTMSG0,	IDT_NT_INMSGSRC0},
	  {IDT_NT_INMSG1,	IDT_NT_OUTMSG1,	IDT_NT_INMSGSRC1},
	  {IDT_NT_INMSG2,	IDT_NT_OUTMSG2,	IDT_NT_INMSGSRC2},
	  {IDT_NT_INMSG3,	IDT_NT_OUTMSG3,	IDT_NT_INMSGSRC3} }
};

 
static const struct idt_ntb_port portdata_tbl[IDT_MAX_NR_PORTS] = {
 	{ IDT_SW_NTP0_PCIECMDSTS,	IDT_SW_NTP0_PCIELCTLSTS,
	  IDT_SW_NTP0_NTCTL,
	  IDT_SW_SWPORT0CTL,		IDT_SW_SWPORT0STS,
	  { {IDT_SW_NTP0_BARSETUP0,	IDT_SW_NTP0_BARLIMIT0,
	     IDT_SW_NTP0_BARLTBASE0,	IDT_SW_NTP0_BARUTBASE0},
	    {IDT_SW_NTP0_BARSETUP1,	IDT_SW_NTP0_BARLIMIT1,
	     IDT_SW_NTP0_BARLTBASE1,	IDT_SW_NTP0_BARUTBASE1},
	    {IDT_SW_NTP0_BARSETUP2,	IDT_SW_NTP0_BARLIMIT2,
	     IDT_SW_NTP0_BARLTBASE2,	IDT_SW_NTP0_BARUTBASE2},
	    {IDT_SW_NTP0_BARSETUP3,	IDT_SW_NTP0_BARLIMIT3,
	     IDT_SW_NTP0_BARLTBASE3,	IDT_SW_NTP0_BARUTBASE3},
	    {IDT_SW_NTP0_BARSETUP4,	IDT_SW_NTP0_BARLIMIT4,
	     IDT_SW_NTP0_BARLTBASE4,	IDT_SW_NTP0_BARUTBASE4},
	    {IDT_SW_NTP0_BARSETUP5,	IDT_SW_NTP0_BARLIMIT5,
	     IDT_SW_NTP0_BARLTBASE5,	IDT_SW_NTP0_BARUTBASE5} } },
 	{0},
 	{ IDT_SW_NTP2_PCIECMDSTS,	IDT_SW_NTP2_PCIELCTLSTS,
	  IDT_SW_NTP2_NTCTL,
	  IDT_SW_SWPORT2CTL,		IDT_SW_SWPORT2STS,
	  { {IDT_SW_NTP2_BARSETUP0,	IDT_SW_NTP2_BARLIMIT0,
	     IDT_SW_NTP2_BARLTBASE0,	IDT_SW_NTP2_BARUTBASE0},
	    {IDT_SW_NTP2_BARSETUP1,	IDT_SW_NTP2_BARLIMIT1,
	     IDT_SW_NTP2_BARLTBASE1,	IDT_SW_NTP2_BARUTBASE1},
	    {IDT_SW_NTP2_BARSETUP2,	IDT_SW_NTP2_BARLIMIT2,
	     IDT_SW_NTP2_BARLTBASE2,	IDT_SW_NTP2_BARUTBASE2},
	    {IDT_SW_NTP2_BARSETUP3,	IDT_SW_NTP2_BARLIMIT3,
	     IDT_SW_NTP2_BARLTBASE3,	IDT_SW_NTP2_BARUTBASE3},
	    {IDT_SW_NTP2_BARSETUP4,	IDT_SW_NTP2_BARLIMIT4,
	     IDT_SW_NTP2_BARLTBASE4,	IDT_SW_NTP2_BARUTBASE4},
	    {IDT_SW_NTP2_BARSETUP5,	IDT_SW_NTP2_BARLIMIT5,
	     IDT_SW_NTP2_BARLTBASE5,	IDT_SW_NTP2_BARUTBASE5} } },
 	{0},
 	{ IDT_SW_NTP4_PCIECMDSTS,	IDT_SW_NTP4_PCIELCTLSTS,
	  IDT_SW_NTP4_NTCTL,
	  IDT_SW_SWPORT4CTL,		IDT_SW_SWPORT4STS,
	  { {IDT_SW_NTP4_BARSETUP0,	IDT_SW_NTP4_BARLIMIT0,
	     IDT_SW_NTP4_BARLTBASE0,	IDT_SW_NTP4_BARUTBASE0},
	    {IDT_SW_NTP4_BARSETUP1,	IDT_SW_NTP4_BARLIMIT1,
	     IDT_SW_NTP4_BARLTBASE1,	IDT_SW_NTP4_BARUTBASE1},
	    {IDT_SW_NTP4_BARSETUP2,	IDT_SW_NTP4_BARLIMIT2,
	     IDT_SW_NTP4_BARLTBASE2,	IDT_SW_NTP4_BARUTBASE2},
	    {IDT_SW_NTP4_BARSETUP3,	IDT_SW_NTP4_BARLIMIT3,
	     IDT_SW_NTP4_BARLTBASE3,	IDT_SW_NTP4_BARUTBASE3},
	    {IDT_SW_NTP4_BARSETUP4,	IDT_SW_NTP4_BARLIMIT4,
	     IDT_SW_NTP4_BARLTBASE4,	IDT_SW_NTP4_BARUTBASE4},
	    {IDT_SW_NTP4_BARSETUP5,	IDT_SW_NTP4_BARLIMIT5,
	     IDT_SW_NTP4_BARLTBASE5,	IDT_SW_NTP4_BARUTBASE5} } },
 	{0},
 	{ IDT_SW_NTP6_PCIECMDSTS,	IDT_SW_NTP6_PCIELCTLSTS,
	  IDT_SW_NTP6_NTCTL,
	  IDT_SW_SWPORT6CTL,		IDT_SW_SWPORT6STS,
	  { {IDT_SW_NTP6_BARSETUP0,	IDT_SW_NTP6_BARLIMIT0,
	     IDT_SW_NTP6_BARLTBASE0,	IDT_SW_NTP6_BARUTBASE0},
	    {IDT_SW_NTP6_BARSETUP1,	IDT_SW_NTP6_BARLIMIT1,
	     IDT_SW_NTP6_BARLTBASE1,	IDT_SW_NTP6_BARUTBASE1},
	    {IDT_SW_NTP6_BARSETUP2,	IDT_SW_NTP6_BARLIMIT2,
	     IDT_SW_NTP6_BARLTBASE2,	IDT_SW_NTP6_BARUTBASE2},
	    {IDT_SW_NTP6_BARSETUP3,	IDT_SW_NTP6_BARLIMIT3,
	     IDT_SW_NTP6_BARLTBASE3,	IDT_SW_NTP6_BARUTBASE3},
	    {IDT_SW_NTP6_BARSETUP4,	IDT_SW_NTP6_BARLIMIT4,
	     IDT_SW_NTP6_BARLTBASE4,	IDT_SW_NTP6_BARUTBASE4},
	    {IDT_SW_NTP6_BARSETUP5,	IDT_SW_NTP6_BARLIMIT5,
	     IDT_SW_NTP6_BARLTBASE5,	IDT_SW_NTP6_BARUTBASE5} } },
 	{0},
 	{ IDT_SW_NTP8_PCIECMDSTS,	IDT_SW_NTP8_PCIELCTLSTS,
	  IDT_SW_NTP8_NTCTL,
	  IDT_SW_SWPORT8CTL,		IDT_SW_SWPORT8STS,
	  { {IDT_SW_NTP8_BARSETUP0,	IDT_SW_NTP8_BARLIMIT0,
	     IDT_SW_NTP8_BARLTBASE0,	IDT_SW_NTP8_BARUTBASE0},
	    {IDT_SW_NTP8_BARSETUP1,	IDT_SW_NTP8_BARLIMIT1,
	     IDT_SW_NTP8_BARLTBASE1,	IDT_SW_NTP8_BARUTBASE1},
	    {IDT_SW_NTP8_BARSETUP2,	IDT_SW_NTP8_BARLIMIT2,
	     IDT_SW_NTP8_BARLTBASE2,	IDT_SW_NTP8_BARUTBASE2},
	    {IDT_SW_NTP8_BARSETUP3,	IDT_SW_NTP8_BARLIMIT3,
	     IDT_SW_NTP8_BARLTBASE3,	IDT_SW_NTP8_BARUTBASE3},
	    {IDT_SW_NTP8_BARSETUP4,	IDT_SW_NTP8_BARLIMIT4,
	     IDT_SW_NTP8_BARLTBASE4,	IDT_SW_NTP8_BARUTBASE4},
	    {IDT_SW_NTP8_BARSETUP5,	IDT_SW_NTP8_BARLIMIT5,
	     IDT_SW_NTP8_BARLTBASE5,	IDT_SW_NTP8_BARUTBASE5} } },
 	{0},
 	{0},
 	{0},
 	{ IDT_SW_NTP12_PCIECMDSTS,	IDT_SW_NTP12_PCIELCTLSTS,
	  IDT_SW_NTP12_NTCTL,
	  IDT_SW_SWPORT12CTL,		IDT_SW_SWPORT12STS,
	  { {IDT_SW_NTP12_BARSETUP0,	IDT_SW_NTP12_BARLIMIT0,
	     IDT_SW_NTP12_BARLTBASE0,	IDT_SW_NTP12_BARUTBASE0},
	    {IDT_SW_NTP12_BARSETUP1,	IDT_SW_NTP12_BARLIMIT1,
	     IDT_SW_NTP12_BARLTBASE1,	IDT_SW_NTP12_BARUTBASE1},
	    {IDT_SW_NTP12_BARSETUP2,	IDT_SW_NTP12_BARLIMIT2,
	     IDT_SW_NTP12_BARLTBASE2,	IDT_SW_NTP12_BARUTBASE2},
	    {IDT_SW_NTP12_BARSETUP3,	IDT_SW_NTP12_BARLIMIT3,
	     IDT_SW_NTP12_BARLTBASE3,	IDT_SW_NTP12_BARUTBASE3},
	    {IDT_SW_NTP12_BARSETUP4,	IDT_SW_NTP12_BARLIMIT4,
	     IDT_SW_NTP12_BARLTBASE4,	IDT_SW_NTP12_BARUTBASE4},
	    {IDT_SW_NTP12_BARSETUP5,	IDT_SW_NTP12_BARLIMIT5,
	     IDT_SW_NTP12_BARLTBASE5,	IDT_SW_NTP12_BARUTBASE5} } },
 	{0},
 	{0},
 	{0},
 	{ IDT_SW_NTP16_PCIECMDSTS,	IDT_SW_NTP16_PCIELCTLSTS,
	  IDT_SW_NTP16_NTCTL,
	  IDT_SW_SWPORT16CTL,		IDT_SW_SWPORT16STS,
	  { {IDT_SW_NTP16_BARSETUP0,	IDT_SW_NTP16_BARLIMIT0,
	     IDT_SW_NTP16_BARLTBASE0,	IDT_SW_NTP16_BARUTBASE0},
	    {IDT_SW_NTP16_BARSETUP1,	IDT_SW_NTP16_BARLIMIT1,
	     IDT_SW_NTP16_BARLTBASE1,	IDT_SW_NTP16_BARUTBASE1},
	    {IDT_SW_NTP16_BARSETUP2,	IDT_SW_NTP16_BARLIMIT2,
	     IDT_SW_NTP16_BARLTBASE2,	IDT_SW_NTP16_BARUTBASE2},
	    {IDT_SW_NTP16_BARSETUP3,	IDT_SW_NTP16_BARLIMIT3,
	     IDT_SW_NTP16_BARLTBASE3,	IDT_SW_NTP16_BARUTBASE3},
	    {IDT_SW_NTP16_BARSETUP4,	IDT_SW_NTP16_BARLIMIT4,
	     IDT_SW_NTP16_BARLTBASE4,	IDT_SW_NTP16_BARUTBASE4},
	    {IDT_SW_NTP16_BARSETUP5,	IDT_SW_NTP16_BARLIMIT5,
	     IDT_SW_NTP16_BARLTBASE5,	IDT_SW_NTP16_BARUTBASE5} } },
 	{0},
 	{0},
 	{0},
 	{ IDT_SW_NTP20_PCIECMDSTS,	IDT_SW_NTP20_PCIELCTLSTS,
	  IDT_SW_NTP20_NTCTL,
	  IDT_SW_SWPORT20CTL,		IDT_SW_SWPORT20STS,
	  { {IDT_SW_NTP20_BARSETUP0,	IDT_SW_NTP20_BARLIMIT0,
	     IDT_SW_NTP20_BARLTBASE0,	IDT_SW_NTP20_BARUTBASE0},
	    {IDT_SW_NTP20_BARSETUP1,	IDT_SW_NTP20_BARLIMIT1,
	     IDT_SW_NTP20_BARLTBASE1,	IDT_SW_NTP20_BARUTBASE1},
	    {IDT_SW_NTP20_BARSETUP2,	IDT_SW_NTP20_BARLIMIT2,
	     IDT_SW_NTP20_BARLTBASE2,	IDT_SW_NTP20_BARUTBASE2},
	    {IDT_SW_NTP20_BARSETUP3,	IDT_SW_NTP20_BARLIMIT3,
	     IDT_SW_NTP20_BARLTBASE3,	IDT_SW_NTP20_BARUTBASE3},
	    {IDT_SW_NTP20_BARSETUP4,	IDT_SW_NTP20_BARLIMIT4,
	     IDT_SW_NTP20_BARLTBASE4,	IDT_SW_NTP20_BARUTBASE4},
	    {IDT_SW_NTP20_BARSETUP5,	IDT_SW_NTP20_BARLIMIT5,
	     IDT_SW_NTP20_BARLTBASE5,	IDT_SW_NTP20_BARUTBASE5} } },
 	{0},
 	{0},
 	{0}
};

 
static const struct idt_ntb_part partdata_tbl[IDT_MAX_NR_PARTS] = {
 	{ IDT_SW_SWPART0CTL,	IDT_SW_SWPART0STS,
	  {IDT_SW_SWP0MSGCTL0,	IDT_SW_SWP0MSGCTL1,
	   IDT_SW_SWP0MSGCTL2,	IDT_SW_SWP0MSGCTL3} },
 	{ IDT_SW_SWPART1CTL,	IDT_SW_SWPART1STS,
	  {IDT_SW_SWP1MSGCTL0,	IDT_SW_SWP1MSGCTL1,
	   IDT_SW_SWP1MSGCTL2,	IDT_SW_SWP1MSGCTL3} },
 	{ IDT_SW_SWPART2CTL,	IDT_SW_SWPART2STS,
	  {IDT_SW_SWP2MSGCTL0,	IDT_SW_SWP2MSGCTL1,
	   IDT_SW_SWP2MSGCTL2,	IDT_SW_SWP2MSGCTL3} },
 	{ IDT_SW_SWPART3CTL,	IDT_SW_SWPART3STS,
	  {IDT_SW_SWP3MSGCTL0,	IDT_SW_SWP3MSGCTL1,
	   IDT_SW_SWP3MSGCTL2,	IDT_SW_SWP3MSGCTL3} },
 	{ IDT_SW_SWPART4CTL,	IDT_SW_SWPART4STS,
	  {IDT_SW_SWP4MSGCTL0,	IDT_SW_SWP4MSGCTL1,
	   IDT_SW_SWP4MSGCTL2,	IDT_SW_SWP4MSGCTL3} },
 	{ IDT_SW_SWPART5CTL,	IDT_SW_SWPART5STS,
	  {IDT_SW_SWP5MSGCTL0,	IDT_SW_SWP5MSGCTL1,
	   IDT_SW_SWP5MSGCTL2,	IDT_SW_SWP5MSGCTL3} },
 	{ IDT_SW_SWPART6CTL,	IDT_SW_SWPART6STS,
	  {IDT_SW_SWP6MSGCTL0,	IDT_SW_SWP6MSGCTL1,
	   IDT_SW_SWP6MSGCTL2,	IDT_SW_SWP6MSGCTL3} },
 	{ IDT_SW_SWPART7CTL,	IDT_SW_SWPART7STS,
	  {IDT_SW_SWP7MSGCTL0,	IDT_SW_SWP7MSGCTL1,
	   IDT_SW_SWP7MSGCTL2,	IDT_SW_SWP7MSGCTL3} }
};

 
static struct dentry *dbgfs_topdir;

 

 
static void idt_nt_write(struct idt_ntb_dev *ndev,
			 const unsigned int reg, const u32 data)
{
	 
	if (WARN_ON(reg > IDT_REG_PCI_MAX || !IS_ALIGNED(reg, IDT_REG_ALIGN)))
		return;

	 
	iowrite32(data, ndev->cfgspc + (ptrdiff_t)reg);
}

 
static u32 idt_nt_read(struct idt_ntb_dev *ndev, const unsigned int reg)
{
	 
	if (WARN_ON(reg > IDT_REG_PCI_MAX || !IS_ALIGNED(reg, IDT_REG_ALIGN)))
		return ~0;

	 
	return ioread32(ndev->cfgspc + (ptrdiff_t)reg);
}

 
static void idt_sw_write(struct idt_ntb_dev *ndev,
			 const unsigned int reg, const u32 data)
{
	unsigned long irqflags;

	 
	if (WARN_ON(reg > IDT_REG_SW_MAX || !IS_ALIGNED(reg, IDT_REG_ALIGN)))
		return;

	 
	spin_lock_irqsave(&ndev->gasa_lock, irqflags);
	 
	iowrite32((u32)reg, ndev->cfgspc + (ptrdiff_t)IDT_NT_GASAADDR);
	 
	iowrite32(data, ndev->cfgspc + (ptrdiff_t)IDT_NT_GASADATA);
	 
	spin_unlock_irqrestore(&ndev->gasa_lock, irqflags);
}

 
static u32 idt_sw_read(struct idt_ntb_dev *ndev, const unsigned int reg)
{
	unsigned long irqflags;
	u32 data;

	 
	if (WARN_ON(reg > IDT_REG_SW_MAX || !IS_ALIGNED(reg, IDT_REG_ALIGN)))
		return ~0;

	 
	spin_lock_irqsave(&ndev->gasa_lock, irqflags);
	 
	iowrite32((u32)reg, ndev->cfgspc + (ptrdiff_t)IDT_NT_GASAADDR);
	 
	data = ioread32(ndev->cfgspc + (ptrdiff_t)IDT_NT_GASADATA);
	 
	spin_unlock_irqrestore(&ndev->gasa_lock, irqflags);

	return data;
}

 
static inline int idt_reg_set_bits(struct idt_ntb_dev *ndev, unsigned int reg,
				   spinlock_t *reg_lock,
				   u64 valid_mask, u64 set_bits)
{
	unsigned long irqflags;
	u32 data;

	if (set_bits & ~(u64)valid_mask)
		return -EINVAL;

	 
	spin_lock_irqsave(reg_lock, irqflags);
	data = idt_nt_read(ndev, reg) | (u32)set_bits;
	idt_nt_write(ndev, reg, data);
	 
	spin_unlock_irqrestore(reg_lock, irqflags);

	return 0;
}

 
static inline void idt_reg_clear_bits(struct idt_ntb_dev *ndev,
				     unsigned int reg, spinlock_t *reg_lock,
				     u64 clear_bits)
{
	unsigned long irqflags;
	u32 data;

	 
	spin_lock_irqsave(reg_lock, irqflags);
	data = idt_nt_read(ndev, reg) & ~(u32)clear_bits;
	idt_nt_write(ndev, reg, data);
	 
	spin_unlock_irqrestore(reg_lock, irqflags);
}

 

 
static int idt_scan_ports(struct idt_ntb_dev *ndev)
{
	unsigned char pidx, port, part;
	u32 data, portsts, partsts;

	 
	data = idt_nt_read(ndev, IDT_NT_PCIELCAP);
	ndev->port = GET_FIELD(PCIELCAP_PORTNUM, data);

	 
	portsts = idt_sw_read(ndev, portdata_tbl[ndev->port].sts);
	ndev->part = GET_FIELD(SWPORTxSTS_SWPART, portsts);

	 
	memset(ndev->port_idx_map, -EINVAL, sizeof(ndev->port_idx_map));
	memset(ndev->part_idx_map, -EINVAL, sizeof(ndev->part_idx_map));

	 
	ndev->peer_cnt = 0;
	for (pidx = 0; pidx < ndev->swcfg->port_cnt; pidx++) {
		port = ndev->swcfg->ports[pidx];
		 
		if (port == ndev->port)
			continue;

		 
		portsts = idt_sw_read(ndev, portdata_tbl[port].sts);
		part = GET_FIELD(SWPORTxSTS_SWPART, portsts);

		 
		partsts = idt_sw_read(ndev, partdata_tbl[part].sts);
		 
		if (IS_FLD_SET(SWPARTxSTS_STATE, partsts, ACT) &&
		    (IS_FLD_SET(SWPORTxSTS_MODE, portsts, NT) ||
		     IS_FLD_SET(SWPORTxSTS_MODE, portsts, USNT) ||
		     IS_FLD_SET(SWPORTxSTS_MODE, portsts, USNTDMA) ||
		     IS_FLD_SET(SWPORTxSTS_MODE, portsts, NTDMA))) {
			 
			ndev->peers[ndev->peer_cnt].port = port;
			ndev->peers[ndev->peer_cnt].part = part;
			 
			ndev->port_idx_map[port] = ndev->peer_cnt;
			ndev->part_idx_map[part] = ndev->peer_cnt;
			ndev->peer_cnt++;
		}
	}

	dev_dbg(&ndev->ntb.pdev->dev, "Local port: %hhu, num of peers: %hhu\n",
		ndev->port, ndev->peer_cnt);

	 
	if (ndev->peer_cnt == 0) {
		dev_warn(&ndev->ntb.pdev->dev, "No active peer found\n");
		return -ENODEV;
	}

	return 0;
}

 
static int idt_ntb_port_number(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return ndev->port;
}

 
static int idt_ntb_peer_port_count(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return ndev->peer_cnt;
}

 
static int idt_ntb_peer_port_number(struct ntb_dev *ntb, int pidx)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	 
	return ndev->peers[pidx].port;
}

 
static int idt_ntb_peer_port_idx(struct ntb_dev *ntb, int port)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (port < 0 || IDT_MAX_NR_PORTS <= port)
		return -EINVAL;

	return ndev->port_idx_map[port];
}

 

static void idt_ntb_local_link_disable(struct idt_ntb_dev *ndev);

 
static void idt_init_link(struct idt_ntb_dev *ndev)
{
	u32 part_mask, port_mask, se_mask;
	unsigned char pidx;

	 
	spin_lock_init(&ndev->mtbl_lock);

	 
	port_mask = ~BIT(ndev->port);
	part_mask = ~BIT(ndev->part);
	for (pidx = 0; pidx < ndev->peer_cnt; pidx++) {
		port_mask &= ~BIT(ndev->peers[pidx].port);
		part_mask &= ~BIT(ndev->peers[pidx].part);
	}

	 
	idt_sw_write(ndev, IDT_SW_SELINKUPSTS, (u32)-1);
	idt_sw_write(ndev, IDT_SW_SELINKDNSTS, (u32)-1);
	idt_sw_write(ndev, IDT_SW_SEGSIGSTS, (u32)-1);

	 
	idt_sw_write(ndev, IDT_SW_SEPMSK, part_mask);

	 
	idt_sw_write(ndev, IDT_SW_SELINKUPMSK, port_mask);

	 
	idt_sw_write(ndev, IDT_SW_SELINKDNMSK, port_mask);

	 
	idt_sw_write(ndev, IDT_SW_SEGSIGMSK, part_mask);

	 
	se_mask = ~(IDT_SEMSK_LINKUP | IDT_SEMSK_LINKDN | IDT_SEMSK_GSIGNAL);
	idt_sw_write(ndev, IDT_SW_SEMSK, se_mask);

	dev_dbg(&ndev->ntb.pdev->dev, "NTB link status events initialized");
}

 
static void idt_deinit_link(struct idt_ntb_dev *ndev)
{
	 
	idt_ntb_local_link_disable(ndev);

	dev_dbg(&ndev->ntb.pdev->dev, "NTB link status events deinitialized");
}

 
static void idt_se_isr(struct idt_ntb_dev *ndev, u32 ntint_sts)
{
	u32 sests;

	 
	sests = idt_sw_read(ndev, IDT_SW_SESTS);

	 
	idt_sw_write(ndev, IDT_SW_SELINKUPSTS, (u32)-1);
	idt_sw_write(ndev, IDT_SW_SELINKDNSTS, (u32)-1);
	idt_sw_write(ndev, IDT_SW_SEGSIGSTS, (u32)-1);

	 
	idt_nt_write(ndev, IDT_NT_NTINTSTS, IDT_NTINTSTS_SEVENT);

	dev_dbg(&ndev->ntb.pdev->dev, "SE IRQ detected %#08x (SESTS %#08x)",
			  ntint_sts, sests);

	 
	ntb_link_event(&ndev->ntb);
}

 
static void idt_ntb_local_link_enable(struct idt_ntb_dev *ndev)
{
	u32 reqid, mtbldata = 0;
	unsigned long irqflags;

	 
	idt_nt_write(ndev, IDT_NT_NTCTL, IDT_NTCTL_CPEN);

	 
	reqid = idt_nt_read(ndev, IDT_NT_REQIDCAP);

	 
	mtbldata = SET_FIELD(NTMTBLDATA_REQID, 0, reqid) |
		   SET_FIELD(NTMTBLDATA_PART, 0, ndev->part) |
		   IDT_NTMTBLDATA_VALID;
	spin_lock_irqsave(&ndev->mtbl_lock, irqflags);
	idt_nt_write(ndev, IDT_NT_NTMTBLADDR, ndev->part);
	idt_nt_write(ndev, IDT_NT_NTMTBLDATA, mtbldata);
	spin_unlock_irqrestore(&ndev->mtbl_lock, irqflags);

	 
	idt_nt_write(ndev, IDT_NT_NTGSIGNAL, IDT_NTGSIGNAL_SET);
	idt_sw_write(ndev, IDT_SW_SEGSIGSTS, (u32)1 << ndev->part);
}

 
static void idt_ntb_local_link_disable(struct idt_ntb_dev *ndev)
{
	unsigned long irqflags;

	 
	idt_nt_write(ndev, IDT_NT_NTCTL, 0);

	 
	spin_lock_irqsave(&ndev->mtbl_lock, irqflags);
	idt_nt_write(ndev, IDT_NT_NTMTBLADDR, ndev->part);
	idt_nt_write(ndev, IDT_NT_NTMTBLDATA, 0);
	spin_unlock_irqrestore(&ndev->mtbl_lock, irqflags);

	 
	idt_nt_write(ndev, IDT_NT_NTGSIGNAL, IDT_NTGSIGNAL_SET);
	idt_sw_write(ndev, IDT_SW_SEGSIGSTS, (u32)1 << ndev->part);
}

 
static bool idt_ntb_local_link_is_up(struct idt_ntb_dev *ndev)
{
	unsigned long irqflags;
	u32 data;

	 
	data = idt_nt_read(ndev, IDT_NT_PCICMDSTS);
	if (!(data & IDT_PCICMDSTS_BME))
		return false;

	 
	data = idt_nt_read(ndev, IDT_NT_NTCTL);
	if (!(data & IDT_NTCTL_CPEN))
		return false;

	 
	spin_lock_irqsave(&ndev->mtbl_lock, irqflags);
	idt_nt_write(ndev, IDT_NT_NTMTBLADDR, ndev->part);
	data = idt_nt_read(ndev, IDT_NT_NTMTBLDATA);
	spin_unlock_irqrestore(&ndev->mtbl_lock, irqflags);

	return !!(data & IDT_NTMTBLDATA_VALID);
}

 
static bool idt_ntb_peer_link_is_up(struct idt_ntb_dev *ndev, int pidx)
{
	unsigned long irqflags;
	unsigned char port;
	u32 data;

	 
	port = ndev->peers[pidx].port;

	 
	data = idt_sw_read(ndev, portdata_tbl[port].sts);
	if (!(data & IDT_SWPORTxSTS_LINKUP))
		return false;

	 
	data = idt_sw_read(ndev, portdata_tbl[port].pcicmdsts);
	if (!(data & IDT_PCICMDSTS_BME))
		return false;

	 
	data = idt_sw_read(ndev, portdata_tbl[port].ntctl);
	if (!(data & IDT_NTCTL_CPEN))
		return false;

	 
	spin_lock_irqsave(&ndev->mtbl_lock, irqflags);
	idt_nt_write(ndev, IDT_NT_NTMTBLADDR, ndev->peers[pidx].part);
	data = idt_nt_read(ndev, IDT_NT_NTMTBLDATA);
	spin_unlock_irqrestore(&ndev->mtbl_lock, irqflags);

	return !!(data & IDT_NTMTBLDATA_VALID);
}

 
static u64 idt_ntb_link_is_up(struct ntb_dev *ntb,
			      enum ntb_speed *speed, enum ntb_width *width)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);
	unsigned char pidx;
	u64 status;
	u32 data;

	 
	if (speed != NULL || width != NULL) {
		data = idt_nt_read(ndev, IDT_NT_PCIELCTLSTS);
		if (speed != NULL)
			*speed = GET_FIELD(PCIELCTLSTS_CLS, data);
		if (width != NULL)
			*width = GET_FIELD(PCIELCTLSTS_NLW, data);
	}

	 
	if (!idt_ntb_local_link_is_up(ndev))
		return 0;

	 
	status = 0;
	for (pidx = 0; pidx < ndev->peer_cnt; pidx++) {
		if (idt_ntb_peer_link_is_up(ndev, pidx))
			status |= ((u64)1 << pidx);
	}

	return status;
}

 
static int idt_ntb_link_enable(struct ntb_dev *ntb, enum ntb_speed speed,
			       enum ntb_width width)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	 
	idt_ntb_local_link_enable(ndev);

	dev_dbg(&ndev->ntb.pdev->dev, "Local NTB link enabled");

	return 0;
}

 
static int idt_ntb_link_disable(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	 
	idt_ntb_local_link_disable(ndev);

	dev_dbg(&ndev->ntb.pdev->dev, "Local NTB link disabled");

	return 0;
}

 

 
static inline unsigned char idt_get_mw_count(enum idt_mw_type mw_type)
{
	switch (mw_type) {
	case IDT_MW_DIR:
		return 1;
	case IDT_MW_LUT12:
		return 12;
	case IDT_MW_LUT24:
		return 24;
	default:
		break;
	}

	return 0;
}

 
static inline char *idt_get_mw_name(enum idt_mw_type mw_type)
{
	switch (mw_type) {
	case IDT_MW_DIR:
		return "DIR  ";
	case IDT_MW_LUT12:
		return "LUT12";
	case IDT_MW_LUT24:
		return "LUT24";
	default:
		break;
	}

	return "unknown";
}

 
static struct idt_mw_cfg *idt_scan_mws(struct idt_ntb_dev *ndev, int port,
				       unsigned char *mw_cnt)
{
	struct idt_mw_cfg mws[IDT_MAX_NR_MWS], *ret_mws;
	const struct idt_ntb_bar *bars;
	enum idt_mw_type mw_type;
	unsigned char widx, bidx, en_cnt;
	bool bar_64bit = false;
	int aprt_size;
	u32 data;

	 
	bars = portdata_tbl[port].bars;

	 
	*mw_cnt = 0;
	for (bidx = 0; bidx < IDT_BAR_CNT; bidx += 1 + bar_64bit) {
		 
		data = idt_sw_read(ndev, bars[bidx].setup);

		 
		if (!(data & IDT_BARSETUP_EN)) {
			bar_64bit = false;
			continue;
		}

		 
		bar_64bit = IS_FLD_SET(BARSETUP_TYPE, data, 64);

		 
		if (data & IDT_BARSETUP_MODE_CFG)
			continue;

		 
		mw_type = GET_FIELD(BARSETUP_ATRAN, data);
		en_cnt = idt_get_mw_count(mw_type);
		aprt_size = (u64)1 << GET_FIELD(BARSETUP_SIZE, data);

		 
		for (widx = 0; widx < en_cnt; widx++, (*mw_cnt)++) {
			 
			if (*mw_cnt >= IDT_MAX_NR_MWS)
				return ERR_PTR(-EINVAL);

			 
			mws[*mw_cnt].type = mw_type;
			mws[*mw_cnt].bar = bidx;
			mws[*mw_cnt].idx = widx;
			 
			mws[*mw_cnt].addr_align = IDT_TRANS_ALIGN;
			 
			if (mw_type == IDT_MW_DIR)
				mws[*mw_cnt].size_max = aprt_size;
			else if (mw_type == IDT_MW_LUT12)
				mws[*mw_cnt].size_max = aprt_size / 16;
			else
				mws[*mw_cnt].size_max = aprt_size / 32;
			mws[*mw_cnt].size_align = (mw_type == IDT_MW_DIR) ?
				IDT_DIR_SIZE_ALIGN : mws[*mw_cnt].size_max;
		}
	}

	 
	ret_mws = devm_kcalloc(&ndev->ntb.pdev->dev, *mw_cnt, sizeof(*ret_mws),
			       GFP_KERNEL);
	if (!ret_mws)
		return ERR_PTR(-ENOMEM);

	 
	memcpy(ret_mws, mws, (*mw_cnt)*sizeof(*ret_mws));

	return ret_mws;
}

 
static int idt_init_mws(struct idt_ntb_dev *ndev)
{
	struct idt_ntb_peer *peer;
	unsigned char pidx;

	 
	ndev->mws = idt_scan_mws(ndev, ndev->port, &ndev->mw_cnt);
	if (IS_ERR(ndev->mws)) {
		dev_err(&ndev->ntb.pdev->dev,
			"Failed to scan mws of local port %hhu", ndev->port);
		return PTR_ERR(ndev->mws);
	}

	 
	for (pidx = 0; pidx < ndev->peer_cnt; pidx++) {
		peer = &ndev->peers[pidx];
		peer->mws = idt_scan_mws(ndev, peer->port, &peer->mw_cnt);
		if (IS_ERR(peer->mws)) {
			dev_err(&ndev->ntb.pdev->dev,
				"Failed to scan mws of port %hhu", peer->port);
			return PTR_ERR(peer->mws);
		}
	}

	 
	spin_lock_init(&ndev->lut_lock);

	dev_dbg(&ndev->ntb.pdev->dev, "Outbound and inbound MWs initialized");

	return 0;
}

 
static int idt_ntb_mw_count(struct ntb_dev *ntb, int pidx)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	return ndev->peers[pidx].mw_cnt;
}

 
static int idt_ntb_mw_get_align(struct ntb_dev *ntb, int pidx, int widx,
				resource_size_t *addr_align,
				resource_size_t *size_align,
				resource_size_t *size_max)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);
	struct idt_ntb_peer *peer;

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	peer = &ndev->peers[pidx];

	if (widx < 0 || peer->mw_cnt <= widx)
		return -EINVAL;

	if (addr_align != NULL)
		*addr_align = peer->mws[widx].addr_align;

	if (size_align != NULL)
		*size_align = peer->mws[widx].size_align;

	if (size_max != NULL)
		*size_max = peer->mws[widx].size_max;

	return 0;
}

 
static int idt_ntb_peer_mw_count(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return ndev->mw_cnt;
}

 
static int idt_ntb_peer_mw_get_addr(struct ntb_dev *ntb, int widx,
				    phys_addr_t *base, resource_size_t *size)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (widx < 0 || ndev->mw_cnt <= widx)
		return -EINVAL;

	 
	if (base != NULL)
		*base = pci_resource_start(ntb->pdev, ndev->mws[widx].bar) +
			ndev->mws[widx].idx * ndev->mws[widx].size_max;

	 
	if (size != NULL)
		*size = ndev->mws[widx].size_max;

	return 0;
}

 
static int idt_ntb_peer_mw_set_trans(struct ntb_dev *ntb, int pidx, int widx,
				     u64 addr, resource_size_t size)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);
	struct idt_mw_cfg *mw_cfg;
	u32 data = 0, lutoff = 0;

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	if (widx < 0 || ndev->mw_cnt <= widx)
		return -EINVAL;

	 
	mw_cfg = &ndev->mws[widx];
	if (!IS_ALIGNED(addr, mw_cfg->addr_align))
		return -EINVAL;
	if (!IS_ALIGNED(size, mw_cfg->size_align) || size > mw_cfg->size_max)
		return -EINVAL;

	 
	if (mw_cfg->type == IDT_MW_DIR) {
		const struct idt_ntb_bar *bar = &ntdata_tbl.bars[mw_cfg->bar];
		u64 limit;
		 
		data = idt_nt_read(ndev, bar->setup);
		data = SET_FIELD(BARSETUP_TPART, data, ndev->peers[pidx].part);
		idt_nt_write(ndev, bar->setup, data);
		 
		idt_nt_write(ndev, bar->ltbase, (u32)addr);
		idt_nt_write(ndev, bar->utbase, (u32)(addr >> 32));
		 
		limit = pci_bus_address(ntb->pdev, mw_cfg->bar) + size;
		idt_nt_write(ndev, bar->limit, (u32)limit);
		if (IS_FLD_SET(BARSETUP_TYPE, data, 64))
			idt_nt_write(ndev, (bar + 1)->limit, (limit >> 32));
	} else {
		unsigned long irqflags;
		 
		lutoff = SET_FIELD(LUTOFFSET_INDEX, 0, mw_cfg->idx) |
			 SET_FIELD(LUTOFFSET_BAR, 0, mw_cfg->bar);
		data = SET_FIELD(LUTUDATA_PART, 0, ndev->peers[pidx].part) |
			IDT_LUTUDATA_VALID;
		spin_lock_irqsave(&ndev->lut_lock, irqflags);
		idt_nt_write(ndev, IDT_NT_LUTOFFSET, lutoff);
		idt_nt_write(ndev, IDT_NT_LUTLDATA, (u32)addr);
		idt_nt_write(ndev, IDT_NT_LUTMDATA, (u32)(addr >> 32));
		idt_nt_write(ndev, IDT_NT_LUTUDATA, data);
		spin_unlock_irqrestore(&ndev->lut_lock, irqflags);
		 
	}

	return 0;
}

 
static int idt_ntb_peer_mw_clear_trans(struct ntb_dev *ntb, int pidx,
					int widx)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);
	struct idt_mw_cfg *mw_cfg;

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	if (widx < 0 || ndev->mw_cnt <= widx)
		return -EINVAL;

	mw_cfg = &ndev->mws[widx];

	 
	if (mw_cfg->type == IDT_MW_DIR) {
		const struct idt_ntb_bar *bar = &ntdata_tbl.bars[mw_cfg->bar];
		u32 data;
		 
		data = idt_nt_read(ndev, bar->setup);
		 
		idt_nt_write(ndev, bar->limit, 0);
		if (IS_FLD_SET(BARSETUP_TYPE, data, 64))
			idt_nt_write(ndev, (bar + 1)->limit, 0);
	} else {
		unsigned long irqflags;
		u32 lutoff;
		 
		lutoff = SET_FIELD(LUTOFFSET_INDEX, 0, mw_cfg->idx) |
			 SET_FIELD(LUTOFFSET_BAR, 0, mw_cfg->bar);
		spin_lock_irqsave(&ndev->lut_lock, irqflags);
		idt_nt_write(ndev, IDT_NT_LUTOFFSET, lutoff);
		idt_nt_write(ndev, IDT_NT_LUTLDATA, 0);
		idt_nt_write(ndev, IDT_NT_LUTMDATA, 0);
		idt_nt_write(ndev, IDT_NT_LUTUDATA, 0);
		spin_unlock_irqrestore(&ndev->lut_lock, irqflags);
	}

	return 0;
}

 

 
static void idt_db_isr(struct idt_ntb_dev *ndev, u32 ntint_sts)
{
	 
	dev_dbg(&ndev->ntb.pdev->dev, "DB IRQ detected %#08x", ntint_sts);

	 
	ntb_db_event(&ndev->ntb, 0);
}

 
static u64 idt_ntb_db_valid_mask(struct ntb_dev *ntb)
{
	return IDT_DBELL_MASK;
}

 
static u64 idt_ntb_db_read(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return idt_nt_read(ndev, IDT_NT_INDBELLSTS);
}

 
static int idt_ntb_db_clear(struct ntb_dev *ntb, u64 db_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	idt_nt_write(ndev, IDT_NT_INDBELLSTS, (u32)db_bits);

	return 0;
}

 
static u64 idt_ntb_db_read_mask(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return idt_nt_read(ndev, IDT_NT_INDBELLMSK);
}

 
static int idt_ntb_db_set_mask(struct ntb_dev *ntb, u64 db_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return idt_reg_set_bits(ndev, IDT_NT_INDBELLMSK, &ndev->db_mask_lock,
				IDT_DBELL_MASK, db_bits);
}

 
static int idt_ntb_db_clear_mask(struct ntb_dev *ntb, u64 db_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	idt_reg_clear_bits(ndev, IDT_NT_INDBELLMSK, &ndev->db_mask_lock,
			   db_bits);

	return 0;
}

 
static int idt_ntb_peer_db_set(struct ntb_dev *ntb, u64 db_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (db_bits & ~(u64)IDT_DBELL_MASK)
		return -EINVAL;

	idt_nt_write(ndev, IDT_NT_OUTDBELLSET, (u32)db_bits);
	return 0;
}

 

 
static void idt_init_msg(struct idt_ntb_dev *ndev)
{
	unsigned char midx;

	 
	for (midx = 0; midx < IDT_MSG_CNT; midx++)
		spin_lock_init(&ndev->msg_locks[midx]);

	dev_dbg(&ndev->ntb.pdev->dev, "NTB Messaging initialized");
}

 
static void idt_msg_isr(struct idt_ntb_dev *ndev, u32 ntint_sts)
{
	 
	dev_dbg(&ndev->ntb.pdev->dev, "Message IRQ detected %#08x", ntint_sts);

	 
	ntb_msg_event(&ndev->ntb);
}

 
static int idt_ntb_msg_count(struct ntb_dev *ntb)
{
	return IDT_MSG_CNT;
}

 
static u64 idt_ntb_msg_inbits(struct ntb_dev *ntb)
{
	return (u64)IDT_INMSG_MASK;
}

 
static u64 idt_ntb_msg_outbits(struct ntb_dev *ntb)
{
	return (u64)IDT_OUTMSG_MASK;
}

 
static u64 idt_ntb_msg_read_sts(struct ntb_dev *ntb)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return idt_nt_read(ndev, IDT_NT_MSGSTS);
}

 
static int idt_ntb_msg_clear_sts(struct ntb_dev *ntb, u64 sts_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	idt_nt_write(ndev, IDT_NT_MSGSTS, sts_bits);

	return 0;
}

 
static int idt_ntb_msg_set_mask(struct ntb_dev *ntb, u64 mask_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	return idt_reg_set_bits(ndev, IDT_NT_MSGSTSMSK, &ndev->msg_mask_lock,
				IDT_MSG_MASK, mask_bits);
}

 
static int idt_ntb_msg_clear_mask(struct ntb_dev *ntb, u64 mask_bits)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	idt_reg_clear_bits(ndev, IDT_NT_MSGSTSMSK, &ndev->msg_mask_lock,
			   mask_bits);

	return 0;
}

 
static u32 idt_ntb_msg_read(struct ntb_dev *ntb, int *pidx, int midx)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);

	if (midx < 0 || IDT_MSG_CNT <= midx)
		return ~(u32)0;

	 
	if (pidx != NULL) {
		u32 srcpart;

		srcpart = idt_nt_read(ndev, ntdata_tbl.msgs[midx].src);
		*pidx = ndev->part_idx_map[srcpart];

		 
		if (*pidx == -EINVAL)
			*pidx = 0;
	}

	 
	return idt_nt_read(ndev, ntdata_tbl.msgs[midx].in);
}

 
static int idt_ntb_peer_msg_write(struct ntb_dev *ntb, int pidx, int midx,
				  u32 msg)
{
	struct idt_ntb_dev *ndev = to_ndev_ntb(ntb);
	unsigned long irqflags;
	u32 swpmsgctl = 0;

	if (midx < 0 || IDT_MSG_CNT <= midx)
		return -EINVAL;

	if (pidx < 0 || ndev->peer_cnt <= pidx)
		return -EINVAL;

	 
	swpmsgctl = SET_FIELD(SWPxMSGCTL_REG, 0, midx) |
		    SET_FIELD(SWPxMSGCTL_PART, 0, ndev->peers[pidx].part);

	 
	spin_lock_irqsave(&ndev->msg_locks[midx], irqflags);
	 
	idt_sw_write(ndev, partdata_tbl[ndev->part].msgctl[midx], swpmsgctl);
	idt_nt_write(ndev, ntdata_tbl.msgs[midx].out, msg);
	 
	spin_unlock_irqrestore(&ndev->msg_locks[midx], irqflags);

	 
	return 0;
}

 

 
static inline s8 idt_get_deg(long mdegC)
{
	return mdegC / 1000;
}

 
static inline u8 idt_get_deg_frac(long mdegC)
{
	return (mdegC % 1000) >= 500 ? 5 : 0;
}

 
static inline u8 idt_temp_get_fmt(long mdegC)
{
	return (idt_get_deg(mdegC) << 1) | (idt_get_deg_frac(mdegC) ? 1 : 0);
}

 
static inline long idt_get_temp_sval(u32 data)
{
	return ((s8)data / 2) * 1000 + (data & 0x1 ? 500 : 0);
}

 
static inline long idt_get_temp_uval(u32 data)
{
	return (data / 2) * 1000 + (data & 0x1 ? 500 : 0);
}

 
static void idt_read_temp(struct idt_ntb_dev *ndev,
			  const enum idt_temp_val type, long *val)
{
	u32 data;

	 
	switch (type) {
	case IDT_TEMP_CUR:
		data = GET_FIELD(TMPSTS_TEMP,
				 idt_sw_read(ndev, IDT_SW_TMPSTS));
		break;
	case IDT_TEMP_LOW:
		data = GET_FIELD(TMPSTS_LTEMP,
				 idt_sw_read(ndev, IDT_SW_TMPSTS));
		break;
	case IDT_TEMP_HIGH:
		data = GET_FIELD(TMPSTS_HTEMP,
				 idt_sw_read(ndev, IDT_SW_TMPSTS));
		break;
	case IDT_TEMP_OFFSET:
		 
		data = GET_FIELD(TMPADJ_OFFSET,
				 idt_sw_read(ndev, IDT_SW_TMPADJ));
		*val = idt_get_temp_sval(data);
		return;
	default:
		data = GET_FIELD(TMPSTS_TEMP,
				 idt_sw_read(ndev, IDT_SW_TMPSTS));
		break;
	}

	 
	*val = idt_get_temp_uval(data);
}

 
static void idt_write_temp(struct idt_ntb_dev *ndev,
			   const enum idt_temp_val type, const long val)
{
	unsigned int reg;
	u32 data;
	u8 fmt;

	 
	fmt = idt_temp_get_fmt(val);

	mutex_lock(&ndev->hwmon_mtx);
	switch (type) {
	case IDT_TEMP_LOW:
		reg = IDT_SW_TMPALARM;
		data = SET_FIELD(TMPALARM_LTEMP, idt_sw_read(ndev, reg), fmt) &
			~IDT_TMPALARM_IRQ_MASK;
		break;
	case IDT_TEMP_HIGH:
		reg = IDT_SW_TMPALARM;
		data = SET_FIELD(TMPALARM_HTEMP, idt_sw_read(ndev, reg), fmt) &
			~IDT_TMPALARM_IRQ_MASK;
		break;
	case IDT_TEMP_OFFSET:
		reg = IDT_SW_TMPADJ;
		data = SET_FIELD(TMPADJ_OFFSET, idt_sw_read(ndev, reg), fmt);
		break;
	default:
		goto inval_spin_unlock;
	}

	idt_sw_write(ndev, reg, data);

inval_spin_unlock:
	mutex_unlock(&ndev->hwmon_mtx);
}

 
static ssize_t idt_sysfs_show_temp(struct device *dev,
				   struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct idt_ntb_dev *ndev = dev_get_drvdata(dev);
	enum idt_temp_val type = attr->index;
	long mdeg;

	idt_read_temp(ndev, type, &mdeg);
	return sprintf(buf, "%ld\n", mdeg);
}

 
static ssize_t idt_sysfs_set_temp(struct device *dev,
				  struct device_attribute *da, const char *buf,
				  size_t count)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct idt_ntb_dev *ndev = dev_get_drvdata(dev);
	enum idt_temp_val type = attr->index;
	long mdeg;
	int ret;

	ret = kstrtol(buf, 10, &mdeg);
	if (ret)
		return ret;

	 
	if (type == IDT_TEMP_OFFSET)
		mdeg = clamp_val(mdeg, IDT_TEMP_MIN_OFFSET,
				 IDT_TEMP_MAX_OFFSET);
	else
		mdeg = clamp_val(mdeg, IDT_TEMP_MIN_MDEG, IDT_TEMP_MAX_MDEG);

	idt_write_temp(ndev, type, mdeg);

	return count;
}

 
static ssize_t idt_sysfs_reset_hist(struct device *dev,
				    struct device_attribute *da,
				    const char *buf, size_t count)
{
	struct idt_ntb_dev *ndev = dev_get_drvdata(dev);

	 
	idt_write_temp(ndev, IDT_TEMP_LOW, IDT_TEMP_MAX_MDEG);
	idt_write_temp(ndev, IDT_TEMP_HIGH, IDT_TEMP_MIN_MDEG);

	return count;
}

 
static SENSOR_DEVICE_ATTR(temp1_input, 0444, idt_sysfs_show_temp, NULL,
			  IDT_TEMP_CUR);
static SENSOR_DEVICE_ATTR(temp1_lowest, 0444, idt_sysfs_show_temp, NULL,
			  IDT_TEMP_LOW);
static SENSOR_DEVICE_ATTR(temp1_highest, 0444, idt_sysfs_show_temp, NULL,
			  IDT_TEMP_HIGH);
static SENSOR_DEVICE_ATTR(temp1_offset, 0644, idt_sysfs_show_temp,
			  idt_sysfs_set_temp, IDT_TEMP_OFFSET);
static DEVICE_ATTR(temp1_reset_history, 0200, NULL, idt_sysfs_reset_hist);

 
static struct attribute *idt_temp_attrs[] = {
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp1_lowest.dev_attr.attr,
	&sensor_dev_attr_temp1_highest.dev_attr.attr,
	&sensor_dev_attr_temp1_offset.dev_attr.attr,
	&dev_attr_temp1_reset_history.attr,
	NULL
};
ATTRIBUTE_GROUPS(idt_temp);

 
static void idt_init_temp(struct idt_ntb_dev *ndev)
{
	struct device *hwmon;

	 
	idt_sw_write(ndev, IDT_SW_TMPCTL, 0x0);

	 
	mutex_init(&ndev->hwmon_mtx);

	hwmon = devm_hwmon_device_register_with_groups(&ndev->ntb.pdev->dev,
		ndev->swcfg->name, ndev, idt_temp_groups);
	if (IS_ERR(hwmon)) {
		dev_err(&ndev->ntb.pdev->dev, "Couldn't create hwmon device");
		return;
	}

	dev_dbg(&ndev->ntb.pdev->dev, "Temperature HWmon interface registered");
}

 

static irqreturn_t idt_thread_isr(int irq, void *devid);

 
static int idt_init_isr(struct idt_ntb_dev *ndev)
{
	struct pci_dev *pdev = ndev->ntb.pdev;
	u32 ntint_mask;
	int ret;

	 
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_LEGACY);
	if (ret != 1) {
		dev_err(&pdev->dev, "Failed to allocate IRQ vector");
		return ret;
	}

	 
	ret = pci_irq_vector(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get IRQ vector");
		goto err_free_vectors;
	}

	 
	ret = devm_request_threaded_irq(&pdev->dev, ret, NULL, idt_thread_isr,
					IRQF_ONESHOT, NTB_IRQNAME, ndev);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to set MSI IRQ handler, %d", ret);
		goto err_free_vectors;
	}

	 
	ntint_mask = idt_nt_read(ndev, IDT_NT_NTINTMSK) & ~IDT_NTINTMSK_ALL;
	idt_nt_write(ndev, IDT_NT_NTINTMSK, ntint_mask);

	 
	dev_dbg(&pdev->dev, "NTB interrupts initialized");

	return 0;

err_free_vectors:
	pci_free_irq_vectors(pdev);

	return ret;
}

 
static void idt_deinit_isr(struct idt_ntb_dev *ndev)
{
	struct pci_dev *pdev = ndev->ntb.pdev;
	u32 ntint_mask;

	 
	ntint_mask = idt_nt_read(ndev, IDT_NT_NTINTMSK) | IDT_NTINTMSK_ALL;
	idt_nt_write(ndev, IDT_NT_NTINTMSK, ntint_mask);

	 
	devm_free_irq(&pdev->dev, pci_irq_vector(pdev, 0), ndev);

	 
	pci_free_irq_vectors(pdev);

	dev_dbg(&pdev->dev, "NTB interrupts deinitialized");
}

 
static irqreturn_t idt_thread_isr(int irq, void *devid)
{
	struct idt_ntb_dev *ndev = devid;
	bool handled = false;
	u32 ntint_sts;

	 
	ntint_sts = idt_nt_read(ndev, IDT_NT_NTINTSTS);

	 
	if (ntint_sts & IDT_NTINTSTS_MSG) {
		idt_msg_isr(ndev, ntint_sts);
		handled = true;
	}

	 
	if (ntint_sts & IDT_NTINTSTS_DBELL) {
		idt_db_isr(ndev, ntint_sts);
		handled = true;
	}

	 
	if (ntint_sts & IDT_NTINTSTS_SEVENT) {
		idt_se_isr(ndev, ntint_sts);
		handled = true;
	}

	dev_dbg(&ndev->ntb.pdev->dev, "IDT IRQs 0x%08x handled", ntint_sts);

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

 

 
static const struct ntb_dev_ops idt_ntb_ops = {
	.port_number		= idt_ntb_port_number,
	.peer_port_count	= idt_ntb_peer_port_count,
	.peer_port_number	= idt_ntb_peer_port_number,
	.peer_port_idx		= idt_ntb_peer_port_idx,
	.link_is_up		= idt_ntb_link_is_up,
	.link_enable		= idt_ntb_link_enable,
	.link_disable		= idt_ntb_link_disable,
	.mw_count		= idt_ntb_mw_count,
	.mw_get_align		= idt_ntb_mw_get_align,
	.peer_mw_count		= idt_ntb_peer_mw_count,
	.peer_mw_get_addr	= idt_ntb_peer_mw_get_addr,
	.peer_mw_set_trans	= idt_ntb_peer_mw_set_trans,
	.peer_mw_clear_trans	= idt_ntb_peer_mw_clear_trans,
	.db_valid_mask		= idt_ntb_db_valid_mask,
	.db_read		= idt_ntb_db_read,
	.db_clear		= idt_ntb_db_clear,
	.db_read_mask		= idt_ntb_db_read_mask,
	.db_set_mask		= idt_ntb_db_set_mask,
	.db_clear_mask		= idt_ntb_db_clear_mask,
	.peer_db_set		= idt_ntb_peer_db_set,
	.msg_count		= idt_ntb_msg_count,
	.msg_inbits		= idt_ntb_msg_inbits,
	.msg_outbits		= idt_ntb_msg_outbits,
	.msg_read_sts		= idt_ntb_msg_read_sts,
	.msg_clear_sts		= idt_ntb_msg_clear_sts,
	.msg_set_mask		= idt_ntb_msg_set_mask,
	.msg_clear_mask		= idt_ntb_msg_clear_mask,
	.msg_read		= idt_ntb_msg_read,
	.peer_msg_write		= idt_ntb_peer_msg_write
};

 
static int idt_register_device(struct idt_ntb_dev *ndev)
{
	int ret;

	 
	ndev->ntb.ops = &idt_ntb_ops;
	ndev->ntb.topo = NTB_TOPO_SWITCH;

	ret = ntb_register_device(&ndev->ntb);
	if (ret != 0) {
		dev_err(&ndev->ntb.pdev->dev, "Failed to register NTB device");
		return ret;
	}

	dev_dbg(&ndev->ntb.pdev->dev, "NTB device successfully registered");

	return 0;
}

 
static void idt_unregister_device(struct idt_ntb_dev *ndev)
{
	 
	ntb_unregister_device(&ndev->ntb);

	dev_dbg(&ndev->ntb.pdev->dev, "NTB device unregistered");
}

 

static ssize_t idt_dbgfs_info_read(struct file *filp, char __user *ubuf,
				   size_t count, loff_t *offp);

 
static const struct file_operations idt_dbgfs_info_ops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = idt_dbgfs_info_read
};

 
static ssize_t idt_dbgfs_info_read(struct file *filp, char __user *ubuf,
				   size_t count, loff_t *offp)
{
	struct idt_ntb_dev *ndev = filp->private_data;
	unsigned char idx, pidx, cnt;
	unsigned long irqflags, mdeg;
	ssize_t ret = 0, off = 0;
	enum ntb_speed speed;
	enum ntb_width width;
	char *strbuf;
	size_t size;
	u32 data;

	 
	size = min_t(size_t, count, 0x1000U);

	 
	strbuf = kmalloc(size, GFP_KERNEL);
	if (strbuf == NULL)
		return -ENOMEM;

	 
	off += scnprintf(strbuf + off, size - off,
		"\n\t\tIDT NTB device Information:\n\n");

	 
	off += scnprintf(strbuf + off, size - off,
		"Local Port %hhu, Partition %hhu\n", ndev->port, ndev->part);

	 
	off += scnprintf(strbuf + off, size - off, "Peers:\n");
	for (idx = 0; idx < ndev->peer_cnt; idx++) {
		off += scnprintf(strbuf + off, size - off,
			"\t%hhu. Port %hhu, Partition %hhu\n",
			idx, ndev->peers[idx].port, ndev->peers[idx].part);
	}

	 
	data = idt_ntb_link_is_up(&ndev->ntb, &speed, &width);
	off += scnprintf(strbuf + off, size - off,
		"NTB link status\t- 0x%08x, ", data);
	off += scnprintf(strbuf + off, size - off, "PCIe Gen %d x%d lanes\n",
		speed, width);

	 
	off += scnprintf(strbuf + off, size - off, "NTB Mapping Table:\n");
	for (idx = 0; idx < IDT_MTBL_ENTRY_CNT; idx++) {
		spin_lock_irqsave(&ndev->mtbl_lock, irqflags);
		idt_nt_write(ndev, IDT_NT_NTMTBLADDR, idx);
		data = idt_nt_read(ndev, IDT_NT_NTMTBLDATA);
		spin_unlock_irqrestore(&ndev->mtbl_lock, irqflags);

		 
		if (data & IDT_NTMTBLDATA_VALID) {
			off += scnprintf(strbuf + off, size - off,
				"\t%hhu. Partition %d, Requester ID 0x%04x\n",
				idx, GET_FIELD(NTMTBLDATA_PART, data),
				GET_FIELD(NTMTBLDATA_REQID, data));
		}
	}
	off += scnprintf(strbuf + off, size - off, "\n");

	 
	off += scnprintf(strbuf + off, size - off,
		"Outbound Memory Windows:\n");
	for (idx = 0; idx < ndev->mw_cnt; idx += cnt) {
		data = ndev->mws[idx].type;
		cnt = idt_get_mw_count(data);

		 
		if (data == IDT_MW_DIR)
			off += scnprintf(strbuf + off, size - off,
				"\t%hhu.\t", idx);
		else
			off += scnprintf(strbuf + off, size - off,
				"\t%hhu-%d.\t", idx, idx + cnt - 1);

		off += scnprintf(strbuf + off, size - off, "%s BAR%hhu, ",
			idt_get_mw_name(data), ndev->mws[idx].bar);

		off += scnprintf(strbuf + off, size - off,
			"Address align 0x%08llx, ", ndev->mws[idx].addr_align);

		off += scnprintf(strbuf + off, size - off,
			"Size align 0x%08llx, Size max %llu\n",
			ndev->mws[idx].size_align, ndev->mws[idx].size_max);
	}

	 
	for (pidx = 0; pidx < ndev->peer_cnt; pidx++) {
		off += scnprintf(strbuf + off, size - off,
			"Inbound Memory Windows for peer %hhu (Port %hhu):\n",
			pidx, ndev->peers[pidx].port);

		 
		for (idx = 0; idx < ndev->peers[pidx].mw_cnt; idx += cnt) {
			data = ndev->peers[pidx].mws[idx].type;
			cnt = idt_get_mw_count(data);

			if (data == IDT_MW_DIR)
				off += scnprintf(strbuf + off, size - off,
					"\t%hhu.\t", idx);
			else
				off += scnprintf(strbuf + off, size - off,
					"\t%hhu-%d.\t", idx, idx + cnt - 1);

			off += scnprintf(strbuf + off, size - off,
				"%s BAR%hhu, ", idt_get_mw_name(data),
				ndev->peers[pidx].mws[idx].bar);

			off += scnprintf(strbuf + off, size - off,
				"Address align 0x%08llx, ",
				ndev->peers[pidx].mws[idx].addr_align);

			off += scnprintf(strbuf + off, size - off,
				"Size align 0x%08llx, Size max %llu\n",
				ndev->peers[pidx].mws[idx].size_align,
				ndev->peers[pidx].mws[idx].size_max);
		}
	}
	off += scnprintf(strbuf + off, size - off, "\n");

	 
	data = idt_sw_read(ndev, IDT_SW_GDBELLSTS);
	off += scnprintf(strbuf + off, size - off,
		 "Global Doorbell state\t- 0x%08x\n", data);
	data = idt_ntb_db_read(&ndev->ntb);
	off += scnprintf(strbuf + off, size - off,
		 "Local  Doorbell state\t- 0x%08x\n", data);
	data = idt_nt_read(ndev, IDT_NT_INDBELLMSK);
	off += scnprintf(strbuf + off, size - off,
		 "Local  Doorbell mask\t- 0x%08x\n", data);
	off += scnprintf(strbuf + off, size - off, "\n");

	 
	off += scnprintf(strbuf + off, size - off,
		 "Message event valid\t- 0x%08x\n", IDT_MSG_MASK);
	data = idt_ntb_msg_read_sts(&ndev->ntb);
	off += scnprintf(strbuf + off, size - off,
		 "Message event status\t- 0x%08x\n", data);
	data = idt_nt_read(ndev, IDT_NT_MSGSTSMSK);
	off += scnprintf(strbuf + off, size - off,
		 "Message event mask\t- 0x%08x\n", data);
	off += scnprintf(strbuf + off, size - off,
		 "Message data:\n");
	for (idx = 0; idx < IDT_MSG_CNT; idx++) {
		int src;
		data = idt_ntb_msg_read(&ndev->ntb, &src, idx);
		off += scnprintf(strbuf + off, size - off,
			"\t%hhu. 0x%08x from peer %d (Port %hhu)\n",
			idx, data, src, ndev->peers[src].port);
	}
	off += scnprintf(strbuf + off, size - off, "\n");

	 
	idt_read_temp(ndev, IDT_TEMP_CUR, &mdeg);
	off += scnprintf(strbuf + off, size - off,
		"Switch temperature\t\t- %hhd.%hhuC\n",
		idt_get_deg(mdeg), idt_get_deg_frac(mdeg));

	 
	ret = simple_read_from_buffer(ubuf, count, offp, strbuf, off);
	kfree(strbuf);

	return ret;
}

 
static int idt_init_dbgfs(struct idt_ntb_dev *ndev)
{
	char devname[64];

	 
	if (IS_ERR_OR_NULL(dbgfs_topdir)) {
		dev_info(&ndev->ntb.pdev->dev, "Top DebugFS directory absent");
		return PTR_ERR_OR_ZERO(dbgfs_topdir);
	}

	 
	snprintf(devname, 64, "info:%s", pci_name(ndev->ntb.pdev));
	ndev->dbgfs_info = debugfs_create_file(devname, 0400, dbgfs_topdir,
		ndev, &idt_dbgfs_info_ops);
	if (IS_ERR(ndev->dbgfs_info)) {
		dev_dbg(&ndev->ntb.pdev->dev, "Failed to create DebugFS node");
		return PTR_ERR(ndev->dbgfs_info);
	}

	dev_dbg(&ndev->ntb.pdev->dev, "NTB device DebugFS node created");

	return 0;
}

 
static void idt_deinit_dbgfs(struct idt_ntb_dev *ndev)
{
	debugfs_remove(ndev->dbgfs_info);

	dev_dbg(&ndev->ntb.pdev->dev, "NTB device DebugFS node discarded");
}

 

 
static int idt_check_setup(struct pci_dev *pdev)
{
	u32 data;
	int ret;

	 
	ret = pci_read_config_dword(pdev, IDT_NT_BARSETUP0, &data);
	if (ret != 0) {
		dev_err(&pdev->dev,
			"Failed to read BARSETUP0 config register");
		return ret;
	}

	 
	if (!(data & IDT_BARSETUP_EN) || !(data & IDT_BARSETUP_MODE_CFG)) {
		dev_err(&pdev->dev, "BAR0 doesn't map config space");
		return -EINVAL;
	}

	 
	if ((data & IDT_BARSETUP_SIZE_MASK) != IDT_BARSETUP_SIZE_CFG) {
		dev_err(&pdev->dev, "Invalid size of config space");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "NTB device pre-initialized correctly");

	return 0;
}

 
static struct idt_ntb_dev *idt_create_dev(struct pci_dev *pdev,
					  const struct pci_device_id *id)
{
	struct idt_ntb_dev *ndev;

	 
	ndev = devm_kzalloc(&pdev->dev, sizeof(*ndev), GFP_KERNEL);
	if (!ndev) {
		dev_err(&pdev->dev, "Memory allocation failed for descriptor");
		return ERR_PTR(-ENOMEM);
	}

	 
	ndev->swcfg = (struct idt_89hpes_cfg *)id->driver_data;
	 
	ndev->ntb.pdev = pdev;

	 
	spin_lock_init(&ndev->db_mask_lock);
	spin_lock_init(&ndev->msg_mask_lock);
	spin_lock_init(&ndev->gasa_lock);

	dev_info(&pdev->dev, "IDT %s discovered", ndev->swcfg->name);

	dev_dbg(&pdev->dev, "NTB device descriptor created");

	return ndev;
}

 
static int idt_init_pci(struct idt_ntb_dev *ndev)
{
	struct pci_dev *pdev = ndev->ntb.pdev;
	int ret;

	 
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret != 0) {
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret != 0) {
			dev_err(&pdev->dev, "Failed to set DMA bit mask\n");
			return ret;
		}
		dev_warn(&pdev->dev, "Cannot set DMA highmem bit mask\n");
	}

	 
	pci_aer_clear_nonfatal_status(pdev);

	 
	ret = pcim_enable_device(pdev);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to enable PCIe device\n");
		return ret;
	}

	 
	pci_set_master(pdev);

	 
	ret = pcim_iomap_regions_request_all(pdev, 1, NTB_NAME);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to request resources\n");
		goto err_clear_master;
	}

	 
	ndev->cfgspc = pcim_iomap_table(pdev)[0];

	 
	pci_set_drvdata(pdev, ndev);

	dev_dbg(&pdev->dev, "NT-function PCIe interface initialized");

	return 0;

err_clear_master:
	pci_clear_master(pdev);

	return ret;
}

 
static void idt_deinit_pci(struct idt_ntb_dev *ndev)
{
	struct pci_dev *pdev = ndev->ntb.pdev;

	 
	pci_set_drvdata(pdev, NULL);

	 
	pci_clear_master(pdev);

	dev_dbg(&pdev->dev, "NT-function PCIe interface cleared");
}

 

 
static int idt_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *id)
{
	struct idt_ntb_dev *ndev;
	int ret;

	 
	ret = idt_check_setup(pdev);
	if (ret != 0)
		return ret;

	 
	ndev = idt_create_dev(pdev, id);
	if (IS_ERR(ndev))
		return PTR_ERR(ndev);

	 
	ret = idt_init_pci(ndev);
	if (ret != 0)
		return ret;

	 
	(void)idt_scan_ports(ndev);

	 
	idt_init_link(ndev);

	 
	ret = idt_init_mws(ndev);
	if (ret != 0)
		goto err_deinit_link;

	 
	idt_init_msg(ndev);

	 
	idt_init_temp(ndev);

	 
	ret = idt_init_isr(ndev);
	if (ret != 0)
		goto err_deinit_link;

	 
	ret = idt_register_device(ndev);
	if (ret != 0)
		goto err_deinit_isr;

	 
	(void)idt_init_dbgfs(ndev);

	 
	dev_info(&pdev->dev, "IDT NTB device is ready");

	 
	return 0;

err_deinit_isr:
	idt_deinit_isr(ndev);
err_deinit_link:
	idt_deinit_link(ndev);
	idt_deinit_pci(ndev);

	return ret;
}

 
static void idt_pci_remove(struct pci_dev *pdev)
{
	struct idt_ntb_dev *ndev = pci_get_drvdata(pdev);

	 
	idt_deinit_dbgfs(ndev);

	 
	idt_unregister_device(ndev);

	 
	idt_deinit_isr(ndev);

	 
	idt_deinit_link(ndev);

	 
	idt_deinit_pci(ndev);

	 
	dev_info(&pdev->dev, "IDT NTB device is removed");

	 
}

 
static const struct idt_89hpes_cfg idt_89hpes24nt6ag2_config = {
	.name = "89HPES24NT6AG2",
	.port_cnt = 6, .ports = {0, 2, 4, 6, 8, 12}
};
static const struct idt_89hpes_cfg idt_89hpes32nt8ag2_config = {
	.name = "89HPES32NT8AG2",
	.port_cnt = 8, .ports = {0, 2, 4, 6, 8, 12, 16, 20}
};
static const struct idt_89hpes_cfg idt_89hpes32nt8bg2_config = {
	.name = "89HPES32NT8BG2",
	.port_cnt = 8, .ports = {0, 2, 4, 6, 8, 12, 16, 20}
};
static const struct idt_89hpes_cfg idt_89hpes12nt12g2_config = {
	.name = "89HPES12NT12G2",
	.port_cnt = 3, .ports = {0, 8, 16}
};
static const struct idt_89hpes_cfg idt_89hpes16nt16g2_config = {
	.name = "89HPES16NT16G2",
	.port_cnt = 4, .ports = {0, 8, 12, 16}
};
static const struct idt_89hpes_cfg idt_89hpes24nt24g2_config = {
	.name = "89HPES24NT24G2",
	.port_cnt = 8, .ports = {0, 2, 4, 6, 8, 12, 16, 20}
};
static const struct idt_89hpes_cfg idt_89hpes32nt24ag2_config = {
	.name = "89HPES32NT24AG2",
	.port_cnt = 8, .ports = {0, 2, 4, 6, 8, 12, 16, 20}
};
static const struct idt_89hpes_cfg idt_89hpes32nt24bg2_config = {
	.name = "89HPES32NT24BG2",
	.port_cnt = 8, .ports = {0, 2, 4, 6, 8, 12, 16, 20}
};

 
static const struct pci_device_id idt_pci_tbl[] = {
	{IDT_PCI_DEVICE_IDS(89HPES24NT6AG2,  idt_89hpes24nt6ag2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES32NT8AG2,  idt_89hpes32nt8ag2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES32NT8BG2,  idt_89hpes32nt8bg2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES12NT12G2,  idt_89hpes12nt12g2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES16NT16G2,  idt_89hpes16nt16g2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES24NT24G2,  idt_89hpes24nt24g2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES32NT24AG2, idt_89hpes32nt24ag2_config)},
	{IDT_PCI_DEVICE_IDS(89HPES32NT24BG2, idt_89hpes32nt24bg2_config)},
	{0}
};
MODULE_DEVICE_TABLE(pci, idt_pci_tbl);

 
static struct pci_driver idt_pci_driver = {
	.name		= KBUILD_MODNAME,
	.probe		= idt_pci_probe,
	.remove		= idt_pci_remove,
	.id_table	= idt_pci_tbl,
};

static int __init idt_pci_driver_init(void)
{
	int ret;
	pr_info("%s %s\n", NTB_DESC, NTB_VER);

	 
	if (debugfs_initialized())
		dbgfs_topdir = debugfs_create_dir(KBUILD_MODNAME, NULL);

	 
	ret = pci_register_driver(&idt_pci_driver);
	if (ret)
		debugfs_remove_recursive(dbgfs_topdir);

	return ret;
}
module_init(idt_pci_driver_init);

static void __exit idt_pci_driver_exit(void)
{
	 
	pci_unregister_driver(&idt_pci_driver);

	 
	debugfs_remove_recursive(dbgfs_topdir);
}
module_exit(idt_pci_driver_exit);

