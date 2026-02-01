


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/slimbus.h>
#include <uapi/sound/asound.h>
#include "slimbus.h"

 
static const struct segdist_code {
	int ratem;
	int seg_interval;
	int segdist_code;
	u32 seg_offset_mask;

} segdist_codes[] = {
	{1,	1536,	0x200,	 0xdff},
	{2,	768,	0x100,	 0xcff},
	{4,	384,	0x080,	 0xc7f},
	{8,	192,	0x040,	 0xc3f},
	{16,	96,	0x020,	 0xc1f},
	{32,	48,	0x010,	 0xc0f},
	{64,	24,	0x008,	 0xc07},
	{128,	12,	0x004,	 0xc03},
	{256,	6,	0x002,	 0xc01},
	{512,	3,	0x001,	 0xc00},
	{3,	512,	0xe00,	 0x1ff},
	{6,	256,	0xd00,	 0x0ff},
	{12,	128,	0xc80,	 0x07f},
	{24,	64,	0xc40,	 0x03f},
	{48,	32,	0xc20,	 0x01f},
	{96,	16,	0xc10,	 0x00f},
	{192,	8,	0xc08,	 0x007},
	{364,	4,	0xc04,	 0x003},
	{768,	2,	0xc02,	 0x001},
};

 
static const int slim_presence_rate_table[] = {
	0,  
	12000,
	24000,
	48000,
	96000,
	192000,
	384000,
	768000,
	0,  
	11025,
	22050,
	44100,
	88200,
	176400,
	352800,
	705600,
	4000,
	8000,
	16000,
	32000,
	64000,
	128000,
	256000,
	512000,
};

 
struct slim_stream_runtime *slim_stream_allocate(struct slim_device *dev,
						 const char *name)
{
	struct slim_stream_runtime *rt;

	rt = kzalloc(sizeof(*rt), GFP_KERNEL);
	if (!rt)
		return ERR_PTR(-ENOMEM);

	rt->name = kasprintf(GFP_KERNEL, "slim-%s", name);
	if (!rt->name) {
		kfree(rt);
		return ERR_PTR(-ENOMEM);
	}

	rt->dev = dev;
	spin_lock(&dev->stream_list_lock);
	list_add_tail(&rt->node, &dev->stream_list);
	spin_unlock(&dev->stream_list_lock);

	return rt;
}
EXPORT_SYMBOL_GPL(slim_stream_allocate);

static int slim_connect_port_channel(struct slim_stream_runtime *stream,
				     struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[2];
	struct slim_val_inf msg = {0, 2, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_CONNECT_SOURCE;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 6, stream->dev->laddr, &msg);

	if (port->direction == SLIM_PORT_SINK)
		txn.mc = SLIM_MSG_MC_CONNECT_SINK;

	wbuf[0] = port->id;
	wbuf[1] = port->ch.id;
	port->ch.state = SLIM_CH_STATE_ASSOCIATED;
	port->state = SLIM_PORT_UNCONFIGURED;

	return slim_do_transfer(sdev->ctrl, &txn);
}

static int slim_disconnect_port(struct slim_stream_runtime *stream,
				struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[1];
	struct slim_val_inf msg = {0, 1, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_DISCONNECT_PORT;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 5, stream->dev->laddr, &msg);

	wbuf[0] = port->id;
	port->ch.state = SLIM_CH_STATE_DISCONNECTED;
	port->state = SLIM_PORT_DISCONNECTED;

	return slim_do_transfer(sdev->ctrl, &txn);
}

static int slim_deactivate_remove_channel(struct slim_stream_runtime *stream,
					  struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[1];
	struct slim_val_inf msg = {0, 1, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_NEXT_DEACTIVATE_CHANNEL;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 5, stream->dev->laddr, &msg);
	int ret;

	wbuf[0] = port->ch.id;
	ret = slim_do_transfer(sdev->ctrl, &txn);
	if (ret)
		return ret;

	txn.mc = SLIM_MSG_MC_NEXT_REMOVE_CHANNEL;
	port->ch.state = SLIM_CH_STATE_REMOVED;

	return slim_do_transfer(sdev->ctrl, &txn);
}

