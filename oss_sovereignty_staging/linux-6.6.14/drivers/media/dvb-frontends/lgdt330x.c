
 

 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/byteorder.h>

#include <media/dvb_frontend.h>
#include <linux/int_log.h>
#include "lgdt330x_priv.h"
#include "lgdt330x.h"

 
 

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Turn on/off lgdt330x frontend debugging (default:off).");

#define dprintk(state, fmt, arg...) do {				\
	if (debug)							\
		dev_printk(KERN_DEBUG, &state->client->dev, fmt, ##arg);\
} while (0)

struct lgdt330x_state {
	struct i2c_client *client;

	 
	struct lgdt330x_config config;

	struct dvb_frontend frontend;

	 
	enum fe_modulation current_modulation;
	u32 snr;	 
	u16 ucblocks;
	unsigned long last_stats_time;

	 
	u32 current_frequency;
};

static int i2c_write_demod_bytes(struct lgdt330x_state *state,
				 const u8 *buf,  
				 int len   )
{
	int i;
	int err;

	for (i = 0; i < len - 1; i += 2) {
		err = i2c_master_send(state->client, buf, 2);
		if (err != 2) {
			dev_warn(&state->client->dev,
				 "%s: error (addr %02x <- %02x, err = %i)\n",
				__func__, buf[0], buf[1], err);
			if (err < 0)
				return err;
			else
				return -EREMOTEIO;
		}
		buf += 2;
	}
	return 0;
}

 
static int i2c_read_demod_bytes(struct lgdt330x_state *state,
				enum I2C_REG reg, u8 *buf, int len)
{
	u8 wr[] = { reg };
	struct i2c_msg msg[] = {
		{
			.addr = state->client->addr,
			.flags = 0,
			.buf = wr,
			.len = 1
		}, {
			.addr = state->client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = len
		},
	};
	int ret;

	ret = i2c_transfer(state->client->adapter, msg, 2);
	if (ret != 2) {
		dev_warn(&state->client->dev,
			 "%s: addr 0x%02x select 0x%02x error (ret == %i)\n",
			 __func__, state->client->addr, reg, ret);
		if (ret >= 0)
			ret = -EIO;
	} else {
		ret = 0;
	}
	return ret;
}

 
static int lgdt3302_sw_reset(struct lgdt330x_state *state)
{
	u8 ret;
	u8 reset[] = {
		IRQ_MASK,
		 
		0x00
	};

	ret = i2c_write_demod_bytes(state,
				    reset, sizeof(reset));
	if (ret == 0) {
		 
		reset[1] = 0x7f;
		ret = i2c_write_demod_bytes(state,
					    reset, sizeof(reset));
	}
	return ret;
}

static int lgdt3303_sw_reset(struct lgdt330x_state *state)
{
	u8 ret;
	u8 reset[] = {
		0x02,
		0x00  
	};

	ret = i2c_write_demod_bytes(state,
				    reset, sizeof(reset));
	if (ret == 0) {
		 
		reset[1] = 0x01;
		ret = i2c_write_demod_bytes(state,
					    reset, sizeof(reset));
	}
	return ret;
}

static int lgdt330x_sw_reset(struct lgdt330x_state *state)
{
	switch (state->config.demod_chip) {
	case LGDT3302:
		return lgdt3302_sw_reset(state);
	case LGDT3303:
		return lgdt3303_sw_reset(state);
	default:
		return -ENODEV;
	}
}

