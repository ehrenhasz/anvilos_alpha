
 

#include "dice.h"

struct dice_mytek_spec {
	unsigned int tx_pcm_chs[MAX_STREAMS][SND_DICE_RATE_MODE_COUNT];
	unsigned int rx_pcm_chs[MAX_STREAMS][SND_DICE_RATE_MODE_COUNT];
};

static const struct dice_mytek_spec stereo_192_dsd_dac = {
	 
	.tx_pcm_chs = {{8, 8, 8}, {0, 0, 0} },
	 
	.rx_pcm_chs = {{4, 4, 4}, {0, 0, 0} }
};

 

int snd_dice_detect_mytek_formats(struct snd_dice *dice)
{
	int i;
	const struct dice_mytek_spec *dev;

	dev = &stereo_192_dsd_dac;

	memcpy(dice->tx_pcm_chs, dev->tx_pcm_chs,
	       MAX_STREAMS * SND_DICE_RATE_MODE_COUNT * sizeof(unsigned int));
	memcpy(dice->rx_pcm_chs, dev->rx_pcm_chs,
	       MAX_STREAMS * SND_DICE_RATE_MODE_COUNT * sizeof(unsigned int));

	for (i = 0; i < MAX_STREAMS; ++i) {
		dice->tx_midi_ports[i] = 0;
		dice->rx_midi_ports[i] = 0;
	}

	return 0;
}
