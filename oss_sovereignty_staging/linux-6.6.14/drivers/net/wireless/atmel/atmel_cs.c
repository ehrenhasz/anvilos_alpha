 

#ifdef __IN_PCMCIA_PACKAGE__
#include <pcmcia/k_compat.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/moduleparam.h>
#include <linux/device.h>

#include <pcmcia/cistpl.h>
#include <pcmcia/cisreg.h>
#include <pcmcia/ds.h>
#include <pcmcia/ciscode.h>

#include <asm/io.h>
#include <linux/wireless.h>

#include "atmel.h"


 

MODULE_AUTHOR("Simon Kelley");
MODULE_DESCRIPTION("Support for Atmel at76c50x 802.11 wireless ethernet cards.");
MODULE_LICENSE("GPL");

 

static int atmel_config(struct pcmcia_device *link);
static void atmel_release(struct pcmcia_device *link);

static void atmel_detach(struct pcmcia_device *p_dev);

struct local_info {
	struct net_device *eth_dev;
};

static int atmel_probe(struct pcmcia_device *p_dev)
{
	struct local_info *local;
	int ret;

	dev_dbg(&p_dev->dev, "atmel_attach()\n");

	 
	local = kzalloc(sizeof(*local), GFP_KERNEL);
	if (!local)
		return -ENOMEM;

	p_dev->priv = local;

	ret = atmel_config(p_dev);
	if (ret)
		goto err_free_priv;

	return 0;

err_free_priv:
	kfree(p_dev->priv);
	return ret;
}

static void atmel_detach(struct pcmcia_device *link)
{
	dev_dbg(&link->dev, "atmel_detach\n");

	atmel_release(link);

	kfree(link->priv);
}

 
static int card_present(void *arg)
{
	struct pcmcia_device *link = (struct pcmcia_device *)arg;

	if (pcmcia_dev_present(link))
		return 1;

	return 0;
}

static int atmel_config_check(struct pcmcia_device *p_dev, void *priv_data)
{
	if (p_dev->config_index == 0)
		return -EINVAL;

	return pcmcia_request_io(p_dev);
}

static int atmel_config(struct pcmcia_device *link)
{
	int ret;
	const struct pcmcia_device_id *did;

	did = dev_get_drvdata(&link->dev);

	dev_dbg(&link->dev, "atmel_config\n");

	link->config_flags |= CONF_ENABLE_IRQ | CONF_AUTO_SET_VPP |
		CONF_AUTO_AUDIO | CONF_AUTO_SET_IO;

	if (pcmcia_loop_config(link, atmel_config_check, NULL))
		goto failed;

	if (!link->irq) {
		dev_err(&link->dev, "atmel: cannot assign IRQ: check that CONFIG_ISA is set in kernel config.");
		goto failed;
	}

	ret = pcmcia_enable_device(link);
	if (ret)
		goto failed;

	((struct local_info *)link->priv)->eth_dev =
		init_atmel_card(link->irq,
				link->resource[0]->start,
				did ? did->driver_info : ATMEL_FW_TYPE_NONE,
				&link->dev,
				card_present,
				link);
	if (!((struct local_info *)link->priv)->eth_dev)
			goto failed;


	return 0;

 failed:
	atmel_release(link);
	return -ENODEV;
}

static void atmel_release(struct pcmcia_device *link)
{
	struct net_device *dev = ((struct local_info *)link->priv)->eth_dev;

	dev_dbg(&link->dev, "atmel_release\n");

	if (dev)
		stop_atmel_card(dev);
	((struct local_info *)link->priv)->eth_dev = NULL;

	pcmcia_disable_device(link);
}

static int atmel_suspend(struct pcmcia_device *link)
{
	struct local_info *local = link->priv;

	netif_device_detach(local->eth_dev);

	return 0;
}

static int atmel_resume(struct pcmcia_device *link)
{
	struct local_info *local = link->priv;

	atmel_open(local->eth_dev);
	netif_device_attach(local->eth_dev);

	return 0;
}

 
 

#define PCMCIA_DEVICE_MANF_CARD_INFO(manf, card, info) { \
	.match_flags = PCMCIA_DEV_ID_MATCH_MANF_ID| \
			PCMCIA_DEV_ID_MATCH_CARD_ID, \
	.manf_id = (manf), \
	.card_id = (card), \
        .driver_info = (kernel_ulong_t)(info), }