static int lgdt330x_init(struct dvb_frontend *fe)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	char  *chip_name;
	int    err;
	 
	static const u8 lgdt3302_init_data[] = {
		 
		 
		VSB_CARRIER_FREQ0, 0x00,
		VSB_CARRIER_FREQ1, 0x87,
		VSB_CARRIER_FREQ2, 0x8e,
		VSB_CARRIER_FREQ3, 0x01,
		 
		DEMUX_CONTROL, 0xfb,
		 
		AGC_RF_BANDWIDTH0, 0x40,
		AGC_RF_BANDWIDTH1, 0x93,
		AGC_RF_BANDWIDTH2, 0x00,
		 
		AGC_FUNC_CTRL2, 0xc6,
		 
		AGC_FUNC_CTRL3, 0x40,
		 
		AGC_DELAY0, 0x07,
		AGC_DELAY2, 0xfe,
		 
		AGC_LOOP_BANDWIDTH0, 0x08,
		AGC_LOOP_BANDWIDTH1, 0x9a
	};
	static const u8 lgdt3303_init_data[] = {
		0x4c, 0x14
	};
	static const u8 flip_1_lgdt3303_init_data[] = {
		0x4c, 0x14,
		0x87, 0xf3
	};
	static const u8 flip_2_lgdt3303_init_data[] = {
		0x4c, 0x14,
		0x87, 0xda
	};

	 

	switch (state->config.demod_chip) {
	case LGDT3302:
		chip_name = "LGDT3302";
		err = i2c_write_demod_bytes(state, lgdt3302_init_data,
					    sizeof(lgdt3302_init_data));
		break;
	case LGDT3303:
		chip_name = "LGDT3303";
		switch (state->config.clock_polarity_flip) {
		case 2:
			err = i2c_write_demod_bytes(state,
						    flip_2_lgdt3303_init_data,
						    sizeof(flip_2_lgdt3303_init_data));
			break;
		case 1:
			err = i2c_write_demod_bytes(state,
						    flip_1_lgdt3303_init_data,
						    sizeof(flip_1_lgdt3303_init_data));
			break;
		case 0:
		default:
			err = i2c_write_demod_bytes(state, lgdt3303_init_data,
						    sizeof(lgdt3303_init_data));
		}
		break;
	default:
		chip_name = "undefined";
		dev_warn(&state->client->dev,
			 "Only LGDT3302 and LGDT3303 are supported chips.\n");
		err = -ENODEV;
	}
	dprintk(state, "Initialized the %s chip\n", chip_name);
	if (err < 0)
		return err;

	p->cnr.len = 1;
	p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	p->block_error.len = 1;
	p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	p->block_count.len = 1;
	p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	state->last_stats_time = 0;

	return lgdt330x_sw_reset(state);
}

static int lgdt330x_read_ucblocks(struct dvb_frontend *fe, u32 *ucblocks)
{
	struct lgdt330x_state *state = fe->demodulator_priv;

	*ucblocks = state->ucblocks;

	return 0;
}

static int lgdt330x_set_parameters(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	struct lgdt330x_state *state = fe->demodulator_priv;
	 
	static const u8 lgdt3303_8vsb_44_data[] = {
		0x04, 0x00,
		0x0d, 0x40,
		0x0e, 0x87,
		0x0f, 0x8e,
		0x10, 0x01,
		0x47, 0x8b
	};
	 
	static const u8 lgdt3303_qam_data[] = {
		0x04, 0x00,
		0x0d, 0x00,
		0x0e, 0x00,
		0x0f, 0x00,
		0x10, 0x00,
		0x51, 0x63,
		0x47, 0x66,
		0x48, 0x66,
		0x4d, 0x1a,
		0x49, 0x08,
		0x4a, 0x9b
	};
	u8 top_ctrl_cfg[]   = { TOP_CONTROL, 0x03 };

	int err = 0;
	 
	if (state->current_modulation != p->modulation) {
		switch (p->modulation) {
		case VSB_8:
			dprintk(state, "VSB_8 MODE\n");

			 
			top_ctrl_cfg[1] = 0x03;

			 
			if (state->config.pll_rf_set)
				state->config.pll_rf_set(fe, 1);

			if (state->config.demod_chip == LGDT3303) {
				err = i2c_write_demod_bytes(state,
							    lgdt3303_8vsb_44_data,
							    sizeof(lgdt3303_8vsb_44_data));
			}
			break;

		case QAM_64:
			dprintk(state, "QAM_64 MODE\n");

			 
			top_ctrl_cfg[1] = 0x00;

			 
			if (state->config.pll_rf_set)
				state->config.pll_rf_set(fe, 0);

			if (state->config.demod_chip == LGDT3303) {
				err = i2c_write_demod_bytes(state,
							    lgdt3303_qam_data,
							    sizeof(lgdt3303_qam_data));
			}
			break;

		case QAM_256:
			dprintk(state, "QAM_256 MODE\n");

			 
			top_ctrl_cfg[1] = 0x01;

			 
			if (state->config.pll_rf_set)
				state->config.pll_rf_set(fe, 0);

			if (state->config.demod_chip == LGDT3303) {
				err = i2c_write_demod_bytes(state,
							    lgdt3303_qam_data,
							    sizeof(lgdt3303_qam_data));
			}
			break;
		default:
			dev_warn(&state->client->dev,
				 "%s: Modulation type(%d) UNSUPPORTED\n",
				 __func__, p->modulation);
			return -1;
		}
		if (err < 0)
			dev_warn(&state->client->dev,
				 "%s: error blasting bytes to lgdt3303 for modulation type(%d)\n",
				 __func__, p->modulation);

		 
		top_ctrl_cfg[1] |= state->config.serial_mpeg;

		 
		i2c_write_demod_bytes(state, top_ctrl_cfg,
				      sizeof(top_ctrl_cfg));
		if (state->config.set_ts_params)
			state->config.set_ts_params(fe, 0);
		state->current_modulation = p->modulation;
	}

	 
	if (fe->ops.tuner_ops.set_params) {
		fe->ops.tuner_ops.set_params(fe);
		if (fe->ops.i2c_gate_ctrl)
			fe->ops.i2c_gate_ctrl(fe, 0);
	}

	 
	 
	state->current_frequency = p->frequency;

	lgdt330x_sw_reset(state);
	return 0;
}

