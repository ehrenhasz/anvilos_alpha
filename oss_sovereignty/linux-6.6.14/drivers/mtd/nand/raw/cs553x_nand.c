
 

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
#include <linux/iopoll.h>

#include <asm/msr.h>

#define NR_CS553X_CONTROLLERS	4

#define MSR_DIVIL_GLD_CAP	0x51400000	 
#define CAP_CS5535		0x2df000ULL
#define CAP_CS5536		0x5df500ULL

 
#define MSR_NANDF_DATA		0x5140001b	 
#define MSR_NANDF_CTL		0x5140001c	 
#define MSR_NANDF_RSVD		0x5140001d	 

 
#define MSR_DIVIL_LBAR_FLSH0	0x51400010	 
#define MSR_DIVIL_LBAR_FLSH1	0x51400011	 
#define MSR_DIVIL_LBAR_FLSH2	0x51400012	 
#define MSR_DIVIL_LBAR_FLSH3	0x51400013	 
	 
#define FLSH_LBAR_EN		(1ULL<<32)
#define FLSH_NOR_NAND		(1ULL<<33)	 
#define FLSH_MEM_IO		(1ULL<<34)	 
	 
	 

 
#define MSR_DIVIL_BALL_OPTS	0x51400015
#define PIN_OPT_IDE		(1<<0)	 

 
#define MM_NAND_DATA		0x00	 
#define MM_NAND_CTL		0x800	 
#define MM_NAND_IO		0x801	 
#define MM_NAND_STS		0x810
#define MM_NAND_ECC_LSB		0x811
#define MM_NAND_ECC_MSB		0x812
#define MM_NAND_ECC_COL		0x813
#define MM_NAND_LAC		0x814
#define MM_NAND_ECC_CTL		0x815

 
#define IO_NAND_DATA		0x00	 
#define IO_NAND_CTL		0x04
#define IO_NAND_IO		0x05
#define IO_NAND_STS		0x06
#define IO_NAND_ECC_CTL		0x08
#define IO_NAND_ECC_LSB		0x09
#define IO_NAND_ECC_MSB		0x0a
#define IO_NAND_ECC_COL		0x0b
#define IO_NAND_LAC		0x0c

#define CS_NAND_CTL_DIST_EN	(1<<4)	 
#define CS_NAND_CTL_RDY_INT_MASK	(1<<3)	 
#define CS_NAND_CTL_ALE		(1<<2)
#define CS_NAND_CTL_CLE		(1<<1)
#define CS_NAND_CTL_CE		(1<<0)	 

#define CS_NAND_STS_FLASH_RDY	(1<<3)
#define CS_NAND_CTLR_BUSY	(1<<2)
#define CS_NAND_CMD_COMP	(1<<1)
#define CS_NAND_DIST_ST		(1<<0)

#define CS_NAND_ECC_PARITY	(1<<2)
#define CS_NAND_ECC_CLRECC	(1<<1)
#define CS_NAND_ECC_ENECC	(1<<0)

struct cs553x_nand_controller {
	struct nand_controller base;
	struct nand_chip chip;
	void __iomem *mmio;
};

static struct cs553x_nand_controller *
to_cs553x(struct nand_controller *controller)
{
	return container_of(controller, struct cs553x_nand_controller, base);
}

static int cs553x_write_ctrl_byte(struct cs553x_nand_controller *cs553x,
				  u32 ctl, u8 data)
{
	u8 status;

	writeb(ctl, cs553x->mmio + MM_NAND_CTL);
	writeb(data, cs553x->mmio + MM_NAND_IO);
	return readb_poll_timeout_atomic(cs553x->mmio + MM_NAND_STS, status,
					!(status & CS_NAND_CTLR_BUSY), 1,
					100000);
}

