
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_address.h>
#include "qcom_pil_info.h"

 
#define PIL_RELOC_NAME_LEN	8
#define PIL_RELOC_ENTRY_SIZE	(PIL_RELOC_NAME_LEN + sizeof(__le64) + sizeof(__le32))

struct pil_reloc {
	void __iomem *base;
	size_t num_entries;
};

static struct pil_reloc _reloc __read_mostly;
static DEFINE_MUTEX(pil_reloc_lock);

static int qcom_pil_info_init(void)
{
	struct device_node *np;
	struct resource imem;
	void __iomem *base;
	int ret;

	 
	if (_reloc.base)
		return 0;

	np = of_find_compatible_node(NULL, NULL, "qcom,pil-reloc-info");
	if (!np)
		return -ENOENT;

	ret = of_address_to_resource(np, 0, &imem);
	of_node_put(np);
	if (ret < 0)
		return ret;

	base = ioremap(imem.start, resource_size(&imem));
	if (!base) {
		pr_err("failed to map PIL relocation info region\n");
		return -ENOMEM;
	}

	memset_io(base, 0, resource_size(&imem));

	_reloc.base = base;
	_reloc.num_entries = (u32)resource_size(&imem) / PIL_RELOC_ENTRY_SIZE;

	return 0;
}

 
int qcom_pil_info_store(const char *image, phys_addr_t base, size_t size)
{
	char buf[PIL_RELOC_NAME_LEN];
	void __iomem *entry;
	int ret;
	int i;

	mutex_lock(&pil_reloc_lock);
	ret = qcom_pil_info_init();
	if (ret < 0) {
		mutex_unlock(&pil_reloc_lock);
		return ret;
	}

	for (i = 0; i < _reloc.num_entries; i++) {
		entry = _reloc.base + i * PIL_RELOC_ENTRY_SIZE;

		memcpy_fromio(buf, entry, PIL_RELOC_NAME_LEN);

		 
		if (!buf[0])
			goto found_unused;

		if (!strncmp(buf, image, PIL_RELOC_NAME_LEN))
			goto found_existing;
	}

	pr_warn("insufficient PIL info slots\n");
	mutex_unlock(&pil_reloc_lock);
	return -ENOMEM;

found_unused:
	memcpy_toio(entry, image, strnlen(image, PIL_RELOC_NAME_LEN));
found_existing:
	 
	writel(base, entry + PIL_RELOC_NAME_LEN);
	writel((u64)base >> 32, entry + PIL_RELOC_NAME_LEN + 4);
	writel(size, entry + PIL_RELOC_NAME_LEN + sizeof(__le64));
	mutex_unlock(&pil_reloc_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_pil_info_store);

static void __exit pil_reloc_exit(void)
{
	mutex_lock(&pil_reloc_lock);
	iounmap(_reloc.base);
	_reloc.base = NULL;
	mutex_unlock(&pil_reloc_lock);
}
module_exit(pil_reloc_exit);

MODULE_DESCRIPTION("Qualcomm PIL relocation info");
MODULE_LICENSE("GPL v2");