static int lgdt330x_get_frontend(struct dvb_frontend *fe,
				 struct dtv_frontend_properties *p)
{
	struct lgdt330x_state *state = fe->demodulator_priv;

	p->frequency = state->current_frequency;
	return 0;
}

 
static u32 calculate_snr(u32 mse, u32 c)
{
	if (mse == 0)  
		return 0;

	mse = intlog10(mse);
	if (mse > c) {
		 
		return 0;
	}
	return 10 * (c - mse);
}

static int lgdt3302_read_snr(struct dvb_frontend *fe)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	u8 buf[5];	 
	u32 noise;	 
	u32 c;		 

	switch (state->current_modulation) {
	case VSB_8:
		i2c_read_demod_bytes(state, LGDT3302_EQPH_ERR0, buf, 5);
#ifdef USE_EQMSE
		 
		 
		noise = ((buf[0] & 7) << 16) | (buf[1] << 8) | buf[2];
		c = 69765745;  
#else
		 
		 
		noise = ((buf[0] & 7 << 3) << 13) | (buf[3] << 8) | buf[4];
		c = 73957994;  
#endif
		break;
	case QAM_64:
	case QAM_256:
		i2c_read_demod_bytes(state, CARRIER_MSEQAM1, buf, 2);
		noise = ((buf[0] & 3) << 8) | buf[1];
		c = state->current_modulation == QAM_64 ? 97939837 : 98026066;
		 
		break;
	default:
		dev_err(&state->client->dev,
			"%s: Modulation set to unsupported value\n",
			__func__);

		state->snr = 0;

		return -EREMOTEIO;  
	}

	state->snr = calculate_snr(noise, c);

	dprintk(state, "noise = 0x%08x, snr = %d.%02d dB\n", noise,
		state->snr >> 24, (((state->snr >> 8) & 0xffff) * 100) >> 16);

	return 0;
}

static int lgdt3303_read_snr(struct dvb_frontend *fe)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	u8 buf[5];	 
	u32 noise;	 
	u32 c;		 

	switch (state->current_modulation) {
	case VSB_8:
		i2c_read_demod_bytes(state, LGDT3303_EQPH_ERR0, buf, 5);
#ifdef USE_EQMSE
		 
		 
		noise = ((buf[0] & 0x78) << 13) | (buf[1] << 8) | buf[2];
		c = 73957994;  
#else
		 
		 
		noise = ((buf[0] & 7) << 16) | (buf[3] << 8) | buf[4];
		c = 73957994;  
#endif
		break;
	case QAM_64:
	case QAM_256:
		i2c_read_demod_bytes(state, CARRIER_MSEQAM1, buf, 2);
		noise = (buf[0] << 8) | buf[1];
		c = state->current_modulation == QAM_64 ? 97939837 : 98026066;
		 
		break;
	default:
		dev_err(&state->client->dev,
			"%s: Modulation set to unsupported value\n",
			__func__);
		state->snr = 0;
		return -EREMOTEIO;  
	}

	state->snr = calculate_snr(noise, c);

	dprintk(state, "noise = 0x%08x, snr = %d.%02d dB\n", noise,
		state->snr >> 24, (((state->snr >> 8) & 0xffff) * 100) >> 16);

	return 0;
}

