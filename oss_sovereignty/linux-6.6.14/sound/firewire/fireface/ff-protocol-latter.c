




#include <linux/delay.h>

#include "ff.h"

#define LATTER_STF		0xffff00000004ULL
#define LATTER_ISOC_CHANNELS	0xffff00000008ULL
#define LATTER_ISOC_START	0xffff0000000cULL
#define LATTER_FETCH_MODE	0xffff00000010ULL
#define LATTER_SYNC_STATUS	0x0000801c0000ULL
























































static int parse_clock_bits(u32 data, unsigned int *rate,
			    enum snd_ff_clock_src *src,
			    enum snd_ff_unit_version unit_version)
{
	static const struct {
		unsigned int rate;
		u32 flag;
	} *rate_entry, rate_entries[] = {
		{ 32000,	0x00, },
		{ 44100,	0x01, },
		{ 48000,	0x02, },
		{ 64000,	0x04, },
		{ 88200,	0x05, },
		{ 96000,	0x06, },
		{ 128000,	0x08, },
		{ 176400,	0x09, },
		{ 192000,	0x0a, },
	};
	static const struct {
		enum snd_ff_clock_src src;
		u32 flag;
	} *clk_entry, *clk_entries, ucx_clk_entries[] = {
		{ SND_FF_CLOCK_SRC_SPDIF,	0x00000200, },
		{ SND_FF_CLOCK_SRC_ADAT1,	0x00000400, },
		{ SND_FF_CLOCK_SRC_WORD,	0x00000600, },
		{ SND_FF_CLOCK_SRC_INTERNAL,	0x00000e00, },
	}, ufx_ff802_clk_entries[] = {
		{ SND_FF_CLOCK_SRC_WORD,	0x00000200, },
		{ SND_FF_CLOCK_SRC_SPDIF,	0x00000400, },
		{ SND_FF_CLOCK_SRC_ADAT1,	0x00000600, },
		{ SND_FF_CLOCK_SRC_ADAT2,	0x00000800, },
		{ SND_FF_CLOCK_SRC_INTERNAL,	0x00000e00, },
	};
	u32 rate_bits;
	unsigned int clk_entry_count;
	int i;

	if (unit_version == SND_FF_UNIT_VERSION_UCX) {
		rate_bits = (data & 0x0f000000) >> 24;
		clk_entries = ucx_clk_entries;
		clk_entry_count = ARRAY_SIZE(ucx_clk_entries);
	} else {
		rate_bits = (data & 0xf0000000) >> 28;
		clk_entries = ufx_ff802_clk_entries;
		clk_entry_count = ARRAY_SIZE(ufx_ff802_clk_entries);
	}

	for (i = 0; i < ARRAY_SIZE(rate_entries); ++i) {
		rate_entry = rate_entries + i;
		if (rate_bits == rate_entry->flag) {
			*rate = rate_entry->rate;
			break;
		}
	}
	if (i == ARRAY_SIZE(rate_entries))
		return -EIO;

	for (i = 0; i < clk_entry_count; ++i) {
		clk_entry = clk_entries + i;
		if ((data & 0x000e00) == clk_entry->flag) {
			*src = clk_entry->src;
			break;
		}
	}
	if (i == clk_entry_count)
		return -EIO;

	return 0;
}

static int latter_get_clock(struct snd_ff *ff, unsigned int *rate,
			   enum snd_ff_clock_src *src)
{
	__le32 reg;
	u32 data;
	int err;

	err = snd_fw_transaction(ff->unit, TCODE_READ_QUADLET_REQUEST,
				 LATTER_SYNC_STATUS, &reg, sizeof(reg), 0);
	if (err < 0)
		return err;
	data = le32_to_cpu(reg);

	return parse_clock_bits(data, rate, src, ff->unit_version);
}

static int latter_switch_fetching_mode(struct snd_ff *ff, bool enable)
{
	u32 data;
	__le32 reg;

	if (enable)
		data = 0x00000000;
	else
		data = 0xffffffff;
	reg = cpu_to_le32(data);

	return snd_fw_transaction(ff->unit, TCODE_WRITE_QUADLET_REQUEST,
				  LATTER_FETCH_MODE, &reg, sizeof(reg), 0);
}

