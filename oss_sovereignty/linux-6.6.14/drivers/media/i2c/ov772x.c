
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/i2c/ov772x.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-subdev.h>

 
#define GAIN        0x00  
#define BLUE        0x01  
#define RED         0x02  
#define GREEN       0x03  
#define COM1        0x04  
#define BAVG        0x05  
#define GAVG        0x06  
#define RAVG        0x07  
#define AECH        0x08  
#define COM2        0x09  
#define PID         0x0A  
#define VER         0x0B  
#define COM3        0x0C  
#define COM4        0x0D  
#define COM5        0x0E  
#define COM6        0x0F  
#define AEC         0x10  
#define CLKRC       0x11  
#define COM7        0x12  
#define COM8        0x13  
#define COM9        0x14  
#define COM10       0x15  
#define REG16       0x16  
#define HSTART      0x17  
#define HSIZE       0x18  
#define VSTART      0x19  
#define VSIZE       0x1A  
#define PSHFT       0x1B  
#define MIDH        0x1C  
#define MIDL        0x1D  
#define LAEC        0x1F  
#define COM11       0x20  
#define BDBASE      0x22  
#define DBSTEP      0x23  
#define AEW         0x24  
#define AEB         0x25  
#define VPT         0x26  
#define REG28       0x28  
#define HOUTSIZE    0x29  
#define EXHCH       0x2A  
#define EXHCL       0x2B  
#define VOUTSIZE    0x2C  
#define ADVFL       0x2D  
#define ADVFH       0x2E  
#define YAVE        0x2F  
#define LUMHTH      0x30  
#define LUMLTH      0x31  
#define HREF        0x32  
#define DM_LNL      0x33  
#define DM_LNH      0x34  
#define ADOFF_B     0x35  
#define ADOFF_R     0x36  
#define ADOFF_GB    0x37  
#define ADOFF_GR    0x38  
#define OFF_B       0x39  
#define OFF_R       0x3A  
#define OFF_GB      0x3B  
#define OFF_GR      0x3C  
#define COM12       0x3D  
#define COM13       0x3E  
#define COM14       0x3F  
#define COM15       0x40  
#define COM16       0x41  
#define TGT_B       0x42  
#define TGT_R       0x43  
#define TGT_GB      0x44  
#define TGT_GR      0x45  
 
#define LCC0        0x46  
#define LCC1        0x47  
#define LCC2        0x48  
#define LCC3        0x49  
#define LCC4        0x4A  
#define LCC5        0x4B  
#define LCC6        0x4C  
 
#define LC_CTR      0x46  
#define LC_XC       0x47  
#define LC_YC       0x48  
#define LC_COEF     0x49  
#define LC_RADI     0x4A  
#define LC_COEFB    0x4B  
#define LC_COEFR    0x4C  

#define FIXGAIN     0x4D  
#define AREF0       0x4E  
#define AREF1       0x4F  
#define AREF2       0x50  
#define AREF3       0x51  
#define AREF4       0x52  
#define AREF5       0x53  
#define AREF6       0x54  
#define AREF7       0x55  
#define UFIX        0x60  
#define VFIX        0x61  
#define AWBB_BLK    0x62  
#define AWB_CTRL0   0x63  
#define DSP_CTRL1   0x64  
#define DSP_CTRL2   0x65  
#define DSP_CTRL3   0x66  
#define DSP_CTRL4   0x67  
#define AWB_BIAS    0x68  
#define AWB_CTRL1   0x69  
#define AWB_CTRL2   0x6A  
#define AWB_CTRL3   0x6B  
#define AWB_CTRL4   0x6C  
#define AWB_CTRL5   0x6D  
#define AWB_CTRL6   0x6E  
#define AWB_CTRL7   0x6F  
#define AWB_CTRL8   0x70  
#define AWB_CTRL9   0x71  
#define AWB_CTRL10  0x72  
#define AWB_CTRL11  0x73  
#define AWB_CTRL12  0x74  
#define AWB_CTRL13  0x75  
#define AWB_CTRL14  0x76  
#define AWB_CTRL15  0x77  
#define AWB_CTRL16  0x78  
#define AWB_CTRL17  0x79  
#define AWB_CTRL18  0x7A  
#define AWB_CTRL19  0x7B  
#define AWB_CTRL20  0x7C  
#define AWB_CTRL21  0x7D  
#define GAM1        0x7E  
#define GAM2        0x7F  
#define GAM3        0x80  
#define GAM4        0x81  
#define GAM5        0x82  
#define GAM6        0x83  
#define GAM7        0x84  
#define GAM8        0x85  
#define GAM9        0x86  
#define GAM10       0x87  
#define GAM11       0x88  
#define GAM12       0x89  
#define GAM13       0x8A  
#define GAM14       0x8B  
#define GAM15       0x8C  
#define SLOP        0x8D  
#define DNSTH       0x8E  
#define EDGE_STRNGT 0x8F  
#define EDGE_TRSHLD 0x90  
#define DNSOFF      0x91  
#define EDGE_UPPER  0x92  
#define EDGE_LOWER  0x93  
#define MTX1        0x94  
#define MTX2        0x95  
#define MTX3        0x96  
#define MTX4        0x97  
#define MTX5        0x98  
#define MTX6        0x99  
#define MTX_CTRL    0x9A  
#define BRIGHT      0x9B  
#define CNTRST      0x9C  
#define CNTRST_CTRL 0x9D  
#define UVAD_J0     0x9E  
#define UVAD_J1     0x9F  
#define SCAL0       0xA0  
#define SCAL1       0xA1  
#define SCAL2       0xA2  
#define FIFODLYM    0xA3  
#define FIFODLYA    0xA4  
#define SDE         0xA6  
#define USAT        0xA7  
#define VSAT        0xA8  
 
