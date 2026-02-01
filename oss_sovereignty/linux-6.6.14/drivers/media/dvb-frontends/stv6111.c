
 

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <asm/div64.h>

#include "stv6111.h"

#include <media/dvb_frontend.h>

struct stv {
	struct i2c_adapter *i2c;
	u8 adr;

	u8 reg[11];
	u32 ref_freq;
	u32 frequency;
};

struct slookup {
	s16 value;
	u16 reg_value;
};

static const struct slookup lnagain_nf_lookup[] = {
	 
static const struct slookup gain_channel_agc_nf_lookup[] = {
	 

	state->reg[0x03] = (state->reg[0x03] & ~0x80) | (psel << 7);
	state->reg[0x04] = (div & 0xFF);
	state->reg[0x05] = (((div >> 8) & 0x01) | ((frac & 0x7F) << 1)) & 0xff;
	state->reg[0x06] = ((frac >> 7) & 0xFF);
	state->reg[0x07] = (state->reg[0x07] & ~0x07) | ((frac >> 15) & 0x07);
	state->reg[0x07] = (state->reg[0x07] & ~0xE0) | (icp << 5);

	state->reg[0x08] = (state->reg[0x08] & ~0xFC) | ((index - 6) << 2);
	 
	state->reg[0x09] = (state->reg[0x09] & ~0x0C) | 0x0C;
	write_regs(state, 2, 8);

	wait_for_call_done(state, 0x0C);

	usleep_range(10000, 12000);

	read_reg(state, 0x03, &tmp);
	if (tmp & 0x10)	{
		state->reg[0x02] &= ~0x80;  
		write_regs(state, 2, 1);
	}
	read_reg(state, 0x08, &tmp);

	state->frequency = frequency;

	return 0;
}

static int set_params(struct dvb_frontend *fe)
{
	struct stv *state = fe->tuner_priv;
	struct dtv_frontend_properties *p = &fe->dtv_property_cache;
	u32 freq, cutoff;
	int stat = 0;

	if (p->delivery_system != SYS_DVBS && p->delivery_system != SYS_DVBS2)
		return -EINVAL;

	freq = p->frequency * 1000;
	cutoff = 5000000 + muldiv32(p->symbol_rate, 135, 200);

	if (fe->ops.i2c_gate_ctrl)
		stat = fe->ops.i2c_gate_ctrl(fe, 1);
	if (!stat)
		set_lof(state, freq, cutoff);
	if (fe->ops.i2c_gate_ctrl && !stat)
		fe->ops.i2c_gate_ctrl(fe, 0);
	return 0;
}

static s32 table_lookup(const struct slookup *table,
			int table_size, u16 reg_value)
{
	s32 gain;
	s32 reg_diff;
	int imin = 0;
	int imax = table_size - 1;
	int i;

	 
	if (reg_value <= table[0].reg_value) {
		gain = table[0].value;
	} else if (reg_value >= table[imax].reg_value) {
		gain = table[imax].value;
	} else {
		while ((imax - imin) > 1) {
			i = (imax + imin) / 2;
			if ((table[imin].reg_value <= reg_value) &&
			    (reg_value <= table[i].reg_value))
				imax = i;
			else
				imin = i;
		}
		reg_diff = table[imax].reg_value - table[imin].reg_value;
		gain = table[imin].value;
		if (reg_diff != 0)
			gain += ((s32)(reg_value - table[imin].reg_value) *
				(s32)(table[imax].value
				- table[imin].value)) / reg_diff;
	}
	return gain;
}

static int get_rf_strength(struct dvb_frontend *fe, u16 *st)
{
	struct stv *state = fe->tuner_priv;
	u16 rfagc = *st;
	s32 gain;

	if ((state->reg[0x03] & 0x60) == 0) {
		 
		u8 reg = 0;
		int stat = 0;

		if (fe->ops.i2c_gate_ctrl)
			stat = fe->ops.i2c_gate_ctrl(fe, 1);
		if (!stat) {
			write_reg(state, 0x02, state->reg[0x02] | 0x20);
			read_reg(state, 2, &reg);
			if (reg & 0x20)
				read_reg(state, 2, &reg);
		}
		if (fe->ops.i2c_gate_ctrl && !stat)
			fe->ops.i2c_gate_ctrl(fe, 0);

		if ((state->reg[0x02] & 0x80) == 0)
			 
			gain = table_lookup(lnagain_nf_lookup,
					    ARRAY_SIZE(lnagain_nf_lookup),
					    reg & 0x1F);
		else
			 
			gain = table_lookup(lnagain_iip3_lookup,
					    ARRAY_SIZE(lnagain_iip3_lookup),
					    reg & 0x1F);

		gain += table_lookup(gain_rfagc_lookup,
				     ARRAY_SIZE(gain_rfagc_lookup), rfagc);

		gain -= 2400;
	} else {
		 
		if ((state->reg[0x02] & 0x80) == 0) {
			 
			gain = table_lookup(
				gain_channel_agc_nf_lookup,
				ARRAY_SIZE(gain_channel_agc_nf_lookup), rfagc);

			gain += 600;
		} else {
			 
			gain = table_lookup(
				gain_channel_agc_iip3_lookup,
				ARRAY_SIZE(gain_channel_agc_iip3_lookup),
				rfagc);
		}
	}

	if (state->frequency > 0)
		 
		gain -= ((((s32)(state->frequency / 1000) - 1550) * 2) / 12);

	 
	gain +=  (s32)((state->reg[0x01] & 0xC0) >> 6) * 600 - 1300;

	if (gain < 0)
		gain = 0;
	else if (gain > 10000)
		gain = 10000;

	*st = 10000 - gain;

	return 0;
}

static const struct dvb_tuner_ops tuner_ops = {
	.info = {
		.name		= "ST STV6111",
		.frequency_min_hz =  950 * MHz,
		.frequency_max_hz = 2150 * MHz,
	},
	.set_params		= set_params,
	.release		= release,
	.get_rf_strength	= get_rf_strength,
	.set_bandwidth		= set_bandwidth,
};

struct dvb_frontend *stv6111_attach(struct dvb_frontend *fe,
				    struct i2c_adapter *i2c, u8 adr)
{
	struct stv *state;
	int stat = -ENODEV;
	int gatestat = 0;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;
	state->adr = adr;
	state->i2c = i2c;
	memcpy(&fe->ops.tuner_ops, &tuner_ops, sizeof(struct dvb_tuner_ops));
	init_state(state);

	if (fe->ops.i2c_gate_ctrl)
		gatestat = fe->ops.i2c_gate_ctrl(fe, 1);
	if (!gatestat)
		stat = attach_init(state);
	if (fe->ops.i2c_gate_ctrl && !gatestat)
		fe->ops.i2c_gate_ctrl(fe, 0);
	if (stat < 0) {
		kfree(state);
		return NULL;
	}
	fe->tuner_priv = state;
	return fe;
}
EXPORT_SYMBOL_GPL(stv6111_attach);

MODULE_DESCRIPTION("ST STV6111 satellite tuner driver");
MODULE_AUTHOR("Ralph Metzler, Manfred Voelkel");
MODULE_LICENSE("GPL v2");