static int slim_get_prate_code(int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(slim_presence_rate_table); i++) {
		if (rate == slim_presence_rate_table[i])
			return i;
	}

	return -EINVAL;
}

 
int slim_stream_prepare(struct slim_stream_runtime *rt,
			struct slim_stream_config *cfg)
{
	struct slim_controller *ctrl = rt->dev->ctrl;
	struct slim_port *port;
	int num_ports, i, port_id, prrate;

	if (rt->ports) {
		dev_err(&rt->dev->dev, "Stream already Prepared\n");
		return -EINVAL;
	}

	num_ports = hweight32(cfg->port_mask);
	rt->ports = kcalloc(num_ports, sizeof(*port), GFP_KERNEL);
	if (!rt->ports)
		return -ENOMEM;

	rt->num_ports = num_ports;
	rt->rate = cfg->rate;
	rt->bps = cfg->bps;
	rt->direction = cfg->direction;

	prrate = slim_get_prate_code(cfg->rate);
	if (prrate < 0) {
		dev_err(&rt->dev->dev, "Cannot get presence rate for rate %d Hz\n",
			cfg->rate);
		return prrate;
	}

	if (cfg->rate % ctrl->a_framer->superfreq) {
		 
		if (cfg->direction == SNDRV_PCM_STREAM_PLAYBACK)
			rt->prot = SLIM_PROTO_PUSH;
		else
			rt->prot = SLIM_PROTO_PULL;
	} else {
		rt->prot = SLIM_PROTO_ISO;
	}

	rt->ratem = cfg->rate/ctrl->a_framer->superfreq;

	i = 0;
	for_each_set_bit(port_id, &cfg->port_mask, SLIM_DEVICE_MAX_PORTS) {
		port = &rt->ports[i];
		port->state = SLIM_PORT_DISCONNECTED;
		port->id = port_id;
		port->ch.prrate = prrate;
		port->ch.id = cfg->chs[i];
		port->ch.data_fmt = SLIM_CH_DATA_FMT_NOT_DEFINED;
		port->ch.aux_fmt = SLIM_CH_AUX_FMT_NOT_APPLICABLE;
		port->ch.state = SLIM_CH_STATE_ALLOCATED;

		if (cfg->direction == SNDRV_PCM_STREAM_PLAYBACK)
			port->direction = SLIM_PORT_SINK;
		else
			port->direction = SLIM_PORT_SOURCE;

		slim_connect_port_channel(rt, port);
		i++;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(slim_stream_prepare);

static int slim_define_channel_content(struct slim_stream_runtime *stream,
				       struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[4];
	struct slim_val_inf msg = {0, 4, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_NEXT_DEFINE_CONTENT;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 8, stream->dev->laddr, &msg);

	wbuf[0] = port->ch.id;
	wbuf[1] = port->ch.prrate;

	 
	if (stream->prot != SLIM_PROTO_ISO)
		wbuf[1] |= SLIM_CHANNEL_CONTENT_FL;

	wbuf[2] = port->ch.data_fmt | (port->ch.aux_fmt << 4);
	wbuf[3] = stream->bps/SLIM_SLOT_LEN_BITS;
	port->ch.state = SLIM_CH_STATE_CONTENT_DEFINED;

	return slim_do_transfer(sdev->ctrl, &txn);
}

static int slim_get_segdist_code(int ratem)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(segdist_codes); i++) {
		if (segdist_codes[i].ratem == ratem)
			return segdist_codes[i].segdist_code;
	}

	return -EINVAL;
}

static int slim_define_channel(struct slim_stream_runtime *stream,
				       struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[4];
	struct slim_val_inf msg = {0, 4, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_NEXT_DEFINE_CHANNEL;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 8, stream->dev->laddr, &msg);

	port->ch.seg_dist = slim_get_segdist_code(stream->ratem);

	wbuf[0] = port->ch.id;
	wbuf[1] = port->ch.seg_dist & 0xFF;
	wbuf[2] = (stream->prot << 4) | ((port->ch.seg_dist & 0xF00) >> 8);
	if (stream->prot == SLIM_PROTO_ISO)
		wbuf[3] = stream->bps/SLIM_SLOT_LEN_BITS;
	else
		wbuf[3] = stream->bps/SLIM_SLOT_LEN_BITS + 1;

	port->ch.state = SLIM_CH_STATE_DEFINED;

	return slim_do_transfer(sdev->ctrl, &txn);
}