#define HUE0        0xA9  
#define HUE1        0xAA  
 
#define HUECOS      0xA9  
#define HUESIN      0xAA  

#define SIGN        0xAB  
#define DSPAUTO     0xAC  

 

 
#define SOFT_SLEEP_MODE 0x10	 
				 
#define OCAP_1x         0x00	 
#define OCAP_2x         0x01	 
#define OCAP_3x         0x02	 
#define OCAP_4x         0x03	 

 
#define SWAP_MASK       (SWAP_RGB | SWAP_YUV | SWAP_ML)
#define IMG_MASK        (VFLIP_IMG | HFLIP_IMG | SCOLOR_TEST)

#define VFLIP_IMG       0x80	 
#define HFLIP_IMG       0x40	 
#define SWAP_RGB        0x20	 
#define SWAP_YUV        0x10	 
#define SWAP_ML         0x08	 
				 
#define NOTRI_CLOCK     0x04	 
				 
				 
#define NOTRI_DATA      0x02	 
				 
#define SCOLOR_TEST     0x01	 

 
				 
#define PLL_BYPASS      0x00	 
#define PLL_4x          0x40	 
#define PLL_6x          0x80	 
#define PLL_8x          0xc0	 
				 
#define AEC_FULL        0x00	 
#define AEC_1p2         0x10	 
#define AEC_1p4         0x20	 
#define AEC_2p3         0x30	 
#define COM4_RESERVED   0x01	 

 
#define AFR_ON_OFF      0x80	 
#define AFR_SPPED       0x40	 
				 
#define AFR_NO_RATE     0x00	 
#define AFR_1p2         0x10	 
#define AFR_1p4         0x20	 
#define AFR_1p8         0x30	 
				 
#define AF_2x           0x00	 
#define AF_4x           0x04	 
#define AF_8x           0x08	 
#define AF_16x          0x0c	 
				 
#define AEC_NO_LIMIT    0x01	 
				 
 
				 
#define CLKRC_RESERVED  0x80	 
#define CLKRC_DIV(n)    ((n) - 1)

 
				 
#define SCCB_RESET      0x80	 
				 
				 
#define SLCT_MASK       0x40	 
#define SLCT_VGA        0x00	 
#define SLCT_QVGA       0x40	 
#define ITU656_ON_OFF   0x20	 
#define SENSOR_RAW	0x10	 
				 
#define FMT_MASK        0x0c	 
#define FMT_GBR422      0x00	 
#define FMT_RGB565      0x04	 
#define FMT_RGB555      0x08	 
#define FMT_RGB444      0x0c	 
				 
#define OFMT_MASK       0x03     
#define OFMT_YUV        0x00	 
#define OFMT_P_BRAW     0x01	 
#define OFMT_RGB        0x02	 
#define OFMT_BRAW       0x03	 

 
#define FAST_ALGO       0x80	 
				 
#define UNLMT_STEP      0x40	 
				 
#define BNDF_ON_OFF     0x20	 
#define AEC_BND         0x10	 
#define AEC_ON_OFF      0x08	 
#define AGC_ON          0x04	 
#define AWB_ON          0x02	 
#define AEC_ON          0x01	 

 
#define BASE_AECAGC     0x80	 
				 
#define GAIN_2x         0x00	 
#define GAIN_4x         0x10	 
#define GAIN_8x         0x20	 
#define GAIN_16x        0x30	 
#define GAIN_32x        0x40	 
#define GAIN_64x        0x50	 
#define GAIN_128x       0x60	 
#define DROP_VSYNC      0x04	 
#define DROP_HREF       0x02	 

 
#define SGLF_ON_OFF     0x02	 
#define SGLF_TRIG       0x01	 

 
#define HREF_VSTART_SHIFT	6	 
#define HREF_HSTART_SHIFT	4	 
#define HREF_VSIZE_SHIFT	2	 
#define HREF_HSIZE_SHIFT	0	 

 
#define EXHCH_VSIZE_SHIFT	2	 
#define EXHCH_HSIZE_SHIFT	0	 

 
#define FIFO_ON         0x80	 
#define UV_ON_OFF       0x40	 
#define YUV444_2_422    0x20	 
#define CLR_MTRX_ON_OFF 0x10	 
#define INTPLT_ON_OFF   0x08	 
#define GMM_ON_OFF      0x04	 
#define AUTO_BLK_ON_OFF 0x02	 
#define AUTO_WHT_ON_OFF 0x01	 

 
#define UV_MASK         0x80	 
#define UV_ON           0x80	 
#define UV_OFF          0x00	 
#define CBAR_MASK       0x20	 
#define CBAR_ON         0x20	 
#define CBAR_OFF        0x00	 

 
#define DSP_OFMT_YUV	0x00
#define DSP_OFMT_RGB	0x00
#define DSP_OFMT_RAW8	0x02
#define DSP_OFMT_RAW10	0x03

 
#define AWB_ACTRL       0x80  
#define DENOISE_ACTRL   0x40  
#define EDGE_ACTRL      0x20  
#define UV_ACTRL        0x10  
#define SCAL0_ACTRL     0x08  
#define SCAL1_2_ACTRL   0x04  

