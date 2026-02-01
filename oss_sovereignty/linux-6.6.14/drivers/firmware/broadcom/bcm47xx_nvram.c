
 

#include <linux/io.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mtd/mtd.h>
#include <linux/bcm47xx_nvram.h>

#define NVRAM_MAGIC			0x48534C46	 
#define NVRAM_SPACE			0x10000
#define NVRAM_MAX_GPIO_ENTRIES		32
#define NVRAM_MAX_GPIO_VALUE_LEN	30

#define FLASH_MIN		0x00020000	 

struct nvram_header {
	u32 magic;
	u32 len;
	u32 crc_ver_init;	 
	u32 config_refresh;	 
	u32 config_ncdl;	 
};

static char nvram_buf[NVRAM_SPACE];
static size_t nvram_len;
static const u32 nvram_sizes[] = {0x6000, 0x8000, 0xF000, 0x10000};

 
static bool bcm47xx_nvram_is_valid(void __iomem *nvram)
{
	return ((struct nvram_header *)nvram)->magic == NVRAM_MAGIC;
}

 
static void bcm47xx_nvram_copy(void __iomem *nvram_start, size_t res_size)
{
	struct nvram_header __iomem *header = nvram_start;
	size_t copy_size;

	copy_size = header->len;
	if (copy_size > res_size) {
		pr_err("The nvram size according to the header seems to be bigger than the partition on flash\n");
		copy_size = res_size;
	}
	if (copy_size >= NVRAM_SPACE) {
		pr_err("nvram on flash (%zu bytes) is bigger than the reserved space in memory, will just copy the first %i bytes\n",
		       copy_size, NVRAM_SPACE - 1);
		copy_size = NVRAM_SPACE - 1;
	}

	__ioread32_copy(nvram_buf, nvram_start, DIV_ROUND_UP(copy_size, 4));
	nvram_buf[NVRAM_SPACE - 1] = '\0';
	nvram_len = copy_size;
}

 
static int bcm47xx_nvram_find_and_copy(void __iomem *flash_start, size_t res_size)
{
	size_t flash_size;
	size_t offset;
	int i;

	if (nvram_len) {
		pr_warn("nvram already initialized\n");
		return -EEXIST;
	}

	 

	 
	for (flash_size = FLASH_MIN; flash_size <= res_size; flash_size <<= 1) {
		for (i = 0; i < ARRAY_SIZE(nvram_sizes); i++) {
			offset = flash_size - nvram_sizes[i];
			if (bcm47xx_nvram_is_valid(flash_start + offset))
				goto found;
		}
	}

	 

	offset = 4096;
	if (bcm47xx_nvram_is_valid(flash_start + offset))
		goto found;

	offset = 1024;
	if (bcm47xx_nvram_is_valid(flash_start + offset))
		goto found;

	pr_err("no nvram found\n");
	return -ENXIO;

found:
	bcm47xx_nvram_copy(flash_start + offset, res_size - offset);

	return 0;
}

int bcm47xx_nvram_init_from_iomem(void __iomem *nvram_start, size_t res_size)
{
	if (nvram_len) {
		pr_warn("nvram already initialized\n");
		return -EEXIST;
	}

	if (!bcm47xx_nvram_is_valid(nvram_start)) {
		pr_err("No valid NVRAM found\n");
		return -ENOENT;
	}

	bcm47xx_nvram_copy(nvram_start, res_size);

	return 0;
}
EXPORT_SYMBOL_GPL(bcm47xx_nvram_init_from_iomem);

 
int bcm47xx_nvram_init_from_mem(u32 base, u32 lim)
{
	void __iomem *iobase;
	int err;

	iobase = ioremap(base, lim);
	if (!iobase)
		return -ENOMEM;

	err = bcm47xx_nvram_find_and_copy(iobase, lim);

	iounmap(iobase);

	return err;
}

static int nvram_init(void)
{
#ifdef CONFIG_MTD
	struct mtd_info *mtd;
	struct nvram_header header;
	size_t bytes_read;
	int err;

	mtd = get_mtd_device_nm("nvram");
	if (IS_ERR(mtd))
		return -ENODEV;

	err = mtd_read(mtd, 0, sizeof(header), &bytes_read, (uint8_t *)&header);
	if (!err && header.magic == NVRAM_MAGIC &&
	    header.len > sizeof(header)) {
		nvram_len = header.len;
		if (nvram_len >= NVRAM_SPACE) {
			pr_err("nvram on flash (%zu bytes) is bigger than the reserved space in memory, will just copy the first %i bytes\n",
				nvram_len, NVRAM_SPACE);
			nvram_len = NVRAM_SPACE - 1;
		}

		err = mtd_read(mtd, 0, nvram_len, &nvram_len,
			       (u8 *)nvram_buf);
		return err;
	}
#endif

	return -ENXIO;
}

int bcm47xx_nvram_getenv(const char *name, char *val, size_t val_len)
{
	char *var, *value, *end, *eq;
	int err;

	if (!name)
		return -EINVAL;

	if (!nvram_len) {
		err = nvram_init();
		if (err)
			return err;
	}

	 
	var = &nvram_buf[sizeof(struct nvram_header)];
	end = nvram_buf + sizeof(nvram_buf);
	while (var < end && *var) {
		eq = strchr(var, '=');
		if (!eq)
			break;
		value = eq + 1;
		if (eq - var == strlen(name) &&
		    strncmp(var, name, eq - var) == 0)
			return snprintf(val, val_len, "%s", value);
		var = value + strlen(value) + 1;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(bcm47xx_nvram_getenv);

int bcm47xx_nvram_gpio_pin(const char *name)
{
	int i, err;
	char nvram_var[] = "gpioXX";
	char buf[NVRAM_MAX_GPIO_VALUE_LEN];

	 
	for (i = 0; i < NVRAM_MAX_GPIO_ENTRIES; i++) {
		err = snprintf(nvram_var, sizeof(nvram_var), "gpio%i", i);
		if (err <= 0)
			continue;
		err = bcm47xx_nvram_getenv(nvram_var, buf, sizeof(buf));
		if (err <= 0)
			continue;
		if (!strcmp(name, buf))
			return i;
	}
	return -ENOENT;
}
EXPORT_SYMBOL(bcm47xx_nvram_gpio_pin);

char *bcm47xx_nvram_get_contents(size_t *nvram_size)
{
	int err;
	char *nvram;

	if (!nvram_len) {
		err = nvram_init();
		if (err)
			return NULL;
	}

	*nvram_size = nvram_len - sizeof(struct nvram_header);
	nvram = vmalloc(*nvram_size);
	if (!nvram)
		return NULL;
	memcpy(nvram, &nvram_buf[sizeof(struct nvram_header)], *nvram_size);

	return nvram;
}
EXPORT_SYMBOL(bcm47xx_nvram_get_contents);

