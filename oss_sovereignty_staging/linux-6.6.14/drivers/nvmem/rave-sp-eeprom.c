

 
#include <linux/kernel.h>
#include <linux/mfd/rave-sp.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>

 
enum rave_sp_eeprom_access_type {
	RAVE_SP_EEPROM_WRITE = 0,
	RAVE_SP_EEPROM_READ  = 1,
};

 
enum rave_sp_eeprom_header_size {
	RAVE_SP_EEPROM_HEADER_SMALL = 4U,
	RAVE_SP_EEPROM_HEADER_BIG   = 5U,
};
#define RAVE_SP_EEPROM_HEADER_MAX	RAVE_SP_EEPROM_HEADER_BIG

#define	RAVE_SP_EEPROM_PAGE_SIZE	32U

 
struct rave_sp_eeprom_page {
	u8  type;
	u8  success;
	u8  data[RAVE_SP_EEPROM_PAGE_SIZE];
} __packed;

 
struct rave_sp_eeprom {
	struct rave_sp *sp;
	struct mutex mutex;
	u8 address;
	unsigned int header_size;
	struct device *dev;
};

 
static int rave_sp_eeprom_io(struct rave_sp_eeprom *eeprom,
			     enum rave_sp_eeprom_access_type type,
			     u16 idx,
			     struct rave_sp_eeprom_page *page)
{
	const bool is_write = type == RAVE_SP_EEPROM_WRITE;
	const unsigned int data_size = is_write ? sizeof(page->data) : 0;
	const unsigned int cmd_size = eeprom->header_size + data_size;
	const unsigned int rsp_size =
		is_write ? sizeof(*page) - sizeof(page->data) : sizeof(*page);
	unsigned int offset = 0;
	u8 cmd[RAVE_SP_EEPROM_HEADER_MAX + sizeof(page->data)];
	int ret;

	if (WARN_ON(cmd_size > sizeof(cmd)))
		return -EINVAL;

	cmd[offset++] = eeprom->address;
	cmd[offset++] = 0;
	cmd[offset++] = type;
	cmd[offset++] = idx;

	 
	if (offset < eeprom->header_size)
		cmd[offset++] = idx >> 8;
	 
	memcpy(&cmd[offset], page->data, data_size);

	ret = rave_sp_exec(eeprom->sp, cmd, cmd_size, page, rsp_size);
	if (ret)
		return ret;

	if (page->type != type)
		return -EPROTO;

	if (!page->success)
		return -EIO;

	return 0;
}

 
static int
rave_sp_eeprom_page_access(struct rave_sp_eeprom *eeprom,
			   enum rave_sp_eeprom_access_type type,
			   unsigned int offset, u8 *data,
			   size_t data_len)
{
	const unsigned int page_offset = offset % RAVE_SP_EEPROM_PAGE_SIZE;
	const unsigned int page_nr     = offset / RAVE_SP_EEPROM_PAGE_SIZE;
	struct rave_sp_eeprom_page page;
	int ret;

	 
	if (WARN_ON(data_len > sizeof(page.data) - page_offset))
		return -EINVAL;

	if (type == RAVE_SP_EEPROM_WRITE) {
		 
		if (data_len < RAVE_SP_EEPROM_PAGE_SIZE) {
			ret = rave_sp_eeprom_io(eeprom, RAVE_SP_EEPROM_READ,
						page_nr, &page);
			if (ret)
				return ret;
		}

		memcpy(&page.data[page_offset], data, data_len);
	}

	ret = rave_sp_eeprom_io(eeprom, type, page_nr, &page);
	if (ret)
		return ret;

	 
	if (type == RAVE_SP_EEPROM_READ)
		memcpy(data, &page.data[page_offset], data_len);

	return 0;
}

 
static int rave_sp_eeprom_access(struct rave_sp_eeprom *eeprom,
				 enum rave_sp_eeprom_access_type type,
				 unsigned int offset, u8 *data,
				 unsigned int data_len)
{
	unsigned int residue;
	unsigned int chunk;
	unsigned int head;
	int ret;

	mutex_lock(&eeprom->mutex);

	head    = offset % RAVE_SP_EEPROM_PAGE_SIZE;
	residue = data_len;

	do {
		 
		if (unlikely(head)) {
			chunk = RAVE_SP_EEPROM_PAGE_SIZE - head;
			 
			head  = 0;
		} else {
			chunk = RAVE_SP_EEPROM_PAGE_SIZE;
		}

		 
		chunk = min(chunk, residue);
		ret = rave_sp_eeprom_page_access(eeprom, type, offset,
						 data, chunk);
		if (ret)
			goto out;

		residue -= chunk;
		offset  += chunk;
		data    += chunk;
	} while (residue);
out:
	mutex_unlock(&eeprom->mutex);
	return ret;
}

