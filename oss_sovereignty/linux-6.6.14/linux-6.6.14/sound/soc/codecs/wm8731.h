#ifndef _WM8731_H
#define _WM8731_H
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
struct clk;
struct snd_pcm_hw_constraint_list;
#define WM8731_LINVOL   0x00
#define WM8731_RINVOL   0x01
#define WM8731_LOUT1V   0x02
#define WM8731_ROUT1V   0x03
#define WM8731_APANA    0x04
#define WM8731_APDIGI   0x05
#define WM8731_PWR      0x06
#define WM8731_IFACE    0x07
#define WM8731_SRATE    0x08
#define WM8731_ACTIVE   0x09
#define WM8731_RESET	0x0f
#define WM8731_CACHEREGNUM 	10
#define WM8731_SYSCLK_MCLK 0
#define WM8731_SYSCLK_XTAL 1
#define WM8731_DAI		0
#define WM8731_NUM_SUPPLIES 4
struct wm8731_priv {
	struct regmap *regmap;
	struct clk *mclk;
	struct regulator_bulk_data supplies[WM8731_NUM_SUPPLIES];
	const struct snd_pcm_hw_constraint_list *constraints;
	unsigned int sysclk;
	int sysclk_type;
	int playback_fs;
	bool deemph;
	struct mutex lock;
};
extern const struct regmap_config wm8731_regmap;
int wm8731_init(struct device *dev, struct wm8731_priv *wm8731);
#endif
