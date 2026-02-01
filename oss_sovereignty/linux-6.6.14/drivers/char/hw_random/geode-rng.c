 

#include <linux/delay.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>


#define PFX	KBUILD_MODNAME ": "

#define GEODE_RNG_DATA_REG   0x50
#define GEODE_RNG_STATUS_REG 0x54

 
static const struct pci_device_id pci_tbl[] = {
	{ PCI_VDEVICE(AMD, PCI_DEVICE_ID_AMD_LX_AES), 0, },
	{ 0, },	 
};
MODULE_DEVICE_TABLE(pci, pci_tbl);

struct amd_geode_priv {
	struct pci_dev *pcidev;
	void __iomem *membase;
};

static int geode_rng_data_read(struct hwrng *rng, u32 *data)
{
	struct amd_geode_priv *priv = (struct amd_geode_priv *)rng->priv;
	void __iomem *mem = priv->membase;

	*data = readl(mem + GEODE_RNG_DATA_REG);

	return 4;
}

static int geode_rng_data_present(struct hwrng *rng, int wait)
{
	struct amd_geode_priv *priv = (struct amd_geode_priv *)rng->priv;
	void __iomem *mem = priv->membase;
	int data, i;

	for (i = 0; i < 20; i++) {
		data = !!(readl(mem + GEODE_RNG_STATUS_REG));
		if (data || !wait)
			break;
		udelay(10);
	}
	return data;
}


static struct hwrng geode_rng = {
	.name		= "geode",
	.data_present	= geode_rng_data_present,
	.data_read	= geode_rng_data_read,
};


static int __init geode_rng_init(void)
{
	int err = -ENODEV;
	struct pci_dev *pdev = NULL;
	const struct pci_device_id *ent;
	void __iomem *mem;
	unsigned long rng_base;
	struct amd_geode_priv *priv;

	for_each_pci_dev(pdev) {
		ent = pci_match_id(pci_tbl, pdev);
		if (ent)
			goto found;
	}
	 
	return err;

found:
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		err = -ENOMEM;
		goto put_dev;
	}

	rng_base = pci_resource_start(pdev, 0);
	if (rng_base == 0)
		goto free_priv;
	err = -ENOMEM;
	mem = ioremap(rng_base, 0x58);
	if (!mem)
		goto free_priv;

	geode_rng.priv = (unsigned long)priv;
	priv->membase = mem;
	priv->pcidev = pdev;

	pr_info("AMD Geode RNG detected\n");
	err = hwrng_register(&geode_rng);
	if (err) {
		pr_err(PFX "RNG registering failed (%d)\n",
		       err);
		goto err_unmap;
	}
	return err;

err_unmap:
	iounmap(mem);
free_priv:
	kfree(priv);
put_dev:
	pci_dev_put(pdev);
	return err;
}

static void __exit geode_rng_exit(void)
{
	struct amd_geode_priv *priv;

	priv = (struct amd_geode_priv *)geode_rng.priv;
	hwrng_unregister(&geode_rng);
	iounmap(priv->membase);
	pci_dev_put(priv->pcidev);
	kfree(priv);
}

module_init(geode_rng_init);
module_exit(geode_rng_exit);

MODULE_DESCRIPTION("H/W RNG driver for AMD Geode LX CPUs");
MODULE_LICENSE("GPL");
