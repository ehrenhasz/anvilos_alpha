
 

#include <linux/acpi.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/log2.h>
#include <linux/platform_device.h>
#include <linux/mailbox_controller.h>
#include <linux/mailbox_client.h>
#include <linux/io-64-nonatomic-lo-hi.h>
#include <acpi/pcc.h>

#include "mailbox.h"

#define MBOX_IRQ_NAME		"pcc-mbox"

 
struct pcc_chan_reg {
	void __iomem *vaddr;
	struct acpi_generic_address *gas;
	u64 preserve_mask;
	u64 set_mask;
	u64 status_mask;
};

 
struct pcc_chan_info {
	struct pcc_mbox_chan chan;
	struct pcc_chan_reg db;
	struct pcc_chan_reg plat_irq_ack;
	struct pcc_chan_reg cmd_complete;
	struct pcc_chan_reg cmd_update;
	struct pcc_chan_reg error;
	int plat_irq;
};

#define to_pcc_chan_info(c) container_of(c, struct pcc_chan_info, chan)
static struct pcc_chan_info *chan_info;
static int pcc_chan_count;

 
static void read_register(void __iomem *vaddr, u64 *val, unsigned int bit_width)
{
	switch (bit_width) {
	case 8:
		*val = readb(vaddr);
		break;
	case 16:
		*val = readw(vaddr);
		break;
	case 32:
		*val = readl(vaddr);
		break;
	case 64:
		*val = readq(vaddr);
		break;
	}
}

static void write_register(void __iomem *vaddr, u64 val, unsigned int bit_width)
{
	switch (bit_width) {
	case 8:
		writeb(val, vaddr);
		break;
	case 16:
		writew(val, vaddr);
		break;
	case 32:
		writel(val, vaddr);
		break;
	case 64:
		writeq(val, vaddr);
		break;
	}
}

static int pcc_chan_reg_read(struct pcc_chan_reg *reg, u64 *val)
{
	int ret = 0;

	if (!reg->gas) {
		*val = 0;
		return 0;
	}

	if (reg->vaddr)
		read_register(reg->vaddr, val, reg->gas->bit_width);
	else
		ret = acpi_read(val, reg->gas);

	return ret;
}

static int pcc_chan_reg_write(struct pcc_chan_reg *reg, u64 val)
{
	int ret = 0;

	if (!reg->gas)
		return 0;

	if (reg->vaddr)
		write_register(reg->vaddr, val, reg->gas->bit_width);
	else
		ret = acpi_write(val, reg->gas);

	return ret;
}