#define PCMCIA_DEVICE_PROD_ID12_INFO(v1, v2, vh1, vh2, info) { \
	.match_flags = PCMCIA_DEV_ID_MATCH_PROD_ID1| \
			PCMCIA_DEV_ID_MATCH_PROD_ID2, \
	.prod_id = { (v1), (v2), NULL, NULL }, \
	.prod_id_hash = { (vh1), (vh2), 0, 0 }, \
        .driver_info = (kernel_ulong_t)(info), }

static const struct pcmcia_device_id atmel_ids[] = {
	PCMCIA_DEVICE_MANF_CARD_INFO(0x0101, 0x0620, ATMEL_FW_TYPE_502_3COM),
	PCMCIA_DEVICE_MANF_CARD_INFO(0x0101, 0x0696, ATMEL_FW_TYPE_502_3COM),
	PCMCIA_DEVICE_MANF_CARD_INFO(0x01bf, 0x3302, ATMEL_FW_TYPE_502E),
	PCMCIA_DEVICE_MANF_CARD_INFO(0xd601, 0x0007, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("11WAVE", "11WP611AL-E", 0x9eb2da1f, 0xc9a0d3f9, ATMEL_FW_TYPE_502E),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C502AR", 0xabda4164, 0x41b37e1f, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C502AR_D", 0xabda4164, 0x3675d704, ATMEL_FW_TYPE_502D),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C502AR_E", 0xabda4164, 0x4172e792, ATMEL_FW_TYPE_502E),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C504_R", 0xabda4164, 0x917f3d72, ATMEL_FW_TYPE_504_2958),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C504", 0xabda4164, 0x5040670a, ATMEL_FW_TYPE_504),
	PCMCIA_DEVICE_PROD_ID12_INFO("ATMEL", "AT76C504A", 0xabda4164, 0xe15ed87f, ATMEL_FW_TYPE_504A_2958),
	PCMCIA_DEVICE_PROD_ID12_INFO("BT", "Voyager 1020 Laptop Adapter", 0xae49b86a, 0x1e957cd5, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("CNet", "CNWLC 11Mbps Wireless PC Card V-5", 0xbc477dde, 0x502fae6b, ATMEL_FW_TYPE_502E),
	PCMCIA_DEVICE_PROD_ID12_INFO("IEEE 802.11b", "Wireless LAN PC Card", 0x5b878724, 0x122f1df6, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("IEEE 802.11b", "Wireless LAN Card S", 0x5b878724, 0x5fba533a, ATMEL_FW_TYPE_504_2958),
	PCMCIA_DEVICE_PROD_ID12_INFO("OEM", "11Mbps Wireless LAN PC Card V-3", 0xfea54c90, 0x1c5b0f68, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("SMC", "2632W", 0xc4f8b18b, 0x30f38774, ATMEL_FW_TYPE_502D),
	PCMCIA_DEVICE_PROD_ID12_INFO("SMC", "2632W-V2", 0xc4f8b18b, 0x172d1377, ATMEL_FW_TYPE_502),
	PCMCIA_DEVICE_PROD_ID12_INFO("Wireless", "PC_CARD", 0xa407ecdd, 0x119f6314, ATMEL_FW_TYPE_502D),
	PCMCIA_DEVICE_PROD_ID12_INFO("WLAN", "802.11b PC CARD", 0x575c516c, 0xb1f6dbc4, ATMEL_FW_TYPE_502D),
	PCMCIA_DEVICE_PROD_ID12_INFO("LG", "LW2100N", 0xb474d43a, 0x6b1fec94, ATMEL_FW_TYPE_502E),
	PCMCIA_DEVICE_NULL
};

MODULE_DEVICE_TABLE(pcmcia, atmel_ids);

static struct pcmcia_driver atmel_driver = {
	.owner		= THIS_MODULE,
	.name		= "atmel_cs",
	.probe          = atmel_probe,
	.remove		= atmel_detach,
	.id_table	= atmel_ids,
	.suspend	= atmel_suspend,
	.resume		= atmel_resume,
};
module_pcmcia_driver(atmel_driver);

 
