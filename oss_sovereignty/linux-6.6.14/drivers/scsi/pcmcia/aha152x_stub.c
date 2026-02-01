 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/major.h>
#include <linux/blkdev.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/scsi_tcq.h>
#include "aha152x.h"

#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>


 

 

 
static int host_id = 7;
static int reconnect = 1;
static int parity = 1;
static int synchronous = 1;
static int reset_delay = 100;
static int ext_trans = 0;

module_param(host_id, int, 0);
module_param(reconnect, int, 0);
module_param(parity, int, 0);
module_param(synchronous, int, 0);
module_param(reset_delay, int, 0);
module_param(ext_trans, int, 0);

MODULE_LICENSE("Dual MPL/GPL");

 

typedef struct scsi_info_t {
	struct pcmcia_device	*p_dev;
    struct Scsi_Host	*host;
} scsi_info_t;

static void aha152x_release_cs(struct pcmcia_device *link);
static void aha152x_detach(struct pcmcia_device *p_dev);
static int aha152x_config_cs(struct pcmcia_device *link);

static int aha152x_probe(struct pcmcia_device *link)
{
    scsi_info_t *info;

    dev_dbg(&link->dev, "aha152x_attach()\n");

     
    info = kzalloc(sizeof(*info), GFP_KERNEL);
    if (!info) return -ENOMEM;
    info->p_dev = link;
    link->priv = info;

    link->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_SET_IO;
    link->config_regs = PRESENT_OPTION;

    return aha152x_config_cs(link);
}  

 

static void aha152x_detach(struct pcmcia_device *link)
{
    dev_dbg(&link->dev, "aha152x_detach\n");

    aha152x_release_cs(link);

     
    kfree(link->priv);
}  

 

static int aha152x_config_check(struct pcmcia_device *p_dev, void *priv_data)
{
	p_dev->io_lines = 10;

	 
	if ((p_dev->resource[0]->end < 0x20) &&
		(p_dev->resource[1]->end >= 0x20))
		p_dev->resource[0]->start = p_dev->resource[1]->start;

	if (p_dev->resource[0]->start >= 0xffff)
		return -EINVAL;

	p_dev->resource[1]->start = p_dev->resource[1]->end = 0;
	p_dev->resource[0]->end = 0x20;
	p_dev->resource[0]->flags &= ~IO_DATA_PATH_WIDTH;
	p_dev->resource[0]->flags |= IO_DATA_PATH_WIDTH_AUTO;

	return pcmcia_request_io(p_dev);
}

static int aha152x_config_cs(struct pcmcia_device *link)
{
    scsi_info_t *info = link->priv;
    struct aha152x_setup s;
    int ret;
    struct Scsi_Host *host;

    dev_dbg(&link->dev, "aha152x_config\n");

    ret = pcmcia_loop_config(link, aha152x_config_check, NULL);
    if (ret)
	    goto failed;

    if (!link->irq)
	    goto failed;

    ret = pcmcia_enable_device(link);
    if (ret)
	    goto failed;
    
     
    memset(&s, 0, sizeof(s));
    s.conf        = "PCMCIA setup";
    s.io_port     = link->resource[0]->start;
    s.irq         = link->irq;
    s.scsiid      = host_id;
    s.reconnect   = reconnect;
    s.parity      = parity;
    s.synchronous = synchronous;
    s.delay       = reset_delay;
    if (ext_trans)
        s.ext_trans = ext_trans;

    host = aha152x_probe_one(&s);
    if (host == NULL) {
	printk(KERN_INFO "aha152x_cs: no SCSI devices found\n");
	goto failed;
    }

    info->host = host;

    return 0;

failed:
    aha152x_release_cs(link);
    return -ENODEV;
}

static void aha152x_release_cs(struct pcmcia_device *link)
{
	scsi_info_t *info = link->priv;

	aha152x_release(info->host);
	pcmcia_disable_device(link);
}

static int aha152x_resume(struct pcmcia_device *link)
{
	scsi_info_t *info = link->priv;

	aha152x_host_reset_host(info->host);

	return 0;
}

static const struct pcmcia_device_id aha152x_ids[] = {
	PCMCIA_DEVICE_PROD_ID123("New Media", "SCSI", "Bus Toaster", 0xcdf7e4cc, 0x35f26476, 0xa8851d6e),
	PCMCIA_DEVICE_PROD_ID123("NOTEWORTHY", "SCSI", "Bus Toaster", 0xad89c6e8, 0x35f26476, 0xa8851d6e),
	PCMCIA_DEVICE_PROD_ID12("Adaptec, Inc.", "APA-1460 SCSI Host Adapter", 0x24ba9738, 0x3a3c3d20),
	PCMCIA_DEVICE_PROD_ID12("New Media Corporation", "Multimedia Sound/SCSI", 0x085a850b, 0x80a6535c),
	PCMCIA_DEVICE_PROD_ID12("NOTEWORTHY", "NWCOMB02 SCSI/AUDIO COMBO CARD", 0xad89c6e8, 0x5f9a615b),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, aha152x_ids);

static struct pcmcia_driver aha152x_cs_driver = {
	.owner		= THIS_MODULE,
	.name		= "aha152x_cs",
	.probe		= aha152x_probe,
	.remove		= aha152x_detach,
	.id_table       = aha152x_ids,
	.resume		= aha152x_resume,
};
module_pcmcia_driver(aha152x_cs_driver);
