





#include <linux/module.h>

#include "aio.h"

static const struct uniphier_aio_spec uniphier_aio_ld11[] = {
	 
	{
		.name = AUD_NAME_PCMIN1,
		.gname = AUD_GNAME_HDMI,
		.swm = {
			.type  = PORT_TYPE_I2S,
			.dir   = PORT_DIR_INPUT,
			.rb    = { 21, 14, },
			.ch    = { 21, 14, },
			.iif   = { 5, 3, },
			.iport = { 0, AUD_HW_PCMIN1, },
		},
	},

	 
	{
		.name = AUD_NAME_PCMIN2,
		.swm = {
			.type  = PORT_TYPE_I2S,
			.dir   = PORT_DIR_INPUT,
			.rb    = { 22, 15, },
			.ch    = { 22, 15, },
			.iif   = { 6, 4, },
			.iport = { 1, AUD_HW_PCMIN2, },
		},
	},

	 
	{
		.name = AUD_NAME_PCMIN3,
		.gname = AUD_GNAME_LINE,
		.swm = {
			.type  = PORT_TYPE_EVE,
			.dir   = PORT_DIR_INPUT,
			.rb    = { 23, 16, },
			.ch    = { 23, 16, },
			.iif   = { 7, 5, },
			.iport = { 2, AUD_HW_PCMIN3, },
		},
	},

	 
	{
		.name = AUD_NAME_IECIN1,
		.gname = AUD_GNAME_IEC,
		.swm = {
			.type  = PORT_TYPE_SPDIF,
			.dir   = PORT_DIR_INPUT,
			.rb    = { 26, 17, },
			.ch    = { 26, 17, },
			.iif   = { 10, 6, },
			.iport = { 3, AUD_HW_IECIN1, },
		},
	},

	 
	{
		.name = AUD_NAME_HPCMOUT1,
		.swm = {
			.type  = PORT_TYPE_I2S,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 0, 0, },
			.ch    = { 0, 0, },
			.oif   = { 0, 0, },
			.oport = { 0, AUD_HW_HPCMOUT1, },
		},
	},

	 
	{
		.name = AUD_NAME_PCMOUT1,
		.gname = AUD_GNAME_HDMI,
		.swm = {
			.type  = PORT_TYPE_I2S,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 0, 0, },
			.ch    = { 0, 0, },
			.oif   = { 0, 0, },
			.oport = { 3, AUD_HW_PCMOUT1, },
		},
	},

	 
	{
		.name = AUD_NAME_PCMOUT2,
		.gname = AUD_GNAME_LINE,
		.swm = {
			.type  = PORT_TYPE_EVE,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 2, 2, },
			.ch    = { 2, 2, },
			.oif   = { 2, 2, },
			.oport = { 1, AUD_HW_PCMOUT2, },
		},
	},

	 
	{
		.name = AUD_NAME_PCMOUT3,
		.swm = {
			.type  = PORT_TYPE_EVE,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 3, 3, },
			.ch    = { 3, 3, },
			.oif   = { 3, 3, },
			.oport = { 2, AUD_HW_PCMOUT3, },
		},
	},

	 
	{
		.name = AUD_NAME_EPCMOUT2,
		.swm = {
			.type  = PORT_TYPE_CONV,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 7, 5, },
			.ch    = { 7, 5, },
			.oif   = { 7, 5, },
			.oport = { 6, AUD_HW_EPCMOUT2, },
			.och   = { 17, 12, },
			.iif   = { 1, 1, },
		},
	},

	 
	{
		.name = AUD_NAME_EPCMOUT3,
		.swm = {
			.type  = PORT_TYPE_CONV,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 8, 6, },
			.ch    = { 8, 6, },
			.oif   = { 8, 6, },
			.oport = { 7, AUD_HW_EPCMOUT3, },
			.och   = { 18, 13, },
			.iif   = { 2, 2, },
		},
	},

	 
	{
		.name = AUD_NAME_HIECOUT1,
		.gname = AUD_GNAME_IEC,
		.swm = {
			.type  = PORT_TYPE_SPDIF,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 1, 1, },
			.ch    = { 1, 1, },
			.oif   = { 1, 1, },
			.oport = { 12, AUD_HW_HIECOUT1, },
		},
	},

	 
	{
		.name = AUD_NAME_HIECCOMPOUT1,
		.gname = AUD_GNAME_IEC,
		.swm = {
			.type  = PORT_TYPE_SPDIF,
			.dir   = PORT_DIR_OUTPUT,
			.rb    = { 1, 1, },
			.ch    = { 1, 1, },
			.oif   = { 1, 1, },
			.oport = { 12, AUD_HW_HIECOUT1, },
		},
	},
};

