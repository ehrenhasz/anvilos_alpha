
 

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/workqueue.h>
#include <scsi/scsicam.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include "fdomain.h"

 
#define FIFO_COUNT	2	 
#define PARITY_MASK	ACTL_PAREN	 

enum chip_type {
	unknown		= 0x00,
	tmc1800		= 0x01,
	tmc18c50	= 0x02,
	tmc18c30	= 0x03,
};

struct fdomain {
	int base;
	struct scsi_cmnd *cur_cmd;
	enum chip_type chip;
	struct work_struct work;
};

static struct scsi_pointer *fdomain_scsi_pointer(struct scsi_cmnd *cmd)
{
	return scsi_cmd_priv(cmd);
}

static inline void fdomain_make_bus_idle(struct fdomain *fd)
{
	outb(0, fd->base + REG_BCTL);
	outb(0, fd->base + REG_MCTL);
	if (fd->chip == tmc18c50 || fd->chip == tmc18c30)
		 
		outb(ACTL_RESET | ACTL_CLRFIRQ | PARITY_MASK,
		     fd->base + REG_ACTL);
	else
		outb(ACTL_RESET | PARITY_MASK, fd->base + REG_ACTL);
}

static enum chip_type fdomain_identify(int port)
{
	u16 id = inb(port + REG_ID_LSB) | inb(port + REG_ID_MSB) << 8;

	switch (id) {
	case 0x6127:
		return tmc1800;
	case 0x60e9:  
		break;
	default:
		return unknown;
	}

	 
	outb(CFG2_32BIT, port + REG_CFG2);
	if ((inb(port + REG_CFG2) & CFG2_32BIT)) {
		outb(0, port + REG_CFG2);
		if ((inb(port + REG_CFG2) & CFG2_32BIT) == 0)
			return tmc18c30;
	}
	 
	return tmc18c50;
}

static int fdomain_test_loopback(int base)
{
	int i;

	for (i = 0; i < 255; i++) {
		outb(i, base + REG_LOOPBACK);
		if (inb(base + REG_LOOPBACK) != i)
			return 1;
	}

	return 0;
}

static void fdomain_reset(int base)
{
	outb(BCTL_RST, base + REG_BCTL);
	mdelay(20);
	outb(0, base + REG_BCTL);
	mdelay(1150);
	outb(0, base + REG_MCTL);
	outb(PARITY_MASK, base + REG_ACTL);
}

static int fdomain_select(struct Scsi_Host *sh, int target)
{
	int status;
	unsigned long timeout;
	struct fdomain *fd = shost_priv(sh);

	outb(BCTL_BUSEN | BCTL_SEL, fd->base + REG_BCTL);
	outb(BIT(sh->this_id) | BIT(target), fd->base + REG_SCSI_DATA_NOACK);

	 
	outb(PARITY_MASK, fd->base + REG_ACTL);

	timeout = 350;	 

	do {
		status = inb(fd->base + REG_BSTAT);
		if (status & BSTAT_BSY) {
			 
			 
			outb(BCTL_BUSEN, fd->base + REG_BCTL);
			return 0;
		}
		mdelay(1);
	} while (--timeout);
	fdomain_make_bus_idle(fd);
	return 1;
}

static void fdomain_finish_cmd(struct fdomain *fd)
{
	outb(0, fd->base + REG_ICTL);
	fdomain_make_bus_idle(fd);
	scsi_done(fd->cur_cmd);
	fd->cur_cmd = NULL;
}

static void fdomain_read_data(struct scsi_cmnd *cmd)
{
	struct fdomain *fd = shost_priv(cmd->device->host);
	unsigned char *virt, *ptr;
	size_t offset, len;

	while ((len = inw(fd->base + REG_FIFO_COUNT)) > 0) {
		offset = scsi_bufflen(cmd) - scsi_get_resid(cmd);
		virt = scsi_kmap_atomic_sg(scsi_sglist(cmd), scsi_sg_count(cmd),
					   &offset, &len);
		ptr = virt + offset;
		if (len & 1)
			*ptr++ = inb(fd->base + REG_FIFO);
		if (len > 1)
			insw(fd->base + REG_FIFO, ptr, len >> 1);
		scsi_set_resid(cmd, scsi_get_resid(cmd) - len);
		scsi_kunmap_atomic_sg(virt);
	}
}