static int pcc_chan_reg_read_modify_write(struct pcc_chan_reg *reg)
{
	int ret = 0;
	u64 val;

	ret = pcc_chan_reg_read(reg, &val);
	if (ret)
		return ret;

	val &= reg->preserve_mask;
	val |= reg->set_mask;

	return pcc_chan_reg_write(reg, val);
}

 
static int pcc_map_interrupt(u32 interrupt, u32 flags)
{
	int trigger, polarity;

	if (!interrupt)
		return 0;

	trigger = (flags & ACPI_PCCT_INTERRUPT_MODE) ? ACPI_EDGE_SENSITIVE
			: ACPI_LEVEL_SENSITIVE;

	polarity = (flags & ACPI_PCCT_INTERRUPT_POLARITY) ? ACPI_ACTIVE_LOW
			: ACPI_ACTIVE_HIGH;

	return acpi_register_gsi(NULL, interrupt, trigger, polarity);
}

 
static irqreturn_t pcc_mbox_irq(int irq, void *p)
{
	struct pcc_chan_info *pchan;
	struct mbox_chan *chan = p;
	u64 val;
	int ret;

	pchan = chan->con_priv;

	ret = pcc_chan_reg_read(&pchan->cmd_complete, &val);
	if (ret)
		return IRQ_NONE;

	if (val) {  
		val &= pchan->cmd_complete.status_mask;
		if (!val)
			return IRQ_NONE;
	}

	ret = pcc_chan_reg_read(&pchan->error, &val);
	if (ret)
		return IRQ_NONE;
	val &= pchan->error.status_mask;
	if (val) {
		val &= ~pchan->error.status_mask;
		pcc_chan_reg_write(&pchan->error, val);
		return IRQ_NONE;
	}

	if (pcc_chan_reg_read_modify_write(&pchan->plat_irq_ack))
		return IRQ_NONE;

	mbox_chan_received_data(chan, NULL);

	return IRQ_HANDLED;
}

 
struct pcc_mbox_chan *
pcc_mbox_request_channel(struct mbox_client *cl, int subspace_id)
{
	struct pcc_chan_info *pchan;
	struct mbox_chan *chan;
	int rc;

	if (subspace_id < 0 || subspace_id >= pcc_chan_count)
		return ERR_PTR(-ENOENT);

	pchan = chan_info + subspace_id;
	chan = pchan->chan.mchan;
	if (IS_ERR(chan) || chan->cl) {
		pr_err("Channel not found for idx: %d\n", subspace_id);
		return ERR_PTR(-EBUSY);
	}

	rc = mbox_bind_client(chan, cl);
	if (rc)
		return ERR_PTR(rc);

	return &pchan->chan;
}
EXPORT_SYMBOL_GPL(pcc_mbox_request_channel);

 
void pcc_mbox_free_channel(struct pcc_mbox_chan *pchan)
{
	struct mbox_chan *chan = pchan->mchan;

	if (!chan || !chan->cl)
		return;

	mbox_free_channel(chan);
}
EXPORT_SYMBOL_GPL(pcc_mbox_free_channel);

 
static int pcc_send_data(struct mbox_chan *chan, void *data)
{
	int ret;
	struct pcc_chan_info *pchan = chan->con_priv;

	ret = pcc_chan_reg_read_modify_write(&pchan->cmd_update);
	if (ret)
		return ret;

	return pcc_chan_reg_read_modify_write(&pchan->db);
}

 
static int pcc_startup(struct mbox_chan *chan)
{
	struct pcc_chan_info *pchan = chan->con_priv;
	int rc;

	if (pchan->plat_irq > 0) {
		rc = devm_request_irq(chan->mbox->dev, pchan->plat_irq, pcc_mbox_irq, 0,
				      MBOX_IRQ_NAME, chan);
		if (unlikely(rc)) {
			dev_err(chan->mbox->dev, "failed to register PCC interrupt %d\n",
				pchan->plat_irq);
			return rc;
		}
	}

	return 0;
}

 
static void pcc_shutdown(struct mbox_chan *chan)
{
	struct pcc_chan_info *pchan = chan->con_priv;

	if (pchan->plat_irq > 0)
		devm_free_irq(chan->mbox->dev, pchan->plat_irq, chan);
}

static const struct mbox_chan_ops pcc_chan_ops = {
	.send_data = pcc_send_data,
	.startup = pcc_startup,
	.shutdown = pcc_shutdown,
};

 
static int parse_pcc_subspace(union acpi_subtable_headers *header,
		const unsigned long end)
{
	struct acpi_pcct_subspace *ss = (struct acpi_pcct_subspace *) header;

	if (ss->header.type < ACPI_PCCT_TYPE_RESERVED)
		return 0;

	return -EINVAL;
}

static int
pcc_chan_reg_init(struct pcc_chan_reg *reg, struct acpi_generic_address *gas,
		  u64 preserve_mask, u64 set_mask, u64 status_mask, char *name)
{
	if (gas->space_id == ACPI_ADR_SPACE_SYSTEM_MEMORY) {
		if (!(gas->bit_width >= 8 && gas->bit_width <= 64 &&
		      is_power_of_2(gas->bit_width))) {
			pr_err("Error: Cannot access register of %u bit width",
			       gas->bit_width);
			return -EFAULT;
		}

		reg->vaddr = acpi_os_ioremap(gas->address, gas->bit_width / 8);
		if (!reg->vaddr) {
			pr_err("Failed to ioremap PCC %s register\n", name);
			return -ENOMEM;
		}
	}
	reg->gas = gas;
	reg->preserve_mask = preserve_mask;
	reg->set_mask = set_mask;
	reg->status_mask = status_mask;
	return 0;
}

 
static int pcc_parse_subspace_irq(struct pcc_chan_info *pchan,
				  struct acpi_subtable_header *pcct_entry)
{
	int ret = 0;
	struct acpi_pcct_hw_reduced *pcct_ss;

	if (pcct_entry->type < ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE ||
	    pcct_entry->type > ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE)
		return 0;

	pcct_ss = (struct acpi_pcct_hw_reduced *)pcct_entry;
	pchan->plat_irq = pcc_map_interrupt(pcct_ss->platform_interrupt,
					    (u32)pcct_ss->flags);
	if (pchan->plat_irq <= 0) {
		pr_err("PCC GSI %d not registered\n",
		       pcct_ss->platform_interrupt);
		return -EINVAL;
	}

