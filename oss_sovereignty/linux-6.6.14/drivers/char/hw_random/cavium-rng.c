
 

#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>

#define THUNDERX_RNM_ENT_EN     0x1
#define THUNDERX_RNM_RNG_EN     0x2

struct cavium_rng_pf {
	void __iomem *control_status;
};

 
static int cavium_rng_probe(struct pci_dev *pdev,
			const struct pci_device_id *id)
{
	struct	cavium_rng_pf *rng;
	int	iov_err;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	 
	rng->control_status = pcim_iomap(pdev, 0, 0);
	if (!rng->control_status) {
		dev_err(&pdev->dev,
			"Error iomap failed retrieving control_status.\n");
		return -ENOMEM;
	}

	 
	writeq(THUNDERX_RNM_RNG_EN | THUNDERX_RNM_ENT_EN,
		rng->control_status);

	pci_set_drvdata(pdev, rng);

	 
	iov_err = pci_enable_sriov(pdev, 1);
	if (iov_err != 0) {
		 
		writeq(0, rng->control_status);
		dev_err(&pdev->dev,
			"Error initializing RNG virtual function,(%i).\n",
			iov_err);
		return iov_err;
	}

	return 0;
}

 
static void cavium_rng_remove(struct pci_dev *pdev)
{
	struct cavium_rng_pf *rng;

	rng = pci_get_drvdata(pdev);

	 
	pci_disable_sriov(pdev);

	 
	writeq(0, rng->control_status);
}

static const struct pci_device_id cavium_rng_pf_id_table[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, 0xa018), 0, 0, 0},  
	{0,},
};

MODULE_DEVICE_TABLE(pci, cavium_rng_pf_id_table);

static struct pci_driver cavium_rng_pf_driver = {
	.name		= "cavium_rng_pf",
	.id_table	= cavium_rng_pf_id_table,
	.probe		= cavium_rng_probe,
	.remove		= cavium_rng_remove,
};

module_pci_driver(cavium_rng_pf_driver);
MODULE_AUTHOR("Omer Khaliq <okhaliq@caviumnetworks.com>");
MODULE_LICENSE("GPL v2");