static void cs553x_data_in(struct cs553x_nand_controller *cs553x, void *buf,
			   unsigned int len)
{
	writeb(0, cs553x->mmio + MM_NAND_CTL);
	while (unlikely(len > 0x800)) {
		memcpy_fromio(buf, cs553x->mmio, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_fromio(buf, cs553x->mmio, len);
}

static void cs553x_data_out(struct cs553x_nand_controller *cs553x,
			    const void *buf, unsigned int len)
{
	writeb(0, cs553x->mmio + MM_NAND_CTL);
	while (unlikely(len > 0x800)) {
		memcpy_toio(cs553x->mmio, buf, 0x800);
		buf += 0x800;
		len -= 0x800;
	}
	memcpy_toio(cs553x->mmio, buf, len);
}

static int cs553x_wait_ready(struct cs553x_nand_controller *cs553x,
			     unsigned int timeout_ms)
{
	u8 mask = CS_NAND_CTLR_BUSY | CS_NAND_STS_FLASH_RDY;
	u8 status;

	return readb_poll_timeout(cs553x->mmio + MM_NAND_STS, status,
				  (status & mask) == CS_NAND_STS_FLASH_RDY, 100,
				  timeout_ms * 1000);
}

static int cs553x_exec_instr(struct cs553x_nand_controller *cs553x,
			     const struct nand_op_instr *instr)
{
	unsigned int i;
	int ret = 0;

	switch (instr->type) {
	case NAND_OP_CMD_INSTR:
		ret = cs553x_write_ctrl_byte(cs553x, CS_NAND_CTL_CLE,
					     instr->ctx.cmd.opcode);
		break;

	case NAND_OP_ADDR_INSTR:
		for (i = 0; i < instr->ctx.addr.naddrs; i++) {
			ret = cs553x_write_ctrl_byte(cs553x, CS_NAND_CTL_ALE,
						     instr->ctx.addr.addrs[i]);
			if (ret)
				break;
		}
		break;

	case NAND_OP_DATA_IN_INSTR:
		cs553x_data_in(cs553x, instr->ctx.data.buf.in,
			       instr->ctx.data.len);
		break;

	case NAND_OP_DATA_OUT_INSTR:
		cs553x_data_out(cs553x, instr->ctx.data.buf.out,
				instr->ctx.data.len);
		break;

	case NAND_OP_WAITRDY_INSTR:
		ret = cs553x_wait_ready(cs553x, instr->ctx.waitrdy.timeout_ms);
		break;
	}

	if (instr->delay_ns)
		ndelay(instr->delay_ns);

	return ret;
}

static int cs553x_exec_op(struct nand_chip *this,
			  const struct nand_operation *op,
			  bool check_only)
{
	struct cs553x_nand_controller *cs553x = to_cs553x(this->controller);
	unsigned int i;
	int ret;

	if (check_only)
		return true;

	 
	writeb(0, cs553x->mmio + MM_NAND_CTL);
	for (i = 0; i < op->ninstrs; i++) {
		ret = cs553x_exec_instr(cs553x, &op->instrs[i]);
		if (ret)
			break;
	}

	 
	writeb(CS_NAND_CTL_CE, cs553x->mmio + MM_NAND_CTL);

	return ret;
}

static void cs_enable_hwecc(struct nand_chip *this, int mode)
{
	struct cs553x_nand_controller *cs553x = to_cs553x(this->controller);

	writeb(0x07, cs553x->mmio + MM_NAND_ECC_CTL);
}

static int cs_calculate_ecc(struct nand_chip *this, const u_char *dat,
			    u_char *ecc_code)
{
	struct cs553x_nand_controller *cs553x = to_cs553x(this->controller);
	uint32_t ecc;

	ecc = readl(cs553x->mmio + MM_NAND_STS);

	ecc_code[1] = ecc >> 8;
	ecc_code[0] = ecc >> 16;
	ecc_code[2] = ecc >> 24;
	return 0;
}

static struct cs553x_nand_controller *controllers[4];

static int cs553x_attach_chip(struct nand_chip *chip)
{
	if (chip->ecc.engine_type != NAND_ECC_ENGINE_TYPE_ON_HOST)
		return 0;

	chip->ecc.size = 256;
	chip->ecc.bytes = 3;
	chip->ecc.hwctl  = cs_enable_hwecc;
	chip->ecc.calculate = cs_calculate_ecc;
	chip->ecc.correct  = rawnand_sw_hamming_correct;
	chip->ecc.strength = 1;

	return 0;
}

static const struct nand_controller_ops cs553x_nand_controller_ops = {
	.exec_op = cs553x_exec_op,
	.attach_chip = cs553x_attach_chip,
};

static int __init cs553x_init_one(int cs, int mmio, unsigned long adr)
{
	struct cs553x_nand_controller *controller;
	int err = 0;
	struct nand_chip *this;
	struct mtd_info *new_mtd;

	pr_notice("Probing CS553x NAND controller CS#%d at %sIO 0x%08lx\n",
		  cs, mmio ? "MM" : "P", adr);

	if (!mmio) {
		pr_notice("PIO mode not yet implemented for CS553X NAND controller\n");
		return -ENXIO;
	}

	 
	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller) {
		err = -ENOMEM;
		goto out;
	}

	this = &controller->chip;
	nand_controller_init(&controller->base);
	controller->base.ops = &cs553x_nand_controller_ops;
	this->controller = &controller->base;
	new_mtd = nand_to_mtd(this);

	 
	new_mtd->owner = THIS_MODULE;

	 
	controller->mmio = ioremap(adr, 4096);
	if (!controller->mmio) {
		pr_warn("ioremap cs553x NAND @0x%08lx failed\n", adr);
		err = -EIO;
		goto out_mtd;
	}

	 
	this->bbt_options = NAND_BBT_USE_FLASH;

	new_mtd->name = kasprintf(GFP_KERNEL, "cs553x_nand_cs%d", cs);
	if (!new_mtd->name) {
		err = -ENOMEM;
		goto out_ior;
	}

	 
	err = nand_scan(this, 1);
	if (err)
		goto out_free;

	controllers[cs] = controller;
	goto out;

out_free:
	kfree(new_mtd->name);
out_ior:
	iounmap(controller->mmio);
out_mtd:
	kfree(controller);
out:
	return err;
}

static int is_geode(void)
{
	 
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 10)
		return 1;  

	if ((boot_cpu_data.x86_vendor == X86_VENDOR_NSC ||
	     boot_cpu_data.x86_vendor == X86_VENDOR_CYRIX) &&
	    boot_cpu_data.x86 == 5 &&
	    boot_cpu_data.x86_model == 5)
		return 1;  

	return 0;
}