	if (pcct_ss->header.type == ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2) {
		struct acpi_pcct_hw_reduced_type2 *pcct2_ss = (void *)pcct_ss;

		ret = pcc_chan_reg_init(&pchan->plat_irq_ack,
					&pcct2_ss->platform_ack_register,
					pcct2_ss->ack_preserve_mask,
					pcct2_ss->ack_write_mask, 0,
					"PLAT IRQ ACK");

	} else if (pcct_ss->header.type == ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE ||
		   pcct_ss->header.type == ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE) {
		struct acpi_pcct_ext_pcc_master *pcct_ext = (void *)pcct_ss;

		ret = pcc_chan_reg_init(&pchan->plat_irq_ack,
					&pcct_ext->platform_ack_register,
					pcct_ext->ack_preserve_mask,
					pcct_ext->ack_set_mask, 0,
					"PLAT IRQ ACK");
	}

	return ret;
}

 
static int pcc_parse_subspace_db_reg(struct pcc_chan_info *pchan,
				     struct acpi_subtable_header *pcct_entry)
{
	int ret = 0;

	if (pcct_entry->type <= ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2) {
		struct acpi_pcct_subspace *pcct_ss;

		pcct_ss = (struct acpi_pcct_subspace *)pcct_entry;

		ret = pcc_chan_reg_init(&pchan->db,
					&pcct_ss->doorbell_register,
					pcct_ss->preserve_mask,
					pcct_ss->write_mask, 0,	"Doorbell");

	} else {
		struct acpi_pcct_ext_pcc_master *pcct_ext;

		pcct_ext = (struct acpi_pcct_ext_pcc_master *)pcct_entry;

		ret = pcc_chan_reg_init(&pchan->db,
					&pcct_ext->doorbell_register,
					pcct_ext->preserve_mask,
					pcct_ext->write_mask, 0, "Doorbell");
		if (ret)
			return ret;

		ret = pcc_chan_reg_init(&pchan->cmd_complete,
					&pcct_ext->cmd_complete_register,
					0, 0, pcct_ext->cmd_complete_mask,
					"Command Complete Check");
		if (ret)
			return ret;

		ret = pcc_chan_reg_init(&pchan->cmd_update,
					&pcct_ext->cmd_update_register,
					pcct_ext->cmd_update_preserve_mask,
					pcct_ext->cmd_update_set_mask, 0,
					"Command Complete Update");
		if (ret)
			return ret;

		ret = pcc_chan_reg_init(&pchan->error,
					&pcct_ext->error_status_register,
					0, 0, pcct_ext->error_status_mask,
					"Error Status");
	}
	return ret;
}

 
static void pcc_parse_subspace_shmem(struct pcc_chan_info *pchan,
				     struct acpi_subtable_header *pcct_entry)
{
	if (pcct_entry->type <= ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2) {
		struct acpi_pcct_subspace *pcct_ss =
			(struct acpi_pcct_subspace *)pcct_entry;

		pchan->chan.shmem_base_addr = pcct_ss->base_address;
		pchan->chan.shmem_size = pcct_ss->length;
		pchan->chan.latency = pcct_ss->latency;
		pchan->chan.max_access_rate = pcct_ss->max_access_rate;
		pchan->chan.min_turnaround_time = pcct_ss->min_turnaround_time;
	} else {
		struct acpi_pcct_ext_pcc_master *pcct_ext =
			(struct acpi_pcct_ext_pcc_master *)pcct_entry;

		pchan->chan.shmem_base_addr = pcct_ext->base_address;
		pchan->chan.shmem_size = pcct_ext->length;
		pchan->chan.latency = pcct_ext->latency;
		pchan->chan.max_access_rate = pcct_ext->max_access_rate;
		pchan->chan.min_turnaround_time = pcct_ext->min_turnaround_time;
	}
}

 
static int __init acpi_pcc_probe(void)
{
	int count, i, rc = 0;
	acpi_status status;
	struct acpi_table_header *pcct_tbl;
	struct acpi_subtable_proc proc[ACPI_PCCT_TYPE_RESERVED];

	status = acpi_get_table(ACPI_SIG_PCCT, 0, &pcct_tbl);
	if (ACPI_FAILURE(status) || !pcct_tbl)
		return -ENODEV;

	 
	for (i = ACPI_PCCT_TYPE_GENERIC_SUBSPACE;
	     i < ACPI_PCCT_TYPE_RESERVED; i++) {
		proc[i].id = i;
		proc[i].count = 0;
		proc[i].handler = parse_pcc_subspace;
	}

	count = acpi_table_parse_entries_array(ACPI_SIG_PCCT,
			sizeof(struct acpi_table_pcct), proc,
			ACPI_PCCT_TYPE_RESERVED, MAX_PCC_SUBSPACES);
	if (count <= 0 || count > MAX_PCC_SUBSPACES) {
		if (count < 0)
			pr_warn("Error parsing PCC subspaces from PCCT\n");
		else
			pr_warn("Invalid PCCT: %d PCC subspaces\n", count);

		rc = -EINVAL;
	} else {
		pcc_chan_count = count;
	}

	acpi_put_table(pcct_tbl);

	return rc;
}

 
static int pcc_mbox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mbox_controller *pcc_mbox_ctrl;
	struct mbox_chan *pcc_mbox_channels;
	struct acpi_table_header *pcct_tbl;
	struct acpi_subtable_header *pcct_entry;
	struct acpi_table_pcct *acpi_pcct_tbl;
	acpi_status status = AE_OK;
	int i, rc, count = pcc_chan_count;

	 
	status = acpi_get_table(ACPI_SIG_PCCT, 0, &pcct_tbl);

	if (ACPI_FAILURE(status) || !pcct_tbl)
		return -ENODEV;

	pcc_mbox_channels = devm_kcalloc(dev, count, sizeof(*pcc_mbox_channels),
					 GFP_KERNEL);
	if (!pcc_mbox_channels) {
		rc = -ENOMEM;
		goto err;
	}

	chan_info = devm_kcalloc(dev, count, sizeof(*chan_info), GFP_KERNEL);
	if (!chan_info) {
		rc = -ENOMEM;
		goto err;
	}

	pcc_mbox_ctrl = devm_kzalloc(dev, sizeof(*pcc_mbox_ctrl), GFP_KERNEL);
	if (!pcc_mbox_ctrl) {
		rc = -ENOMEM;
		goto err;
	}

	 
	pcct_entry = (struct acpi_subtable_header *) (
		(unsigned long) pcct_tbl + sizeof(struct acpi_table_pcct));

	acpi_pcct_tbl = (struct acpi_table_pcct *) pcct_tbl;
	if (acpi_pcct_tbl->flags & ACPI_PCCT_DOORBELL)
		pcc_mbox_ctrl->txdone_irq = true;

	for (i = 0; i < count; i++) {
		struct pcc_chan_info *pchan = chan_info + i;

		pcc_mbox_channels[i].con_priv = pchan;
		pchan->chan.mchan = &pcc_mbox_channels[i];

		if (pcct_entry->type == ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE &&
		    !pcc_mbox_ctrl->txdone_irq) {
			pr_err("Platform Interrupt flag must be set to 1");
			rc = -EINVAL;
			goto err;
		}

		if (pcc_mbox_ctrl->txdone_irq) {
			rc = pcc_parse_subspace_irq(pchan, pcct_entry);
			if (rc < 0)
				goto err;
		}
		rc = pcc_parse_subspace_db_reg(pchan, pcct_entry);
		if (rc < 0)
			goto err;

		pcc_parse_subspace_shmem(pchan, pcct_entry);

		pcct_entry = (struct acpi_subtable_header *)
			((unsigned long) pcct_entry + pcct_entry->length);
	}

	pcc_mbox_ctrl->num_chans = count;

	pr_info("Detected %d PCC Subspaces\n", pcc_mbox_ctrl->num_chans);

	pcc_mbox_ctrl->chans = pcc_mbox_channels;
	pcc_mbox_ctrl->ops = &pcc_chan_ops;
	pcc_mbox_ctrl->dev = dev;

	pr_info("Registering PCC driver as Mailbox controller\n");
	rc = mbox_controller_register(pcc_mbox_ctrl);
	if (rc)
		pr_err("Err registering PCC as Mailbox controller: %d\n", rc);
	else
		return 0;
err:
	acpi_put_table(pcct_tbl);
	return rc;
}

static struct platform_driver pcc_mbox_driver = {
	.probe = pcc_mbox_probe,
	.driver = {
		.name = "PCCT",
	},
};

static int __init pcc_init(void)
{
	int ret;
	struct platform_device *pcc_pdev;

	if (acpi_disabled)
		return -ENODEV;

	 
	ret = acpi_pcc_probe();

	if (ret) {
		pr_debug("ACPI PCC probe failed.\n");
		return -ENODEV;
	}

	pcc_pdev = platform_create_bundle(&pcc_mbox_driver,
			pcc_mbox_probe, NULL, 0, NULL, 0);

	if (IS_ERR(pcc_pdev)) {
		pr_debug("Err creating PCC platform bundle\n");
		pcc_chan_count = 0;
		return PTR_ERR(pcc_pdev);
	}

	return 0;
}

 
postcore_initcall(pcc_init);