static int rave_sp_eeprom_reg_read(void *eeprom, unsigned int offset,
				   void *val, size_t bytes)
{
	return rave_sp_eeprom_access(eeprom, RAVE_SP_EEPROM_READ,
				     offset, val, bytes);
}

static int rave_sp_eeprom_reg_write(void *eeprom, unsigned int offset,
				    void *val, size_t bytes)
{
	return rave_sp_eeprom_access(eeprom, RAVE_SP_EEPROM_WRITE,
				     offset, val, bytes);
}

static int rave_sp_eeprom_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rave_sp *sp = dev_get_drvdata(dev->parent);
	struct device_node *np = dev->of_node;
	struct nvmem_config config = { 0 };
	struct rave_sp_eeprom *eeprom;
	struct nvmem_device *nvmem;
	u32 reg[2], size;

	if (of_property_read_u32_array(np, "reg", reg, ARRAY_SIZE(reg))) {
		dev_err(dev, "Failed to parse \"reg\" property\n");
		return -EINVAL;
	}

	size = reg[1];
	 
	if (size > U16_MAX * RAVE_SP_EEPROM_PAGE_SIZE) {
		dev_err(dev, "Specified size is too big\n");
		return -EINVAL;
	}

	eeprom = devm_kzalloc(dev, sizeof(*eeprom), GFP_KERNEL);
	if (!eeprom)
		return -ENOMEM;

	eeprom->address = reg[0];
	eeprom->sp      = sp;
	eeprom->dev     = dev;

	if (size > SZ_8K)
		eeprom->header_size = RAVE_SP_EEPROM_HEADER_BIG;
	else
		eeprom->header_size = RAVE_SP_EEPROM_HEADER_SMALL;

	mutex_init(&eeprom->mutex);

	config.id		= -1;
	of_property_read_string(np, "zii,eeprom-name", &config.name);
	config.priv		= eeprom;
	config.dev		= dev;
	config.size		= size;
	config.reg_read		= rave_sp_eeprom_reg_read;
	config.reg_write	= rave_sp_eeprom_reg_write;
	config.word_size	= 1;
	config.stride		= 1;

	nvmem = devm_nvmem_register(dev, &config);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id rave_sp_eeprom_of_match[] = {
	{ .compatible = "zii,rave-sp-eeprom" },
	{}
};
MODULE_DEVICE_TABLE(of, rave_sp_eeprom_of_match);

static struct platform_driver rave_sp_eeprom_driver = {
	.probe = rave_sp_eeprom_probe,
	.driver	= {
		.name = KBUILD_MODNAME,
		.of_match_table = rave_sp_eeprom_of_match,
	},
};
module_platform_driver(rave_sp_eeprom_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey Vostrikov <andrey.vostrikov@cogentembedded.com>");
MODULE_AUTHOR("Nikita Yushchenko <nikita.yoush@cogentembedded.com>");
MODULE_AUTHOR("Andrey Smirnov <andrew.smirnov@gmail.com>");
MODULE_DESCRIPTION("RAVE SP EEPROM driver");
