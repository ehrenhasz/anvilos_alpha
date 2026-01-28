#ifndef __LINUX_FBTFT_H
#define __LINUX_FBTFT_H
#include <linux/fb.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/platform_device.h>
#define FBTFT_ONBOARD_BACKLIGHT 2
#define FBTFT_GPIO_NO_MATCH		0xFFFF
#define FBTFT_GPIO_NAME_SIZE	32
#define FBTFT_MAX_INIT_SEQUENCE      512
#define FBTFT_GAMMA_MAX_VALUES_TOTAL 128
#define FBTFT_OF_INIT_CMD	BIT(24)
#define FBTFT_OF_INIT_DELAY	BIT(25)
struct fbtft_gpio {
	char name[FBTFT_GPIO_NAME_SIZE];
	struct gpio_desc *gpio;
};
struct fbtft_par;
struct fbtft_ops {
	int (*write)(struct fbtft_par *par, void *buf, size_t len);
	int (*read)(struct fbtft_par *par, void *buf, size_t len);
	int (*write_vmem)(struct fbtft_par *par, size_t offset, size_t len);
	void (*write_register)(struct fbtft_par *par, int len, ...);
	void (*set_addr_win)(struct fbtft_par *par,
			     int xs, int ys, int xe, int ye);
	void (*reset)(struct fbtft_par *par);
	void (*mkdirty)(struct fb_info *info, int from, int to);
	void (*update_display)(struct fbtft_par *par,
			       unsigned int start_line, unsigned int end_line);
	int (*init_display)(struct fbtft_par *par);
	int (*blank)(struct fbtft_par *par, bool on);
	unsigned long (*request_gpios_match)(struct fbtft_par *par,
					     const struct fbtft_gpio *gpio);
	int (*request_gpios)(struct fbtft_par *par);
	int (*verify_gpios)(struct fbtft_par *par);
	void (*register_backlight)(struct fbtft_par *par);
	void (*unregister_backlight)(struct fbtft_par *par);
	int (*set_var)(struct fbtft_par *par);
	int (*set_gamma)(struct fbtft_par *par, u32 *curves);
};
struct fbtft_display {
	unsigned int width;
	unsigned int height;
	unsigned int regwidth;
	unsigned int buswidth;
	unsigned int backlight;
	struct fbtft_ops fbtftops;
	unsigned int bpp;
	unsigned int fps;
	int txbuflen;
	const s16 *init_sequence;
	char *gamma;
	int gamma_num;
	int gamma_len;
	unsigned long debug;
};
struct fbtft_platform_data {
	struct fbtft_display display;
	unsigned int rotate;
	bool bgr;
	unsigned int fps;
	int txbuflen;
	u8 startbyte;
	char *gamma;
	void *extra;
};
struct fbtft_par {
	struct spi_device *spi;
	struct platform_device *pdev;
	struct fb_info *info;
	struct fbtft_platform_data *pdata;
	u16 *ssbuf;
	u32 pseudo_palette[16];
	struct {
		void *buf;
		size_t len;
	} txbuf;
	u8 *buf;
	u8 startbyte;
	struct fbtft_ops fbtftops;
	spinlock_t dirty_lock;
	unsigned int dirty_lines_start;
	unsigned int dirty_lines_end;
	struct {
		struct gpio_desc *reset;
		struct gpio_desc *dc;
		struct gpio_desc *rd;
		struct gpio_desc *wr;
		struct gpio_desc *latch;
		struct gpio_desc *cs;
		struct gpio_desc *db[16];
		struct gpio_desc *led[16];
		struct gpio_desc *aux[16];
	} gpio;
	const s16 *init_sequence;
	struct {
		struct mutex lock;
		u32 *curves;
		int num_values;
		int num_curves;
	} gamma;
	unsigned long debug;
	bool first_update_done;
	ktime_t update_time;
	bool bgr;
	void *extra;
	bool polarity;
};
#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__}) / sizeof(int))
#define write_reg(par, ...)                                            \
	((par)->fbtftops.write_register(par, NUMARGS(__VA_ARGS__), __VA_ARGS__))
int fbtft_write_buf_dc(struct fbtft_par *par, void *buf, size_t len, int dc);
__printf(5, 6)
void fbtft_dbg_hex(const struct device *dev, int groupsize,
		   const void *buf, size_t len, const char *fmt, ...);
struct fb_info *fbtft_framebuffer_alloc(struct fbtft_display *display,
					struct device *dev,
					struct fbtft_platform_data *pdata);