static void fdomain_write_data(struct scsi_cmnd *cmd)
{
	struct fdomain *fd = shost_priv(cmd->device->host);
	 
	int FIFO_Size = fd->chip == tmc18c30 ? 0x800 : 0x2000;
	unsigned char *virt, *ptr;
	size_t offset, len;

	while ((len = FIFO_Size - inw(fd->base + REG_FIFO_COUNT)) > 512) {
		offset = scsi_bufflen(cmd) - scsi_get_resid(cmd);
		if (len + offset > scsi_bufflen(cmd)) {
			len = scsi_bufflen(cmd) - offset;
			if (len == 0)
				break;
		}
		virt = scsi_kmap_atomic_sg(scsi_sglist(cmd), scsi_sg_count(cmd),
					   &offset, &len);
		ptr = virt + offset;
		if (len & 1)
			outb(*ptr++, fd->base + REG_FIFO);
		if (len > 1)
			outsw(fd->base + REG_FIFO, ptr, len >> 1);
		scsi_set_resid(cmd, scsi_get_resid(cmd) - len);
		scsi_kunmap_atomic_sg(virt);
	}
}

static void fdomain_work(struct work_struct *work)
{
	struct fdomain *fd = container_of(work, struct fdomain, work);
	struct Scsi_Host *sh = container_of((void *)fd, struct Scsi_Host,
					    hostdata);
	struct scsi_cmnd *cmd = fd->cur_cmd;
	struct scsi_pointer *scsi_pointer = fdomain_scsi_pointer(cmd);
	unsigned long flags;
	int status;
	int done = 0;

	spin_lock_irqsave(sh->host_lock, flags);

	if (scsi_pointer->phase & in_arbitration) {
		status = inb(fd->base + REG_ASTAT);
		if (!(status & ASTAT_ARB)) {
			set_host_byte(cmd, DID_BUS_BUSY);
			fdomain_finish_cmd(fd);
			goto out;
		}
		scsi_pointer->phase = in_selection;

		outb(ICTL_SEL | FIFO_COUNT, fd->base + REG_ICTL);
		outb(BCTL_BUSEN | BCTL_SEL, fd->base + REG_BCTL);
		outb(BIT(cmd->device->host->this_id) | BIT(scmd_id(cmd)),
		     fd->base + REG_SCSI_DATA_NOACK);
		 
		outb(ACTL_IRQEN | PARITY_MASK, fd->base + REG_ACTL);
		goto out;
	} else if (scsi_pointer->phase & in_selection) {
		status = inb(fd->base + REG_BSTAT);
		if (!(status & BSTAT_BSY)) {
			 
			if (fdomain_select(cmd->device->host, scmd_id(cmd))) {
				set_host_byte(cmd, DID_NO_CONNECT);
				fdomain_finish_cmd(fd);
				goto out;
			}
			 
			outb(ACTL_IRQEN | PARITY_MASK, fd->base + REG_ACTL);
		}
		scsi_pointer->phase = in_other;
		outb(ICTL_FIFO | ICTL_REQ | FIFO_COUNT, fd->base + REG_ICTL);
		outb(BCTL_BUSEN, fd->base + REG_BCTL);
		goto out;
	}

	 
	status = inb(fd->base + REG_BSTAT);

	if (status & BSTAT_REQ) {
		switch (status & (BSTAT_MSG | BSTAT_CMD | BSTAT_IO)) {
		case BSTAT_CMD:	 
			outb(cmd->cmnd[scsi_pointer->sent_command++],
			     fd->base + REG_SCSI_DATA);
			break;
		case 0:	 
			if (fd->chip != tmc1800 && !scsi_pointer->have_data_in) {
				scsi_pointer->have_data_in = -1;
				outb(ACTL_IRQEN | ACTL_FIFOWR | ACTL_FIFOEN |
				     PARITY_MASK, fd->base + REG_ACTL);
			}
			break;
		case BSTAT_IO:	 
			if (fd->chip != tmc1800 && !scsi_pointer->have_data_in) {
				scsi_pointer->have_data_in = 1;
				outb(ACTL_IRQEN | ACTL_FIFOEN | PARITY_MASK,
				     fd->base + REG_ACTL);
			}
			break;
		case BSTAT_CMD | BSTAT_IO:	 
			scsi_pointer->Status = inb(fd->base + REG_SCSI_DATA);
			break;
		case BSTAT_MSG | BSTAT_CMD:	 
			outb(MESSAGE_REJECT, fd->base + REG_SCSI_DATA);
			break;
		case BSTAT_MSG | BSTAT_CMD | BSTAT_IO:	 
			scsi_pointer->Message = inb(fd->base + REG_SCSI_DATA);
			if (scsi_pointer->Message == COMMAND_COMPLETE)
				++done;
			break;
		}
	}

	if (fd->chip == tmc1800 && !scsi_pointer->have_data_in &&
	    scsi_pointer->sent_command >= cmd->cmd_len) {
		if (cmd->sc_data_direction == DMA_TO_DEVICE) {
			scsi_pointer->have_data_in = -1;
			outb(ACTL_IRQEN | ACTL_FIFOWR | ACTL_FIFOEN |
			     PARITY_MASK, fd->base + REG_ACTL);
		} else {
			scsi_pointer->have_data_in = 1;
			outb(ACTL_IRQEN | ACTL_FIFOEN | PARITY_MASK,
			     fd->base + REG_ACTL);
		}
	}

	if (scsi_pointer->have_data_in == -1)  
		fdomain_write_data(cmd);

	if (scsi_pointer->have_data_in == 1)  
		fdomain_read_data(cmd);

	if (done) {
		set_status_byte(cmd, scsi_pointer->Status);
		set_host_byte(cmd, DID_OK);
		scsi_msg_to_host_byte(cmd, scsi_pointer->Message);
		fdomain_finish_cmd(fd);
	} else {
		if (scsi_pointer->phase & disconnect) {
			outb(ICTL_FIFO | ICTL_SEL | ICTL_REQ | FIFO_COUNT,
			     fd->base + REG_ICTL);
			outb(0, fd->base + REG_BCTL);
		} else
			outb(ICTL_FIFO | ICTL_REQ | FIFO_COUNT,
			     fd->base + REG_ICTL);
	}
out:
	spin_unlock_irqrestore(sh->host_lock, flags);
}