static int latter_allocate_resources(struct snd_ff *ff, unsigned int rate)
{
	enum snd_ff_stream_mode mode;
	unsigned int code;
	__le32 reg;
	unsigned int count;
	int i;
	int err;

	
	if (rate % 48000 == 0)
		code = 0x04;
	else if (rate % 44100 == 0)
		code = 0x02;
	else if (rate % 32000 == 0)
		code = 0x00;
	else
		return -EINVAL;

	if (rate >= 64000 && rate < 128000)
		code |= 0x08;
	else if (rate >= 128000)
		code |= 0x10;

	reg = cpu_to_le32(code);
	err = snd_fw_transaction(ff->unit, TCODE_WRITE_QUADLET_REQUEST,
				 LATTER_STF, &reg, sizeof(reg), 0);
	if (err < 0)
		return err;

	
	count = 0;
	while (count++ < 10) {
		unsigned int curr_rate;
		enum snd_ff_clock_src src;

		err = latter_get_clock(ff, &curr_rate, &src);
		if (err < 0)
			return err;

		if (curr_rate == rate)
			break;
	}
	if (count > 10)
		return -ETIMEDOUT;

	for (i = 0; i < ARRAY_SIZE(amdtp_rate_table); ++i) {
		if (rate == amdtp_rate_table[i])
			break;
	}
	if (i == ARRAY_SIZE(amdtp_rate_table))
		return -EINVAL;

	err = snd_ff_stream_get_multiplier_mode(i, &mode);
	if (err < 0)
		return err;

	
	ff->tx_resources.channels_mask = 0x00000000000000ffuLL;
	err = fw_iso_resources_allocate(&ff->tx_resources,
			amdtp_stream_get_max_payload(&ff->tx_stream),
			fw_parent_device(ff->unit)->max_speed);
	if (err < 0)
		return err;

	
	ff->rx_resources.channels_mask = 0x00000000000000ffuLL;
	err = fw_iso_resources_allocate(&ff->rx_resources,
			amdtp_stream_get_max_payload(&ff->rx_stream),
			fw_parent_device(ff->unit)->max_speed);
	if (err < 0)
		fw_iso_resources_free(&ff->tx_resources);

	return err;
}

static int latter_begin_session(struct snd_ff *ff, unsigned int rate)
{
	unsigned int generation = ff->rx_resources.generation;
	unsigned int flag;
	u32 data;
	__le32 reg;
	int err;

	if (ff->unit_version == SND_FF_UNIT_VERSION_UCX) {
		
		
		if (rate >= 32000 && rate <= 48000)
			flag = 0x92;
		else if (rate >= 64000 && rate <= 96000)
			flag = 0x8e;
		else if (rate >= 128000 && rate <= 192000)
			flag = 0x8c;
		else
			return -EINVAL;
	} else {
		
		
		
		if (rate >= 32000 && rate <= 48000)
			flag = 0x9e;
		else if (rate >= 64000 && rate <= 96000)
			flag = 0x96;
		else if (rate >= 128000 && rate <= 192000)
			flag = 0x8e;
		else
			return -EINVAL;
	}

	if (generation != fw_parent_device(ff->unit)->card->generation) {
		err = fw_iso_resources_update(&ff->tx_resources);
		if (err < 0)
			return err;

		err = fw_iso_resources_update(&ff->rx_resources);
		if (err < 0)
			return err;
	}

	data = (ff->tx_resources.channel << 8) | ff->rx_resources.channel;
	reg = cpu_to_le32(data);
	err = snd_fw_transaction(ff->unit, TCODE_WRITE_QUADLET_REQUEST,
				 LATTER_ISOC_CHANNELS, &reg, sizeof(reg), 0);
	if (err < 0)
		return err;

	reg = cpu_to_le32(flag);
	return snd_fw_transaction(ff->unit, TCODE_WRITE_QUADLET_REQUEST,
				  LATTER_ISOC_START, &reg, sizeof(reg), 0);
}

static void latter_finish_session(struct snd_ff *ff)
{
	__le32 reg;

	reg = cpu_to_le32(0x00000000);
	snd_fw_transaction(ff->unit, TCODE_WRITE_QUADLET_REQUEST,
			   LATTER_ISOC_START, &reg, sizeof(reg), 0);
}

static void latter_dump_status(struct snd_ff *ff, struct snd_info_buffer *buffer)
{
	static const struct {
		char *const label;
		u32 locked_mask;
		u32 synced_mask;
	} *clk_entry, *clk_entries, ucx_clk_entries[] = {
		{ "S/PDIF",	0x00000001, 0x00000010, },
		{ "ADAT",	0x00000002, 0x00000020, },
		{ "WDClk",	0x00000004, 0x00000040, },
	}, ufx_ff802_clk_entries[] = {
		{ "WDClk",	0x00000001, 0x00000010, },
		{ "AES/EBU",	0x00000002, 0x00000020, },
		{ "ADAT-A",	0x00000004, 0x00000040, },
		{ "ADAT-B",	0x00000008, 0x00000080, },
	};
	__le32 reg;
	u32 data;
	unsigned int rate;
	enum snd_ff_clock_src src;
	const char *label;
	unsigned int clk_entry_count;
	int i;
	int err;

	err = snd_fw_transaction(ff->unit, TCODE_READ_QUADLET_REQUEST,
				 LATTER_SYNC_STATUS, &reg, sizeof(reg), 0);
	if (err < 0)
		return;
	data = le32_to_cpu(reg);

	snd_iprintf(buffer, "External source detection:\n");

	if (ff->unit_version == SND_FF_UNIT_VERSION_UCX) {
		clk_entries = ucx_clk_entries;
		clk_entry_count = ARRAY_SIZE(ucx_clk_entries);
	} else {
		clk_entries = ufx_ff802_clk_entries;
		clk_entry_count = ARRAY_SIZE(ufx_ff802_clk_entries);
	}

	for (i = 0; i < clk_entry_count; ++i) {
		clk_entry = clk_entries + i;
		snd_iprintf(buffer, "%s: ", clk_entry->label);
		if (data & clk_entry->locked_mask) {
			if (data & clk_entry->synced_mask)
				snd_iprintf(buffer, "sync\n");
			else
				snd_iprintf(buffer, "lock\n");
		} else {
			snd_iprintf(buffer, "none\n");
		}
	}

	err = parse_clock_bits(data, &rate, &src, ff->unit_version);
	if (err < 0)
		return;
	label = snd_ff_proc_get_clk_label(src);
	if (!label)
		return;

	snd_iprintf(buffer, "Referred clock: %s %d\n", label, rate);
}





















