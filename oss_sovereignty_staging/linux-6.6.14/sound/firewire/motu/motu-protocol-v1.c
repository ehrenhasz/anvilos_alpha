




#include "motu.h"

#include <linux/delay.h>




































#define CLK_828_STATUS_OFFSET				0x0b00
#define  CLK_828_STATUS_MASK				0x0000ffff
#define  CLK_828_STATUS_FLAG_OPT_IN_IFACE_IS_SPDIF	0x00008000
#define  CLK_828_STATUS_FLAG_OPT_OUT_IFACE_IS_SPDIF	0x00004000
#define  CLK_828_STATUS_FLAG_FETCH_PCM_FRAMES		0x00000080
#define  CLK_828_STATUS_FLAG_ENABLE_OUTPUT		0x00000008
#define  CLK_828_STATUS_FLAG_RATE_48000			0x00000004
#define  CLK_828_STATUS_MASK_SRC			0x00000023
#define   CLK_828_STATUS_FLAG_SRC_ADAT_ON_OPT		0x00000021
#define   CLK_828_STATUS_FLAG_SRC_SPH			0x00000003
#define   CLK_828_STATUS_FLAG_SRC_SPDIF			0x00000002
#define   CLK_828_STATUS_FLAG_SRC_ADAT_ON_DSUB		0x00000001
#define   CLK_828_STATUS_FLAG_SRC_INTERNAL		0x00000000



















































#define CLK_896_STATUS_OFFSET			0x0b14
#define  CLK_896_STATUS_FLAG_FETCH_ENABLE	0x20000000
#define  CLK_896_STATUS_FLAG_OUTPUT_ON		0x03000000
#define  CLK_896_STATUS_MASK_SRC		0x00000007
#define   CLK_896_STATUS_FLAG_SRC_INTERNAL	0x00000000
#define   CLK_896_STATUS_FLAG_SRC_ADAT_ON_OPT	0x00000001
#define   CLK_896_STATUS_FLAG_SRC_AESEBU	0x00000002
#define   CLK_896_STATUS_FLAG_SRC_SPH		0x00000003
#define   CLK_896_STATUS_FLAG_SRC_WORD		0x00000004
#define   CLK_896_STATUS_FLAG_SRC_ADAT_ON_DSUB	0x00000005
#define  CLK_896_STATUS_MASK_RATE		0x00000018
#define   CLK_896_STATUS_FLAG_RATE_44100	0x00000000
#define   CLK_896_STATUS_FLAG_RATE_48000	0x00000008
#define   CLK_896_STATUS_FLAG_RATE_88200	0x00000010
#define   CLK_896_STATUS_FLAG_RATE_96000	0x00000018

static void parse_clock_rate_828(u32 data, unsigned int *rate)
{
	if (data & CLK_828_STATUS_FLAG_RATE_48000)
		*rate = 48000;
	else
		*rate = 44100;
}

