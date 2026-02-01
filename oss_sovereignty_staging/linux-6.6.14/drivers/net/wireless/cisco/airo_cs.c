 

#ifdef __IN_PCMCIA_PACKAGE__
#include <pcmcia/k_compat.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/netdevice.h>

#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>

#include <linux/io.h>

#include "airo.h"


 

MODULE_AUTHOR("Benjamin Reed");
MODULE_DESCRIPTION("Support for Cisco/Aironet 802.11 wireless ethernet "
		   "cards.  This is the module that links the PCMCIA card "
		   "with the airo module.");
MODULE_LICENSE("Dual BSD/GPL");

 

static int airo_config(struct pcmcia_device *link);
static void airo_release(struct pcmcia_device *link);

static void airo_detach(struct pcmcia_device *p_dev);

struct local_info {
	struct net_device *eth_dev;
};

static int airo_probe(struct pcmcia_device *p_dev)
{
	struct local_info *local;

	dev_dbg(&p_dev->dev, "airo_attach()\n");

	 
	local = kzalloc(sizeof(*local), GFP_KERNEL);
	if (!local)
		return -ENOMEM;

	p_dev->priv = local;

	return airo_config(p_dev);
}  

static void airo_detach(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "airo_detach\n");

	airo_release(link);

	if (((struct local_info *)link->priv)->eth_dev) {
		stop_airo_card(((struct local_info *)link->priv)->eth_dev,
			       0);
	}
	((struct local_info *)link->priv)->eth_dev = NULL;

	kfree(link->priv);
}  

static int airo_cs_config_check(struct pcmcia_device *p_dev, void *priv_data)
{
	if (p_dev->config_index == 0)
		return -EINVAL;

	return pcmcia_request_io(p_dev);
}


static int airo_config(struct pcmcia_device *link)
{
	int ret;

	dev_dbg(&link->dev, "airo_config\n");

	link->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_SET_VPP |
		CONF_AUTO_AUDIO | CONF_AUTO_SET_IO;

	ret = pcmcia_loop_config(link, airo_cs_config_check, NULL);
	if (ret)
		goto failed;

	if (!link->irq)
		goto failed;

	ret = pcmcia_enable_device(link);
	if (ret)
		goto failed;
	((struct local_info *)link->priv)->eth_dev =
		init_airo_card(link->irq,
			       link->resource[0]->start, 1, &link->dev);
	if (!((struct local_info *)link->priv)->eth_dev)
		goto failed;

	return 0;

 failed:
	airo_release(link);
	return -ENODEV;
}  

static void airo_release(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "airo_release\n");
	pcmcia_disable_device(link);
}

static int airo_suspend(struct pcmcia_device *link)
{
	struct local_info *local = link->priv;

	netif_device_detach(local->eth_dev);

	return 0;
}

static int airo_resume(struct pcmcia_device *link)
{
	struct local_info *local = link->priv;

	if (link->open) {
		reset_airo_card(local->eth_dev);
		netif_device_attach(local->eth_dev);
	}

	return 0;
}

static const struct pcmcia_device_id airo_ids[] = {
	PCMCIA_DEVICE_MANF_CARD(0x015f, 0x000a),
	PCMCIA_DEVICE_MANF_CARD(0x015f, 0x0005),
	PCMCIA_DEVICE_MANF_CARD(0x015f, 0x0007),
	PCMCIA_DEVICE_MANF_CARD(0x0105, 0x0007),
	PCMCIA_DEVICE_NULL,
};
MODULE_DEVICE_TABLE(pcmcia, airo_ids);

static struct pcmcia_driver airo_driver = {
	.owner		= THIS_MODULE,
	.name		= "airo_cs",
	.probe		= airo_probe,
	.remove		= airo_detach,
	.id_table       = airo_ids,
	.suspend	= airo_suspend,
	.resume		= airo_resume,
};
module_pcmcia_driver(airo_driver);

 