static int lgdt330x_read_snr(struct dvb_frontend *fe, u16 *snr)
{
	struct lgdt330x_state *state = fe->demodulator_priv;

	*snr = (state->snr) >> 16;  

	return 0;
}

static int lgdt330x_read_signal_strength(struct dvb_frontend *fe, u16 *strength)
{
	 
	 
	struct lgdt330x_state *state = fe->demodulator_priv;
	u16 snr;
	int ret;

	ret = fe->ops.read_snr(fe, &snr);
	if (ret != 0)
		return ret;
	 
	 
	if (state->snr >= 8960 * 0x10000)
		*strength = 0xffff;
	else
		*strength = state->snr / 8960;

	return 0;
}


static int lgdt3302_read_status(struct dvb_frontend *fe,
				enum fe_status *status)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u8 buf[3];
	int err;

	*status = 0;  

	 
	i2c_read_demod_bytes(state, AGC_STATUS, buf, 1);
	dprintk(state, "AGC_STATUS = 0x%02x\n", buf[0]);
	if ((buf[0] & 0x0c) == 0x8) {
		 
		*status |= FE_HAS_SIGNAL;
	}

	 

	 
	i2c_read_demod_bytes(state, TOP_CONTROL, buf, sizeof(buf));
	dprintk(state,
		"TOP_CONTROL = 0x%02x, IRO_MASK = 0x%02x, IRQ_STATUS = 0x%02x\n",
		buf[0], buf[1], buf[2]);

	 
	if ((buf[2] & 0x03) == 0x01)
		*status |= FE_HAS_SYNC;

	 
	if ((buf[2] & 0x0c) == 0x08)
		*status |= FE_HAS_LOCK | FE_HAS_VITERBI;

	 
	i2c_read_demod_bytes(state, CARRIER_LOCK, buf, 1);
	dprintk(state, "CARRIER_LOCK = 0x%02x\n", buf[0]);
	switch (state->current_modulation) {
	case QAM_256:
	case QAM_64:
		 
		if ((buf[0] & 0x07) == 0x07)
			*status |= FE_HAS_CARRIER;
		break;
	case VSB_8:
		if ((buf[0] & 0x80) == 0x80)
			*status |= FE_HAS_CARRIER;
		break;
	default:
		dev_warn(&state->client->dev,
			 "%s: Modulation set to unsupported value\n",
			 __func__);
	}

	if (!(*status & FE_HAS_LOCK)) {
		p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		return 0;
	}

	if (state->last_stats_time &&
	    time_is_after_jiffies(state->last_stats_time))
		return 0;

	state->last_stats_time = jiffies + msecs_to_jiffies(1000);

	err = lgdt3302_read_snr(fe);
	if (!err) {
		p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		p->cnr.stat[0].svalue = (((u64)state->snr) * 1000) >> 24;
	} else {
		p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}

	err = i2c_read_demod_bytes(state, LGDT3302_PACKET_ERR_COUNTER1,
					   buf, sizeof(buf));
	if (!err) {
		state->ucblocks = (buf[0] << 8) | buf[1];

		dprintk(state, "UCB = 0x%02x\n", state->ucblocks);

		p->block_error.stat[0].uvalue += state->ucblocks;
		 
		p->block_count.stat[0].uvalue += 10000;

		p->block_error.stat[0].scale = FE_SCALE_COUNTER;
		p->block_count.stat[0].scale = FE_SCALE_COUNTER;
	} else {
		p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}

	return 0;
}