static int get_clock_rate_828(struct snd_motu *motu, unsigned int *rate)
{
	__be32 reg;
	int err;

	err = snd_motu_transaction_read(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	parse_clock_rate_828(be32_to_cpu(reg), rate);

	return 0;
}

static int parse_clock_rate_896(u32 data, unsigned int *rate)
{
	switch (data & CLK_896_STATUS_MASK_RATE) {
	case CLK_896_STATUS_FLAG_RATE_44100:
		*rate = 44100;
		break;
	case CLK_896_STATUS_FLAG_RATE_48000:
		*rate = 48000;
		break;
	case CLK_896_STATUS_FLAG_RATE_88200:
		*rate = 88200;
		break;
	case CLK_896_STATUS_FLAG_RATE_96000:
		*rate = 96000;
		break;
	default:
		return -ENXIO;
	}

	return 0;
}

static int get_clock_rate_896(struct snd_motu *motu, unsigned int *rate)
{
	__be32 reg;
	int err;

	err = snd_motu_transaction_read(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	return parse_clock_rate_896(be32_to_cpu(reg), rate);
}

int snd_motu_protocol_v1_get_clock_rate(struct snd_motu *motu, unsigned int *rate)
{
	if (motu->spec == &snd_motu_spec_828)
		return get_clock_rate_828(motu, rate);
	else if (motu->spec == &snd_motu_spec_896)
		return get_clock_rate_896(motu, rate);
	else
		return -ENXIO;
}

static int set_clock_rate_828(struct snd_motu *motu, unsigned int rate)
{
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg) & CLK_828_STATUS_MASK;

	data &= ~CLK_828_STATUS_FLAG_RATE_48000;
	if (rate == 48000)
		data |= CLK_828_STATUS_FLAG_RATE_48000;

	reg = cpu_to_be32(data);
	return snd_motu_transaction_write(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
}

static int set_clock_rate_896(struct snd_motu *motu, unsigned int rate)
{
	unsigned int flag;
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg);

	switch (rate) {
	case 44100:
		flag = CLK_896_STATUS_FLAG_RATE_44100;
		break;
	case 48000:
		flag = CLK_896_STATUS_FLAG_RATE_48000;
		break;
	case 88200:
		flag = CLK_896_STATUS_FLAG_RATE_88200;
		break;
	case 96000:
		flag = CLK_896_STATUS_FLAG_RATE_96000;
		break;
	default:
		return -EINVAL;
	}

	data &= ~CLK_896_STATUS_MASK_RATE;
	data |= flag;

	reg = cpu_to_be32(data);
	return snd_motu_transaction_write(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
}

int snd_motu_protocol_v1_set_clock_rate(struct snd_motu *motu, unsigned int rate)
{
	if (motu->spec == &snd_motu_spec_828)
		return set_clock_rate_828(motu, rate);
	else if (motu->spec == &snd_motu_spec_896)
		return set_clock_rate_896(motu, rate);
	else
		return -ENXIO;
}

static int get_clock_source_828(struct snd_motu *motu, enum snd_motu_clock_source *src)
{
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg) & CLK_828_STATUS_MASK;

	switch (data & CLK_828_STATUS_MASK_SRC) {
	case CLK_828_STATUS_FLAG_SRC_ADAT_ON_OPT:
		*src = SND_MOTU_CLOCK_SOURCE_ADAT_ON_OPT;
		break;
	case CLK_828_STATUS_FLAG_SRC_SPH:
		*src = SND_MOTU_CLOCK_SOURCE_SPH;
		break;
	case CLK_828_STATUS_FLAG_SRC_SPDIF:
	{
		if (data & CLK_828_STATUS_FLAG_OPT_IN_IFACE_IS_SPDIF)
			*src = SND_MOTU_CLOCK_SOURCE_SPDIF_ON_COAX;
		else
			*src = SND_MOTU_CLOCK_SOURCE_SPDIF_ON_OPT;
		break;
	}
	case CLK_828_STATUS_FLAG_SRC_ADAT_ON_DSUB:
		*src = SND_MOTU_CLOCK_SOURCE_ADAT_ON_DSUB;
		break;
	case CLK_828_STATUS_FLAG_SRC_INTERNAL:
		*src = SND_MOTU_CLOCK_SOURCE_INTERNAL;
		break;
	default:
		return -ENXIO;
	}

	return 0;
}

static int get_clock_source_896(struct snd_motu *motu, enum snd_motu_clock_source *src)
{
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg);

	switch (data & CLK_896_STATUS_MASK_SRC) {
	case CLK_896_STATUS_FLAG_SRC_INTERNAL:
		*src = SND_MOTU_CLOCK_SOURCE_INTERNAL;
		break;
	case CLK_896_STATUS_FLAG_SRC_ADAT_ON_OPT:
		*src = SND_MOTU_CLOCK_SOURCE_ADAT_ON_OPT;
		break;
	case CLK_896_STATUS_FLAG_SRC_AESEBU:
		*src = SND_MOTU_CLOCK_SOURCE_AESEBU_ON_XLR;
		break;
	case CLK_896_STATUS_FLAG_SRC_SPH:
		*src = SND_MOTU_CLOCK_SOURCE_SPH;
		break;
	case CLK_896_STATUS_FLAG_SRC_WORD:
		*src = SND_MOTU_CLOCK_SOURCE_WORD_ON_BNC;
		break;
	case CLK_896_STATUS_FLAG_SRC_ADAT_ON_DSUB:
		*src = SND_MOTU_CLOCK_SOURCE_ADAT_ON_DSUB;
		break;
	default:
		return -ENXIO;
	}

	return 0;
}

int snd_motu_protocol_v1_get_clock_source(struct snd_motu *motu, enum snd_motu_clock_source *src)
{
	if (motu->spec == &snd_motu_spec_828)
		return get_clock_source_828(motu, src);
	else if (motu->spec == &snd_motu_spec_896)
		return get_clock_source_896(motu, src);
	else
		return -ENXIO;
}

