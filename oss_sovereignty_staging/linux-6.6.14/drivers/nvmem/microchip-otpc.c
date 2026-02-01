
 

#include <linux/bitfield.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define MCHP_OTPC_CR			(0x0)
#define MCHP_OTPC_CR_READ		BIT(6)
#define MCHP_OTPC_MR			(0x4)
#define MCHP_OTPC_MR_ADDR		GENMASK(31, 16)
#define MCHP_OTPC_AR			(0x8)
#define MCHP_OTPC_SR			(0xc)
#define MCHP_OTPC_SR_READ		BIT(6)
#define MCHP_OTPC_HR			(0x20)
#define MCHP_OTPC_HR_SIZE		GENMASK(15, 8)
#define MCHP_OTPC_DR			(0x24)

#define MCHP_OTPC_NAME			"mchp-otpc"
#define MCHP_OTPC_SIZE			(11 * 1024)

 
struct mchp_otpc {
	void __iomem *base;
	struct device *dev;
	struct list_head packets;
	u32 npackets;
};

 
struct mchp_otpc_packet {
	struct list_head list;
	u32 id;
	u32 offset;
};

static struct mchp_otpc_packet *mchp_otpc_id_to_packet(struct mchp_otpc *otpc,
						       u32 id)
{
	struct mchp_otpc_packet *packet;

	if (id >= otpc->npackets)
		return NULL;

	list_for_each_entry(packet, &otpc->packets, list) {
		if (packet->id == id)
			return packet;
	}

	return NULL;
}

static int mchp_otpc_prepare_read(struct mchp_otpc *otpc,
				  unsigned int offset)
{
	u32 tmp;

	 
	tmp = readl_relaxed(otpc->base + MCHP_OTPC_MR);
	tmp &= ~MCHP_OTPC_MR_ADDR;
	tmp |= FIELD_PREP(MCHP_OTPC_MR_ADDR, offset);
	writel_relaxed(tmp, otpc->base + MCHP_OTPC_MR);

	 
	tmp = readl_relaxed(otpc->base + MCHP_OTPC_CR);
	tmp |= MCHP_OTPC_CR_READ;
	writel_relaxed(tmp, otpc->base + MCHP_OTPC_CR);

	 
	return read_poll_timeout(readl_relaxed, tmp, !(tmp & MCHP_OTPC_SR_READ),
				 10000, 2000, false, otpc->base + MCHP_OTPC_SR);
}

 
static int mchp_otpc_read(void *priv, unsigned int off, void *val,
			  size_t bytes)
{
	struct mchp_otpc *otpc = priv;
	struct mchp_otpc_packet *packet;
	u32 *buf = val;
	u32 offset;
	size_t len = 0;
	int ret, payload_size;

	 
	packet = mchp_otpc_id_to_packet(otpc, off / 4);
	if (!packet)
		return -EINVAL;
	offset = packet->offset;

	while (len < bytes) {
		ret = mchp_otpc_prepare_read(otpc, offset);
		if (ret)
			return ret;

		 
		*buf++ = readl_relaxed(otpc->base + MCHP_OTPC_HR);
		len += sizeof(*buf);
		offset++;
		if (len >= bytes)
			break;

		 
		payload_size = FIELD_GET(MCHP_OTPC_HR_SIZE, *(buf - 1));
		writel_relaxed(0UL, otpc->base + MCHP_OTPC_AR);
		do {
			*buf++ = readl_relaxed(otpc->base + MCHP_OTPC_DR);
			len += sizeof(*buf);
			offset++;
			payload_size--;
		} while (payload_size >= 0 && len < bytes);
	}

	return 0;
}

static int mchp_otpc_init_packets_list(struct mchp_otpc *otpc, u32 *size)
{
	struct mchp_otpc_packet *packet;
	u32 word, word_pos = 0, id = 0, npackets = 0, payload_size;
	int ret;

	INIT_LIST_HEAD(&otpc->packets);
	*size = 0;

	while (*size < MCHP_OTPC_SIZE) {
		ret = mchp_otpc_prepare_read(otpc, word_pos);
		if (ret)
			return ret;

		word = readl_relaxed(otpc->base + MCHP_OTPC_HR);
		payload_size = FIELD_GET(MCHP_OTPC_HR_SIZE, word);
		if (!payload_size)
			break;

		packet = devm_kzalloc(otpc->dev, sizeof(*packet), GFP_KERNEL);
		if (!packet)
			return -ENOMEM;

		packet->id = id++;
		packet->offset = word_pos;
		INIT_LIST_HEAD(&packet->list);
		list_add_tail(&packet->list, &otpc->packets);

		 
		*size += 4 * (payload_size + 1);
		 
		word_pos += payload_size + 2;

		npackets++;
	}

	otpc->npackets = npackets;

	return 0;
}

static struct nvmem_config mchp_nvmem_config = {
	.name = MCHP_OTPC_NAME,
	.type = NVMEM_TYPE_OTP,
	.read_only = true,
	.word_size = 4,
	.stride = 4,
	.reg_read = mchp_otpc_read,
};

static int mchp_otpc_probe(struct platform_device *pdev)
{
	struct nvmem_device *nvmem;
	struct mchp_otpc *otpc;
	u32 size;
	int ret;

	otpc = devm_kzalloc(&pdev->dev, sizeof(*otpc), GFP_KERNEL);
	if (!otpc)
		return -ENOMEM;

	otpc->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(otpc->base))
		return PTR_ERR(otpc->base);

	otpc->dev = &pdev->dev;
	ret = mchp_otpc_init_packets_list(otpc, &size);
	if (ret)
		return ret;

	mchp_nvmem_config.dev = otpc->dev;
	mchp_nvmem_config.size = size;
	mchp_nvmem_config.priv = otpc;
	nvmem = devm_nvmem_register(&pdev->dev, &mchp_nvmem_config);

	return PTR_ERR_OR_ZERO(nvmem);
}

static const struct of_device_id __maybe_unused mchp_otpc_ids[] = {
	{ .compatible = "microchip,sama7g5-otpc", },
	{ },
};
MODULE_DEVICE_TABLE(of, mchp_otpc_ids);

static struct platform_driver mchp_otpc_driver = {
	.probe = mchp_otpc_probe,
	.driver = {
		.name = MCHP_OTPC_NAME,
		.of_match_table = of_match_ptr(mchp_otpc_ids),
	},
};
module_platform_driver(mchp_otpc_driver);

MODULE_AUTHOR("Claudiu Beznea <claudiu.beznea@microchip.com>");
MODULE_DESCRIPTION("Microchip SAMA7G5 OTPC driver");
MODULE_LICENSE("GPL");
