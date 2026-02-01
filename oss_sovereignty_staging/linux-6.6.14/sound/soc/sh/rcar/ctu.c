





#include "rsnd.h"

#define CTU_NAME_SIZE	16
#define CTU_NAME "ctu"

 

struct rsnd_ctu {
	struct rsnd_mod mod;
	struct rsnd_kctrl_cfg_m pass;
	struct rsnd_kctrl_cfg_m sv[4];
	struct rsnd_kctrl_cfg_s reset;
	int channels;
	u32 flags;
};

#define KCTRL_INITIALIZED	(1 << 0)

#define rsnd_ctu_nr(priv) ((priv)->ctu_nr)
#define for_each_rsnd_ctu(pos, priv, i)					\
	for ((i) = 0;							\
	     ((i) < rsnd_ctu_nr(priv)) &&				\
		     ((pos) = (struct rsnd_ctu *)(priv)->ctu + i);	\
	     i++)

#define rsnd_mod_to_ctu(_mod)	\
	container_of((_mod), struct rsnd_ctu, mod)

#define rsnd_ctu_get(priv, id) ((struct rsnd_ctu *)(priv->ctu) + id)

static void rsnd_ctu_activation(struct rsnd_mod *mod)
{
	rsnd_mod_write(mod, CTU_SWRSR, 0);
	rsnd_mod_write(mod, CTU_SWRSR, 1);
}

static void rsnd_ctu_halt(struct rsnd_mod *mod)
{
	rsnd_mod_write(mod, CTU_CTUIR, 1);
	rsnd_mod_write(mod, CTU_SWRSR, 0);
}

static int rsnd_ctu_probe_(struct rsnd_mod *mod,
			   struct rsnd_dai_stream *io,
			   struct rsnd_priv *priv)
{
	return rsnd_cmd_attach(io, rsnd_mod_id(mod));
}

static void rsnd_ctu_value_init(struct rsnd_dai_stream *io,
			       struct rsnd_mod *mod)
{
	struct rsnd_ctu *ctu = rsnd_mod_to_ctu(mod);
	u32 cpmdr = 0;
	u32 scmdr = 0;
	int i, j;

	for (i = 0; i < RSND_MAX_CHANNELS; i++) {
		u32 val = rsnd_kctrl_valm(ctu->pass, i);

		cpmdr |= val << (28 - (i * 4));

		if ((val > 0x8) && (scmdr < (val - 0x8)))
			scmdr = val - 0x8;
	}

	rsnd_mod_write(mod, CTU_CTUIR, 1);

	rsnd_mod_write(mod, CTU_ADINR, rsnd_runtime_channel_original(io));

	rsnd_mod_write(mod, CTU_CPMDR, cpmdr);

	rsnd_mod_write(mod, CTU_SCMDR, scmdr);

	for (i = 0; i < 4; i++) {

		if (i >= scmdr)
			break;

		for (j = 0; j < RSND_MAX_CHANNELS; j++)
			rsnd_mod_write(mod, CTU_SVxxR(i, j), rsnd_kctrl_valm(ctu->sv[i], j));
	}

	rsnd_mod_write(mod, CTU_CTUIR, 0);
}

static void rsnd_ctu_value_reset(struct rsnd_dai_stream *io,
				 struct rsnd_mod *mod)
{
	struct rsnd_ctu *ctu = rsnd_mod_to_ctu(mod);
	int i;

	if (!rsnd_kctrl_vals(ctu->reset))
		return;

	for (i = 0; i < RSND_MAX_CHANNELS; i++) {
		rsnd_kctrl_valm(ctu->pass, i) = 0;
		rsnd_kctrl_valm(ctu->sv[0],  i) = 0;
		rsnd_kctrl_valm(ctu->sv[1],  i) = 0;
		rsnd_kctrl_valm(ctu->sv[2],  i) = 0;
		rsnd_kctrl_valm(ctu->sv[3],  i) = 0;
	}
	rsnd_kctrl_vals(ctu->reset) = 0;
}

static int rsnd_ctu_init(struct rsnd_mod *mod,
			 struct rsnd_dai_stream *io,
			 struct rsnd_priv *priv)
{
	int ret;

	ret = rsnd_mod_power_on(mod);
	if (ret < 0)
		return ret;

	rsnd_ctu_activation(mod);

	rsnd_ctu_value_init(io, mod);

	return 0;
}

static int rsnd_ctu_quit(struct rsnd_mod *mod,
			 struct rsnd_dai_stream *io,
			 struct rsnd_priv *priv)
{
	rsnd_ctu_halt(mod);

	rsnd_mod_power_off(mod);

	return 0;
}

static int rsnd_ctu_pcm_new(struct rsnd_mod *mod,
			    struct rsnd_dai_stream *io,
			    struct snd_soc_pcm_runtime *rtd)
{
	struct rsnd_ctu *ctu = rsnd_mod_to_ctu(mod);
	int ret;

