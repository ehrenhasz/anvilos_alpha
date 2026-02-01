
 

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/pm_runtime.h>
#include <sound/hdaudio.h>
#include <sound/hda_regmap.h>
#include <sound/pcm.h>
#include "local.h"

static void setup_fg_nodes(struct hdac_device *codec);
static int get_codec_vendor_name(struct hdac_device *codec);

static void default_release(struct device *dev)
{
	snd_hdac_device_exit(dev_to_hdac_dev(dev));
}

 
int snd_hdac_device_init(struct hdac_device *codec, struct hdac_bus *bus,
			 const char *name, unsigned int addr)
{
	struct device *dev;
	hda_nid_t fg;
	int err;

	dev = &codec->dev;
	device_initialize(dev);
	dev->parent = bus->dev;
	dev->bus = &snd_hda_bus_type;
	dev->release = default_release;
	dev->groups = hdac_dev_attr_groups;
	dev_set_name(dev, "%s", name);
	device_enable_async_suspend(dev);

	codec->bus = bus;
	codec->addr = addr;
	codec->type = HDA_DEV_CORE;
	mutex_init(&codec->widget_lock);
	mutex_init(&codec->regmap_lock);
	pm_runtime_set_active(&codec->dev);
	pm_runtime_get_noresume(&codec->dev);
	atomic_set(&codec->in_pm, 0);

	err = snd_hdac_bus_add_device(bus, codec);
	if (err < 0)
		goto error;

	 
	codec->vendor_id = snd_hdac_read_parm(codec, AC_NODE_ROOT,
					      AC_PAR_VENDOR_ID);
	if (codec->vendor_id == -1) {
		 
		codec->vendor_id = snd_hdac_read_parm(codec, AC_NODE_ROOT,
						      AC_PAR_VENDOR_ID);
	}

	codec->subsystem_id = snd_hdac_read_parm(codec, AC_NODE_ROOT,
						 AC_PAR_SUBSYSTEM_ID);
	codec->revision_id = snd_hdac_read_parm(codec, AC_NODE_ROOT,
						AC_PAR_REV_ID);

	setup_fg_nodes(codec);
	if (!codec->afg && !codec->mfg) {
		dev_err(dev, "no AFG or MFG node found\n");
		err = -ENODEV;
		goto error;
	}

	fg = codec->afg ? codec->afg : codec->mfg;

	err = snd_hdac_refresh_widgets(codec);
	if (err < 0)
		goto error;

	codec->power_caps = snd_hdac_read_parm(codec, fg, AC_PAR_POWER_STATE);
	 
	if (codec->subsystem_id == -1 || codec->subsystem_id == 0)
		snd_hdac_read(codec, fg, AC_VERB_GET_SUBSYSTEM_ID, 0,
			      &codec->subsystem_id);

	err = get_codec_vendor_name(codec);
	if (err < 0)
		goto error;

	codec->chip_name = kasprintf(GFP_KERNEL, "ID %x",
				     codec->vendor_id & 0xffff);
	if (!codec->chip_name) {
		err = -ENOMEM;
		goto error;
	}

	return 0;

 error:
	put_device(&codec->dev);
	return err;
}
EXPORT_SYMBOL_GPL(snd_hdac_device_init);

 
void snd_hdac_device_exit(struct hdac_device *codec)
{
	pm_runtime_put_noidle(&codec->dev);
	 
	pm_runtime_set_suspended(&codec->dev);
	snd_hdac_bus_remove_device(codec->bus, codec);
	kfree(codec->vendor_name);
	kfree(codec->chip_name);
}
EXPORT_SYMBOL_GPL(snd_hdac_device_exit);

 
int snd_hdac_device_register(struct hdac_device *codec)
{
	int err;

	err = device_add(&codec->dev);
	if (err < 0)
		return err;
	mutex_lock(&codec->widget_lock);
	err = hda_widget_sysfs_init(codec);
	mutex_unlock(&codec->widget_lock);
	if (err < 0) {
		device_del(&codec->dev);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_device_register);

 
void snd_hdac_device_unregister(struct hdac_device *codec)
{
	if (device_is_registered(&codec->dev)) {
		mutex_lock(&codec->widget_lock);
		hda_widget_sysfs_exit(codec);
		mutex_unlock(&codec->widget_lock);
		device_del(&codec->dev);
		snd_hdac_bus_remove_device(codec->bus, codec);
	}
}
EXPORT_SYMBOL_GPL(snd_hdac_device_unregister);

 
int snd_hdac_device_set_chip_name(struct hdac_device *codec, const char *name)
{
	char *newname;

	if (!name)
		return 0;
	newname = kstrdup(name, GFP_KERNEL);
	if (!newname)
		return -ENOMEM;
	kfree(codec->chip_name);
	codec->chip_name = newname;
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_device_set_chip_name);

 
int snd_hdac_codec_modalias(const struct hdac_device *codec, char *buf, size_t size)
{
	return scnprintf(buf, size, "hdaudio:v%08Xr%08Xa%02X\n",
			codec->vendor_id, codec->revision_id, codec->type);
}
EXPORT_SYMBOL_GPL(snd_hdac_codec_modalias);

 
static unsigned int snd_hdac_make_cmd(struct hdac_device *codec, hda_nid_t nid,
				      unsigned int verb, unsigned int parm)
{
	u32 val, addr;

	addr = codec->addr;
	if ((addr & ~0xf) || (nid & ~0x7f) ||
	    (verb & ~0xfff) || (parm & ~0xffff)) {
		dev_err(&codec->dev, "out of range cmd %x:%x:%x:%x\n",
			addr, nid, verb, parm);
		return -1;
	}

	val = addr << 28;
	val |= (u32)nid << 20;
	val |= verb << 8;
	val |= parm;
	return val;
}

 
int snd_hdac_exec_verb(struct hdac_device *codec, unsigned int cmd,
		       unsigned int flags, unsigned int *res)
{
	if (codec->exec_verb)
		return codec->exec_verb(codec, cmd, flags, res);
	return snd_hdac_bus_exec_verb(codec->bus, codec->addr, cmd, res);
}


 
int snd_hdac_read(struct hdac_device *codec, hda_nid_t nid,
		  unsigned int verb, unsigned int parm, unsigned int *res)
{
	unsigned int cmd = snd_hdac_make_cmd(codec, nid, verb, parm);

	return snd_hdac_exec_verb(codec, cmd, 0, res);
}
EXPORT_SYMBOL_GPL(snd_hdac_read);

 
int _snd_hdac_read_parm(struct hdac_device *codec, hda_nid_t nid, int parm,
			unsigned int *res)
{
	unsigned int cmd;

	cmd = snd_hdac_regmap_encode_verb(nid, AC_VERB_PARAMETERS) | parm;
	return snd_hdac_regmap_read_raw(codec, cmd, res);
}
EXPORT_SYMBOL_GPL(_snd_hdac_read_parm);

 
int snd_hdac_read_parm_uncached(struct hdac_device *codec, hda_nid_t nid,
				int parm)
{
	unsigned int cmd, val;

	cmd = snd_hdac_regmap_encode_verb(nid, AC_VERB_PARAMETERS) | parm;
	if (snd_hdac_regmap_read_raw_uncached(codec, cmd, &val) < 0)
		return -1;
	return val;
}
EXPORT_SYMBOL_GPL(snd_hdac_read_parm_uncached);

 
int snd_hdac_override_parm(struct hdac_device *codec, hda_nid_t nid,
			   unsigned int parm, unsigned int val)
{
	unsigned int verb = (AC_VERB_PARAMETERS << 8) | (nid << 20) | parm;
	int err;

	if (!codec->regmap)
		return -EINVAL;

	codec->caps_overwriting = true;
	err = snd_hdac_regmap_write_raw(codec, verb, val);
	codec->caps_overwriting = false;
	return err;
}
EXPORT_SYMBOL_GPL(snd_hdac_override_parm);

 
int snd_hdac_get_sub_nodes(struct hdac_device *codec, hda_nid_t nid,
			   hda_nid_t *start_id)
{
	unsigned int parm;

	parm = snd_hdac_read_parm_uncached(codec, nid, AC_PAR_NODE_COUNT);
	if (parm == -1) {
		*start_id = 0;
		return 0;
	}
	*start_id = (parm >> 16) & 0x7fff;
	return (int)(parm & 0x7fff);
}
EXPORT_SYMBOL_GPL(snd_hdac_get_sub_nodes);

 
static void setup_fg_nodes(struct hdac_device *codec)
{
	int i, total_nodes, function_id;
	hda_nid_t nid;

	total_nodes = snd_hdac_get_sub_nodes(codec, AC_NODE_ROOT, &nid);
	for (i = 0; i < total_nodes; i++, nid++) {
		function_id = snd_hdac_read_parm(codec, nid,
						 AC_PAR_FUNCTION_TYPE);
		switch (function_id & 0xff) {
		case AC_GRP_AUDIO_FUNCTION:
			codec->afg = nid;
			codec->afg_function_id = function_id & 0xff;
			codec->afg_unsol = (function_id >> 8) & 1;
			break;
		case AC_GRP_MODEM_FUNCTION:
			codec->mfg = nid;
			codec->mfg_function_id = function_id & 0xff;
			codec->mfg_unsol = (function_id >> 8) & 1;
			break;
		default:
			break;
		}
	}
}

 
int snd_hdac_refresh_widgets(struct hdac_device *codec)
{
	hda_nid_t start_nid;
	int nums, err = 0;

	 
	mutex_lock(&codec->widget_lock);
	nums = snd_hdac_get_sub_nodes(codec, codec->afg, &start_nid);
	if (!start_nid || nums <= 0 || nums >= 0xff) {
		dev_err(&codec->dev, "cannot read sub nodes for FG 0x%02x\n",
			codec->afg);
		err = -EINVAL;
		goto unlock;
	}

	err = hda_widget_sysfs_reinit(codec, start_nid, nums);
	if (err < 0)
		goto unlock;

	codec->num_nodes = nums;
	codec->start_nid = start_nid;
	codec->end_nid = start_nid + nums;
unlock:
	mutex_unlock(&codec->widget_lock);
	return err;
}
EXPORT_SYMBOL_GPL(snd_hdac_refresh_widgets);

 
static unsigned int get_num_conns(struct hdac_device *codec, hda_nid_t nid)
{
	unsigned int wcaps = get_wcaps(codec, nid);
	unsigned int parm;

	if (!(wcaps & AC_WCAP_CONN_LIST) &&
	    get_wcaps_type(wcaps) != AC_WID_VOL_KNB)
		return 0;

	parm = snd_hdac_read_parm(codec, nid, AC_PAR_CONNLIST_LEN);
	if (parm == -1)
		parm = 0;
	return parm;
}

 
int snd_hdac_get_connections(struct hdac_device *codec, hda_nid_t nid,
			     hda_nid_t *conn_list, int max_conns)
{
	unsigned int parm;
	int i, conn_len, conns, err;
	unsigned int shift, num_elems, mask;
	hda_nid_t prev_nid;
	int null_count = 0;

	parm = get_num_conns(codec, nid);
	if (!parm)
		return 0;

	if (parm & AC_CLIST_LONG) {
		 
		shift = 16;
		num_elems = 2;
	} else {
		 
		shift = 8;
		num_elems = 4;
	}
	conn_len = parm & AC_CLIST_LENGTH;
	mask = (1 << (shift-1)) - 1;

	if (!conn_len)
		return 0;  

	if (conn_len == 1) {
		 
		err = snd_hdac_read(codec, nid, AC_VERB_GET_CONNECT_LIST, 0,
				    &parm);
		if (err < 0)
			return err;
		if (conn_list)
			conn_list[0] = parm & mask;
		return 1;
	}

	 
	conns = 0;
	prev_nid = 0;
	for (i = 0; i < conn_len; i++) {
		int range_val;
		hda_nid_t val, n;

		if (i % num_elems == 0) {
			err = snd_hdac_read(codec, nid,
					    AC_VERB_GET_CONNECT_LIST, i,
					    &parm);
			if (err < 0)
				return -EIO;
		}
		range_val = !!(parm & (1 << (shift-1)));  
		val = parm & mask;
		if (val == 0 && null_count++) {   
			dev_dbg(&codec->dev,
				"invalid CONNECT_LIST verb %x[%i]:%x\n",
				nid, i, parm);
			return 0;
		}
		parm >>= shift;
		if (range_val) {
			 
			if (!prev_nid || prev_nid >= val) {
				dev_warn(&codec->dev,
					 "invalid dep_range_val %x:%x\n",
					 prev_nid, val);
				continue;
			}
			for (n = prev_nid + 1; n <= val; n++) {
				if (conn_list) {
					if (conns >= max_conns)
						return -ENOSPC;
					conn_list[conns] = n;
				}
				conns++;
			}
		} else {
			if (conn_list) {
				if (conns >= max_conns)
					return -ENOSPC;
				conn_list[conns] = val;
			}
			conns++;
		}
		prev_nid = val;
	}
	return conns;
}
EXPORT_SYMBOL_GPL(snd_hdac_get_connections);

#ifdef CONFIG_PM
 
int snd_hdac_power_up(struct hdac_device *codec)
{
	return pm_runtime_get_sync(&codec->dev);
}
EXPORT_SYMBOL_GPL(snd_hdac_power_up);

 
int snd_hdac_power_down(struct hdac_device *codec)
{
	struct device *dev = &codec->dev;

	pm_runtime_mark_last_busy(dev);
	return pm_runtime_put_autosuspend(dev);
}
EXPORT_SYMBOL_GPL(snd_hdac_power_down);

 
int snd_hdac_power_up_pm(struct hdac_device *codec)
{
	if (!atomic_inc_not_zero(&codec->in_pm))
		return snd_hdac_power_up(codec);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_power_up_pm);

 
int snd_hdac_keep_power_up(struct hdac_device *codec)
{
	if (!atomic_inc_not_zero(&codec->in_pm)) {
		int ret = pm_runtime_get_if_active(&codec->dev, true);
		if (!ret)
			return -1;
		if (ret < 0)
			return 0;
	}
	return 1;
}

 
int snd_hdac_power_down_pm(struct hdac_device *codec)
{
	if (atomic_dec_if_positive(&codec->in_pm) < 0)
		return snd_hdac_power_down(codec);
	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_power_down_pm);
#endif

 
struct hda_vendor_id {
	unsigned int id;
	const char *name;
};

static const struct hda_vendor_id hda_vendor_ids[] = {
	{ 0x0014, "Loongson" },
	{ 0x1002, "ATI" },
	{ 0x1013, "Cirrus Logic" },
	{ 0x1057, "Motorola" },
	{ 0x1095, "Silicon Image" },
	{ 0x10de, "Nvidia" },
	{ 0x10ec, "Realtek" },
	{ 0x1102, "Creative" },
	{ 0x1106, "VIA" },
	{ 0x111d, "IDT" },
	{ 0x11c1, "LSI" },
	{ 0x11d4, "Analog Devices" },
	{ 0x13f6, "C-Media" },
	{ 0x14f1, "Conexant" },
	{ 0x17e8, "Chrontel" },
	{ 0x1854, "LG" },
	{ 0x19e5, "Huawei" },
	{ 0x1aec, "Wolfson Microelectronics" },
	{ 0x1af4, "QEMU" },
	{ 0x434d, "C-Media" },
	{ 0x8086, "Intel" },
	{ 0x8384, "SigmaTel" },
	{}  
};

 
static int get_codec_vendor_name(struct hdac_device *codec)
{
	const struct hda_vendor_id *c;
	u16 vendor_id = codec->vendor_id >> 16;

	for (c = hda_vendor_ids; c->id; c++) {
		if (c->id == vendor_id) {
			codec->vendor_name = kstrdup(c->name, GFP_KERNEL);
			return codec->vendor_name ? 0 : -ENOMEM;
		}
	}

	codec->vendor_name = kasprintf(GFP_KERNEL, "Generic %04x", vendor_id);
	return codec->vendor_name ? 0 : -ENOMEM;
}

 
struct hda_rate_tbl {
	unsigned int hz;
	unsigned int alsa_bits;
	unsigned int hda_fmt;
};

 
#define HDA_RATE(base, mult, div) \
	(AC_FMT_BASE_##base##K | (((mult) - 1) << AC_FMT_MULT_SHIFT) | \
	 (((div) - 1) << AC_FMT_DIV_SHIFT))

static const struct hda_rate_tbl rate_bits[] = {
	 

	 
	{ 8000, SNDRV_PCM_RATE_8000, HDA_RATE(48, 1, 6) },
	{ 11025, SNDRV_PCM_RATE_11025, HDA_RATE(44, 1, 4) },
	{ 16000, SNDRV_PCM_RATE_16000, HDA_RATE(48, 1, 3) },
	{ 22050, SNDRV_PCM_RATE_22050, HDA_RATE(44, 1, 2) },
	{ 32000, SNDRV_PCM_RATE_32000, HDA_RATE(48, 2, 3) },
	{ 44100, SNDRV_PCM_RATE_44100, HDA_RATE(44, 1, 1) },
	{ 48000, SNDRV_PCM_RATE_48000, HDA_RATE(48, 1, 1) },
	{ 88200, SNDRV_PCM_RATE_88200, HDA_RATE(44, 2, 1) },
	{ 96000, SNDRV_PCM_RATE_96000, HDA_RATE(48, 2, 1) },
	{ 176400, SNDRV_PCM_RATE_176400, HDA_RATE(44, 4, 1) },
	{ 192000, SNDRV_PCM_RATE_192000, HDA_RATE(48, 4, 1) },
#define AC_PAR_PCM_RATE_BITS	11
	 

	 
	{ 9600, SNDRV_PCM_RATE_KNOT, HDA_RATE(48, 1, 5) },

	{ 0 }  
};

 
unsigned int snd_hdac_calc_stream_format(unsigned int rate,
					 unsigned int channels,
					 snd_pcm_format_t format,
					 unsigned int maxbps,
					 unsigned short spdif_ctls)
{
	int i;
	unsigned int val = 0;

	for (i = 0; rate_bits[i].hz; i++)
		if (rate_bits[i].hz == rate) {
			val = rate_bits[i].hda_fmt;
			break;
		}
	if (!rate_bits[i].hz)
		return 0;

	if (channels == 0 || channels > 8)
		return 0;
	val |= channels - 1;

	switch (snd_pcm_format_width(format)) {
	case 8:
		val |= AC_FMT_BITS_8;
		break;
	case 16:
		val |= AC_FMT_BITS_16;
		break;
	case 20:
	case 24:
	case 32:
		if (maxbps >= 32 || format == SNDRV_PCM_FORMAT_FLOAT_LE)
			val |= AC_FMT_BITS_32;
		else if (maxbps >= 24)
			val |= AC_FMT_BITS_24;
		else
			val |= AC_FMT_BITS_20;
		break;
	default:
		return 0;
	}

	if (spdif_ctls & AC_DIG1_NONAUDIO)
		val |= AC_FMT_TYPE_NON_PCM;

	return val;
}
EXPORT_SYMBOL_GPL(snd_hdac_calc_stream_format);

static unsigned int query_pcm_param(struct hdac_device *codec, hda_nid_t nid)
{
	unsigned int val = 0;

	if (nid != codec->afg &&
	    (get_wcaps(codec, nid) & AC_WCAP_FORMAT_OVRD))
		val = snd_hdac_read_parm(codec, nid, AC_PAR_PCM);
	if (!val || val == -1)
		val = snd_hdac_read_parm(codec, codec->afg, AC_PAR_PCM);
	if (!val || val == -1)
		return 0;
	return val;
}

static unsigned int query_stream_param(struct hdac_device *codec, hda_nid_t nid)
{
	unsigned int streams = snd_hdac_read_parm(codec, nid, AC_PAR_STREAM);

	if (!streams || streams == -1)
		streams = snd_hdac_read_parm(codec, codec->afg, AC_PAR_STREAM);
	if (!streams || streams == -1)
		return 0;
	return streams;
}

 
int snd_hdac_query_supported_pcm(struct hdac_device *codec, hda_nid_t nid,
				 u32 *ratesp, u64 *formatsp, unsigned int *bpsp)
{
	unsigned int i, val, wcaps;

	wcaps = get_wcaps(codec, nid);
	val = query_pcm_param(codec, nid);

	if (ratesp) {
		u32 rates = 0;
		for (i = 0; i < AC_PAR_PCM_RATE_BITS; i++) {
			if (val & (1 << i))
				rates |= rate_bits[i].alsa_bits;
		}
		if (rates == 0) {
			dev_err(&codec->dev,
				"rates == 0 (nid=0x%x, val=0x%x, ovrd=%i)\n",
				nid, val,
				(wcaps & AC_WCAP_FORMAT_OVRD) ? 1 : 0);
			return -EIO;
		}
		*ratesp = rates;
	}

	if (formatsp || bpsp) {
		u64 formats = 0;
		unsigned int streams, bps;

		streams = query_stream_param(codec, nid);
		if (!streams)
			return -EIO;

		bps = 0;
		if (streams & AC_SUPFMT_PCM) {
			if (val & AC_SUPPCM_BITS_8) {
				formats |= SNDRV_PCM_FMTBIT_U8;
				bps = 8;
			}
			if (val & AC_SUPPCM_BITS_16) {
				formats |= SNDRV_PCM_FMTBIT_S16_LE;
				bps = 16;
			}
			if (wcaps & AC_WCAP_DIGITAL) {
				if (val & AC_SUPPCM_BITS_32)
					formats |= SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE;
				if (val & (AC_SUPPCM_BITS_20|AC_SUPPCM_BITS_24))
					formats |= SNDRV_PCM_FMTBIT_S32_LE;
				if (val & AC_SUPPCM_BITS_24)
					bps = 24;
				else if (val & AC_SUPPCM_BITS_20)
					bps = 20;
			} else if (val & (AC_SUPPCM_BITS_20|AC_SUPPCM_BITS_24|
					  AC_SUPPCM_BITS_32)) {
				formats |= SNDRV_PCM_FMTBIT_S32_LE;
				if (val & AC_SUPPCM_BITS_32)
					bps = 32;
				else if (val & AC_SUPPCM_BITS_24)
					bps = 24;
				else if (val & AC_SUPPCM_BITS_20)
					bps = 20;
			}
		}
#if 0  
		if (streams & AC_SUPFMT_FLOAT32) {
			formats |= SNDRV_PCM_FMTBIT_FLOAT_LE;
			if (!bps)
				bps = 32;
		}
#endif
		if (streams == AC_SUPFMT_AC3) {
			 
			 
			formats |= SNDRV_PCM_FMTBIT_U8;
			bps = 8;
		}
		if (formats == 0) {
			dev_err(&codec->dev,
				"formats == 0 (nid=0x%x, val=0x%x, ovrd=%i, streams=0x%x)\n",
				nid, val,
				(wcaps & AC_WCAP_FORMAT_OVRD) ? 1 : 0,
				streams);
			return -EIO;
		}
		if (formatsp)
			*formatsp = formats;
		if (bpsp)
			*bpsp = bps;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(snd_hdac_query_supported_pcm);

 
bool snd_hdac_is_supported_format(struct hdac_device *codec, hda_nid_t nid,
				  unsigned int format)
{
	int i;
	unsigned int val = 0, rate, stream;

	val = query_pcm_param(codec, nid);
	if (!val)
		return false;

	rate = format & 0xff00;
	for (i = 0; i < AC_PAR_PCM_RATE_BITS; i++)
		if (rate_bits[i].hda_fmt == rate) {
			if (val & (1 << i))
				break;
			return false;
		}
	if (i >= AC_PAR_PCM_RATE_BITS)
		return false;

	stream = query_stream_param(codec, nid);
	if (!stream)
		return false;

	if (stream & AC_SUPFMT_PCM) {
		switch (format & 0xf0) {
		case 0x00:
			if (!(val & AC_SUPPCM_BITS_8))
				return false;
			break;
		case 0x10:
			if (!(val & AC_SUPPCM_BITS_16))
				return false;
			break;
		case 0x20:
			if (!(val & AC_SUPPCM_BITS_20))
				return false;
			break;
		case 0x30:
			if (!(val & AC_SUPPCM_BITS_24))
				return false;
			break;
		case 0x40:
			if (!(val & AC_SUPPCM_BITS_32))
				return false;
			break;
		default:
			return false;
		}
	} else {
		 
	}

	return true;
}
EXPORT_SYMBOL_GPL(snd_hdac_is_supported_format);

static unsigned int codec_read(struct hdac_device *hdac, hda_nid_t nid,
			int flags, unsigned int verb, unsigned int parm)
{
	unsigned int cmd = snd_hdac_make_cmd(hdac, nid, verb, parm);
	unsigned int res;

	if (snd_hdac_exec_verb(hdac, cmd, flags, &res))
		return -1;

	return res;
}

static int codec_write(struct hdac_device *hdac, hda_nid_t nid,
			int flags, unsigned int verb, unsigned int parm)
{
	unsigned int cmd = snd_hdac_make_cmd(hdac, nid, verb, parm);

	return snd_hdac_exec_verb(hdac, cmd, flags, NULL);
}

 
int snd_hdac_codec_read(struct hdac_device *hdac, hda_nid_t nid,
			int flags, unsigned int verb, unsigned int parm)
{
	return codec_read(hdac, nid, flags, verb, parm);
}
EXPORT_SYMBOL_GPL(snd_hdac_codec_read);

 
int snd_hdac_codec_write(struct hdac_device *hdac, hda_nid_t nid,
			int flags, unsigned int verb, unsigned int parm)
{
	return codec_write(hdac, nid, flags, verb, parm);
}
EXPORT_SYMBOL_GPL(snd_hdac_codec_write);

 
bool snd_hdac_check_power_state(struct hdac_device *hdac,
		hda_nid_t nid, unsigned int target_state)
{
	unsigned int state = codec_read(hdac, nid, 0,
				AC_VERB_GET_POWER_STATE, 0);

	if (state & AC_PWRST_ERROR)
		return true;
	state = (state >> 4) & 0x0f;
	return (state == target_state);
}
EXPORT_SYMBOL_GPL(snd_hdac_check_power_state);
 
unsigned int snd_hdac_sync_power_state(struct hdac_device *codec,
			hda_nid_t nid, unsigned int power_state)
{
	unsigned long end_time = jiffies + msecs_to_jiffies(500);
	unsigned int state, actual_state, count;

	for (count = 0; count < 500; count++) {
		state = snd_hdac_codec_read(codec, nid, 0,
				AC_VERB_GET_POWER_STATE, 0);
		if (state & AC_PWRST_ERROR) {
			msleep(20);
			break;
		}
		actual_state = (state >> 4) & 0x0f;
		if (actual_state == power_state)
			break;
		if (time_after_eq(jiffies, end_time))
			break;
		 
		msleep(1);
	}
	return state;
}
EXPORT_SYMBOL_GPL(snd_hdac_sync_power_state);