static void latter_handle_midi_msg(struct snd_ff *ff, unsigned int offset, const __le32 *buf,
				   size_t length, u32 tstamp)
{
	u32 data = le32_to_cpu(*buf);
	unsigned int index = (data & 0x000000f0) >> 4;
	u8 byte[3];
	struct snd_rawmidi_substream *substream;
	unsigned int len;

	if (index >= ff->spec->midi_in_ports)
		return;

	switch (data & 0x0000000f) {
	case 0x00000008:
	case 0x00000009:
	case 0x0000000a:
	case 0x0000000b:
	case 0x0000000e:
		len = 3;
		break;
	case 0x0000000c:
	case 0x0000000d:
		len = 2;
		break;
	default:
		len = data & 0x00000003;
		if (len == 0)
			len = 3;
		break;
	}

	byte[0] = (data & 0x0000ff00) >> 8;
	byte[1] = (data & 0x00ff0000) >> 16;
	byte[2] = (data & 0xff000000) >> 24;

	substream = READ_ONCE(ff->tx_midi_substreams[index]);
	if (substream)
		snd_rawmidi_receive(substream, byte, len);
}

 
static inline int calculate_message_bytes(u8 status)
{
	switch (status) {
	case 0xf6:	 
	case 0xf8:	 
	case 0xfa:	 
	case 0xfb:	 
	case 0xfc:	 
	case 0xfe:	 
	case 0xff:	 
		return 1;
	case 0xf1:	 
	case 0xf3:	 
		return 2;
	case 0xf2:	 
		return 3;
	case 0xf0:	 
		return 0;
	case 0xf7:	 
		break;
	case 0xf4:	 
	case 0xf5:	 
	case 0xf9:	 
	case 0xfd:	 
		break;
	default:
		switch (status & 0xf0) {
		case 0x80:	 
		case 0x90:	 
		case 0xa0:	 
		case 0xb0:	 
		case 0xe0:	 
			return 3;
		case 0xc0:	 
		case 0xd0:	 
			return 2;
		default:
		break;
		}
	break;
	}

	return -EINVAL;
}

static int latter_fill_midi_msg(struct snd_ff *ff,
				struct snd_rawmidi_substream *substream,
				unsigned int port)
{
	u32 data = {0};
	u8 *buf = (u8 *)&data;
	int consumed;

	buf[0] = port << 4;
	consumed = snd_rawmidi_transmit_peek(substream, buf + 1, 3);
	if (consumed <= 0)
		return consumed;

	if (!ff->on_sysex[port]) {
		if (buf[1] != 0xf0) {
			if (consumed < calculate_message_bytes(buf[1]))
				return 0;
		} else {
			
			ff->on_sysex[port] = true;
		}

		buf[0] |= consumed;
	} else {
		if (buf[1] != 0xf7) {
			if (buf[2] == 0xf7 || buf[3] == 0xf7) {
				
				consumed -= 1;
			}

			buf[0] |= consumed;
		} else {
			
			ff->on_sysex[port] = false;
			consumed = 1;
			buf[0] |= 0x0f;
		}
	}

	ff->msg_buf[port][0] = cpu_to_le32(data);
	ff->rx_bytes[port] = consumed;

	return 1;
}

const struct snd_ff_protocol snd_ff_protocol_latter = {
	.handle_msg		= latter_handle_midi_msg,
	.fill_midi_msg		= latter_fill_midi_msg,
	.get_clock		= latter_get_clock,
	.switch_fetching_mode	= latter_switch_fetching_mode,
	.allocate_resources	= latter_allocate_resources,
	.begin_session		= latter_begin_session,
	.finish_session		= latter_finish_session,
	.dump_status		= latter_dump_status,
};
