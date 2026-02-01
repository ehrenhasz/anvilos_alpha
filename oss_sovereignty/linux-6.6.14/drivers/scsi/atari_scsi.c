 

 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/nvram.h>
#include <linux/bitops.h>
#include <linux/wait.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/atarihw.h>
#include <asm/atariints.h>
#include <asm/atari_stdma.h>
#include <asm/atari_stram.h>
#include <asm/io.h>

#include <scsi/scsi_host.h>

#define DMA_MIN_SIZE                    32

 

#define NCR5380_implementation_fields    

static u8 (*atari_scsi_reg_read)(unsigned int);
static void (*atari_scsi_reg_write)(unsigned int, u8);

#define NCR5380_read(reg)               atari_scsi_reg_read(reg)
#define NCR5380_write(reg, value)       atari_scsi_reg_write(reg, value)

#define NCR5380_queue_command           atari_scsi_queue_command
#define NCR5380_abort                   atari_scsi_abort
#define NCR5380_info                    atari_scsi_info

#define NCR5380_dma_xfer_len            atari_scsi_dma_xfer_len
#define NCR5380_dma_recv_setup          atari_scsi_dma_recv_setup
#define NCR5380_dma_send_setup          atari_scsi_dma_send_setup
#define NCR5380_dma_residual            atari_scsi_dma_residual

#define NCR5380_acquire_dma_irq(instance)      falcon_get_lock(instance)
#define NCR5380_release_dma_irq(instance)      falcon_release_lock()

#include "NCR5380.h"


#define	IS_A_TT()	ATARIHW_PRESENT(TT_SCSI)

#define	SCSI_DMA_WRITE_P(elt,val)				\
	do {							\
		unsigned long v = val;				\
		tt_scsi_dma.elt##_lo = v & 0xff;		\
		v >>= 8;					\
		tt_scsi_dma.elt##_lmd = v & 0xff;		\
		v >>= 8;					\
		tt_scsi_dma.elt##_hmd = v & 0xff;		\
		v >>= 8;					\
		tt_scsi_dma.elt##_hi = v & 0xff;		\
	} while(0)

