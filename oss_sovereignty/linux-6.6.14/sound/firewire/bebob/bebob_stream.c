
 

#include "./bebob.h"

#define READY_TIMEOUT_MS	4000

 

 
#define FORMAT_MAXIMUM_LENGTH 128

const unsigned int snd_bebob_rate_table[SND_BEBOB_STRM_FMT_ENTRIES] = {
	[0] = 32000,
	[1] = 44100,
	[2] = 48000,
	[3] = 88200,
	[4] = 96000,
	[5] = 176400,
	[6] = 192000,
};

 
static const unsigned int bridgeco_freq_table[] = {
	[0] = 0x02,
	[1] = 0x03,
	[2] = 0x04,
	[3] = 0x0a,
	[4] = 0x05,
	[5] = 0x06,
	[6] = 0x07,
};

static int
get_formation_index(unsigned int rate, unsigned int *index)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(snd_bebob_rate_table); i++) {
		if (snd_bebob_rate_table[i] == rate) {
			*index = i;
			return 0;
		}
	}
	return -EINVAL;
}

int
snd_bebob_stream_get_rate(struct snd_bebob *bebob, unsigned int *curr_rate)
{
	unsigned int tx_rate, rx_rate, trials;
	int err;

	trials = 0;
	do {
		err = avc_general_get_sig_fmt(bebob->unit, &tx_rate,
					      AVC_GENERAL_PLUG_DIR_OUT, 0);
	} while (err == -EAGAIN && ++trials < 3);
	if (err < 0)
		goto end;

	trials = 0;
	do {
		err = avc_general_get_sig_fmt(bebob->unit, &rx_rate,
					      AVC_GENERAL_PLUG_DIR_IN, 0);
	} while (err == -EAGAIN && ++trials < 3);
	if (err < 0)
		goto end;

	*curr_rate = rx_rate;
	if (rx_rate == tx_rate)
		goto end;

	 
	err = avc_general_set_sig_fmt(bebob->unit, rx_rate,
				      AVC_GENERAL_PLUG_DIR_IN, 0);
end:
	return err;
}

int
snd_bebob_stream_set_rate(struct snd_bebob *bebob, unsigned int rate)
{
	int err;

	err = avc_general_set_sig_fmt(bebob->unit, rate,
				      AVC_GENERAL_PLUG_DIR_OUT, 0);
	if (err < 0)
		goto end;

	err = avc_general_set_sig_fmt(bebob->unit, rate,
				      AVC_GENERAL_PLUG_DIR_IN, 0);
	if (err < 0)
		goto end;

	 
	msleep(300);
end:
	return err;
}

int snd_bebob_stream_get_clock_src(struct snd_bebob *bebob,
				   enum snd_bebob_clock_type *src)
{
	const struct snd_bebob_clock_spec *clk_spec = bebob->spec->clock;
	u8 addr[AVC_BRIDGECO_ADDR_BYTES], input[7];
	unsigned int id;
	enum avc_bridgeco_plug_type type;
	int err = 0;

	 
	if (clk_spec) {
		err = clk_spec->get(bebob, &id);
		if (err < 0) {
			dev_err(&bebob->unit->device,
				"fail to get clock source: %d\n", err);
			goto end;
		}

		if (id >= clk_spec->num) {
			dev_err(&bebob->unit->device,
				"clock source %d out of range 0..%d\n",
				id, clk_spec->num - 1);
			err = -EIO;
			goto end;
		}

		*src = clk_spec->types[id];
		goto end;
	}

	 
	if (bebob->sync_input_plug < 0) {
		*src = SND_BEBOB_CLOCK_TYPE_INTERNAL;
		goto end;
	}

	 
	avc_bridgeco_fill_msu_addr(addr, AVC_BRIDGECO_PLUG_DIR_IN,
				   bebob->sync_input_plug);
	err = avc_bridgeco_get_plug_input(bebob->unit, addr, input);
	if (err < 0) {
		dev_err(&bebob->unit->device,
			"fail to get an input for MSU in plug %d: %d\n",
			bebob->sync_input_plug, err);
		goto end;
	}

	 
	if (input[0] == 0xff) {
		*src = SND_BEBOB_CLOCK_TYPE_INTERNAL;
		goto end;
	}

	 
	if (input[0] == AVC_BRIDGECO_PLUG_DIR_OUT) {
		 
		if (input[1] == AVC_BRIDGECO_PLUG_MODE_SUBUNIT &&
		    input[2] == 0x0c) {
			*src = SND_BEBOB_CLOCK_TYPE_INTERNAL;
			goto end;
		}
	 
	} else if (input[1] == AVC_BRIDGECO_PLUG_MODE_UNIT) {
		if (input[2] == AVC_BRIDGECO_PLUG_UNIT_ISOC) {
			if (input[3] == 0x00) {
				 
				*src = SND_BEBOB_CLOCK_TYPE_SYT;
				goto end;
			} else {
				 
				*src = SND_BEBOB_CLOCK_TYPE_EXTERNAL;
				goto end;
			}
		} else if (input[2] == AVC_BRIDGECO_PLUG_UNIT_EXT) {
			 
			avc_bridgeco_fill_unit_addr(addr,
						    AVC_BRIDGECO_PLUG_DIR_IN,
						    AVC_BRIDGECO_PLUG_UNIT_EXT,
						    input[3]);
			err = avc_bridgeco_get_plug_type(bebob->unit, addr,
							 &type);
			if (err < 0)
				goto end;

			if (type == AVC_BRIDGECO_PLUG_TYPE_DIG) {
				 
				*src = SND_BEBOB_CLOCK_TYPE_EXTERNAL;
				goto end;
			} else if (type == AVC_BRIDGECO_PLUG_TYPE_SYNC) {
				 
				*src = SND_BEBOB_CLOCK_TYPE_EXTERNAL;
				goto end;
			} else if (type == AVC_BRIDGECO_PLUG_TYPE_ADDITION) {
				 
				*src = SND_BEBOB_CLOCK_TYPE_INTERNAL;
				goto end;
			}
		}
	}

	 
	err = -EIO;
end:
	return err;
}