#define OV772X_MAX_WIDTH	VGA_WIDTH
#define OV772X_MAX_HEIGHT	VGA_HEIGHT

 
#define OV7720  0x7720
#define OV7725  0x7721
#define VERSION(pid, ver) ((pid << 8) | (ver & 0xFF))

 
static struct {
	unsigned int mult;
	u8 com4;
} ov772x_pll[] = {
	{ 1, PLL_BYPASS, },
	{ 4, PLL_4x, },
	{ 6, PLL_6x, },
	{ 8, PLL_8x, },
};

 

struct ov772x_color_format {
	u32 code;
	enum v4l2_colorspace colorspace;
	u8 dsp3;
	u8 dsp4;
	u8 com3;
	u8 com7;
};

struct ov772x_win_size {
	char                     *name;
	unsigned char             com7_bit;
	unsigned int		  sizeimage;
	struct v4l2_rect	  rect;
};

struct ov772x_priv {
	struct v4l2_subdev                subdev;
	struct v4l2_ctrl_handler	  hdl;
	struct clk			 *clk;
	struct regmap			 *regmap;
	struct ov772x_camera_info        *info;
	struct gpio_desc		 *pwdn_gpio;
	struct gpio_desc		 *rstb_gpio;
	const struct ov772x_color_format *cfmt;
	const struct ov772x_win_size     *win;
	struct v4l2_ctrl		 *vflip_ctrl;
	struct v4l2_ctrl		 *hflip_ctrl;
	unsigned int			  test_pattern;
	 
	struct v4l2_ctrl		 *band_filter_ctrl;
	unsigned int			  fps;
	 
	struct mutex			  lock;
	int				  power_count;
	int				  streaming;
#ifdef CONFIG_MEDIA_CONTROLLER
	struct media_pad pad;
#endif
	enum v4l2_mbus_type		  bus_type;
};

 
static const struct ov772x_color_format ov772x_cfmts[] = {
	{
		.code		= MEDIA_BUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= SWAP_YUV,
		.com7		= OFMT_YUV,
	},
	{
		.code		= MEDIA_BUS_FMT_YVYU8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= UV_ON,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= SWAP_YUV,
		.com7		= OFMT_YUV,
	},
	{
		.code		= MEDIA_BUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= 0x0,
		.com7		= OFMT_YUV,
	},
	{
		.code		= MEDIA_BUS_FMT_RGB555_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= SWAP_RGB,
		.com7		= FMT_RGB555 | OFMT_RGB,
	},
	{
		.code		= MEDIA_BUS_FMT_RGB555_2X8_PADHI_BE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= 0x0,
		.com7		= FMT_RGB555 | OFMT_RGB,
	},
	{
		.code		= MEDIA_BUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= SWAP_RGB,
		.com7		= FMT_RGB565 | OFMT_RGB,
	},
	{
		.code		= MEDIA_BUS_FMT_RGB565_2X8_BE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_YUV,
		.com3		= 0x0,
		.com7		= FMT_RGB565 | OFMT_RGB,
	},
	{
		 
		.code		= MEDIA_BUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.dsp3		= 0x0,
		.dsp4		= DSP_OFMT_RAW10,
		.com3		= 0x0,
		.com7		= SENSOR_RAW | OFMT_BRAW,
	},
};

 

static const struct ov772x_win_size ov772x_win_sizes[] = {
	{
		.name		= "VGA",
		.com7_bit	= SLCT_VGA,
		.sizeimage	= 510 * 748,
		.rect = {
			.left	= 140,
			.top	= 14,
			.width	= VGA_WIDTH,
			.height	= VGA_HEIGHT,
		},
	}, {
		.name		= "QVGA",
		.com7_bit	= SLCT_QVGA,
		.sizeimage	= 278 * 576,
		.rect = {
			.left	= 252,
			.top	= 6,
			.width	= QVGA_WIDTH,
			.height	= QVGA_HEIGHT,
		},
	},
};

static const char * const ov772x_test_pattern_menu[] = {
	"Disabled",
	"Vertical Color Bar Type 1",
};

 
static const unsigned int ov772x_frame_intervals[] = { 5, 10, 15, 20, 30, 60 };

 

static struct ov772x_priv *to_ov772x(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ov772x_priv, subdev);
}

static int ov772x_reset(struct ov772x_priv *priv)
{
	int ret;

	ret = regmap_write(priv->regmap, COM7, SCCB_RESET);
	if (ret < 0)
		return ret;

	usleep_range(1000, 5000);

	return regmap_update_bits(priv->regmap, COM2, SOFT_SLEEP_MODE,
				  SOFT_SLEEP_MODE);
}

 

