
 

 

#include <linux/kernel.h>
#include <linux/module.h>
#include <media/dvb_frontend.h>
#include "qm1d1b0004.h"

 

#define QM1D1B0004_PSC_MASK (1 << 4)

#define QM1D1B0004_XTL_FREQ 4000
#define QM1D1B0004_LPF_FALLBACK 30000

#if 0  
static const struct qm1d1b0004_config default_cfg = {
	.lpf_freq = QM1D1B0004_CFG_LPF_DFLT,
	.half_step = false,
};
#endif

struct qm1d1b0004_state {
	struct qm1d1b0004_config cfg;
	struct i2c_client *i2c;
};


struct qm1d1b0004_cb_map {
	u32 frequency;
	u8 cb;
};

static const struct qm1d1b0004_cb_map cb_maps[] = {
	{  986000, 0xb2 },
	{ 1072000, 0xd2 },
	{ 1154000, 0xe2 },
	{ 1291000, 0x20 },
	{ 1447000, 0x40 },
	{ 1615000, 0x60 },
	{ 1791000, 0x80 },
	{ 1972000, 0xa0 },
};

static u8 lookup_cb(u32 frequency)
{
	int i;
	const struct qm1d1b0004_cb_map *map;

	for (i = 0; i < ARRAY_SIZE(cb_maps); i++) {
		map = &cb_maps[i];
		if (frequency < map->frequency)
			return map->cb;
	}
	return 0xc0;
}

static int qm1d1b0004_set_params(struct dvb_frontend *fe)
{
	struct qm1d1b0004_state *state;
	u32 frequency, pll, lpf_freq;
	u16 word;
	u8 buf[4], cb, lpf;
	int ret;

	state = fe->tuner_priv;
	frequency = fe->dtv_property_cache.frequency;

	pll = QM1D1B0004_XTL_FREQ / 4;
	if (state->cfg.half_step)
		pll /= 2;
	word = DIV_ROUND_CLOSEST(frequency, pll);
	cb = lookup_cb(frequency);
	if (cb & QM1D1B0004_PSC_MASK)
		word = (word << 1 & ~0x1f) | (word & 0x0f);

	 
	buf[0] = 0x40 | word >> 8;
	buf[1] = word;
	 
	buf[2] = 0xe0 | state->cfg.half_step;
	buf[3] = cb;
	ret = i2c_master_send(state->i2c, buf, 4);
	if (ret < 0)
		return ret;

	 
	buf[0] = 0xe4 | state->cfg.half_step;
	ret = i2c_master_send(state->i2c, buf, 1);
	if (ret < 0)
		return ret;
	msleep(20);

	 
	lpf_freq = state->cfg.lpf_freq;
	if (lpf_freq == QM1D1B0004_CFG_LPF_DFLT)
		lpf_freq = fe->dtv_property_cache.symbol_rate / 1000;
	if (lpf_freq == 0)
		lpf_freq = QM1D1B0004_LPF_FALLBACK;
	lpf = DIV_ROUND_UP(lpf_freq, 2000) - 2;
	buf[0] = 0xe4 | ((lpf & 0x0c) << 1) | state->cfg.half_step;
	buf[1] = cb | ((lpf & 0x03) << 2);
	ret = i2c_master_send(state->i2c, buf, 2);
	if (ret < 0)
		return ret;

	 
	buf[0] = 0;
	ret = i2c_master_recv(state->i2c, buf, 1);
	if (ret < 0)
		return ret;
	return 0;
}


static int qm1d1b0004_set_config(struct dvb_frontend *fe, void *priv_cfg)
{
	struct qm1d1b0004_state *state;

	state = fe->tuner_priv;
	memcpy(&state->cfg, priv_cfg, sizeof(state->cfg));
	return 0;
}


static int qm1d1b0004_init(struct dvb_frontend *fe)
{
	struct qm1d1b0004_state *state;
	u8 buf[2] = {0xf8, 0x04};

	state = fe->tuner_priv;
	if (state->cfg.half_step)
		buf[0] |= 0x01;

	return i2c_master_send(state->i2c, buf, 2);
}


static const struct dvb_tuner_ops qm1d1b0004_ops = {
	.info = {
		.name = "Sharp qm1d1b0004",

		.frequency_min_hz =  950 * MHz,
		.frequency_max_hz = 2150 * MHz,
	},

	.init = qm1d1b0004_init,

	.set_params = qm1d1b0004_set_params,
	.set_config = qm1d1b0004_set_config,
};

static int
qm1d1b0004_probe(struct i2c_client *client)
{
	struct dvb_frontend *fe;
	struct qm1d1b0004_config *cfg;
	struct qm1d1b0004_state *state;
	int ret;

	cfg = client->dev.platform_data;
	fe = cfg->fe;
	i2c_set_clientdata(client, fe);

	fe->tuner_priv = kzalloc(sizeof(struct qm1d1b0004_state), GFP_KERNEL);
	if (!fe->tuner_priv) {
		ret = -ENOMEM;
		goto err_mem;
	}

	memcpy(&fe->ops.tuner_ops, &qm1d1b0004_ops, sizeof(fe->ops.tuner_ops));

	state = fe->tuner_priv;
	state->i2c = client;
	ret = qm1d1b0004_set_config(fe, cfg);
	if (ret != 0)
		goto err_priv;

	dev_info(&client->dev, "Sharp QM1D1B0004 attached.\n");
	return 0;

err_priv:
	kfree(fe->tuner_priv);
err_mem:
	fe->tuner_priv = NULL;
	return ret;
}

static void qm1d1b0004_remove(struct i2c_client *client)
{
	struct dvb_frontend *fe;

	fe = i2c_get_clientdata(client);
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
}


static const struct i2c_device_id qm1d1b0004_id[] = {
	{"qm1d1b0004", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, qm1d1b0004_id);

static struct i2c_driver qm1d1b0004_driver = {
	.driver = {
		.name = "qm1d1b0004",
	},
	.probe    = qm1d1b0004_probe,
	.remove   = qm1d1b0004_remove,
	.id_table = qm1d1b0004_id,
};

module_i2c_driver(qm1d1b0004_driver);

MODULE_DESCRIPTION("Sharp QM1D1B0004");
MODULE_AUTHOR("Akihiro Tsukada");
MODULE_LICENSE("GPL");