static int map_data_channels(struct snd_bebob *bebob, struct amdtp_stream *s)
{
	unsigned int sec, sections, ch, channels;
	unsigned int pcm, midi, location;
	unsigned int stm_pos, sec_loc, pos;
	u8 *buf, addr[AVC_BRIDGECO_ADDR_BYTES], type;
	enum avc_bridgeco_plug_dir dir;
	int err;

	 
	buf = kzalloc(256, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (s == &bebob->tx_stream)
		dir = AVC_BRIDGECO_PLUG_DIR_OUT;
	else
		dir = AVC_BRIDGECO_PLUG_DIR_IN;

	avc_bridgeco_fill_unit_addr(addr, dir, AVC_BRIDGECO_PLUG_UNIT_ISOC, 0);
	err = avc_bridgeco_get_plug_ch_pos(bebob->unit, addr, buf, 256);
	if (err < 0) {
		dev_err(&bebob->unit->device,
			"fail to get channel position for isoc %s plug 0: %d\n",
			(dir == AVC_BRIDGECO_PLUG_DIR_IN) ? "in" : "out",
			err);
		goto end;
	}
	pos = 0;

	 
	pcm = 0;
	midi = 0;

	 
	sections = buf[pos++];

	for (sec = 0; sec < sections; sec++) {
		 
		avc_bridgeco_fill_unit_addr(addr, dir,
					    AVC_BRIDGECO_PLUG_UNIT_ISOC, 0);
		err = avc_bridgeco_get_plug_section_type(bebob->unit, addr,
							 sec, &type);
		if (err < 0) {
			dev_err(&bebob->unit->device,
			"fail to get section type for isoc %s plug 0: %d\n",
				(dir == AVC_BRIDGECO_PLUG_DIR_IN) ? "in" :
								    "out",
				err);
			goto end;
		}
		 
		if (type == 0xff) {
			err = -ENOSYS;
			goto end;
		}

		 
		channels = buf[pos++];

		for (ch = 0; ch < channels; ch++) {
			 
			stm_pos = buf[pos++] - 1;
			 
			sec_loc = buf[pos++] - 1;

			 
			if (sec_loc >= channels)
				sec_loc = ch;

			switch (type) {
			 
			case 0x0a:
				 
				if ((midi > 0) && (stm_pos != midi)) {
					err = -ENOSYS;
					goto end;
				}
				amdtp_am824_set_midi_position(s, stm_pos);
				midi = stm_pos;
				break;
			 
			case 0x01:	 
			case 0x02:	 
			case 0x03:	 
			case 0x04:	 
			case 0x05:	 
			case 0x06:	 
			case 0x07:	 
			 
			case 0x08:	 
			case 0x09:	 
			default:
				location = pcm + sec_loc;
				if (location >= AM824_MAX_CHANNELS_FOR_PCM) {
					err = -ENOSYS;
					goto end;
				}
				amdtp_am824_set_pcm_position(s, location,
							     stm_pos);
				break;
			}
		}

		if (type != 0x0a)
			pcm += channels;
		else
			midi += channels;
	}
end:
	kfree(buf);
	return err;
}

static int
check_connection_used_by_others(struct snd_bebob *bebob, struct amdtp_stream *s)
{
	struct cmp_connection *conn;
	bool used;
	int err;

	if (s == &bebob->tx_stream)
		conn = &bebob->out_conn;
	else
		conn = &bebob->in_conn;

	err = cmp_connection_check_used(conn, &used);
	if ((err >= 0) && used && !amdtp_stream_running(s)) {
		dev_err(&bebob->unit->device,
			"Connection established by others: %cPCR[%d]\n",
			(conn->direction == CMP_OUTPUT) ? 'o' : 'i',
			conn->pcr_index);
		err = -EBUSY;
	}

	return err;
}

static void break_both_connections(struct snd_bebob *bebob)
{
	cmp_connection_break(&bebob->in_conn);
	cmp_connection_break(&bebob->out_conn);
}

static int start_stream(struct snd_bebob *bebob, struct amdtp_stream *stream)
{
	struct cmp_connection *conn;
	int err = 0;

	if (stream == &bebob->rx_stream)
		conn = &bebob->in_conn;
	else
		conn = &bebob->out_conn;

	 
	if (bebob->maudio_special_quirk == NULL) {
		err = map_data_channels(bebob, stream);
		if (err < 0)
			return err;
	}

	err = cmp_connection_establish(conn);
	if (err < 0)
		return err;

	return amdtp_domain_add_stream(&bebob->domain, stream,
				       conn->resources.channel, conn->speed);
}

static int init_stream(struct snd_bebob *bebob, struct amdtp_stream *stream)
{
	unsigned int flags = CIP_BLOCKING;
	enum amdtp_stream_direction dir_stream;
	struct cmp_connection *conn;
	enum cmp_direction dir_conn;
	int err;

	if (stream == &bebob->tx_stream) {
		dir_stream = AMDTP_IN_STREAM;
		conn = &bebob->out_conn;
		dir_conn = CMP_OUTPUT;
	} else {
		dir_stream = AMDTP_OUT_STREAM;
		conn = &bebob->in_conn;
		dir_conn = CMP_INPUT;
	}

	if (stream == &bebob->tx_stream) {
		if (bebob->quirks & SND_BEBOB_QUIRK_WRONG_DBC)
			flags |= CIP_EMPTY_HAS_WRONG_DBC;
	}

	err = cmp_connection_init(conn, bebob->unit, dir_conn, 0);
	if (err < 0)
		return err;

	err = amdtp_am824_init(stream, bebob->unit, dir_stream, flags);
	if (err < 0) {
		cmp_connection_destroy(conn);
		return err;
	}

	return 0;
}

static void destroy_stream(struct snd_bebob *bebob, struct amdtp_stream *stream)
{
	amdtp_stream_destroy(stream);

	if (stream == &bebob->tx_stream)
		cmp_connection_destroy(&bebob->out_conn);
	else
		cmp_connection_destroy(&bebob->in_conn);
}

int snd_bebob_stream_init_duplex(struct snd_bebob *bebob)
{
	int err;

	err = init_stream(bebob, &bebob->tx_stream);
	if (err < 0)
		return err;

	err = init_stream(bebob, &bebob->rx_stream);
	if (err < 0) {
		destroy_stream(bebob, &bebob->tx_stream);
		return err;
	}

	err = amdtp_domain_init(&bebob->domain);
	if (err < 0) {
		destroy_stream(bebob, &bebob->tx_stream);
		destroy_stream(bebob, &bebob->rx_stream);
	}

	return err;
}

static int keep_resources(struct snd_bebob *bebob, struct amdtp_stream *stream,
			  unsigned int rate, unsigned int index)
{
	unsigned int pcm_channels;
	unsigned int midi_ports;
	struct cmp_connection *conn;
	int err;

	if (stream == &bebob->tx_stream) {
		pcm_channels = bebob->tx_stream_formations[index].pcm;
		midi_ports = bebob->midi_input_ports;
		conn = &bebob->out_conn;
	} else {
		pcm_channels = bebob->rx_stream_formations[index].pcm;
		midi_ports = bebob->midi_output_ports;
		conn = &bebob->in_conn;
	}

	err = amdtp_am824_set_parameters(stream, rate, pcm_channels, midi_ports, false);
	if (err < 0)
		return err;

	return cmp_connection_reserve(conn, amdtp_stream_get_max_payload(stream));
}

int snd_bebob_stream_reserve_duplex(struct snd_bebob *bebob, unsigned int rate,
				    unsigned int frames_per_period,
				    unsigned int frames_per_buffer)
{
	unsigned int curr_rate;
	int err;

	 
	 
	err = check_connection_used_by_others(bebob, &bebob->rx_stream);
	if (err < 0)
		return err;

	err = bebob->spec->rate->get(bebob, &curr_rate);
	if (err < 0)
		return err;
	if (rate == 0)
		rate = curr_rate;
	if (curr_rate != rate) {
		amdtp_domain_stop(&bebob->domain);
		break_both_connections(bebob);

		cmp_connection_release(&bebob->out_conn);
		cmp_connection_release(&bebob->in_conn);
	}

	if (bebob->substreams_counter == 0 || curr_rate != rate) {
		unsigned int index;

		 
		 
		 
		
		
		err = bebob->spec->rate->set(bebob, rate);
		if (err < 0) {
			dev_err(&bebob->unit->device,
				"fail to set sampling rate: %d\n",
				err);
			return err;
		}

		err = get_formation_index(rate, &index);
		if (err < 0)
			return err;

		err = keep_resources(bebob, &bebob->tx_stream, rate, index);
		if (err < 0)
			return err;

		err = keep_resources(bebob, &bebob->rx_stream, rate, index);
		if (err < 0) {
			cmp_connection_release(&bebob->out_conn);
			return err;
		}

		err = amdtp_domain_set_events_per_period(&bebob->domain,
					frames_per_period, frames_per_buffer);
		if (err < 0) {
			cmp_connection_release(&bebob->out_conn);
			cmp_connection_release(&bebob->in_conn);
			return err;
		}
	}

	return 0;
}

int snd_bebob_stream_start_duplex(struct snd_bebob *bebob)
{
	int err;

	
	if (bebob->substreams_counter == 0)
		return -EIO;

	
	if (amdtp_streaming_error(&bebob->rx_stream) ||
	    amdtp_streaming_error(&bebob->tx_stream)) {
		amdtp_domain_stop(&bebob->domain);
		break_both_connections(bebob);
	}

	if (!amdtp_stream_running(&bebob->rx_stream)) {
		enum snd_bebob_clock_type src;
		unsigned int curr_rate;
		unsigned int tx_init_skip_cycles;

		if (bebob->maudio_special_quirk) {
			err = bebob->spec->rate->get(bebob, &curr_rate);
			if (err < 0)
				return err;
		}

		err = snd_bebob_stream_get_clock_src(bebob, &src);
		if (err < 0)
			return err;

		err = start_stream(bebob, &bebob->rx_stream);
		if (err < 0)
			goto error;

		err = start_stream(bebob, &bebob->tx_stream);
		if (err < 0)
			goto error;

		if (!(bebob->quirks & SND_BEBOB_QUIRK_INITIAL_DISCONTINUOUS_DBC))
			tx_init_skip_cycles = 0;
		else
			tx_init_skip_cycles = 16000;

		
		
		
		
		
		
		
		
		err = amdtp_domain_start(&bebob->domain, tx_init_skip_cycles, true, false);
		if (err < 0)
			goto error;

		
		
		
		if (bebob->maudio_special_quirk) {
			err = bebob->spec->rate->set(bebob, curr_rate);
			if (err < 0) {
				dev_err(&bebob->unit->device,
					"fail to ensure sampling rate: %d\n",
					err);
				goto error;
			}
		}

		
		
		if (!amdtp_domain_wait_ready(&bebob->domain, READY_TIMEOUT_MS)) {
			err = -ETIMEDOUT;
			goto error;
		}
	}

	return 0;
error:
	amdtp_domain_stop(&bebob->domain);
	break_both_connections(bebob);
	return err;
}

void snd_bebob_stream_stop_duplex(struct snd_bebob *bebob)
{
	if (bebob->substreams_counter == 0) {
		amdtp_domain_stop(&bebob->domain);
		break_both_connections(bebob);

		cmp_connection_release(&bebob->out_conn);
		cmp_connection_release(&bebob->in_conn);
	}
}

 
void snd_bebob_stream_destroy_duplex(struct snd_bebob *bebob)
{
	amdtp_domain_destroy(&bebob->domain);

	destroy_stream(bebob, &bebob->tx_stream);
	destroy_stream(bebob, &bebob->rx_stream);
}

 
static int
parse_stream_formation(u8 *buf, unsigned int len,
		       struct snd_bebob_stream_formation *formation)
{
	unsigned int i, e, channels, format;

	 
	if ((buf[0] != 0x90) || (buf[1] != 0x40))
		return -ENOSYS;

	 
	for (i = 0; i < ARRAY_SIZE(bridgeco_freq_table); i++) {
		if (buf[2] == bridgeco_freq_table[i])
			break;
	}
	if (i == ARRAY_SIZE(bridgeco_freq_table))
		return -ENOSYS;

	 
	memset(&formation[i], 0, sizeof(struct snd_bebob_stream_formation));

	for (e = 0; e < buf[4]; e++) {
		channels = buf[5 + e * 2];
		format = buf[6 + e * 2];

		switch (format) {
		 
		case 0x00:
		 
		case 0x06:	 
			formation[i].pcm += channels;
			break;
		 
		case 0x0d:
			formation[i].midi += channels;
			break;
		 
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		 
		case 0x07:	 
		case 0x0c:	 
		 
		case 0x08:	 
		case 0x09:	 
		case 0x0a:	 
		case 0x0b:	 
		 
		case 0x40:
		 
		case 0xff:
		default:
			return -ENOSYS;	 
		}
	}

	if (formation[i].pcm  > AM824_MAX_CHANNELS_FOR_PCM ||
	    formation[i].midi > AM824_MAX_CHANNELS_FOR_MIDI)
		return -ENOSYS;

	return 0;
}

static int fill_stream_formations(struct snd_bebob *bebob, u8 addr[AVC_BRIDGECO_ADDR_BYTES],
				  enum avc_bridgeco_plug_dir plug_dir, unsigned int plug_id,
				  struct snd_bebob_stream_formation *formations)
{
	enum avc_bridgeco_plug_type plug_type;
	u8 *buf;
	unsigned int len, eid;
	int err;

	avc_bridgeco_fill_unit_addr(addr, plug_dir, AVC_BRIDGECO_PLUG_UNIT_ISOC, plug_id);

	err = avc_bridgeco_get_plug_type(bebob->unit, addr, &plug_type);
	if (err < 0) {
		dev_err(&bebob->unit->device,
			"Fail to get type for isoc %d plug 0: %d\n", plug_dir, err);
		return err;
	} else if (plug_type != AVC_BRIDGECO_PLUG_TYPE_ISOC)
		return -ENXIO;

	buf = kmalloc(FORMAT_MAXIMUM_LENGTH, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	for (eid = 0; eid < SND_BEBOB_STRM_FMT_ENTRIES; ++eid) {
		avc_bridgeco_fill_unit_addr(addr, plug_dir, AVC_BRIDGECO_PLUG_UNIT_ISOC, plug_id);

		len = FORMAT_MAXIMUM_LENGTH;
		err = avc_bridgeco_get_plug_strm_fmt(bebob->unit, addr, buf, &len, eid);
		 
		if (err == -EINVAL && eid > 0) {
			err = 0;
			break;
		} else if (err < 0) {
			dev_err(&bebob->unit->device,
				"fail to get stream format %d for isoc %d plug %d:%d\n",
				eid, plug_dir, plug_id, err);
			break;
		}

		err = parse_stream_formation(buf, len, formations);
		if (err < 0)
			break;
	}

	kfree(buf);
	return err;
}

static int detect_midi_ports(struct snd_bebob *bebob,
			     const struct snd_bebob_stream_formation *formats,
			     u8 addr[AVC_BRIDGECO_ADDR_BYTES], enum avc_bridgeco_plug_dir plug_dir,
			     unsigned int plug_count, unsigned int *midi_ports)
{
	int i;
	int err = 0;

	*midi_ports = 0;

	 
	for (i = 0; i < SND_BEBOB_STRM_FMT_ENTRIES; ++i) {
		if (formats[i].midi > 0)
			break;
	}
	if (i >= SND_BEBOB_STRM_FMT_ENTRIES)
		return 0;

	for (i = 0; i < plug_count; ++i) {
		enum avc_bridgeco_plug_type plug_type;
		unsigned int ch_count;

		avc_bridgeco_fill_unit_addr(addr, plug_dir, AVC_BRIDGECO_PLUG_UNIT_EXT, i);

		err = avc_bridgeco_get_plug_type(bebob->unit, addr, &plug_type);
		if (err < 0) {
			dev_err(&bebob->unit->device,
				"fail to get type for external %d plug %d: %d\n",
				plug_dir, i, err);
			break;
		} else if (plug_type != AVC_BRIDGECO_PLUG_TYPE_MIDI) {
			continue;
		}

		err = avc_bridgeco_get_plug_ch_count(bebob->unit, addr, &ch_count);
		if (err < 0)
			break;
		 
		 
		 
		if (ch_count == 0)
			ch_count = 1;
		*midi_ports += ch_count;
	}

	return err;
}

static int
seek_msu_sync_input_plug(struct snd_bebob *bebob)
{
	u8 plugs[AVC_PLUG_INFO_BUF_BYTES], addr[AVC_BRIDGECO_ADDR_BYTES];
	unsigned int i;
	enum avc_bridgeco_plug_type type;
	int err;

	 
	err = avc_general_get_plug_info(bebob->unit, 0x0c, 0x00, 0x00, plugs);
	if (err < 0) {
		dev_err(&bebob->unit->device,
			"fail to get info for MSU in/out plugs: %d\n",
			err);
		goto end;
	}

	 
	bebob->sync_input_plug = -1;
	for (i = 0; i < plugs[0]; i++) {
		avc_bridgeco_fill_msu_addr(addr, AVC_BRIDGECO_PLUG_DIR_IN, i);
		err = avc_bridgeco_get_plug_type(bebob->unit, addr, &type);
		if (err < 0) {
			dev_err(&bebob->unit->device,
				"fail to get type for MSU in plug %d: %d\n",
				i, err);
			goto end;
		}

		if (type == AVC_BRIDGECO_PLUG_TYPE_SYNC) {
			bebob->sync_input_plug = i;
			break;
		}
	}
end:
	return err;
}

int snd_bebob_stream_discover(struct snd_bebob *bebob)
{
	const struct snd_bebob_clock_spec *clk_spec = bebob->spec->clock;
	u8 plugs[AVC_PLUG_INFO_BUF_BYTES], addr[AVC_BRIDGECO_ADDR_BYTES];
	int err;

	 
	err = avc_general_get_plug_info(bebob->unit, 0x1f, 0x07, 0x00, plugs);
	if (err < 0) {
		dev_err(&bebob->unit->device,
		"fail to get info for isoc/external in/out plugs: %d\n",
			err);
		goto end;
	}

	 
	if ((plugs[0] == 0) || (plugs[1] == 0)) {
		err = -ENOSYS;
		goto end;
	}

	err = fill_stream_formations(bebob, addr, AVC_BRIDGECO_PLUG_DIR_IN, 0,
				     bebob->rx_stream_formations);
	if (err < 0)
		goto end;

	err = fill_stream_formations(bebob, addr, AVC_BRIDGECO_PLUG_DIR_OUT, 0,
				     bebob->tx_stream_formations);
	if (err < 0)
		goto end;

	err = detect_midi_ports(bebob, bebob->tx_stream_formations, addr, AVC_BRIDGECO_PLUG_DIR_IN,
				plugs[2], &bebob->midi_input_ports);
	if (err < 0)
		goto end;

	err = detect_midi_ports(bebob, bebob->rx_stream_formations, addr, AVC_BRIDGECO_PLUG_DIR_OUT,
				plugs[3], &bebob->midi_output_ports);
	if (err < 0)
		goto end;

	 
	if (!clk_spec)
		err = seek_msu_sync_input_plug(bebob);
end:
	return err;
}

void snd_bebob_stream_lock_changed(struct snd_bebob *bebob)
{
	bebob->dev_lock_changed = true;
	wake_up(&bebob->hwdep_wait);
}

int snd_bebob_stream_lock_try(struct snd_bebob *bebob)
{
	int err;

	spin_lock_irq(&bebob->lock);

	 
	if (bebob->dev_lock_count < 0) {
		err = -EBUSY;
		goto end;
	}

	 
	if (bebob->dev_lock_count++ == 0)
		snd_bebob_stream_lock_changed(bebob);
	err = 0;
end:
	spin_unlock_irq(&bebob->lock);
	return err;
}

void snd_bebob_stream_lock_release(struct snd_bebob *bebob)
{
	spin_lock_irq(&bebob->lock);

	if (WARN_ON(bebob->dev_lock_count <= 0))
		goto end;
	if (--bebob->dev_lock_count == 0)
		snd_bebob_stream_lock_changed(bebob);
end:
	spin_unlock_irq(&bebob->lock);
}