static int lgdt3303_read_status(struct dvb_frontend *fe,
				enum fe_status *status)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u8 buf[3];
	int err;

	*status = 0;  

	 
	err = i2c_read_demod_bytes(state, 0x58, buf, 1);
	if (err < 0)
		return err;

	dprintk(state, "AGC_STATUS = 0x%02x\n", buf[0]);
	if ((buf[0] & 0x21) == 0x01) {
		 
		*status |= FE_HAS_SIGNAL;
	}

	 
	i2c_read_demod_bytes(state, CARRIER_LOCK, buf, 1);
	dprintk(state, "CARRIER_LOCK = 0x%02x\n", buf[0]);
	switch (state->current_modulation) {
	case QAM_256:
	case QAM_64:
		 
		if ((buf[0] & 0x07) == 0x07)
			*status |= FE_HAS_CARRIER;
		else
			break;
		i2c_read_demod_bytes(state, 0x8a, buf, 1);
		dprintk(state, "QAM LOCK = 0x%02x\n", buf[0]);

		if ((buf[0] & 0x04) == 0x04)
			*status |= FE_HAS_SYNC;
		if ((buf[0] & 0x01) == 0x01)
			*status |= FE_HAS_LOCK;
		if ((buf[0] & 0x08) == 0x08)
			*status |= FE_HAS_VITERBI;
		break;
	case VSB_8:
		if ((buf[0] & 0x80) == 0x80)
			*status |= FE_HAS_CARRIER;
		else
			break;
		i2c_read_demod_bytes(state, 0x38, buf, 1);
		dprintk(state, "8-VSB LOCK = 0x%02x\n", buf[0]);

		if ((buf[0] & 0x02) == 0x00)
			*status |= FE_HAS_SYNC;
		if ((buf[0] & 0x01) == 0x01)
			*status |= FE_HAS_VITERBI | FE_HAS_LOCK;
		break;
	default:
		dev_warn(&state->client->dev,
			 "%s: Modulation set to unsupported value\n",
			 __func__);
	}

	if (!(*status & FE_HAS_LOCK)) {
		p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		return 0;
	}

	if (state->last_stats_time &&
	    time_is_after_jiffies(state->last_stats_time))
		return 0;

	state->last_stats_time = jiffies + msecs_to_jiffies(1000);

	err = lgdt3303_read_snr(fe);
	if (!err) {
		p->cnr.stat[0].scale = FE_SCALE_DECIBEL;
		p->cnr.stat[0].svalue = (((u64)state->snr) * 1000) >> 24;
	} else {
		p->cnr.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}

	err = i2c_read_demod_bytes(state, LGDT3303_PACKET_ERR_COUNTER1,
					   buf, sizeof(buf));
	if (!err) {
		state->ucblocks = (buf[0] << 8) | buf[1];

		dprintk(state, "UCB = 0x%02x\n", state->ucblocks);

		p->block_error.stat[0].uvalue += state->ucblocks;
		 
		p->block_count.stat[0].uvalue += 10000;

		p->block_error.stat[0].scale = FE_SCALE_COUNTER;
		p->block_count.stat[0].scale = FE_SCALE_COUNTER;
	} else {
		p->block_error.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
		p->block_count.stat[0].scale = FE_SCALE_NOT_AVAILABLE;
	}

	return 0;
}

static int
lgdt330x_get_tune_settings(struct dvb_frontend *fe,
			   struct dvb_frontend_tune_settings *fe_tune_settings)
{
	 
	fe_tune_settings->min_delay_ms = 500;
	fe_tune_settings->step_size = 0;
	fe_tune_settings->max_drift = 0;
	return 0;
}

static void lgdt330x_release(struct dvb_frontend *fe)
{
	struct lgdt330x_state *state = fe->demodulator_priv;
	struct i2c_client *client = state->client;

	dev_dbg(&client->dev, "\n");

	i2c_unregister_device(client);
}

static struct dvb_frontend *lgdt330x_get_dvb_frontend(struct i2c_client *client)
{
	struct lgdt330x_state *state = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "\n");

	return &state->frontend;
}

static const struct dvb_frontend_ops lgdt3302_ops;
static const struct dvb_frontend_ops lgdt3303_ops;