static int ov772x_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov772x_priv *priv = to_ov772x(sd);
	int ret = 0;

	mutex_lock(&priv->lock);

	if (priv->streaming == enable)
		goto done;

	if (priv->bus_type == V4L2_MBUS_BT656) {
		ret = regmap_update_bits(priv->regmap, COM7, ITU656_ON_OFF,
					 enable ?
					 ITU656_ON_OFF : ~ITU656_ON_OFF);
		if (ret)
			goto done;
	}

	ret = regmap_update_bits(priv->regmap, COM2, SOFT_SLEEP_MODE,
				 enable ? 0 : SOFT_SLEEP_MODE);
	if (ret)
		goto done;

	if (enable) {
		dev_dbg(&client->dev, "format %d, win %s\n",
			priv->cfmt->code, priv->win->name);
	}
	priv->streaming = enable;

done:
	mutex_unlock(&priv->lock);

	return ret;
}

static unsigned int ov772x_select_fps(struct ov772x_priv *priv,
				      struct v4l2_fract *tpf)
{
	unsigned int fps = tpf->numerator ?
			   tpf->denominator / tpf->numerator :
			   tpf->denominator;
	unsigned int best_diff;
	unsigned int diff;
	unsigned int idx;
	unsigned int i;

	 
	best_diff = ~0L;
	for (i = 0, idx = 0; i < ARRAY_SIZE(ov772x_frame_intervals); i++) {
		diff = abs(fps - ov772x_frame_intervals[i]);
		if (diff < best_diff) {
			idx = i;
			best_diff = diff;
		}
	}

	return ov772x_frame_intervals[idx];
}

static int ov772x_set_frame_rate(struct ov772x_priv *priv,
				 unsigned int fps,
				 const struct ov772x_color_format *cfmt,
				 const struct ov772x_win_size *win)
{
	unsigned long fin = clk_get_rate(priv->clk);
	unsigned int best_diff;
	unsigned int fsize;
	unsigned int pclk;
	unsigned int diff;
	unsigned int i;
	u8 clkrc = 0;
	u8 com4 = 0;
	int ret;

	 
	switch (cfmt->com7 & OFMT_MASK) {
	case OFMT_BRAW:
		fsize = win->sizeimage;
		break;
	case OFMT_RGB:
	case OFMT_YUV:
	default:
		fsize = win->sizeimage * 2;
		break;
	}

	pclk = fps * fsize;

	 
	best_diff = ~0L;
	for (i = 0; i < ARRAY_SIZE(ov772x_pll); i++) {
		unsigned int pll_mult = ov772x_pll[i].mult;
		unsigned int pll_out = pll_mult * fin;
		unsigned int t_pclk;
		unsigned int div;

		if (pll_out < pclk)
			continue;

		div = DIV_ROUND_CLOSEST(pll_out, pclk);
		t_pclk = DIV_ROUND_CLOSEST(fin * pll_mult, div);
		diff = abs(pclk - t_pclk);
		if (diff < best_diff) {
			best_diff = diff;
			clkrc = CLKRC_DIV(div);
			com4 = ov772x_pll[i].com4;
		}
	}

	ret = regmap_write(priv->regmap, COM4, com4 | COM4_RESERVED);
	if (ret < 0)
		return ret;

	ret = regmap_write(priv->regmap, CLKRC, clkrc | CLKRC_RESERVED);
	if (ret < 0)
		return ret;

	return 0;
}

static int ov772x_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *ival)
{
	struct ov772x_priv *priv = to_ov772x(sd);
	struct v4l2_fract *tpf = &ival->interval;

	tpf->numerator = 1;
	tpf->denominator = priv->fps;

	return 0;
}

static int ov772x_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *ival)
{
	struct ov772x_priv *priv = to_ov772x(sd);
	struct v4l2_fract *tpf = &ival->interval;
	unsigned int fps;
	int ret = 0;

	mutex_lock(&priv->lock);

	if (priv->streaming) {
		ret = -EBUSY;
		goto error;
	}

	fps = ov772x_select_fps(priv, tpf);

	 
	if (priv->power_count > 0) {
		ret = ov772x_set_frame_rate(priv, fps, priv->cfmt, priv->win);
		if (ret)
			goto error;
	}

	tpf->numerator = 1;
	tpf->denominator = fps;
	priv->fps = fps;

error:
	mutex_unlock(&priv->lock);

	return ret;
}