static irqreturn_t fdomain_irq(int irq, void *dev_id)
{
	struct fdomain *fd = dev_id;

	 
	if ((inb(fd->base + REG_ASTAT) & ASTAT_IRQ) == 0)
		return IRQ_NONE;

	outb(0, fd->base + REG_ICTL);

	 
	if (!fd->cur_cmd)	 
		return IRQ_NONE;

	schedule_work(&fd->work);

	return IRQ_HANDLED;
}

static int fdomain_queue(struct Scsi_Host *sh, struct scsi_cmnd *cmd)
{
	struct scsi_pointer *scsi_pointer = fdomain_scsi_pointer(cmd);
	struct fdomain *fd = shost_priv(cmd->device->host);
	unsigned long flags;

	scsi_pointer->Status		= 0;
	scsi_pointer->Message		= 0;
	scsi_pointer->have_data_in	= 0;
	scsi_pointer->sent_command	= 0;
	scsi_pointer->phase		= in_arbitration;
	scsi_set_resid(cmd, scsi_bufflen(cmd));

	spin_lock_irqsave(sh->host_lock, flags);

	fd->cur_cmd = cmd;

	fdomain_make_bus_idle(fd);

	 
	outb(0, fd->base + REG_ICTL);
	outb(0, fd->base + REG_BCTL);	 
	 
	outb(BIT(cmd->device->host->this_id), fd->base + REG_SCSI_DATA_NOACK);
	outb(ICTL_ARB, fd->base + REG_ICTL);
	 
	outb(ACTL_ARB | ACTL_IRQEN | PARITY_MASK, fd->base + REG_ACTL);

	spin_unlock_irqrestore(sh->host_lock, flags);

	return 0;
}

static int fdomain_abort(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *sh = cmd->device->host;
	struct fdomain *fd = shost_priv(sh);
	unsigned long flags;

	if (!fd->cur_cmd)
		return FAILED;

	spin_lock_irqsave(sh->host_lock, flags);

	fdomain_make_bus_idle(fd);
	fdomain_scsi_pointer(fd->cur_cmd)->phase |= aborted;

	 
	set_host_byte(fd->cur_cmd, DID_ABORT);
	fdomain_finish_cmd(fd);
	spin_unlock_irqrestore(sh->host_lock, flags);
	return SUCCESS;
}

static int fdomain_host_reset(struct scsi_cmnd *cmd)
{
	struct Scsi_Host *sh = cmd->device->host;
	struct fdomain *fd = shost_priv(sh);
	unsigned long flags;

	spin_lock_irqsave(sh->host_lock, flags);
	fdomain_reset(fd->base);
	spin_unlock_irqrestore(sh->host_lock, flags);
	return SUCCESS;
}