static int slim_activate_channel(struct slim_stream_runtime *stream,
				 struct slim_port *port)
{
	struct slim_device *sdev = stream->dev;
	u8 wbuf[1];
	struct slim_val_inf msg = {0, 1, NULL, wbuf, NULL};
	u8 mc = SLIM_MSG_MC_NEXT_ACTIVATE_CHANNEL;
	DEFINE_SLIM_LDEST_TXN(txn, mc, 5, stream->dev->laddr, &msg);

	txn.msg->num_bytes = 1;
	txn.msg->wbuf = wbuf;
	wbuf[0] = port->ch.id;
	port->ch.state = SLIM_CH_STATE_ACTIVE;

	return slim_do_transfer(sdev->ctrl, &txn);
}

 
int slim_stream_enable(struct slim_stream_runtime *stream)
{
	DEFINE_SLIM_BCAST_TXN(txn, SLIM_MSG_MC_BEGIN_RECONFIGURATION,
				3, SLIM_LA_MANAGER, NULL);
	struct slim_controller *ctrl = stream->dev->ctrl;
	int ret, i;

	if (ctrl->enable_stream) {
		ret = ctrl->enable_stream(stream);
		if (ret)
			return ret;

		for (i = 0; i < stream->num_ports; i++)
			stream->ports[i].ch.state = SLIM_CH_STATE_ACTIVE;

		return ret;
	}

	ret = slim_do_transfer(ctrl, &txn);
	if (ret)
		return ret;

	 
	for (i = 0; i < stream->num_ports; i++) {
		struct slim_port *port = &stream->ports[i];

		slim_define_channel(stream, port);
		slim_define_channel_content(stream, port);
	}

	for (i = 0; i < stream->num_ports; i++) {
		struct slim_port *port = &stream->ports[i];

		slim_activate_channel(stream, port);
		port->state = SLIM_PORT_CONFIGURED;
	}
	txn.mc = SLIM_MSG_MC_RECONFIGURE_NOW;

	return slim_do_transfer(ctrl, &txn);
}
EXPORT_SYMBOL_GPL(slim_stream_enable);

 
int slim_stream_disable(struct slim_stream_runtime *stream)
{
	DEFINE_SLIM_BCAST_TXN(txn, SLIM_MSG_MC_BEGIN_RECONFIGURATION,
				3, SLIM_LA_MANAGER, NULL);
	struct slim_controller *ctrl = stream->dev->ctrl;
	int ret, i;

	if (!stream->ports || !stream->num_ports)
		return -EINVAL;

	if (ctrl->disable_stream)
		ctrl->disable_stream(stream);

	ret = slim_do_transfer(ctrl, &txn);
	if (ret)
		return ret;

	for (i = 0; i < stream->num_ports; i++)
		slim_deactivate_remove_channel(stream, &stream->ports[i]);

	txn.mc = SLIM_MSG_MC_RECONFIGURE_NOW;

	return slim_do_transfer(ctrl, &txn);
}
EXPORT_SYMBOL_GPL(slim_stream_disable);

 
int slim_stream_unprepare(struct slim_stream_runtime *stream)
{
	int i;

	if (!stream->ports || !stream->num_ports)
		return -EINVAL;

	for (i = 0; i < stream->num_ports; i++)
		slim_disconnect_port(stream, &stream->ports[i]);

	kfree(stream->ports);
	stream->ports = NULL;
	stream->num_ports = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(slim_stream_unprepare);

 
int slim_stream_free(struct slim_stream_runtime *stream)
{
	struct slim_device *sdev = stream->dev;

	spin_lock(&sdev->stream_list_lock);
	list_del(&stream->node);
	spin_unlock(&sdev->stream_list_lock);

	kfree(stream->name);
	kfree(stream);

	return 0;
}
EXPORT_SYMBOL_GPL(slim_stream_free);