static int ov772x_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct ov772x_priv *priv = container_of(ctrl->handler,
						struct ov772x_priv, hdl);
	struct regmap *regmap = priv->regmap;
	int ret = 0;
	u8 val;

	 

	 
	if (priv->power_count == 0)
		return 0;

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		val = ctrl->val ? VFLIP_IMG : 0x00;
		if (priv->info && (priv->info->flags & OV772X_FLAG_VFLIP))
			val ^= VFLIP_IMG;
		return regmap_update_bits(regmap, COM3, VFLIP_IMG, val);
	case V4L2_CID_HFLIP:
		val = ctrl->val ? HFLIP_IMG : 0x00;
		if (priv->info && (priv->info->flags & OV772X_FLAG_HFLIP))
			val ^= HFLIP_IMG;
		return regmap_update_bits(regmap, COM3, HFLIP_IMG, val);
	case V4L2_CID_BAND_STOP_FILTER:
		if (!ctrl->val) {
			 
			ret = regmap_update_bits(regmap, BDBASE, 0xff, 0xff);
			if (!ret)
				ret = regmap_update_bits(regmap, COM8,
							 BNDF_ON_OFF, 0);
		} else {
			 
			val = 256 - ctrl->val;
			ret = regmap_update_bits(regmap, COM8,
						 BNDF_ON_OFF, BNDF_ON_OFF);
			if (!ret)
				ret = regmap_update_bits(regmap, BDBASE,
							 0xff, val);
		}

		return ret;
	case V4L2_CID_TEST_PATTERN:
		priv->test_pattern = ctrl->val;
		return 0;
	}

	return -EINVAL;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov772x_g_register(struct v4l2_subdev *sd,
			     struct v4l2_dbg_register *reg)
{
	struct ov772x_priv *priv = to_ov772x(sd);
	int ret;
	unsigned int val;

	reg->size = 1;
	if (reg->reg > 0xff)
		return -EINVAL;

	ret = regmap_read(priv->regmap, reg->reg, &val);
	if (ret < 0)
		return ret;

	reg->val = (__u64)val;

	return 0;
}

static int ov772x_s_register(struct v4l2_subdev *sd,
			     const struct v4l2_dbg_register *reg)
{
	struct ov772x_priv *priv = to_ov772x(sd);

	if (reg->reg > 0xff ||
	    reg->val > 0xff)
		return -EINVAL;

	return regmap_write(priv->regmap, reg->reg, reg->val);
}
#endif

static int ov772x_power_on(struct ov772x_priv *priv)
{
	struct i2c_client *client = v4l2_get_subdevdata(&priv->subdev);
	int ret;

	if (priv->clk) {
		ret = clk_prepare_enable(priv->clk);
		if (ret)
			return ret;
	}

	if (priv->pwdn_gpio) {
		gpiod_set_value(priv->pwdn_gpio, 1);
		usleep_range(500, 1000);
	}

	 
	priv->rstb_gpio = gpiod_get_optional(&client->dev, "reset",
					     GPIOD_OUT_LOW);
	if (IS_ERR(priv->rstb_gpio)) {
		dev_info(&client->dev, "Unable to get GPIO \"reset\"");
		clk_disable_unprepare(priv->clk);
		return PTR_ERR(priv->rstb_gpio);
	}

	if (priv->rstb_gpio) {
		gpiod_set_value(priv->rstb_gpio, 1);
		usleep_range(500, 1000);
		gpiod_set_value(priv->rstb_gpio, 0);
		usleep_range(500, 1000);

		gpiod_put(priv->rstb_gpio);
	}

	return 0;
}

static int ov772x_power_off(struct ov772x_priv *priv)
{
	clk_disable_unprepare(priv->clk);

	if (priv->pwdn_gpio) {
		gpiod_set_value(priv->pwdn_gpio, 0);
		usleep_range(500, 1000);
	}

	return 0;
}

static int ov772x_set_params(struct ov772x_priv *priv,
			     const struct ov772x_color_format *cfmt,
			     const struct ov772x_win_size *win);

static int ov772x_s_power(struct v4l2_subdev *sd, int on)
{
	struct ov772x_priv *priv = to_ov772x(sd);
	int ret = 0;

	mutex_lock(&priv->lock);

	 
	if (priv->power_count == !on) {
		if (on) {
			ret = ov772x_power_on(priv);
			 
			if (!ret)
				ret = ov772x_set_params(priv, priv->cfmt,
							priv->win);
		} else {
			ret = ov772x_power_off(priv);
		}
	}

	if (!ret) {
		 
		priv->power_count += on ? 1 : -1;
		WARN(priv->power_count < 0, "Unbalanced power count\n");
		WARN(priv->power_count > 1, "Duplicated s_power call\n");
	}

	mutex_unlock(&priv->lock);

	return ret;
}

static const struct ov772x_win_size *ov772x_select_win(u32 width, u32 height)
{
	const struct ov772x_win_size *win = &ov772x_win_sizes[0];
	u32 best_diff = UINT_MAX;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ov772x_win_sizes); ++i) {
		u32 diff = abs(width - ov772x_win_sizes[i].rect.width)
			 + abs(height - ov772x_win_sizes[i].rect.height);
		if (diff < best_diff) {
			best_diff = diff;
			win = &ov772x_win_sizes[i];
		}
	}

	return win;
}

static void ov772x_select_params(const struct v4l2_mbus_framefmt *mf,
				 const struct ov772x_color_format **cfmt,
				 const struct ov772x_win_size **win)
{
	unsigned int i;

	 
	*cfmt = &ov772x_cfmts[0];

	for (i = 0; i < ARRAY_SIZE(ov772x_cfmts); i++) {
		if (mf->code == ov772x_cfmts[i].code) {
			*cfmt = &ov772x_cfmts[i];
			break;
		}
	}

	 
	*win = ov772x_select_win(mf->width, mf->height);
}