static int switch_fetching_mode_828(struct snd_motu *motu, bool enable)
{
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg) & CLK_828_STATUS_MASK;

	data &= ~(CLK_828_STATUS_FLAG_FETCH_PCM_FRAMES | CLK_828_STATUS_FLAG_ENABLE_OUTPUT);
	if (enable) {
		
		
		
		msleep(100);
		data |= CLK_828_STATUS_FLAG_FETCH_PCM_FRAMES | CLK_828_STATUS_FLAG_ENABLE_OUTPUT;
	}

	reg = cpu_to_be32(data);
	return snd_motu_transaction_write(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
}

static int switch_fetching_mode_896(struct snd_motu *motu, bool enable)
{
	__be32 reg;
	u32 data;
	int err;

	err = snd_motu_transaction_read(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg);

	data &= ~CLK_896_STATUS_FLAG_FETCH_ENABLE;
	if (enable)
		data |= CLK_896_STATUS_FLAG_FETCH_ENABLE | CLK_896_STATUS_FLAG_OUTPUT_ON;

	reg = cpu_to_be32(data);
	return snd_motu_transaction_write(motu, CLK_896_STATUS_OFFSET, &reg, sizeof(reg));
}

int snd_motu_protocol_v1_switch_fetching_mode(struct snd_motu *motu, bool enable)
{
	if (motu->spec == &snd_motu_spec_828)
		return switch_fetching_mode_828(motu, enable);
	else if (motu->spec == &snd_motu_spec_896)
		return switch_fetching_mode_896(motu, enable);
	else
		return -ENXIO;
}

static int detect_packet_formats_828(struct snd_motu *motu)
{
	__be32 reg;
	u32 data;
	int err;

	motu->tx_packet_formats.pcm_byte_offset = 4;
	motu->tx_packet_formats.msg_chunks = 2;

	motu->rx_packet_formats.pcm_byte_offset = 4;
	motu->rx_packet_formats.msg_chunks = 0;

	err = snd_motu_transaction_read(motu, CLK_828_STATUS_OFFSET, &reg, sizeof(reg));
	if (err < 0)
		return err;
	data = be32_to_cpu(reg) & CLK_828_STATUS_MASK;

	
	if (!(data & CLK_828_STATUS_FLAG_OPT_IN_IFACE_IS_SPDIF))
		motu->tx_packet_formats.pcm_chunks[0] += 8;

	if (!(data & CLK_828_STATUS_FLAG_OPT_OUT_IFACE_IS_SPDIF))
		motu->rx_packet_formats.pcm_chunks[0] += 8;

	return 0;
}

static int detect_packet_formats_896(struct snd_motu *motu)
{
	
	motu->tx_packet_formats.pcm_byte_offset = 4;
	motu->rx_packet_formats.pcm_byte_offset = 4;

	
	motu->tx_packet_formats.msg_chunks = 0;
	motu->rx_packet_formats.msg_chunks = 0;

	
	
	motu->tx_packet_formats.pcm_chunks[0] += 8;
	motu->tx_packet_formats.pcm_chunks[1] += 8;

	motu->rx_packet_formats.pcm_chunks[0] += 8;
	motu->rx_packet_formats.pcm_chunks[1] += 8;

	return 0;
}

int snd_motu_protocol_v1_cache_packet_formats(struct snd_motu *motu)
{
	memcpy(motu->tx_packet_formats.pcm_chunks, motu->spec->tx_fixed_pcm_chunks,
	       sizeof(motu->tx_packet_formats.pcm_chunks));
	memcpy(motu->rx_packet_formats.pcm_chunks, motu->spec->rx_fixed_pcm_chunks,
	       sizeof(motu->rx_packet_formats.pcm_chunks));

	if (motu->spec == &snd_motu_spec_828)
		return detect_packet_formats_828(motu);
	else if (motu->spec == &snd_motu_spec_896)
		return detect_packet_formats_896(motu);
	else
		return 0;
}

const struct snd_motu_spec snd_motu_spec_828 = {
	.name = "828",
	.protocol_version = SND_MOTU_PROTOCOL_V1,
	.tx_fixed_pcm_chunks = {10, 0, 0},
	.rx_fixed_pcm_chunks = {10, 0, 0},
};

const struct snd_motu_spec snd_motu_spec_896 = {
	.name = "896",
	.tx_fixed_pcm_chunks = {10, 10, 0},
	.rx_fixed_pcm_chunks = {10, 10, 0},
};