static int lgdt330x_probe(struct i2c_client *client)
{
	struct lgdt330x_state *state = NULL;
	u8 buf[1];

	 
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		goto error;

	 
	memcpy(&state->config, client->dev.platform_data,
	       sizeof(state->config));
	i2c_set_clientdata(client, state);
	state->client = client;

	 
	switch (state->config.demod_chip) {
	case LGDT3302:
		memcpy(&state->frontend.ops, &lgdt3302_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	case LGDT3303:
		memcpy(&state->frontend.ops, &lgdt3303_ops,
		       sizeof(struct dvb_frontend_ops));
		break;
	default:
		goto error;
	}
	state->frontend.demodulator_priv = state;

	 
	state->config.get_dvb_frontend = lgdt330x_get_dvb_frontend;

	 
	if (i2c_read_demod_bytes(state, 2, buf, 1))
		goto error;

	state->current_frequency = -1;
	state->current_modulation = -1;

	dev_info(&state->client->dev,
		"Demod loaded for LGDT330%s chip\n",
		state->config.demod_chip == LGDT3302 ? "2" : "3");

	return 0;

error:
	kfree(state);
	if (debug)
		dev_printk(KERN_DEBUG, &client->dev, "Error loading lgdt330x driver\n");
	return -ENODEV;
}
struct dvb_frontend *lgdt330x_attach(const struct lgdt330x_config *_config,
				     u8 demod_address,
				     struct i2c_adapter *i2c)
{
	struct i2c_client *client;
	struct i2c_board_info board_info = {};
	struct lgdt330x_config config = *_config;

	strscpy(board_info.type, "lgdt330x", sizeof(board_info.type));
	board_info.addr = demod_address;
	board_info.platform_data = &config;
	client = i2c_new_client_device(i2c, &board_info);
	if (!i2c_client_has_driver(client))
		return NULL;

	return lgdt330x_get_dvb_frontend(client);
}
EXPORT_SYMBOL_GPL(lgdt330x_attach);

static const struct dvb_frontend_ops lgdt3302_ops = {
	.delsys = { SYS_ATSC, SYS_DVBC_ANNEX_B },
	.info = {
		.name = "LG Electronics LGDT3302 VSB/QAM Frontend",
		.frequency_min_hz =  54 * MHz,
		.frequency_max_hz = 858 * MHz,
		.frequency_stepsize_hz = 62500,
		.symbol_rate_min    = 5056941,	 
		.symbol_rate_max    = 10762000,	 
		.caps = FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_8VSB
	},
	.init                 = lgdt330x_init,
	.set_frontend         = lgdt330x_set_parameters,
	.get_frontend         = lgdt330x_get_frontend,
	.get_tune_settings    = lgdt330x_get_tune_settings,
	.read_status          = lgdt3302_read_status,
	.read_signal_strength = lgdt330x_read_signal_strength,
	.read_snr             = lgdt330x_read_snr,
	.read_ucblocks        = lgdt330x_read_ucblocks,
	.release              = lgdt330x_release,
};

static const struct dvb_frontend_ops lgdt3303_ops = {
	.delsys = { SYS_ATSC, SYS_DVBC_ANNEX_B },
	.info = {
		.name = "LG Electronics LGDT3303 VSB/QAM Frontend",
		.frequency_min_hz =  54 * MHz,
		.frequency_max_hz = 858 * MHz,
		.frequency_stepsize_hz = 62500,
		.symbol_rate_min    = 5056941,	 
		.symbol_rate_max    = 10762000,	 
		.caps = FE_CAN_QAM_64 | FE_CAN_QAM_256 | FE_CAN_8VSB
	},
	.init                 = lgdt330x_init,
	.set_frontend         = lgdt330x_set_parameters,
	.get_frontend         = lgdt330x_get_frontend,
	.get_tune_settings    = lgdt330x_get_tune_settings,
	.read_status          = lgdt3303_read_status,
	.read_signal_strength = lgdt330x_read_signal_strength,
	.read_snr             = lgdt330x_read_snr,
	.read_ucblocks        = lgdt330x_read_ucblocks,
	.release              = lgdt330x_release,
};

static void lgdt330x_remove(struct i2c_client *client)
{
	struct lgdt330x_state *state = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "\n");

	kfree(state);
}

static const struct i2c_device_id lgdt330x_id_table[] = {
	{"lgdt330x", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, lgdt330x_id_table);

static struct i2c_driver lgdt330x_driver = {
	.driver = {
		.name	= "lgdt330x",
		.suppress_bind_attrs = true,
	},
	.probe		= lgdt330x_probe,
	.remove		= lgdt330x_remove,
	.id_table	= lgdt330x_id_table,
};

module_i2c_driver(lgdt330x_driver);


MODULE_DESCRIPTION("LGDT330X (ATSC 8VSB & ITU-T J.83 AnnexB 64/256 QAM) Demodulator Driver");
MODULE_AUTHOR("Wilson Michaels");
MODULE_LICENSE("GPL");