static int ov772x_edgectrl(struct ov772x_priv *priv)
{
	struct regmap *regmap = priv->regmap;
	int ret;

	if (!priv->info)
		return 0;

	if (priv->info->edgectrl.strength & OV772X_MANUAL_EDGE_CTRL) {
		 

		ret = regmap_update_bits(regmap, DSPAUTO, EDGE_ACTRL, 0x00);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(regmap, EDGE_TRSHLD,
					 OV772X_EDGE_THRESHOLD_MASK,
					 priv->info->edgectrl.threshold);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(regmap, EDGE_STRNGT,
					 OV772X_EDGE_STRENGTH_MASK,
					 priv->info->edgectrl.strength);
		if (ret < 0)
			return ret;

	} else if (priv->info->edgectrl.upper > priv->info->edgectrl.lower) {
		 
		ret = regmap_update_bits(regmap, EDGE_UPPER,
					 OV772X_EDGE_UPPER_MASK,
					 priv->info->edgectrl.upper);
		if (ret < 0)
			return ret;

		ret = regmap_update_bits(regmap, EDGE_LOWER,
					 OV772X_EDGE_LOWER_MASK,
					 priv->info->edgectrl.lower);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ov772x_set_params(struct ov772x_priv *priv,
			     const struct ov772x_color_format *cfmt,
			     const struct ov772x_win_size *win)
{
	int ret;
	u8  val;

	 
	ov772x_reset(priv);

	 
	ret = ov772x_edgectrl(priv);
	if (ret < 0)
		return ret;

	 
	ret = regmap_write(priv->regmap, HSTART, win->rect.left >> 2);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, HSIZE, win->rect.width >> 2);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, VSTART, win->rect.top >> 1);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, VSIZE, win->rect.height >> 1);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, HOUTSIZE, win->rect.width >> 2);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, VOUTSIZE, win->rect.height >> 1);
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, HREF,
			   ((win->rect.top & 1) << HREF_VSTART_SHIFT) |
			   ((win->rect.left & 3) << HREF_HSTART_SHIFT) |
			   ((win->rect.height & 1) << HREF_VSIZE_SHIFT) |
			   ((win->rect.width & 3) << HREF_HSIZE_SHIFT));
	if (ret < 0)
		goto ov772x_set_fmt_error;
	ret = regmap_write(priv->regmap, EXHCH,
			   ((win->rect.height & 1) << EXHCH_VSIZE_SHIFT) |
			   ((win->rect.width & 3) << EXHCH_HSIZE_SHIFT));
	if (ret < 0)
		goto ov772x_set_fmt_error;

	 
	val = cfmt->dsp3;
	if (val) {
		ret = regmap_update_bits(priv->regmap, DSP_CTRL3, UV_MASK, val);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	 
	if (cfmt->dsp4) {
		ret = regmap_write(priv->regmap, DSP_CTRL4, cfmt->dsp4);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	 
	val = cfmt->com3;
	if (priv->info && (priv->info->flags & OV772X_FLAG_VFLIP))
		val |= VFLIP_IMG;
	if (priv->info && (priv->info->flags & OV772X_FLAG_HFLIP))
		val |= HFLIP_IMG;
	if (priv->vflip_ctrl->val)
		val ^= VFLIP_IMG;
	if (priv->hflip_ctrl->val)
		val ^= HFLIP_IMG;
	if (priv->test_pattern)
		val |= SCOLOR_TEST;

	ret = regmap_update_bits(priv->regmap, COM3, SWAP_MASK | IMG_MASK, val);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	 
	ret = regmap_write(priv->regmap, COM7, win->com7_bit | cfmt->com7);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	 
	ret = ov772x_set_frame_rate(priv, priv->fps, cfmt, win);
	if (ret < 0)
		goto ov772x_set_fmt_error;

	 
	if (priv->band_filter_ctrl->val) {
		unsigned short band_filter = priv->band_filter_ctrl->val;

		ret = regmap_update_bits(priv->regmap, COM8,
					 BNDF_ON_OFF, BNDF_ON_OFF);
		if (!ret)
			ret = regmap_update_bits(priv->regmap, BDBASE,
						 0xff, 256 - band_filter);
		if (ret < 0)
			goto ov772x_set_fmt_error;
	}

	return ret;

ov772x_set_fmt_error:

	ov772x_reset(priv);

	return ret;
}

static int ov772x_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct ov772x_priv *priv = to_ov772x(sd);

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	sel->r.left = 0;
	sel->r.top = 0;
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP:
		sel->r.width = priv->win->rect.width;
		sel->r.height = priv->win->rect.height;
		return 0;
	default:
		return -EINVAL;
	}
}