static const struct uniphier_aio_pll uniphier_aio_pll_ld11[] = {
	[AUD_PLL_A1]   = { .enable = true, },
	[AUD_PLL_F1]   = { .enable = true, },
	[AUD_PLL_A2]   = { .enable = true, },
	[AUD_PLL_F2]   = { .enable = true, },
	[AUD_PLL_APLL] = { .enable = true, },
	[AUD_PLL_RX0]  = { .enable = true, },
	[AUD_PLL_USB0] = { .enable = true, },
	[AUD_PLL_HSC0] = { .enable = true, },
};

static struct snd_soc_dai_driver uniphier_aio_dai_ld11[] = {
	{
		.name    = AUD_GNAME_HDMI,
		.playback = {
			.stream_name = AUD_NAME_PCMOUT1,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = AUD_NAME_PCMIN1,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_32000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_PCMIN2,
		.capture = {
			.stream_name = AUD_NAME_PCMIN2,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_GNAME_LINE,
		.playback = {
			.stream_name = AUD_NAME_PCMOUT2,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.capture = {
			.stream_name = AUD_NAME_PCMIN3,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_HPCMOUT1,
		.playback = {
			.stream_name = AUD_NAME_HPCMOUT1,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 8,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_PCMOUT3,
		.playback = {
			.stream_name = AUD_NAME_PCMOUT3,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_HIECOUT1,
		.playback = {
			.stream_name = AUD_NAME_HIECOUT1,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_spdif_ld11_ops,
	},
	{
		.name    = AUD_NAME_EPCMOUT2,
		.playback = {
			.stream_name = AUD_NAME_EPCMOUT2,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_32000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_EPCMOUT3,
		.playback = {
			.stream_name = AUD_NAME_EPCMOUT3,
			.formats     = SNDRV_PCM_FMTBIT_S32_LE,
			.rates       = SNDRV_PCM_RATE_48000 |
				SNDRV_PCM_RATE_44100 |
				SNDRV_PCM_RATE_32000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &uniphier_aio_i2s_ld11_ops,
	},
	{
		.name    = AUD_NAME_HIECCOMPOUT1,
		.playback = {
			.stream_name = AUD_NAME_HIECCOMPOUT1,
			.channels_min = 1,
			.channels_max = 1,
		},
		.ops = &uniphier_aio_spdif_ld11_ops2,
	},
};

static const struct uniphier_aio_chip_spec uniphier_aio_ld11_spec = {
	.specs     = uniphier_aio_ld11,
	.num_specs = ARRAY_SIZE(uniphier_aio_ld11),
	.dais      = uniphier_aio_dai_ld11,
	.num_dais  = ARRAY_SIZE(uniphier_aio_dai_ld11),
	.plls      = uniphier_aio_pll_ld11,
	.num_plls  = ARRAY_SIZE(uniphier_aio_pll_ld11),
	.addr_ext  = 0,
};

static const struct uniphier_aio_chip_spec uniphier_aio_ld20_spec = {
	.specs     = uniphier_aio_ld11,
	.num_specs = ARRAY_SIZE(uniphier_aio_ld11),
	.dais      = uniphier_aio_dai_ld11,
	.num_dais  = ARRAY_SIZE(uniphier_aio_dai_ld11),
	.plls      = uniphier_aio_pll_ld11,
	.num_plls  = ARRAY_SIZE(uniphier_aio_pll_ld11),
	.addr_ext  = 1,
};

static const struct of_device_id uniphier_aio_of_match[] __maybe_unused = {
	{
		.compatible = "socionext,uniphier-ld11-aio",
		.data = &uniphier_aio_ld11_spec,
	},
	{
		.compatible = "socionext,uniphier-ld20-aio",
		.data = &uniphier_aio_ld20_spec,
	},
	{},
};
MODULE_DEVICE_TABLE(of, uniphier_aio_of_match);

static struct platform_driver uniphier_aio_driver = {
	.driver = {
		.name = "snd-uniphier-aio-ld11",
		.of_match_table = of_match_ptr(uniphier_aio_of_match),
	},
	.probe    = uniphier_aio_probe,
	.remove   = uniphier_aio_remove,
};
module_platform_driver(uniphier_aio_driver);

MODULE_AUTHOR("Katsuhiro Suzuki <suzuki.katsuhiro@socionext.com>");
MODULE_DESCRIPTION("UniPhier LD11/LD20 AIO driver.");
MODULE_LICENSE("GPL v2");
