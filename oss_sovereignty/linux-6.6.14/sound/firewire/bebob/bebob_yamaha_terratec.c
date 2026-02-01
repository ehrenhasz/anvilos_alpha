
 

#include "./bebob.h"

 

static const enum snd_bebob_clock_type clk_src_types[] = {
	SND_BEBOB_CLOCK_TYPE_INTERNAL,
	SND_BEBOB_CLOCK_TYPE_EXTERNAL,	 
};
static int
clk_src_get(struct snd_bebob *bebob, unsigned int *id)
{
	int err;

	err = avc_audio_get_selector(bebob->unit, 0, 4, id);
	if (err < 0)
		return err;

	if (*id >= ARRAY_SIZE(clk_src_types))
		return -EIO;

	return 0;
}
static const struct snd_bebob_clock_spec clock_spec = {
	.num	= ARRAY_SIZE(clk_src_types),
	.types	= clk_src_types,
	.get	= &clk_src_get,
};
static const struct snd_bebob_rate_spec rate_spec = {
	.get	= &snd_bebob_stream_get_rate,
	.set	= &snd_bebob_stream_set_rate,
};
const struct snd_bebob_spec yamaha_terratec_spec = {
	.clock	= &clock_spec,
	.rate	= &rate_spec,
	.meter	= NULL
};
