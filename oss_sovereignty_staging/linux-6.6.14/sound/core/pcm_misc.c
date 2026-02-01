 
  
#include <linux/time.h>
#include <linux/export.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include "pcm_local.h"

#define SND_PCM_FORMAT_UNKNOWN (-1)

 
struct pcm_format_data {
	unsigned char width;	 
	unsigned char phys;	 
	signed char le;	 
	signed char signd;	 
	unsigned char silence[8];	 
};

 
#define INT	__force int

static bool valid_format(snd_pcm_format_t format)
{
	return (INT)format >= 0 && (INT)format <= (INT)SNDRV_PCM_FORMAT_LAST;
}

static const struct pcm_format_data pcm_formats[(INT)SNDRV_PCM_FORMAT_LAST+1] = {
	[SNDRV_PCM_FORMAT_S8] = {
		.width = 8, .phys = 8, .le = -1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U8] = {
		.width = 8, .phys = 8, .le = -1, .signd = 0,
		.silence = { 0x80 },
	},
	[SNDRV_PCM_FORMAT_S16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 0,
		.silence = { 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S24_LE] = {
		.width = 24, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S24_BE] = {
		.width = 24, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U24_LE] = {
		.width = 24, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U24_BE] = {
		.width = 24, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x00, 0x80, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_FLOAT_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT64_LE] = {
		.width = 64, .phys = 64, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_FLOAT64_BE] = {
		.width = 64, .phys = 64, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_MU_LAW] = {
		.width = 8, .phys = 8, .le = -1, .signd = -1,
		.silence = { 0x7f },
	},
	[SNDRV_PCM_FORMAT_A_LAW] = {
		.width = 8, .phys = 8, .le = -1, .signd = -1,
		.silence = { 0x55 },
	},
	[SNDRV_PCM_FORMAT_IMA_ADPCM] = {
		.width = 4, .phys = 4, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_24] = {
		.width = 3, .phys = 3, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_40] = {
		.width = 5, .phys = 5, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_DSD_U8] = {
		.width = 8, .phys = 8, .le = 1, .signd = 0,
		.silence = { 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U16_LE] = {
		.width = 16, .phys = 16, .le = 1, .signd = 0,
		.silence = { 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U32_LE] = {
		.width = 32, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x69, 0x69, 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U16_BE] = {
		.width = 16, .phys = 16, .le = 0, .signd = 0,
		.silence = { 0x69, 0x69 },
	},
	[SNDRV_PCM_FORMAT_DSD_U32_BE] = {
		.width = 32, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x69, 0x69, 0x69, 0x69 },
	},
	 
	[SNDRV_PCM_FORMAT_MPEG] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_GSM] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_S20_LE] = {
		.width = 20, .phys = 32, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S20_BE] = {
		.width = 20, .phys = 32, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U20_LE] = {
		.width = 20, .phys = 32, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x08, 0x00 },
	},
	[SNDRV_PCM_FORMAT_U20_BE] = {
		.width = 20, .phys = 32, .le = 0, .signd = 0,
		.silence = { 0x00, 0x08, 0x00, 0x00 },
	},
	 
	[SNDRV_PCM_FORMAT_SPECIAL] = {
		.le = -1, .signd = -1,
	},
	[SNDRV_PCM_FORMAT_S24_3LE] = {
		.width = 24, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S24_3BE] = {
		.width = 24, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U24_3LE] = {
		.width = 24, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x80 },
	},
	[SNDRV_PCM_FORMAT_U24_3BE] = {
		.width = 24, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x80, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S20_3LE] = {
		.width = 20, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S20_3BE] = {
		.width = 20, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U20_3LE] = {
		.width = 20, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x08 },
	},
	[SNDRV_PCM_FORMAT_U20_3BE] = {
		.width = 20, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x08, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_S18_3LE] = {
		.width = 18, .phys = 24, .le = 1, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_S18_3BE] = {
		.width = 18, .phys = 24, .le = 0, .signd = 1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_U18_3LE] = {
		.width = 18, .phys = 24, .le = 1, .signd = 0,
		.silence = { 0x00, 0x00, 0x02 },
	},
	[SNDRV_PCM_FORMAT_U18_3BE] = {
		.width = 18, .phys = 24, .le = 0, .signd = 0,
		.silence = { 0x02, 0x00, 0x00 },
	},
	[SNDRV_PCM_FORMAT_G723_24_1B] = {
		.width = 3, .phys = 8, .le = -1, .signd = -1,
		.silence = {},
	},
	[SNDRV_PCM_FORMAT_G723_40_1B] = {
		.width = 5, .phys = 8, .le = -1, .signd = -1,
		.silence = {},
	},
};


 
int snd_pcm_format_signed(snd_pcm_format_t format)
{
	int val;
	if (!valid_format(format))
		return -EINVAL;
	val = pcm_formats[(INT)format].signd;
	if (val < 0)
		return -EINVAL;
	return val;
}
EXPORT_SYMBOL(snd_pcm_format_signed);

 
int snd_pcm_format_unsigned(snd_pcm_format_t format)
{
	int val;

	val = snd_pcm_format_signed(format);
	if (val < 0)
		return val;
	return !val;
}
EXPORT_SYMBOL(snd_pcm_format_unsigned);

 
int snd_pcm_format_linear(snd_pcm_format_t format)
{
	return snd_pcm_format_signed(format) >= 0;
}
EXPORT_SYMBOL(snd_pcm_format_linear);

 
int snd_pcm_format_little_endian(snd_pcm_format_t format)
{
	int val;
	if (!valid_format(format))
		return -EINVAL;
	val = pcm_formats[(INT)format].le;
	if (val < 0)
		return -EINVAL;
	return val;
}
EXPORT_SYMBOL(snd_pcm_format_little_endian);

 
int snd_pcm_format_big_endian(snd_pcm_format_t format)
{
	int val;

	val = snd_pcm_format_little_endian(format);
	if (val < 0)
		return val;
	return !val;
}
EXPORT_SYMBOL(snd_pcm_format_big_endian);

 
int snd_pcm_format_width(snd_pcm_format_t format)
{
	int val;
	if (!valid_format(format))
		return -EINVAL;
	val = pcm_formats[(INT)format].width;
	if (!val)
		return -EINVAL;
	return val;
}
EXPORT_SYMBOL(snd_pcm_format_width);

 
int snd_pcm_format_physical_width(snd_pcm_format_t format)
{
	int val;
	if (!valid_format(format))
		return -EINVAL;
	val = pcm_formats[(INT)format].phys;
	if (!val)
		return -EINVAL;
	return val;
}
EXPORT_SYMBOL(snd_pcm_format_physical_width);

 
ssize_t snd_pcm_format_size(snd_pcm_format_t format, size_t samples)
{
	int phys_width = snd_pcm_format_physical_width(format);
	if (phys_width < 0)
		return -EINVAL;
	return samples * phys_width / 8;
}
EXPORT_SYMBOL(snd_pcm_format_size);

 
const unsigned char *snd_pcm_format_silence_64(snd_pcm_format_t format)
{
	if (!valid_format(format))
		return NULL;
	if (! pcm_formats[(INT)format].phys)
		return NULL;
	return pcm_formats[(INT)format].silence;
}
EXPORT_SYMBOL(snd_pcm_format_silence_64);

 
int snd_pcm_format_set_silence(snd_pcm_format_t format, void *data, unsigned int samples)
{
	int width;
	unsigned char *dst;
	const unsigned char *pat;

	if (!valid_format(format))
		return -EINVAL;
	if (samples == 0)
		return 0;
	width = pcm_formats[(INT)format].phys;  
	pat = pcm_formats[(INT)format].silence;
	if (!width || !pat)
		return -EINVAL;
	 
	if (pcm_formats[(INT)format].signd == 1 || width <= 8) {
		unsigned int bytes = samples * width / 8;
		memset(data, *pat, bytes);
		return 0;
	}
	 
	width /= 8;
	dst = data;
#if 0
	while (samples--) {
		memcpy(dst, pat, width);
		dst += width;
	}
#else
	 
	switch (width) {
	case 2:
		while (samples--) {
			memcpy(dst, pat, 2);
			dst += 2;
		}
		break;
	case 3:
		while (samples--) {
			memcpy(dst, pat, 3);
			dst += 3;
		}
		break;
	case 4:
		while (samples--) {
			memcpy(dst, pat, 4);
			dst += 4;
		}
		break;
	case 8:
		while (samples--) {
			memcpy(dst, pat, 8);
			dst += 8;
		}
		break;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(snd_pcm_format_set_silence);

 
int snd_pcm_hw_limit_rates(struct snd_pcm_hardware *hw)
{
	int i;
	for (i = 0; i < (int)snd_pcm_known_rates.count; i++) {
		if (hw->rates & (1 << i)) {
			hw->rate_min = snd_pcm_known_rates.list[i];
			break;
		}
	}
	for (i = (int)snd_pcm_known_rates.count - 1; i >= 0; i--) {
		if (hw->rates & (1 << i)) {
			hw->rate_max = snd_pcm_known_rates.list[i];
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(snd_pcm_hw_limit_rates);

 
unsigned int snd_pcm_rate_to_rate_bit(unsigned int rate)
{
	unsigned int i;

	for (i = 0; i < snd_pcm_known_rates.count; i++)
		if (snd_pcm_known_rates.list[i] == rate)
			return 1u << i;
	return SNDRV_PCM_RATE_KNOT;
}
EXPORT_SYMBOL(snd_pcm_rate_to_rate_bit);

 
unsigned int snd_pcm_rate_bit_to_rate(unsigned int rate_bit)
{
	unsigned int i;

	for (i = 0; i < snd_pcm_known_rates.count; i++)
		if ((1u << i) == rate_bit)
			return snd_pcm_known_rates.list[i];
	return 0;
}
EXPORT_SYMBOL(snd_pcm_rate_bit_to_rate);

static unsigned int snd_pcm_rate_mask_sanitize(unsigned int rates)
{
	if (rates & SNDRV_PCM_RATE_CONTINUOUS)
		return SNDRV_PCM_RATE_CONTINUOUS;
	else if (rates & SNDRV_PCM_RATE_KNOT)
		return SNDRV_PCM_RATE_KNOT;
	return rates;
}

 
unsigned int snd_pcm_rate_mask_intersect(unsigned int rates_a,
	unsigned int rates_b)
{
	rates_a = snd_pcm_rate_mask_sanitize(rates_a);
	rates_b = snd_pcm_rate_mask_sanitize(rates_b);

	if (rates_a & SNDRV_PCM_RATE_CONTINUOUS)
		return rates_b;
	else if (rates_b & SNDRV_PCM_RATE_CONTINUOUS)
		return rates_a;
	else if (rates_a & SNDRV_PCM_RATE_KNOT)
		return rates_b;
	else if (rates_b & SNDRV_PCM_RATE_KNOT)
		return rates_a;
	return rates_a & rates_b;
}
EXPORT_SYMBOL_GPL(snd_pcm_rate_mask_intersect);

 
unsigned int snd_pcm_rate_range_to_bits(unsigned int rate_min,
	unsigned int rate_max)
{
	unsigned int rates = 0;
	int i;

	for (i = 0; i < snd_pcm_known_rates.count; i++) {
		if (snd_pcm_known_rates.list[i] >= rate_min
			&& snd_pcm_known_rates.list[i] <= rate_max)
			rates |= 1 << i;
	}

	if (!rates)
		rates = SNDRV_PCM_RATE_KNOT;

	return rates;
}
EXPORT_SYMBOL_GPL(snd_pcm_rate_range_to_bits);