void fbtft_framebuffer_release(struct fb_info *info);
int fbtft_register_framebuffer(struct fb_info *fb_info);
int fbtft_unregister_framebuffer(struct fb_info *fb_info);
void fbtft_register_backlight(struct fbtft_par *par);
void fbtft_unregister_backlight(struct fbtft_par *par);
int fbtft_init_display(struct fbtft_par *par);
int fbtft_probe_common(struct fbtft_display *display, struct spi_device *sdev,
		       struct platform_device *pdev);
void fbtft_remove_common(struct device *dev, struct fb_info *info);
int fbtft_write_spi(struct fbtft_par *par, void *buf, size_t len);
int fbtft_write_spi_emulate_9(struct fbtft_par *par, void *buf, size_t len);
int fbtft_read_spi(struct fbtft_par *par, void *buf, size_t len);
int fbtft_write_gpio8_wr(struct fbtft_par *par, void *buf, size_t len);
int fbtft_write_gpio16_wr(struct fbtft_par *par, void *buf, size_t len);
int fbtft_write_gpio16_wr_latched(struct fbtft_par *par, void *buf, size_t len);
int fbtft_write_vmem8_bus8(struct fbtft_par *par, size_t offset, size_t len);
int fbtft_write_vmem16_bus16(struct fbtft_par *par, size_t offset, size_t len);
int fbtft_write_vmem16_bus8(struct fbtft_par *par, size_t offset, size_t len);
int fbtft_write_vmem16_bus9(struct fbtft_par *par, size_t offset, size_t len);
void fbtft_write_reg8_bus8(struct fbtft_par *par, int len, ...);
void fbtft_write_reg8_bus9(struct fbtft_par *par, int len, ...);
void fbtft_write_reg16_bus8(struct fbtft_par *par, int len, ...);
void fbtft_write_reg16_bus16(struct fbtft_par *par, int len, ...);
#define FBTFT_DT_TABLE(_compatible)						\
static const struct of_device_id dt_ids[] = {					\
	{ .compatible = _compatible },						\
	{},									\
};										\
MODULE_DEVICE_TABLE(of, dt_ids);
#define FBTFT_SPI_DRIVER(_name, _compatible, _display, _spi_ids)		\
										\
static int fbtft_driver_probe_spi(struct spi_device *spi)			\
{										\
	return fbtft_probe_common(_display, spi, NULL);				\
}										\
										\
static void fbtft_driver_remove_spi(struct spi_device *spi)			\
{										\
	struct fb_info *info = spi_get_drvdata(spi);				\
										\
	fbtft_remove_common(&spi->dev, info);					\
}										\
										\
static struct spi_driver fbtft_driver_spi_driver = {				\
	.driver = {								\
		.name = _name,							\
		.of_match_table = dt_ids,					\
	},									\
	.id_table = _spi_ids,							\
	.probe = fbtft_driver_probe_spi,					\
	.remove = fbtft_driver_remove_spi,					\
};
#define FBTFT_REGISTER_DRIVER(_name, _compatible, _display)                \
									   \
static int fbtft_driver_probe_pdev(struct platform_device *pdev)           \
{                                                                          \
	return fbtft_probe_common(_display, NULL, pdev);                   \
}                                                                          \
									   \
static int fbtft_driver_remove_pdev(struct platform_device *pdev)          \
{                                                                          \
	struct fb_info *info = platform_get_drvdata(pdev);                 \
									   \
	fbtft_remove_common(&pdev->dev, info);                             \
	return 0;                                                          \
}                                                                          \
									   \
FBTFT_DT_TABLE(_compatible)						   \
									   \
FBTFT_SPI_DRIVER(_name, _compatible, _display, NULL)			   \
									   \
static struct platform_driver fbtft_driver_platform_driver = {             \
	.driver = {                                                        \
		.name   = _name,                                           \
		.owner  = THIS_MODULE,                                     \
		.of_match_table = dt_ids,                                  \
	},                                                                 \
	.probe  = fbtft_driver_probe_pdev,                                 \
	.remove = fbtft_driver_remove_pdev,                                \
};                                                                         \
									   \
static int __init fbtft_driver_module_init(void)                           \
{                                                                          \
	int ret;                                                           \
									   \
	ret = spi_register_driver(&fbtft_driver_spi_driver);               \
	if (ret < 0)                                                       \
		return ret;                                                \
	ret = platform_driver_register(&fbtft_driver_platform_driver);     \
	if (ret < 0)                                                       \
		spi_unregister_driver(&fbtft_driver_spi_driver);           \
	return ret;                                                        \
}                                                                          \
									   \
static void __exit fbtft_driver_module_exit(void)                          \
{                                                                          \
	spi_unregister_driver(&fbtft_driver_spi_driver);                   \
	platform_driver_unregister(&fbtft_driver_platform_driver);         \
}                                                                          \
									   \
