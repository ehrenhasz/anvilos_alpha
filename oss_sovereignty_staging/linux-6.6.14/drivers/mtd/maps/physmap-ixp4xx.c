
 
#include <linux/export.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mtd/map.h>
#include <linux/mtd/xip.h>
#include "physmap-ixp4xx.h"

 
#ifndef CONFIG_CPU_BIG_ENDIAN

static inline u16 flash_read16(void __iomem *addr)
{
	return be16_to_cpu(__raw_readw((void __iomem *)((unsigned long)addr ^ 0x2)));
}

static inline void flash_write16(u16 d, void __iomem *addr)
{
	__raw_writew(cpu_to_be16(d), (void __iomem *)((unsigned long)addr ^ 0x2));
}

#define	BYTE0(h)	((h) & 0xFF)
#define	BYTE1(h)	(((h) >> 8) & 0xFF)

#else

static inline u16 flash_read16(const void __iomem *addr)
{
	return __raw_readw(addr);
}

static inline void flash_write16(u16 d, void __iomem *addr)
{
	__raw_writew(d, addr);
}

#define	BYTE0(h)	(((h) >> 8) & 0xFF)
#define	BYTE1(h)	((h) & 0xFF)
#endif

static map_word ixp4xx_read16(struct map_info *map, unsigned long ofs)
{
	map_word val;

	val.x[0] = flash_read16(map->virt + ofs);
	return val;
}

 
static void ixp4xx_copy_from(struct map_info *map, void *to,
			     unsigned long from, ssize_t len)
{
	u8 *dest = (u8 *) to;
	void __iomem *src = map->virt + from;

	if (len <= 0)
		return;

	if (from & 1) {
		*dest++ = BYTE1(flash_read16(src-1));
		src++;
		--len;
	}

	while (len >= 2) {
		u16 data = flash_read16(src);
		*dest++ = BYTE0(data);
		*dest++ = BYTE1(data);
		src += 2;
		len -= 2;
	}

	if (len > 0)
		*dest++ = BYTE0(flash_read16(src));
}

static void ixp4xx_write16(struct map_info *map, map_word d, unsigned long adr)
{
	flash_write16(d.x[0], map->virt + adr);
}

int of_flash_probe_ixp4xx(struct platform_device *pdev,
			  struct device_node *np,
			  struct map_info *map)
{
	struct device *dev = &pdev->dev;

	 
	if (!of_device_is_compatible(np, "intel,ixp4xx-flash"))
		return 0;

	map->read = ixp4xx_read16;
	map->write = ixp4xx_write16;
	map->copy_from = ixp4xx_copy_from;
	map->copy_to = NULL;

	dev_info(dev, "initialized Intel IXP4xx-specific physmap control\n");

	return 0;
}