static int ov772x_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct ov772x_priv *priv = to_ov772x(sd);

	if (format->pad)
		return -EINVAL;

	mf->width	= priv->win->rect.width;
	mf->height	= priv->win->rect.height;
	mf->code	= priv->cfmt->code;
	mf->colorspace	= priv->cfmt->colorspace;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int ov772x_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct ov772x_priv *priv = to_ov772x(sd);
	struct v4l2_mbus_framefmt *mf = &format->format;
	const struct ov772x_color_format *cfmt;
	const struct ov772x_win_size *win;
	int ret = 0;

	if (format->pad)
		return -EINVAL;

	ov772x_select_params(mf, &cfmt, &win);

	mf->code = cfmt->code;
	mf->width = win->rect.width;
	mf->height = win->rect.height;
	mf->field = V4L2_FIELD_NONE;
	mf->colorspace = cfmt->colorspace;
	mf->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	mf->quantization = V4L2_QUANTIZATION_DEFAULT;
	mf->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	if (format->which == V4L2_SUBDEV_FORMAT_TRY) {
		sd_state->pads->try_fmt = *mf;
		return 0;
	}

	mutex_lock(&priv->lock);

	if (priv->streaming) {
		ret = -EBUSY;
		goto error;
	}

	 
	if (priv->power_count > 0) {
		ret = ov772x_set_params(priv, cfmt, win);
		if (ret < 0)
			goto error;
	}
	priv->win = win;
	priv->cfmt = cfmt;

error:
	mutex_unlock(&priv->lock);

	return ret;
}

static int ov772x_video_probe(struct ov772x_priv *priv)
{
	struct i2c_client  *client = v4l2_get_subdevdata(&priv->subdev);
	int		    pid, ver, midh, midl;
	const char         *devname;
	int		    ret;

	ret = ov772x_power_on(priv);
	if (ret < 0)
		return ret;

	 
	ret = regmap_read(priv->regmap, PID, &pid);
	if (ret < 0)
		return ret;
	ret = regmap_read(priv->regmap, VER, &ver);
	if (ret < 0)
		return ret;

	switch (VERSION(pid, ver)) {
	case OV7720:
		devname     = "ov7720";
		break;
	case OV7725:
		devname     = "ov7725";
		break;
	default:
		dev_err(&client->dev,
			"Product ID error %x:%x\n", pid, ver);
		ret = -ENODEV;
		goto done;
	}

	ret = regmap_read(priv->regmap, MIDH, &midh);
	if (ret < 0)
		return ret;
	ret = regmap_read(priv->regmap, MIDL, &midl);
	if (ret < 0)
		return ret;

	dev_info(&client->dev,
		 "%s Product ID %0x:%0x Manufacturer ID %x:%x\n",
		 devname, pid, ver, midh, midl);

	ret = v4l2_ctrl_handler_setup(&priv->hdl);

done:
	ov772x_power_off(priv);

	return ret;
}

static const struct v4l2_ctrl_ops ov772x_ctrl_ops = {
	.s_ctrl = ov772x_s_ctrl,
};

static const struct v4l2_subdev_core_ops ov772x_subdev_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
	.subscribe_event = v4l2_ctrl_subdev_subscribe_event,
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= ov772x_g_register,
	.s_register	= ov772x_s_register,
#endif
	.s_power	= ov772x_s_power,
};

static int ov772x_enum_frame_interval(struct v4l2_subdev *sd,
				      struct v4l2_subdev_state *sd_state,
				      struct v4l2_subdev_frame_interval_enum *fie)
{
	if (fie->pad || fie->index >= ARRAY_SIZE(ov772x_frame_intervals))
		return -EINVAL;

	if (fie->width != VGA_WIDTH && fie->width != QVGA_WIDTH)
		return -EINVAL;
	if (fie->height != VGA_HEIGHT && fie->height != QVGA_HEIGHT)
		return -EINVAL;

	fie->interval.numerator = 1;
	fie->interval.denominator = ov772x_frame_intervals[fie->index];

	return 0;
}

static int ov772x_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->pad || code->index >= ARRAY_SIZE(ov772x_cfmts))
		return -EINVAL;

	code->code = ov772x_cfmts[code->index].code;

	return 0;
}

static const struct v4l2_subdev_video_ops ov772x_subdev_video_ops = {
	.s_stream		= ov772x_s_stream,
	.s_frame_interval	= ov772x_s_frame_interval,
	.g_frame_interval	= ov772x_g_frame_interval,
};

static const struct v4l2_subdev_pad_ops ov772x_subdev_pad_ops = {
	.enum_frame_interval	= ov772x_enum_frame_interval,
	.enum_mbus_code		= ov772x_enum_mbus_code,
	.get_selection		= ov772x_get_selection,
	.get_fmt		= ov772x_get_fmt,
	.set_fmt		= ov772x_set_fmt,
};

static const struct v4l2_subdev_ops ov772x_subdev_ops = {
	.core	= &ov772x_subdev_core_ops,
	.video	= &ov772x_subdev_video_ops,
	.pad	= &ov772x_subdev_pad_ops,
};

static int ov772x_parse_dt(struct i2c_client *client,
			   struct ov772x_priv *priv)
{
	struct v4l2_fwnode_endpoint bus_cfg = {
		.bus_type = V4L2_MBUS_PARALLEL
	};
	struct fwnode_handle *ep;
	int ret;