#define	SCSI_DMA_READ_P(elt)					\
	(((((((unsigned long)tt_scsi_dma.elt##_hi << 8) |	\
	     (unsigned long)tt_scsi_dma.elt##_hmd) << 8) |	\
	   (unsigned long)tt_scsi_dma.elt##_lmd) << 8) |	\
	 (unsigned long)tt_scsi_dma.elt##_lo)


static inline void SCSI_DMA_SETADR(unsigned long adr)
{
	st_dma.dma_lo = (unsigned char)adr;
	MFPDELAY();
	adr >>= 8;
	st_dma.dma_md = (unsigned char)adr;
	MFPDELAY();
	adr >>= 8;
	st_dma.dma_hi = (unsigned char)adr;
	MFPDELAY();
}

static inline unsigned long SCSI_DMA_GETADR(void)
{
	unsigned long adr;
	adr = st_dma.dma_lo;
	MFPDELAY();
	adr |= (st_dma.dma_md & 0xff) << 8;
	MFPDELAY();
	adr |= (st_dma.dma_hi & 0xff) << 16;
	MFPDELAY();
	return adr;
}

static void atari_scsi_fetch_restbytes(void);

static unsigned long	atari_dma_residual, atari_dma_startaddr;
static short		atari_dma_active;
 
static char		*atari_dma_buffer;
 
static unsigned long	atari_dma_phys_buffer;
 
static char		*atari_dma_orig_addr;
 
#define	STRAM_BUFFER_SIZE	(4096)
 
static unsigned long	atari_dma_stram_mask;
#define STRAM_ADDR(a)	(((a) & atari_dma_stram_mask) == 0)

static int setup_can_queue = -1;
module_param(setup_can_queue, int, 0);
static int setup_cmd_per_lun = -1;
module_param(setup_cmd_per_lun, int, 0);
static int setup_sg_tablesize = -1;
module_param(setup_sg_tablesize, int, 0);
static int setup_hostid = -1;
module_param(setup_hostid, int, 0);
static int setup_toshiba_delay = -1;
module_param(setup_toshiba_delay, int, 0);


static int scsi_dma_is_ignored_buserr(unsigned char dma_stat)
{
	int i;
	unsigned long addr = SCSI_DMA_READ_P(dma_addr), end_addr;

	if (dma_stat & 0x01) {

		 

		for (i = 0; i < m68k_num_memory; ++i) {
			end_addr = m68k_memory[i].addr + m68k_memory[i].size;
			if (end_addr <= addr && addr <= end_addr + 4)
				return 1;
		}
	}
	return 0;
}


static irqreturn_t scsi_tt_intr(int irq, void *dev)
{
	struct Scsi_Host *instance = dev;
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int dma_stat;

	dma_stat = tt_scsi_dma.dma_ctrl;

	dsprintk(NDEBUG_INTR, instance, "NCR5380 interrupt, DMA status = %02x\n",
	         dma_stat & 0xff);

	 
	if (dma_stat & 0x80) {
		if (!scsi_dma_is_ignored_buserr(dma_stat)) {
			printk(KERN_ERR "SCSI DMA caused bus error near 0x%08lx\n",
			       SCSI_DMA_READ_P(dma_addr));
			printk(KERN_CRIT "SCSI DMA bus error -- bad DMA programming!");
		}
	}

	 
	if ((dma_stat & 0x02) && !(dma_stat & 0x40)) {
		atari_dma_residual = hostdata->dma_len -
			(SCSI_DMA_READ_P(dma_addr) - atari_dma_startaddr);

		dprintk(NDEBUG_DMA, "SCSI DMA: There are %ld residual bytes.\n",
			   atari_dma_residual);

		if ((signed int)atari_dma_residual < 0)
			atari_dma_residual = 0;
		if ((dma_stat & 1) == 0) {
			 
			atari_scsi_fetch_restbytes();
		} else {
			 
			if (atari_dma_residual & 0x1ff) {
				dprintk(NDEBUG_DMA, "SCSI DMA: DMA bug corrected, "
					   "difference %ld bytes\n",
					   512 - (atari_dma_residual & 0x1ff));
				atari_dma_residual = (atari_dma_residual + 511) & ~0x1ff;
			}
		}
		tt_scsi_dma.dma_ctrl = 0;
	}

	 
	if (dma_stat & 0x40) {
		atari_dma_residual = 0;
		if ((dma_stat & 1) == 0)
			atari_scsi_fetch_restbytes();
		tt_scsi_dma.dma_ctrl = 0;
	}

	NCR5380_intr(irq, dev);

	return IRQ_HANDLED;
}


static irqreturn_t scsi_falcon_intr(int irq, void *dev)
{
	struct Scsi_Host *instance = dev;
	struct NCR5380_hostdata *hostdata = shost_priv(instance);
	int dma_stat;

	 
	st_dma.dma_mode_status = 0x90;
	dma_stat = st_dma.dma_mode_status;

	 
	if (!(dma_stat & 0x01)) {
		 
		printk(KERN_CRIT "SCSI DMA error near 0x%08lx!\n", SCSI_DMA_GETADR());
	}

	 
	if (atari_dma_active && (dma_stat & 0x02)) {
		unsigned long transferred;

		transferred = SCSI_DMA_GETADR() - atari_dma_startaddr;
		 
		if (transferred & 15)
			printk(KERN_ERR "SCSI DMA error: %ld bytes lost in "
			       "ST-DMA fifo\n", transferred & 15);

		atari_dma_residual = hostdata->dma_len - transferred;
		dprintk(NDEBUG_DMA, "SCSI DMA: There are %ld residual bytes.\n",
			   atari_dma_residual);
	} else
		atari_dma_residual = 0;
	atari_dma_active = 0;

	if (atari_dma_orig_addr) {
		 
		memcpy(atari_dma_orig_addr, phys_to_virt(atari_dma_startaddr),
		       hostdata->dma_len - atari_dma_residual);
		atari_dma_orig_addr = NULL;
	}

	NCR5380_intr(irq, dev);

	return IRQ_HANDLED;
}


static void atari_scsi_fetch_restbytes(void)
{
	int nr;
	char *src, *dst;
	unsigned long phys_dst;

	 
	phys_dst = SCSI_DMA_READ_P(dma_addr);
	nr = phys_dst & 3;
	if (nr) {
		 
		phys_dst ^= nr;
		dprintk(NDEBUG_DMA, "SCSI DMA: there are %d rest bytes for phys addr 0x%08lx",
			   nr, phys_dst);
		 
		dst = phys_to_virt(phys_dst);
		dprintk(NDEBUG_DMA, " = virt addr %p\n", dst);
		for (src = (char *)&tt_scsi_dma.dma_restdata; nr != 0; --nr)
			*dst++ = *src++;
	}
}


 

static void falcon_release_lock(void)
{
	if (IS_A_TT())
		return;

	if (stdma_is_locked_by(scsi_falcon_intr))
		stdma_release();
}

 

static int falcon_get_lock(struct Scsi_Host *instance)
{
	if (IS_A_TT())
		return 1;

	if (stdma_is_locked_by(scsi_falcon_intr))
		return 1;

	 
	return stdma_try_lock(scsi_falcon_intr, instance);
}

#ifndef MODULE
static int __init atari_scsi_setup(char *str)
{
	 
	int ints[8];

	get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] < 1) {
		printk("atari_scsi_setup: no arguments!\n");
		return 0;
	}
	if (ints[0] >= 1)
		setup_can_queue = ints[1];
	if (ints[0] >= 2)
		setup_cmd_per_lun = ints[2];
	if (ints[0] >= 3)
		setup_sg_tablesize = ints[3];
	if (ints[0] >= 4)
		setup_hostid = ints[4];
	 
	 
	if (ints[0] >= 7)
		setup_toshiba_delay = ints[7];

	return 1;
}

__setup("atascsi=", atari_scsi_setup);
#endif  

static unsigned long atari_scsi_dma_setup(struct NCR5380_hostdata *hostdata,
					  void *data, unsigned long count,
					  int dir)
{
	unsigned long addr = virt_to_phys(data);

	dprintk(NDEBUG_DMA, "scsi%d: setting up dma, data = %p, phys = %lx, count = %ld, dir = %d\n",
	        hostdata->host->host_no, data, addr, count, dir);

	if (!IS_A_TT() && !STRAM_ADDR(addr)) {
		 
		if (dir)
			memcpy(atari_dma_buffer, data, count);
		else
			atari_dma_orig_addr = data;
		addr = atari_dma_phys_buffer;
	}

	atari_dma_startaddr = addr;	 

	 
	dma_cache_maintenance(addr, count, dir);

	if (IS_A_TT()) {
		tt_scsi_dma.dma_ctrl = dir;
		SCSI_DMA_WRITE_P(dma_addr, addr);
		SCSI_DMA_WRITE_P(dma_cnt, count);
		tt_scsi_dma.dma_ctrl = dir | 2;
	} else {  

		 
		SCSI_DMA_SETADR(addr);

		 
		dir <<= 8;
		st_dma.dma_mode_status = 0x90 | dir;
		st_dma.dma_mode_status = 0x90 | (dir ^ 0x100);
		st_dma.dma_mode_status = 0x90 | dir;
		udelay(40);
		 
		st_dma.fdc_acces_seccount = (count + (dir ? 511 : 0)) >> 9;
		udelay(40);
		st_dma.dma_mode_status = 0x10 | dir;
		udelay(40);
		 
		atari_dma_active = 1;
	}

	return count;
}

static inline int atari_scsi_dma_recv_setup(struct NCR5380_hostdata *hostdata,
                                            unsigned char *data, int count)
{
	return atari_scsi_dma_setup(hostdata, data, count, 0);
}

static inline int atari_scsi_dma_send_setup(struct NCR5380_hostdata *hostdata,
                                            unsigned char *data, int count)
{
	return atari_scsi_dma_setup(hostdata, data, count, 1);
}

static int atari_scsi_dma_residual(struct NCR5380_hostdata *hostdata)
{
	return atari_dma_residual;
}


#define	CMD_SURELY_BLOCK_MODE	0
#define	CMD_SURELY_BYTE_MODE	1
#define	CMD_MODE_UNKNOWN		2

static int falcon_classify_cmd(struct scsi_cmnd *cmd)
{
	unsigned char opcode = cmd->cmnd[0];

	if (opcode == READ_DEFECT_DATA || opcode == READ_LONG ||
	    opcode == READ_BUFFER)
		return CMD_SURELY_BYTE_MODE;
	else if (opcode == READ_6 || opcode == READ_10 ||
		 opcode == 0xa8   || opcode == READ_REVERSE ||
		 opcode == RECOVER_BUFFERED_DATA) {
		 
		if (cmd->device->type == TYPE_TAPE && !(cmd->cmnd[1] & 1))
			return CMD_SURELY_BYTE_MODE;
		else
			return CMD_SURELY_BLOCK_MODE;
	} else
		return CMD_MODE_UNKNOWN;
}


 

static int atari_scsi_dma_xfer_len(struct NCR5380_hostdata *hostdata,
                                   struct scsi_cmnd *cmd)
{
	int wanted_len = NCR5380_to_ncmd(cmd)->this_residual;
	int possible_len, limit;

	if (wanted_len < DMA_MIN_SIZE)
		return 0;

	if (IS_A_TT())
		 
		return wanted_len;

	 

	if (cmd->sc_data_direction == DMA_TO_DEVICE) {
		 
		possible_len = wanted_len;
	} else {
		 
		if (wanted_len & 0x1ff)
			possible_len = 0;
		else {
			 
			switch (falcon_classify_cmd(cmd)) {
			case CMD_SURELY_BLOCK_MODE:
				possible_len = wanted_len;
				break;
			case CMD_SURELY_BYTE_MODE:
				possible_len = 0;  
				break;
			case CMD_MODE_UNKNOWN:
			default:
				 
				possible_len = (wanted_len < 1024) ? 0 : wanted_len;
				break;
			}
		}
	}

	 
	limit = (atari_dma_buffer && !STRAM_ADDR(virt_to_phys(NCR5380_to_ncmd(cmd)->ptr))) ?
		    STRAM_BUFFER_SIZE : 255*512;
	if (possible_len > limit)
		possible_len = limit;

	if (possible_len != wanted_len)
		dprintk(NDEBUG_DMA, "DMA transfer now %d bytes instead of %d\n",
		        possible_len, wanted_len);

	return possible_len;
}


 

static u8 atari_scsi_tt_reg_read(unsigned int reg)
{
	return tt_scsi_regp[reg * 2];
}

static void atari_scsi_tt_reg_write(unsigned int reg, u8 value)
{
	tt_scsi_regp[reg * 2] = value;
}

static u8 atari_scsi_falcon_reg_read(unsigned int reg)
{
	unsigned long flags;
	u8 result;

	reg += 0x88;
	local_irq_save(flags);
	dma_wd.dma_mode_status = (u_short)reg;
	result = (u8)dma_wd.fdc_acces_seccount;
	local_irq_restore(flags);
	return result;
}

static void atari_scsi_falcon_reg_write(unsigned int reg, u8 value)
{
	unsigned long flags;

	reg += 0x88;
	local_irq_save(flags);
	dma_wd.dma_mode_status = (u_short)reg;
	dma_wd.fdc_acces_seccount = (u_short)value;
	local_irq_restore(flags);
}


#include "NCR5380.c"

static int atari_scsi_host_reset(struct scsi_cmnd *cmd)
{
	int rv;
	unsigned long flags;

	local_irq_save(flags);

	 
	if (IS_A_TT()) {
		tt_scsi_dma.dma_ctrl = 0;
	} else {
		if (stdma_is_locked_by(scsi_falcon_intr))
			st_dma.dma_mode_status = 0x90;
		atari_dma_active = 0;
		atari_dma_orig_addr = NULL;
	}

	rv = NCR5380_host_reset(cmd);

	 

	local_irq_restore(flags);

	return rv;
}

#define DRV_MODULE_NAME         "atari_scsi"
#define PFX                     DRV_MODULE_NAME ": "

static struct scsi_host_template atari_scsi_template = {
	.module			= THIS_MODULE,
	.proc_name		= DRV_MODULE_NAME,
	.name			= "Atari native SCSI",
	.info			= atari_scsi_info,
	.queuecommand		= atari_scsi_queue_command,
	.eh_abort_handler	= atari_scsi_abort,
	.eh_host_reset_handler	= atari_scsi_host_reset,
	.this_id		= 7,
	.cmd_per_lun		= 2,
	.dma_boundary		= PAGE_SIZE - 1,
	.cmd_size		= sizeof(struct NCR5380_cmd),
};

static int __init atari_scsi_probe(struct platform_device *pdev)
{
	struct Scsi_Host *instance;
	int error;
	struct resource *irq;
	int host_flags = 0;

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!irq)
		return -ENODEV;

	if (ATARIHW_PRESENT(TT_SCSI)) {
		atari_scsi_reg_read  = atari_scsi_tt_reg_read;
		atari_scsi_reg_write = atari_scsi_tt_reg_write;
	} else {
		atari_scsi_reg_read  = atari_scsi_falcon_reg_read;
		atari_scsi_reg_write = atari_scsi_falcon_reg_write;
	}

	if (ATARIHW_PRESENT(TT_SCSI)) {
		atari_scsi_template.can_queue    = 16;
		atari_scsi_template.sg_tablesize = SG_ALL;
	} else {
		atari_scsi_template.can_queue    = 1;
		atari_scsi_template.sg_tablesize = 1;
	}

	if (setup_can_queue > 0)
		atari_scsi_template.can_queue = setup_can_queue;

	if (setup_cmd_per_lun > 0)
		atari_scsi_template.cmd_per_lun = setup_cmd_per_lun;

	 
	if (ATARIHW_PRESENT(TT_SCSI) && setup_sg_tablesize > 0)
		atari_scsi_template.sg_tablesize = setup_sg_tablesize;

	if (setup_hostid >= 0) {
		atari_scsi_template.this_id = setup_hostid & 7;
	} else if (IS_REACHABLE(CONFIG_NVRAM)) {
		 
		if (ATARIHW_PRESENT(TT_CLK)) {
			unsigned char b;
			loff_t offset = 16;
			ssize_t count = nvram_read(&b, 1, &offset);

			 
			if ((count == 1) && (b & 0x80))
				atari_scsi_template.this_id = b & 7;
		}
	}

	 
	if (ATARIHW_PRESENT(ST_SCSI) && !ATARIHW_PRESENT(EXTD_DMA) &&
	    m68k_realnum_memory > 1) {
		atari_dma_buffer = atari_stram_alloc(STRAM_BUFFER_SIZE, "SCSI");
		if (!atari_dma_buffer) {
			pr_err(PFX "can't allocate ST-RAM double buffer\n");
			return -ENOMEM;
		}
		atari_dma_phys_buffer = atari_stram_to_phys(atari_dma_buffer);
		atari_dma_orig_addr = NULL;
	}

	instance = scsi_host_alloc(&atari_scsi_template,
	                           sizeof(struct NCR5380_hostdata));
	if (!instance) {
		error = -ENOMEM;
		goto fail_alloc;
	}

	instance->irq = irq->start;

	host_flags |= IS_A_TT() ? 0 : FLAG_LATE_DMA_SETUP;
	host_flags |= setup_toshiba_delay > 0 ? FLAG_TOSHIBA_DELAY : 0;

	error = NCR5380_init(instance, host_flags);
	if (error)
		goto fail_init;

	if (IS_A_TT()) {
		error = request_irq(instance->irq, scsi_tt_intr, 0,
		                    "NCR5380", instance);
		if (error) {
			pr_err(PFX "request irq %d failed, aborting\n",
			       instance->irq);
			goto fail_irq;
		}
		tt_mfp.active_edge |= 0x80;	 

		tt_scsi_dma.dma_ctrl = 0;
		atari_dma_residual = 0;

		 
		if (MACH_IS_MEDUSA) {
			struct NCR5380_hostdata *hostdata =
				shost_priv(instance);

			hostdata->read_overruns = 4;
		}
	} else {
		 
		atari_dma_residual = 0;
		atari_dma_active = 0;
		atari_dma_stram_mask = (ATARIHW_PRESENT(EXTD_DMA) ? 0x00000000
					: 0xff000000);
	}

	NCR5380_maybe_reset_bus(instance);

	error = scsi_add_host(instance, NULL);
	if (error)
		goto fail_host;

	platform_set_drvdata(pdev, instance);

	scsi_scan_host(instance);
	return 0;

fail_host:
	if (IS_A_TT())
		free_irq(instance->irq, instance);
fail_irq:
	NCR5380_exit(instance);
fail_init:
	scsi_host_put(instance);
fail_alloc:
	if (atari_dma_buffer)
		atari_stram_free(atari_dma_buffer);
	return error;
}

static int __exit atari_scsi_remove(struct platform_device *pdev)
{
	struct Scsi_Host *instance = platform_get_drvdata(pdev);

	scsi_remove_host(instance);
	if (IS_A_TT())
		free_irq(instance->irq, instance);
	NCR5380_exit(instance);
	scsi_host_put(instance);
	if (atari_dma_buffer)
		atari_stram_free(atari_dma_buffer);
	return 0;
}

static struct platform_driver atari_scsi_driver = {
	.remove = __exit_p(atari_scsi_remove),
	.driver = {
		.name	= DRV_MODULE_NAME,
	},
};

module_platform_driver_probe(atari_scsi_driver, atari_scsi_probe);

MODULE_ALIAS("platform:" DRV_MODULE_NAME);
MODULE_LICENSE("GPL");