static int __init cs553x_init(void)
{
	int err = -ENXIO;
	int i;
	uint64_t val;

	 
	if (!is_geode())
		return -ENXIO;

	 
	rdmsrl(MSR_DIVIL_GLD_CAP, val);
	val &= ~0xFFULL;
	if (val != CAP_CS5535 && val != CAP_CS5536)
		return -ENXIO;

	 
	rdmsrl(MSR_DIVIL_BALL_OPTS, val);
	if (val & PIN_OPT_IDE) {
		pr_info("CS553x NAND controller: Flash I/O not enabled in MSR_DIVIL_BALL_OPTS.\n");
		return -ENXIO;
	}

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		rdmsrl(MSR_DIVIL_LBAR_FLSH0 + i, val);

		if ((val & (FLSH_LBAR_EN|FLSH_NOR_NAND)) == (FLSH_LBAR_EN|FLSH_NOR_NAND))
			err = cs553x_init_one(i, !!(val & FLSH_MEM_IO), val & 0xFFFFFFFF);
	}

	 
	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		if (controllers[i]) {
			 
			mtd_device_register(nand_to_mtd(&controllers[i]->chip),
					    NULL, 0);
			err = 0;
		}
	}

	return err;
}

module_init(cs553x_init);

static void __exit cs553x_cleanup(void)
{
	int i;

	for (i = 0; i < NR_CS553X_CONTROLLERS; i++) {
		struct cs553x_nand_controller *controller = controllers[i];
		struct nand_chip *this = &controller->chip;
		struct mtd_info *mtd = nand_to_mtd(this);
		int ret;

		if (!mtd)
			continue;

		 
		ret = mtd_device_unregister(mtd);
		WARN_ON(ret);
		nand_cleanup(this);
		kfree(mtd->name);
		controllers[i] = NULL;

		 
		iounmap(controller->mmio);

		 
		kfree(controller);
	}
}

module_exit(cs553x_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("NAND controller driver for AMD CS5535/CS5536 companion chip");