	ep = fwnode_graph_get_next_endpoint(dev_fwnode(&client->dev), NULL);
	if (!ep) {
		dev_err(&client->dev, "Endpoint node not found\n");
		return -EINVAL;
	}

	 
	ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
	if (ret) {
		bus_cfg = (struct v4l2_fwnode_endpoint)
			  { .bus_type = V4L2_MBUS_BT656 };
		ret = v4l2_fwnode_endpoint_alloc_parse(ep, &bus_cfg);
		if (ret)
			goto error_fwnode_put;
	}

	priv->bus_type = bus_cfg.bus_type;
	v4l2_fwnode_endpoint_free(&bus_cfg);

error_fwnode_put:
	fwnode_handle_put(ep);

	return ret;
}

 

static int ov772x_probe(struct i2c_client *client)
{
	struct ov772x_priv	*priv;
	int			ret;
	static const struct regmap_config ov772x_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8,
		.max_register = DSPAUTO,
	};

	if (!client->dev.of_node && !client->dev.platform_data) {
		dev_err(&client->dev,
			"Missing ov772x platform data for non-DT device\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->regmap = devm_regmap_init_sccb(client, &ov772x_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev, "Failed to allocate register map\n");
		return PTR_ERR(priv->regmap);
	}

	priv->info = client->dev.platform_data;
	mutex_init(&priv->lock);

	v4l2_i2c_subdev_init(&priv->subdev, client, &ov772x_subdev_ops);
	priv->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
			      V4L2_SUBDEV_FL_HAS_EVENTS;
	v4l2_ctrl_handler_init(&priv->hdl, 3);
	 
	priv->hdl.lock = &priv->lock;
	priv->vflip_ctrl = v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
					     V4L2_CID_VFLIP, 0, 1, 1, 0);
	priv->hflip_ctrl = v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
					     V4L2_CID_HFLIP, 0, 1, 1, 0);
	priv->band_filter_ctrl = v4l2_ctrl_new_std(&priv->hdl, &ov772x_ctrl_ops,
						   V4L2_CID_BAND_STOP_FILTER,
						   0, 256, 1, 0);
	v4l2_ctrl_new_std_menu_items(&priv->hdl, &ov772x_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov772x_test_pattern_menu) - 1,
				     0, 0, ov772x_test_pattern_menu);
	priv->subdev.ctrl_handler = &priv->hdl;
	if (priv->hdl.error) {
		ret = priv->hdl.error;
		goto error_ctrl_free;
	}

	priv->clk = clk_get(&client->dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(&client->dev, "Unable to get xclk clock\n");
		ret = PTR_ERR(priv->clk);
		goto error_ctrl_free;
	}

	priv->pwdn_gpio = gpiod_get_optional(&client->dev, "powerdown",
					     GPIOD_OUT_LOW);
	if (IS_ERR(priv->pwdn_gpio)) {
		dev_info(&client->dev, "Unable to get GPIO \"powerdown\"");
		ret = PTR_ERR(priv->pwdn_gpio);
		goto error_clk_put;
	}

	ret = ov772x_parse_dt(client, priv);
	if (ret)
		goto error_clk_put;

	ret = ov772x_video_probe(priv);
	if (ret < 0)
		goto error_gpio_put;

#ifdef CONFIG_MEDIA_CONTROLLER
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->subdev.entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&priv->subdev.entity, 1, &priv->pad);
	if (ret < 0)
		goto error_gpio_put;
#endif

	priv->cfmt = &ov772x_cfmts[0];
	priv->win = &ov772x_win_sizes[0];
	priv->fps = 15;

	ret = v4l2_async_register_subdev(&priv->subdev);
	if (ret)
		goto error_entity_cleanup;

	return 0;

error_entity_cleanup:
	media_entity_cleanup(&priv->subdev.entity);
error_gpio_put:
	if (priv->pwdn_gpio)
		gpiod_put(priv->pwdn_gpio);
error_clk_put:
	clk_put(priv->clk);
error_ctrl_free:
	v4l2_ctrl_handler_free(&priv->hdl);
	mutex_destroy(&priv->lock);

	return ret;
}

static void ov772x_remove(struct i2c_client *client)
{
	struct ov772x_priv *priv = to_ov772x(i2c_get_clientdata(client));

	media_entity_cleanup(&priv->subdev.entity);
	clk_put(priv->clk);
	if (priv->pwdn_gpio)
		gpiod_put(priv->pwdn_gpio);
	v4l2_async_unregister_subdev(&priv->subdev);
	v4l2_ctrl_handler_free(&priv->hdl);
	mutex_destroy(&priv->lock);
}

static const struct i2c_device_id ov772x_id[] = {
	{ "ov772x", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov772x_id);

static const struct of_device_id ov772x_of_match[] = {
	{ .compatible = "ovti,ov7725", },
	{ .compatible = "ovti,ov7720", },
	{   },
};
MODULE_DEVICE_TABLE(of, ov772x_of_match);

static struct i2c_driver ov772x_i2c_driver = {
	.driver = {
		.name = "ov772x",
		.of_match_table = ov772x_of_match,
	},
	.probe    = ov772x_probe,
	.remove   = ov772x_remove,
	.id_table = ov772x_id,
};

module_i2c_driver(ov772x_i2c_driver);

MODULE_DESCRIPTION("V4L2 driver for OV772x image sensor");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