module_init(fbtft_driver_module_init);                                     \
module_exit(fbtft_driver_module_exit);
#define FBTFT_REGISTER_SPI_DRIVER(_name, _comp_vend, _comp_dev, _display)	\
										\
FBTFT_DT_TABLE(_comp_vend "," _comp_dev)					\
										\
static const struct spi_device_id spi_ids[] = {					\
	{ .name = _comp_dev },							\
	{},									\
};										\
MODULE_DEVICE_TABLE(spi, spi_ids);						\
										\
FBTFT_SPI_DRIVER(_name, _comp_vend "," _comp_dev, _display, spi_ids)		\
										\
module_spi_driver(fbtft_driver_spi_driver);
#define DEBUG_LEVEL_1	DEBUG_REQUEST_GPIOS
#define DEBUG_LEVEL_2	(DEBUG_LEVEL_1 | DEBUG_DRIVER_INIT_FUNCTIONS        \
				       | DEBUG_TIME_FIRST_UPDATE)
#define DEBUG_LEVEL_3	(DEBUG_LEVEL_2 | DEBUG_RESET | DEBUG_INIT_DISPLAY   \
				       | DEBUG_BLANK | DEBUG_REQUEST_GPIOS  \
				       | DEBUG_FREE_GPIOS                   \
				       | DEBUG_VERIFY_GPIOS                 \
				       | DEBUG_BACKLIGHT | DEBUG_SYSFS)
#define DEBUG_LEVEL_4	(DEBUG_LEVEL_2 | DEBUG_FB_READ | DEBUG_FB_WRITE     \
				       | DEBUG_FB_FILLRECT                  \
				       | DEBUG_FB_COPYAREA                  \
				       | DEBUG_FB_IMAGEBLIT | DEBUG_FB_BLANK)
#define DEBUG_LEVEL_5	(DEBUG_LEVEL_3 | DEBUG_UPDATE_DISPLAY)
#define DEBUG_LEVEL_6	(DEBUG_LEVEL_4 | DEBUG_LEVEL_5)
#define DEBUG_LEVEL_7	0xFFFFFFFF
#define DEBUG_DRIVER_INIT_FUNCTIONS BIT(3)
#define DEBUG_TIME_FIRST_UPDATE     BIT(4)
#define DEBUG_TIME_EACH_UPDATE      BIT(5)
#define DEBUG_DEFERRED_IO           BIT(6)
#define DEBUG_FBTFT_INIT_FUNCTIONS  BIT(7)
#define DEBUG_FB_READ               BIT(8)
#define DEBUG_FB_WRITE              BIT(9)
#define DEBUG_FB_FILLRECT           BIT(10)
#define DEBUG_FB_COPYAREA           BIT(11)
#define DEBUG_FB_IMAGEBLIT          BIT(12)
#define DEBUG_FB_SETCOLREG          BIT(13)
#define DEBUG_FB_BLANK              BIT(14)
#define DEBUG_SYSFS                 BIT(16)
#define DEBUG_BACKLIGHT             BIT(17)
#define DEBUG_READ                  BIT(18)
#define DEBUG_WRITE                 BIT(19)
#define DEBUG_WRITE_VMEM            BIT(20)
#define DEBUG_WRITE_REGISTER        BIT(21)
#define DEBUG_SET_ADDR_WIN          BIT(22)
#define DEBUG_RESET                 BIT(23)
#define DEBUG_MKDIRTY               BIT(24)
#define DEBUG_UPDATE_DISPLAY        BIT(25)
#define DEBUG_INIT_DISPLAY          BIT(26)
#define DEBUG_BLANK                 BIT(27)
#define DEBUG_REQUEST_GPIOS         BIT(28)
#define DEBUG_FREE_GPIOS            BIT(29)
#define DEBUG_REQUEST_GPIOS_MATCH   BIT(30)
#define DEBUG_VERIFY_GPIOS          BIT(31)
#define fbtft_init_dbg(dev, format, arg...)                  \
do {                                                         \
	if (unlikely((dev)->platform_data &&                 \
	    (((struct fbtft_platform_data *)(dev)->platform_data)->display.debug & DEBUG_DRIVER_INIT_FUNCTIONS))) \
		dev_info(dev, format, ##arg);                \
} while (0)
#define fbtft_par_dbg(level, par, format, arg...)            \
do {                                                         \
	if (unlikely((par)->debug & (level)))                    \
		dev_info((par)->info->device, format, ##arg);  \
} while (0)
#define fbtft_par_dbg_hex(level, par, dev, type, buf, num, format, arg...) \
do {                                                                       \
	if (unlikely((par)->debug & (level)))                                  \
		fbtft_dbg_hex(dev, sizeof(type), buf,\
			      (num) * sizeof(type), format, ##arg); \
} while (0)
#endif  