static int fdomain_biosparam(struct scsi_device *sdev,
			     struct block_device *bdev,	sector_t capacity,
			     int geom[])
{
	unsigned char *p = scsi_bios_ptable(bdev);

	if (p && p[65] == 0xaa && p[64] == 0x55  
	    && p[4]) {	  
		geom[0] = p[5] + 1;	 
		geom[1] = p[6] & 0x3f;	 
	} else {
		if (capacity >= 0x7e0000) {
			geom[0] = 255;	 
			geom[1] = 63;	 
		} else if (capacity >= 0x200000) {
			geom[0] = 128;	 
			geom[1] = 63;	 
		} else {
			geom[0] = 64;	 
			geom[1] = 32;	 
		}
	}
	geom[2] = sector_div(capacity, geom[0] * geom[1]);
	kfree(p);

	return 0;
}

static const struct scsi_host_template fdomain_template = {
	.module			= THIS_MODULE,
	.name			= "Future Domain TMC-16x0",
	.proc_name		= "fdomain",
	.queuecommand		= fdomain_queue,
	.eh_abort_handler	= fdomain_abort,
	.eh_host_reset_handler	= fdomain_host_reset,
	.bios_param		= fdomain_biosparam,
	.can_queue		= 1,
	.this_id		= 7,
	.sg_tablesize		= 64,
	.dma_boundary		= PAGE_SIZE - 1,
	.cmd_size		= sizeof(struct scsi_pointer),
};

struct Scsi_Host *fdomain_create(int base, int irq, int this_id,
				 struct device *dev)
{
	struct Scsi_Host *sh;
	struct fdomain *fd;
	enum chip_type chip;
	static const char * const chip_names[] = {
		"Unknown", "TMC-1800", "TMC-18C50", "TMC-18C30"
	};
	unsigned long irq_flags = 0;

	chip = fdomain_identify(base);
	if (!chip)
		return NULL;

	fdomain_reset(base);

	if (fdomain_test_loopback(base))
		return NULL;

	if (!irq) {
		dev_err(dev, "card has no IRQ assigned");
		return NULL;
	}

	sh = scsi_host_alloc(&fdomain_template, sizeof(struct fdomain));
	if (!sh)
		return NULL;

	if (this_id)
		sh->this_id = this_id & 0x07;

	sh->irq = irq;
	sh->io_port = base;
	sh->n_io_port = FDOMAIN_REGION_SIZE;

	fd = shost_priv(sh);
	fd->base = base;
	fd->chip = chip;
	INIT_WORK(&fd->work, fdomain_work);

	if (dev_is_pci(dev) || !strcmp(dev->bus->name, "pcmcia"))
		irq_flags = IRQF_SHARED;

	if (request_irq(irq, fdomain_irq, irq_flags, "fdomain", fd))
		goto fail_put;

	shost_printk(KERN_INFO, sh, "%s chip at 0x%x irq %d SCSI ID %d\n",
		     dev_is_pci(dev) ? "TMC-36C70 (PCI bus)" : chip_names[chip],
		     base, irq, sh->this_id);

	if (scsi_add_host(sh, dev))
		goto fail_free_irq;

	scsi_scan_host(sh);

	return sh;

fail_free_irq:
	free_irq(irq, fd);
fail_put:
	scsi_host_put(sh);
	return NULL;
}
EXPORT_SYMBOL_GPL(fdomain_create);

int fdomain_destroy(struct Scsi_Host *sh)
{
	struct fdomain *fd = shost_priv(sh);

	cancel_work_sync(&fd->work);
	scsi_remove_host(sh);
	if (sh->irq)
		free_irq(sh->irq, fd);
	scsi_host_put(sh);
	return 0;
}
EXPORT_SYMBOL_GPL(fdomain_destroy);

#ifdef CONFIG_PM_SLEEP
static int fdomain_resume(struct device *dev)
{
	struct fdomain *fd = shost_priv(dev_get_drvdata(dev));

	fdomain_reset(fd->base);
	return 0;
}

static SIMPLE_DEV_PM_OPS(fdomain_pm_ops, NULL, fdomain_resume);
#endif  

MODULE_AUTHOR("Ondrej Zary, Rickard E. Faith");
MODULE_DESCRIPTION("Future Domain TMC-16x0/TMC-3260 SCSI driver");
MODULE_LICENSE("GPL");