	if (rsnd_flags_has(ctu, KCTRL_INITIALIZED))
		return 0;

	 
	ret = rsnd_kctrl_new_m(mod, io, rtd, "CTU Pass",
			       rsnd_kctrl_accept_anytime,
			       NULL,
			       &ctu->pass, RSND_MAX_CHANNELS,
			       0xC);
	if (ret < 0)
		return ret;

	 
	ret = rsnd_kctrl_new_m(mod, io, rtd, "CTU SV0",
			       rsnd_kctrl_accept_anytime,
			       NULL,
			       &ctu->sv[0], RSND_MAX_CHANNELS,
			       0x00FFFFFF);
	if (ret < 0)
		return ret;

	 
	ret = rsnd_kctrl_new_m(mod, io, rtd, "CTU SV1",
			       rsnd_kctrl_accept_anytime,
			       NULL,
			       &ctu->sv[1], RSND_MAX_CHANNELS,
			       0x00FFFFFF);
	if (ret < 0)
		return ret;

	 
	ret = rsnd_kctrl_new_m(mod, io, rtd, "CTU SV2",
			       rsnd_kctrl_accept_anytime,
			       NULL,
			       &ctu->sv[2], RSND_MAX_CHANNELS,
			       0x00FFFFFF);
	if (ret < 0)
		return ret;

	 
	ret = rsnd_kctrl_new_m(mod, io, rtd, "CTU SV3",
			       rsnd_kctrl_accept_anytime,
			       NULL,
			       &ctu->sv[3], RSND_MAX_CHANNELS,
			       0x00FFFFFF);
	if (ret < 0)
		return ret;

	 
	ret = rsnd_kctrl_new_s(mod, io, rtd, "CTU Reset",
			       rsnd_kctrl_accept_anytime,
			       rsnd_ctu_value_reset,
			       &ctu->reset, 1);

	rsnd_flags_set(ctu, KCTRL_INITIALIZED);

	return ret;
}

static int rsnd_ctu_id(struct rsnd_mod *mod)
{
	 
	return mod->id / 4;
}

static int rsnd_ctu_id_sub(struct rsnd_mod *mod)
{
	 
	return mod->id % 4;
}

#ifdef CONFIG_DEBUG_FS
static void rsnd_ctu_debug_info(struct seq_file *m,
				struct rsnd_dai_stream *io,
				struct rsnd_mod *mod)
{
	rsnd_debugfs_mod_reg_show(m, mod, RSND_GEN2_SCU,
				  0x500 + rsnd_mod_id_raw(mod) * 0x100, 0x100);
}
#define DEBUG_INFO .debug_info = rsnd_ctu_debug_info
#else
#define DEBUG_INFO
#endif

static struct rsnd_mod_ops rsnd_ctu_ops = {
	.name		= CTU_NAME,
	.probe		= rsnd_ctu_probe_,
	.init		= rsnd_ctu_init,
	.quit		= rsnd_ctu_quit,
	.pcm_new	= rsnd_ctu_pcm_new,
	.get_status	= rsnd_mod_get_status,
	.id		= rsnd_ctu_id,
	.id_sub		= rsnd_ctu_id_sub,
	.id_cmd		= rsnd_mod_id_raw,
	DEBUG_INFO
};

struct rsnd_mod *rsnd_ctu_mod_get(struct rsnd_priv *priv, int id)
{
	if (WARN_ON(id < 0 || id >= rsnd_ctu_nr(priv)))
		id = 0;

	return rsnd_mod_get(rsnd_ctu_get(priv, id));
}

int rsnd_ctu_probe(struct rsnd_priv *priv)
{
	struct device_node *node;
	struct device_node *np;
	struct device *dev = rsnd_priv_to_dev(priv);
	struct rsnd_ctu *ctu;
	struct clk *clk;
	char name[CTU_NAME_SIZE];
	int i, nr, ret;

	 
	if (rsnd_is_gen1(priv))
		return 0;

	node = rsnd_ctu_of_node(priv);
	if (!node)
		return 0;  

	nr = of_get_child_count(node);
	if (!nr) {
		ret = -EINVAL;
		goto rsnd_ctu_probe_done;
	}

	ctu = devm_kcalloc(dev, nr, sizeof(*ctu), GFP_KERNEL);
	if (!ctu) {
		ret = -ENOMEM;
		goto rsnd_ctu_probe_done;
	}

	priv->ctu_nr	= nr;
	priv->ctu	= ctu;

	i = 0;
	ret = 0;
	for_each_child_of_node(node, np) {
		ctu = rsnd_ctu_get(priv, i);

		 
		snprintf(name, CTU_NAME_SIZE, "%s.%d",
			 CTU_NAME, i / 4);

		clk = devm_clk_get(dev, name);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			of_node_put(np);
			goto rsnd_ctu_probe_done;
		}

		ret = rsnd_mod_init(priv, rsnd_mod_get(ctu), &rsnd_ctu_ops,
				    clk, RSND_MOD_CTU, i);
		if (ret) {
			of_node_put(np);
			goto rsnd_ctu_probe_done;
		}

		i++;
	}


rsnd_ctu_probe_done:
	of_node_put(node);

	return ret;
}

void rsnd_ctu_remove(struct rsnd_priv *priv)
{
	struct rsnd_ctu *ctu;
	int i;

	for_each_rsnd_ctu(ctu, priv, i) {
		rsnd_mod_quit(rsnd_mod_get(ctu));
	}
}
